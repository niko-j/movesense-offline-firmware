#pragma once
#include "sbem.hpp"

namespace sbem_loader
{
    /// @brief Load SBEM document from a file
    /// @param path Path to the file
    /// @param sbem Reference to SBEM document
    /// @return True if success, otherwise false
    bool load_sbem(const std::string& path, SbemDocument& sbem);

    /// @brief Read chunks matching measurement name
    /// @param sbem Const reference to SBEM document
    /// @param measurement Substring to match with measurement names
    /// @param parser Function to parse chunks
    /// @return Number of read chunks
    size_t read_chunks(
        const SbemDocument& sbem, 
        const std::string& measurement, 
        std::function<void(const SbemChunk&)> parser);

    /// @brief Find descriptor for first chunk item
    /// @param sbem Const reference to SBEM document
    /// @param chunk Const reference to data chunk
    /// @return Const pointer to descriptor if found, otherwise null
    const SbemDescriptor* get_first_item_descriptor(
        const SbemDocument& sbem, 
        const SbemChunk& chunk);
} // namespace sbem_loader
