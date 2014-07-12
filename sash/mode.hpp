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

#ifndef SASH_MODE_HPP
#define SASH_MODE_HPP

#include <memory>
#include <string>

#include "sash/color.hpp"
#include "sash/command.hpp"

namespace sash {

/// A command-line context with its own commands, history, and prompt.
template<class Backend, class Command>
class mode
{

  mode(mode const&) = delete;
  mode& operator=(mode const&) = delete;

public:
  /// The type for our completion context.
  using completer_type = typename Backend::completer_type;

  /// A callback for CLI completions.
  using completion_cb = typename completer_type::callback_type;

  /// A smart pointer to a command.
  using command_ptr = std::shared_ptr<Command>;

  /// A callback for commands.
  using command_cb = typename Command::callback_type;

  /// Constructs a mode.
  /// @param name The name of the mode.
  /// @param prompt The prompt string.
  /// @param prompt_color The color of *prompt*.
  /// @param history_file The file where to store the mode history.
  mode(std::string name,
       std::string history_file,
       int history_size = 1000,
       bool unique_history = true,
       std::string prompt = ">",
       char const* prompt_color = color::none,
       char const* shell_name = "sash",
       char const* completion_key = "\t")
    : el_{shell_name, std::move(history_file),
          history_size, unique_history, completion_key},
      root_{std::make_shared<Command>(nullptr, el_.get_completer(),
                                      std::move(name), std::string{})}
  {
    el_.set_prompt(std::move(prompt), prompt_color);
  }

  /// Adds a sub-command to this command.
  /// @param name The name of the command.
  /// @param desc A one-line description of the command.
  /// @returns If successful, a valid pointer to the newly created command.
  command_ptr add(std::string name, std::string desc)
  {
    return root_->add(std::move(name), std::move(desc));
  }

  /// Assigns a callback handler for unknown commands.
  /// @param f The function to execute for unknown commands.
  void on_unknown_command(command_cb f)
  {
    root_->on_command(std::move(f));
  }

  /// Assigns a callback handler for unknown commands.
  /// @param f The function to execute for unknown commands.
  void on_complete(completion_cb f)
  {
    el_.get_completer()->on_completion(std::move(f));
  }

  /// Registers a completion with this mode.
  /// @param The string to register.
  void add_completion(std::string str)
  {
    el_.get_completer()->add_completion(std::move(str));
  }

  /// Replaces the completions associated with a set of new ones.
  /// @param completiosn The new completions.
  void replace_completions(std::vector<std::string> completions)
  {
    el_.get_completer()->replace_completions(std::move(completions));
  }

  /// Execute a command line.
  command_result execute(std::string const& line) const
  {
    return root_->execute(line);
  }

  /// Retrieves the name of this mode.
  /// @returns The name of this mode
  std::string const& name() const
  {
    return root_->name();
  }

  /// Retrieves the autogenerated help string for this mode.
  /// @param indent The number of spaces to indent the help text.
  /// @returns The help string for this mode.
  std::string help(size_t indent = 0) const
  {
    return root_->help(indent);
  }

  /// Returns a reference to the CLI backend used by this mode.
  Backend& backend()
  {
    return el_;
  }

private:
  Backend el_;
  command_ptr root_;
};

} // namespace sash

#endif // SASH_MODE_HPP
