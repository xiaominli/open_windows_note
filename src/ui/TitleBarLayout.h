#pragma once
#include "domain/Models.h"
namespace own {
enum class TitleHit { None, Drag, Close, Pin, Roll, Opacity, Theme };
struct TitleBarMetrics { int height; int btnSize; int btnGap; int pad; };
struct TitleBarRects { RectI titleBar, closeBtn, pinBtn, rollBtn, opacityBtn, themeBtn, dragArea; };
TitleBarRects layoutTitleBar(RectI client, TitleBarMetrics m);
TitleHit hitTestTitleBar(const TitleBarRects& r, int px, int py);
}
