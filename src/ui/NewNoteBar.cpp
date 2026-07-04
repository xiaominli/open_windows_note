#include "ui/NewNoteBar.h"
#include <gdiplus.h>
using namespace Gdiplus;

namespace {
const int kBtn = 24, kGap = 8, kPadX = 8, kPadY = 4;
}

BEGIN_MESSAGE_MAP(CNewNoteBar, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
END_MESSAGE_MAP()

bool CNewNoteBar::Create(CWnd* parent, const CRect& rc) {
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW));
    return CreateEx(0, cls, _T(""), WS_CHILD | WS_VISIBLE, rc, parent, 0x3400) != FALSE;
}
void CNewNoteBar::Reposition(const CRect& rc) { if (GetSafeHwnd()) MoveWindow(rc); }

CRect CNewNoteBar::btnRect(int i) {
    int x = kPadX + i * (kBtn + kGap);
    return CRect(x, kPadY, x + kBtn, kPadY + kBtn);
}
int CNewNoteBar::hitTest(CPoint pt) const {
    for (int i = 0; i < 3; ++i) if (btnRect(i).PtInRect(pt)) return i;
    return -1;
}

BOOL CNewNoteBar::OnEraseBkgnd(CDC*) { return TRUE; }   // 防闪烁，全在 OnPaint 画

void CNewNoteBar::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* old = mem.SelectObject(&bmp);
    {
        Graphics g(mem.GetSafeHdc());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush bg(Color(255, 0xF5, 0xF5, 0xF5));
        g.FillRectangle(&bg, 0, 0, rc.Width(), rc.Height());
        Pen sep(Color(255, 0xD8, 0xD8, 0xD8), 1.0f);
        g.DrawLine(&sep, 0, rc.Height() - 1, rc.Width(), rc.Height() - 1);

        Pen pen(Color(255, 0x40, 0x40, 0x40), 2.0f);
        SolidBrush ink(Color(255, 0x40, 0x40, 0x40));
        for (int i = 0; i < 3; ++i) {
            CRect b = btnRect(i);
            if (i == m_hover) {   // 悬停高亮：浅灰圆角底
                SolidBrush hov(Color(255, 0xDE, 0xDE, 0xDE));
                g.FillRectangle(&hov, b.left, b.top, b.Width(), b.Height());
            }
            switch (i) {
                case 0: {   // 文本：文档页 + 三行
                    g.DrawRectangle(&pen, b.left + 6, b.top + 3, 12, 18);
                    Pen line(Color(255, 0x40, 0x40, 0x40), 1.0f);
                    g.DrawLine(&line, b.left + 9, b.top + 8,  b.left + 15, b.top + 8);
                    g.DrawLine(&line, b.left + 9, b.top + 12, b.left + 15, b.top + 12);
                    g.DrawLine(&line, b.left + 9, b.top + 16, b.left + 13, b.top + 16);
                    break;
                }
                case 1: {   // 清单：方框 + 勾
                    g.DrawRectangle(&pen, b.left + 5, b.top + 5, 14, 14);
                    Point chk[3] = { Point(b.left + 8,  b.top + 12),
                                     Point(b.left + 11, b.top + 15),
                                     Point(b.left + 16, b.top + 9) };
                    g.DrawLines(&pen, chk, 3);
                    break;
                }
                case 2: {   // 涂鸦：随手一笔（贝塞尔弧）
                    g.DrawBezier(&pen, b.left + 4,  b.top + 17,
                                       b.left + 9,  b.top + 4,
                                       b.left + 14, b.top + 20,
                                       b.left + 20, b.top + 7);
                    break;
                }
            }
        }
    }
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(old);
}

void CNewNoteBar::OnMouseMove(UINT, CPoint pt) {
    if (!m_tracking) {   // 挂 WM_MOUSELEAVE,否则移出后悬停高亮残留
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        ::TrackMouseEvent(&tme);
        m_tracking = true;
    }
    int h = hitTest(pt);
    if (h != m_hover) { m_hover = h; Invalidate(FALSE); }
}
LRESULT CNewNoteBar::OnMouseLeave(WPARAM, LPARAM) {
    m_tracking = false;
    if (m_hover != -1) { m_hover = -1; Invalidate(FALSE); }
    return 0;
}
void CNewNoteBar::OnLButtonUp(UINT, CPoint pt) {
    switch (hitTest(pt)) {
        case 0: if (onNewText) onNewText(); break;
        case 1: if (onNewChecklist) onNewChecklist(); break;
        case 2: if (onNewDrawing) onNewDrawing(); break;
        default: break;
    }
}
