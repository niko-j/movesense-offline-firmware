#include "utils.hpp"
#include "samples.hpp"

void utils::printHeader(const SbemDocument& sbem)
{
    const auto& header = sbem.getHeader();
    std::string headerString(header.bytes.begin(), header.bytes.end());
    std::cout << "Header(" << headerString << ")\n";
}

void utils::printDescriptors(const SbemDocument& sbem)
{
    std::cout << "Descriptors (\n";
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

    std::cout << ")\n";
}

void utils::printDataChunks(const SbemDocument& sbem)
{
    std::cout << "Data(\n";
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
    std::cout << ")\n";
}


template<typename T>
void printImuSamples(const std::string& name, const std::vector<T>& samples)
{
    std::cout << name << "(\n";
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
    std::cout << ")\n";
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
    std::cout << "HR(\n";
    for (const auto& sample : samples.hr)
    {
        std::cout
            << "Average(" << fixed_point_to_float<uint16_t, 8>(sample.average)
            << ") RR[ ";
        for (const auto& rr : sample.rrValues)
        {
            std::cout << rr << " ";
        }
        std::cout << "]\n";
    }
    std::cout << ")\n";
}

void utils::printECGSamples(const Samples& samples)
{
    std::cout << "ECG(\n";
    for (const auto& sample : samples.ecg)
    {
        std::cout << " @" << sample.timestamp << " [ ";
        for (const auto& v : sample.sampleData)
        {
            std::cout << v.toFloat() << " ";
        }
        std::cout << "]\n";
    }
    std::cout << ")\n";
}

void utils::printTempSamples(const Samples& samples)
{
    std::cout << "Temp(\n";
    for (const auto& sample : samples.temp)
    {
        std::cout << " @" << sample.timestamp << " Value(" << sample.measurement << ")\n";
    }
    std::cout << ")\n";
}
