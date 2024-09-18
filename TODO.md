# TODO

## OfflineManager
[x] Configure debugging (logs/uart)
[x] Read/write configuration to/from EEPROM
[x] Subscribe to BLE connections and change state
[x] Implement GET/PUT /Offline/Config
[x] Implement state events
[ ] Manage power states, sleep when not active (later)
[ ] Configuration validation (later)

## OfflineGATTService
[x] Register service and characteristics
[x] Subscribe to characteristics resources
[x] Handle communication (Commands)
[x] Handle data log read transmission 
[x] Handle config read/write

## OfflineLogger
[x] Subscribe and react to state events
[x] Retrieve config
[x] Subscribe to measurements based on config
[ ] Implement /Offline/Data API
[ ] Implement events (storage full, config error)
[ ] Receive and process samples
[ ] Sending samples to DataLogger
[ ] Implement compression (later)
