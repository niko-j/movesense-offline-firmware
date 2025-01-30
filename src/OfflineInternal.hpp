#pragma once
#include "protocol/OfflineConfig.hpp"
#include "protocol/OfflineBuffers.hpp"
#include <app-resources/resources.h>

WB_RES::OfflineConfig internalToWb(const OfflineConfig& config);
OfflineConfig wbToInternal(const WB_RES::OfflineConfig& config);

wb::Array<uint8_t> bufferToArray(const WritableBuffer& stream);
ReadableBuffer arrayToBuffer(const wb::Array<uint8_t>& array);