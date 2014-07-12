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

#ifndef SASH_COMMAND_HPP
#define SASH_COMMAND_HPP

#include <memory>
#include <string>
#include <sstream>
#include <cassert>
#include <numeric>
#include <iomanip>
#include <algorithm>

#include "sash/completer.hpp"

namespace sash {

/// The return code for commands. Each command invoation can
/// either succeed, perform a NOP in case no input was provided,
/// or fail because no command handler was found.
enum command_result
{
  command_succeeded,
  command_nop,
  no_command_handler_found
};

/// A representation of command with zero or more arguments.
/// @tparam Completer The type of our completion context.
/// @tparam CommandCallback A functor providing the signature
/// `command_result (string::iterator, string::iterator)`. The
/// callback is invoked whenever a command is executed. The first iterator
/// points to the first input character, the second iterator to the end
/// of the string.
template<class Completer, class CommandCallback>
class command : public std::enable_shared_from_this<command<Completer,
                                                            CommandCallback>>
{
  command(command const&) = delete; // you shall not pass
  command& operator=(command const&) = delete; // you neither

public:
  /// An iterator to the command line input.
  using const_iterator = std::string::const_iterator;

  /// A (smart) pointer to a command.
  using pointer = std::shared_ptr<command>;

  /// A (smart) pointer to the completion context
  using completer_pointer = std::shared_ptr<Completer>;

  using callback_type = CommandCallback;

  /// Constructs a command.
  /// @param name The parent command.
  /// @param comp The completion context this command exists in.
  /// @param name The name of the command.
  /// @param desc A one-line description of the command for the help.
  /// @pre `mode`
  command(pointer parent,
          completer_pointer comp,
          std::string name,
          std::string desc)
      : parent_{std::move(parent)},
        completer_{std::move(comp)},
        name_{std::move(name)},
        description_{std::move(desc)}
  {
    if (parent_)
    {
      completer_->add_completion(absolute_name() + ' ');
    }
    // else: I am ROOT
    //       The only one
    //       I will bring back
    //       Freedom to your heart
    //       You won't believe
    //       That blind can't see
    //       No one else before me ever knew
    //       The way to paradise
    //       For another bloody crime
    //       I shall return
  }

  /// Adds a sub-command to this command.
  /// @param name The name of the command.
  /// @param desc A one-line description of the command.
  /// @returns If successful, a valid pointer to the newly created command.
  pointer add(std::string name, std::string desc)
  {
    if (name.empty() || std::any_of(children_.begin(), children_.end(),
                                    [&](pointer const& child)
                                    { return child->name() == name; }))
    {
      return nullptr;
    }
    children_.push_back(std::make_shared<command>(this->shared_from_this(),
                                                  completer_,
                                                  std::move(name),
                                                  std::move(desc)));
    return children_.back();
  }

  /// Assigns a callback handler for arguments to this command.
  /// @param f The function to execute for the command arguments.
  void on_command(CommandCallback f)
  {
    handler_ = std::move(f);
  }

  /// Retrieves the name of this very command.
  /// @returns The name of this command.
  inline std::string const& name() const
  {
    return name_;
  }

  /// Retrieves the absolute name of the path from the root command.
  /// @returns A space-separted command sequence.
  std::string absolute_name() const
  {
    if (! parent_)
      // the root command has no name
      return "";
    // we traverse from our position back to root, i.e., our name will
    // have the wrong order; we fix this by adding the reversed string for
    // each parent and then reverse the whole resulting string in place
    std::string result;
    for (auto i = parent_; i != nullptr; i = i->parent_)
    {
      if (! result.empty())
        result += ' '; // separate names with one white space
      result.insert(result.end(), i->name().rbegin(), i->name().rend());
    }
    assert(!result.empty());
    std::reverse(result.begin(), result.end());
    return result;
  }

  /// Retrieves the autogenerated help string for this command.
  /// @param indent The number of spaces to indent the help text.
  /// @returns The help string for this command.
  std::string help(size_t indent = 0) const
  {
    if (children_.empty())
      return "";
    auto binary_op = [](size_t old_max, pointer const& cmd)
    {
      return std::max(old_max, cmd->name().size());
    };
    size_t max_len = std::accumulate(children_.begin(), children_.end(),
                                     size_t{0}, binary_op);
    std::string padding(indent, ' ');
    std::ostringstream oss;
    oss << std::left;
    for (auto& cmd : children_)
      // always separate name & desciption by at least two spaces
      oss << padding << std::setw(max_len) << cmd->name()
          << "  " << cmd->description() << "\n";
    return oss.str();
  }

  /// Execute a command line.
  command_result execute(const_iterator first, const_iterator last) const
  {
    auto delim = std::find(first, last, ' ');
    for (auto& cmd : children_)
    {
      auto dist = static_cast<size_t>(std::distance(first, delim));
      auto& n = cmd->name();
      if (dist == n.size() && std::equal(first, delim, n.begin())) {
        return cmd->execute(delim == last ? std::string{}
                                          : std::string(delim + 1, last));
      }
    }
    return handler_ ? handler_(first, last) : no_command_handler_found;
  }

  /// Execute a command line.
  inline command_result execute(std::string const& line) const
  {
    return execute(line.begin(), line.end());
  }

private:
  pointer parent_;
  completer_pointer completer_;
  std::vector<pointer> children_;
  std::string name_;
  std::string description_;
  CommandCallback handler_;
};

} // namespace sash

#endif // SASH_COMMAND_HPP
