# ztl

This is a repository of header-only, single-file libraries -- something like [nothings/stb](https://github.com/nothings/stb),
but in C++, with templates. duh.


# what

library | description
--------|------------
zpr     | (freestanding) type-safe printf; std::format for C++17, but not so bloated and overkill
zbuf    | (freestanding) lightweight stretchy buffers and spans, and a superior string_view
znet    | lightweight socket wrapper, with TCP, UDP, and SSL support
zurl    | http/1.1 request library, depends on zbuf and znet


# use

Libraries marked *(freestanding)* can be configured to depend only on the freestanding set of C/C++ headers (usually `stdint.h`, `stddef.h`, that kind of stuff). These libraries can also be configured as not necessarily freestanding, but without using the C++ STL (there is a subtle difference).

Usage is as simple as copying the required header files into your project and `#include`-ing them. Some of them require `_IMPLEMENTATION` macros, read their documentation for more info. Also, some of them require others (eg. `zurl` requires both `znet` and `zbuf`), so those must be copied as well.

All libraries are licensed under the Apache License 2.0.


# disclaimer

`zpr` is production ready. The output should be consistent with `printf` using the same format specifiers, but... no guarantees.

`znet` is not exactly production-ready, but it is being used in production.

`zbuf` is production ready, and is feature-complete.

`zurl`  is not production ready, and not even feature-complete.
