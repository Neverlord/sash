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
 * Distributed under the 3-clause BSD License.                                *
 * See accompanying file LICENSE.                                             *
\******************************************************************************/

#ifndef SASH_VARIABLES_ENGINE_HPP
#define SASH_VARIABLES_ENGINE_HPP

#include <map>
#include <string>
#include <iterator>
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
    auto valid_varname_char = [](char c)
    {
      return std::isalnum(c) || c == '_';
    };
    using iter = typename std::string::const_iterator;
    char c = '\0'; // current character
    char lastc = ' '; // the last character, needed to distinct "$" from "\$"
    iter pos = in.begin(); // our current position in the stream
    iter last = in.end();
    enum
    {
        // traversing input, copying characters as we go
        traverse,
        // just read a $ indicating the start of a variable name
        after_dollar_sign,
        // read a variable name in the form of $name
        read_variable,
        // read a variable name in the form of ${name}
        read_braced_variable
    }
    state = traverse;
    auto i = in.begin();
    raii_error_string scoped_err{err};
    auto set_error = [&]() -> std::ostream&
    {
      return scoped_err.oss << "syntax error at position "
                            << std::to_string(std::distance(in.begin(), i))
                            << ": ";
    };
    auto flush = [&]() -> bool
    {
        switch (state)
        {
          case traverse:
            if (pos != last)
              out.insert(out.end(), pos, i);
            return true;
          case after_dollar_sign:
            if (i == last)
            {
              set_error() << "$ at end of line";
            }
            else if (*i == '$')
            {
              set_error() << "$$ is not a valid expression";
            }
            else
            {
              set_error() << "unexpected character '"
                          << *i << "' after $";
            }
            out.clear();
            return false;
          // pos is always set to the first character of the variable name,
          // i is always set to the first character that's not part of it
          case read_braced_variable:
            if (! valid_varname_char(c) && c != '}')
            {
              set_error() << "'" << c
                          << "' is an invalid character inside ${...}";
              return false;
            }
            // else:: fall through
          case read_variable:
            std::string varname(pos, i);
            auto j = variables_.find(varname);
            out += j == variables_.end() ? "" : j->second;
            state = traverse;
            if (state == read_braced_variable)
              pos = i + 1;
            else
              pos = i;
            return true;
        }
        return false; // should be unreachable
    };
    while (i != last)
    {
      c = *i;
      switch (c)
      {
        case '=':
          // assignment lines are parsed as '([a-zA-Z0-9_]+)=(.+)' => check
          if (! sub_parse && i != pos && pos == in.begin()
              && std::all_of(pos, i, valid_varname_char))
          {
              std::string key(pos, i);
              std::string val_input(i + 1, in.end());
              std::string value;
              // assignments don't produce an output
              out.clear();
              // our value can in turn have variables
              parse(err, val_input, value, true);
              if (err.empty())
                variables_.emplace(std::move(key), std::move(value));
              return;
          }
          break;
        case '$':
          if (lastc == '\\')
            break;
          // write previous input
          if (! flush())
            return;
          state = after_dollar_sign;
          pos = i + 1;
          break;
        case '{':
          if (state == after_dollar_sign)
          {
            pos = i + 1; // '{' is not part of the variable name
            state = read_braced_variable;
          }
          break;
        case '}':
          if (state == read_braced_variable)
          {
            if (! flush())
              return;
            pos = i + 1; // '}' won't be part of the output
            break;
          }
          // else: fall through
        default:
          if (! valid_varname_char(c) && state != traverse)
          {
            if (! flush())
              return;
          }
          else if (state == after_dollar_sign)
          {
            pos = i;
            state = read_variable;
          }
          break;
      }
      lastc = c;
      ++i;
    }
    if (state == read_braced_variable)
    {
      err = "syntax error: missing '}' at end of line";
    }
    flush();
  }

  /// Sets a variable to given value.
  void set(const std::string& identifier, std::string value)
  {
    variables_[identifier] = std::move(value);
  }

  /// Unsets a variable.
  void unset(const std::string& identifier)
  {
    variables_.erase(identifier);
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
  struct raii_error_string
  {
    raii_error_string(std::string& ref) : err(ref)
    {
      // nop
    }
    ~raii_error_string()
    {
      // leave err untouched if no error occured
      auto str = oss.str();
      if (! str.empty())
        err = std::move(str);
    }
    std::ostringstream oss;
    std::string& err;
  };

  Container variables_;
};

} // namespace sash

#endif // SASH_VARIABLES_ENGINE_HPP
