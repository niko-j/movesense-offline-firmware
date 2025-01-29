#include "samples.hpp"
#include "utils.hpp"
#include "sbem_loader.hpp"

constexpr const char* CSV_DELIMITER = ",";

Samples::Samples(const SbemDocument& sbem)
{
    for (const auto& chunk : sbem.getChunks())
    {
        auto* desc = sbem_loader::get_first_item_descriptor(sbem, chunk);
        if (!desc)
        {
            printf("Missing descriptor for chunk type %u\n", chunk.header.id);
            continue;
        }
        auto measurement = desc->getName().value_or("");

        if (measurement.find("OfflineMeasAcc") != std::string::npos)
        {
            OfflineAccData data;
            if (chunk.tryRead(data))
                acc.push_back(data);
        }
        else if (measurement.find("OfflineMeasGyro") != std::string::npos)
        {
            OfflineGyroData data;
            if (chunk.tryRead(data))
                gyro.push_back(data);
        }
        else if (measurement.find("OfflineMeasMagn") != std::string::npos)
        {
            OfflineMagnData data;
            if (chunk.tryRead(data))
                magn.push_back(data);
        }
        else if (measurement.find("OfflineMeasHR") != std::string::npos)
        {
            OfflineHRData data;
            if (chunk.tryRead(data))
                hr.push_back(data);
        }
        else if (measurement.find("OfflineMeasRR") != std::string::npos)
        {
            OfflineRRData data;
            if (chunk.tryRead(data))
                rr.push_back(data);
        }
        else if (measurement.find("OfflineMeasECG") != std::string::npos)
        {
            OfflineECGData data;
            if (chunk.tryRead(data))
                ecg.push_back(data);
        }
        else if (measurement.find("OfflineMeasECGCompressed") != std::string::npos)
        {
            // TODO
        }
        else if (measurement.find("OfflineMeasTemp") != std::string::npos)
        {
            OfflineTempData data;
            if (chunk.tryRead(data))
                temp.push_back(data);
        }
        else if (measurement.find("OfflineMeasActivity") != std::string::npos)
        {
            OfflineActivityData data;
            if (chunk.tryRead(data))
                activity.push_back(data);
        }
        else if (measurement.find("OfflineMeasTap") != std::string::npos)
        {
            OfflineTapData data;
            if (chunk.tryRead(data))
                taps.push_back(data);
        }
        else
        {
            printf("Unknown measurement: %s\n", measurement.c_str());
        }
    }
}

std::ostream& Samples::writeAccSamplesCSV(std::ostream& out) const
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "X" << CSV_DELIMITER
        << "Y" << CSV_DELIMITER
        << "Z" << std::endl;

    // Figure out sample rate
    uint16_t samplerate = 0;
    uint16_t interval = 0;
    if (acc.size() > 1)
    {
        const auto& a = acc.at(0);
        const auto& b = acc.at(1);
        double diff = utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = utils::calculateSampleRate(diff);
    }

    // Data rows
    for (size_t i = 0; i < acc.size(); i++)
    {
        const auto& entry = acc[i];
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

std::ostream& Samples::writeGyroSamplesCSV(std::ostream& out) const
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "X" << CSV_DELIMITER
        << "Y" << CSV_DELIMITER
        << "Z" << std::endl;

    // Figure out sample rate
    uint16_t samplerate = 0;
    uint16_t interval = 0;
    if (gyro.size() > 1)
    {
        const auto& a = gyro.at(0);
        const auto& b = gyro.at(1);

        double diff = utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = utils::calculateSampleRate(diff);
    }

    // Data rows
    for (size_t i = 0; i < gyro.size(); i++)
    {
        const auto& entry = gyro[i];
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

std::ostream& Samples::writeMagnSamplesCSV(std::ostream& out) const
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "X" << CSV_DELIMITER
        << "Y" << CSV_DELIMITER
        << "Z" << std::endl;

    // Figure out sample rate
    uint16_t samplerate = 0;
    uint16_t interval = 0;
    if (magn.size() > 1)
    {
        const auto& a = magn.at(0);
        const auto& b = magn.at(1);

        double diff = utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = utils::calculateSampleRate(diff);
    }

    // Data rows
    for (size_t i = 0; i < magn.size(); i++)
    {
        const auto& entry = magn[i];
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

std::ostream& Samples::writeHRSamplesCSV(std::ostream& out) const
{
    // Title row
    out << "Average" <<  std::endl;

    // Data rows
    for (size_t i = 0; i < hr.size(); i++)
    {
        out << (int) hr[i].average << std::endl;
    }

    return out;
}

std::ostream& Samples::writeRRSamplesCSV(std::ostream& out) const
{
    // Title row
    out << "RR" << std::endl;

    // Data rows
    for (size_t i = 0; i < rr.size(); i++)
    {
        auto values = rr[i].unpack();
        for (size_t j = 0; j < values.size(); j++)
        {
            out << values[j] << std::endl;
        }
    }

    return out;
}

std::ostream& Samples::writeECGSamplesCSV(std::ostream& out) const
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "mV" << std::endl;

    // Figure out sample rate
    uint16_t samplerate = 0;
    uint16_t interval = 0;
    if (ecg.size() > 1)
    {
        const auto& a = ecg.at(0);
        const auto& b = ecg.at(1);

        double diff = utils::calculateSampleInterval(a.timestamp, a.sampleData.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = utils::calculateSampleRate(diff);
    }

    // Data rows
    for (size_t i = 0; i < ecg.size(); i++)
    {
        const auto& entry = ecg[i];
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

std::ostream& Samples::writeTempSamplesCSV(std::ostream& out) const
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "TempC" << std::endl;

    // Data rows
    for (size_t i = 0; i < temp.size(); i++)
    {
        out
            << temp[i].timestamp << CSV_DELIMITER
            << (int) temp[i].measurement << std::endl;
    }

    return out;
}

std::ostream& Samples::writeActivitySamplesCSV(std::ostream& out) const
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "Activity" << std::endl;

    // Data rows
    for (size_t i = 0; i < activity.size(); i++)
    {
        out
            << activity[i].timestamp << CSV_DELIMITER
            << activity[i].activity.toFloat() << std::endl;
    }

    return out;
}

std::ostream& Samples::writeTapDetectionSamplesCSV(std::ostream& out) const
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "Count" << std::endl;

    // Data rows
    for (size_t i = 0; i < taps.size(); i++)
    {
        out
            << taps[i].timestamp << CSV_DELIMITER
            << (int) taps[i].count << std::endl;
    }

    return out;
}
