#include "ui/MonitorEnum.h"
#include <windows.h>
namespace own {
static BOOL CALLBACK cb(HMONITOR h, HDC, LPRECT, LPARAM p) {
    MONITORINFO mi{ sizeof(mi) };
    if (::GetMonitorInfo(h, &mi)) {
        RECT r = mi.rcWork;
        reinterpret_cast<std::vector<RectI>*>(p)->push_back(
            { r.left, r.top, r.right - r.left, r.bottom - r.top });
    }
    return TRUE;
}
std::vector<RectI> enumMonitorWorkAreas() {
    std::vector<RectI> out;
    ::EnumDisplayMonitors(nullptr, nullptr, cb, reinterpret_cast<LPARAM>(&out));
    return out;
}
}
