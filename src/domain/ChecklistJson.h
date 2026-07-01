#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "domain/Models.h"
namespace own {
std::vector<uint8_t> serializeChecklist(const std::vector<ChecklistItem>& items);
std::vector<ChecklistItem> deserializeChecklist(const std::vector<uint8_t>& blob);
std::string checklistPlainText(const std::vector<ChecklistItem>& items);
}
