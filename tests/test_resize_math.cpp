#include "doctest.h"
#include "ui/ResizeMath.h"
using own::ResizeEdge;

TEST_CASE("hit test edges and corners with margin") {
    own::RectI c{0,0,200,150};
    CHECK(own::hitTestResizeEdge(c, 199, 75, 6) == ResizeEdge::Right);
    CHECK(own::hitTestResizeEdge(c, 1, 75, 6)   == ResizeEdge::Left);
    CHECK(own::hitTestResizeEdge(c, 100, 149, 6)== ResizeEdge::Bottom);
    CHECK(own::hitTestResizeEdge(c, 198, 148, 6)== ResizeEdge::BottomRight); // 角优先
    CHECK(own::hitTestResizeEdge(c, 100, 75, 6) == ResizeEdge::None);        // 内部
}
TEST_CASE("apply resize right/bottom grows size") {
    own::RectI r = own::applyResize({10,10,200,150}, ResizeEdge::Right, 30, 0, 80, 60);
    CHECK(r.x==10); CHECK(r.y==10); CHECK(r.w==230); CHECK(r.h==150);
    r = own::applyResize({10,10,200,150}, ResizeEdge::Bottom, 0, 25, 80, 60);
    CHECK(r.h==175);
}
TEST_CASE("apply resize left moves origin and shrinks, respecting min width") {
    own::RectI r = own::applyResize({100,10,200,150}, ResizeEdge::Left, 50, 0, 80, 60);
    CHECK(r.x==150); CHECK(r.w==150);
    // 收缩超过下限：宽锁 80，x 停在 start.x+start.w-minW
    own::RectI r2 = own::applyResize({100,10,200,150}, ResizeEdge::Left, 500, 0, 80, 60);
    CHECK(r2.w==80); CHECK(r2.x==220);   // 100+200-80
}
