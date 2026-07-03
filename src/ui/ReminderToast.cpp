#include "ui/ReminderToast.h"
#include "app/NoteWindowHost.h"
#include "data/NoteStore.h"
#include "domain/ReminderRules.h"
#include "domain/NoteListFormat.h"
#include <mmsystem.h>
#include <ctime>

std::vector<bool> CReminderToast::s_slotUsed;
static const int kW = 300, kH = 96;

static CString u8ToWide(const std::string& s) {
    if (s.empty()) return CString();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CString w;
    if (n > 0) { ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.GetBuffer(n), n); w.ReleaseBuffer(n); }
    return w;
}
static void playReminderSound(const std::string& pathU8) {
    if (!pathU8.empty()) {
        CString w = u8ToWide(pathU8);
        if (::PlaySoundW(w, nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
    }
    ::MessageBeep(MB_OK);   // 规格 §6：提示音缺失回落
}

BEGIN_MESSAGE_MAP(CReminderToast, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

bool CReminderToast::show(const own::Reminder& r, const own::Note& note,
                          own::NoteStore* store, INoteWindowHost* host,
                          std::function<void(int64_t)> onClosed) {
    CReminderToast* t = new CReminderToast();
    t->m_rem = r; t->m_note = note; t->m_store = store; t->m_host = host;
    t->m_onClosed = std::move(onClosed);
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW));
    RECT wa{ 0, 0, 1280, 800 };
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int slot = 0;
    while (slot < (int)s_slotUsed.size() && s_slotUsed[slot]) ++slot;
    t->m_slot = slot;
    CRect rc(0, 0, kW, kH);
    rc.OffsetRect(wa.right - kW - 12, wa.bottom - kH - 12 - slot * (kH + 8));
    if (!t->CreateEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, cls, _T("reminder"),
                     WS_POPUP, rc, nullptr, 0)) { delete t; return false; }
    if (slot >= (int)s_slotUsed.size()) s_slotUsed.resize(slot + 1, false);
    s_slotUsed[slot] = true;
    t->ShowWindow(SW_SHOWNOACTIVATE);
    t->UpdateWindow();
    playReminderSound(r.soundPath);
    return true;
}

CRect CReminderToast::btnRect(int i) const {
    CRect c; GetClientRect(&c);
    const int bw = 90, bh = 26, gap = 8;
    int x0 = c.right - 3 * bw - 2 * gap - 8;
    return CRect(x0 + i * (bw + gap), c.bottom - bh - 8,
                 x0 + i * (bw + gap) + bw, c.bottom - 8);
}

BOOL CReminderToast::OnEraseBkgnd(CDC*) { return TRUE; }

void CReminderToast::OnPaint() {
    CPaintDC dc(this);
    CRect c; GetClientRect(&c);
    dc.FillSolidRect(c, RGB(45, 45, 48));
    dc.Draw3dRect(c, RGB(90, 90, 96), RGB(20, 20, 22));
    dc.SetBkMode(TRANSPARENT);
    CFont* old = dc.SelectObject(CFont::FromHandle((HFONT)::GetStockObject(DEFAULT_GUI_FONT)));
    dc.SetTextColor(RGB(0xF2, 0xD2, 0x4A));
    CRect hd(10, 8, c.right - 10, 26);
    dc.DrawText(_T("\x23F0 \x63D0\x9192"), hd, DT_SINGLELINE | DT_VCENTER);   // ⏰ 提醒
    dc.SetTextColor(RGB(0xE0, 0xE0, 0xE0));
    CRect bd(10, 28, c.right - 10, 52);
    CString title = u8ToWide(own::noteTitleText(m_note));
    dc.DrawText(title, bd, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    static const LPCTSTR labels[3] = {
        _T("\x6253\x5F00"),                    // 打开
        _T("\x8D2A\x7761 10 \x5206"),          // 贪睡 10 分
        _T("\x5173\x95ED"),                    // 关闭
    };
    for (int i = 0; i < 3; ++i) {
        CRect b = btnRect(i);
        dc.FillSolidRect(b, RGB(62, 62, 66));
        dc.Draw3dRect(b, RGB(90, 90, 96), RGB(30, 30, 32));
        dc.DrawText(labels[i], b, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
    dc.SelectObject(old);
}

void CReminderToast::OnLButtonUp(UINT, CPoint pt) {
    int64_t now = (int64_t)time(nullptr);
    if (btnRect(0).PtInRect(pt)) {                 // 打开：开便签 + 按"关闭"落库
        if (m_host) m_host->openOrFocusNote(m_note.id);
        if (m_store) m_store->updateReminder(own::resolveReminderDismiss(m_rem, now));
        closeToast();
    } else if (btnRect(1).PtInRect(pt)) {          // 贪睡 10 分钟
        if (m_store) m_store->updateReminder(own::resolveReminderSnooze(m_rem, now, 10));
        closeToast();
    } else if (btnRect(2).PtInRect(pt)) {          // 关闭：一次性禁用 / 重复推进
        if (m_store) m_store->updateReminder(own::resolveReminderDismiss(m_rem, now));
        closeToast();
    }
}

void CReminderToast::closeToast() {
    if (m_onClosed) m_onClosed(m_rem.id);
    DestroyWindow();                               // → PostNcDestroy → delete this
}

void CReminderToast::OnDestroy() {
    if (m_slot >= 0 && m_slot < (int)s_slotUsed.size()) s_slotUsed[m_slot] = false;
    CWnd::OnDestroy();
}
