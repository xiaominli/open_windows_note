#include "ui/SettingsDialog.h"
#include "ui/UiFont.h"
#include "data/SettingsStore.h"
#include "data/NoteStore.h"
#include "domain/ThemeRules.h"
#include "services/AutostartManager.h"
#include "services/HotkeyManager.h"
#include <string>
#include <vector>

namespace own_ui {

static CString u8ToWide(const std::string& s) {
    if (s.empty()) return CString();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CString w;
    if (n > 0) { ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.GetBuffer(n), n); w.ReleaseBuffer(n); }
    return w;
}

static const int kRowH = 34, kPad = 12, kWidth = 360;

class CSettingsWnd : public CWnd {
public:
    own::Database* db = nullptr;
    own::NoteStore* store = nullptr;
    HotkeyManager* hotkeys = nullptr;
    HWND hotkeyHwnd = nullptr;
    bool done = false;
    int rowCount() const { return 3; }               // Task 6 扩展为 3 + 热键行数
    CRect rowRect(int i) const { return CRect(kPad, kPad + i * kRowH, kWidth - kPad, kPad + (i + 1) * kRowH - 4); }

    BOOL PreTranslateMessage(MSG* m) override {
        if (m->message == WM_KEYDOWN && m->wParam == VK_ESCAPE
            && (m->hwnd == m_hWnd || ::IsChild(m_hWnd, m->hwnd))) { done = true; return TRUE; }
        return CWnd::PreTranslateMessage(m);
    }
    void PostNcDestroy() override {}                  // 栈对象

    CString rowLabel(int i) {
        own::SettingsStore st(*db);
        if (i == 0) {                                 // 默认主题：<名>
            int64_t tid = st.getInt("default_theme_id", 0);
            CString name = _T("(\x672A\x8BBE)");      // (未设)
            if (tid != 0) { if (auto t = store->getTheme(tid)) name = u8ToWide(t->name); }
            return _T("\x9ED8\x8BA4\x4E3B\x9898\xFF1A") + name;                    // 默认主题：
        }
        if (i == 1) {                                 // 默认透明度：<n%>
            int op = st.getInt("default_opacity", 255);
            int pct = (op * 100 + 127) / 255;
            CString s; s.Format(_T("\x9ED8\x8BA4\x900F\x660E\x5EA6\xFF1A%d%%"), pct); // 默认透明度：
            return s;
        }
        bool on = own_svc::autostartIsEnabled();      // 开机自启：开/关
        return CString(_T("\x5F00\x673A\x81EA\x542F\xFF1A")) + (on ? _T("\x5F00") : _T("\x5173"));
    }
    void clickRow(int i) {
        own::SettingsStore st(*db);
        if (i == 0) {
            auto themes = store->allThemes();
            int64_t cur = st.getInt("default_theme_id", 0);
            int64_t next = own::nextThemeId(themes, cur);
            if (next != 0) st.setInt("default_theme_id", (int)next);
        } else if (i == 1) {
            static const int steps[] = { 255, 204, 153, 102 };
            int cur = st.getInt("default_opacity", 255);
            int idx = 0; for (int k = 0; k < 4; ++k) if (steps[k] == cur) { idx = k; break; }
            st.setInt("default_opacity", steps[(idx + 1) % 4]);
        } else if (i == 2) {
            own_svc::autostartSetEnabled(!own_svc::autostartIsEnabled());
        }
        Invalidate(FALSE);
    }

protected:
    afx_msg void OnPaint() {
        CPaintDC dc(this);
        CRect rc; GetClientRect(&rc);
        dc.FillSolidRect(rc, RGB(45, 45, 48));
        dc.SetBkMode(TRANSPARENT);
        CFont* old = dc.SelectObject(CFont::FromHandle(uiFont(16)));
        for (int i = 0; i < rowCount(); ++i) {
            CRect r = rowRect(i);
            dc.FillSolidRect(r, RGB(62, 62, 66));
            dc.Draw3dRect(r, RGB(90, 90, 96), RGB(30, 30, 32));
            dc.SetTextColor(RGB(0xE0, 0xE0, 0xE0));
            CRect tr = r; tr.left += 10;
            dc.DrawText(rowLabel(i), tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        dc.SelectObject(old);
    }
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    afx_msg void OnLButtonUp(UINT, CPoint pt) {
        for (int i = 0; i < rowCount(); ++i)
            if (rowRect(i).PtInRect(pt)) { clickRow(i); return; }
    }
    afx_msg void OnClose() { done = true; }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CSettingsWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

static CSettingsWnd* s_open = nullptr;                    // 重入守卫：防止设置窗口被打开多次

void showSettingsDialog(own::Database& db, own::NoteStore& store,
                        HotkeyManager& hotkeys, HWND hotkeyHwnd) {
    if (s_open && s_open->GetSafeHwnd()) { s_open->SetForegroundWindow(); return; }
    CSettingsWnd w;
    w.db = &db; w.store = &store; w.hotkeys = &hotkeys; w.hotkeyHwnd = hotkeyHwnd;
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW));
    int h = kPad * 2 + w.rowCount() * kRowH + 30;
    CRect r(0, 0, kWidth, h);
    r.OffsetRect(400, 260);
    if (!w.CreateEx(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, cls,
               _T("\x8BBE\x7F6E"),                                   // 设置
               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, r, nullptr, 0)) return;
    s_open = &w;
    MSG msg;
    for (;;) {
        if (w.done) break;
        BOOL got = ::GetMessage(&msg, nullptr, 0, 0);
        if (got == 0) {
            // GetMessage 返回 0（WM_QUIT）：TextPrompt.cpp 的手写循环未处理此情形，会把
            // WM_QUIT 吞掉导致外层 CWinApp::Run 永不退出；这里补发一次，防止应用无法关闭。
            ::PostQuitMessage((int)msg.wParam);
            break;
        }
        if (!w.PreTranslateMessage(&msg)) { ::TranslateMessage(&msg); ::DispatchMessage(&msg); }
    }
    w.DestroyWindow();
    s_open = nullptr;
}

} // namespace own_ui
