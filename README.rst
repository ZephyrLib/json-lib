
Json Lib
========
Json serialization/deserialization lib

Usage
-----
See tests

Features
--------
- Templated solution

  - control over json object class implementation

- Stack-based parsing algorithm

CMake
-----

- Include ``./include`` path

  - ``target_include_directories(<target> <PRIVATE | PUBLIC | ...> json-lib/include)``

- ``#include "json-lib/json.hpp"``

Tested compilers
----------------
- msvc 19
- g++ 8.3.0
