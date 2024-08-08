#!/bin/bash

SCRIPT_DIR=$(dirname "$0")
BUILD_DIR="$SCRIPT_DIR/../build"
FW_HEX_FILE="Movesense_combined.hex"
PROGRAM="$BUILD_DIR/$FW_HEX_FILE"

if [ ! -f $PROGRAM ]; then
    echo "Could not find $FW_HEX_FILE"
    exit 1
fi

nrfjprog -f nrf52 --sectoranduicrerase --program $BUILD_DIR/$FW_HEX_FILE --reset --verify
