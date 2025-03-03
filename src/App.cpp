#include "OfflineApp.hpp"
#include "../modules/OfflineMeasurements/OfflineMeasurements.hpp"
#include "../modules/OfflineGattService/OfflineGattService.hpp"
#include "../modules/GestureService/GestureService.hpp"
#include "movesense.h"

MOVESENSE_APPLICATION_STACKSIZE(1024)

MOVESENSE_PROVIDERS_BEGIN(4)
MOVESENSE_PROVIDER_DEF(OfflineApp)
MOVESENSE_PROVIDER_DEF(OfflineMeasurements)
MOVESENSE_PROVIDER_DEF(OfflineGattService)
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
OPTIONAL_CORE_MODULE(SystemMemoryService, false)
OPTIONAL_CORE_MODULE(BypassService, false)
OPTIONAL_CORE_MODULE(BleNordicUART, false)

#ifdef NDEBUG
OPTIONAL_CORE_MODULE(DebugService, true)
OPTIONAL_CORE_MODULE(BleStandardHRS, true)
#else
OPTIONAL_CORE_MODULE(DebugService, false)
OPTIONAL_CORE_MODULE(BleStandardHRS, false)
#endif

OFFLINE_CONFIG_EEPROM_AREA(0, 128)
DEBUGSERVICE_BUFFER_SIZE(10, 320);
DEBUG_EEPROM_MEMORY_AREA(true, 1024, 15360)
LOGBOOK_EEPROM_MEMORY_AREA(16384, MEMORY_SIZE_FILL_REST);

APPINFO_NAME("Offline Tracker");
APPINFO_VERSION("1.0.0");
APPINFO_COMPANY("Niko Junnila");

MOVESENSE_FEATURES_END()
