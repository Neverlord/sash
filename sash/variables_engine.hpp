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

#ifndef SASH_VARIABLES_ENGINE_HPP
#define SASH_VARIABLES_ENGINE_HPP

#include <map>
#include <string>
#include <algorithm>

namespace sash {

/// An example implementation for a variables engine that is
/// convertible to a std::function object.
template<typename Container = std::map<std::string, std::string>>
class variables_engine
{
public:
  /// The type of a std::function wrapper.
  using functor = std::function<void (std::string&,
                                      const std::string&,
                                      std::string&)>;

  /// Parses an input line and replaces all variables in the output.
  void parse(std::string& err, const std::string& in,
             std::string& out, bool sub_parse = false)
  {
    auto valid_char = [](char c)
    {
      return std::isalnum(c) || c == '_';
    };
    using iter = typename std::string::const_iterator;
    char lastc = ' '; // the last character, needed to distinct "$" from "\$"
    iter pos = in.begin(); // our current position in the stream
    iter last = in.end();
    for (auto i = in.begin(); i != last; ++i)
    {
      switch (*i)
      {
        case '=':
          // assignment lines are parsed as '([a-zA-Z0-9_]+)=(.+)' => check
          if (! sub_parse && i != pos && pos == in.begin() && std::all_of(pos, i, valid_char))
          {
              std::string key(pos, i);
              std::string val_input(i + 1, in.end());
              std::string value;
              // our value can in turn have variables
              parse(err, val_input, value, true);
              variables_.emplace(std::move(key), std::move(value));
              out.clear();
              return;
          }
      }
      lastc = *i;
    }
    if (pos != last) out.insert(out.end(), pos, last);
  }

  /// Factory function to create a std::function using this implementation.
  /// @param predef A set of predefined variables to initialize the engine.
  static functor create(Container predef = Container{})
  {
    auto ptr = std::make_shared<variables_engine>();
    ptr->variables_ = std::move(predef);
    return [ptr](std::string& err, const std::string& in, std::string& out)
    {
      ptr->parse(err, in, out);
    };
  }

private:
  Container variables_;
};

} // namespace sash

#endif // SASH_VARIABLES_ENGINE_HPP
