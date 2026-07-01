#include "domain/StrokesJson.h"
#include "json.hpp"
using nlohmann::json;
namespace own {

std::vector<uint8_t> serializeStrokes(const std::vector<Stroke>& strokes) {
    json arr = json::array();
    for (const auto& s : strokes) {
        json pts = json::array();
        for (const auto& p : s.points) pts.push_back({ p.first, p.second });
        arr.push_back({ {"color", s.color}, {"width", s.width}, {"points", pts} });
    }
    json root = { {"strokes", arr} };
    std::string str = root.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<Stroke> deserializeStrokes(const std::vector<uint8_t>& blob) {
    std::vector<Stroke> out;
    if (blob.empty()) return out;
    json j = json::parse(blob.begin(), blob.end(), nullptr, false);
    if (!j.is_object() || !j.contains("strokes") || !j["strokes"].is_array()) return out;
    for (const auto& e : j["strokes"]) {
        if (!e.is_object()) continue;
        Stroke s;
        s.color = e.value("color", 0u);
        s.width = e.value("width", 3);
        if (e.contains("points") && e["points"].is_array()) {
            for (const auto& p : e["points"]) {
                if (p.is_array() && p.size()==2 && p[0].is_number_integer() && p[1].is_number_integer())
                    s.points.emplace_back((int)p[0], (int)p[1]);
            }
        }
        out.push_back(std::move(s));
    }
    return out;
}
} // namespace own
