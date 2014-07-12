#include <iostream>

#include "sash/sash.hpp"
#include "sash/libedit_backend.hpp" // our backend
#include "sash/variables_engine.hpp"

using namespace std;

int main()
{
  using char_iter = string::const_iterator;
  using sash::command_result;
  sash::sash<sash::libedit_backend>::type cli;
  string line;
  auto mptr = cli.mode_add("default", "SASH> ", sash::color::blue);
  cli.mode_push("default");
  cli.add_preprocessor(sash::variables_engine<>::create());
  bool done = false;
  mptr->add_all({
    {
      "quit", "terminates the whole thing",
      [&](string& err, char_iter first, char_iter last) -> command_result
      {
        if (first == last)
        {
          done = true;
          return sash::executed;
        }
        err = "quit: to many arguments (none expected)";
        return sash::no_command;
      }
    },
    {
      "echo", "prints its arguments",
      [](string&, char_iter first, char_iter last) -> command_result
      {
        copy(first, last, ostream_iterator<char>(cout));
        cout << endl;
        return sash::executed;
      }
    },
    {
      "help", "prints this text",
      [&](string& err, char_iter first, char_iter last) -> command_result
      {
        if (first == last)
        {
          // doin' it complicated!
          std::string cmd = "echo ";
          cmd += cli.current_mode().help();
          return cli.process(cmd);
        }
        err = "help: to many arguments (none expected)";
        return sash::no_command;
      }
    }});
  while (!done)
  {
    cli.read_line(line);
    switch (cli.process(line))
    {
      default:
        break;
      case sash::nop:
        break;
      case sash::executed:
        cli.append_to_history(line);
        break;
      case sash::no_command:
        cout << sash::color::red
             << cli.last_error()
             << sash::color::reset
             << endl;
        break;
    }
  }
}
