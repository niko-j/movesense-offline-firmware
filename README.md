# movesense-offline-tracking

An offline tracking firmware for Movesense Flash (SS2_NAND variant) sensor devices.

The firmware implements the following modules:

- `OfflineTracker`: The main application controlling and monitoring the device.
- TBD

A separate tool application will be used for communicating with the services over BLE and for configuring the device.

## Prerequisites

- [Docker](https://www.docker.com/) for running the build environment.
- [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools) for flashing and debugging tools.

## Repository Structure

- `movesense-device-lib` submodule contains the Movesense device library containing resources for building the firmware.
- `scripts` contains convenient scripts for building, dumping, and flashing the firmware. It also contains scripts for pulling and running the Docker image for building.
- `src` contains the firmware project files and source code.

## Getting Started

### Pull the docker image

```sh
docker pull movesense/sensor-build-env:2.2
```

### Docker Build Environment

There's a VSCode task for running the build environment. You can also run the Docker container with command:

```sh
docker run -it --rm -v `pwd`:/movesense:delegated --name movesense-build-env movesense/sensor-build-env:2.2
```

### Building the Firmware

Either use the VSCode task `Build (debug)` or run the build script `/movesense/scripts/build.sh debug` in the container.

### Flash the Firmware

You can flash the firmware using your mobile phone and the DFU package found in the `build` directory. 

If you have a jig, you can either run the VSCode task `Flash firmware` or run the script `./scripts/flash.sh`. The flashing scripts do not work on Windows, use nRF Command Line tools to program the device.

## Modules

Notes about the modules (subject to change).

### OfflineTracker

- Monitors the device and its sensors, manages power states.

