# json lib 

> DBJ port of [JSMN](https://github.com/zserge/jsmn.git)

## Project principles

- Not "cross platform"
  - one platform: Windows
  - one IDE: Visual Studio
    - might provide `Makefile` for local GCC
    - CMake has no point as it produces Visual Studio project in a very roundabout way
      - it is a "cross platform" build system
    - Might look into [`xmake`](https://xmake.io/#/) if there is interest and time
  - primary compiler: `clang-cl`
    - will not compile under `cl`
- language is C11 
  - c++17 API is of a secondary concern
    - future C++ API will depend on this C core

--- 
This work &copy; 2021 by dbj@dbj.org

---
Original is at: https://github.com/zserge/jsmn.git