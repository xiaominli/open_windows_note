#pragma once
#include <optional>
#include <vector>
#include "domain/Models.h"
namespace own {
class Database;
struct NoteQuery {
    std::string search;
    int64_t groupId = -1;
    int64_t tagId = -1;
    bool onlyVisible = false;
};
class NoteStore {
public:
    explicit NoteStore(Database& db) : db_(db) {}
    int64_t insertNote(Note n, int64_t now);
    bool updateNote(const Note& n, int64_t now);
    bool updateGeometry(int64_t id, RectI r, const std::string& monitorId);
    bool updateFlags(int64_t id, int opacity, bool pinned, bool rolledUp, bool visible);
    bool updateContent(int64_t id, const std::vector<uint8_t>& blob, const std::string& plainText, int64_t now);
    bool deleteNote(int64_t id);
    std::optional<Note> getNote(int64_t id);
    std::vector<Note> allNotes();
    std::vector<Note> query(const NoteQuery& q);
    // ---- groups ----
    int64_t upsertGroup(Group g);
    std::vector<Group> allGroups();
    bool deleteGroup(int64_t id);
    // ---- tags ----
    int64_t upsertTag(const std::string& name);
    std::vector<Tag> allTags();
    bool addTagToNote(int64_t noteId, int64_t tagId);
    bool removeTagFromNote(int64_t noteId, int64_t tagId);
    std::vector<Tag> tagsOfNote(int64_t noteId);
    // ---- reminders ----
    int64_t insertReminder(Reminder r);
    bool updateReminder(const Reminder& r);
    bool deleteReminder(int64_t id);
    std::vector<Reminder> remindersOfNote(int64_t noteId);
    std::vector<Reminder> enabledReminders();
    // ---- themes ----
    std::vector<Theme> allThemes();
    std::optional<Theme> getTheme(int64_t id);
    bool updateNoteTheme(int64_t noteId, int64_t themeId);   // 只改 theme_id（updateNote 会整行覆盖 blob）
    bool updateNoteGroup(int64_t noteId, int64_t groupId);   // 只改 group_id（updateNote 会整行覆盖 blob）
private:
    Note readRow(class Statement& s);  // 从 SELECT * 顺序读一行
    Database& db_;
};
} // namespace own
