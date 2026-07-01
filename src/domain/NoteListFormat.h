#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "domain/Models.h"
namespace own {
std::string noteTitleText(const Note& n);
std::string formatRelativeTime(int64_t nowSec, int64_t thenSec);
enum class NoteSortKey { Title, Updated };
void sortNoteRows(std::vector<Note>& rows, NoteSortKey key, int order);
}
