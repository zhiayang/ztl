# ztl

This is a repository of header-only, single-file libraries -- something like [nothings/stb](https://github.com/nothings/stb),
but in C++, with templates. duh.


# what

library | description
--------|------------
zpr     | type-safe printf; std::format for C++17, but not so bloated and overkill
zfu     | functional utilities. **best not to use this library**.


# use

Each header file is standalone, and includes minimal headers from the C++ STL. Just copy and paste it into
your project, as header-only libraries should be.

All libraries are licensed under the Apache License 2.0.


# disclaimer

`zpr` is probably production ready. The output should be consistent with `printf` using the same format
specifiers, but... no guarantees.

`zfu` isn't really optimised very well, so probably don't use it.
