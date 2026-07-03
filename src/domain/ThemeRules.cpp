#include "domain/ThemeRules.h"
namespace own {
int64_t nextThemeId(const std::vector<Theme>& themes, int64_t currentId) {
    if (themes.empty()) return 0;
    for (size_t i = 0; i < themes.size(); ++i)
        if (themes[i].id == currentId)
            return themes[(i + 1) % themes.size()].id;
    return themes[0].id;
}
}
