#!/bin/bash
LOGLEVEL=$1
PORT=$2

if [[ -z "$WBCMD_PATH" ]]; then
    if ! which wbcmdd &> /dev/null;  then
        echo "wbcmd not found in PATH"
        echo "You can set the environment value WBCMD_PATH to the exact location of wbcmd."
        exit 1
    fi
else
    if [[ -f "$WBCMD_PATH" ]]; then
        echo "WBCMD_PATH set to $WBCMD_PATH"
    else
        echo "Could not find WBCMD from $WBCMD_PATH"
        exit 1
    fi
fi

echo "Setting debug config..."
wbcmd --port $PORT --op PUT --path /System/Debug/Config \
    --opdatatype DebugMessageConfig \
    --opdata '{ "SystemMessages": true, "DebugMessages": true }' || exit -1

echo "Subscribing to log..."
wbcmd --port $PORT --op SUBSCRIBE --path /System/Debug/$LOGLEVEL || exit -2
