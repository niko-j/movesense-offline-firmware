#pragma once
#include <cstdlib>

namespace elias_gamma
{
    inline size_t count_leading_zeros(char b)
    {
        for (size_t i = 0; i < 8; i++)
        {
            if ((b << i) & 128)
                return i;
        }
        return 8;
    }

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

    template<typename T, bool Signed>
    inline size_t get_encoded_length(const T& value)
    {
        uint64_t data = abs(value);
        if (Signed)
            data = (data << 1) + (value >= 0 ? 1 : 0);
        else
            data = data + 1;
        return (count_bits(data) - 1) * 2 + 1;
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

    template<typename T, bool Signed>
    inline bool decode_value(const char* data, size_t dataLen, T& valueOut, size_t& bitOffset)
    {
        size_t n = 0;
        size_t pos = bitOffset / 8;
        size_t offset = bitOffset % 8;
        int read_bits = 0;
        size_t max_bits = sizeof(T) * 8;
        valueOut = 0;

        while (pos < dataLen)
        {
            if (!read_bits)
            {
                char b = data[pos] << offset;
                size_t leadingZeros = count_leading_zeros(b);

                if (leadingZeros == 8)
                {
                    n += (8 - offset);
                    pos += 1;
                    offset = 0; // Start next byte from start
                    continue;
                }
                else
                {
                    n += leadingZeros;
                    if (n > max_bits)
                        return false; // Too many bits

                    offset = offset + leadingZeros % 8; // Start reading bits from this offset

                    pos += offset / 8;
                    offset %= 8;

                    read_bits = n + 1;
                    // pos stays, read byte again on next iter
                }
            }
            else
            {
                char b = data[pos];

                // Read value bits from the byte, start after zeros (if any)
                for (int i = 7 - offset; i >= 0 && read_bits > 0; i--)
                {
                    read_bits -= 1;
                    bool bit = (b >> i) & 0b1;

                    if (bit)
                        valueOut += std::pow(2.0, read_bits);
                }

                if (read_bits > 0)
                {
                    pos += 1; // move to next byte and
                    offset = 0; // start from the beginning of the next byte
                }
                else
                {
                    if (Signed)
                    {
                        if (valueOut & 0b1)
                            valueOut = (valueOut >> 1);
                        else
                            valueOut = (valueOut >> 1) * -1;
                    }
                    else
                    {
                        valueOut -= 1;
                    }
                    bitOffset += 2 * n + 1; // new offset for the next value
                    return true;
                }
            }
        }

        return false;
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
                size_t count = std::min(8 - offset, sampleBits - valueWrittenBits);
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

