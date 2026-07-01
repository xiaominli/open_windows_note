#pragma once
#include <optional>
#include <vector>
#include "domain/Models.h"
namespace own {
class Database;
class NoteStore {
public:
    explicit NoteStore(Database& db) : db_(db) {}
    int64_t insertNote(Note n, int64_t now);
    bool updateNote(const Note& n, int64_t now);
    bool updateGeometry(int64_t id, RectI r, const std::string& monitorId);
    bool updateFlags(int64_t id, int opacity, bool pinned, bool rolledUp, bool visible);
    bool deleteNote(int64_t id);
    std::optional<Note> getNote(int64_t id);
    std::vector<Note> allNotes();
private:
    Note readRow(class Statement& s);  // 从 SELECT * 顺序读一行
    Database& db_;
};
} // namespace own
