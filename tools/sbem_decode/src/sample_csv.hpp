#pragma once
#include "samples.hpp"
#include <iostream>

namespace sample_csv
{
    std::ostream& writeAccSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& writeGyroSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& writeMagnSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& writeRRSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& writeHRSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& writeECGSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& writeTempSamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& writeActivitySamplesCSV(const Samples& samples, std::ostream& out);
    std::ostream& writeTapDetectionSamplesCSV(const Samples& samples, std::ostream& out);
} // namespace sample_csv
