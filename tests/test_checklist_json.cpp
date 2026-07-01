#include "doctest.h"
#include "domain/ChecklistJson.h"

TEST_CASE("checklist roundtrip") {
    std::vector<own::ChecklistItem> items = {
        {"买牛奶", false, 0}, {"交电费", true, 1}
    };
    auto blob = own::serializeChecklist(items);
    auto back = own::deserializeChecklist(blob);
    REQUIRE(back.size() == 2);
    CHECK(back[0].text == "买牛奶");
    CHECK(back[0].checked == false);
    CHECK(back[1].checked == true);
    CHECK(back[1].order == 1);
}

TEST_CASE("deserialize garbage returns empty, no throw") {
    std::vector<uint8_t> junk = {'x','y','z'};
    auto r = own::deserializeChecklist(junk);
    CHECK(r.empty());
}

TEST_CASE("plain text joins lowercased") {
    std::vector<own::ChecklistItem> items = {{"ABC",false,0},{"Def",false,1}};
    CHECK(own::checklistPlainText(items) == "abc def");
}
