# GestureService

This service implements detection algorithms for various gestures based on acceleration samples. 

## APIs

The service provides the following APIs:

- `/Gesture/Tap` for tap events.
- `/Gesture/Shake` for shake events.
- `/Gesture/Orientation` for device orientation events.

Please refer to the [API definition](./wbresources/Gesture.yaml) for more information.

## Adding to Firmware

To add the module into you firmware project, you need to add it to your CMakeLists.txt:

```cmake
if(NOT DEFINED MOVESENSE_MODULES)
  ...
  list(APPEND MOVESENSE_MODULES ${CMAKE_CURRENT_LIST_DIR}/path/to/GestureService)
  ...
endif()
```

You also have to add the module into your `App.cpp`:

```cpp
#include "path/to/GestureService/GestureService.hpp"

MOVESENSE_PROVIDERS_BEGIN(...)
...
MOVESENSE_PROVIDER_DEF(GestureService)
...
MOVESENSE_PROVIDERS_END(...)
```

Remember to add the APIs to `app_root.yaml`:

```yaml
apis:
  Gesture.*:
    apiId: ...
    defaultExecutionContext: application
```
