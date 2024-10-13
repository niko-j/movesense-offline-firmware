# TODO

## Firmware features

- OfflineLogger: Recording
  - By resource type (measurement):
    - ~~Receive subscription notifications~~
    - Buffer samples (figure out suitable precision / format)
    - How to encode data in bytes
    - Send processed buffered samples to DataLogger

## PC Tool

- Listing logs
- Download raw log data
- (Nice to have) Visualize / unpack data

## Firmware research / nice to haves

-  Research and experiment with compression techniques 
   - Samples should already be saved using optimal data types (32-bit to 16-bit in most cases)
   - Block compression (LZ)
   - Double rate sampling to reduce noise (power consumption!)

- Low power mode (nice to have)
  - Disable all subscription, disable BLE adv
  - Will consume 2x FullPowerOff, still low
