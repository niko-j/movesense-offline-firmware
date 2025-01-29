#include "sbem_utils.hpp"
#include <map>
#include <vector>
#include <string>

void sbem_utils::printHeader(const SbemDocument& sbem, std::ostream& out)
{
    const auto& header = sbem.getHeader();
    std::string headerString(header.bytes.begin(), header.bytes.end());
    out << "Header(" << headerString << ")\n";
}

void sbem_utils::printDescriptors(const SbemDocument& sbem, std::ostream& out)
{
    out << "Descriptors {\n";
    for (const auto& desc : sbem.getDescriptors())
    {
        auto name = desc.second.getName();
        auto type = desc.second.getFormat();
        auto group = desc.second.getGroup();

        if (name.has_value())
        {
            out
                << " ID(" << desc.first
                << ") Name(" << name.value()
                << ") Type(" << type.value_or("")
                << ")\n";
        }
        else if (group.has_value())
        {
            out
                << " ID(" << desc.first
                << ") Group(";

            bool first = true;
            for (auto item : group.value())
            {
                if (!first)
                    out << ", ";
                else
                    first = false;

                out << item;
            }
            out << ")\n";
        }
        else
        {
            out
                << " Unknown descriptor ID("
                << desc.first << ") Data(";
            bool first = true;
            for (auto item : desc.second.bytes)
            {
                if (!first)
                    out << " ";
                else
                    first = false;

                out << item;
            }
            out << ")\n";
        }
    }

    out << "}\n";
}

void sbem_utils::printDataChunks(const SbemDocument& sbem, std::ostream& out)
{
    out << "Data {\n";
    std::map<SbemId, std::vector<const SbemChunk*>> ChunksById;
    for (const auto& chunk : sbem.getChunks())
    {
        if (!ChunksById.count(chunk.header.id))
            ChunksById[chunk.header.id] = {};

        ChunksById[chunk.header.id].push_back(&chunk);
    }

    for (const auto& chunks : ChunksById)
    {
        out
            << " Chunk ID(" << chunks.first
            << ") Count(" << chunks.second.size()
            << ")\n";

        for (const auto& chunk : chunks.second)
        {
            out << "  Data [";
            for (const auto b : chunk->bytes)
                printf(" %02x ", uint8_t(b));
            out << "]\n";
        }
    }
    out << "}\n";
}