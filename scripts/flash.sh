#!/bin/bash

echo "NOTE: Run this on the host machine or set up USB pass-through into the container for the jig."

# Flash *_combined.hex files
nrfjprog -f nrf52 --sectoranduicrerase --program $1 --reset
