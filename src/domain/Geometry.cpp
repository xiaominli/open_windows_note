#include "domain/Geometry.h"
#include <algorithm>
namespace own {

static bool intersects(const RectI& a, const RectI& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w &&
           a.y < b.y + b.h && b.y < a.y + a.h;
}

RectI clampRectToWorkArea(RectI note, const std::vector<RectI>& monitors) {
    if (monitors.empty()) return note;
    for (const auto& m : monitors) if (intersects(note, m)) return note;
    const RectI& m = monitors.front();
    RectI r = note;
    r.x = std::max(m.x, std::min(note.x, m.x + m.w - note.w));
    r.y = std::max(m.y, std::min(note.y, m.y + m.h - note.h));
    if (r.x < m.x) r.x = m.x;         // note 比屏宽时左对齐
    if (r.y < m.y) r.y = m.y;
    return r;
}
} // namespace own
