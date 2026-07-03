#pragma once
#include <cstdint>
#include "domain/Models.h"
namespace own {
// 文本便签格式工具条：纯布局 + 字号阶梯 + 文字色调色板（无 Win32 依赖，进 tests）
enum class FmtOp { Bold, Italic, Underline, Strike, SizeDown, SizeUp, TextColor };
constexpr int kFmtOpCount = 7;
struct FormatBarMetrics { int height; int btnSize; int padX; int gap; };
RectI formatBarRect(RectI client, int titleHeight, FormatBarMetrics m);
RectI formatBarButton(RectI bar, FormatBarMetrics m, int index);
int hitTestFormatBar(RectI bar, FormatBarMetrics m, int count, int px, int py);
int fontSizeStep(int twips, bool up);            // 阶梯 160..480 twip，走一步并夹住
uint32_t nextPaletteColor(uint32_t cur);         // 0xRRGGBB 调色板循环
}
