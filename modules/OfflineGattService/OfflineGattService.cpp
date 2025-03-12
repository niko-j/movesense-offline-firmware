#include "OfflineGattService.hpp"

#include "app-resources/resources.h"
#include "comm_ble/resources.h"
#include "comm_ble_gattsvc/resources.h"
#include "sbem_types.h"
#include "mem_logbook/resources.h"
#include "movesense_time/resources.h"
#include "system_debug/resources.h"

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

#define OFFLINE_GATT_CONVERSIONS_IMPL
#include "internal/Conversions.hpp"

using namespace gatt_svc;

const char* const OfflineGattService::LAUNCHABLE_NAME = "OfflineGatt";

OfflineGattService::OfflineGattService()
    : ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , m_download({})
    , m_debugLogStream({})
    , serviceHandle(0)
    , pendingRequestId(Packet::INVALID_REF)
{

}

OfflineGattService::~OfflineGattService()
{

}

bool OfflineGattService::initModule()
{
    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void OfflineGattService::deinitModule()
{
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool OfflineGattService::startModule()
{
    constexpr uint8_t CHARACTERISTICS_COUNT = 2;
    WB_RES::GattSvc offlineSvc;
    WB_RES::GattChar offlineChars[CHARACTERISTICS_COUNT];

    WB_RES::GattProperty rxProps[] = { WB_RES::GattProperty::WRITE };
    WB_RES::GattProperty txProps[] = { WB_RES::GattProperty::NOTIFY };

    offlineChars[0].props = wb::MakeArray<WB_RES::GattProperty>(rxProps, 1);
    offlineChars[0].uuid = wb::MakeArray<uint8_t>(SENSOR_GATT_CHAR_RX_UUID, sizeof(SENSOR_GATT_CHAR_RX_UUID));

    offlineChars[1].props = wb::MakeArray<WB_RES::GattProperty>(txProps, 1);
    offlineChars[1].uuid = wb::MakeArray<uint8_t>(SENSOR_GATT_CHAR_TX_UUID, sizeof(SENSOR_GATT_CHAR_TX_UUID));

    offlineSvc.uuid = wb::MakeArray<uint8_t>(SENSOR_GATT_SERVICE_UUID, sizeof(SENSOR_GATT_SERVICE_UUID));
    offlineSvc.chars = wb::MakeArray<WB_RES::GattChar>(offlineChars, CHARACTERISTICS_COUNT);

    asyncPost(WB_RES::LOCAL::COMM_BLE_GATTSVC(), AsyncRequestOptions::ForceAsync, offlineSvc);

    WB_RES::DebugMessageConfig config = {
        .systemMessages = true,
        .userMessages = true,
    };

    asyncPut(WB_RES::LOCAL::SYSTEM_DEBUG_CONFIG(), AsyncRequestOptions::ForceAsync, config);
    asyncSubscribe(WB_RES::LOCAL::SYSTEM_DEBUG_LEVEL(), AsyncRequestOptions::ForceAsync, WB_RES::DebugLevel::INFO);
    asyncSubscribe(WB_RES::LOCAL::COMM_BLE_PEERS(), AsyncRequestOptions::ForceAsync);

    mModuleState = WB_RES::ModuleStateValues::STARTED;
    return true;
}

void OfflineGattService::stopModule()
{
    asyncUnsubscribe(WB_RES::LOCAL::COMM_BLE_PEERS());
    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_DEBUG_LEVEL::ID);

    asyncUnsubscribe(txChar.resourceId);
    asyncUnsubscribe(rxChar.resourceId);

    releaseResource(txChar.resourceId);
    releaseResource(rxChar.resourceId);

    txChar = {}; rxChar = {};
    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineGattService::onGetResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onGetResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

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

        for (size_t i = 0; i < svc.chars.size(); i++)
        {
            const WB_RES::GattChar& c = svc.chars[i];
            uint16_t uuid16 = *reinterpret_cast<const uint16_t*>(&(c.uuid[12]));

            if (uuid16 == SENSOR_GATT_CHAR_TX_UUID16)
                txChar.handle = c.handle.hasValue() ? c.handle.getValue() : 0;
            else if (uuid16 == SENSOR_GATT_CHAR_RX_UUID16)
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
            pendingRequestId = Packet::INVALID_REF;
        }
        else
        {
            sendStatusResponse(pendingRequestId, resultCode);
            return;
        }

        break;
    }
    case WB_RES::LOCAL::OFFLINE_DEBUG::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            auto info = result.convertTo<const WB_RES::OfflineDebugInfo&>();

            size_t size = info.lastFault.size() + sizeof(info.resetTime);
            uint8_t data[50] = {};
            if (size > 50)
            {
                sendStatusResponse(pendingRequestId, wb::HTTP_CODE_INTERNAL_SERVER_ERROR);
                return;
            }

            memcpy(data, &info.resetTime, sizeof(info.resetTime));
            memcpy(data + sizeof(info.resetTime), info.lastFault.begin(), info.lastFault.size());
            sendData(data, size);
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
            sendStatusResponse(pendingRequestId, wb::HTTP_CODE_NO_CONTENT);
            return;
        }

        // Are we are looking for a particular log?
        bool log_download = (m_download.index > 0);
        bool list_complete = (resultCode == wb::HTTP_CODE_OK);
        uint32_t logCount = entries.elements.size();

        LogListPacket packet(pendingRequestId);

        for (size_t i = 0; i < logCount; i++)
        {
            const auto& el = entries.elements[i];
            if (log_download)
            {
                if (el.id == m_download.index)
                {
                    m_download.size = el.size.hasValue() ? el.size.getValue() : 0;
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
                if (all_items_sent || packet.count == LogListPacket::MAX_ITEMS)
                {
                    packet.complete = (list_complete && all_items_sent);
                    sendPacket(packet);

                    if (packet.complete)
                        pendingRequestId = Packet::INVALID_REF;

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
            if (m_download.size > 0) // Get data
            {
                asyncSubscribe(
                    WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA(),
                    AsyncRequestOptions::ForceAsync,
                    m_download.index);
            }
            else
            {
                m_download = {};
                sendStatusResponse(pendingRequestId, wb::HTTP_CODE_NO_CONTENT);
            }
        }

        break;
    }
    default:
        break;
    }
}

void OfflineGattService::onPutResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onPutResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        sendStatusResponse(pendingRequestId, resultCode);
        break;
    }
    case WB_RES::LOCAL::TIME::LID:
    {
        sendStatusResponse(pendingRequestId, resultCode);
        break;
    }
    default:
        break;
    }
}

void OfflineGattService::onPostResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onPostResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

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
        break;
    }
    default:
        break;
    }
}

void OfflineGattService::onDeleteResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onDeleteResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES::LID:
    {
        sendStatusResponse(pendingRequestId, resultCode);
        break;
    }
    default:
        break;
    }
}

void OfflineGattService::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onSubscribeResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
            sendStatusResponse(pendingRequestId, resultCode);
        break;
    }
    default:
        break;
    }
}

void OfflineGattService::onUnsubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onUnsubscribeResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }
}

void OfflineGattService::onNotify(
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
        if (ch.bytes.size() == 0 && ch.notifications.hasValue())
        {
            DebugLogger::info("%s: Notifications set to %s",
                LAUNCHABLE_NAME,
                ch.notifications.getValue() ? "'true'" : "'false'");
            return;
        }

        auto bytes = arrayToBuffer(ch.bytes);
        Packet::Type type;
        uint8_t ref = Packet::INVALID_REF;
        bool valid = (
            bytes.read(&type, sizeof(Packet::Type)) &&
            bytes.read(&ref, sizeof(ref)) &&
            bytes.seek_read(0)
            );

        if (!valid || ref == Packet::INVALID_REF)
        {
            DebugLogger::warning("%s: Received invalid packet", LAUNCHABLE_NAME);
            sendStatusResponse(ref, wb::HTTP_CODE_BAD_REQUEST);
            return;
        }

        DebugLogger::info("%s: Received packet ref: %u type: %u size: %u",
            LAUNCHABLE_NAME, ref, type, ch.bytes.size());

        if (pendingRequestId != Packet::INVALID_REF)
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

        pendingRequestId = ref;

        switch (type)
        {
        case Packet::TypeHandshake:
        {
            HandshakePacket handshake(ref);
            if (!handshake.Read(bytes))
            {
                DebugLogger::error("%s: Corrupted handshake packet (ref %u)", LAUNCHABLE_NAME, ref);
                sendStatusResponse(ref, wb::HTTP_CODE_BAD_REQUEST);
                return;
            }

            // Major version mismatch means breaking changes.
            if (handshake.version_major != SENSOR_PROTOCOL_VERSION_MAJOR)
            {
                DebugLogger::error("%s: Handshake failed, version mismatch. Server: %u. Client: %u",
                    LAUNCHABLE_NAME, SENSOR_PROTOCOL_VERSION_MAJOR, handshake.version_major);

                if (handshake.version_major < SENSOR_PROTOCOL_VERSION_MAJOR)
                    sendStatusResponse(ref, wb::HTTP_CODE_UPGRADE_REQUIRED);
                else
                    sendStatusResponse(ref, wb::HTTP_CODE_BAD_VERSION);
                return;
            }

            HandshakePacket response(ref); // Shake back
            sendPacket(response);
            pendingRequestId = Packet::INVALID_REF;

            break;
        }
        case Packet::TypeCommand:
        {
            CommandPacket cmd(ref);
            if (!cmd.Read(bytes))
            {
                DebugLogger::error("%s: Corrupted command packet (ref %u)", LAUNCHABLE_NAME, ref);
                sendStatusResponse(ref, wb::HTTP_CODE_BAD_REQUEST);
                return;
            }

            handleCommand(cmd);
            break;
        }
        case Packet::TypeOfflineConfig:
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
        case Packet::TypeTime:
        {
            TimePacket pkt(ref);
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
        if (m_download.index == 0)
            break; // Ignore

        const auto& params = WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA::EVENT::ParameterListRef(parameters);
        auto index = params.getLogId();
        auto data = value.convertTo<const WB_RES::LogDataNotification&>();

        if (data.bytes.size() > 0)
        {
            DebugLogger::info("%s: Sending log data %u of %u bytes, offset %u",
                LAUNCHABLE_NAME, data.bytes.size(), m_download.size, data.offset);

            sendPartialData(&data.bytes[0], data.bytes.size(), m_download.size, data.offset);
        }
        else
        {
            DebugLogger::info("%s: Finished sending log %u", LAUNCHABLE_NAME, m_download.index);
            asyncUnsubscribe(WB_RES::LOCAL::MEM_LOGBOOK_BYID_LOGID_DATA(), AsyncRequestOptions::Empty, m_download.index);
            pendingRequestId = Packet::INVALID_REF;
            m_download = {};
        }
        break;
    }
    case WB_RES::LOCAL::SYSTEM_DEBUG_LEVEL::LID:
    {
        if (m_debugLogStream.packetRef == Packet::INVALID_REF)
            return;

        auto msg = value.convertTo<const WB_RES::DebugMessage&>();
        if (m_debugLogStream.logLevel > msg.level.getValue())
            return;

        DebugMessagePacket packet(m_debugLogStream.packetRef);
        const char* message_str = msg.message;
        size_t message_len = strnlen(msg.message, packet.MAX_MESSAGE_LEN);

        packet.level = msg.level.getValue();
        packet.timestamp = msg.timestamp;
        packet.message = ReadableBuffer((const uint8_t*)message_str, message_len);
        sendPacket(packet);
        break;
    }
    case WB_RES::LOCAL::COMM_BLE_PEERS::LID:
    {
        auto peerChange = value.convertTo<const WB_RES::PeerChange&>();
        if (peerChange.state == peerChange.state.CONNECTED)
        {
            DebugLogger::info("%s: Peer connected", LAUNCHABLE_NAME);
        }
        else if (peerChange.state == peerChange.state.DISCONNECTED)
        {
            DebugLogger::info("%s: Peer disconnected", LAUNCHABLE_NAME);
            pendingRequestId = Packet::INVALID_REF;
            m_debugLogStream.packetRef = Packet::INVALID_REF;
        }
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled notification from resource: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId);
        break;
    }
}

void OfflineGattService::handleCommand(const CommandPacket& packet)
{
    DebugLogger::info("%s: Received command: %u", LAUNCHABLE_NAME, packet.command);

    switch (packet.command)
    {
    case CommandPacket::CmdReadConfig:
    {
        asyncGet(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case CommandPacket::CmdListLogs:
    {
        asyncGet(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case CommandPacket::CmdReadLog:
    {
        uint16_t id = packet.params.readLog.logIndex;
        DebugLogger::info("%s: Requested log (ref: %u) (id: %u)",
            LAUNCHABLE_NAME, pendingRequestId, id);
        sendLog(id);
        break;
    }
    case CommandPacket::CmdClearLogs:
    {
        asyncDelete(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case CommandPacket::CmdDebugLastFault:
    {
        asyncGet(WB_RES::LOCAL::OFFLINE_DEBUG(), AsyncRequestOptions::ForceAsync);
        break;
    }
    case CommandPacket::CmdStartDebugLogStream:
    {
        if (m_debugLogStream.packetRef != Packet::INVALID_REF)
        {
            sendStatusResponse(packet.reference, wb::HTTP_CODE_ALREADY_REPORTED);
            break;
        }

        m_debugLogStream.packetRef = packet.reference;
        m_debugLogStream.logLevel = packet.params.debugLog.logLevel;

        WB_RES::DebugMessageConfig config = {
            .systemMessages = (packet.params.debugLog.sources & packet.params.debugLog.System),
            .userMessages = (packet.params.debugLog.sources & packet.params.debugLog.User)
        };
        asyncPut(WB_RES::LOCAL::SYSTEM_DEBUG_CONFIG(), AsyncRequestOptions::ForceAsync, config);

        sendStatusResponse(packet.reference, wb::HTTP_CODE_ACCEPTED);
        break;
    }
    case CommandPacket::CmdStopDebugLogStream:
    {
        m_debugLogStream.packetRef = Packet::INVALID_REF;
        sendStatusResponse(packet.reference, wb::HTTP_CODE_OK);
        break;
    }
    default:
        DebugLogger::warning("%s: Unknown command: %u",
            LAUNCHABLE_NAME, packet.command);
        sendStatusResponse(packet.reference, wb::HTTP_CODE_BAD_REQUEST);
        break;
    }
}

bool OfflineGattService::asyncSubsribeHandleResource(int16_t charHandle, whiteboard::ResourceId& resourceOut)
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

void OfflineGattService::sendData(const uint8_t* data, uint32_t size)
{
    uint32_t sent = 0;
    do
    {
        DataPacket packet(pendingRequestId);
        packet.offset = sent;
        packet.totalBytes = size;

        uint32_t len = WB_MIN(size, DataPacket::MAX_PAYLOAD);
        packet.data = ReadableBuffer(data + sent, len);

        sendPacket(packet);
        sent += len;
    } while (sent < size);

    pendingRequestId = Packet::INVALID_REF;
}

void OfflineGattService::sendPartialData(const uint8_t* data, uint32_t partSize, uint32_t totalSize, uint32_t offset)
{
    uint32_t sent = 0;
    do
    {
        DataPacket packet(pendingRequestId);
        packet.offset = offset + sent;
        packet.totalBytes = totalSize;

        uint32_t len = WB_MIN(partSize - sent, DataPacket::MAX_PAYLOAD);
        packet.data = ReadableBuffer(data + sent, len);

        sendPacket(packet);
        sent += len;
    } while (sent < partSize);

    if (partSize + offset == totalSize)
        pendingRequestId = Packet::INVALID_REF;
}

void OfflineGattService::sendStatusResponse(uint8_t requestRef, uint16_t status)
{
    StatusPacket packet(requestRef, status);
    sendPacket(packet);

    if (pendingRequestId == requestRef)
        pendingRequestId = Packet::INVALID_REF;
}

void OfflineGattService::sendPacket(Packet& packet)
{
    ByteBuffer packetBuffer(buffer, Packet::MAX_PACKET_SIZE);
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

bool OfflineGattService::sendLog(uint32_t id)
{
    if (m_download.index > 0) // Transmission in progress
        return false;

    m_download.index = id;
    m_download.size = 0;

    asyncGet(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::ForceAsync);
    return true;
}
