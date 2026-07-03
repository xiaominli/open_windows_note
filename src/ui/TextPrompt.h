#pragma once
#include <afxwin.h>
namespace own_ui {
// 极简模态文本输入：回车确认返回 true 并写回 io；ESC/关闭返回 false。
// allowEmpty=false（默认）时，回车但输入为空视同取消，返回 false；
// allowEmpty=true 时，回车提交空字符串也算确认，返回 true（用于「空=清除」语义的调用方）。
bool promptText(CWnd* parent, const CString& title, CString& io, bool allowEmpty = false);
}
