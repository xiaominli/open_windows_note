#include "domain/ChecklistJson.h"
#include "json.hpp"
#include <algorithm>
#include <cctype>
using nlohmann::json;
namespace own {

std::vector<uint8_t> serializeChecklist(const std::vector<ChecklistItem>& items) {
    json arr = json::array();
    for (const auto& it : items)
        arr.push_back({ {"text", it.text}, {"checked", it.checked}, {"order", it.order} });
    std::string s = arr.dump();
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<ChecklistItem> deserializeChecklist(const std::vector<uint8_t>& blob) {
    std::vector<ChecklistItem> out;
    if (blob.empty()) return out;
    json j = json::parse(blob.begin(), blob.end(), nullptr, /*allow_exceptions*/false);
    if (!j.is_array()) return out;
    for (const auto& e : j) {
        if (!e.is_object()) continue;
        ChecklistItem it;
        it.text    = e.value("text", std::string());
        it.checked = e.value("checked", false);
        it.order   = e.value("order", 0);
        out.push_back(std::move(it));
    }
    return out;
}

std::string checklistPlainText(const std::vector<ChecklistItem>& items) {
    std::string s;
    for (size_t i=0;i<items.size();++i) { if (i) s += ' '; s += items[i].text; }
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}
} // namespace own
