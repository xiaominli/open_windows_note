#pragma once
#include <cstdint>
#include <vector>
#include "domain/Models.h"
namespace own {
// 主题循环：返回 themes 里 currentId 的下一个（回绕）；找不到→第一个；空→0
int64_t nextThemeId(const std::vector<Theme>& themes, int64_t currentId);
}
