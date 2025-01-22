#pragma once
#include <iostream>
#include <string>
#include <vector>

#include "sbem.hpp"
#include "samples.hpp"

namespace utils
{
    void printHeader(const SbemDocument& sbem);
    void printDescriptors(const SbemDocument& sbem);
    void printDataChunks(const SbemDocument& sbem);

    void printAccSamples(const Samples& samples);
    void printGyroSamples(const Samples& samples);
    void printMagnSamples(const Samples& samples);
    void printRRSamples(const Samples& samples);
    void printHRSamples(const Samples& samples);
    void printECGSamples(const Samples& samples);
    void printTempSamples(const Samples& samples);
    void printActivitySamples(const Samples& samples);
    void printTapDetectionSamples(const Samples& samples);

    std::ostream& printAccSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printGyroSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printMagnSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printRRSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printHRSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printECGSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printTempSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printActivitySamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printTapDetectionSamplesCSV(const Samples& samples, std::ostream& out);

    inline double calculateSampleInterval(OfflineTimestamp timestamp, size_t sampleCount, OfflineTimestamp next)
    {
        OfflineTimestamp diff = next - timestamp;
        double msPerSample = diff / sampleCount;
        return msPerSample;
    }

    inline uint16_t calculateSampleRate(double interval_ms)
    {
        return static_cast<uint16_t>(round(1000.0 / interval_ms));
    }

    template<typename T, uint8_t N_BITS>
    std::vector<T> bitpack_unpack(const std::vector<uint8_t>& data)
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
}
