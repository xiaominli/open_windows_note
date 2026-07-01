#include "doctest.h"
#include "domain/StrokesJson.h"

TEST_CASE("strokes roundtrip") {
    std::vector<own::Stroke> s = { { 0xFF0000, 4, {{1,2},{3,4},{5,6}} } };
    auto blob = own::serializeStrokes(s);
    auto back = own::deserializeStrokes(blob);
    REQUIRE(back.size() == 1);
    CHECK(back[0].color == 0xFF0000);
    CHECK(back[0].width == 4);
    REQUIRE(back[0].points.size() == 3);
    CHECK(back[0].points[1].first == 3);
    CHECK(back[0].points[1].second == 4);
}

TEST_CASE("strokes deserialize garbage returns empty") {
    CHECK(own::deserializeStrokes({'!','?'}).empty());
}
