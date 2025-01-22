#include "samples.hpp"

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
            for(auto i = 0; i < group->size(); i++)
            {
                desc = sbem.getDescriptor(group->at(i));
                if (desc)
                {
                    name = desc->getName();
                    if(name.has_value() && name.value() != "[")
                    {
                        return desc;
                    }
                }
            }
        }
    }

    return nullptr;
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
