#include "offline_measurements.hpp"
#include "delta_compression.hpp"
#include "bit_pack.hpp"

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
    return readValue<uint8>(data, offset + 0, average);
}

bool OfflineRRData::readFrom(const std::vector<char>& data, size_t offset)
{
    int sampleDataSizeInBits = data.size() * 8;
    constexpr uint32_t sampleSizeInBits = 12; // 12-bit samples bit-packed
    int samples = sampleDataSizeInBits / sampleSizeInBits;

    if (sampleDataSizeInBits < 0 || samples <= 0 || sampleDataSizeInBits % 12)
        return false;

    intervals.resize(data.size());
    memcpy(intervals.data(), data.data(), data.size());
    return true;
}

std::vector<uint16_t> OfflineRRData::unpack() const
{
    return bit_pack::unpack<uint16_t, 12>(intervals);
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
        readValue<OfflineTimestamp>(data, offset, timestamp) &&
        activity.readFrom(data, offset + sizeof(timestamp))
        );
}

bool OfflineTapData::readFrom(const std::vector<char>& data, size_t offset)
{
    return (
        readValue<uint8>(data, offset + 0, count) &&
        readValue<OfflineTimestamp>(data, offset + sizeof(count), timestamp)
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
        readValue<uint8>(data, offset + 0, b0) &&
        readValue<uint8>(data, offset + 1, b1) &&
        readValue<int8>(data, offset + 2, b2)
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
    return (
        readValue<uint8>(data, offset + 0, b0) &&
        readValue<int8>(data, offset + 1, b1)
        );
}
