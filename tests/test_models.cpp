#include "doctest.h"
#include "domain/Models.h"
TEST_CASE("models default-construct") {
    own::Note n;
    CHECK(n.opacity == 255);
    CHECK(n.pinned == true);
    CHECK(static_cast<int>(n.type) == 0);
}
