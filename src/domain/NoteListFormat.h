#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "domain/Models.h"
namespace own {
std::string noteTitleText(const Note& n);
// 便签窗标题栏文案:展开只显自定义标题(空则留空,不重复正文首行);
// 卷起时窗口只剩标题栏,回落 首行 → (无标题) 保证可识别。
std::string noteWindowTitleText(const Note& n, bool rolledUp);
std::string formatRelativeTime(int64_t nowSec, int64_t thenSec);
enum class NoteSortKey { Title, Updated };
void sortNoteRows(std::vector<Note>& rows, NoteSortKey key, int order);
}
