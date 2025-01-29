#pragma once
#include <iostream>
#include <string>
#include <vector>

#include "sbem.hpp"
#include "samples.hpp"

namespace utils
{
    void printAccSamples(const Samples& samples);
    void printGyroSamples(const Samples& samples);
    void printMagnSamples(const Samples& samples);
    void printRRSamples(const Samples& samples);
    void printHRSamples(const Samples& samples);
    void printECGSamples(const Samples& samples);
    void printTempSamples(const Samples& samples);
    void printActivitySamples(const Samples& samples);
    void printTapDetectionSamples(const Samples& samples);

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
