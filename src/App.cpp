#include "OfflineManager.hpp"
#include "OfflineLogger.hpp"
#include "OfflineGATTService.hpp"
#include "movesense.h"

MOVESENSE_APPLICATION_STACKSIZE(1024)

MOVESENSE_PROVIDERS_BEGIN(3)
MOVESENSE_PROVIDER_DEF(OfflineManager)
MOVESENSE_PROVIDER_DEF(OfflineLogger)
MOVESENSE_PROVIDER_DEF(OfflineGATTService)
MOVESENSE_PROVIDERS_END(3)

MOVESENSE_FEATURES_BEGIN()
// Explicitly enable or disable Movesense framework core modules.
// List of modules and their default state is found in documentation
OPTIONAL_CORE_MODULE(DataLogger, true)
OPTIONAL_CORE_MODULE(Logbook, true)
OPTIONAL_CORE_MODULE(LedService, true)
OPTIONAL_CORE_MODULE(IndicationService, true)
OPTIONAL_CORE_MODULE(BleService, true)
OPTIONAL_CORE_MODULE(EepromService, true)
OPTIONAL_CORE_MODULE(BypassService, false)
OPTIONAL_CORE_MODULE(SystemMemoryService, true)
OPTIONAL_CORE_MODULE(DebugService, true)
OPTIONAL_CORE_MODULE(BleStandardHRS, false)
OPTIONAL_CORE_MODULE(BleNordicUART, false)
OPTIONAL_CORE_MODULE(CustomGattService, true)

// NOTE: It is inadvisable to enable both Logbook/DataLogger and EepromService without
// explicit definition of Logbook memory area (see LOGBOOK_EEPROM_MEMORY_AREA macro in movesense.h).
// Default setting is for Logbook to use the whole EEPROM memory area.

// Define 16kB DEBUG message area
// NOTE: If building a simulator build, these macros are obligatory!
DEBUGSERVICE_BUFFER_SIZE(6, 120); // 6 lines, 120 characters total
DEBUG_EEPROM_MEMORY_AREA(true, 0, 16384)
// Rest of the EEPROM is for Logbook 
LOGBOOK_EEPROM_MEMORY_AREA(16384, MEMORY_SIZE_FILL_REST);

APPINFO_NAME("Offline Tracker");
APPINFO_VERSION("0.0.1");
APPINFO_COMPANY("TUNI");

// NOTE: SERIAL_COMMUNICATION & BLE_COMMUNICATION macros have been DEPRECATED
MOVESENSE_FEATURES_END()