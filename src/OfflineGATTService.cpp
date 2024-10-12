#include "OfflineGATTService.hpp"

#include "app-resources/resources.h"
#include "comm_ble/resources.h"
#include "comm_ble_gattsvc/resources.h"
#include "sbem_types.h"
#include "mem_logbook/resources.h"

#include "common/core/dbgassert.h"

#include "DebugLogger.hpp"

const char* const OfflineGATTService::LAUNCHABLE_NAME = "OfflineGATT";

/* UUID string: 0000b001-0000-1000-8000-00805f9b34fb */
constexpr uint8_t OfflineGattServiceUuid[] = { 0x01, 0xB0 };
constexpr uint8_t txUuid[] = { 0x02, 0xB0 };
constexpr uint8_t rxUuid[] = { 0x03, 0xB0 };

OfflineGATTService::OfflineGATTService()
    : ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_RES::LOCAL::OFFLINE_CONFIG::EXECUTION_CONTEXT)
    , LaunchableModule(LAUNCHABLE_NAME, WB_RES::LOCAL::OFFLINE_CONFIG::EXECUTION_CONTEXT)
    , logDataTransmission({})
    , serviceHandle(0)
    , pendingRequestId(0)
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
    asyncUnsubscribe(txChar.resourceId);
    asyncUnsubscribe(rxChar.resourceId);

    releaseResource(txChar.resourceId);
    releaseResource(rxChar.resourceId);

    txChar = {}; rxChar = {};
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
        if(resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::info("Failed to subcribe GATT service handles");
            return;
        }

        DebugLogger::info("Reading characteristics handles");
        const WB_RES::GattSvc& svc = result.convertTo<const WB_RES::GattSvc&>();

        uint16_t svcHandle = svc.handle.getValue();
        DebugLogger::info("Characteristics for service %d", svcHandle);

        uint16_t senderUuid = *reinterpret_cast<const uint16_t*>(txUuid);
        uint16_t recvUuid = *reinterpret_cast<const uint16_t*>(rxUuid);

        for (size_t i = 0; i < svc.chars.size(); i++)
        {
            const WB_RES::GattChar& c = svc.chars[i];
            if (c.uuid.size() != 2)
                continue;

            uint16_t uuid16 = *reinterpret_cast<const uint16_t*>(&(c.uuid[0]));

            if (uuid16 == senderUuid)
                txChar.handle = c.handle.hasValue() ? c.handle.getValue() : 0;
            else if (uuid16 == recvUuid)
                rxChar.handle = c.handle.hasValue() ? c.handle.getValue() : 0;
        }

        if (!txChar.handle || !rxChar.handle)
        {
            DebugLogger::error("Failed to get characteristics handles");
            ASSERT(false);
            return;
        }

        DebugLogger::info("Received characteristics handles!");

        asyncSubsribeHandleResource(txChar.handle, txChar.resourceId);
        asyncSubsribeHandleResource(rxChar.handle, rxChar.resourceId);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            auto conf = result.convertTo<const WB_RES::OfflineConfig&>();

            OfflineConfigPacket packet(buffer);
            packet.setReference(pendingRequestId);
            packet.writeConfig(conf);
            sendPacket(packet);
            pendingRequestId = OFFLINE_PACKET_INVALID_REF;
        }
        else
        {
            sendStatusResponse(pendingRequestId, resultCode);
            return;
        }

        break;
    }
    case WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES::LID:
    {
        if (resultCode >= 400)
        {
            sendStatusResponse(pendingRequestId, resultCode);
            return;
        }

        auto entries = result.convertTo<const WB_RES::LogEntries&>();
        if (entries.elements.size() == 0)
        {
            sendStatusResponse(pendingRequestId, wb::HTTP_CODE_NOT_FOUND);
            return;
        }

        // Are we are looking for a particular log?
        if (logDataTransmission.index == (int32_t)entries.elements.begin()->id)
        {
            asyncReadLogData(*entries.elements.begin());
            return;
        }

        bool list_complete = true;
        if (resultCode == wb::HTTP_CODE_CONTINUE)
        {
            uint32_t lastId = (entries.elements.end() - 1)->id;
            asyncGet(resourceId, AsyncRequestOptions::ForceAsync, lastId);
            list_complete = false;
        }

        OfflineLogListPacket packet(buffer);
        packet.setReference(pendingRequestId);

        uint32_t logCount = entries.elements.size();
        for (size_t i = 0; i < logCount; i++)
        {
            const auto& el = entries.elements[i];
            packet.addItem(el);

            bool all_items_sent = (i + 1 == logCount);
            if (all_items_sent || packet.isFull())
            {
                packet.setComplete(list_complete && all_items_sent);
                sendPacket(packet);

                if(packet.getComplete())
                    pendingRequestId = OFFLINE_PACKET_INVALID_REF;
            }
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
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        sendStatusResponse(pendingRequestId, resultCode);
        break;
    }
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
        sendStatusResponse(pendingRequestId, resultCode);
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
            sendStatusResponse(pendingRequestId, resultCode);
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
        const auto& ch = value.convertTo<const WB_RES::Characteristic&>();

        if (ch.bytes.size() < 2)
            return;

        auto type = parseOfflinePacketType(ch.bytes[0]);
        uint8_t ref = ch.bytes[1];
        DebugLogger::info("Received packet ref: %u type: %u size: %u",
            ref, type, ch.bytes.size());

        if (pendingRequestId != OFFLINE_PACKET_INVALID_REF)
        {
            uint8_t statusBuf[4];
            if (pendingRequestId == ref)
            {
                DebugLogger::info("Duplicate request, ignoring");
                return;
            }

            DebugLogger::info("Service is busy");
            sendStatusResponse(ref, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
            return;
        }

        pendingRequestId = ref;

        switch (type)
        {
        case OfflinePacketTypeCommand:
        {
            OfflineCommandPacket cmd(buffer);
            if (!cmd.decode(ch.bytes))
            {
                DebugLogger::error("Corrupted command packet (ref %u)", ref);
                sendStatusResponse(ref, wb::HTTP_CODE_BAD_REQUEST);
                return;
            }
            handleCommand(cmd);
            break;
        }
        case OfflinePacketTypeConfig:
        {
            OfflineConfigPacket conf(buffer);
            if (!conf.decode(ch.bytes))
            {
                DebugLogger::error("Corrupted config packet (ref %u)", ref);
                sendStatusResponse(ref, wb::HTTP_CODE_BAD_REQUEST);
                return;
            }

            asyncPut(
                WB_RES::LOCAL::OFFLINE_CONFIG(), 
                AsyncRequestOptions::ForceAsync, 
                conf.getConfig());
            break;
        }
        default:
        {
            DebugLogger::info("Ignored packet (ref %u)", ref);
            pendingRequestId = OFFLINE_PACKET_INVALID_REF;
            break;
        }
        }
        break;
    }
    case WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::LID:
    {
        const auto& params = WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::EVENT::ParameterListRef(parameters);

        auto index = params.getLogId();
        auto data = value.convertTo<const WB_RES::LogDataNotification&>();

        if (data.bytes.size() > 0)
        {
            sendPartialData(
                &data.bytes[0], data.bytes.size(),
                logDataTransmission.size, data.offset);
        }
        else
        {
            // completed
            asyncUnsubscribe(resourceId, AsyncRequestOptions::Empty, index);
            logDataTransmission = {};
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
    constexpr uint8_t CHARACTERISTICS_COUNT = 2;
    WB_RES::GattSvc offlineSvc;
    WB_RES::GattChar offlineChars[CHARACTERISTICS_COUNT];

    WB_RES::GattChar& rx = offlineChars[0];
    WB_RES::GattProperty rxProps[] = {
        WB_RES::GattProperty::WRITE
    };

    WB_RES::GattChar& tx = offlineChars[1];
    WB_RES::GattProperty txProps[] = {
        WB_RES::GattProperty::NOTIFY
    };

    rx.props = wb::MakeArray<WB_RES::GattProperty>(rxProps, 1);
    rx.uuid = wb::MakeArray<uint8_t>(rxUuid, sizeof(rxUuid));

    tx.props = wb::MakeArray<WB_RES::GattProperty>(txProps, 1);
    tx.uuid = wb::MakeArray<uint8_t>(txUuid, sizeof(txUuid));

    offlineSvc.uuid = wb::MakeArray<uint8_t>(OfflineGattServiceUuid, sizeof(OfflineGattServiceUuid));
    offlineSvc.chars = wb::MakeArray<WB_RES::GattChar>(offlineChars, CHARACTERISTICS_COUNT);

    asyncPost(WB_RES::LOCAL::COMM_BLE_GATTSVC(), AsyncRequestOptions::ForceAsync, offlineSvc);
}

void OfflineGATTService::handleCommand(const OfflineCommandPacket& packet)
{
    DebugLogger::info("Received command: %u", packet.getCommand());

    switch (packet.getCommand())
    {
    case OfflineCmdReadConfig:
    {
        asyncGet(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case OfflineCmdListLogs:
    {
        asyncGet(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case OfflineCmdReadLog:
    {
        if (packet.getParameters().size() == 2)
        {
            uint16_t id = *reinterpret_cast<const uint16_t*>(packet.getParameters().begin());
            asyncSendLog(id);
        }
        else
        {
            sendStatusResponse(packet.getReference(), wb::HTTP_CODE_BAD_REQUEST);
        }
        break;
    }
    case OfflineCmdClearLogs:
    {
        asyncDelete(WB_RES::LOCAL::OFFLINE_DATA(), AsyncRequestOptions::ForceAsync);
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled command: %u", packet.getCommand());
        pendingRequestId = OFFLINE_PACKET_INVALID_REF;
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

void OfflineGATTService::sendData(const uint8_t* data, uint32_t size)
{
    ASSERT(pendingRequestId != OFFLINE_PACKET_INVALID_REF);
    uint32_t sent = 0;
    do
    {
        OfflineDataPacket packet(buffer);
        uint32_t len = WB_MIN(size, packet.MAX_PAYLOAD);
        packet.setReference(pendingRequestId);
        packet.setOffset(sent);
        packet.setTotalBytes(size);
        packet.pushBytes(data + sent, len);
        sendPacket(packet);

        sent += len;
    } while (sent < size);

    pendingRequestId = OFFLINE_PACKET_INVALID_REF;
}

void OfflineGATTService::sendPartialData(const uint8_t* data, uint32_t partSize, uint32_t totalSize, uint32_t offset)
{
    ASSERT(pendingRequestId != OFFLINE_PACKET_INVALID_REF);
    uint32_t sent = 0;
    do
    {
        OfflineDataPacket packet(buffer);
        uint32_t len = WB_MIN(partSize - sent, packet.MAX_PAYLOAD);
        packet.setReference(pendingRequestId);
        packet.setOffset(offset + sent);
        packet.setTotalBytes(totalSize);
        packet.pushBytes(data + sent, len);
        sendPacket(packet);

        sent += len;
    } while (sent < partSize);

    if(partSize + offset == totalSize)
        pendingRequestId = OFFLINE_PACKET_INVALID_REF;
}

void OfflineGATTService::sendSbem(wb::LocalResourceId resourceId, const wb::Value& data)
{
    ASSERT(pendingRequestId != OFFLINE_PACKET_INVALID_REF);
    uint32_t sent = 0;
    uint32_t size = getSbemLength(resourceId, data);
    do
    {
        OfflineDataPacket packet(buffer);
        packet.setReference(pendingRequestId);
        packet.setOffset(sent);
        packet.setTotalBytes(size);

        sent += writeToSbemBuffer(
            packet.getRawBytes(), packet.MAX_PAYLOAD - 8, 
            sent, resourceId, data);

        sendPacket(packet);
    } while (sent < size);

    pendingRequestId = OFFLINE_PACKET_INVALID_REF;
}

void OfflineGATTService::sendStatusResponse(uint8_t requestRef, uint16_t status)
{
    ASSERT(requestRef != OFFLINE_PACKET_INVALID_REF);
    static uint8_t buf[OfflineStatusPacket::SIZE] = {};
    
    OfflineStatusPacket packet(buf);
    packet.setReference(requestRef);
    packet.setStatus(status);
    sendPacket(packet);

    if(pendingRequestId == requestRef)
        pendingRequestId = OFFLINE_PACKET_INVALID_REF;
}

void OfflineGATTService::sendPacket(OfflinePacket& packet)
{
    WB_RES::Characteristic value;
    value.bytes = packet.encode();
    asyncPut(txChar.resourceId, AsyncRequestOptions::Empty, value);
}

bool OfflineGATTService::asyncSendLog(uint32_t id)
{
    if (logDataTransmission.index > -1) // Transmission in progress
        return false;

    logDataTransmission.index = id;
    logDataTransmission.size = 0;

    asyncGet(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync, id);
    return true;
}

void OfflineGATTService::asyncReadLogData(const WB_RES::LogEntry& entry)
{
    if (logDataTransmission.index != (int32_t)entry.id || !entry.size.hasValue())
        return;

    logDataTransmission.size = entry.size.getValue();

    asyncSubscribe(
        WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA(),
        AsyncRequestOptions::ForceAsync,
        entry.id);
}
