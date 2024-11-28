#pragma once
#include <iostream>
#include <string>
#include <vector>

#include "sbem/sbem.hpp"
#include "samples.hpp"

namespace utils
{
    void printHeader(const SbemDocument& sbem);
    void printDescriptors(const SbemDocument& sbem);
    void printDataChunks(const SbemDocument& sbem);

    void printAccSamples(const Samples& samples);
    void printGyroSamples(const Samples& samples);
    void printMagnSamples(const Samples& samples);
    void printHRSamples(const Samples& samples);
    void printECGSamples(const Samples& samples);
    void printTempSamples(const Samples& samples);

    std::ostream& printAccSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printGyroSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printMagnSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printHRSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printECGSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& printTempSamplesCSV(const Samples& samples, std::ostream& out);

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
}
