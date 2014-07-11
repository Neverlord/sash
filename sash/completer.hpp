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

#ifndef SASH_COMPLETER_H
#define SASH_COMPLETER_H

#include <string>
#include <vector>
#include <algorithm>

namespace sash {

/// The return code for completion operations. Each operation can
/// succeed or fail because either a) no completion was found or
/// b) no completion handler could be found.
enum completion_result
{
    completion_successful,
    no_completion_found,
    no_completion_handler_found
};

/// A completion context. This class is used as template parameter
/// for the backend implementation.
/// @tparam CompletionCallback A functor providing the signature
/// <tt>string (string, vector<string>)</tt>. The callback is executed on
/// matching prefixes for a candidate string. The first argument represent the
/// prefix which was given, and the second the corresponding matches.
/// The return value, if not empty, represents the string to insert back onto
/// the command line.
template<class CompletionCallback>
class completer
{
public:
    /// Adds a string to complete.
    /// @param str The string to complete.
    /// @returns `true` if *str* did not already exist.
    bool add_completion(std::string str)
    {
      if (std::any_of(strings_.begin(), strings_.end(),
                      [&](std::string const& val) { return val == str; }))
      {
        return false;
      }
      strings_.emplace_back(std::move(str));
      return true;
    }

    /// Removes a registered string from the completer.
    /// @param str The string to remove.
    /// @returns `true` if *str* existed.
    bool remove_completion(std::string const& str)
    {
      auto last = strings_.end();
      auto i = std::find(strings_.begin(), last, str);
      if (i != strings_.end())
      {
        strings_.erase(i);
        return true;
      }
      return false;
    }

    /// Replaces the existing completions with a given set of new ones.
    /// @param completions The new completions to use.
    void replace_completions(std::vector<std::string> completions)
    {
      strings_.swap(completions);
    }

    /// Sets a callback handler for the list of matches.
    /// @param f The function to execute for matching completions.
    void on_completion(CompletionCallback f)
    {
      callback_ = std::move(f);
    }

    /// Completes a given string by calling the given callback.
    /// @param prefix The string to complete.
    /// @returns The result of the completion function.
    completion_result complete(std::string& result,
                               std::string const& prefix) const {
      if (!callback_)
      {
        return no_completion_handler_found;
      }
      if (strings_.empty())
      {
        return no_completion_found;
      }
      std::vector<std::string> matches;
      for (auto& str : strings_)
      {
        if (str.compare(0, prefix.size(), prefix) == 0)
        {
          matches.push_back(str);
        }
      }
      result = callback_(prefix, std::move(matches));
      return completion_successful;
    }

private:
  std::vector<std::string> strings_;
  CompletionCallback callback_;
};

} // namespace sash

#endif // SASH_COMPLETER_H
