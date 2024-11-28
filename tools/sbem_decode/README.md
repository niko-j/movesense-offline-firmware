# SBEM Decode

This utility parses and converts Offline mode data from SBEM files and is able to export the measurement samples in CSV format.

## Compiling

Run `make` to compile the program with debug symbols, or run `make release` to compile an optimized release build.

The compiled program will be placed under the `build` directory.

## Usage

```sh
./build/sbem_decode {command} {measurement?} {filename}
```

Commands: `info`, `csv`

Measurements with `csv` command: 
- `acc` - Acceleration samples
- `gyro` - Gyroscope samples
- `magn` - Magnetometer samples
- `ecg` - ECG samples
- `hr` - Heartrate and R-R interval samples
- `temp` - Temperature samples

Parse and show information about the file:

```sh
./build/sbem_decode info my_samples.sbem
```

Print decoded samples in CSV format:

```sh
./build/sbem_decode csv acc my_samples.sbem
```

Export samples in CSV format:

```sh
./build/sbem_decode csv ecg my_samples.sbem > my_ecg_samples.csv
```
