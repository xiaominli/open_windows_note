#pragma once
#include <vector>
#include <string>
#include "domain/Models.h"
namespace own {
void checklistToggle(std::vector<ChecklistItem>& items, size_t i);
void checklistAdd(std::vector<ChecklistItem>& items, const std::string& text);
void checklistRemoveAt(std::vector<ChecklistItem>& items, size_t i);
void checklistMove(std::vector<ChecklistItem>& items, size_t from, size_t to);
void checklistSetText(std::vector<ChecklistItem>& items, size_t i, const std::string& text);
}
