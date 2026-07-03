#include "ui/SettingsDialog.h"
#include "ui/UiFont.h"
#include "data/SettingsStore.h"
#include "data/NoteStore.h"
#include "domain/ThemeRules.h"
#include "services/AutostartManager.h"
#include "services/HotkeyManager.h"
#include "domain/Hotkey.h"
#include "ui/TextPrompt.h"
#include "ui/TextContentView.h"
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
    static CString hkDisplayName(const std::string& name) {
        if (name == "new")           return _T("\x70ED\x952E\xB7\x65B0\x5EFA\x4FBF\x7B7E");   // 热键·新建便签
        if (name == "new_checklist") return _T("\x70ED\x952E\xB7\x65B0\x5EFA\x6E05\x5355");   // 热键·新建清单
        if (name == "new_drawing")   return _T("\x70ED\x952E\xB7\x65B0\x5EFA\x6D82\x9E26");   // 热键·新建涂鸦
        if (name == "manager")       return _T("\x70ED\x952E\xB7\x7BA1\x7406\x5668");         // 热键·管理器
        if (name == "toggle_all")    return _T("\x70ED\x952E\xB7\x663E\x9690\x5168\x90E8");   // 热键·显隐全部
        if (name == "quit")          return _T("\x70ED\x952E\xB7\x9000\x51FA");               // 热键·退出
        return CString(name.c_str());
    }
    std::string bindingText(const HkBinding& b) {     // settings 覆盖优先
        own::SettingsStore st(*db);
        return st.getString("hotkey." + b.name, b.defBinding);
    }
    int rowCount() const { return 4 + (int)hotkeys->bindings().size(); }   // 4 通用行 + 热键行数
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
        if (i == 2) {                                  // 默认字号：N pt
            int pt = st.getInt("default_font_pt", 10);
            CString s; s.Format(_T("\x9ED8\x8BA4\x5B57\x53F7\xFF1A%d pt"), pt);   // 默认字号：
            return s;
        }
        if (i == 3) {                                  // 开机自启：开/关
            bool on = own_svc::autostartIsEnabled();
            return CString(_T("\x5F00\x673A\x81EA\x542F\xFF1A")) + (on ? _T("\x5F00") : _T("\x5173"));
        }
        const auto& b = hotkeys->bindings()[i - 4];
        return hkDisplayName(b.name) + _T("\xFF1A") + CString(bindingText(b).c_str()); // ：
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
            static const int pts[] = { 9, 10, 11, 12, 14 };
            int cur = st.getInt("default_font_pt", 10);
            int idx = 0; for (int k = 0; k < 5; ++k) if (pts[k] == cur) { idx = k; break; }
            int next = pts[(idx + 1) % 5];
            st.setInt("default_font_pt", next);
            CTextContentView::SetDefaultFontPt(next);          // 即时生效于此后新输入/新便签
        } else if (i == 3) {
            own_svc::autostartSetEnabled(!own_svc::autostartIsEnabled());
        } else if (i >= 4) {
            const auto& bs = hotkeys->bindings();
            const auto& b = bs[i - 4];
            CString io(bindingText(b).c_str());
            if (!own_ui::promptText(this, _T("\x8F93\x5165\x70ED\x952E (\x5982 Ctrl+Alt+N)"), io)) return; // 输入热键 (如 …)
            CStringA a(io);                                    // 热键串全 ASCII
            std::string s((LPCSTR)a);
            own::Hotkey parsed;
            if (!own::parseHotkey(s, parsed)) {
                AfxMessageBox(_T("\x70ED\x952E\x683C\x5F0F\x65E0\x6548"));            // 热键格式无效
                return;
            }
            std::vector<own::Hotkey> all;                       // 冲突检测：候选 + 其余现值
            all.push_back(parsed);
            for (size_t k = 0; k < bs.size(); ++k) {
                if ((int)k == i - 4) continue;
                own::Hotkey other;
                if (own::parseHotkey(st.getString("hotkey." + bs[k].name, bs[k].defBinding), other))
                    all.push_back(other);
            }
            if (!own::findHotkeyConflicts(all).empty()) {
                AfxMessageBox(_T("\x4E0E\x5176\x5B83\x70ED\x952E\x51B2\x7A81"));      // 与其它热键冲突
                return;
            }
            st.setString("hotkey." + b.name, own::formatHotkey(parsed));   // 规范化写回
            hotkeys->unregisterAll(hotkeyHwnd);
            hotkeys->loadAndRegister(hotkeyHwnd, st);
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
