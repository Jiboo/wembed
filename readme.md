[![Build Status](https://travis-ci.com/Jiboo/wembed.svg?branch=dwarf)](https://travis-ci.com/Jiboo/wembed)

WASM embedding library using LLVM's Orc JIT.

Branch info
-----------

This branch has experimental DWARF (yurydelendik/webassembly-dwarf) to LLVM IR
debug metadata, partially working on some simple C input (source+lines only).

It depends on some LLVM patches before merging into master:
  - https://reviews.llvm.org/D58323
  - https://reviews.llvm.org/D58334

Also you should remove the value of DEBUG_PREFIX_MAP in wasmception/Makefile so
that file path don't start with `wasmception://` and GDB may find them.
``
TODO:
  - DW_TAG_lexical_block
  - DW_TAG_variable
  - DW_TAG_inlined_subroutine
  - Macro sections

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
------------

- LLVM libraries (nightly)
- boost (endian and functional)

Optional (required for tests):
- google-test (unittests and running testsuite)
- wabt (used to generate wasm binaries from testsuite text)
- wasmception (used to generate wasm32 from some C test cases)
