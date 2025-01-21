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
    constexpr uint32_t sampleSize = (3 * 3); // 24-bit 3D vec
    int samples = payload / sampleSize;

    if (payload < 0 || samples <= 0)
        return false;

    readValue<OfflineTimestamp>(data, offset + 0, timestamp);
    offset += sizeof(OfflineTimestamp);

    for (auto i = 0; i < samples; i++)
    {
        Vec3_Q12_12 sample;
        if (!sample.readFrom(data, offset + i * sampleSize))
            return false;
        measurements.push_back(sample);
    }
    return true;
}

bool OfflineGyroData::readFrom(const std::vector<char>& data, size_t offset)
{
    int payload = data.size() - sizeof(timestamp);
    constexpr uint32_t sampleSize = (3 * 3); // 24-bit 3D vec
    int samples = payload / sampleSize;

    if (payload < 0 || samples <= 0)
        return false;

    readValue<OfflineTimestamp>(data, offset + 0, timestamp);
    offset += sizeof(OfflineTimestamp);

    for (auto i = 0; i < samples; i++)
    {
        Vec3_Q12_12 sample;
        if (!sample.readFrom(data, offset + i * sampleSize))
            return false;
        measurements.push_back(sample);
    }
    return true;
}

bool OfflineMagnData::readFrom(const std::vector<char>& data, size_t offset)
{
    int payload = data.size() - sizeof(timestamp);
    constexpr uint32_t sampleSize = (2 * 3); // 16-bit 3D vec
    int samples = payload / sampleSize;

    if (payload < 0 || samples <= 0)
        return false;

    readValue<OfflineTimestamp>(data, offset + 0, timestamp);
    offset += sizeof(OfflineTimestamp);

    for (auto i = 0; i < samples; i++)
    {
        Vec3_Q10_6 sample;
        if (!sample.readFrom(data, offset + i * sampleSize))
            return false;
        measurements.push_back(sample);
    }
    return true;
}

bool OfflineHRData::readFrom(const std::vector<char>& data, size_t offset)
{
    int sampleDataSize = data.size() - sizeof(uint8); // Average BPM as u8 (0 - 250)
    constexpr uint32_t sampleSize = sizeof(uint16_t); // 16-bit  samples
    int samples = sampleDataSize / sampleSize;

    if (sampleDataSize < 0 || samples <= 0)
        return false;

    readValue<uint8>(data, offset + 0, average);
    offset += sizeof(uint8);

    for (auto i = 0; i < samples; i++)
    {
        uint16_t rr;
        if (!readValue(data, offset + i * sampleSize, rr))
            return false;
        rrValues.push_back(rr);
    }
    return true;
}

bool OfflineECGData::readFrom(const std::vector<char>& data, size_t offset)
{
    int payload = data.size() - sizeof(timestamp);
    constexpr uint32_t sampleSize = sizeof(int16_t); // 16-bit signed values
    int samples = payload / sampleSize;

    if (payload < 0 || samples <= 0)
        return false;

    readValue<OfflineTimestamp>(data, offset + 0, timestamp);
    offset += sizeof(OfflineTimestamp);
    
    for (auto i = 0; i < samples; i++)
    {
        int16_t sample;
        if (!readValue(data, offset + i * sampleSize, sample))
            return false;
        sampleData.push_back(sample);
    }

    return true;
}

bool OfflineTempData::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        readValue<int8>(data, offset, measurement) &&
        readValue<OfflineTimestamp>(data, offset + sizeof(int8), timestamp)
        );
}

bool OfflineActivityData::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        readValue<uint32>(data, offset, activity) &&
        readValue<OfflineTimestamp>(data, offset + sizeof(uint32), timestamp)
        );
}

bool OfflineTapData::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        readValue<uint8>(data, offset + 0, count) &&
        readValue<OfflineTimestamp>(data, offset + sizeof(count), timestamp) &&
        magnitude.readFrom(data, offset + sizeof(count) + sizeof(timestamp))
        );
}

bool Vec3_Q16_8::readFrom(const std::vector<char>& data, size_t offset)
{
    constexpr uint32_t componentSize = 3; // Components as 24-bit fixed-point values
    return (
        x.readFrom(data, offset + 0 * componentSize) &&
        y.readFrom(data, offset + 1 * componentSize) &&
        z.readFrom(data, offset + 2 * componentSize)
        );
}

bool Q16_8::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        readValue<uint8>(data, offset + 0, fraction) &&
        readValue<int16>(data, offset + 1, integer)
        );
}

bool Vec3_Q12_12::readFrom(const std::vector<char>& data, size_t offset)
{
    constexpr uint32_t componentSize = 3; // Components as 24-bit fixed-point values
    return (
        x.readFrom(data, offset + 0 * componentSize) &&
        y.readFrom(data, offset + 1 * componentSize) &&
        z.readFrom(data, offset + 2 * componentSize)
        );
}

bool Q12_12::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        readValue<uint8>(data, offset + 0, b0) &&
        readValue<uint8>(data, offset + 1, b1) &&
        readValue<int8>(data, offset + 2, b2)
        );
}

bool Vec3_Q10_6::readFrom(const std::vector<char>& data, size_t offset)
{
    constexpr uint32_t componentSize = 2; // Components as 16-bit fixed-point values
    return (
        x.readFrom(data, offset + 0 * componentSize) &&
        y.readFrom(data, offset + 1 * componentSize) &&
        z.readFrom(data, offset + 2 * componentSize)
        );
}

bool Q10_6::readFrom(const std::vector<char>& data, size_t offset)
{
    return readValue<int16>(data, offset, value);
}
