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
}
