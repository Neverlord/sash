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

#ifndef SASH_SASH_HPP
#define SASH_SASH_HPP

#include <string>
#include <vector>
#include <functional>

#include "sash/mode.hpp"
#include "sash/color.hpp"
#include "sash/command.hpp"
#include "sash/completer.hpp"
#include "sash/command_line.hpp"

/// The namespace of the SAne SHell wrapper.
namespace sash {

/// The default type for completion callbacks.
using completion_cb = std::function<std::string (std::string const&,
                                                 std::vector<std::string>)>;

/// The default type for command callbacks.
using command_cb = std::function<command_result (std::string&,
                                                 std::string::const_iterator,
                                                 std::string::const_iterator)>;

/// The default type for preprocessors.
using preprocessor_fun = std::function<void (std::string&,
                                             const std::string&,
                                             std::string&)>;

/// Utility class that fuses all types together and provides
/// a typedef @p type for the actual CLI we want.
template<template<class> class Backend,
         class CompletionCallback = completion_cb,
         class CommandCallback = command_cb,
         class Preprocessor = preprocessor_fun>
struct sash
{
  /// The type of our completion context.
  using completer_type = completer<CompletionCallback>;

  /// The type of our backend.
  using backend_type = Backend<completer_type>;

  /// The type of a command.
  using command_type = command<completer_type, command_cb>;

  /// The type of our CLI.
  using type = command_line<backend_type, command_type, preprocessor_fun>;
};

} // namespace sash

#endif // SASH_SASH_HPP
