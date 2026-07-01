#include "domain/ChecklistModel.h"
namespace own {
static void renumber(std::vector<ChecklistItem>& items) {
    for (size_t i = 0; i < items.size(); ++i) items[i].order = (int)i;
}
void checklistToggle(std::vector<ChecklistItem>& items, size_t i) {
    if (i < items.size()) items[i].checked = !items[i].checked;
}
void checklistAdd(std::vector<ChecklistItem>& items, const std::string& text) {
    ChecklistItem it; it.text = text; it.checked = false; it.order = (int)items.size();
    items.push_back(it);
}
void checklistRemoveAt(std::vector<ChecklistItem>& items, size_t i) {
    if (i >= items.size()) return;
    items.erase(items.begin() + i);
    renumber(items);
}
void checklistMove(std::vector<ChecklistItem>& items, size_t from, size_t to) {
    if (from >= items.size() || to >= items.size() || from == to) return;
    ChecklistItem tmp = items[from];
    items.erase(items.begin() + from);
    items.insert(items.begin() + to, tmp);
    renumber(items);
}
void checklistSetText(std::vector<ChecklistItem>& items, size_t i, const std::string& text) {
    if (i < items.size()) items[i].text = text;
}
}
