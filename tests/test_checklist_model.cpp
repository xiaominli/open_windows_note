#include "doctest.h"
#include "domain/ChecklistModel.h"
using own::ChecklistItem;
static std::vector<ChecklistItem> mk3() {
    std::vector<ChecklistItem> v;
    own::checklistAdd(v, "a"); own::checklistAdd(v, "b"); own::checklistAdd(v, "c");
    return v;
}
TEST_CASE("add appends with contiguous order") {
    auto v = mk3();
    REQUIRE(v.size() == 3);
    CHECK(v[0].text == "a"); CHECK(v[0].order == 0);
    CHECK(v[2].text == "c"); CHECK(v[2].order == 2);
    CHECK(v[1].checked == false);
}
TEST_CASE("toggle flips only the target") {
    auto v = mk3();
    own::checklistToggle(v, 1);
    CHECK(v[1].checked == true);
    CHECK(v[0].checked == false);
    own::checklistToggle(v, 1);
    CHECK(v[1].checked == false);
}
TEST_CASE("removeAt renumbers order") {
    auto v = mk3();
    own::checklistRemoveAt(v, 0);
    REQUIRE(v.size() == 2);
    CHECK(v[0].text == "b"); CHECK(v[0].order == 0);
    CHECK(v[1].text == "c"); CHECK(v[1].order == 1);
}
TEST_CASE("move reorders and renumbers") {
    auto v = mk3();
    own::checklistMove(v, 2, 0);          // c 提到最前
    CHECK(v[0].text == "c"); CHECK(v[0].order == 0);
    CHECK(v[1].text == "a"); CHECK(v[1].order == 1);
    CHECK(v[2].text == "b"); CHECK(v[2].order == 2);
}
TEST_CASE("out-of-range calls are no-ops") {
    auto v = mk3();
    own::checklistToggle(v, 99);
    own::checklistRemoveAt(v, 99);
    own::checklistMove(v, 0, 99);
    own::checklistSetText(v, 99, "x");
    CHECK(v.size() == 3);
    CHECK(v[0].text == "a");
}
TEST_CASE("setText edits target") {
    auto v = mk3();
    own::checklistSetText(v, 1, "bb");
    CHECK(v[1].text == "bb");
}
