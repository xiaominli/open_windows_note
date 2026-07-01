#pragma once
#include <afxwin.h>
#include <functional>

// Hidden top-level message-owner window: owns app lifetime, receives global
// hotkeys, and (from Task 11) hosts the set of note windows. P2 scaffold.
class CAppHostWindow : public CWnd {
public:
    static const UINT kHotkeyQuit = 1;
    static const UINT kHotkeyNew  = 2;
    static const UINT kHotkeyNewChecklist = 3;
    static const UINT kHotkeyNewDrawing = 4;
    static const UINT kHotkeyManager = 5;
    std::function<void()> onNewNote;
    std::function<void()> onNewChecklist;
    std::function<void()> onNewDrawing;
    std::function<void()> onQuit;
    std::function<void()> onToggleManager;
    bool Create();                 // create hidden top-level window + register hotkeys
protected:
    afx_msg void OnHotKey(UINT idHotKey, UINT fuModifiers, UINT vk);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
};
