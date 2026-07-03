#include "ui/FormatBarLayout.h"
namespace own {
RectI formatBarRect(RectI client, int titleHeight, FormatBarMetrics m) {
    return { client.x, client.y + titleHeight, client.w, m.height };
}
RectI formatBarButton(RectI bar, FormatBarMetrics m, int index) {
    int x = bar.x + m.padX + index * (m.btnSize + m.gap);
    int y = bar.y + (bar.h - m.btnSize) / 2;
    return { x, y, m.btnSize, m.btnSize };
}
int hitTestFormatBar(RectI bar, FormatBarMetrics m, int count, int px, int py) {
    for (int i = 0; i < count; ++i) {
        RectI b = formatBarButton(bar, m, i);
        if (px >= b.x && px < b.x + b.w && py >= b.y && py < b.y + b.h) return i;
    }
    return -1;
}
static int snapIdx(int twips) {                       // 不小于 twips 的最近档；超顶取顶
    static const int ladder[] = { 160, 180, 200, 220, 240, 280, 320, 360, 480 };
    for (int i = 0; i < 9; ++i) if (twips <= ladder[i]) return i;
    return 8;
}
int fontSizeStep(int twips, bool up) {                // 语义：先 snap 到档位，再走一步，两端夹住
    static const int ladder[] = { 160, 180, 200, 220, 240, 280, 320, 360, 480 };
    int idx = snapIdx(twips);
    if (up)  { if (idx < 8) ++idx; }
    else     { if (idx > 0) --idx; }
    return ladder[idx];
}
uint32_t nextPaletteColor(uint32_t cur) {
    static const uint32_t pal[] = { 0x202020, 0xC0392B, 0x1F6FBF, 0x1E8449, 0xB7770D };
    const int n = sizeof(pal) / sizeof(pal[0]);
    for (int i = 0; i < n; ++i)
        if (pal[i] == cur) return pal[(i + 1) % n];
    return pal[0];
}
}
