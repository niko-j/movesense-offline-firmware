#pragma once
#include "sbem.hpp"
#include <iostream>

namespace sbem_utils
{
    void printHeader(const SbemDocument& sbem, std::ostream& out);
    void printDescriptors(const SbemDocument& sbem, std::ostream& out);
    void printDataChunks(const SbemDocument& sbem, std::ostream& out);
} // namespace sbem_utils
