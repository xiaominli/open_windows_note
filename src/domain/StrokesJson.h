#pragma once
#include <vector>
#include <cstdint>
#include "domain/Models.h"
namespace own {
std::vector<uint8_t> serializeStrokes(const std::vector<Stroke>& strokes);
std::vector<Stroke> deserializeStrokes(const std::vector<uint8_t>& blob);
}
