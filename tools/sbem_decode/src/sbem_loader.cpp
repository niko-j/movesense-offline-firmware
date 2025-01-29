#include "sbem_loader.hpp"
#include <filesystem>

#define SBEM_IMPL
#include "sbem.hpp"

bool sbem_loader::load_sbem(const std::string& path, SbemDocument& sbem)
{
    std::filesystem::path filepath(path);
    auto parseResult = sbem.parse(filepath.c_str());
    return (parseResult == SbemDocument::ParseResult::Success);
}

size_t sbem_loader::read_chunks(const SbemDocument& sbem, const std::string& measurement, std::function<void(const SbemChunk&)> parser)
{
    size_t chunkCount = 0;
    for (const auto& chunk : sbem.getChunks())
    {
        auto* desc = get_first_item_descriptor(sbem, chunk);
        if (!desc)
            continue;

        auto name = desc->getName().value_or("");
        if (name.find(measurement) == std::string::npos)
            continue;

        // Read chunk
        chunkCount++;
        parser(chunk);
    }

    return chunkCount;
}

const SbemDescriptor* sbem_loader::get_first_item_descriptor(const SbemDocument& sbem, const SbemChunk& chunk)
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