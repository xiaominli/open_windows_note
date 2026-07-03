#include "domain/StickyRules.h"
#include "domain/SearchText.h"
namespace own {
bool matchesStickTarget(const std::string& titleU8, const std::string& classU8, const std::string& pattern) {
    if (pattern.empty()) return false;
    static const std::string kClassPrefix = "class:";
    if (pattern.rfind(kClassPrefix, 0) == 0) {
        std::string p = searchNormalize(pattern.substr(kClassPrefix.size()));
        if (p.empty()) return false;
        return searchNormalize(classU8).find(p) != std::string::npos;
    }
    std::string p = searchNormalize(pattern);
    if (p.empty()) return false;
    return searchNormalize(titleU8).find(p) != std::string::npos;
}
}
