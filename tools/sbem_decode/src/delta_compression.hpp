#pragma once
#include "elias_gamma.hpp"
#include <functional>

template<typename TSample, size_t BlockSize>
class DeltaCompression
{
public:
    using write_callback = std::function<void(char data[BlockSize])>;

private:
    static_assert(sizeof(TSample) <= (BlockSize + 1) && "Too small block size");

    static inline TSample calculate_diffs(
        TSample init,
        const TSample* samples,
        size_t sampleCount,
        TSample* outDeltas)
    {
        TSample value = init;
        for (size_t i = 0; i < sampleCount; i++)
        {
            outDeltas[i] = samples[i] - value;
            value = samples[i];
        }
        return value;
    }

    static inline TSample calculate_values(
        TSample init,
        const TSample* deltas,
        size_t deltaCount,
        TSample* outSamples)
    {
        TSample value = init;
        for (size_t i = 0; i < deltaCount; i++)
        {
            outSamples[i] = value + deltas[i];
            value = outSamples[i];
        }
        return value;
    }

    inline void write_block(write_callback callback)
    {
        m_buffer[0] = m_bufferedSamples;
        callback(m_buffer);
    }

    bool m_initialize;
    char m_buffer[BlockSize];
    size_t m_usedBits;
    uint8_t m_bufferedSamples;
    TSample m_value;

    static constexpr size_t MAX_DIFFS = 8;

public:
    DeltaCompression()
        : m_initialize(true)
        , m_usedBits(0)
        , m_bufferedSamples(0)
        , m_value(0)
    {
        memset(m_buffer, 0x00, BlockSize);
    }

    size_t pack_continuous(const std::vector<TSample>& samples, write_callback callback)
    {
        size_t sampleCount = samples.size();
        size_t processedSamples = 0;

        while (processedSamples < sampleCount)
        {
            if (m_initialize) // Start a new block with absolute initial value
            {
                m_value = samples[processedSamples];
                memcpy(m_buffer + 1, &m_value, sizeof(TSample));

                m_usedBits = sizeof(TSample) * 8;
                m_initialize = false;
                m_bufferedSamples = 1;

                processedSamples += 1;
            }
            else // Append diffs in Elias Gamma VLC with bijection for negative deltas
            {
                TSample diffs[MAX_DIFFS] = {};
                size_t count = std::min(MAX_DIFFS, sampleCount - processedSamples);
                m_value = calculate_diffs(m_value, &samples[processedSamples], count, diffs);

                size_t encoded = elias_gamma::encode_buffer<TSample, true>(diffs, count, m_buffer + 1, BlockSize - 1, m_usedBits);
                processedSamples += encoded;
                m_bufferedSamples += encoded;

                if (encoded < count || m_usedBits == (BlockSize - 1) * 8) // buffer full
                {
                    write_block(callback);
                    m_initialize = true; // Start new block on next samples
                }
            }
        }

        return processedSamples;
    }

    static std::vector<TSample> decompress_block(const char* data)
    {
        std::vector<TSample> samples;
        size_t sampleCount = data[0];

        TSample init = 0;
        memcpy(&init, &data[1], sizeof(TSample));
        samples.push_back(init);

        size_t bitOffset = 0;
        size_t deltasOffset = sizeof(TSample) + 1;
        size_t deltasLen = BlockSize - deltasOffset;
        while (samples.size() < sampleCount)
        {
            TSample diff;
            if (elias_gamma::decode_value<TSample, true>(data + deltasOffset, deltasLen, diff, bitOffset))
            {
                TSample sample;
                init = calculate_values(init, &diff, 1, &sample);
                samples.push_back(sample);
            }
            else
            {
                break;
            }
        }
        return samples;
    }
};
