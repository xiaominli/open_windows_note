#include "doctest.h"
#include "domain/StickyRules.h"
#include "domain/BackupRules.h"

TEST_CASE("stick pattern matches window title case-insensitively as substring") {
    CHECK(own::matchesStickTarget("Untitled - Notepad", "NotepadClass", "notepad"));
    CHECK(own::matchesStickTarget("MY REPORT.docx - Word", "OpusApp", "report"));
    CHECK_FALSE(own::matchesStickTarget("Calculator", "AppFrame", "notepad"));
}
TEST_CASE("class: prefix matches window class, not title") {
    CHECK(own::matchesStickTarget("anything", "ConsoleWindowClass", "class:consolewindow"));
    CHECK_FALSE(own::matchesStickTarget("class:consolewindow in title", "Other", "class:consolewindow"));
    CHECK_FALSE(own::matchesStickTarget("x", "ConsoleWindowClass", "class:opusapp"));
}
TEST_CASE("empty pattern never matches") {
    CHECK_FALSE(own::matchesStickTarget("Untitled - Notepad", "NotepadClass", ""));
}
TEST_CASE("class: prefix with empty rest never matches") {
    CHECK_FALSE(own::matchesStickTarget("t", "c", "class:"));
}
TEST_CASE("escapeSqlLiteral doubles single quotes only") {
    CHECK(own::escapeSqlLiteral("plain") == "plain");
    CHECK(own::escapeSqlLiteral("o'brien") == "o''brien");
    CHECK(own::escapeSqlLiteral("''") == "''''");
    CHECK(own::escapeSqlLiteral("") == "");
}
TEST_CASE("defaultBackupName formats zero-padded local fields") {
    CHECK(own::defaultBackupName(2026, 7, 4, 9, 5) == "notes-backup-20260704-0905.db");
    CHECK(own::defaultBackupName(2026, 12, 31, 23, 59) == "notes-backup-20261231-2359.db");
}
