#include "sbem_types.hpp"

template<typename T>
bool readValue(const std::vector<char>& data, size_t offset, T& out)
{
    if (offset + sizeof(T) > data.size())
        return false;
    out = *reinterpret_cast<const T*>(data.data() + offset);
    return true;
}

bool OfflineAccData::readFrom(const std::vector<char>& data, size_t offset)
{
    int payload = data.size() - sizeof(timestamp);
    int samples = payload / 9;

    if (payload < 0 || samples <= 0)
        return false;

    readValue<OfflineTimestamp>(data, offset + 0, timestamp);
    offset += sizeof(OfflineTimestamp);

    for (auto i = 0; i < samples; i += 9)
    {
        FixedPointVec3_S16_8 sample;
        if (!sample.readFrom(data, offset + i * 9))
            return false;
        measurements.push_back(sample);
    }
    return true;
}

bool OfflineGyroData::readFrom(const std::vector<char>& data, size_t offset)
{
    int payload = data.size() - sizeof(timestamp);
    int samples = payload / 9;

    if (payload < 0 || samples <= 0)
        return false;

    readValue<OfflineTimestamp>(data, offset + 0, timestamp);
    offset += sizeof(OfflineTimestamp);

    for (auto i = 0; i < samples; i += 9)
    {
        FixedPointVec3_S16_8 sample;
        if (!sample.readFrom(data, offset + i * 9))
            return false;
        measurements.push_back(sample);
    }
    return true;
}

bool OfflineMagnData::readFrom(const std::vector<char>& data, size_t offset)
{
    int payload = data.size() - sizeof(timestamp);
    int samples = payload / 9;

    if (payload < 0 || samples <= 0)
        return false;

    readValue<OfflineTimestamp>(data, offset + 0, timestamp);
    offset += sizeof(OfflineTimestamp);

    for (auto i = 0; i < samples; i += 9)
    {
        FixedPointVec3_S16_8 sample;
        if (!sample.readFrom(data, offset + i * 9))
            return false;
        measurements.push_back(sample);
    }
    return true;
}

bool OfflineHRData::readFrom(const std::vector<char>& data, size_t offset)
{
    int sampleDataSize = data.size() - sizeof(average);
    int samples = sampleDataSize / 2;

    if (sampleDataSize < 0 || samples <= 0)
        return false;

    readValue<uint16>(data, offset + 0, average);
    offset += 2;

    for (auto i = 0; i < samples; i += 2)
    {
        uint16_t rr;
        if (!readValue(data, offset + i * 2, rr))
            return false;
        rrValues.push_back(rr);
    }
    return true;
}

bool OfflineECGData::readFrom(const std::vector<char>& data, size_t offset)
{
    int payload = data.size() - sizeof(timestamp);
    int samples = payload / 3;

    if (payload < 0 || samples <= 0)
        return false;

    readValue<OfflineTimestamp>(data, offset + 0, timestamp);
    offset += sizeof(OfflineTimestamp);
    
    for (auto i = 0; i < samples; i += 3)
    {
        FixedPoint_S16_8 sample;
        if (!sample.readFrom(data, offset + i * 3))
            return false;
        sampleData.push_back(sample);
    }

    return true;
}

bool OfflineTempData::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        readValue<OfflineTimestamp>(data, offset + 0, timestamp) &&
        readValue<int8>(data, offset + sizeof(OfflineTimestamp), measurement)
        );
}

bool FixedPointVec3_S16_8::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        x.readFrom(data, offset + 0) &&
        y.readFrom(data, offset + 3) &&
        z.readFrom(data, offset + 6)
        );
}

bool FixedPoint_S16_8::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        readValue<uint8>(data, offset + 0, fraction) &&
        readValue<int16>(data, offset + 1, integer)
        );
}
