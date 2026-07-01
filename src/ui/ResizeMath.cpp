#include "ui/ResizeMath.h"
namespace own {
ResizeEdge hitTestResizeEdge(RectI c, int px, int py, int margin) {
    bool L = px >= c.x && px < c.x + margin;
    bool R = px < c.x + c.w && px >= c.x + c.w - margin;
    bool T = py >= c.y && py < c.y + margin;
    bool B = py < c.y + c.h && py >= c.y + c.h - margin;
    if (T && L) return ResizeEdge::TopLeft;
    if (T && R) return ResizeEdge::TopRight;
    if (B && L) return ResizeEdge::BottomLeft;
    if (B && R) return ResizeEdge::BottomRight;
    if (L) return ResizeEdge::Left;
    if (R) return ResizeEdge::Right;
    if (T) return ResizeEdge::Top;
    if (B) return ResizeEdge::Bottom;
    return ResizeEdge::None;
}
RectI applyResize(RectI s, ResizeEdge e, int dx, int dy, int minW, int minH) {
    int left = s.x, top = s.y, right = s.x + s.w, bottom = s.y + s.h;
    switch (e) {
        case ResizeEdge::Left:  left += dx; break;
        case ResizeEdge::Right: right += dx; break;
        case ResizeEdge::Top:   top += dy; break;
        case ResizeEdge::Bottom:bottom += dy; break;
        case ResizeEdge::TopLeft:     left += dx; top += dy; break;
        case ResizeEdge::TopRight:    right += dx; top += dy; break;
        case ResizeEdge::BottomLeft:  left += dx; bottom += dy; break;
        case ResizeEdge::BottomRight: right += dx; bottom += dy; break;
        default: break;
    }
    if (right - left < minW) {              // 锁最小宽，固定未拖动的那条边
        if (e==ResizeEdge::Left||e==ResizeEdge::TopLeft||e==ResizeEdge::BottomLeft) left = right - minW;
        else right = left + minW;
    }
    if (bottom - top < minH) {
        if (e==ResizeEdge::Top||e==ResizeEdge::TopLeft||e==ResizeEdge::TopRight) top = bottom - minH;
        else bottom = top + minH;
    }
    return { left, top, right - left, bottom - top };
}
}
