# Lua 5.4.8

Unmodified official Lua source from https://www.lua.org/ftp/lua-5.4.8.tar.gz.

Archive SHA-256: `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`

The upstream copyright and MIT permission notice are in `src/lua.h` and
`doc/readme.html`. Storyland compiles the upstream C files as C++, which Lua
supports, to unwind C++ host data correctly when a Lua API check raises an error.
The standalone `lua.c` and `luac.c` programs are excluded from the static library.
