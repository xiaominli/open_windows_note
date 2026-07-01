#include "ui/DrawingMath.h"
#include <cmath>
namespace own {
double pointSegmentDistance(double px, double py, double ax, double ay, double bx, double by) {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx*dx + dy*dy;
    double t = 0.0;
    if (len2 > 0.0) {
        t = ((px - ax) * dx + (py - ay) * dy) / len2;
        if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
    }
    double cx = ax + t*dx, cy = ay + t*dy;
    double ex = px - cx, ey = py - cy;
    return std::sqrt(ex*ex + ey*ey);
}
int strokeHitTest(const std::vector<Stroke>& strokes, int px, int py, double tol) {
    for (int i = (int)strokes.size() - 1; i >= 0; --i) {   // 后画的在上
        const auto& pts = strokes[i].points;
        if (pts.empty()) continue;
        if (pts.size() == 1) {
            double ex = px - pts[0].first, ey = py - pts[0].second;
            if (std::sqrt(ex*ex + ey*ey) <= tol) return i;
            continue;
        }
        for (size_t j = 0; j + 1 < pts.size(); ++j) {
            double d = pointSegmentDistance(px, py,
                pts[j].first, pts[j].second, pts[j+1].first, pts[j+1].second);
            if (d <= tol) return i;
        }
    }
    return -1;
}
}
