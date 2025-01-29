#pragma once
#include <wb-resources/resources.h>

namespace elias_gamma
{
    template<typename T>
    inline size_t count_bits(const T& value)
    {
        size_t result = 0;
        uint64_t v = static_cast<uint64_t>(value);
        for (size_t i = 0; i < 64; i++)
        {
            if ((v >> i) & 0b1)
                result = (i + 1);
        }
        return result;
    }

    inline void write_bits(char* c, char bits, size_t offset, size_t count)
    {
        char mask = (0xFF << (8 - offset));
        char set = (bits << (8 - (count + offset)));
        *c = (*c & mask) | (set & ~mask);
    }

    template<typename T, bool Signed>
    inline int encode_value(const T& value, uint64_t& outValue)
    {
        outValue = 0;

        uint64_t data = abs(value);
        if (Signed)
            data = (data << 1) + (value >= 0 ? 1 : 0);
        else
            data = data + 1;

        outValue = data;

        int n = count_bits<T>(data) - 1;
        return n * 2 + 1;
    }

    /// @brief Encode samples using Elias Gamma encoding with bijection
    /// @tparam T Type of samples
    /// @tparam Signed 
    /// @param samples Pointer to samples
    /// @param count Number of samples
    /// @param outBuffer Pointer to output buffer
    /// @param bufferSize Size of the buffer
    /// @param writtenBits Reference to bit counter
    /// @return Number of encoded samples, 0 if encoding failed
    template<typename T, bool Signed>
    size_t encode_buffer(const T* samples, size_t count, char* outBuffer, size_t bufferSize, size_t& writtenBits)
    {
        size_t samplesEncoded = 0;

        for (size_t i = 0; i < count; i++)
        {
            uint64_t encodedValue = 0;
            int sampleBits = encode_value<T, Signed>(samples[i], encodedValue);

            if (!(writtenBits + sampleBits < bufferSize * 8))
                break; // Out of buffer

            size_t valueWrittenBits = 0;
            while (valueWrittenBits < sampleBits)
            {
                size_t bufOffset = writtenBits / 8;
                size_t offset = writtenBits % 8;
                size_t count = WB_MIN(8 - offset, sampleBits - valueWrittenBits);
                size_t left = sampleBits - valueWrittenBits;

                char value = encodedValue >> (left - count);
                write_bits(outBuffer + bufOffset, value, offset, count);

                writtenBits += count;
                valueWrittenBits += count;
            }

            samplesEncoded++;
        }

        return samplesEncoded;
    }
} // namespace elias_gamma
