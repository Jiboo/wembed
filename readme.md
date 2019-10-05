[![Build Status](https://travis-ci.com/Jiboo/wembed.svg?branch=dwarf)](https://travis-ci.com/Jiboo/wembed)

WASM embedding library using LLVM's Orc JIT.

Branch info
-----------

This branch has experimental DWARF (yurydelendik/webassembly-dwarf) to LLVM IR
debug metadata, partially working on some simple C input (functions/files/lines/parameters).

It depends on some LLVM patches before merging into master:
  - https://reviews.llvm.org/D58323
  - https://reviews.llvm.org/D58334

TODO:
  - DW_TAG_lexical_block
  - DW_TAG_variable
    * https://reviews.llvm.org/D52634
  - DW_TAG_inlined_subroutine
  - Macro sections
    * Apparently macro are not included in debug info by default, not sure if useful

FIXME:
  - Refactor to parse debug info earlier? As WASM was designed to be parsed with one-pass, wembed is too, although we need to book-keep a lot of info (notably, every instr offset) because debug sections are after code section
  - How to ask debugger to show pointers as 32bit? And offset them to our memory base?

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
- c/c++ api proposal

WASI support
------------

WASI support is minimal, unstable and probably unsafe.

Dependencies
------------

- LLVM libraries (nightly)
- boost (endian and functional)

Optional (required for tests):
- google-test (unittests and running testsuite)
- wabt (used to generate wasm binaries from testsuite text)
- wasm-libc (used to generate wasm32 from some C test cases)

TODO
----

- load/store opt, don't check overflow if offset == 0, else mark branch without overflow as "likely"
