#include "doctest.h"
#include "ui/DrawingMath.h"
using own::Stroke;
TEST_CASE("point-segment distance: perpendicular and endpoint clamp") {
    CHECK(own::pointSegmentDistance(5, 3, 0, 0, 10, 0) == doctest::Approx(3.0));
    CHECK(own::pointSegmentDistance(-5, 0, 0, 0, 10, 0) == doctest::Approx(5.0)); // 端点外
    CHECK(own::pointSegmentDistance(2, 0, 0, 0, 10, 0) == doctest::Approx(0.0));
}
TEST_CASE("strokeHitTest picks topmost within tolerance, else -1") {
    Stroke a; a.points = {{0,0},{10,0}};     // 下标 0
    Stroke b; b.points = {{0,50},{10,50}};   // 下标 1（后画，更靠上）
    std::vector<Stroke> s = { a, b };
    CHECK(own::strokeHitTest(s, 5, 2, 4.0) == 0);
    CHECK(own::strokeHitTest(s, 5, 48, 4.0) == 1);
    CHECK(own::strokeHitTest(s, 5, 25, 4.0) == -1);   // 都不在容差内
}
TEST_CASE("strokeHitTest topmost wins when overlapping") {
    Stroke a; a.points = {{0,0},{10,0}};
    Stroke b; b.points = {{0,0},{10,0}};
    std::vector<Stroke> s = { a, b };
    CHECK(own::strokeHitTest(s, 5, 0, 2.0) == 1);      // 取后画的
}
TEST_CASE("single-point stroke uses point distance") {
    Stroke a; a.points = {{5,5}};
    std::vector<Stroke> s = { a };
    CHECK(own::strokeHitTest(s, 6, 5, 2.0) == 0);
    CHECK(own::strokeHitTest(s, 20, 20, 2.0) == -1);
}
