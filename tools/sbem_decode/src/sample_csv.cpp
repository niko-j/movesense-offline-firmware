#include "sample_csv.hpp"
#include "sample_utils.hpp"

#ifndef CSV_DELIMITER
#define CSV_DELIMITER ","
#endif

std::ostream& sample_csv::writeAccSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "X" << CSV_DELIMITER
        << "Y" << CSV_DELIMITER
        << "Z" << std::endl;

    // Figure out sample rate
    uint16_t samplerate = 0;
    uint16_t interval = 0;
    if (samples.acc.size() > 1)
    {
        const auto& a = samples.acc.at(0);
        const auto& b = samples.acc.at(1);
        double diff = sample_utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = sample_utils::calculateSampleRate(diff);
    }

    // Data rows
    for (size_t i = 0; i < samples.acc.size(); i++)
    {
        const auto& entry = samples.acc[i];
        for (size_t j = 0; j < entry.measurements.size(); j++)
        {
            const auto& meas = entry.measurements[j];
            out
                << entry.timestamp + interval * j << CSV_DELIMITER
                << meas.x.toFloat() << CSV_DELIMITER
                << meas.y.toFloat() << CSV_DELIMITER
                << meas.z.toFloat() << std::endl;
        }
    }

    return out;
}

std::ostream& sample_csv::writeGyroSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "X" << CSV_DELIMITER
        << "Y" << CSV_DELIMITER
        << "Z" << std::endl;

    // Figure out sample rate
    uint16_t samplerate = 0;
    uint16_t interval = 0;
    if (samples.gyro.size() > 1)
    {
        const auto& a = samples.gyro.at(0);
        const auto& b = samples.gyro.at(1);

        double diff = sample_utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = sample_utils::calculateSampleRate(diff);
    }

    // Data rows
    for (size_t i = 0; i < samples.gyro.size(); i++)
    {
        const auto& entry = samples.gyro[i];
        for (size_t j = 0; j < entry.measurements.size(); j++)
        {
            const auto& meas = entry.measurements[j];
            out
                << entry.timestamp + interval * j << CSV_DELIMITER
                << meas.x.toFloat() << CSV_DELIMITER
                << meas.y.toFloat() << CSV_DELIMITER
                << meas.z.toFloat() << std::endl;
        }
    }

    return out;
}

std::ostream& sample_csv::writeMagnSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "X" << CSV_DELIMITER
        << "Y" << CSV_DELIMITER
        << "Z" << std::endl;

    // Figure out sample rate
    uint16_t samplerate = 0;
    uint16_t interval = 0;
    if (samples.magn.size() > 1)
    {
        const auto& a = samples.magn.at(0);
        const auto& b = samples.magn.at(1);

        double diff = sample_utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = sample_utils::calculateSampleRate(diff);
    }

    // Data rows
    for (size_t i = 0; i < samples.magn.size(); i++)
    {
        const auto& entry = samples.magn[i];
        for (size_t j = 0; j < entry.measurements.size(); j++)
        {
            const auto& meas = entry.measurements[j];
            out
                << entry.timestamp + interval * j << CSV_DELIMITER
                << meas.x.toFloat() << CSV_DELIMITER
                << meas.y.toFloat() << CSV_DELIMITER
                << meas.z.toFloat() << std::endl;
        }
    }

    return out;
}

std::ostream& sample_csv::writeHRSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Average" << std::endl;

    // Data rows
    for (size_t i = 0; i < samples.hr.size(); i++)
    {
        out << (int)samples.hr[i].average << std::endl;
    }

    return out;
}

std::ostream& sample_csv::writeRRSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "RR" << std::endl;

    // Data rows
    for (size_t i = 0; i < samples.rr.size(); i++)
    {
        auto values = samples.rr[i].unpack();
        for (size_t j = 0; j < values.size(); j++)
        {
            out << values[j] << std::endl;
        }
    }

    return out;
}

std::ostream& sample_csv::writeECGSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "mV" << std::endl;

    // Figure out sample rate
    uint16_t samplerate = 0;
    uint16_t interval = 0;
    if (samples.ecg.size() > 1)
    {
        const auto& a = samples.ecg.at(0);
        const auto& b = samples.ecg.at(1);

        double diff = sample_utils::calculateSampleInterval(a.timestamp, a.sampleData.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = sample_utils::calculateSampleRate(diff);
    }

    // Data rows
    for (size_t i = 0; i < samples.ecg.size(); i++)
    {
        const auto& entry = samples.ecg[i];
        for (size_t j = 0; j < entry.sampleData.size(); j++)
        {
            const auto& meas = entry.sampleData[j];
            out
                << entry.timestamp + interval * j << CSV_DELIMITER
                << meas << std::endl;
        }
    }

    return out;
}

std::ostream& sample_csv::writeTempSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "TempC" << std::endl;

    // Data rows
    for (size_t i = 0; i < samples.temp.size(); i++)
    {
        out
            << samples.temp[i].timestamp << CSV_DELIMITER
            << (int)samples.temp[i].measurement << std::endl;
    }

    return out;
}

std::ostream& sample_csv::writeActivitySamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "Activity" << std::endl;

    // Data rows
    for (size_t i = 0; i < samples.activity.size(); i++)
    {
        out
            << samples.activity[i].timestamp << CSV_DELIMITER
            << samples.activity[i].activity.toFloat() << std::endl;
    }

    return out;
}

std::ostream& sample_csv::writeTapDetectionSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "Count" << std::endl;

    // Data rows
    for (size_t i = 0; i < samples.taps.size(); i++)
    {
        out
            << samples.taps[i].timestamp << CSV_DELIMITER
            << (int)samples.taps[i].count << std::endl;
    }

    return out;
}
