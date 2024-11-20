#ifndef __SBEM_HPP__
#define __SBEM_HPP__

#include <fstream>
#include <optional>
#include <map>
#include <vector>

constexpr uint8_t SBEM_ID_DESCRIPTOR = 0x00;
constexpr uint8_t SBEM_ID_ESCAPE = 0xFF;

using SbemId = uint16_t;
using SbemSize = uint32_t;

struct ISbemSerialized
{
    virtual bool readFrom(const std::vector<char>& data, size_t offset) = 0;
};

struct SbemHeader
{
    std::vector<char> bytes;
};

struct SbemChunkHeader
{
    SbemId id;
    SbemSize dataSize;
};

struct SbemChunk
{
    SbemChunkHeader header;
    std::vector<char> bytes;

    inline bool tryRead(ISbemSerialized& out) const
    {
        return out.readFrom(bytes, 0);
    }
};

struct SbemDescriptor : public SbemChunk
{
    SbemId getId() const;
    std::optional<std::string> getName() const;
    std::optional<std::string> getFormat() const;
    std::optional<std::vector<SbemId>> getGroup() const;

    SbemDescriptor() {}
    SbemDescriptor(const SbemChunk& chunk)
        : SbemChunk(chunk)
    {
    }
};

class SbemDocument
{
public:
    SbemDocument();
    ~SbemDocument();

    enum class ParseResult
    {
        Success = 0,
        FailedToOpen = 1,
        InvalidHeaderMagic = 2,
        CorruptedFile = 3,
    };

    ParseResult parse(const char* filename);

    const SbemHeader& getHeader() const;
    const std::map<SbemId, SbemDescriptor>& getDescriptors() const;
    const SbemDescriptor* getDescriptor(SbemId id) const;
    const std::vector<SbemChunk>& getChunks() const;

private:
    std::ifstream fileStream;
    size_t fileSize;
    SbemHeader header;

    std::map<SbemId, SbemDescriptor> descriptors;
    std::vector<SbemChunk> chunks;

    std::optional<uint16_t> readId();
    std::optional<uint32_t> readLen();
    std::optional<SbemHeader> readHeader();
    std::optional<SbemChunkHeader> readChunkHeader();
};

#endif // __SBEM_HPP__
