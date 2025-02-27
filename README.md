# Movesense Offline Firmware

This is an offline tracking firmware for Movesense Flash (SS2_NAND variant) sensors.

This project implements several firmware modules that are useful for offline use cases where the sensor is not connected to another device and streaming samples. Instead, the samples are stored on the device itself until it is connected to, and the logs are downloaded.

## Related Projects

- [Movesense Offline Configurator](https://github.com/niko-j/movesense-offline-configurator) project contains tools for configuring the offline mode and downloading logs from the device.
- [Movesense Offline SBEM Decoder](https://github.com/niko-j/movesense-offline-sbem-decoder) project contains a utility to read the log files and export measurements into CSV files.

## Firmware Modules

The application uses the following custom modules:

- [OfflineMeasurements](./modules/OfflineMeasurements/README.md) is a middle-man API that takes samples from the core measurement API and, using different compression techniques, optimizes the samples to take as little storage space as possible, without meaningfully impacting the usefulness of the measurements. It also implements actigraphy measurement that is derived from acceleration samples.
- [OfflineGattService](./modules/OfflineGattService/README.md) implements a custom BLE GATT service for interacting with the offline app, which can be used instead of the MDS library.
- [GestureService](./modules/GestureService/README.md) offers custom gesture detection for tapping and shaking.

The `OfflineApp` application module that brings everything together can be found in the [source directory](./src/).

## Getting Started

This chapter describes the build and flashing process.

### Prerequisites

- [Docker](https://www.docker.com/) for running the build environment.
- [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools) for flashing and debugging tools.

### Repository Structure

- [movesense-device-lib](./movesense-device-lib/)
  - is the Movesense device library submodule containing resources for building the firmware.
- [modules](./modules/)
  - contains the source code of the custom firmware modules.
- [scripts](./scripts/) 
  - contains convenient scripts for building and flashing the firmware.
- [src](./src/)
  - contains the main firmware application files and source code.

### Pull the docker image

```sh
docker pull movesense/sensor-build-env:2.2
```

### Docker Build Environment

There's a VSCode task for running the build environment. You can also run the Docker container with the following command:

```sh
docker run -it --rm -v `pwd`:/movesense:delegated --name movesense-build-env movesense/sensor-build-env:2.2
```

### Building the Firmware

Either use the VSCode task `Build (debug)` or run the build script `./scripts/build.sh debug` in the container.

### Flash the Firmware

You can flash the firmware using your mobile phone and the DFU package found in the `build` directory. 

If you have a jig, you can either run the VSCode task `Flash firmware` or run the script `./scripts/flash.sh`. The flashing scripts do not work on Windows, use nRF Command Line tools to program the device.

