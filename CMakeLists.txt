cmake_minimum_required(VERSION 3.8.2)
project(wembed)

set(TESTSUITE_PATH ${CMAKE_SOURCE_DIR}/dep/testsuite)
# https://github.com/WebAssembly/spec.git

set(ENABLE_NATIVE_CODE_DUMP FALSE)
# if enabled, dump x86 assembly after context construction

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

find_package(LLVM 8.0 REQUIRED CONFIG)
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

add_subdirectory(dep/try_signal)
include_directories(dep/try_signal)

include_directories(inc)
file(GLOB_RECURSE WEMBED_INC "inc/**.hpp")
file(GLOB_RECURSE WEMBED_SRC "src/**.cpp")

add_library(wembed ${WEMBED_INC} ${WEMBED_SRC})
target_link_libraries(wembed ${LLVM_LIBS})

add_executable(tstgen tst/test_gen.cpp)
target_link_libraries(tstgen wembed stdc++fs)

set(TESTSUITE_OUTPUT ${CMAKE_BINARY_DIR}/tests)
file(MAKE_DIRECTORY ${TESTSUITE_OUTPUT})

set(TESTSUITE_SRC)
file(GLOB TESTSUITE_FILES "${TESTSUITE_PATH}/*.wast")
#set(TESTSUITE_FILES "${TESTSUITE_PATH}/conversions.wast")
message("${TESTSUITE_FILES}")
foreach(TESTCASE_WAST ${TESTSUITE_FILES})
  string(REGEX REPLACE "${TESTSUITE_PATH}/(.*).wast" "${TESTSUITE_OUTPUT}/\\1.cpp" TESTCASE_CPP "${TESTCASE_WAST}")
  add_custom_command(OUTPUT ${TESTCASE_CPP}
    COMMAND tstgen ${TESTCASE_WAST} ${TESTCASE_CPP}
    DEPENDS tstgen ${TESTCASE_WAST}
  )
  list(APPEND TESTSUITE_SRC ${TESTCASE_CPP})
endforeach()

include_directories(tst)
add_executable(generated_tests tst/test.cpp ${TESTSUITE_SRC} tst/test.hpp tst/try_signal.cpp)
target_link_libraries(generated_tests gtest wembed try_signal ${LLVM_LIBS} stdc++fs)
