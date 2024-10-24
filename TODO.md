# TODO

## Firmware Features / Research
  
- Research: Delta compression

- Implement: Check for system failures on boot and reset config if necessary

- Debug: 929:Whiteboard.cpp
  - Probably too many notifications/requests going on simultaneously on init.

- Debug: ECG measurements seem to run FIFO overflow quite easily.
  
```log
00> 00:00:21 !isRREnabled
00> 00:00:21 mNextECGTimestamp == 0 && sLastInterruptTS == 0. Wait for next interrupt.
00> 00:00:22 MAX30003 FIFO OVERFLOW: ts: 22504
```

## Configurator Tool

- All done for now!
  
## Nice-to-Haves (not strictly inside of the scope of the thesis)

- Low power mode
  - Disable all subscription, disable BLE adv
  - Will consume 2x FullPowerOff, still low
  - Could replace full power off
  - This will preserve RTC
- Tool to parse SBEM format and visualize data
