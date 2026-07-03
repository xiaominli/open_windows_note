#pragma once
#include <afxwin.h>
namespace own_ui {
// 全应用统一 UI 字体（微软雅黑）。heightPx 为像素字高。
// 返回进程级缓存的 HFONT，调用方不得 DeleteObject。
HFONT uiFont(int heightPx);
}
