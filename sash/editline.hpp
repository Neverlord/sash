#ifndef SASH_EDITLINE_HPP
#define SASH_EDITLINE_HPP

#include <string>
#include <functional>
#include <vector>
#include "vast/util/trial.h"

namespace vast {
namespace util {

/// Wraps command line editing functionality provided by `libedit`.
class editline
{
public:


  /// Constructs an editline context.
  /// @param The name of the edit line context.
  editline(char const* name = "vast");

  ~editline();

  /// Defines the character reading function, which should store the character
  /// in its argument and return the number of characters read. This is the
  /// equivalent of `EL_GETCFN`.
  ///
  /// @param handler The handler to read a character.
  void on_char_read(std::function<int(char*)> handler);

  /// Sources an editline config.
  ///
  /// @param filename If not `nullptr` the config file name and otherwise
  /// attempts to read `$PWD/.editrc` and then `$HOME/.editrc`.
  ///
  /// @return `true` on success.
  bool source(char const* filename = nullptr);

  /// Sets the prompt.
  /// @param p The new prompt.
  void set(prompt p);

  /// Sets a history.
  /// @param hist The history to use.
  void set(history& hist);

  /// Sets a new completer.
  /// @param comp The completer to use
  void set(completer comp);

  /// Retrieves a character from the TTY.
  /// @param c The result parameter containing the character.
  /// @returns `true` on success, `false` on EOF, and an error otherwise.
  trial<bool> get(char& c);

  /// Retrieves a line from the TTY.
  /// @param line The result parameter containing the line.
  /// @returns `true` on success, `false` on EOF, and an error otherwise.
  trial<bool> get(std::string& line);

  /// Pushes a string back into the input stream.
  /// @param str The string to push.
  void push(char const* str);

  /// Adds a string to the current line.
  /// @param str The string to add.
  void put(std::string const& str);

  /// Checks whether editline received EOF on an empty line.
  /// @returns `true` if EOF occurred on an empty line.
  bool eof();

  /// Retrieves the current line.
  /// @return The current line.
  std::string line();

  /// Retrieves the current cursor position.
  /// @return The position of the cursor.
  size_t cursor();

  /// Resets the TTY and editline parser.
  void reset();

  /// Retrieves the completion context.
  completer& completion();

private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace util
} // namespace vast

#endif
