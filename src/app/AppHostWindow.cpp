#include "app/AppHostWindow.h"
#include "services/AutostartManager.h"

static const UINT WM_TRAY_CALLBACK = WM_APP + 1;
static UINT WM_TASKBARCREATED = ::RegisterWindowMessageW(L"TaskbarCreated");

BEGIN_MESSAGE_MAP(CAppHostWindow, CWnd)
    ON_WM_HOTKEY()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_MESSAGE(WM_TRAY_CALLBACK, &CAppHostWindow::OnTrayCallback)
    ON_REGISTERED_MESSAGE(WM_TASKBARCREATED, &CAppHostWindow::OnTaskbarCreated)
END_MESSAGE_MAP()

bool CAppHostWindow::Create() {
    LPCTSTR cls = AfxRegisterWndClass(0);
    if (!CreateEx(0, cls, _T("OwnAppHost"), WS_POPUP, CRect(0, 0, 0, 0), NULL, 0))
        return false;
    // 热键由 HotkeyManager 用本窗 HWND 外部注册（设置驱动、冲突安全）。
    return true;
}

bool CAppHostWindow::createTray() {
    HICON icon = ::LoadIcon(nullptr, IDI_APPLICATION);
    return m_tray.add(m_hWnd, WM_TRAY_CALLBACK, 1, icon, L"open_windows_note");
}

void CAppHostWindow::OnHotKey(UINT idHotKey, UINT, UINT) {
    if (idHotKey == kHotkeyQuit) { if (onQuit) onQuit(); else ::PostQuitMessage(0); }
    else if (idHotKey == kHotkeyNew) { if (onNewNote) onNewNote(); }
    else if (idHotKey == kHotkeyNewChecklist) { if (onNewChecklist) onNewChecklist(); }
    else if (idHotKey == kHotkeyNewDrawing) { if (onNewDrawing) onNewDrawing(); }
    else if (idHotKey == kHotkeyManager) { if (onToggleManager) onToggleManager(); }
    else if (idHotKey == kHotkeyToggleAll) { if (onToggleAll) onToggleAll(); }
}

LRESULT CAppHostWindow::OnTaskbarCreated(WPARAM, LPARAM) {
    m_tray.reAdd();   // 资源管理器重启后重建图标
    return 0;
}

LRESULT CAppHostWindow::OnTrayCallback(WPARAM, LPARAM lParam) {
    if (LOWORD(lParam) == WM_LBUTTONDBLCLK) { if (onToggleManager) onToggleManager(); }
    else if (LOWORD(lParam) == WM_RBUTTONUP) { showTrayMenu(); }
    return 0;
}

void CAppHostWindow::showTrayMenu() {
    CMenu menu; menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 1, _T("\x65B0\x5EFA\x4FBF\x7B7E"));         // 新建便签
    menu.AppendMenu(MF_STRING, 2, _T("\x663E\x793A\x7BA1\x7406\x5668"));   // 显示管理器
    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    menu.AppendMenu(MF_STRING, 3, _T("\x663E\x793A\x5168\x90E8"));         // 显示全部
    menu.AppendMenu(MF_STRING, 4, _T("\x9690\x85CF\x5168\x90E8"));         // 隐藏全部
    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    UINT autostartFlag = MF_STRING | ((isAutostartEnabled && isAutostartEnabled()) ? MF_CHECKED : MF_UNCHECKED);
    menu.AppendMenu(autostartFlag, 5, _T("\x5F00\x673A\x81EA\x542F"));     // 开机自启
    menu.AppendMenu(MF_STRING, 7, _T("\x8BBE\x7F6E\x2026"));               // 设置…
    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    menu.AppendMenu(MF_STRING, 6, _T("\x9000\x51FA"));                     // 退出
    CPoint pt; ::GetCursorPos(&pt);
    ::SetForegroundWindow(m_hWnd);   // 托盘菜单必需，否则菜单不消失
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    ::PostMessage(m_hWnd, WM_NULL, 0, 0);
    switch (cmd) {
        case 1: if (onNewNote) onNewNote(); break;
        case 2: if (onToggleManager) onToggleManager(); break;
        case 3: if (onSetAllVisible) onSetAllVisible(true); break;
        case 4: if (onSetAllVisible) onSetAllVisible(false); break;
        case 5: if (onToggleAutostart) onToggleAutostart(); break;
        case 6: if (onQuit) onQuit(); break;
        case 7: if (onOpenSettings) onOpenSettings(); break;
    }
}

void CAppHostWindow::OnDestroy() {
    KillTimer(kReminderTimerId);   // 显式清理（窗口销毁本会自动杀，防御式）
    m_tray.remove();
    CWnd::OnDestroy();
}

void CAppHostWindow::startReminderTimer() {
    SetTimer(kReminderTimerId, 30 * 1000, nullptr);   // 规格 §5：UI 线程 SetTimer 轮询
}
void CAppHostWindow::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kReminderTimerId) {
        if (onReminderTick) onReminderTick();
        return;
    }
    CWnd::OnTimer(nIDEvent);
}
