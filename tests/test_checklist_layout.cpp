#include "doctest.h"
#include "ui/ChecklistLayout.h"
using own::ChecklistHit;
static own::ChecklistMetrics M{ 24, 16, 4 };   // rowHeight,boxSize,pad
static own::RectI C{ 0, 0, 200, 300 };
TEST_CASE("row rect stacks by rowHeight and spans width") {
    auto r0 = own::checklistRowRect(C, M, 0);
    CHECK(r0.x == 0); CHECK(r0.y == 0); CHECK(r0.w == 200); CHECK(r0.h == 24);
    auto r2 = own::checklistRowRect(C, M, 2);
    CHECK(r2.y == 48);
}
TEST_CASE("box rect sits at left, vertically centered, boxSize square") {
    auto b = own::checklistBoxRect(C, M, 0);
    CHECK(b.x == 4);                       // pad
    CHECK(b.w == 16); CHECK(b.h == 16);
    CHECK(b.y == (24 - 16) / 2);           // 居中 -> 4
}
TEST_CASE("hit test: checkbox / text / add-row / none") {
    // 3 条目：第 0 行 box 命中
    auto hb = own::checklistHitTest(C, M, 3, 8, 12);
    CHECK(hb.kind == ChecklistHit::Checkbox); CHECK(hb.index == 0);
    // 第 1 行文本区（x 越过 box）
    auto ht = own::checklistHitTest(C, M, 3, 120, 24 + 12);
    CHECK(ht.kind == ChecklistHit::Text); CHECK(ht.index == 1);
    // 第 3 行（itemCount 行）= 新增行
    auto ha = own::checklistHitTest(C, M, 3, 50, 3 * 24 + 5);
    CHECK(ha.kind == ChecklistHit::AddRow); CHECK(ha.index == -1);
    // 再往下 = None
    auto hn = own::checklistHitTest(C, M, 3, 50, 10 * 24);
    CHECK(hn.kind == ChecklistHit::None);
}
