#include "ui/TitleBarLayout.h"
namespace own {
static bool inRect(const RectI& r, int px, int py) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}
TitleBarRects layoutTitleBar(RectI client, TitleBarMetrics m) {
    TitleBarRects r{};
    r.titleBar = { client.x, client.y, client.w, m.height };
    int y = client.y + (m.height - m.btnSize) / 2;
    int x = client.x + client.w - m.pad - m.btnSize;   // close 最右
    auto place = [&](RectI& btn){ btn = { x, y, m.btnSize, m.btnSize }; x -= (m.btnSize + m.btnGap); };
    place(r.closeBtn);
    place(r.rollBtn);
    place(r.pinBtn);
    place(r.opacityBtn);
    place(r.themeBtn);
    int dragRight = r.themeBtn.x - m.btnGap;            // 拖动区到最左钮之前
    r.dragArea = { client.x, client.y, dragRight - client.x, m.height };
    return r;
}
TitleHit hitTestTitleBar(const TitleBarRects& r, int px, int py) {
    if (inRect(r.closeBtn, px, py))   return TitleHit::Close;
    if (inRect(r.rollBtn, px, py))    return TitleHit::Roll;
    if (inRect(r.pinBtn, px, py))     return TitleHit::Pin;
    if (inRect(r.opacityBtn, px, py)) return TitleHit::Opacity;
    if (inRect(r.themeBtn, px, py))   return TitleHit::Theme;
    if (inRect(r.dragArea, px, py))   return TitleHit::Drag;
    return TitleHit::None;
}
}
