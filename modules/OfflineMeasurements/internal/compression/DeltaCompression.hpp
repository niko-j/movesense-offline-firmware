#pragma once
#include "EliasGamma.hpp"
#include <functional>

namespace offline_meas::compression
{
    template<typename TSample, size_t BlockSize>
    class DeltaCompression
    {
    public:
        using write_callback = std::function<void(uint8_t[BlockSize])>;

    private:
        static_assert(sizeof(TSample) <= (BlockSize + 1) && "Too small block size");

        bool m_initialize;
        uint8_t m_buffer[BlockSize - 1];
        size_t m_usedBits;
        uint8_t m_bufferedSamples;
        TSample m_value;

        static constexpr size_t MAX_DIFFS = 8;

        static inline TSample calculate_diffs(
            TSample init, const TSample* samples, size_t sampleCount, TSample* outDeltas)
        {
            TSample value = init;
            for (size_t i = 0; i < sampleCount; i++)
            {
                outDeltas[i] = samples[i] - value;
                value = samples[i];
            }
            return value;
        }

        inline void write_block(write_callback callback)
        {
            m_buffer[0] = m_bufferedSamples;
            callback(m_buffer);
        }

    public:
        DeltaCompression()
            : m_initialize(true)
            , m_usedBits(0)
            , m_bufferedSamples(0)
            , m_value(0)
        {
            memset(m_buffer, 0x00, BlockSize);
        }

        size_t pack_continuous(const wb::Array<TSample>& samples, write_callback callback)
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
                    size_t count = WB_MIN(MAX_DIFFS, sampleCount - processedSamples);
                    m_value = calculate_diffs(m_value, &samples[processedSamples], count, diffs);

                    size_t encoded = elias_gamma::encode_buffer<TSample>(diffs, count, m_buffer + 1, BlockSize - 1, m_usedBits);
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

        void reset()
        {
            m_initialize = true;
            m_usedBits = 0;
            m_bufferedSamples = 0;
            m_value = 0;
            memset(m_buffer, 0x00, sizeof(m_buffer));
        }
    };
} // namespace offline_meas::compression
