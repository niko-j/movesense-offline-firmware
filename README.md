# movesense-offline-tracking

An offline tracking firmware for Movesense Flash (SS2_NAND variant) sensor devices.

The firmware implements the following modules:

- `OfflineConfigService`: A service for configuring the offline tracking data sources and behavior over BLE.
- `OfflineLogService`: A service for transferring the gathered data over BLE.
- `OfflineDataClient`: An application gathering and saving sensor data when the device is not connected.
- `OfflineTracker`: The main application controlling the device states.

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

### Run the build environment

```sh
docker run -it --rm -v `pwd`:/movesense:delegated movesense/sensor-build-env:2.2
```

or run `scripts/run_docker.sh`

### Build the project using the convenience script

Run inside the Docker container:

```sh
cd movesense/build
../scripts/build.sh debug ../src
```

### Flashing

Either use a mobile phone to upload the firmware to the device or use the `scripts/flash.sh` if you have a jig.

