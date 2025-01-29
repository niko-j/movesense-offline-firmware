#pragma once

#include <iostream>
#include <vector>
#include "offline_measurements.hpp"

struct Samples
{
    std::vector<OfflineAccData> acc;
    std::vector<OfflineGyroData> gyro;
    std::vector<OfflineMagnData> magn;
    std::vector<OfflineHRData> hr;
    std::vector<OfflineRRData> rr;
    std::vector<OfflineECGData> ecg;
    std::vector<OfflineTempData> temp;
    std::vector<OfflineActivityData> activity;
    std::vector<OfflineTapData> taps;

    Samples(const SbemDocument& sbem);

    std::ostream& writeAccSamplesCSV(std::ostream& out) const;
    std::ostream& writeGyroSamplesCSV(std::ostream& out) const;
    std::ostream& writeMagnSamplesCSV(std::ostream& out) const;
    std::ostream& writeRRSamplesCSV(std::ostream& out) const;
    std::ostream& writeHRSamplesCSV(std::ostream& out) const;
    std::ostream& writeECGSamplesCSV(std::ostream& out) const;
    std::ostream& writeTempSamplesCSV(std::ostream& out) const;
    std::ostream& writeActivitySamplesCSV(std::ostream& out) const;
    std::ostream& writeTapDetectionSamplesCSV(std::ostream& out) const;
};
