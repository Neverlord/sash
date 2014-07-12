/******************************************************************************
 *                   ____     ______   ____     __  __                        *
 *                  /\  _`\  /\  _  \ /\  _`\  /\ \/\ \                       *
 *                  \ \,\L\_\\ \ \L\ \\ \,\L\_\\ \ \_\ \                      *
 *                   \/_\__ \ \ \  __ \\/_\__ \ \ \  _  \                     *
 *                     /\ \L\ \\ \ \/\ \ /\ \L\ \\ \ \ \ \                    *
 *                     \ `\____\\ \_\ \_\\ `\____\\ \_\ \_\                   *
 *                      \/_____/ \/_/\/_/ \/_____/ \/_/\/_/                   *
 *                                                                            *
 *                                                                            *
 * Copyright (c) 2014                                                         *
 * Matthias Vallentin <vallentin (at) icir.org>                               *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef SASH_COMMAND_LINE_HPP
#define SASH_COMMAND_LINE_HPP

#include <map>
#include <memory>
#include <vector>
#include <cctype>

#include "sash/mode.hpp"
#include "sash/color.hpp"
#include "sash/command.hpp"

namespace sash {

/// An abstraction for a mode-based command line interace (CLI). The CLI offers
/// multiple *modes* each of which contain a set of *commands*. At any given
/// time, one can change the mode of the command line by pushing or popping a
/// mode from the mode stack. Each mode has a distinct prompt and command
/// history. A command is a recursive element consisting of zero or more
/// space-separated sub-commands. Each command has a description and a callback
/// handler for arguments which themselves are not commands.
/// @tparam Backend The CLI implementation, e.g., libedit.
template<class Backend, class Command, class Preprocessor>
class command_line
{
public:
  /// The callback functor for command arguments.
  using command_cb = typename Command::callback_type;

  /// A (smart) pointer to a command.
  using command_ptr = std::shared_ptr<Command>;

  /// A unique pointer to our backend.
  using backend_ptr = std::unique_ptr<Backend>;

  using mode_type = mode<Backend, Command>;

  using mode_ptr = std::shared_ptr<mode_type>;

  /// Creates a new mode for a set of related commands. Only one mode can
  /// be active at a time. Each mode has its own history.
  ///
  /// @param name The name of the mode.
  ///
  /// @param prompt The prompt of the mode.
  ///
  /// @param prompt_color The color of *prompt*.
  ///
  /// @param history_file The filename of the history.
  ///
  /// @returns A valid pointer to the new mode on success.
  mode_ptr mode_add(std::string name,
                    std::string prompt,
                    char const* prompt_color = nullptr,
                    std::string history_file = std::string{})
  {
    auto& ptrref = modes_[name];
    if (ptrref == nullptr)
    {
      ptrref = std::make_shared<mode_type>(std::move(name),
                                           std::move(history_file),
                                           1000,
                                           true,
                                           std::move(prompt),
                                           prompt_color);
      return ptrref;
    }
    // return nullptr to indicate that we didn't insert a new element
    return nullptr;
  }

  /// Processes a single command from the command line.
  /// @param cmd The command to process.
  /// @returns A valid result if the callback executed and an error on failure.
  command_result process(std::string const& cmd)
  {
    if (cmd.empty())
      return nop;
    if (mode_stack_.empty())
    {
      last_error_ = "command_line: mode stack is empty";
      return no_command;
    }
    if (preprocessors_.empty())
      return mode_stack_.back()->execute(last_error_, cmd);
    // We use two strings here for input and output. The next best
    // alternative would be to move the input into the preprocessor
    // and let it return a string. However, it's very likely that our
    // preprocessor cannot perform its operation in place, meaning that
    // it needs a temporary string. When chaining multiple preprocessors,
    // this design would cause each preprocessor to allocate a new string
    // and deallocate its input. With this design, we have only two string
    // buffers that are re-used every time. It's uglier, but way
    // more efficient.
    last_error_.clear();
    std::string in = cmd;
    std::string out;
    for (auto& p : preprocessors_)
    {
      p(last_error_, const_cast<const std::string&>(in), out);
      if (! last_error_.empty())
        return no_command;
      if (out.empty())
        return executed;
      in.swap(out);
    }
    return mode_stack_.back()->execute(last_error_, in);
  }

  /// Removes an existing mode.
  /// @param name The name of the mode.
  /// @returns `true` on success.
  bool mode_rm(std::string const& name)
  {
    return modes_.erase(name) > 0;
  }

  /// Enters a given mode.
  /// @param mode The name of mode.
  /// @returns `true` on success.
  bool mode_push(std::string const& mode)
  {
    auto last = modes_.end();
    auto i = modes_.find(mode);
    if (i == last)
      return false;
    mode_stack_.emplace_back(i->second);
    return true;
  }

  /// Leaves the current mode.
  /// @returns The number of modes left on the stack.
  size_t mode_pop()
  {
    if (mode_stack_.empty())
      return false;
    mode_stack_.pop_back();
    return true;
  }

  /// Appends an entry to the history of the current mode.
  /// @param entry The history entry to add.
  /// @returns `true` on success.
  bool append_to_history(std::string const& entry)
  {
    auto bptr = current_backend();
    if (bptr == nullptr)
      return false;
    bptr->history_enter(entry);
    bptr->history_save();
    return true;
  }

  /// Retrieves a single character from the command line in a blocking fashion.
  /// @param c The result parameter containing the character from STDIN.
  /// @returns `true` on success, `false` on EOF, and an error otherwise.
  bool read_char(char& c)
  {
    auto bptr = current_backend();
    return bptr == nullptr ? false : bptr->read_char(c);
  }

  /// Retrieves a full line from the command line in a blocking fashion.
  /// @param line The result parameter containing the line.
  /// @returns `true` on success, `false` on EOF, and an error otherwise.
  bool read_line(std::string& line)
  {
    auto bptr = current_backend();
    if (! bptr)
      return false;
    // Fixes TTY weirdness which may occur when switching between modes.
    bptr->reset();
    if (! bptr->read_line(line))
      return false;
    // Trim line from leading/trailing whitespace.
    auto not_space = [](char c) { return !isspace(c); };
    line.erase(line.begin(), find_if(line.begin(), line.end(), not_space));
    line.erase(find_if(line.rbegin(), line.rend(), not_space).base(),
               line.end());
    return true;
  }

  std::string const& last_error() const
  {
    return last_error_;
  }

  /// Queries whether this command line has an active mode.
  bool has_mode() const
  {
    return ! mode_stack_.empty();
  }

  /// Returns a reference to the currently active mode.
  /// @pre has_mode()
  mode_type& current_mode()
  {
    return *mode_stack_.back();
  }

  /// Adds a new preprocessor to the command line. Preprocessors
  /// intercept any input line *before* it is being passed to the
  /// currently active mode and can manipulate the string.
  /// For example, a preprocessor can be used to add an engine for
  /// builtin commands or to provide variables.
  void add_preprocessor(Preprocessor preproc)
  {
    preprocessors_.emplace_back(std::move(preproc));
  }

private:
  // get the current backend or nullptr if mode stack is empty
  Backend* current_backend()
  {
    return mode_stack_.empty() ? nullptr : &mode_stack_.back()->backend();
  }

  std::vector<std::shared_ptr<mode_type>> mode_stack_;
  std::map<std::string, std::shared_ptr<mode_type>> modes_;
  backend_ptr backend_;
  std::string last_error_;
  std::vector<Preprocessor> preprocessors_;
};

} // namespace sash

#endif
