cmake_minimum_required(VERSION 3.8.2)
project(wembed)

set(TEST_SUITE_PATH ${CMAKE_SOURCE_DIR}/spec/test/core)
# https://github.com/WebAssembly/spec.git

set(ENABLE_NATIVE_CODE_DUMP FALSE)
# if enabled, dump x86 assembly after context construction

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

find_package(LLVM 6.0 REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(LLVM_LIBS
  Analysis Core  ExecutionEngine
  IPO MC MCJIT RuntimeDyld OrcJIT
  ScalarOpts Support Target
  nativecodegen
)

if (${ENABLE_NATIVE_CODE_DUMP})
  add_definitions(-DWEMBED_NATIVE_CODE_DUMP)

  llvm_map_components_to_libnames(LLVM_EXTRAS
    webassemblydisassembler x86disassembler webassemblydesc
    x86desc webassemblyinfo x86info)

  set(LLVM_LIBS ${LLVM_LIBS} ${LLVM_EXTRAS})
endif()

include_directories(inc)
file(GLOB_RECURSE WEMBED_INC "inc/**.hpp")
file(GLOB_RECURSE WEMBED_SRC "src/**.cpp")

add_library(wembed ${WEMBED_INC} ${WEMBED_SRC})
target_link_libraries(wembed ${LLVM_LIBS})

add_executable(tstgen tst/test_gen.cpp)
target_link_libraries(tstgen stdc++fs)

include_directories(tst)
set(TESTSUITE_OUTPUT ${CMAKE_BINARY_DIR}/generated_tests.cpp)
add_executable(generated_tests ${TESTSUITE_OUTPUT} ${WEMBED_INC})
target_link_libraries(generated_tests ${LLVM_LIBS} wembed gtest)

add_custom_command(OUTPUT ${TESTSUITE_OUTPUT}
  COMMAND tstgen ${TEST_SUITE_PATH} ${TESTSUITE_OUTPUT}
)
