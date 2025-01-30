#pragma once
#include <wb-resources/resources.h>

namespace bit_pack
{
    template<typename T, uint8_t BitsPerValue, uint8_t ValuesInChunk>
    bool write(const T& sample, uint8_t buffer[BitsPerValue * ValuesInChunk / 8], uint8_t valueIndex)
    {
        if(valueIndex >= ValuesInChunk)
            return false;

        uint8_t bitsUsed = valueIndex * BitsPerValue;
        uint8_t writtenBits = 0;

        while (writtenBits < BitsPerValue)
        {
            uint8_t byteOffset = bitsUsed / 8;
            uint8_t bitOffset = bitsUsed % 8;
            uint8_t bitCount = WB_MIN(8 - bitOffset, BitsPerValue - writtenBits);
            uint8_t bitsLeft = BitsPerValue - writtenBits;

            uint8_t value = sample >> (bitsLeft - bitCount);
            uint8_t* out = buffer + byteOffset;
            uint8_t mask = (0xFF << (8 - bitOffset));
            uint8_t bits = (value << (8 - (bitCount + bitOffset)));
            *out = (*out & mask) | (bits & ~mask);

            bitsUsed += bitCount;
            writtenBits += bitCount;
        }

        return true;
    }
} // namespace bit_pack
