#include "ui/NoteWindow.h"
#include "ui/MonitorEnum.h"
#include "ui/ContentLayout.h"
#include "ui/ContentViewFactory.h"
#include "domain/Geometry.h"
#include "data/NoteStore.h"
#include <gdiplus.h>
#include <ctime>
using namespace Gdiplus;

static const own::TitleBarMetrics kTitleMetrics{ 28, 20, 4, 4 };

BEGIN_MESSAGE_MAP(CNoteWindow, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_SETCURSOR()
    ON_WM_MOUSEACTIVATE()
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_WM_DESTROY()
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

    m_content = own::makeContentView(m_note.type);
    if (m_content) {
        CRect rc; GetClientRect(&rc);
        own::RectI cr = own::noteContentRect({0,0,rc.Width(),rc.Height()}, kTitleMetrics.height, 6);
        m_content->Create(this, CRect(cr.x, cr.y, cr.x+cr.w, cr.y+cr.h));
        m_content->Load(m_note);
        if (m_note.rolledUp) m_content->SetVisible(false);
    }
    SetTimer(kSaveTimer, 800, nullptr);
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
        // 占位内容（卷起时不画；有内容视图时由子控件自绘）
        if (!m_note.rolledUp && !m_content) {
            FontFamily ff(L"Segoe UI"); Font font(&ff, 12, FontStyleRegular, UnitPixel);
            SolidBrush text(Color(255,0x20,0x20,0x20));
            std::string body = m_note.plainText.empty() ? std::string("(empty)") : m_note.plainText;
            std::wstring w(body.begin(), body.end());   // ASCII 占位；真正内容 P3 处理
            g.DrawString(w.c_str(), (int)w.size(), &font, PointF((REAL)6, (REAL)(kTitleMetrics.height+6)), &text);
        }
    }
    dc.BitBlt(0,0,rc.Width(),rc.Height(), &mem, 0,0, SRCCOPY);
    mem.SelectObject(old);
}

BOOL CNoteWindow::OnSetCursor(CWnd*, UINT, UINT) {
    CPoint pt; ::GetCursorPos(&pt); ScreenToClient(&pt);
    CRect rc; GetClientRect(&rc);
    auto e = own::hitTestResizeEdge({0,0,rc.Width(),rc.Height()}, pt.x, pt.y, 6);
    LPCTSTR c = IDC_ARROW;
    switch (e) {
        case own::ResizeEdge::Left: case own::ResizeEdge::Right: c = IDC_SIZEWE; break;
        case own::ResizeEdge::Top: case own::ResizeEdge::Bottom: c = IDC_SIZENS; break;
        case own::ResizeEdge::TopLeft: case own::ResizeEdge::BottomRight: c = IDC_SIZENWSE; break;
        case own::ResizeEdge::TopRight: case own::ResizeEdge::BottomLeft: c = IDC_SIZENESW; break;
        default: break;
    }
    ::SetCursor(::LoadCursor(nullptr, c));
    return TRUE;
}
int CNoteWindow::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message) {
    // 点便签时强制成为前台窗口：中文 IME 组字按前台窗路由，否则拼音会以 ASCII 漏进控件。
    ::SetForegroundWindow(m_hWnd);
    return CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);   // MA_ACTIVATE
}
void CNoteWindow::OnLButtonDown(UINT, CPoint pt) {
    CRect rc; GetClientRect(&rc);
    auto edge = own::hitTestResizeEdge({0,0,rc.Width(),rc.Height()}, pt.x, pt.y, 6);
    if (edge != own::ResizeEdge::None) {
        m_resizing = true; m_resizeEdge = edge;
        ::GetCursorPos(&m_resizeAnchorScreen); GetWindowRect(&m_resizeStartRect);
        SetCapture(); return;
    }
    auto L = layout();
    switch (own::hitTestTitleBar(L, pt.x, pt.y)) {
        case own::TitleHit::Close: {
            flushContent();          // 隐藏前存一次
            ShowWindow(SW_HIDE);
            m_note.visible = false;
            if (m_store) m_store->updateFlags(m_note.id, m_note.opacity, m_note.pinned, m_note.rolledUp, false);
            return;
        }
        case own::TitleHit::Pin: {
            m_note.pinned = !m_note.pinned;
            SetWindowPos(m_note.pinned ? &wndTopMost : &wndNoTopMost, 0,0,0,0,
                         SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
            if (m_store) m_store->updateFlags(m_note.id, m_note.opacity, m_note.pinned, m_note.rolledUp, m_note.visible);
            Invalidate(FALSE); return;
        }
        case own::TitleHit::Roll: {
            CRect r; GetWindowRect(&r);
            if (!m_note.rolledUp) {
                m_expandedHeight = r.Height();
                SetWindowPos(nullptr,0,0,r.Width(), kTitleMetrics.height, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                m_note.rolledUp = true;
                if (m_content) m_content->SetVisible(false);
            } else {
                int h = m_expandedHeight > 0 ? m_expandedHeight : 200;
                SetWindowPos(nullptr,0,0,r.Width(), h, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                m_note.rolledUp = false;
                if (m_content) m_content->SetVisible(true);
            }
            if (m_store) m_store->updateFlags(m_note.id, m_note.opacity, m_note.pinned, m_note.rolledUp, m_note.visible);
            Invalidate(FALSE); return;
        }
        case own::TitleHit::Opacity: {
            static const int steps[] = {255,204,153,102};
            int idx = 0; for (int i=0;i<4;++i) if (steps[i]==m_note.opacity) { idx=i; break; }
            m_note.opacity = steps[(idx+1)%4];
            ::SetLayeredWindowAttributes(m_hWnd, 0, (BYTE)m_note.opacity, LWA_ALPHA);
            if (m_store) m_store->updateFlags(m_note.id, m_note.opacity, m_note.pinned, m_note.rolledUp, m_note.visible);
            return;
        }
        case own::TitleHit::Drag: {
            m_dragging = true; ::GetCursorPos(&m_dragAnchorScreen); GetWindowRect(&m_dragStartRect); SetCapture(); return;
        }
        default: break;
    }
}
void CNoteWindow::OnMouseMove(UINT, CPoint) {
    if (m_resizing) {
        CPoint cur; ::GetCursorPos(&cur);
        int dx = cur.x - m_resizeAnchorScreen.x, dy = cur.y - m_resizeAnchorScreen.y;
        own::RectI start{ m_resizeStartRect.left, m_resizeStartRect.top,
                          m_resizeStartRect.Width(), m_resizeStartRect.Height() };
        own::RectI nr = own::applyResize(start, m_resizeEdge, dx, dy, 120, 80);
        SetWindowPos(nullptr, nr.x, nr.y, nr.w, nr.h, SWP_NOZORDER | SWP_NOACTIVATE);
        Invalidate(FALSE);
        return;
    }
    if (!m_dragging) return;
    CPoint cur; ::GetCursorPos(&cur);
    int nx = m_dragStartRect.left + (cur.x - m_dragAnchorScreen.x);
    int ny = m_dragStartRect.top  + (cur.y - m_dragAnchorScreen.y);
    SetWindowPos(nullptr, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
void CNoteWindow::OnLButtonUp(UINT, CPoint) {
    if (m_resizing) {
        m_resizing = false; ReleaseCapture();
        CRect r; GetWindowRect(&r);
        m_note.rect = { r.left, r.top, r.Width(), r.Height() };
        if (m_store) m_store->updateGeometry(m_note.id, m_note.rect, "");
        return;
    }
    if (!m_dragging) return;
    m_dragging = false; ReleaseCapture();
    CRect r; GetWindowRect(&r);
    m_note.rect = { r.left, r.top, r.Width(), r.Height() };
    if (m_store) m_store->updateGeometry(m_note.id, m_note.rect, "");
}

void CNoteWindow::layoutContent() {
    if (!m_content) return;
    CRect rc; GetClientRect(&rc);
    own::RectI cr = own::noteContentRect({0,0,rc.Width(),rc.Height()}, kTitleMetrics.height, 6);
    m_content->Reposition(CRect(cr.x, cr.y, cr.x+cr.w, cr.y+cr.h));
}
void CNoteWindow::flushContent() {
    if (!m_content || !m_store) return;
    if (!m_content->IsDirty()) return;
    std::vector<uint8_t> blob; std::string plain;
    if (m_content->Save(blob, plain))
        m_store->updateContent(m_note.id, blob, plain, (int64_t)time(nullptr));
}
void CNoteWindow::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    layoutContent();
}
void CNoteWindow::OnTimer(UINT_PTR id) {
    if (id == kSaveTimer) flushContent();
    CWnd::OnTimer(id);
}
void CNoteWindow::OnDestroy() {
    KillTimer(kSaveTimer);
    flushContent();
    if (m_content) { m_content->DestroyView(); m_content.reset(); }
    CWnd::OnDestroy();
}
