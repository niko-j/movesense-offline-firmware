#include "utils.hpp"

constexpr const char* CSV_DELIMITER = ",";

void utils::printHeader(const SbemDocument& sbem)
{
    const auto& header = sbem.getHeader();
    std::string headerString(header.bytes.begin(), header.bytes.end());
    std::cout << "Header(" << headerString << ")\n";
}

void utils::printDescriptors(const SbemDocument& sbem)
{
    std::cout << "Descriptors {\n";
    for (const auto& desc : sbem.getDescriptors())
    {
        auto name = desc.second.getName();
        auto type = desc.second.getFormat();
        auto group = desc.second.getGroup();

        if (name.has_value())
        {
            std::cout
                << " ID(" << desc.first
                << ") Name(" << name.value()
                << ") Type(" << type.value_or("")
                << ")\n";
        }
        else if (group.has_value())
        {
            std::cout
                << " ID(" << desc.first
                << ") Group(";

            bool first = true;
            for (auto item : group.value())
            {
                if (!first)
                    std::cout << ", ";
                else
                    first = false;

                std::cout << item;
            }
            std::cout << ")\n";
        }
        else
        {
            std::cout
                << " Unknown descriptor ID("
                << desc.first << ") Data(";
            bool first = true;
            for (auto item : desc.second.bytes)
            {
                if (!first)
                    std::cout << " ";
                else
                    first = false;

                std::cout << item;
            }
            std::cout << ")\n";
        }
    }

    std::cout << "}\n";
}

void utils::printDataChunks(const SbemDocument& sbem)
{
    std::cout << "Data {\n";
    std::map<SbemId, std::vector<const SbemChunk*>> ChunksById;
    for (const auto& chunk : sbem.getChunks())
    {
        if (!ChunksById.count(chunk.header.id))
            ChunksById[chunk.header.id] = {};

        ChunksById[chunk.header.id].push_back(&chunk);
    }

    for (const auto& chunks : ChunksById)
    {
        std::cout
            << " Chunk ID(" << chunks.first
            << ") Count(" << chunks.second.size()
            << ")\n";

        for (const auto& chunk : chunks.second)
        {
            std::cout << "  Data [";
            for (const auto b : chunk->bytes)
                printf(" %02x ", uint8_t(b));
            std::cout << "]\n";
        }
    }
    std::cout << "}\n";
}


template<typename T>
void printImuSamples(const std::string& name, const std::vector<T>& samples)
{
    uint16_t samplerate = 0;
    if (samples.size() > 1)
    {
        const auto& a = samples.at(0);
        const auto& b = samples.at(1);
        double interval = utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        samplerate = utils::calculateSampleRate(interval);
    }

    std::cout << name << " SampleRate(" << samplerate << " Hz) {\n";
    for (const auto& sample : samples)
    {
        std::cout << " @" << sample.timestamp << " [ ";
        for (const auto& v : sample.measurements)
        {
            std::cout
                << "{ X(" << v.x.toFloat()
                << ") Y(" << v.y.toFloat()
                << ") Z(" << v.z.toFloat()
                << ") } ";
        }
        std::cout << "]\n";
    }
    std::cout << "}\n";
}

void utils::printAccSamples(const Samples& samples)
{
    printImuSamples("Acc", samples.acc);
}

void utils::printGyroSamples(const Samples& samples)
{
    printImuSamples("Gyro", samples.gyro);
}

void utils::printMagnSamples(const Samples& samples)
{
    printImuSamples("Magn", samples.magn);
}

void utils::printHRSamples(const Samples& samples)
{
    std::cout << "HR {\n";
    for (const auto& sample : samples.hr)
    {
        std::cout
            << "Average(" << (int) sample.average
            << ") RR[ ";
        for (const auto& rr : sample.rrValues)
        {
            std::cout << rr << " ";
        }
        std::cout << "]\n";
    }
    std::cout << "}\n";
}

void utils::printECGSamples(const Samples& samples)
{
    uint16_t samplerate = 0;
    if (samples.ecg.size() > 1)
    {
        const auto& a = samples.ecg.at(0);
        const auto& b = samples.ecg.at(1);
        double interval = calculateSampleInterval(a.timestamp, a.sampleData.size(), b.timestamp);
        samplerate = calculateSampleRate(interval);
    }

    std::cout << "ECG SampleRate(" << samplerate << " Hz) {\n";
    for (const auto& sample : samples.ecg)
    {
        std::cout << " @" << sample.timestamp << " [ ";
        for (const auto& v : sample.sampleData)
        {
            std::cout << v << " ";
        }
        std::cout << "]\n";
    }
    std::cout << "}\n";
}

void utils::printTempSamples(const Samples& samples)
{
    std::cout << "Temp {\n";
    for (const auto& sample : samples.temp)
    {
        std::cout << " @" << sample.timestamp << " Value(" << (int) sample.measurement << ")\n";
    }
    std::cout << "}\n";
}

void utils::printActivitySamples(const Samples& samples)
{
    std::cout << "Activity {\n";
    for (const auto& sample : samples.activity)
    {
        std::cout << " @" << sample.timestamp << " Value(" << sample.activity.toFloat() << ")\n";
    }
    std::cout << "}\n";
}

void utils::printTapDetectionSamples(const Samples& samples)
{
    std::cout << "TapDetection {\n";
    for (const auto& sample : samples.taps)
    {
        std::cout << " @" << sample.timestamp 
            << " Magnitude(" << sample.magnitude.toFloat()
            << " Count(" << sample.count << ")\n";
    }
    std::cout << "}\n";
}

std::ostream& utils::printAccSamplesCSV(const Samples& samples, std::ostream& out)
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
        double diff = utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = utils::calculateSampleRate(diff);
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

std::ostream& utils::printGyroSamplesCSV(const Samples& samples, std::ostream& out)
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

        double diff = utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = utils::calculateSampleRate(diff);
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

std::ostream& utils::printMagnSamplesCSV(const Samples& samples, std::ostream& out)
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

        double diff = utils::calculateSampleInterval(a.timestamp, a.measurements.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = utils::calculateSampleRate(diff);
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

std::ostream& utils::printHRSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Average" << CSV_DELIMITER
        << "RR" << std::endl;

    // Data rows
    for (size_t i = 0; i < samples.hr.size(); i++)
    {
        const auto& entry = samples.hr[i];
        for (size_t j = 0; j < entry.rrValues.size(); j++)
        {
            out
                << (int) entry.average << CSV_DELIMITER
                << entry.rrValues[j] << std::endl;
        }
    }

    return out;
}

std::ostream& utils::printECGSamplesCSV(const Samples& samples, std::ostream& out)
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

        double diff = utils::calculateSampleInterval(a.timestamp, a.sampleData.size(), b.timestamp);
        interval = static_cast<uint16_t>(round(diff));
        samplerate = utils::calculateSampleRate(diff);
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

std::ostream& utils::printTempSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "TempC" << std::endl;

    // Data rows
    for (size_t i = 0; i < samples.temp.size(); i++)
    {
        out
            << samples.temp[i].timestamp << CSV_DELIMITER
            << (int) samples.temp[i].measurement << std::endl;
    }

    return out;
}

std::ostream& utils::printActivitySamplesCSV(const Samples& samples, std::ostream& out)
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

std::ostream& utils::printTapDetectionSamplesCSV(const Samples& samples, std::ostream& out)
{
    // Title row
    out << "Timestamp" << CSV_DELIMITER
        << "Magnitude" << CSV_DELIMITER
        << "Count" << std::endl;

    // Data rows
    for (size_t i = 0; i < samples.taps.size(); i++)
    {
        out
            << samples.taps[i].timestamp << CSV_DELIMITER
            << samples.taps[i].magnitude.toFloat() << CSV_DELIMITER
            << (int) samples.taps[i].count << std::endl;
    }

    return out;
}
