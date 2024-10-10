# TODO

## Firmware APIs

[ ] OfflineManager: Status WB API /Offline/Status
- Data
    - Free space (bytes)
    - Recorded sessions (count)
    - Last error code (byte)
- BLE Command: REPORT_STATUS

[ ] OfflineLogger: Session WB API /Offline/Sessions
- Session info (GET /Offline/Sessions)
  - Return info from all recorded sessions
  - Data per session:
    - Begin timestamp
    - End timestamp
    - Measurement configuration
  - BLE Command: GET_SESSION_INFO (index in payload)
- Session samples (GET /Offline/Sessions/{index})
  - Data:
    - Compressed?
    - Data structure to be defined (!!!)
  - BLE Command: GET_SESSION_LOG
- Session erase (DELETE /Offline/Sessions)
  - Clear all of the sessions
  - BLE Command: CLEAR_SESSION_LOGS

## Firmware features

[x] OfflineManager: Wake up logic
- Wake up / enter sleep based on config (wakeup and sleep delay)
- Offline state management to control recording

[ ] OfflineLogger: Subscribing / unsubscribing measurements
- Based on config

[ ] OfflineLogger: Recording
- By resource type (measurement):
  - Receive subscription notifications
  - Buffer samples (figure out suitable precision / format)
  - Send processed buffered samples to DataLogger

[ ] OfflineGATT: Upload
- Check if we could just call LogBook APIs from OfflineGATT
- Otherwise, use OfflineLogger Session WB API?

[x] OfflineManager: Storage full / error states
- LED indication (e.g., fast triple blink) to indicate storage is full.
- Automatic sleep after 30 seconds. 

## Firmware research / nice to haves

[ ] Research and experiment with compression techniques (Research)
- Samples should already be saved using optimal data types (32-bit to 16-bit in most cases)
- Block compression (LZ)
- Double rate sampling to reduce noise

[ ] Gesture to toggle BLE advertising (Nice to have)
- Power saving measure
- Probably best to leave outside the scope of thesis

## PC app

[ ] Status reports
- Implemnt BLE command: REPORT_STATUS
- Show a message box

[ ] Session info
- Implement BLE command: GET_SESSION_INFO
- Show another window containing session information

[ ] Session download
- Implement BLE command: GET_SESSION_LOG
- File dialog for raw data output

[ ] Device reset
- Implement BLE command: CLEAR_SESSION_LOGS
- Simple button and confirmation dialog
  