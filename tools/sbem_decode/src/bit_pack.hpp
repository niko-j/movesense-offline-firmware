#pragma once
#include <vector>

namespace bit_pack
{
    /// @brief Unpack bit-packed values
    /// @tparam T Data type of values
    /// @tparam N_BITS Width of the packed values
    /// @param data Buffer
    /// @return Array of unpacked values
    template<typename T, uint8_t N_BITS>
    std::vector<T> unpack(const std::vector<uint8_t>& data)
    {
        std::vector<T> unpackedValues = {};
        int bits = data.size() * 8;
        int samples = bits / N_BITS;

        uint32_t totalBitsRead = 0;
        const uint8_t* buf = data.data();
        for (size_t i = 0; i < samples; i++)
        {
            uint16_t value = 0;
            uint32_t bitsRead = 0;

            while (bitsRead < N_BITS)
            {
                uint32_t bufByteOffset = totalBitsRead / 8;
                uint32_t bufBitOffset = totalBitsRead % 8;
                uint32_t bufReadCount = std::min(8 - bufBitOffset, N_BITS - bitsRead);

                uint8_t in = *(buf + bufByteOffset);
                uint8_t in_byte = (in & (0xFF >> bufBitOffset)) >> (8 - (bufReadCount + bufBitOffset));
                value += (in_byte << (N_BITS - (bitsRead + bufReadCount)));

                bitsRead += bufReadCount;
                totalBitsRead += bufReadCount;
            }
            unpackedValues.push_back(value);
        }
        return unpackedValues;
    }
} // namespace bit_pack
