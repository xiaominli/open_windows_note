#pragma once
#include <afxwin.h>
namespace own_ui {
// 极简模态文本输入：回车确认返回 true 并写回 io；ESC/关闭返回 false。
bool promptText(CWnd* parent, const CString& title, CString& io);
}
