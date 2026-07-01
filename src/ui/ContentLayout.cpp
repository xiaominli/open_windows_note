#include "ui/ContentLayout.h"
namespace own {
RectI noteContentRect(RectI client, int titleHeight, int resizeMargin) {
    int x = client.x + resizeMargin;
    int y = client.y + titleHeight;
    int w = client.w - 2 * resizeMargin;
    int h = client.h - titleHeight - resizeMargin;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    return { x, y, w, h };
}
}
