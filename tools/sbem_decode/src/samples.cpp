#include "samples.hpp"
#include "utils.hpp"
#include <iostream>

constexpr const char* CSV_DELIMITER = ",";

const SbemDescriptor* getFirstItemName(const SbemDocument& sbem, const SbemChunk& chunk)
{
    const SbemDescriptor* desc = sbem.getDescriptor(chunk.header.id);
    if (!desc)
        return nullptr;

    auto name = desc->getName();
    while (desc && !name.has_value())
    {
        auto group = desc->getGroup();
        if (group.has_value())
        {
            desc = sbem.getDescriptor(group->at(0));
            if (desc)
            {
                name = desc->getName();
            }
        }
    }

    if (!name.has_value())
        return nullptr;

    return desc;
}

Samples::Samples(const SbemDocument& sbem)
{
    for (const auto& chunk : sbem.getChunks())
    {
        auto* desc = getFirstItemName(sbem, chunk);
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
        else if (measurement.find("OfflineMeasECG") != std::string::npos)
        {
            OfflineECGData data;
            if (chunk.tryRead(data))
                ecg.push_back(data);
        }
        else if (measurement.find("OfflineMeasTemp") != std::string::npos)
        {
            OfflineTempData data;
            if (chunk.tryRead(data))
                temp.push_back(data);
        }
        else
        {
            printf("Unknown measurement: %s\n", measurement.c_str());
        }
    }
}

bool Samples::exportAccSamples(const std::string& filepath)
{
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        printf("Failed to open file '%s' for writing.", filepath.c_str());
        return false;
    }

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

    out.close();
    return true;
}

bool Samples::exportGyroSamples(const std::string& filepath)
{
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        printf("Failed to open file '%s' for writing.\n", filepath.c_str());
        return false;
    }

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

    out.close();
    return true;
}

bool Samples::exportMagnSamples(const std::string& filepath)
{
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        printf("Failed to open file '%s' for writing.", filepath.c_str());
        return false;
    }

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

    out.close();
    return true;
}

bool Samples::exportHRSamples(const std::string& filepath)
{
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        printf("Failed to open file '%s' for writing.", filepath.c_str());
        return false;
    }

    // Title row
    out << "Average" << CSV_DELIMITER
        << "RR" << std::endl;

    // Data rows
    for (size_t i = 0; i < hr.size(); i++)
    {
        const auto& entry = hr[i];
        for (size_t j = 0; j < entry.rrValues.size(); j++)
        {
            out
                << fixed_point_to_float<uint16_t, 8>(entry.average) << CSV_DELIMITER
                << entry.rrValues[j] << std::endl;
        }
    }

    out.close();
    return true;
}

bool Samples::exportECGSamples(const std::string& filepath)
{
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        printf("Failed to open file '%s' for writing.", filepath.c_str());
        return false;
    }

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
                << meas.toFloat() << std::endl;
        }
    }

    out.close();
    return true;
}

bool Samples::exportTempSamples(const std::string& filepath)
{
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        printf("Failed to open file '%s' for writing.", filepath.c_str());
        return false;
    }

    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "TempC" << std::endl;

    // Data rows
    for (size_t i = 0; i < temp.size(); i++)
    {
        out
            << temp[i].timestamp << CSV_DELIMITER
            << temp[i].measurement << std::endl;
    }

    out.close();
    return true;
}
