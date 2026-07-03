#pragma once
#include <afxwin.h>
#include <functional>
#include "services/TrayIcon.h"

// Hidden top-level message-owner window: owns app lifetime, receives global
// hotkeys, and hosts the tray icon. Hotkey registration is done externally by
// HotkeyManager (using this window's HWND); dispatch stays here by id.
class CAppHostWindow : public CWnd {
public:
    static const UINT kHotkeyQuit = 1;
    static const UINT kHotkeyNew  = 2;
    static const UINT kHotkeyNewChecklist = 3;
    static const UINT kHotkeyNewDrawing = 4;
    static const UINT kHotkeyManager = 5;
    static const UINT kHotkeyToggleAll = 6;
    std::function<void()> onNewNote;
    std::function<void()> onNewChecklist;
    std::function<void()> onNewDrawing;
    std::function<void()> onQuit;
    std::function<void()> onToggleManager;
    std::function<void()> onToggleAll;
    std::function<void(bool)> onSetAllVisible;   // 托盘“显示/隐藏全部”
    std::function<void()> onToggleAutostart;     // 托盘“开机自启”切换
    std::function<bool()> isAutostartEnabled;    // 菜单勾选状态查询
    std::function<void()> onOpenSettings;        // 托盘「设置…」
    static const UINT kReminderTimerId = 1;
    std::function<void()> onReminderTick;        // 30s 提醒轮询滴答
    void startReminderTimer();
    bool Create();                 // create hidden top-level window
    bool createTray();             // add tray icon (call after Create)
protected:
    afx_msg void OnHotKey(UINT idHotKey, UINT fuModifiers, UINT vk);
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg LRESULT OnTrayCallback(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTaskbarCreated(WPARAM wParam, LPARAM lParam);
    void showTrayMenu();
    DECLARE_MESSAGE_MAP()
private:
    TrayIcon m_tray;
};
