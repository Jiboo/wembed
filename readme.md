WASM embedding library.
 
Status
------

Project is still in early stage of development and will be used by
[jiboo/sophia](https://github.com/Jiboo/sophia).

Todo/Help wanted
----------------

 - [ ] Imports/exports between modules in same file
 - [ ] Refactor min/max/ceil... intrinsics into IR builder calls

Known bugs
----------

Cases in call.wast:

    (assert_exhaustion (invoke "runaway") "call stack exhausted")
    (assert_exhaustion (invoke "mutual-runaway") "call stack exhausted")

- Thus no-effect recursive calls gets removed by the optimizer.

Cases in call_indirect.wast:

    (assert_exhaustion (invoke "runaway") "call stack exhausted")
    (assert_exhaustion (invoke "mutual-runaway") "call stack exhausted")

- Thus recursive calls get optimized into jumps, not causing a stack overflow.
- Were commented out in testsuite fork

Cases in fac.wast:

    (assert_exhaustion (invoke "fac-rec" (i64.const 1073741824)) "call stack exhausted")
    
- fac-rec gets optimized as a non recursive, preventing stack exhaust

Cases in int_exprs.wast:

- Some FPE errors are optimized out, preventing the crash in some assert_trap.

Cases in names.wast:

- The Clang C API doesn't allow to pass a string size to LLVMGetFunctionAddress,
making it difficult to use names with '\0'.

Cases in traps.wast:

- assert_trap failures due to some dead code elimination.

Cases in unreached_invalid.wast:

- Lots of failed assert_invalid, due to the fact that we do no validation during unreachable skip path
