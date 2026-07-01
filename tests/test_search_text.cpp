#include "doctest.h"
#include "domain/SearchText.h"
TEST_CASE("searchNormalize lowercases ascii and trims/collapses whitespace") {
    CHECK(own::searchNormalize("  Hello   World \n") == "hello world");
    CHECK(own::searchNormalize("A\tB\r\nC") == "a b c");
    CHECK(own::searchNormalize("") == "");
    CHECK(own::searchNormalize("   ") == "");
}
TEST_CASE("searchNormalize preserves non-ascii bytes") {
    // "黄 Note" -> 中文原样，ASCII 小写；内部空白折叠
    std::string in = "\xE9\xBB\x84  Note";
    CHECK(own::searchNormalize(in) == "\xE9\xBB\x84 note");
}
