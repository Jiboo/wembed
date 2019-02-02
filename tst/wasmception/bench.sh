#!/usr/bin/env bash

ROOT_DIR=../..
BUILD_DIR=$ROOT_DIR/cmake-build-debug
WASMCEPTION_BUILD_DIR=$BUILD_DIR/tst/wasmception
DRIVER_PATH=$BUILD_DIR/wembed-driver

for WASM in $WASMCEPTION_BUILD_DIR/*.wasm; do
    WAT=${WASM/.wasm/.wat}
    wasm2wat $WASM -o $WAT
    echo
    echo "Running test " $WASM "..."
    echo
    $DRIVER_PATH $WASM
done
