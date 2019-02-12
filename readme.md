[![Build Status](https://travis-ci.com/Jiboo/wembed.svg?branch=master)](https://travis-ci.com/Jiboo/wembed)

WASM embedding library using LLVM's Orc JIT.

Usage
-----

See [demo.cpp](demo.cpp) for a quick overview on how to use the library.

Status
------

Except for unreached_invalid tests, the rest of the official [testsuite](https://github.com/WebAssembly/testsuite)
is passing.

Support linux x86_64 only.

Post WASM-MVP support
---------------------

- [x] [Import/export of mutable globals](https://github.com/WebAssembly/proposals/issues/5)
- [x] [Sign-extension operators](https://github.com/WebAssembly/proposals/issues/9)
- [x] [Non-trapping float-to-int conversions](https://github.com/WebAssembly/proposals/issues/11)

Out of scope
------------

- WAST support, only binary modules can be imported
- Any post-MVP feature below phase 4
- wasm64

Dependencies
---------------------

- LLVM libraries (8+)
- boost (endian and functional)

Optional (required for tests):
- google-test (unittests and running testsuite)
- wabt (used to generate wasm binaries from testsuite text)
- wasmception (used to generate wasm32 from some C test cases)
