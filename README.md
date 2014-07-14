SASH
====

SASH is an open source C++ library providing a *SA*ne *SH*ell infrastructure.
It allows developers to build a fully customizable shell environment tailored
to their own needs. Unlike other shell APIs, SASH makes use of high-level
concepts and lightweight abstractions. Modes and commands can be added to the
shell by passing callback objects, e.g., lambda expressions. SASH does *not*
implement a full TTY. Instead, SASH supports pluggable backends and ships with
a libedit backend.


Get the Sources
---------------

* git clone https://github.com/Neverlord/sash.git
* cd sash


First Steps
-----------

* `./configure`
* `make`
* `./build/bin/simple_shell`


Dependencies
------------

* CMake
* libedit (for the default backend)


Supported Compilers
-------------------

* GCC >= 4.8
* Clang >= 3.2


Supported Operating Systems
---------------------------

* Linux
* Mac OS X
