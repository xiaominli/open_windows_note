#include "ui/DrawingContentView.h"
#include "ui/DrawingMath.h"
#include "domain/StrokesJson.h"
#include <gdiplus.h>
using namespace Gdiplus;

static const int kToolH = 22;
static const int kSwatch = 16;
static const double kEraseTol = 6.0;
static const uint32_t kPalette[4] = { 0x000000, 0xE03030, 0x3060E0, 0x30A030 };

BEGIN_MESSAGE_MAP(CDrawingContentView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

bool CDrawingContentView::Create(CWnd* parent, const CRect& rc) {
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_CROSS));
    if (!CWnd::CreateEx(0, cls, _T("Drawing"), WS_CHILD | WS_VISIBLE, rc, parent, 0x1003))
        return false;
    m_created = true;
    return true;
}
void CDrawingContentView::Load(const own::Note& note) {
    m_strokes = own::deserializeStrokes(note.contentBlob);
    m_dirty = false;
    if (m_created) Invalidate(FALSE);
}
bool CDrawingContentView::Save(std::vector<uint8_t>& outBlob, std::string& outPlain) {
    outBlob = own::serializeStrokes(m_strokes);
    outPlain = "";                       // 涂鸦无搜索文本（OCR 属 v2）
    m_dirty = false;
    return true;
}
void CDrawingContentView::Reposition(const CRect& rc) { if (m_created) MoveWindow(rc); }
bool CDrawingContentView::IsDirty() const { return m_dirty; }
void CDrawingContentView::SetVisible(bool show) { if (m_created) ShowWindow(show ? SW_SHOW : SW_HIDE); }
void CDrawingContentView::DestroyView() { if (m_created) { DestroyWindow(); m_created = false; } }

BOOL CDrawingContentView::OnEraseBkgnd(CDC*) { return TRUE; }

int CDrawingContentView::toolAtPoint(CPoint pt) const {
    if (pt.y >= kToolH) return -1;
    int x = 2;
    for (int i = 0; i < 4; ++i) { if (pt.x >= x && pt.x < x + kSwatch) return i; x += kSwatch + 4; }
    if (pt.x >= x && pt.x < x + kSwatch) return 4;   // 橡皮槽
    return -1;
}
void CDrawingContentView::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* old = mem.SelectObject(&bmp);
    {
        Graphics g(mem.GetSafeHdc());
        g.Clear(Color(255, 0xFF, 0xF7, 0xB0));
        // 工具行
        int x = 2;
        for (int i = 0; i < 4; ++i) {
            SolidBrush b(Color(255, (kPalette[i]>>16)&0xFF, (kPalette[i]>>8)&0xFF, kPalette[i]&0xFF));
            g.FillRectangle(&b, x, 2, kSwatch, kSwatch);
            if (!m_eraser && m_color == kPalette[i]) {
                Pen sel(Color(255,0,0,0), 2.0f); g.DrawRectangle(&sel, x, 2, kSwatch, kSwatch);
            }
            x += kSwatch + 4;
        }
        SolidBrush eb(Color(255, 0xEE, 0xEE, 0xEE));
        g.FillRectangle(&eb, x, 2, kSwatch, kSwatch);
        FontFamily ff(L"Segoe UI"); Font f(&ff, 9, FontStyleRegular, UnitPixel);
        SolidBrush tb(Color(255,0x40,0x40,0x40));
        g.DrawString(L"E", 1, &f, PointF((REAL)(x+4),(REAL)3), &tb);
        if (m_eraser) { Pen sel(Color(255,0,0,0), 2.0f); g.DrawRectangle(&sel, x, 2, kSwatch, kSwatch); }
        // 已有笔迹 + 当前笔迹
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        auto drawStroke = [&](const own::Stroke& s) {
            if (s.points.size() < 2) return;
            Pen p(Color(255, (s.color>>16)&0xFF, (s.color>>8)&0xFF, s.color&0xFF), (REAL)s.width);
            p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound); p.SetLineJoin(LineJoinRound);
            std::vector<Point> pts;
            for (auto& pr : s.points) pts.push_back(Point(pr.first, pr.second));
            g.DrawLines(&p, pts.data(), (INT)pts.size());
        };
        for (auto& s : m_strokes) drawStroke(s);
        if (m_drawing) drawStroke(m_cur);
    }
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(old);
}
void CDrawingContentView::OnLButtonDown(UINT, CPoint pt) {
    int tool = toolAtPoint(pt);
    if (tool >= 0) {                          // 点工具行
        if (tool == 4) m_eraser = true;
        else { m_eraser = false; m_color = kPalette[tool]; }
        Invalidate(FALSE);
        return;
    }
    if (m_eraser) {
        int hit = own::strokeHitTest(m_strokes, pt.x, pt.y, kEraseTol);
        if (hit >= 0) { m_strokes.erase(m_strokes.begin() + hit); m_dirty = true; Invalidate(FALSE); }
        return;
    }
    m_drawing = true;
    m_cur = own::Stroke{};
    m_cur.color = m_color; m_cur.width = m_width;
    m_cur.points.push_back({ pt.x, pt.y });
    SetCapture();
}
void CDrawingContentView::OnMouseMove(UINT, CPoint pt) {
    if (!m_drawing) return;
    m_cur.points.push_back({ pt.x, pt.y });
    Invalidate(FALSE);
}
void CDrawingContentView::OnLButtonUp(UINT, CPoint) {
    if (!m_drawing) return;
    m_drawing = false; ReleaseCapture();
    if (m_cur.points.size() >= 2) { m_strokes.push_back(m_cur); m_dirty = true; }
    m_cur = own::Stroke{};
    Invalidate(FALSE);
}
