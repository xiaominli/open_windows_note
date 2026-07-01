#include "doctest.h"
#include "app/AppPaths.h"

TEST_CASE("chooseDbPath prefers portable exe dir when writable") {
    auto c = own::chooseDbPath("C:\\apps\\own", "C:\\Users\\u\\AppData\\Roaming", true);
    CHECK(c.portable == true);
    CHECK(c.path == "C:\\apps\\own\\notes.db");
}
TEST_CASE("chooseDbPath falls back to appdata when exe dir not writable") {
    auto c = own::chooseDbPath("C:\\Program Files\\own", "C:\\Users\\u\\AppData\\Roaming", false);
    CHECK(c.portable == false);
    CHECK(c.path == "C:\\Users\\u\\AppData\\Roaming\\open_windows_note\\notes.db");
}
