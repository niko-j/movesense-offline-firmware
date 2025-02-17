#include "OfflineManager.hpp"
#include "OfflineMeasurements.hpp"
#include "OfflineGATTService.hpp"
#include "GestureService.hpp"
#include "movesense.h"

MOVESENSE_APPLICATION_STACKSIZE(1024)

MOVESENSE_PROVIDERS_BEGIN(4)
MOVESENSE_PROVIDER_DEF(OfflineManager)
MOVESENSE_PROVIDER_DEF(OfflineMeasurements)
MOVESENSE_PROVIDER_DEF(OfflineGATTService)
MOVESENSE_PROVIDER_DEF(GestureService)
MOVESENSE_PROVIDERS_END(4)

MOVESENSE_FEATURES_BEGIN()

// Enabled optional modules
OPTIONAL_CORE_MODULE(DataLogger, true)
OPTIONAL_CORE_MODULE(Logbook, true)
OPTIONAL_CORE_MODULE(LedService, true)
OPTIONAL_CORE_MODULE(BleService, true)
OPTIONAL_CORE_MODULE(EepromService, true)
OPTIONAL_CORE_MODULE(CustomGattService, true)

// Disabled optional modules
OPTIONAL_CORE_MODULE(IndicationService, false)
OPTIONAL_CORE_MODULE(BypassService, false)
OPTIONAL_CORE_MODULE(BleNordicUART, false)

#ifdef DEBUG
OPTIONAL_CORE_MODULE(SystemMemoryService, true)
OPTIONAL_CORE_MODULE(DebugService, true)
OPTIONAL_CORE_MODULE(BleStandardHRS, false)
#else
OPTIONAL_CORE_MODULE(SystemMemoryService, false)
OPTIONAL_CORE_MODULE(DebugService, false)
OPTIONAL_CORE_MODULE(BleStandardHRS, true) // Standard HR service enabled in release
#endif

// NOTE: It is inadvisable to enable both Logbook/DataLogger and EepromService without
// explicit definition of Logbook memory area (see LOGBOOK_EEPROM_MEMORY_AREA macro in movesense.h).
// Default setting is for Logbook to use the whole EEPROM memory area.

// Define 16kB DEBUG message area
// NOTE: If building a simulator build, these macros are obligatory!
DEBUGSERVICE_BUFFER_SIZE(6, 120); // 6 lines, 120 characters total
DEBUG_EEPROM_MEMORY_AREA(true, 1024, 15360)
// Rest of the EEPROM is for Logbook 
LOGBOOK_EEPROM_MEMORY_AREA(16384, MEMORY_SIZE_FILL_REST);

APPINFO_NAME("Offline Tracker");
APPINFO_VERSION("0.0.1");
APPINFO_COMPANY("TUNI");

// NOTE: SERIAL_COMMUNICATION & BLE_COMMUNICATION macros have been DEPRECATED
MOVESENSE_FEATURES_END()