#include "doctest.h"
#include "domain/Geometry.h"

TEST_CASE("visible rect unchanged") {
    std::vector<own::RectI> mons = { {0,0,1920,1080} };
    own::RectI n{100,100,200,150};
    auto r = own::clampRectToWorkArea(n, mons);
    CHECK(r.x==100); CHECK(r.y==100); CHECK(r.w==200); CHECK(r.h==150);
}

TEST_CASE("offscreen rect moved onto first monitor keeping size") {
    std::vector<own::RectI> mons = { {0,0,1920,1080} };
    own::RectI n{5000,5000,200,150};                 // 完全在屏外
    auto r = own::clampRectToWorkArea(n, mons);
    CHECK(r.w==200); CHECK(r.h==150);
    // 完整落在屏内
    CHECK(r.x >= 0); CHECK(r.y >= 0);
    CHECK(r.x + r.w <= 1920); CHECK(r.y + r.h <= 1080);
}

TEST_CASE("no monitors returns input unchanged") {
    own::RectI n{5000,5000,200,150};
    auto r = own::clampRectToWorkArea(n, {});
    CHECK(r.x==5000);
}
