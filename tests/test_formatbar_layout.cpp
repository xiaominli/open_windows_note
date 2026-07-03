#include "doctest.h"
#include "ui/FormatBarLayout.h"

static own::FormatBarMetrics fm() { return { 22, 18, 6, 4 }; }

TEST_CASE("format bar sits under title bar, full width") {
    auto bar = own::formatBarRect({0,0,240,200}, 22, fm());
    CHECK(bar.x == 0);
    CHECK(bar.y == 22);
    CHECK(bar.w == 240);
    CHECK(bar.h == 22);
}
TEST_CASE("buttons lay out left to right with gap") {
    auto bar = own::formatBarRect({0,0,240,200}, 22, fm());
    auto b0 = own::formatBarButton(bar, fm(), 0);
    auto b1 = own::formatBarButton(bar, fm(), 1);
    CHECK(b0.x == 6);                     // padX
    CHECK(b0.y == 22 + 2);                // (22-18)/2 vertical center
    CHECK(b0.w == 18);
    CHECK(b1.x == 6 + 18 + 4);            // prev + btnSize + gap
    CHECK(own::kFmtOpCount == 7);
}
TEST_CASE("hit test maps point to button index or -1") {
    auto bar = own::formatBarRect({0,0,240,200}, 22, fm());
    auto b2 = own::formatBarButton(bar, fm(), 2);
    CHECK(own::hitTestFormatBar(bar, fm(), 7, b2.x + 1, b2.y + 1) == 2);
    CHECK(own::hitTestFormatBar(bar, fm(), 7, 239, 23) == -1);     // right blank area
    CHECK(own::hitTestFormatBar(bar, fm(), 7, 3, 30) == -1);       // in padX gutter
    CHECK(own::hitTestFormatBar(bar, fm(), 7, 10, 5) == -1);       // above the bar
}
TEST_CASE("fontSizeStep walks the ladder and clamps") {
    CHECK(own::fontSizeStep(200, true) == 220);    // 10pt -> 11pt
    CHECK(own::fontSizeStep(200, false) == 180);   // 10pt -> 9pt
    CHECK(own::fontSizeStep(480, true) == 480);    // top clamp
    CHECK(own::fontSizeStep(160, false) == 160);   // bottom clamp
    CHECK(own::fontSizeStep(210, true) == 240);    // off-ladder snaps to 220 then steps
    CHECK(own::fontSizeStep(210, false) == 200);
    CHECK(own::fontSizeStep(9999, true) == 480);   // beyond top treated as 480
    CHECK(own::fontSizeStep(1, false) == 160);     // below bottom treated as 160
}
TEST_CASE("nextPaletteColor cycles and falls back to ink") {
    CHECK(own::nextPaletteColor(0x202020) == 0xC0392B);
    CHECK(own::nextPaletteColor(0xC0392B) == 0x1F6FBF);
    CHECK(own::nextPaletteColor(0xB7770D) == 0x202020);   // wrap
    CHECK(own::nextPaletteColor(0x123456) == 0x202020);   // unknown -> ink
}
