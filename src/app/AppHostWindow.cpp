#include "app/AppHostWindow.h"

BEGIN_MESSAGE_MAP(CAppHostWindow, CWnd)
    ON_WM_HOTKEY()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

bool CAppHostWindow::Create() {
    LPCTSTR cls = AfxRegisterWndClass(0);
    if (!CreateEx(0, cls, _T("OwnAppHost"), WS_POPUP, CRect(0, 0, 0, 0), NULL, 0))
        return false;
    // Temporary hotkeys Ctrl+Alt+Q / Ctrl+Alt+N (P5 replaces the whole scheme).
    ::RegisterHotKey(m_hWnd, kHotkeyQuit, MOD_CONTROL | MOD_ALT, 'Q');
    ::RegisterHotKey(m_hWnd, kHotkeyNew,  MOD_CONTROL | MOD_ALT, 'N');
    ::RegisterHotKey(m_hWnd, kHotkeyNewChecklist, MOD_CONTROL | MOD_ALT, '2');
    ::RegisterHotKey(m_hWnd, kHotkeyNewDrawing, MOD_CONTROL | MOD_ALT, '3');
    ::RegisterHotKey(m_hWnd, kHotkeyManager, MOD_CONTROL | MOD_ALT, 'M');
    return true;
}

void CAppHostWindow::OnHotKey(UINT idHotKey, UINT, UINT) {
    if (idHotKey == kHotkeyQuit) { if (onQuit) onQuit(); else ::PostQuitMessage(0); }
    else if (idHotKey == kHotkeyNew) { if (onNewNote) onNewNote(); }
    else if (idHotKey == kHotkeyNewChecklist) { if (onNewChecklist) onNewChecklist(); }
    else if (idHotKey == kHotkeyNewDrawing) { if (onNewDrawing) onNewDrawing(); }
    else if (idHotKey == kHotkeyManager) { if (onToggleManager) onToggleManager(); }
}

void CAppHostWindow::OnDestroy() {
    ::UnregisterHotKey(m_hWnd, kHotkeyQuit);
    ::UnregisterHotKey(m_hWnd, kHotkeyNew);
    ::UnregisterHotKey(m_hWnd, kHotkeyNewChecklist);
    ::UnregisterHotKey(m_hWnd, kHotkeyNewDrawing);
    ::UnregisterHotKey(m_hWnd, kHotkeyManager);
    CWnd::OnDestroy();
}
