#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace own {  // open_windows_note

enum class NoteType : int { RichText = 0, Checklist = 1, Drawing = 2 };
enum class Recurrence : int { None = 0, Daily = 1, Weekly = 2, Monthly = 3 };

struct RectI { int x = 0, y = 0, w = 0, h = 0; };

struct Note {
    int64_t id = 0;
    NoteType type = NoteType::RichText;
    std::string title;
    std::vector<uint8_t> contentBlob;   // RTF / JSON items / JSON strokes
    std::string plainText;              // 搜索缓存(已小写)
    int64_t themeId = 0;
    int64_t groupId = 0;                // 0 = 无分组
    RectI rect{ 100, 100, 240, 200 };
    std::string monitorId;
    int opacity = 255;                  // 0..255
    bool pinned = true;
    bool rolledUp = false;
    bool visible = true;
    std::string stickTarget;            // 空 = 不贴
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
};

struct ChecklistItem { std::string text; bool checked = false; int order = 0; };

struct Stroke {
    uint32_t color = 0x000000;          // 0xRRGGBB
    int width = 3;
    std::vector<std::pair<int,int>> points;
};

struct Tag { int64_t id = 0; std::string name; };
struct Group { int64_t id = 0; std::string name; int orderIdx = 0; };

struct Reminder {
    int64_t id = 0;
    int64_t noteId = 0;
    int64_t dueAt = 0;                  // Unix 秒
    Recurrence recurrence = Recurrence::None;
    int recurInterval = 1;
    int64_t snoozeUntil = 0;            // 0 = 无
    std::string soundPath;
    bool enabled = true;
};

struct Theme {
    int64_t id = 0;
    std::string name;
    uint32_t bgColor = 0xFFF7B0;        // 0xRRGGBB
    uint32_t titleColor = 0xF2D24A;
    uint32_t textColor = 0x202020;
    bool isBuiltin = false;
};

} // namespace own
