#include "doctest.h"
#include "domain/ThemeRules.h"

static std::vector<own::Theme> mk3() {
    std::vector<own::Theme> v(3);
    v[0].id = 1; v[1].id = 2; v[2].id = 3;
    return v;
}
TEST_CASE("nextThemeId cycles through list") {
    auto v = mk3();
    CHECK(own::nextThemeId(v, 1) == 2);
    CHECK(own::nextThemeId(v, 2) == 3);
    CHECK(own::nextThemeId(v, 3) == 1);       // 回绕
}
TEST_CASE("nextThemeId falls back to first when current unknown") {
    auto v = mk3();
    CHECK(own::nextThemeId(v, 0) == 1);       // 未设置主题
    CHECK(own::nextThemeId(v, 42) == 1);
    CHECK(own::nextThemeId({}, 1) == 0);      // 空列表
}
