#include "OfflineGATTService.hpp"

#include "app-resources/resources.h"
#include "comm_ble/resources.h"
#include "comm_ble_gattsvc/resources.h"
#include "sbem_types.h"
#include "mem_logbook/resources.h"

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
    , serviceHandle(0)
    , pendingRequest(0)
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
    asyncUnsubscribe(commandChar.resourceId);
    asyncUnsubscribe(configChar.resourceId);
    asyncUnsubscribe(dataChar.resourceId);

    releaseResource(commandChar.resourceId);
    releaseResource(configChar.resourceId);
    releaseResource(dataChar.resourceId);

    commandChar = {}; configChar = {}; dataChar = {};
    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineGATTService::onGetResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
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
                commandChar.handle = c.handle.hasValue() ? c.handle.getValue() : 0;
            else if (uuid16 == configUUID)
                configChar.handle = c.handle.hasValue() ? c.handle.getValue() : 0;
            else if (uuid16 == dataUUID)
                dataChar.handle = c.handle.hasValue() ? c.handle.getValue() : 0;
        }

        if (!commandChar.handle || !configChar.handle || !dataChar.handle)
        {
            DebugLogger::error("Failed to get characteristics handles");
            ASSERT(false);
            return;
        }

        DebugLogger::info("Received characteristics handles!");

        asyncSubsribeHandleResource(commandChar.handle, commandChar.resourceId);
        asyncSubsribeHandleResource(configChar.handle, configChar.resourceId);
        asyncSubsribeHandleResource(dataChar.handle, dataChar.resourceId);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK) {
            sendSbem(configChar.resourceId, resourceId.localResourceId, result);
        }
        else {
            sendStatusResponse(configChar.resourceId, pendingRequest, resultCode);
            return;
        }

        break;
    }
    case WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES::LID:
    {
        if (resultCode >= 400) 
        {
            sendStatusResponse(dataChar.resourceId, pendingRequest, resultCode);
            return;
        }

        auto entries = result.convertTo<const WB_RES::LogEntries&>();
        if(entries.elements.size() == 0)
        {
            sendStatusResponse(dataChar.resourceId, pendingRequest, wb::HTTP_CODE_NOT_FOUND);
            return;
        }

        // Are we are looking for a particular log?
        if(logDataTransmission.logIndex == (int32_t) entries.elements.begin()->id)
        {
            asyncReadLogData(*entries.elements.begin());
            return;
        }

        WB_RES::OfflineLogList logs = {};
        logs.complete = true;
        
        if (resultCode == wb::HTTP_CODE_CONTINUE)
        {
            uint32_t lastId = (entries.elements.end() - 1)->id;
            asyncGet(resourceId, AsyncRequestOptions::ForceAsync, lastId);
            logs.complete = false;
        }
        
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled GET result (%d) for resource: %d", resultCode, resourceId.localResourceId);
        break;
    }
    }

    ASSERT(resultCode < 400)
}

void OfflineGATTService::onPutResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
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

    ASSERT(resultCode < 400)
}

void OfflineGATTService::onPostResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::verbose("%s: onPostResult %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_GATTSVC::LID:
    {
        if (resultCode == wb::HTTP_CODE_CREATED)
        {
            serviceHandle = (int32_t)result.convertTo<uint16_t>();
            DebugLogger::info("Offline GATT service created. Handle: %d", serviceHandle);

            // Get handles for characteristics
            asyncGet(
                WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE(),
                AsyncRequestOptions::ForceAsync,
                serviceHandle);
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

    ASSERT(resultCode < 400)
}

void OfflineGATTService::onDeleteResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::verbose("%s: onPostResult %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_DATA::LID:
    {
        sendStatusResponse(dataChar.resourceId, pendingRequest, resultCode);
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled DELETE result (%d) for resource: %d",
            resultCode, resourceId.localResourceId);
        break;
    }
    }

    ASSERT(resultCode < 400)
}

void OfflineGATTService::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::verbose("%s: onSubscribeResult %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            sendStatusResponse(dataChar.resourceId, pendingRequest, resultCode);
            return;
        }
        break;
    }
    default:
    {
        break;
    }
    }

    ASSERT(resultCode < 400)
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
        const auto& ch = value.convertTo<const WB_RES::Characteristic&>();

        if (handle == commandChar.handle)
        {
            OfflineCommandRequest cmd(ch);
            handleCommand(cmd);
            break;
        }
        else if (handle == configChar.handle)
        {
            if (ch.bytes.size() == 15)
            {
                WB_RES::OfflineConfig config = {
                    .wakeUpBehavior = static_cast<WB_RES::WakeUpBehavior::Type>(ch.bytes[0]),
                    .sampleRates = wb::Array<uint16_t>((const uint16_t*)&ch.bytes[1], 6),
                    .sleepDelay = *reinterpret_cast<const uint16_t*>(&ch.bytes[13]),
                };

                asyncPut(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync, config);
            }
            else
            {
                DebugLogger::error("Unexpected config size: %u", ch.bytes.size());
            }
        }
        break;
    }
    case WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::LID:
    {
        const auto& params = WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::EVENT::ParameterListRef(parameters);

        auto index = params.getLogId();
        auto data = value.convertTo<const WB_RES::LogDataNotification&>();

        if(data.bytes.size() > 0) {
            uint8_t req = pendingRequest;
            sendSbem(dataChar.resourceId, WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::LID, value);
            pendingRequest = req; // Not complete yet
        }
        else // completed
            asyncUnsubscribe(resourceId, AsyncRequestOptions::Empty, index);
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

void OfflineGATTService::handleCommand(const OfflineCommandRequest& req)
{
    auto cmd = req.getCommand();
    uint8_t ref = req.getRequestRef();

    if(ref == 0 || cmd == OfflineCommand::UNKNOWN)
    {
        DebugLogger::info("Invalid command received, ignoring");
        return;
    }

    if (pendingRequest > 0)
    {
        if (pendingRequest == ref)
        {
            DebugLogger::info("Duplicate request, ignoring");
            return;
        }

        DebugLogger::info("Service is busy");
        sendStatusResponse(commandChar.resourceId, ref, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    pendingRequest = ref;
    DebugLogger::info("Received command: %d, ref: %d", cmd, ref);

    switch (cmd)
    {
    case OfflineCommand::READ_CONFIG:
    {
        asyncGet(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case OfflineCommand::GET_SESSIONS:
    {
        asyncGet(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case OfflineCommand::GET_SESSION_LOG:
    {
        uint16_t id = 0;
        if(req.tryGetValue<uint16_t>(0, id))
        {
            asyncSendLog(id);
        }
        break;
    }
    case OfflineCommand::CLEAR_SESSION_LOGS:
    {
        asyncDelete(WB_RES::LOCAL::OFFLINE_DATA(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case OfflineCommand::REPORT_STATUS: // TODO: Implement status reporting???
    default:
    {
        DebugLogger::warning("Unhandled command: %u", cmd);
        break;
    }
    }
}

bool OfflineGATTService::asyncSubsribeHandleResource(int16_t charHandle, whiteboard::ResourceId& resourceOut)
{
    resourceOut = whiteboard::ID_INVALID_RESOURCE;

    char path[32] = {};
    snprintf(path, sizeof(path), "/Comm/Ble/GattSvc/%d/%d", serviceHandle, charHandle);

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

void OfflineGATTService::sendData(wb::ResourceId characteristic, const uint8_t* data, uint32_t size)
{
    ASSERT(pendingRequest > 0);

    uint32_t sent = 0;
    do
    {
        pendingDataPacket.init(pendingRequest);
        sent += pendingDataPacket.writeRawData(data, size, sent);
        sendPendingDataPacket(characteristic);
    } while (sent < size);

    pendingRequest = 0;
}

void OfflineGATTService::sendSbem(wb::ResourceId characteristic, wb::LocalResourceId resourceId, const wb::Value& data)
{
    ASSERT(pendingRequest > 0);

    uint32_t sent = 0;
    uint32_t size = getSbemLength(resourceId, data);
    do
    {
        pendingDataPacket.init(pendingRequest);
        sent += pendingDataPacket.writeSbem(resourceId, data, sent);
        sendPendingDataPacket(characteristic);
    } while (sent < size);

    pendingRequest = 0;
}

void OfflineGATTService::sendStatusResponse(wb::ResourceId characteristic, uint8_t requestRef, uint16_t status)
{
    OfflineStatusPacket packet;
    packet.init(requestRef);
    packet.writeStatus(status);

    WB_RES::Characteristic value;
    value.bytes = wb::MakeArray<uint8_t>(packet.getData(), packet.getSize());
    asyncPut(characteristic, AsyncRequestOptions::Empty, value);

    if (requestRef == pendingRequest)
        pendingRequest = 0;
}

void OfflineGATTService::sendPendingDataPacket(wb::ResourceId characteristic)
{
    WB_RES::Characteristic value;
    value.bytes = wb::MakeArray<uint8_t>(
        pendingDataPacket.getData(),
        pendingDataPacket.getSize());
    asyncPut(characteristic, AsyncRequestOptions::Empty, value);
}

bool OfflineGATTService::asyncSendLog(uint32_t id)
{
    if (logDataTransmission.logIndex > -1) // Transmission in progress
        return false;

    logDataTransmission.logIndex = id;
    logDataTransmission.logSize = 0;
    logDataTransmission.sentBytes = 0;

    asyncGet(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync, id);
    return true;
}

void OfflineGATTService::asyncReadLogData(const WB_RES::LogEntry& entry)
{
    if (logDataTransmission.logIndex != (int32_t) entry.id || !entry.size.hasValue())
        return;

    logDataTransmission.logSize = entry.size.getValue();

    asyncSubscribe(
        WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA(),
        AsyncRequestOptions::ForceAsync,
        entry.id);
}
