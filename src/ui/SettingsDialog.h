#pragma once
#include <afxwin.h>
namespace own { class Database; class NoteStore; }
class HotkeyManager;
namespace own_ui {
// 自绘设置弹层：默认主题/默认透明度/开机自启（+P7 Task6 的热键改键区）。
// 行级点击即时生效；ESC/关闭退出。模态（手写消息循环）。
void showSettingsDialog(own::Database& db, own::NoteStore& store,
                        HotkeyManager& hotkeys, HWND hotkeyHwnd);
}
