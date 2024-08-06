#!/bin/bash

if [ -z "${MOVESENSE_CORELIB_PATH}" ]
then
    MOVESENSE_CORELIB_PATH=../movesense-device-lib/MovesenseCoreLib
fi

if [[ "$1" == "release" ]]
then
    BUILD_TYPE=Release
elif [[ "$1" == "debug" ]]
then
    BUILD_TYPE=Debug
else
    CMD=$(basename -- $0)
    echo "Usage: $CMD [debug|release] <path_to_project>"
    exit 1
fi


cmake -G Ninja \
    -DMOVESENSE_CORE_LIBRARY=$MOVESENSE_CORELIB_PATH/ \
    -DCMAKE_TOOLCHAIN_FILE=$MOVESENSE_CORELIB_PATH/toolchain/gcc-nrf52.cmake \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DHWCONFIG=SS2_NAND \
    $2

ninja pkgs
