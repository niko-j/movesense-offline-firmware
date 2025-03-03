#!/bin/bash
LOGLEVEL=$1
PORT=$2

echo "Setting debug config..."
wbcmd --port $PORT --op PUT --path /System/Debug/Config \
    --opdatatype DebugMessageConfig \
    --opdata '{ "SystemMessages": true, "DebugMessages": true }' || exit -1

echo "Subscribing to log..."
wbcmd --port $PORT --op SUBSCRIBE --path /System/Debug/$LOGLEVEL || exit -2
