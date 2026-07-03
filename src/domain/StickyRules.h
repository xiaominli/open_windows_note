#pragma once
#include <string>
namespace own {
// 贴窗匹配："class:" 前缀 -> 类名子串匹配；否则标题子串匹配；大小写不敏感（searchNormalize 折叠）
bool matchesStickTarget(const std::string& titleU8, const std::string& classU8, const std::string& pattern);
}
