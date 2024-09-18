#!/bin/bash

WORK_DIR=$(pwd)
SCRIPT_DIR=$(dirname "$0")
BUILD_DIR=$SCRIPT_DIR/../build
MOVESENSE_CORELIB_PATH=../movesense-device-lib/MovesenseCoreLib

if [[ "$1" == "release" ]]
then
    BUILD_TYPE=Release
elif [[ "$1" == "debug" ]]
then
    BUILD_TYPE=Debug
else
    echo "Usage: $(basename -- $0) [debug|release]"
    exit 1
fi

abort() {
    echo ERROR: $1
    cd $WORK_DIR
    exit 1
}

echo "Moving to build directory...."
cd $BUILD_DIR

echo "Generating files..."
cmake -G Ninja \
    -DMOVESENSE_CORE_LIBRARY=$MOVESENSE_CORELIB_PATH/ \
    -DCMAKE_TOOLCHAIN_FILE=$MOVESENSE_CORELIB_PATH/toolchain/gcc-nrf52.cmake \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DHWCONFIG=SS2_NAND \
    ../src || abort "cmake failed"

echo "Building packages..."
ninja pkgs || abort "failed to build packages"
