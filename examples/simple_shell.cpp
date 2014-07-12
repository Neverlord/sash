#include "sash/sash.hpp"
#include "sash/libedit_backend.hpp" // pick a backend

int main()
{
  using char_iter = std::string::const_iterator;
  sash::sash<sash::libedit_backend>::type cli;
  std::string line;
  auto mptr = cli.mode_add("default", "> ");
  cli.mode_push("default");
  bool done = false;
  mptr->add("quit", "terminates the whole thing")
  ->on([&](char_iter, char_iter) -> sash::command_result {
      done = true;
      return sash::executed;
  });
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
        break;
      case sash::no_command:
        break;
    }
  }
}
