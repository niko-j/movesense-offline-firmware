# Movesense Offline Firmware

This is an offline tracking firmware for Movesense Flash (SS2_NAND variant) sensors.

This project implements several firmware modules that are useful for offline use cases where the sensor is not connected and does not stream samples to another device. Instead, the samples are stored on the device itself until it connected to and the logs are downloaded.

## Related Projects

- [Movesense Offline Configurator](https://github.com/niko-j/movesense-offline-configurator) project contains tools for configuring the offline mode and downloading logs from the device.
- [Movesense Offline SBEM Decoder](https://github.com/niko-j/movesense-offline-sbem-decoder) project contains a utility to read the log files and export measurements into CSV files.

## Firmware Modules

- `OfflineManager` is the central module that controls and configures rest of the system.
- `OfflineMeasurements` is a middle-man API that takes samples from the core measurement API and, using different compression techniques, optimizes the samples to take as little storage space as possible, without meaningfully impacting the usefulness of the measurements. It also implements actigraphy measurement that is derived from acceleration samples.
- `OfflineGATTService` implements a custom BLE GATT service for interacting with the offline services, which can be used instead of the MDS library.
- `GestureService` offers custom gesture detection for tapping and shaking.

## APIs

Summary of Whiteboard APIs of each firmware module. You can find detailed API definitions [here](./src/wbresources/).

### OfflineManager

- `/Offline/State` Read, write, and subscribe to offline mode's state.
- `/Offline/Config` Read, write, and subscribe to offline mode configuration.

### OfflineMeasurements

- `/Offline/Meas/ECG/{SampleRate}` Subscribe to receive 16-bit ECG data.
- `/Offline/Meas/ECG/Compressed/{SampleRate}` Subscribe to receive compressed ECG data.
- `/Offline/Meas/Acc/{SampleRate}` Subscribe to receive acceleration data in Q12.12 fixed-point format.
- `/Offline/Meas/Gyro/{SampleRate}` Subscribe to receive anglular velocity data in Q12.12 fixed-point format.
- `/Offline/Meas/Magn/{SampleRate}` Subscribe to receive magnetic flux density data in Q10.6 fixed-point format.
- `/Offline/Meas/HR` Subscribe to receive average heart rate in 8-bit unsigned integers.
- `/Offline/Meas/RR` Subscribe to receive R-to-R interval data in 12-bit (bit packed) format.
- `/Offline/Meas/Temp` Subscribe to receive temperature (°C) in signed 8-bit integers.
- `/Offline/Meas/Activity` Subscribe to receive average acceleration in every minute.

### GestureService

- `/Gesture/Tap` Subscribe to detect and count tapping. Tapping at least twice on Z axis will be recognized.
- `/Gesture/Shake` Subscribe to detect shaking. Counts the duration.

## Getting Started

This chapter describes the build and flashing process.

### Prerequisites

- [Docker](https://www.docker.com/) for running the build environment.
- [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools) for flashing and debugging tools.

### Repository Structure

- `movesense-device-lib` submodule contains the Movesense device library containing resources for building the firmware.
- `scripts` contains convenient scripts for building, dumping, and flashing the firmware. It also contains scripts for pulling and running the Docker image for building.
- `src` contains the firmware project files and source code.

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

Either use the VSCode task `Build (debug)` or run the build script `/movesense/scripts/build.sh debug` in the container.

### Flash the Firmware

You can flash the firmware using your mobile phone and the DFU package found in the `build` directory. 

If you have a jig, you can either run the VSCode task `Flash firmware` or run the script `./scripts/flash.sh`. The flashing scripts do not work on Windows, use nRF Command Line tools to program the device.

