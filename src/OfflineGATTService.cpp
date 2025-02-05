#include "OfflineGATTService.hpp"
#include "OfflineInternal.hpp"

#include "app-resources/resources.h"
#include "comm_ble_gattsvc/resources.h"
#include "sbem_types.h"
#include "mem_logbook/resources.h"
#include "movesense_time/resources.h"

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

#include "protocol/packets/OfflineCommandPacket.hpp"
#include "protocol/packets/OfflineStatusPacket.hpp"
#include "protocol/packets/OfflineConfigPacket.hpp"
#include "protocol/packets/OfflineDataPacket.hpp"
#include "protocol/packets/OfflineLogPacket.hpp"
#include "protocol/packets/OfflineTimePacket.hpp"

const char* const OfflineGATTService::LAUNCHABLE_NAME = "OfflineGATT";

/* UUID string: 0000b001-0000-1000-8000-00805f9b34fb */
constexpr uint8_t OfflineGattServiceUuid[] = { 0x01, 0xB0 };
constexpr uint8_t txUuid[] = { 0x02, 0xB0 };
constexpr uint8_t rxUuid[] = { 0x03, 0xB0 };

OfflineGATTService::OfflineGATTService()
    : ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , logDownload({})
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
    configGattSvc();
    mModuleState = WB_RES::ModuleStateValues::STARTED;
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
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::error("%s: Failed to subcribe GATT service handles, status: %d",
                LAUNCHABLE_NAME, resultCode);
            return;
        }

        const WB_RES::GattSvc& svc = result.convertTo<const WB_RES::GattSvc&>();

        uint16_t svcHandle = svc.handle.getValue();
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
            DebugLogger::error("%s: Failed to get characteristics handles for service %u",
                LAUNCHABLE_NAME, serviceHandle);
            ASSERT(false);
            return;
        }

        DebugLogger::info("%s: Received characteristics handles for service %u",
            LAUNCHABLE_NAME, serviceHandle);

        asyncSubsribeHandleResource(txChar.handle, txChar.resourceId);
        asyncSubsribeHandleResource(rxChar.handle, rxChar.resourceId);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            auto conf = result.convertTo<const WB_RES::OfflineConfig&>();

            OfflineConfigPacket packet(pendingRequestId);
            packet.config = wbToInternal(conf);
            sendPacket(packet);
            pendingRequestId = OfflinePacket::INVALID_REF;
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
        bool log_download = (logDownload.index > 0);
        bool list_complete = (resultCode == wb::HTTP_CODE_OK);
        uint32_t logCount = entries.elements.size();

        OfflineLogPacket packet(pendingRequestId);

        for (size_t i = 0; i < logCount; i++)
        {
            const auto& el = entries.elements[i];
            if (log_download)
            {
                if (el.id == logDownload.index)
                {
                    logDownload.size = el.size.hasValue() ? el.size.getValue() : 0;
                }
            }
            else // Send entries
            {
                packet.items[packet.count] = {
                    .id = el.id,
                    .size = (uint32_t)(el.size.hasValue() ? el.size.getValue() : 0),
                    .modified = el.modificationTimestamp
                };
                packet.count += 1;

                bool all_items_sent = (i + 1 == logCount);
                if (all_items_sent || packet.count == OfflineLogPacket::MAX_ITEMS)
                {
                    packet.complete = (list_complete && all_items_sent);
                    sendPacket(packet);

                    if (packet.complete)
                        pendingRequestId = OfflinePacket::INVALID_REF;

                    packet.count = 0;
                }
            }
        }

        if (resultCode == wb::HTTP_CODE_CONTINUE)
        {
            uint32_t lastId = (entries.elements.end() - 1)->id;
            asyncGet(resourceId, AsyncRequestOptions::ForceAsync, lastId);
        }

        if (list_complete && log_download)
        {
            if (logDownload.size > 0) // Get data
            {
                asyncSubscribe(
                    WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA(),
                    AsyncRequestOptions::ForceAsync,
                    logDownload.index);
            }
            else
            {
                logDownload = {};
                sendStatusResponse(pendingRequestId, wb::HTTP_CODE_NOT_FOUND);
            }
        }

        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled GET result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }

    ASSERT(resultCode < 400)
}

void OfflineGATTService::onPutResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        sendStatusResponse(pendingRequestId, resultCode);
        break;
    }
    case WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::error("%s: Characteristics PUT failed: %d",
                LAUNCHABLE_NAME, resultCode);
        }
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled PUT result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }
}

void OfflineGATTService::onPostResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_GATTSVC::LID:
    {
        if (resultCode == wb::HTTP_CODE_CREATED)
        {
            serviceHandle = (int32_t)result.convertTo<uint16_t>();
            DebugLogger::info("%s: Offline GATT service created. Handle: %d",
                LAUNCHABLE_NAME, serviceHandle);

            // Get handles for characteristics
            asyncGet(
                WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE(),
                AsyncRequestOptions::ForceAsync,
                serviceHandle);
        }
        else
        {
            DebugLogger::error("%s: Failed to create Offline GATT service: %d",
                LAUNCHABLE_NAME, resultCode);
        }
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled POST result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }
}

void OfflineGATTService::onDeleteResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES::LID:
    {
        sendStatusResponse(pendingRequestId, resultCode);
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled DELETE result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }

    ASSERT(resultCode < 400)
}

void OfflineGATTService::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::error("%s: Failed to subscribe logbook data, status: %d",
                LAUNCHABLE_NAME, resultCode);
            sendStatusResponse(pendingRequestId, resultCode);
            return;
        }
        break;
    }
    case WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE::LID:
    {
        DebugLogger::info("%s: Subscribed to characteristic handle", LAUNCHABLE_NAME);
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled SUBSCRIBE result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }
}

void OfflineGATTService::onNotify(
    wb::ResourceId resourceId,
    const wb::Value& value,
    const wb::ParameterList& parameters)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE::LID:
    {
        WB_RES::LOCAL::COMM_BLE_GATTSVC_SVCHANDLE_CHARHANDLE::SUBSCRIBE::ParameterListRef parameterRef(parameters);
        const auto& ch = value.convertTo<const WB_RES::Characteristic&>();

        auto bytes = arrayToBuffer(ch.bytes);
        OfflinePacket::Type type;
        uint8_t ref;
        bool valid = (
            bytes.read(&type, sizeof(OfflinePacket::Type)) &&
            bytes.read(&ref, sizeof(ref)) &&
            bytes.seek_read(0)
            );

        if (!valid || ref == OfflinePacket::INVALID_REF)
        {
            DebugLogger::warning("%s, Received invalid packet", LAUNCHABLE_NAME);
            return;
        }

        DebugLogger::info("%s, Received packet ref: %u type: %u size: %u",
            LAUNCHABLE_NAME, ref, type, ch.bytes.size());

        if (pendingRequestId != OfflinePacket::INVALID_REF)
        {
            if (pendingRequestId == ref)
            {
                DebugLogger::info("%s: Duplicate request (ref %u) (type %u), ignoring",
                    LAUNCHABLE_NAME, ref, type);
                return;
            }

            sendStatusResponse(ref, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
            return;
        }

        switch (type)
        {
        case OfflinePacket::TypeCommand:
        {
            OfflineCommandPacket cmd(ref);
            if (!cmd.Read(bytes))
            {
                DebugLogger::error("%s: Corrupted command packet (ref %u)", LAUNCHABLE_NAME, ref);
                sendStatusResponse(ref, wb::HTTP_CODE_BAD_REQUEST);
                return;
            }
            
            pendingRequestId = ref;
            handleCommand(cmd);
            break;
        }
        case OfflinePacket::TypeConfig:
        {
            OfflineConfigPacket pkt(ref);
            if (!pkt.Read(bytes))
            {
                DebugLogger::error("%s: Corrupted config packet (ref %u)", LAUNCHABLE_NAME, ref);
                sendStatusResponse(ref, wb::HTTP_CODE_BAD_REQUEST);
                return;
            }

            asyncPut(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::Empty, internalToWb(pkt.config));
            break;
        }
        case OfflinePacket::TypeTime:
        {
            OfflineTimePacket pkt(ref);
            if (!pkt.Read(bytes))
            {
                DebugLogger::error("%s: Corrupted time packet (ref %u)", LAUNCHABLE_NAME, ref);
                sendStatusResponse(ref, wb::HTTP_CODE_BAD_REQUEST);
                return;
            }

            asyncPut(WB_RES::LOCAL::TIME(), AsyncRequestOptions::Empty, pkt.time);
            break;
        }
        default:
        {
            DebugLogger::info("%s: Not accepting packet (ref %u) (type %u)",
                LAUNCHABLE_NAME, ref, type);
            sendStatusResponse(ref, wb::HTTP_CODE_NOT_ACCEPTABLE);
            break;
        }
        }
        break;
    }
    case WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::LID:
    {
        if (logDownload.index == 0)
            break; // Ignore

        const auto& params = WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::EVENT::ParameterListRef(parameters);
        auto index = params.getLogId();
        auto data = value.convertTo<const WB_RES::LogDataNotification&>();

        if (data.bytes.size() > 0)
        {
            DebugLogger::info("%s: Sending log data %u of %u bytes, offset %u",
                LAUNCHABLE_NAME, data.bytes.size(), logDownload.size, data.offset);

            sendPartialData(&data.bytes[0], data.bytes.size(), logDownload.size, data.offset);
        }
        else
        {
            // completed 
            logDownload = {};
        }
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled notification from resource: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId);
        break;
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
    DebugLogger::info("Received command: %u", packet.command);

    switch (packet.command)
    {
    case OfflineCommandPacket::CmdReadConfig:
    {
        asyncGet(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case OfflineCommandPacket::CmdListLogs:
    {
        asyncGet(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case OfflineCommandPacket::CmdReadLog:
    {
        uint16_t id = packet.params.ReadLogParams.logIndex;
        DebugLogger::info("%s: Requested log (ref: %u) (id: %u)",
            LAUNCHABLE_NAME, pendingRequestId, id);
        asyncSendLog(id);
        break;
    }
    case OfflineCommandPacket::CmdClearLogs:
    {
        asyncDelete(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync);
        break;
    }
    default:
        DebugLogger::warning("%s: Unknown command: %u",
            LAUNCHABLE_NAME, packet.command);
        sendStatusResponse(packet.reference, wb::HTTP_CODE_BAD_REQUEST);
        pendingRequestId = OfflinePacket::INVALID_REF;
        break;
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
    ASSERT(pendingRequestId != OfflinePacket::INVALID_REF);
    uint32_t sent = 0;
    do
    {
        OfflineDataPacket packet(pendingRequestId);
        packet.offset = sent;
        packet.totalBytes = size;

        uint32_t len = WB_MIN(size, OfflineDataPacket::MAX_PAYLOAD);
        packet.data = ReadableBuffer(data + sent, len);

        sendPacket(packet);
        sent += len;
    } while (sent < size);

    pendingRequestId = OfflinePacket::INVALID_REF;
}

void OfflineGATTService::sendPartialData(const uint8_t* data, uint32_t partSize, uint32_t totalSize, uint32_t offset)
{
    ASSERT(pendingRequestId != OfflinePacket::INVALID_REF);
    uint32_t sent = 0;
    do
    {
        OfflineDataPacket packet(pendingRequestId);
        packet.offset = offset + sent;
        packet.totalBytes = totalSize;

        uint32_t len = WB_MIN(partSize - sent, OfflineDataPacket::MAX_PAYLOAD);
        packet.data = ReadableBuffer(data + sent, len);

        sendPacket(packet);
        sent += len;
    } while (sent < partSize);

    if (partSize + offset == totalSize)
        pendingRequestId = OfflinePacket::INVALID_REF;
}

void OfflineGATTService::sendStatusResponse(uint8_t requestRef, uint16_t status)
{
    ASSERT(requestRef != OfflinePacket::INVALID_REF);

    OfflineStatusPacket packet(requestRef, status);
    sendPacket(packet);

    if (pendingRequestId == requestRef)
        pendingRequestId = OfflinePacket::INVALID_REF;
}

void OfflineGATTService::sendPacket(OfflinePacket& packet)
{
    ByteBuffer packetBuffer(buffer, OfflinePacket::MAX_PACKET_SIZE);
    packetBuffer.reset();

    if (!packet.Write(packetBuffer))
    {
        DebugLogger::error("%s: Failed to write packet!", LAUNCHABLE_NAME);
        return;
    }

    WB_RES::Characteristic value;
    value.bytes = bufferToArray(packetBuffer);
    asyncPut(txChar.resourceId, AsyncRequestOptions::Empty, value);
}

bool OfflineGATTService::asyncSendLog(uint32_t id)
{
    if (logDownload.index > 0) // Transmission in progress
        return false;

    logDownload.index = id;
    logDownload.size = 0;

    asyncGet(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync);
    return true;
}
