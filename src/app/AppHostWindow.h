#pragma once
#include <afxwin.h>

// Hidden top-level message-owner window: owns app lifetime, receives global
// hotkeys, and (from Task 11) hosts the set of note windows. P2 scaffold.
class CAppHostWindow : public CWnd {
public:
    static const UINT kHotkeyQuit = 1;
    bool Create();                 // create hidden top-level window + register quit hotkey
protected:
    afx_msg void OnHotKey(UINT idHotKey, UINT fuModifiers, UINT vk);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
};
