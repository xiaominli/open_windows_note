#include "ui/UiFont.h"
#include <map>
namespace own_ui {
HFONT uiFont(int heightPx) {
    static std::map<int, HFONT> cache;   // 进程生命周期，退出由系统回收
    auto it = cache.find(heightPx);
    if (it != cache.end()) return it->second;
    HFONT f = ::CreateFontW(-heightPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                            L"微软雅黑");   // 微软雅黑
    cache[heightPx] = f;
    return f;
}
}
