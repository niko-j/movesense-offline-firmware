# TODO

## Firmware Features / Research

- Implement: Logging
  - Implement: Buffering samples, converting to more optimal format
  - Implement: Encode data into chunks
  - Test: Send chunks to DataLogger, retrieve data with tool
  
- Research: Compression
  - Note: Samples should already be saved using optimal data types (32-bit to 16-bit in most cases)
  - Experiment: Viability of LZ compression algorithms
  - Experiment: Double rate sampling to reduce noise to improve compression ratio?
    - Note: This will consume more power!
  
## Configurator Tool

- All done for now!
  
## Nice-to-Haves (not strictly inside of the scope of the thesis)

- Low power mode
  - Disable all subscription, disable BLE adv
  - Will consume 2x FullPowerOff, still low
  - Could replace full power off
  - This will preserve RTC
- Tool to parse SBEM format and visualize data
