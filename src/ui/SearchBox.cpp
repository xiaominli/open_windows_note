#include "ui/SearchBox.h"
static const UINT kEditId = 0x3101;
BEGIN_MESSAGE_MAP(CSearchBox, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_EN_CHANGE(kEditId, OnEditChanged)
END_MESSAGE_MAP()
bool CSearchBox::Create(CWnd* parent, const CRect& rc) {
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_WINDOW+1));
    if (!CWnd::CreateEx(0, cls, _T("SearchBox"), WS_CHILD | WS_VISIBLE, rc, parent, 0x3100))
        return false;
    CRect e(4, 3, rc.Width() - 4, rc.Height() - 3);
    m_edit.Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, e, this, kEditId);
    return true;
}
void CSearchBox::Reposition(const CRect& rc) {
    MoveWindow(rc);
    if (m_edit.GetSafeHwnd()) m_edit.MoveWindow(4, 3, rc.Width() - 8, rc.Height() - 6);
}
BOOL CSearchBox::OnEraseBkgnd(CDC*) { return TRUE; }
void CSearchBox::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    dc.FillSolidRect(rc, RGB(0xFF,0xFF,0xFF));
    CBrush border(RGB(0xB0,0xB0,0xB0)); dc.FrameRect(rc, &border);
}
void CSearchBox::OnEditChanged() {
    CString w; m_edit.GetWindowText(w);
    CStringA a(w);
    if (onChanged) onChanged(std::string((LPCSTR)a));
}
