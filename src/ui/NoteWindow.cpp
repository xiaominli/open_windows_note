#include "ui/NoteWindow.h"
#include "ui/MonitorEnum.h"
#include "domain/Geometry.h"
#include "data/NoteStore.h"
#include <gdiplus.h>
using namespace Gdiplus;

static const own::TitleBarMetrics kTitleMetrics{ 28, 20, 4, 4 };

BEGIN_MESSAGE_MAP(CNoteWindow, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

bool CNoteWindow::Create(const own::Note& note, own::NoteStore* store) {
    m_note = note; m_store = store;
    // 越界钳制到可见工作区
    own::RectI clamped = own::clampRectToWorkArea(note.rect, own::enumMonitorWorkAreas());
    m_note.rect = clamped;
    LPCTSTR cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW));
    if (!CreateEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, cls, _T("Note"),
                  WS_POPUP, CRect(clamped.x, clamped.y, clamped.x+clamped.w, clamped.y+clamped.h),
                  nullptr, 0))
        return false;
    ::SetLayeredWindowAttributes(m_hWnd, 0, (BYTE)m_note.opacity, LWA_ALPHA);
    ShowWindow(SW_SHOWNOACTIVATE);
    return true;
}
own::TitleBarRects CNoteWindow::layout() const {
    CRect rc; GetClientRect(&rc);
    return own::layoutTitleBar({0,0,rc.Width(), rc.Height()}, kTitleMetrics);
}
BOOL CNoteWindow::OnEraseBkgnd(CDC*) { return TRUE; }   // 防闪烁，全在 OnPaint 画

void CNoteWindow::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    // 双缓冲
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* old = mem.SelectObject(&bmp);
    {
        Graphics g(mem.GetSafeHdc());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        // 背景（主题色，暂用 note 的 theme 对应色的简化：黄底，P3/主题接入后替换）
        SolidBrush bg(Color(255, 0xFF, 0xF7, 0xB0));
        g.FillRectangle(&bg, 0, 0, rc.Width(), rc.Height());
        // 标题栏
        auto L = own::layoutTitleBar({0,0,rc.Width(),rc.Height()}, kTitleMetrics);
        SolidBrush title(Color(255, 0xF2, 0xD2, 0x4A));
        g.FillRectangle(&title, L.titleBar.x, L.titleBar.y, L.titleBar.w, L.titleBar.h);
        // 按钮图标（简单符号：× 关闭，─ 卷起，pin 小方块，○ 透明）
        Pen pen(Color(255,0x40,0x40,0x40), 2.0f);
        auto drawX = [&](const own::RectI& b){ g.DrawLine(&pen, b.x+4,b.y+4,b.x+b.w-4,b.y+b.h-4);
                                               g.DrawLine(&pen, b.x+b.w-4,b.y+4,b.x+4,b.y+b.h-4); };
        drawX(L.closeBtn);
        g.DrawLine(&pen, L.rollBtn.x+4, L.rollBtn.y+L.rollBtn.h/2, L.rollBtn.x+L.rollBtn.w-4, L.rollBtn.y+L.rollBtn.h/2);
        SolidBrush dot(Color(255,0x40,0x40,0x40));
        g.FillRectangle(&dot, L.pinBtn.x+6, L.pinBtn.y+4, 6, L.pinBtn.h-8);
        g.DrawEllipse(&pen, L.opacityBtn.x+3, L.opacityBtn.y+3, L.opacityBtn.w-6, L.opacityBtn.h-6);
        // 占位内容
        FontFamily ff(L"Segoe UI"); Font font(&ff, 12, FontStyleRegular, UnitPixel);
        SolidBrush text(Color(255,0x20,0x20,0x20));
        std::string body = m_note.plainText.empty() ? std::string("(empty)") : m_note.plainText;
        std::wstring w(body.begin(), body.end());   // ASCII 占位；真正内容 P3 处理
        g.DrawString(w.c_str(), (int)w.size(), &font, PointF((REAL)6, (REAL)(kTitleMetrics.height+6)), &text);
    }
    dc.BitBlt(0,0,rc.Width(),rc.Height(), &mem, 0,0, SRCCOPY);
    mem.SelectObject(old);
}

void CNoteWindow::OnLButtonDown(UINT, CPoint pt) {
    auto L = layout();
    if (own::hitTestTitleBar(L, pt.x, pt.y) == own::TitleHit::Drag) {
        m_dragging = true;
        ::GetCursorPos(&m_dragAnchorScreen);
        GetWindowRect(&m_dragStartRect);
        SetCapture();
    }
}
void CNoteWindow::OnMouseMove(UINT, CPoint) {
    if (!m_dragging) return;
    CPoint cur; ::GetCursorPos(&cur);
    int nx = m_dragStartRect.left + (cur.x - m_dragAnchorScreen.x);
    int ny = m_dragStartRect.top  + (cur.y - m_dragAnchorScreen.y);
    SetWindowPos(nullptr, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
void CNoteWindow::OnLButtonUp(UINT, CPoint) {
    if (!m_dragging) return;
    m_dragging = false; ReleaseCapture();
    CRect r; GetWindowRect(&r);
    m_note.rect = { r.left, r.top, r.Width(), r.Height() };
    if (m_store) m_store->updateGeometry(m_note.id, m_note.rect, "");
}
