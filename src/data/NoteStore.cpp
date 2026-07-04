#include "data/NoteStore.h"
#include "data/Database.h"
#include "data/Statement.h"

namespace own {

static const char* kCols =
    "id,type,title,content_blob,plain_text,theme_id,group_id,"
    "pos_x,pos_y,width,height,monitor_id,opacity,pinned,rolled_up,"
    "visible,stick_target,created_at,updated_at";

Note NoteStore::readRow(Statement& s) {
    Note n;
    n.id = s.columnInt64(0);
    n.type = (NoteType)s.columnInt64(1);
    n.title = s.columnText(2);
    n.contentBlob = s.columnBlob(3);
    n.plainText = s.columnText(4);
    n.themeId = s.columnInt64(5);
    n.groupId = s.columnInt64(6);
    n.rect = { (int)s.columnInt64(7),(int)s.columnInt64(8),(int)s.columnInt64(9),(int)s.columnInt64(10) };
    n.monitorId = s.columnText(11);
    n.opacity = (int)s.columnInt64(12);
    n.pinned = s.columnInt64(13) != 0;
    n.rolledUp = s.columnInt64(14) != 0;
    n.visible = s.columnInt64(15) != 0;
    n.stickTarget = s.columnText(16);
    n.createdAt = s.columnInt64(17);
    n.updatedAt = s.columnInt64(18);
    return n;
}

int64_t NoteStore::insertNote(Note n, int64_t now) {
    n.createdAt = now; n.updatedAt = now;
    Statement s(db_,
        "INSERT INTO notes(type,title,content_blob,plain_text,theme_id,group_id,"
        "pos_x,pos_y,width,height,monitor_id,opacity,pinned,rolled_up,visible,"
        "stick_target,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    s.bind(1, (int64_t)n.type); s.bind(2, n.title);
    s.bindBlob(3, n.contentBlob.data(), n.contentBlob.size());
    s.bind(4, n.plainText); s.bind(5, n.themeId); s.bind(6, n.groupId);
    s.bind(7, (int64_t)n.rect.x); s.bind(8, (int64_t)n.rect.y);
    s.bind(9, (int64_t)n.rect.w); s.bind(10, (int64_t)n.rect.h);
    s.bind(11, n.monitorId); s.bind(12, (int64_t)n.opacity);
    s.bind(13, (int64_t)(n.pinned?1:0)); s.bind(14, (int64_t)(n.rolledUp?1:0));
    s.bind(15, (int64_t)(n.visible?1:0)); s.bind(16, n.stickTarget);
    s.bind(17, n.createdAt); s.bind(18, n.updatedAt);
    s.execDone();
    return db_.lastInsertRowId();
}

bool NoteStore::updateGeometry(int64_t id, RectI r, const std::string& monitorId) {
    Statement s(db_, "UPDATE notes SET pos_x=?,pos_y=?,width=?,height=?,monitor_id=? WHERE id=?;");
    s.bind(1,(int64_t)r.x); s.bind(2,(int64_t)r.y); s.bind(3,(int64_t)r.w);
    s.bind(4,(int64_t)r.h); s.bind(5, monitorId); s.bind(6, id);
    s.execDone(); return true;
}

bool NoteStore::updateFlags(int64_t id, int opacity, bool pinned, bool rolledUp, bool visible) {
    Statement s(db_, "UPDATE notes SET opacity=?,pinned=?,rolled_up=?,visible=? WHERE id=?;");
    s.bind(1,(int64_t)opacity); s.bind(2,(int64_t)(pinned?1:0));
    s.bind(3,(int64_t)(rolledUp?1:0)); s.bind(4,(int64_t)(visible?1:0)); s.bind(5, id);
    s.execDone(); return true;
}

bool NoteStore::updateTitle(int64_t id, const std::string& titleU8) {
    Statement s(db_, "UPDATE notes SET title=? WHERE id=?;");
    s.bind(1, titleU8); s.bind(2, id);
    s.execDone(); return true;
}

bool NoteStore::updateContent(int64_t id, const std::vector<uint8_t>& blob,
                              const std::string& plainText, int64_t now) {
    Statement s(db_, "UPDATE notes SET content_blob=?,plain_text=?,updated_at=? WHERE id=?;");
    s.bindBlob(1, blob.data(), blob.size());
    s.bind(2, plainText);
    s.bind(3, now);
    s.bind(4, id);
    s.execDone();
    return true;
}

bool NoteStore::deleteNote(int64_t id) {
    Transaction tx(db_);
    { Statement s(db_, "DELETE FROM note_tags WHERE note_id=?;"); s.bind(1,id); s.execDone(); }
    { Statement s(db_, "DELETE FROM reminders WHERE note_id=?;"); s.bind(1,id); s.execDone(); }
    { Statement s(db_, "DELETE FROM notes WHERE id=?;"); s.bind(1,id); s.execDone(); }
    tx.commit(); return true;
}

std::optional<Note> NoteStore::getNote(int64_t id) {
    Statement s(db_, std::string("SELECT ")+kCols+" FROM notes WHERE id=?;");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return readRow(s);
}

std::vector<Note> NoteStore::allNotes() {
    Statement s(db_, std::string("SELECT ")+kCols+" FROM notes ORDER BY updated_at DESC, id DESC;");
    std::vector<Note> out;
    while (s.step()) out.push_back(readRow(s));
    return out;
}

std::vector<Note> NoteStore::query(const NoteQuery& q) {
    std::string sql = std::string("SELECT ")+kCols+" FROM notes n";
    std::vector<std::string> where;
    if (q.tagId >= 0) sql += " JOIN note_tags nt ON nt.note_id=n.id AND nt.tag_id=?";
    if (!q.search.empty()) where.push_back("plain_text LIKE '%'||?||'%'");
    if (q.groupId >= 0)   where.push_back("group_id=?");
    if (q.onlyVisible)    where.push_back("visible=1");
    for (size_t i=0;i<where.size();++i) sql += (i==0?" WHERE ":" AND ") + where[i];
    sql += " ORDER BY updated_at DESC, id DESC;";
    Statement s(db_, sql);
    int idx = 1;
    if (q.tagId >= 0) s.bind(idx++, q.tagId);
    if (!q.search.empty()) s.bind(idx++, q.search);
    if (q.groupId >= 0) s.bind(idx++, q.groupId);
    std::vector<Note> out;
    while (s.step()) out.push_back(readRow(s));
    return out;
}

// ---- groups ----
int64_t NoteStore::upsertGroup(Group g) {
    if (g.id > 0) {
        Statement s(db_, "UPDATE groups SET name=?,order_idx=? WHERE id=?;");
        s.bind(1,g.name); s.bind(2,(int64_t)g.orderIdx); s.bind(3,g.id); s.execDone();
        return g.id;
    }
    Statement s(db_, "INSERT INTO groups(name,order_idx) VALUES(?,?);");
    s.bind(1,g.name); s.bind(2,(int64_t)g.orderIdx); s.execDone();
    return db_.lastInsertRowId();
}
std::vector<Group> NoteStore::allGroups() {
    Statement s(db_, "SELECT id,name,order_idx FROM groups ORDER BY order_idx,id;");
    std::vector<Group> out;
    while (s.step()) out.push_back({ s.columnInt64(0), s.columnText(1), (int)s.columnInt64(2) });
    return out;
}
bool NoteStore::deleteGroup(int64_t id) {
    Transaction tx(db_);
    { Statement s(db_, "UPDATE notes SET group_id=0 WHERE group_id=?;"); s.bind(1,id); s.execDone(); }
    { Statement s(db_, "DELETE FROM groups WHERE id=?;"); s.bind(1,id); s.execDone(); }
    tx.commit(); return true;
}
// ---- tags ----
int64_t NoteStore::upsertTag(const std::string& name) {
    { Statement s(db_, "SELECT id FROM tags WHERE name=?;"); s.bind(1,name);
      if (s.step()) return s.columnInt64(0); }
    Statement s(db_, "INSERT INTO tags(name) VALUES(?);"); s.bind(1,name); s.execDone();
    return db_.lastInsertRowId();
}
std::vector<Tag> NoteStore::allTags() {
    Statement s(db_, "SELECT id,name FROM tags ORDER BY name;");
    std::vector<Tag> out; while (s.step()) out.push_back({ s.columnInt64(0), s.columnText(1) });
    return out;
}
bool NoteStore::addTagToNote(int64_t noteId, int64_t tagId) {
    Statement s(db_, "INSERT OR IGNORE INTO note_tags(note_id,tag_id) VALUES(?,?);");
    s.bind(1,noteId); s.bind(2,tagId); s.execDone(); return true;
}
bool NoteStore::removeTagFromNote(int64_t noteId, int64_t tagId) {
    Statement s(db_, "DELETE FROM note_tags WHERE note_id=? AND tag_id=?;");
    s.bind(1,noteId); s.bind(2,tagId); s.execDone(); return true;
}
std::vector<Tag> NoteStore::tagsOfNote(int64_t noteId) {
    Statement s(db_, "SELECT t.id,t.name FROM tags t JOIN note_tags nt ON nt.tag_id=t.id "
                     "WHERE nt.note_id=? ORDER BY t.name;");
    s.bind(1,noteId);
    std::vector<Tag> out; while (s.step()) out.push_back({ s.columnInt64(0), s.columnText(1) });
    return out;
}
// ---- reminders ----
static void bindReminder(Statement& s, const Reminder& r, int base) {
    s.bind(base+0, r.noteId); s.bind(base+1, r.dueAt);
    s.bind(base+2,(int64_t)r.recurrence); s.bind(base+3,(int64_t)r.recurInterval);
    s.bind(base+4, r.snoozeUntil); s.bind(base+5, r.soundPath);
    s.bind(base+6,(int64_t)(r.enabled?1:0));
}
int64_t NoteStore::insertReminder(Reminder r) {
    Statement s(db_, "INSERT INTO reminders(note_id,due_at,recurrence,recur_interval,"
                     "snooze_until,sound_path,enabled) VALUES(?,?,?,?,?,?,?);");
    bindReminder(s, r, 1); s.execDone(); return db_.lastInsertRowId();
}
bool NoteStore::updateReminder(const Reminder& r) {
    Statement s(db_, "UPDATE reminders SET note_id=?,due_at=?,recurrence=?,recur_interval=?,"
                     "snooze_until=?,sound_path=?,enabled=? WHERE id=?;");
    bindReminder(s, r, 1); s.bind(8, r.id); s.execDone(); return true;
}
bool NoteStore::deleteReminder(int64_t id) {
    Statement s(db_, "DELETE FROM reminders WHERE id=?;"); s.bind(1,id); s.execDone(); return true;
}
static Reminder readReminder(Statement& s) {
    Reminder r;
    r.id=s.columnInt64(0); r.noteId=s.columnInt64(1); r.dueAt=s.columnInt64(2);
    r.recurrence=(Recurrence)s.columnInt64(3); r.recurInterval=(int)s.columnInt64(4);
    r.snoozeUntil=s.columnInt64(5); r.soundPath=s.columnText(6); r.enabled=s.columnInt64(7)!=0;
    return r;
}
std::vector<Reminder> NoteStore::remindersOfNote(int64_t noteId) {
    Statement s(db_, "SELECT id,note_id,due_at,recurrence,recur_interval,snooze_until,"
                     "sound_path,enabled FROM reminders WHERE note_id=? ORDER BY due_at;");
    s.bind(1,noteId);
    std::vector<Reminder> out; while (s.step()) out.push_back(readReminder(s)); return out;
}
std::vector<Reminder> NoteStore::enabledReminders() {
    Statement s(db_, "SELECT id,note_id,due_at,recurrence,recur_interval,snooze_until,"
                     "sound_path,enabled FROM reminders WHERE enabled=1 ORDER BY due_at;");
    std::vector<Reminder> out; while (s.step()) out.push_back(readReminder(s)); return out;
}
// ---- themes ----
static Theme readTheme(Statement& s) {
    Theme t;
    t.id = s.columnInt64(0);
    t.name = s.columnText(1);
    t.bgColor = (uint32_t)s.columnInt64(2);
    t.titleColor = (uint32_t)s.columnInt64(3);
    t.textColor = (uint32_t)s.columnInt64(4);
    t.isBuiltin = s.columnInt64(5) != 0;
    return t;
}
std::vector<Theme> NoteStore::allThemes() {
    std::vector<Theme> out;
    Statement s(db_, "SELECT id,name,bg_color,title_color,text_color,is_builtin FROM themes ORDER BY id;");
    while (s.step()) out.push_back(readTheme(s));
    return out;
}
std::optional<Theme> NoteStore::getTheme(int64_t id) {
    Statement s(db_, "SELECT id,name,bg_color,title_color,text_color,is_builtin FROM themes WHERE id=?;");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return readTheme(s);
}
bool NoteStore::updateNoteTheme(int64_t noteId, int64_t themeId) {
    Statement s(db_, "UPDATE notes SET theme_id=? WHERE id=?;");
    s.bind(1, themeId); s.bind(2, noteId);
    s.execDone();
    return true;
}
bool NoteStore::updateNoteGroup(int64_t noteId, int64_t groupId) {
    Statement s(db_, "UPDATE notes SET group_id=? WHERE id=?;");
    s.bind(1, groupId); s.bind(2, noteId);
    s.execDone();
    return true;
}
bool NoteStore::updateNoteStick(int64_t noteId, const std::string& target) {
    Statement s(db_, "UPDATE notes SET stick_target=? WHERE id=?;");
    s.bind(1, target); s.bind(2, noteId);
    s.execDone();
    return true;
}

} // namespace own
