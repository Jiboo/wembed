WASM embedding library.
 
Status
------

Project is still in early stage of development and will be used by
[jiboo/sophia](https://github.com/Jiboo/sophia).

Todo/Help wanted
----------------

 - [ ] Imports/exports between modules in same file

Known bugs
----------

Cases in unreached_invalid.wast:

- Lots of failed assert_invalid, due to the fact that there is no validation during unreachable skip path
