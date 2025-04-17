#pragma once
#include <cstdlib>
#include <cstring>
#include <functional>

template<size_t BlockSize, typename TSample, typename TDiff>
class ECGCompression
{
public:
    using write_callback = std::function<void(uint8_t[BlockSize])>;

private:
    bool m_initialize;
    uint8_t m_buffer[BlockSize];
    size_t m_usedBits;
    uint8_t m_bufferedSamples;
    TSample m_value;
    static constexpr size_t MAX_DIFFS = 15;

    static TSample calculate_diffs(TSample init, const TSample* samples, size_t sampleCount, TDiff* outDeltas)
    {
        TSample value = init;
        for (size_t i = 0; i < sampleCount; i++)
        {
            outDeltas[i] = ((TDiff)samples[i]) - value;
            value = samples[i];
        }
        return value;
    }

    template<typename TSigned>
    static size_t count_bits_unsigned(const TSigned& value)
    {
        int32_t a = abs(value);
        float l = a > 0 ? log2f(a) : 0.0f;
        return (size_t)floor(l);
    }

    static size_t encode_value(const TDiff& value, uint64_t& out)
    {
        out = (abs(value) << 1) + (value >= 0 ? 1 : 0);
        int n = count_bits_unsigned<int64_t>(out);
        return n * 2 + 1;
    }

    static void write_bits(uint8_t* c, uint8_t bits, size_t offset, size_t count)
    {
        uint8_t mask = (0xFF << (8 - offset));
        uint8_t set = (bits << (8 - (count + offset)));
        *c = (*c & mask) | (set & ~mask);
    }

    /// Encode values using Elias Gamma encoding with bijection
    size_t encode_buffer(const TDiff* values, size_t count, uint8_t* outBuffer, size_t bufferSize, size_t& writtenBits)
    {
        size_t samplesEncoded = 0;

        for (size_t i = 0; i < count; i++)
        {
            uint64_t encodedValue = 0;
            size_t sampleBits = encode_value(values[i], encodedValue);
            if (!(writtenBits + sampleBits < bufferSize * 8))
                break; // Out of buffer

            size_t valueWrittenBits = 0;
            while (valueWrittenBits < sampleBits)
            {
                size_t bufOffset = writtenBits / 8;
                size_t offset = writtenBits % 8;
                size_t bitCount = WB_MIN(8 - offset, sampleBits - valueWrittenBits);
                size_t left = sampleBits - valueWrittenBits;

                uint8_t value = encodedValue >> (left - bitCount);
                write_bits(outBuffer + bufOffset, value, offset, bitCount);

                writtenBits += bitCount;
                valueWrittenBits += bitCount;
            }

            samplesEncoded++;
        }

        return samplesEncoded;
    }

    void write_block(write_callback callback)
    {
        if (!m_initialize && m_bufferedSamples > 0)
        {
            m_buffer[0] = m_bufferedSamples;
            callback(m_buffer);
        }
    }

public:
    void reset()
    {
        m_initialize = true;
        m_usedBits = 0;
        m_bufferedSamples = 0;
        m_value = 0;
        memset(m_buffer, 0x00, sizeof(m_buffer));
    }

    ECGCompression()
    {
        reset();
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
                memcpy(m_buffer + 1, &m_value, sizeof(m_value));

                m_usedBits = sizeof(m_value) * 8;
                m_initialize = false;
                m_bufferedSamples = 1;

                processedSamples += 1;
            }
            else // Append diffs in Elias Gamma VLC with bijection for negative deltas
            {
                TDiff diffs[MAX_DIFFS] = {};
                size_t count = WB_MIN(MAX_DIFFS, sampleCount - processedSamples);
                m_value = calculate_diffs(m_value, &samples[processedSamples], count, diffs);

                size_t encoded = encode_buffer(diffs, count, m_buffer + 1, BlockSize - 1, m_usedBits);
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

    void dump_buffer(write_callback callback)
    {
        write_block(callback);
        m_initialize = true;
    }
};
