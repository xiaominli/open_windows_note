#include "doctest.h"
#include "ui/ContentLayout.h"
TEST_CASE("content rect sits below title bar, inset by resize margin") {
    auto r = own::noteContentRect(own::RectI{0,0,240,200}, 28, 6);
    CHECK(r.x == 6); CHECK(r.y == 28);
    CHECK(r.w == 228);                 // 240 - 12
    CHECK(r.h == 200 - 28 - 6);        // 166
}
TEST_CASE("degenerate sizes clamp to zero") {
    auto r = own::noteContentRect(own::RectI{0,0,4,20}, 28, 6);
    CHECK(r.w == 0);
    CHECK(r.h == 0);
}
