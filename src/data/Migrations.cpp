#include "data/Migrations.h"
#include "data/Database.h"
#include "data/Statement.h"

namespace own {

static const char* kSchemaSqlV1 = R"SQL(
CREATE TABLE IF NOT EXISTS notes(
  id INTEGER PRIMARY KEY, type INTEGER NOT NULL, title TEXT,
  content_blob BLOB, plain_text TEXT, theme_id INTEGER, group_id INTEGER,
  pos_x INTEGER, pos_y INTEGER, width INTEGER, height INTEGER, monitor_id TEXT,
  opacity INTEGER DEFAULT 255, pinned INTEGER DEFAULT 1, rolled_up INTEGER DEFAULT 0,
  visible INTEGER DEFAULT 1, stick_target TEXT, created_at INTEGER, updated_at INTEGER);
CREATE TABLE IF NOT EXISTS groups(id INTEGER PRIMARY KEY, name TEXT, order_idx INTEGER);
CREATE TABLE IF NOT EXISTS tags(id INTEGER PRIMARY KEY, name TEXT UNIQUE);
CREATE TABLE IF NOT EXISTS note_tags(note_id INTEGER, tag_id INTEGER, PRIMARY KEY(note_id,tag_id));
CREATE TABLE IF NOT EXISTS reminders(
  id INTEGER PRIMARY KEY, note_id INTEGER NOT NULL, due_at INTEGER NOT NULL,
  recurrence INTEGER DEFAULT 0, recur_interval INTEGER DEFAULT 1,
  snooze_until INTEGER, sound_path TEXT, enabled INTEGER DEFAULT 1);
CREATE TABLE IF NOT EXISTS themes(
  id INTEGER PRIMARY KEY, name TEXT, bg_color INTEGER, title_color INTEGER,
  text_color INTEGER, is_builtin INTEGER);
CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT);
CREATE INDEX IF NOT EXISTS idx_notes_group ON notes(group_id);
CREATE INDEX IF NOT EXISTS idx_notes_visible ON notes(visible);
CREATE INDEX IF NOT EXISTS idx_note_tags_tag ON note_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_reminders_due ON reminders(due_at, enabled);
)SQL";

static bool seedThemes(Database& db, std::string* err) {
    // 4 个内置主题: 黄/粉/蓝/绿
    struct T { const char* n; int bg, title, text; };
    const T ts[] = {
        {"黄", 0xFFF7B0, 0xF2D24A, 0x202020},
        {"粉", 0xFFCFE0, 0xF29CB8, 0x202020},
        {"蓝", 0xCFE3FF, 0x8FBCF2, 0x202020},
        {"绿", 0xCFF7D6, 0x8FE0A0, 0x202020},
    };
    for (const T& t : ts) {
        Statement s(db, "INSERT INTO themes(name,bg_color,title_color,text_color,is_builtin) VALUES(?,?,?,?,1);");
        s.bind(1, std::string(t.n)); s.bind(2, (int64_t)t.bg);
        s.bind(3, (int64_t)t.title); s.bind(4, (int64_t)t.text);
        s.execDone();
    }
    (void)err; return true;
}

bool migrate(Database& db, std::string* err) {
    int v = db.userVersion();
    if (v >= kSchemaVersion) return true;
    Transaction tx(db);
    if (!db.exec(kSchemaSqlV1, err)) return false;
    if (v < 1) { if (!seedThemes(db, err)) return false; }
    db.setUserVersion(kSchemaVersion);
    tx.commit();
    return true;
}

} // namespace own
