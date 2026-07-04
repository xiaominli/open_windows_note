#include "ui/TextPrompt.h"
#include "ui/UiFont.h"
namespace own_ui {
class CPromptWnd : public CWnd {
public:
    CEdit m_edit;
    bool m_done = false;
    bool m_ok = false;
    BOOL PreTranslateMessage(MSG* pMsg) override {
        if (pMsg->message == WM_KEYDOWN) {
            if (pMsg->wParam == VK_RETURN) { m_ok = true; m_done = true; return TRUE; }
            if (pMsg->wParam == VK_ESCAPE) { m_ok = false; m_done = true; return TRUE; }
        }
        return CWnd::PreTranslateMessage(pMsg);
    }
    void PostNcDestroy() override {}   // 栈上对象，禁止 delete this
};
bool promptText(CWnd* parent, const CString& title, CString& io, bool allowEmpty) {
    CPromptWnd w;
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE+1));
    CRect r(0, 0, 300, 90);
    if (parent && parent->GetSafeHwnd()) {
        CRect pr; parent->GetWindowRect(&pr);
        r.OffsetRect(pr.left + (pr.Width() - 300) / 2, pr.top + (pr.Height() - 90) / 2);
    } else {
        r.OffsetRect(400, 300);
    }
    if (!w.CreateEx(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, cls, title,
               WS_POPUP | WS_CAPTION | WS_VISIBLE, r, parent, 0)) return false;
    w.m_edit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                    CRect(12, 12, 288, 40), &w, 0x3301);
    w.m_edit.SetFont(CFont::FromHandle(uiFont(16)));
    w.m_edit.SetWindowText(io);
    w.m_edit.SetFocus();
    w.m_edit.SetSel(0, -1);
    if (parent) parent->EnableWindow(FALSE);
    MSG msg;
    for (;;) {
        if (w.m_done) break;
        BOOL got = ::GetMessage(&msg, nullptr, 0, 0);
        if (got == 0) {
            // GetMessage 返回 0（WM_QUIT）：补发一次，避免被这里的手写循环吞掉，
            // 否则外层 CWinApp::Run 收不到 WM_QUIT，应用将无法退出。
            ::PostQuitMessage((int)msg.wParam);
            break;
        }
        if (!w.PreTranslateMessage(&msg)) { ::TranslateMessage(&msg); ::DispatchMessage(&msg); }
    }
    if (parent) parent->EnableWindow(TRUE);
    if (w.m_ok) w.m_edit.GetWindowText(io);
    w.DestroyWindow();
    return w.m_ok && (allowEmpty || !io.IsEmpty());
}
}
