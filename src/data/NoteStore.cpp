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

bool NoteStore::updateNote(const Note& n, int64_t now) {
    Statement s(db_,
        "UPDATE notes SET type=?,title=?,content_blob=?,plain_text=?,theme_id=?,group_id=?,"
        "pos_x=?,pos_y=?,width=?,height=?,monitor_id=?,opacity=?,pinned=?,rolled_up=?,"
        "visible=?,stick_target=?,updated_at=? WHERE id=?;");
    s.bind(1, (int64_t)n.type); s.bind(2, n.title);
    s.bindBlob(3, n.contentBlob.data(), n.contentBlob.size());
    s.bind(4, n.plainText); s.bind(5, n.themeId); s.bind(6, n.groupId);
    s.bind(7,(int64_t)n.rect.x); s.bind(8,(int64_t)n.rect.y);
    s.bind(9,(int64_t)n.rect.w); s.bind(10,(int64_t)n.rect.h);
    s.bind(11, n.monitorId); s.bind(12,(int64_t)n.opacity);
    s.bind(13,(int64_t)(n.pinned?1:0)); s.bind(14,(int64_t)(n.rolledUp?1:0));
    s.bind(15,(int64_t)(n.visible?1:0)); s.bind(16, n.stickTarget);
    s.bind(17, now); s.bind(18, n.id);
    s.execDone();
    return true;
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

} // namespace own
