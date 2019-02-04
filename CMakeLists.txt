cmake_minimum_required(VERSION 3.8.2)
project(wembed)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

enable_testing()

find_package(LLVM 7.0 REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(LLVM_LIBS
  Analysis Core IPO OrcJIT
  ScalarOpts Support Target
  nativecodegen
)

find_package(Boost)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(dep)
add_subdirectory(dep/try_signal)
include_directories(dep/try_signal)

include_directories(inc)
file(GLOB_RECURSE WEMBED_INC "inc/**.hpp")
file(GLOB_RECURSE WEMBED_SRC "src/**.cpp")

add_library(wembed ${WEMBED_INC} ${WEMBED_SRC})
target_link_libraries(wembed ${LLVM_LIBS})

add_executable(demo demo.cpp)
target_link_libraries(demo wembed try_signal ${LLVM_LIBS})

#add_executable(debug_proto debug_proto.cpp)
#target_link_libraries(debug_proto ${LLVM_LIBS})

add_executable(tstgen tst/test_gen.cpp)
target_link_libraries(tstgen wembed stdc++fs)

set(TESTSUITE_OUTPUT ${CMAKE_BINARY_DIR}/tst/testsuite)
file(MAKE_DIRECTORY ${TESTSUITE_OUTPUT})

set(TESTSUITE_SRC)
set(TESTSUITE_PATH "${CMAKE_SOURCE_DIR}/dep/testsuite")
file(GLOB TESTSUITE_FILES
    "${TESTSUITE_PATH}/*.wast"
    "${TESTSUITE_PATH}/proposals/sign-extension-ops/*.wast"
    "${TESTSUITE_PATH}/proposals/nontrapping-float-to-int-conversions/*.wast")
#set(TESTSUITE_FILES "${TESTSUITE_PATH}/address.wast")
foreach(TESTCASE_WAST ${TESTSUITE_FILES})
  string(REGEX REPLACE "${TESTSUITE_PATH}/(.*).wast" "${TESTSUITE_OUTPUT}/\\1.cpp" TESTCASE_CPP "${TESTCASE_WAST}")
  add_custom_command(OUTPUT ${TESTCASE_CPP}
    COMMAND tstgen ${TESTCASE_WAST} ${TESTCASE_CPP}
    DEPENDS tstgen ${TESTCASE_WAST}
  )
  list(APPEND TESTSUITE_SRC ${TESTCASE_CPP})
  message(STATUS "Adding testsuite file ${TESTCASE_WAST} as ${TESTCASE_CPP}")
endforeach()
#set(TESTSUITE_SRC "")

file(GLOB_RECURSE TESTSUITE_EXTRA_INC "tst/testsuite/**.hpp")
file(GLOB_RECURSE TESTSUITE_EXTRA_SRC "tst/testsuite/**.cpp")

file(GLOB_RECURSE GTESTS_EXTRA_SRC "tst/gtests/**.cpp")
#set(GTESTS_EXTRA_SRC "")

include_directories(tst/testsuite)
add_executable(gtests ${TESTSUITE_SRC} ${TESTSUITE_EXTRA_INC} ${TESTSUITE_EXTRA_SRC} ${GTESTS_EXTRA_SRC})
target_link_libraries(gtests gtest wembed try_signal ${LLVM_LIBS} stdc++fs)
add_test(NAME gtests_test COMMAND gtests)

add_executable(wembed-driver cli/driver.cpp)
target_link_libraries(wembed-driver wembed try_signal ${LLVM_LIBS} stdc++fs)

if (NOT "$ENV{WASMCEPTION}" STREQUAL "")
  set(WASMCEPTION_MODULES)
  set(WASMCEPTION_PATH "${CMAKE_SOURCE_DIR}/tst/wasmception")
  set(WASMCEPTION_OUTPUT ${CMAKE_BINARY_DIR}/tst/wasmception)
  file(MAKE_DIRECTORY ${WASMCEPTION_OUTPUT})
  file(GLOB WASMCEPTION_FILES "${WASMCEPTION_PATH}/*.c")
  foreach(TESTCASE_C ${WASMCEPTION_FILES})
    string(REGEX REPLACE "${WASMCEPTION_PATH}/(.*).c" "\\1" TESTCASE "${TESTCASE_C}")
    set(TESTCASE_WASM "${WASMCEPTION_OUTPUT}/${TESTCASE}.wasm")
    add_custom_command(OUTPUT "${TESTCASE_WASM}"
        COMMAND $ENV{WASMCEPTION}/dist/bin/clang --sysroot=$ENV{WASMCEPTION}/sysroot/ "${TESTCASE_C}" -O0 -g -o "${TESTCASE_WASM}" -Wl,--export=main
        DEPENDS ${TESTCASE_C}
        )
    set(TESTCASE_TARGET "wasmception_${TESTCASE}")
    set(GEN_TESTCASE_TARGET "gen_${TESTCASE_TARGET}")
    #add_executable(${TESTCASE_TARGET} ${TESTCASE_C})
    add_custom_target(${GEN_TESTCASE_TARGET} DEPENDS ${TESTCASE_WASM})
    #add_dependencies(${TESTCASE_TARGET} ${GEN_TESTCASE_TARGET})
    add_dependencies(wembed-driver ${GEN_TESTCASE_TARGET})
    set(TST_TESTCASE_NAME "tst_${TESTCASE_TARGET}")
    add_test(NAME ${TST_TESTCASE_NAME} COMMAND wembed-driver -i ${TESTCASE_WASM} -O0 -- arg0 arg1 arg2 arg3 arg4 arg5 arg6)
    message(STATUS "Adding wasmception file ${TESTCASE_C} as ${TESTCASE_WASM}")
    list(APPEND WASMCEPTION_MODULES ${TESTCASE_WASM})
  endforeach()
endif()
