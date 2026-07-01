#pragma once
#include "domain/Models.h"
namespace own {
enum class ResizeEdge { None, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };
ResizeEdge hitTestResizeEdge(RectI client, int px, int py, int margin);
RectI applyResize(RectI start, ResizeEdge edge, int dx, int dy, int minW, int minH);
}
