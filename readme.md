WASM embedding library.

Status
------

Project is still in early stage of development and will be used by
[jiboo/sophia](https://github.com/Jiboo/sophia).

Except for unreached_invalid tests, the rest of the official [testsuite](https://github.com/WebAssembly/testsuite)
is passing.

Out of scope
------------

- No WAST support, only binary modules can be imported
- Any post-MVP feature below phase 4

Post WASM-MVP support
---------------------

Phase 5 (The Feature is Standardized):

- [x] [Import/export of mutable globals](https://github.com/WebAssembly/proposals/issues/5)

Phase 4 (Standardize the Feature):

- [ ] [Sign-extension operators](https://github.com/WebAssembly/proposals/issues/9)
- [ ] [Non-trapping float-to-int conversions](https://github.com/WebAssembly/proposals/issues/11)

TODO/Help wanted
----------------

- Do validation when skipping unreachable code.
- C bindings
- Port/test the [try_signal fork](https://github.com/Jiboo/try_signal) on other platforms than Linux
