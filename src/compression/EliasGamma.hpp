#pragma once
#include <wb-resources/resources.h>

namespace elias_gamma
{
    template<typename T>
    inline size_t count_bits(const T& value)
    {
        T a = abs(value);
        float l = a > 0 ? log2f(a) : 0.0f;
        return 1 + (size_t)floor(l);
    }

    inline void write_bits(uint8_t* c, uint8_t bits, size_t offset, size_t count)
    {
        char mask = (0xFF << (8 - offset));
        char set = (bits << (8 - (count + offset)));
        *c = (*c & mask) | (set & ~mask);
    }

    template<typename T>
    inline size_t encode_value(const T& value, uint64_t& out)
    {
        out = (abs(value) << 1) + (value >= 0 ? 1 : 0);
        int n = count_bits<T>(out) - 1;
        return n * 2 + 1;
    }

    /// @brief Encode samples using Elias Gamma encoding with bijection
    /// @tparam T Type of samples
    /// @param samples Pointer to samples
    /// @param count Number of samples
    /// @param outBuffer Pointer to output buffer
    /// @param bufferSize Size of the buffer
    /// @param writtenBits Reference to bit counter
    /// @return Number of encoded samples, 0 if encoding failed
    template<typename T>
    size_t encode_buffer(const T* samples, size_t count, uint8_t* outBuffer, size_t bufferSize, size_t& writtenBits)
    {
        size_t samplesEncoded = 0;

        for (size_t i = 0; i < count; i++)
        {
            uint64_t encodedValue = 0;
            size_t sampleBits = encode_value<T>(samples[i], encodedValue);

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
} // namespace elias_gamma
