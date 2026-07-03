#include "doctest.h"
#include "ui/TitleBarLayout.h"
using own::TitleHit;

static own::TitleBarRects mk() {
    own::TitleBarMetrics m{ 28, 20, 4, 4 };     // height,btnSize,gap,pad
    return own::layoutTitleBar(own::RectI{0,0,240,200}, m);
}
TEST_CASE("title bar spans width at top") {
    auto r = mk();
    CHECK(r.titleBar.x == 0); CHECK(r.titleBar.y == 0);
    CHECK(r.titleBar.w == 240); CHECK(r.titleBar.h == 28);
}
TEST_CASE("close is rightmost button, buttons within title bar") {
    auto r = mk();
    CHECK(r.closeBtn.x + r.closeBtn.w == 240 - 4);        // 右侧留 pad
    CHECK(r.closeBtn.w == 20); CHECK(r.closeBtn.h == 20);
    CHECK(r.closeBtn.y >= 0); CHECK(r.closeBtn.y + r.closeBtn.h <= 28);
    // 顺序 从右到左: close, roll, pin, opacity —— x 递减
    CHECK(r.rollBtn.x < r.closeBtn.x);
    CHECK(r.pinBtn.x  < r.rollBtn.x);
    CHECK(r.opacityBtn.x < r.pinBtn.x);
}
TEST_CASE("hit test maps points to controls") {
    auto r = mk();
    CHECK(own::hitTestTitleBar(r, r.closeBtn.x+2, r.closeBtn.y+2) == TitleHit::Close);
    CHECK(own::hitTestTitleBar(r, r.pinBtn.x+2,   r.pinBtn.y+2)   == TitleHit::Pin);
    CHECK(own::hitTestTitleBar(r, 10, 10) == TitleHit::Drag);      // 左侧空白=拖动
    CHECK(own::hitTestTitleBar(r, 10, 100) == TitleHit::None);     // 内容区
}
TEST_CASE("theme button sits left of opacity and hit-tests") {
    auto r = mk();
    CHECK(r.themeBtn.w == 20);
    CHECK(r.themeBtn.x < r.opacityBtn.x);                       // 从右数第 5 个
    CHECK(r.dragArea.x + r.dragArea.w <= r.themeBtn.x);         // 拖动区不与按钮重叠
    CHECK(own::hitTestTitleBar(r, r.themeBtn.x+2, r.themeBtn.y+2) == TitleHit::Theme);
}
