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

#ifndef SASH_BACKEND_HPP
#define SASH_BACKEND_HPP

#include <string>
#include <cassert>

#include <histedit.h>

#include "sash/color.hpp"

namespace sash {

/// The default backend wraps command line editing functionality
/// as provided by `libedit`.
template<class Completer>
class libedit_backend
{
public:
  /// A (smart) pointer to the completion context
  using completer_pointer = std::shared_ptr<Completer>;

  using completer_type = Completer;

  libedit_backend(const char* shell_name,
                  std::string history_filename = "",
                  int history_size = 1000,
                  bool unique_history = true,
                  std::string completion_key = "\t")
   : history_filename_{std::move(history_filename)},
     completer_{std::make_shared<Completer>()},
     eof_{false}
  {
    el_ = el_init(shell_name, stdin, stdout, stderr);
    assert(el_ != nullptr);
    // Make ourselves available in callbacks.
    set(EL_CLIENTDATA, this);
    // Keyboard defaults.
    set(EL_EDITOR, "vi");
    set(EL_BIND, "^r", "em-inc-search-prev", NULL);
    set(EL_BIND, "^w", "ed-delete-prev-word", NULL);
    // Setup completion.
    using completion_handler = unsigned char (*)(EditLine*, int);
    completion_handler ch_callback = [](EditLine* el, int) -> unsigned char
    {
      libedit_backend* self = nullptr;
      el_get(el, EL_CLIENTDATA, &self);
      assert(self != nullptr);
      std::string line;
      self->get_cursor_line(line);
      std::string completed_line;
      auto cres = self->completer_->complete(completed_line, line);
      if (cres != completed)
        return CC_REFRESH_BEEP;
      el_insertstr(el, completed_line.c_str());
      return CC_REDISPLAY;
    };
    set(EL_ADDFN, "sash-complete", "SASH complete", ch_callback);
    set(EL_BIND, completion_key.c_str(), "sash-complete", NULL);
    // FIXME: this is a fix for folks that have "bind -v" in their .editrc.
    // Most of these also have "bind ^I rl_complete" in there to re-enable tab
    // completion, which "bind -v" somehow disabled. A better solution to
    // handle this problem would be desirable.
    set(EL_ADDFN, "rl_complete", "default complete", ch_callback);
    // Let all character reads go through our custom handler so that we can
    // figure out when we receive EOF.
    using char_read_handler = int (*)(EditLine*, char*);
    char_read_handler cr_callback = [](EditLine* el, char* result) -> int
    {
      libedit_backend* self = nullptr;
      el_get(el, EL_CLIENTDATA, &self);
      assert(self);
      FILE* input_file_handle = nullptr;
      el_get(el, EL_GETFP, 0, &input_file_handle);
      auto empty_line = [el]() -> bool
      {
        auto info = el_line(el);
        return info->buffer == info->cursor && info->buffer == info->lastchar;
      };
      for (;;)
      {
        errno = 0;
        char ch = ::fgetc(stdin);
        if (ch == '\x04' && empty_line())
        {
          errno = 0;
          ch = EOF;
        }
        if (ch == EOF)
        {
          if (errno == EINTR)
          {
            continue;
          }
          else
          {
            self->eof_ = true;
            return 0;
          }
        }
        else
        {
          *result = ch;
          return 1;
        }
      }
    };
    set(EL_GETCFN, cr_callback);
    // Setup for our prompt.
    using prompt_function = char* (*)(EditLine* el);
    prompt_function pf = [](EditLine* el) -> char*
    {
      libedit_backend* self;
      el_get(el, EL_CLIENTDATA, &self);
      assert(self);
      return const_cast<char*>(self->cprompt());
    };
    set(EL_PROMPT, pf);
    // Setup for our history.
    hist = ::history_init();
    assert(hist != nullptr);
    minitrue(H_SETSIZE, history_size);
    minitrue(H_SETUNIQUE, unique_history ? 1 : 0);
    load_history();
  }

  inline ~libedit_backend()
  {
    save_history();
    el_end(el_);
  }

  /// Changes the source for libedit.
  bool source(char const* filename)
  {
    return el_source(el_, filename) != -1;
  }

  /// Resets the TTY and the parser.
  void reset()
  {
    el_reset(el_);
  }

  /// Writes the history to file.
  inline void save_history()
  {
    if (! history_filename_.empty())
    {
      minitrue(H_SAVE, history_filename_.c_str());
    }
  }

  /// Reads the history from file.
  inline void load_history()
  {
    if (! history_filename_.empty())
    {
      minitrue(H_LOAD, history_filename_.c_str());
    }
  }

  /// Appends @p str to the current element of the history, or
  /// behave like {@link enter_history} if there is no current element.
  inline void history_add(std::string const& str)
  {
    minitrue(H_ADD, str.c_str());
    save_history();
  }

  /// Appends @p str to the last new element of the history.
  inline void history_append(std::string const& str)
  {
    minitrue(H_APPEND, str.c_str());
  }

  /// Adds @p str as a new element to the history.
  inline void history_enter(std::string const& str)
  {
    minitrue(H_ENTER, str.c_str());
  }

  /// Sets a (colored) string as prompt for the shell.
  inline void set_prompt(std::string str, color::type strcolor = color::none)
  {
    prompt_.clear();
    add_to_prompt(std::move(str), strcolor);
  }

  /// Appends a (colored) string to the prompt.
  inline void add_to_prompt(std::string str, color::type strcolor = color::none)
  {
    if (! str.empty())
    {
      if (strcolor != color::none)
      {
        prompt_ += strcolor;
        prompt_ += std::move(str);
        prompt_ += color::reset;
      }
      else
      {
        prompt_ += std::move(str);
      }
    }
  }

  /// Returns the current prompt.
  std::string const& prompt() const
  {
    return prompt_;
  }

  /// Shortcut for <tt>prompt().c_str()</tt>.
  const char* cprompt() const
  {
    return prompt_.c_str();
  }

  /// Checks whether we've reached the end of file.
  inline bool eof() const
  {
    return eof_;
  }

  /// Reads a character.
  bool read_char(char& c)
  {
    return ! eof() && get(&c) == 1;
  }

  /// Reads a line.
  bool read_line(std::string& line)
  {
    if (eof())
    {
      return false;
    }
    line.clear();
    raii_set guard{el_, EL_PREP_TERM};
    int n;
    auto str = el_gets(el_, &n);
    if (n == -1 || eof())
    {
      return false;
    }
    if (str != nullptr)
    {
      while (n > 0 && (str[n - 1] == '\n' || str[n - 1] == '\r'))
      {
        --n;
      }
      line.assign(str, n);
    }
    return true;
  }

  void get_current_line(std::string& line)
  {
    auto info = el_line(el_);
    line.assign(
        info->buffer,
        static_cast<std::string::size_type>(info->lastchar - info->buffer));
  }

  void get_cursor_line(std::string& line)
  {
    auto info = el_line(el_);
    line.assign(
        info->buffer,
        static_cast<std::string::size_type>(info->cursor - info->buffer));
  }

  /// Returns the current cursor position.
  size_t cursor()
  {
    auto info = el_line(el_);
    return info->cursor - info->buffer;
  }

  /// Resizes the shell.
  void resize()
  {
    el_resize(el_);
  }

  /// Rings a bell.
  void beep()
  {
    el_beep(el_);
  }

  void push(char const* str)
  {
    el_push(el_, str);
  }

  void insert(std::string const& str)
  {
    el_insertstr(el_, str.c_str());
  }

  completer_pointer get_completer()
  {
      return completer_;
  }

private:
  // The Ministry of Truth. Its purpose is to
  // rewrite history over and over again...
  template<typename... Ts>
  inline void minitrue(int flag, Ts... args) {
    ::history(hist, &hist_event, flag, args...);
  }

  template<typename... Ts>
  void get(int flag, Ts... args) {
    el_get(el_, flag, args...);
  }

  template<typename... Ts>
  void set(int flag, Ts... args) {
    el_set(el_, flag, args...);
  }

  // RAII enabling of editline settings.
  struct raii_set
  {
    raii_set(EditLine* el, int flag)
      : el{el}, flag{flag}
    {
      el_set(el, flag, 1);
    }

    ~raii_set()
    {
      assert(el);
      el_set(el, flag, 0);
    }

    EditLine* el;
    int flag;
  };

  EditLine* el_;
  History* hist;
  HistEvent hist_event;
  std::string history_filename_;
  std::string prompt_;
  std::string comp_key_;
  completer_pointer completer_;
  bool eof_;
};

} // namespace sash

#endif // SASH_BACKEND_HPP