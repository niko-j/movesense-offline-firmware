# GattService

This is a custom GATT service module intended to be used with the Offline App for Movesense sensor.

## Identifiers

The service has the identifier `cf270001-1d1c-4df3-8196-4f464c535643`, and has two characterstics:

- Sensor RX: `cf270002-1d1c-4df3-8196-4f464c535643`
- Sensor TX: `cf270003-1d1c-4df3-8196-4f464c535643`

These characteristics are used with a custom [protocol](./protocol/).

## Adding to Firmware

To add the module into you firmware project, you need to add it to your CMakeLists.txt:

```cmake
if(NOT DEFINED MOVESENSE_MODULES)
    ...

    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/path/to/GattService)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/path/to/GattService/protocol)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/path/to/GattService/protocol/types)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/path/to/GattService/protocol/packets)
    list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/path/to/GattService/protocol/utils)

    ...
endif()
```

You also have to add the module into your `App.cpp` and enable `CustomGattService` core module:

```cpp
#include "path/to/GattService/GattService.hpp"

MOVESENSE_PROVIDERS_BEGIN(...)
...
MOVESENSE_PROVIDER_DEF(GestureService)
...
MOVESENSE_PROVIDERS_END(...)


MOVESENSE_FEATURES_BEGIN()
...
OPTIONAL_CORE_MODULE(CustomGattService, true)
...
MOVESENSE_FEATURES_END()
```

