#include "OfflineGATTService.hpp"

#include "app-resources/resources.h"
#include "comm_ble/resources.h"
#include "comm_ble_gattsvc/resources.h"
#include "sbem_types.h"

#include "common/core/dbgassert.h"

#include "DebugLogger.hpp"

const char* const OfflineGATTService::LAUNCHABLE_NAME = "OfflineGATT";

enum Commands : uint8_t
{
    READ_CONFIG = 0x01,
    REPORT_STATUS = 0x02,
    GET_SESSIONS = 0x03,
    GET_SESSION_LOG = 0x04,
    CLEAR_SESSION_LOGS = 0x05
};

/* UUID string: 0000b000-0000-1000-8000-00805f9b34fb */
constexpr uint8_t OfflineGattServiceUuid[] = { 0x00, 0xB0 };
constexpr uint8_t commandCharUuid[] = { 0x01, 0xB0 };
constexpr uint8_t configCharUuid[] = { 0x02, 0xB0 };
constexpr uint8_t dataCharUuid[] = { 0x03, 0xB0 };

OfflineGATTService::OfflineGATTService()
    : ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_RES::LOCAL::OFFLINE_CONFIG::EXECUTION_CONTEXT)
    , LaunchableModule(LAUNCHABLE_NAME, WB_RES::LOCAL::OFFLINE_CONFIG::EXECUTION_CONTEXT)
    , offlineSvcHandle(0)
    , commandCharHandle(0)
    , configCharHandle(0)
    , dataCharHandle(0)
    , commandCharResource(wb::ID_INVALID_RESOURCE)
    , configCharResource(wb::ID_INVALID_RESOURCE)
    , dataCharResource(wb::ID_INVALID_RESOURCE)
    , dataClientReference(0)
{

}

OfflineGATTService::~OfflineGATTService()
{

}

bool OfflineGATTService::initModule()
{
    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void OfflineGATTService::deinitModule()
{
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool OfflineGATTService::startModule()
{
    DebugLogger::info("Starting GATT service");
    configGattSvc();
    mModuleState = WB_RES::ModuleStateValues::STARTED;
    DebugLogger::info("Started GATT service");
    return true;
}

void OfflineGATTService::stopModule()
{
    asyncUnsubscribe(commandCharResource);
    asyncUnsubscribe(configCharResource);
    asyncUnsubscribe(dataCharResource);

    releaseResource(commandCharResource);
    releaseResource(configCharResource);
    releaseResource(dataCharResource);

    commandCharResource = wb::ID_INVALID_RESOURCE;
    configCharResource = wb::ID_INVALID_RESOURCE;
    dataCharResource = wb::ID_INVALID_RESOURCE;

    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineGATTService::onGetResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onGetResult %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE::LID:
    {
        DebugLogger::info("Reading characteristics handles");

        const WB_RES::GattSvc& svc = result.convertTo<const WB_RES::GattSvc&>();

        uint16_t svcHandle = svc.handle.getValue();
        DebugLogger::info("Characteristics for service %d", svcHandle);

        uint16_t commandUUID = *reinterpret_cast<const uint16_t*>(commandCharUuid);
        uint16_t configUUID = *reinterpret_cast<const uint16_t*>(configCharUuid);
        uint16_t dataUUID = *reinterpret_cast<const uint16_t*>(dataCharUuid);

        for (size_t i = 0; i < svc.chars.size(); i++)
        {
            const WB_RES::GattChar& c = svc.chars[i];
            if (c.uuid.size() != 2)
                continue;

            uint16_t uuid16 = *reinterpret_cast<const uint16_t*>(&(c.uuid[0]));

            if (uuid16 == commandUUID)
                commandCharHandle = c.handle.hasValue() ? c.handle.getValue() : 0;
            else if (uuid16 == configUUID)
                configCharHandle = c.handle.hasValue() ? c.handle.getValue() : 0;
            else if (uuid16 == dataUUID)
                dataCharHandle = c.handle.hasValue() ? c.handle.getValue() : 0;
        }

        if (!commandCharHandle || !configCharHandle || !dataCharHandle)
        {
            DebugLogger::error("Failed to get characteristics handles");
            ASSERT(false);
            return;
        }

        DebugLogger::info("Received characteristics handles!");

        asyncSubsribeHandleResource(commandCharHandle, commandCharResource);
        asyncSubsribeHandleResource(configCharHandle, configCharResource);
        asyncSubsribeHandleResource(dataCharHandle, dataCharResource);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            DebugLogger::info("Sending config");
            sendSbemValue(configCharResource, resourceId.localResourceId, result);
        }
        break;
    }
    case WB_RES::LOCAL::OFFLINE_SESSIONS::LID:
    {
        // TODO: Send sessions list
        break;
    }
    case WB_RES::LOCAL::OFFLINE_SESSIONS_SESSIONINDEX::LID:
    {
        const auto& data = result.convertTo<const WB_RES::OfflineData&>();

        if (resultCode == wb::HTTP_CODE_CONTINUE)
        {
            asyncGet(WB_RES::LOCAL::OFFLINE_SESSIONS_SESSIONINDEX(), AsyncRequestOptions::ForceAsync, data.sessionIndex);
        }

        if (resultCode < 400)
        {
            // TODO: Send data
            //sendOfflineData(&data.bytes[0], data.bytes.size());
        }
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled GET result (%d) for resource: %d", resultCode, resourceId.localResourceId);
        break;
    }
    }
}

void OfflineGATTService::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onPutResult %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    default:
    {
        DebugLogger::warning("Unhandled PUT result (%d) for resource: %d",
            resultCode, resourceId.localResourceId);
        break;
    }
    }
}

void OfflineGATTService::onPostResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onPostResult %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_GATTSVC::LID:
    {
        if (resultCode == wb::HTTP_CODE_CREATED)
        {
            offlineSvcHandle = (int32_t)result.convertTo<uint16_t>();
            DebugLogger::info("Offline GATT service created. Handle: %d", offlineSvcHandle);

            // Get handles for characteristics
            asyncGet(
                WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE(),
                AsyncRequestOptions::ForceAsync,
                offlineSvcHandle);
        }
        else
        {
            DebugLogger::error("Failed to create Offline GATT service: %d", resultCode);
        }
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled POST result (%d) for resource: %d",
            resultCode, resourceId.localResourceId);
        break;
    }
    }
}

void OfflineGATTService::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::verbose("%s: onSubscribeResult %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineGATTService::onNotify(
    wb::ResourceId resourceId,
    const wb::Value& value,
    const wb::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onNotify %d", LAUNCHABLE_NAME, resourceId.localResourceId);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE::LID:
    {
        WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE::SUBSCRIBE::ParameterListRef parameterRef(parameters);
        const auto& handle = parameterRef.getCharHandle();
        const auto& data = value.convertTo<const WB_RES::Characteristic&>();

        if (handle == commandCharHandle)
        {
            uint8_t len = data.bytes.size();
            uint8_t cmd = data.bytes[0];

            if (dataClientReference > 0)
            {
                // TODO: Error response
                DebugLogger::info("Client is busy");
                return;
            }

            if (len > 1)
            {
                dataClientReference = data.bytes[1];
            }

            DebugLogger::info("Received command: %d ref: %d", cmd, dataClientReference);

            switch (cmd)
            {
            case Commands::READ_CONFIG:
            {
                asyncGet(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync);
                break;
            }
            case Commands::REPORT_STATUS:
            case Commands::GET_SESSIONS:
            case Commands::GET_SESSION_LOG:
            case Commands::CLEAR_SESSION_LOGS:
            default:
            {
                DebugLogger::warning("Unhandled command: %u", cmd);
                break;
            }
            }
        }
        else if (handle == configCharHandle)
        {
            if (data.bytes.size() == 15)
            {
                WB_RES::OfflineConfig config = {
                    .wakeUpBehavior = static_cast<WB_RES::WakeUpBehavior::Type>(data.bytes[0]),
                    .sampleRates = wb::Array<uint16_t>((const uint16_t*) &data.bytes[1], 6),
                    .sleepDelay = *reinterpret_cast<const uint16_t*>(&data.bytes[13]),
                };
                
                asyncPut(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync, config);
            }
            else
            {
                DebugLogger::error("Unexpected config size: %u", data.bytes.size());
            }
        }
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled notification from resource: %d", resourceId.localResourceId);
        break;
    }
    }
}

void OfflineGATTService::configGattSvc()
{
    constexpr uint8_t CHARACTERISTICS_COUNT = 3;
    WB_RES::GattSvc offlineSvc;
    WB_RES::GattChar offlineChars[CHARACTERISTICS_COUNT];

    WB_RES::GattChar& offlineCommandChar = offlineChars[0];
    WB_RES::GattProperty commmandCharProps[] = {
        WB_RES::GattProperty::WRITE
    };

    WB_RES::GattChar& offlineConfigChar = offlineChars[1];
    WB_RES::GattProperty configCharProps[] = {
        WB_RES::GattProperty::NOTIFY,
        WB_RES::GattProperty::WRITE
    };

    WB_RES::GattChar& offlineDataChar = offlineChars[2];
    WB_RES::GattProperty dataCharProps[] = {
        WB_RES::GattProperty::NOTIFY
    };

    offlineCommandChar.props = wb::MakeArray<WB_RES::GattProperty>(commmandCharProps, 1);
    offlineCommandChar.uuid = wb::MakeArray<uint8_t>(commandCharUuid, sizeof(commandCharUuid));

    offlineConfigChar.props = wb::MakeArray<WB_RES::GattProperty>(configCharProps, 2);
    offlineConfigChar.uuid = wb::MakeArray<uint8_t>(configCharUuid, sizeof(configCharUuid));

    offlineDataChar.props = wb::MakeArray<WB_RES::GattProperty>(dataCharProps, 1);
    offlineDataChar.uuid = wb::MakeArray<uint8_t>(dataCharUuid, sizeof(dataCharUuid));

    offlineSvc.uuid = wb::MakeArray<uint8_t>(OfflineGattServiceUuid, sizeof(OfflineGattServiceUuid));
    offlineSvc.chars = wb::MakeArray<WB_RES::GattChar>(offlineChars, CHARACTERISTICS_COUNT);

    asyncPost(WB_RES::LOCAL::COMM_BLE_GATTSVC(), AsyncRequestOptions::ForceAsync, offlineSvc);
}

bool OfflineGATTService::asyncSubsribeHandleResource(int16_t charHandle, whiteboard::ResourceId& resourceOut)
{
    resourceOut = whiteboard::ID_INVALID_RESOURCE;

    char path[32] = {};
    snprintf(path, sizeof(path), "/Comm/Ble/GattSvc/%d/%d", offlineSvcHandle, charHandle);

    whiteboard::Result res = getResource(path, resourceOut);
    if (whiteboard::IsErrorResult(res))
    {
        DebugLogger::error("Failed to find resource for handle %d", charHandle);
        return false;
    }

    DebugLogger::verbose("%s: subscribing to resource %d", LAUNCHABLE_NAME, resourceOut);
    asyncSubscribe(resourceOut, AsyncRequestOptions::ForceAsync);
    return true;
}

void OfflineGATTService::resetBuffer()
{
    memset(dataBuffer, 0, sizeof(dataBuffer));
}

size_t OfflineGATTService::writeHeaderToBuffer(uint32_t offset, uint32_t total_bytes)
{
    memcpy(&dataBuffer[0], &offset, sizeof(uint32_t));
    memcpy(&dataBuffer[4], &total_bytes, sizeof(uint32_t));
    memcpy(&dataBuffer[8], &dataClientReference, sizeof(uint8_t));
    return HEADER_SIZE;
}

size_t OfflineGATTService::writeRawDataToBuffer(const uint8_t* data, uint8_t length)
{
    size_t written = WB_MIN(length, PAYLOAD_SIZE);
    memcpy(&dataBuffer[HEADER_SIZE], data, written);
    return written;
}

size_t OfflineGATTService::writeSbemToBuffer(wb::LocalResourceId resourceId, const wb::Value& data, uint32_t offset)
{
    return writeToSbemBuffer(&dataBuffer[HEADER_SIZE], PAYLOAD_SIZE, offset, resourceId, data);
}

void OfflineGATTService::sendBuffer(wb::ResourceId characteristic, uint32_t len)
{
    WB_RES::Characteristic value;
    value.bytes = wb::MakeArray<uint8_t>(dataBuffer, len);
    asyncPut(characteristic, AsyncRequestOptions::Empty, value);
}


void OfflineGATTService::sendData(wb::ResourceId characteristic, const uint8_t* data, uint32_t size)
{
    uint32_t sent = 0;
    do
    {
        resetBuffer();
        size_t header = writeHeaderToBuffer(sent, size);
        size_t payload = writeRawDataToBuffer(data + sent, size - sent);
        sent += payload;
        DebugLogger::info("Sending data (ref %u): %u bytes sent / %u bytes total", dataClientReference, sent, size);
        sendBuffer(characteristic, header + payload);
    } while (sent < size);

    dataClientReference = 0; // Request completed
}

void OfflineGATTService::sendSbemValue(wb::ResourceId characteristic, wb::LocalResourceId resourceId, const wb::Value& value)
{
    uint32_t sent = 0;
    uint32_t size = getSbemLength(resourceId, value);
    do
    {
        resetBuffer();
        size_t header = writeHeaderToBuffer(sent, size);
        size_t payload = writeSbemToBuffer(resourceId, value, sent);
        sent += payload;
        DebugLogger::info("Sending SBEM value (ref %u): %u bytes from %u bytes total", dataClientReference, sent, size);
        sendBuffer(characteristic, header + payload);
    } while (sent < size);

    dataClientReference = 0; // Request completed
}
