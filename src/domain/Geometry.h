#pragma once
#include <vector>
#include "domain/Models.h"
namespace own {
RectI clampRectToWorkArea(RectI note, const std::vector<RectI>& monitors);
}
