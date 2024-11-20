#include "sbem.hpp"

SbemDocument::SbemDocument()
    : fileSize(0)
    , header({})
{
}

SbemDocument::~SbemDocument()
{
    if (fileStream.is_open())
        fileStream.close();
}

SbemDocument::ParseResult SbemDocument::parse(const char* filePath)
{
    fileStream.open(filePath, std::ios::binary | std::ios::end);
    if (!fileStream.is_open())
        return ParseResult::FailedToOpen;

    fileSize = fileStream.tellg();
    fileStream.seekg(0, std::ios::beg);

    ParseResult result = ParseResult::Success;
    {
        auto documentHeader = readHeader();
        if (!documentHeader.has_value())
        {
            result = ParseResult::CorruptedFile;
            goto close_and_return;
        }
        header = documentHeader.value();

        if (strncmp(header.bytes.data(), "SBEM", 4) != 0)
        {
            result = ParseResult::InvalidHeaderMagic;
            goto close_and_return;
        }

        while ((size_t)fileStream.tellg() < fileSize)
        {
            auto chunkHeader = readChunkHeader();
            if (!chunkHeader.has_value())
                break;

            SbemChunk chunk = {
                .header = chunkHeader.value(),
                .bytes = {}
            };

            chunk.bytes.resize(chunk.header.dataSize);
            if ((size_t)fileStream.tellg() + chunk.header.dataSize > fileSize)
                break;

            fileStream.read(chunk.bytes.data(), chunk.header.dataSize);

            if (chunk.header.id == SBEM_ID_DESCRIPTOR)
            {
                SbemDescriptor desc(chunk);
                descriptors[desc.getId()] = desc;
            }
            else
            {
                chunks.push_back(chunk);
            }
        }
    }

close_and_return:
    result = ((size_t)fileStream.tellg() != fileSize)
        ? ParseResult::CorruptedFile
        : ParseResult::Success;
    fileStream.close();
    return result;
}

const SbemHeader& SbemDocument::getHeader() const
{
    return header;
}

const std::map<SbemId, SbemDescriptor>& SbemDocument::getDescriptors() const
{
    return descriptors;
}

const SbemDescriptor* SbemDocument::getDescriptor(SbemId id) const
{
    if (descriptors.count(id))
        return &descriptors.at(id);
    return nullptr;
}

const std::vector<SbemChunk>& SbemDocument::getChunks() const
{
    return chunks;
}

std::optional<uint16_t> SbemDocument::readId()
{
    if ((size_t)fileStream.tellg() + 1 > fileSize)
        return std::optional<uint16_t>();

    uint8_t buffer[2] = {};
    fileStream.read((char*)buffer, 1);

    if (buffer[0] < SBEM_ID_ESCAPE)
    {
        return static_cast<uint16_t>(buffer[0]);
    }
    else
    {
        if ((size_t)fileStream.tellg() + 2 > fileSize)
            return std::optional<uint16_t>();

        fileStream.read((char*)buffer, 2);
        return *reinterpret_cast<uint16_t*>(buffer);
    }
}

std::optional<uint32_t> SbemDocument::readLen()
{
    if ((size_t)fileStream.tellg() + 1 > fileSize)
        return std::optional<uint32_t>();

    uint8_t buffer[4] = {};
    fileStream.read((char*)buffer, 1);

    if (buffer[0] < SBEM_ID_ESCAPE)
    {
        return static_cast<uint32_t>(buffer[0]);
    }
    else
    {
        if ((size_t)fileStream.tellg() + 4 > fileSize)
            return std::optional<uint32_t>();

        fileStream.read((char*)buffer, 4);
        return *reinterpret_cast<uint32_t*>(buffer);
    }
}

std::optional<SbemHeader> SbemDocument::readHeader()
{
    if ((size_t)fileStream.tellg() + 8 <= fileSize)
    {
        SbemHeader header;
        header.bytes.resize(8);
        fileStream.read(header.bytes.data(), 8);
        return header;
    }
    return std::optional<SbemHeader>();
}

std::optional<SbemChunkHeader> SbemDocument::readChunkHeader()
{
    auto id = readId();
    auto size = readLen();

    if (id.has_value() && size.has_value())
    {
        SbemChunkHeader header = {
            .id = id.value(),
            .dataSize = size.value()
        };
        return header;
    }

    return std::optional<SbemChunkHeader>();
}

SbemId SbemDescriptor::getId() const
{
    if (bytes.size() > 1)
        return *reinterpret_cast<const SbemId*>(bytes.data());
    return (SbemId)SBEM_ID_ESCAPE;
}

std::optional<std::string> SbemDescriptor::getName() const
{
    auto str = std::string(bytes.begin(), bytes.end());
    auto beg = str.find("<PTH>");
    auto newline = str.find_last_of('\n');
    auto null = str.find_last_of('\0');

    if (beg != std::string::npos)
    {
        if (newline != std::string::npos)
            return std::string(str.begin() + beg + 5, str.begin() + newline);
        else if (null != std::string::npos)
            return std::string(str.begin() + beg + 5, str.begin() + null);
    }

    return std::optional<std::string>();
}

std::optional<std::string> SbemDescriptor::getFormat() const
{
    auto str = std::string(bytes.begin(), bytes.end());
    auto beg = str.find("<FRM>");
    auto end = str.find_last_of('\0');
    if (beg != std::string::npos && end != std::string::npos)
        return std::string(str.begin() + beg + 5, str.begin() + end);
    return std::optional<std::string>();
}

std::optional<std::vector<SbemId>> SbemDescriptor::getGroup() const
{
    auto str = std::string(bytes.begin(), bytes.end());
    auto beg = str.find("<GRP>");
    auto end = str.find_last_of('\0');
    if (beg != std::string::npos && end != std::string::npos)
    {
        auto list = std::string(str.begin() + beg + 5, str.begin() + end);
        std::vector<SbemId> ids = {};
        while (!list.empty())
        {
            size_t limit = list.find(',');
            std::string sub = "";

            if (limit != std::string::npos)
            {
                sub = list.substr(0, limit);
                list = list.erase(0, limit + 1);
            }
            else
            {
                sub = list; list = "";
            }

            try
            {
                SbemId id = std::stoi(sub);
                ids.push_back(id);
            }
            catch (const std::exception& e)
            {
                return std::optional<std::vector<SbemId>>();
            }
        }
        return ids;
    }

    return std::optional<std::vector<SbemId>>();
}
