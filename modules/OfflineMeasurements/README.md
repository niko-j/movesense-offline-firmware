# OfflineMeasurements

This service adds measurement APIs that are designed for on-device storage logging. 

Some important features are:
- Separate APIs for average heartrate and R-to-R intervals with timestamping.
- Quantization of IMU values (Acc&Gyro: Q12.12, Magn: Q10.6).
- ECG compression using relative encoding and variable-length code.
- Actigraphy measurement with adjustable reporting interval.
- Temperature readings in °C.

## APIs

The service provides the following APIs:

- `/Offline/Meas/ECG/{SampleRate}` Subscribe to receive 16-bit ECG data.
- `/Offline/Meas/ECG/Compressed/{SampleRate}` Subscribe to receive compressed ECG data.
- `/Offline/Meas/Acc/{SampleRate}` Subscribe to receive acceleration data in Q12.12 fixed-point format.
- `/Offline/Meas/Gyro/{SampleRate}` Subscribe to receive anglular velocity data in Q12.12 fixed-point format.
- `/Offline/Meas/Magn/{SampleRate}` Subscribe to receive magnetic flux density data in Q10.6 fixed-point format.
- `/Offline/Meas/HR` Subscribe to receive average heart rate in 8-bit unsigned integers.
- `/Offline/Meas/RR` Subscribe to receive R-to-R interval data in 12-bit (bit packed) format.
- `/Offline/Meas/Temp` Subscribe to receive temperature (°C) in signed 8-bit integers.
- `/Offline/Meas/Activity/{Interval}` Subscribe to receive (relative) activity measurements in set intervals (as seconds).

Please refer to the [API definition](./wbresources/OfflineMeas.yaml) for more information.

## Adding to Firmware

To add the module into you firmware project, you need to add it to your CMakeLists.txt:

```cmake
if(NOT DEFINED MOVESENSE_MODULES)
  ...
  list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/path/to/OfflineMeasurements)
  ...
endif()
```

You also have to add the module into your `App.cpp`:

```cpp
#include "path/to/OfflineMeasurements/OfflineMeasurements.hpp"

MOVESENSE_PROVIDERS_BEGIN(...)
...
MOVESENSE_PROVIDER_DEF(OfflineMeasurements)
...
MOVESENSE_PROVIDERS_END(...)
```

Remember to add the APIs to `app_root.yaml`:

```yaml
apis:
  OfflineMeas.*:
    apiId: ...
    defaultExecutionContext: application
```
