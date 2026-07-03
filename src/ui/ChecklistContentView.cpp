#include "ui/ChecklistContentView.h"
#include "ui/ChecklistLayout.h"
#include "ui/UiFont.h"
#include "domain/ChecklistModel.h"
#include "domain/ChecklistJson.h"
#include "domain/SearchText.h"

static const own::ChecklistMetrics kMetrics{ 24, 16, 4 };
static const UINT kInplaceEditId = 0x2001;

// 条目文本存储为 UTF-8；CString(const char*)/CStringA 按 ANSI 转换会把中文变乱码。
static CString u8ToWide(const std::string& s) {
    if (s.empty()) return CString();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CString w;
    if (n > 0) { ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.GetBuffer(n), n); w.ReleaseBuffer(n); }
    return w;
}
static std::string wideToU8(const CString& w) {
    if (w.IsEmpty()) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), &s[0], n, nullptr, nullptr);
    return s;
}

BEGIN_MESSAGE_MAP(CChecklistContentView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_EN_KILLFOCUS(kInplaceEditId, OnCommitEdit)
END_MESSAGE_MAP()

bool CChecklistContentView::Create(CWnd* parent, const CRect& rc) {
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW));
    if (!CWnd::CreateEx(0, cls, _T("Checklist"), WS_CHILD | WS_VISIBLE, rc, parent, 0x1002))
        return false;
    m_created = true;
    return true;
}
void CChecklistContentView::Load(const own::Note& note) {
    m_originalBlob = note.contentBlob;
    m_items = own::deserializeChecklist(note.contentBlob);  // 失败返回空；originalBlob 保留
    m_dirty = false;
    if (m_created) Invalidate(FALSE);
}
bool CChecklistContentView::Save(std::vector<uint8_t>& outBlob, std::string& outPlain) {
    outBlob = own::serializeChecklist(m_items);
    outPlain = own::searchNormalize(own::checklistPlainText(m_items));
    m_dirty = false;
    return true;
}
void CChecklistContentView::Reposition(const CRect& rc) { if (m_created) MoveWindow(rc); }
bool CChecklistContentView::IsDirty() const { return m_dirty; }
void CChecklistContentView::SetVisible(bool show) { if (m_created) ShowWindow(show ? SW_SHOW : SW_HIDE); }
void CChecklistContentView::DestroyView() { if (m_created) { DestroyWindow(); m_created = false; } }

BOOL CChecklistContentView::OnEraseBkgnd(CDC*) { return TRUE; }

void CChecklistContentView::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* old = mem.SelectObject(&bmp);
    mem.FillSolidRect(&rc, RGB(0xFF, 0xF7, 0xB0));           // 背景（与便签底色一致，主题化留 P4）
    CPen pen(PS_SOLID, 1, RGB(0x50, 0x50, 0x50));
    CPen* op = mem.SelectObject(&pen);
    CFont* of = mem.SelectObject(CFont::FromHandle(own_ui::uiFont(14)));   // DC 默认 SYSTEM_FONT 中文发虚
    mem.SetBkMode(TRANSPARENT);
    own::RectI content{ 0, 0, rc.Width(), rc.Height() };
    for (size_t i = 0; i < m_items.size(); ++i) {
        own::RectI b = own::checklistBoxRect(content, kMetrics, (int)i);
        mem.Rectangle(b.x, b.y, b.x + b.w, b.y + b.h);       // 勾选框
        if (m_items[i].checked) {                            // 勾：对角线两笔
            mem.MoveTo(b.x + 2, b.y + b.h / 2); mem.LineTo(b.x + b.w / 2, b.y + b.h - 2);
            mem.LineTo(b.x + b.w - 2, b.y + 2);
        }
        own::RectI row = own::checklistRowRect(content, kMetrics, (int)i);
        CRect tr(b.x + b.w + 4, row.y, row.x + row.w, row.y + row.h);
        CString t = u8ToWide(m_items[i].text);
        mem.DrawText(t, tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    own::RectI addRow = own::checklistRowRect(content, kMetrics, (int)m_items.size());
    CRect ar(addRow.x + kMetrics.pad, addRow.y, addRow.x + addRow.w, addRow.y + addRow.h);
    mem.DrawText(_T("+ add"), ar, DT_SINGLELINE | DT_VCENTER);
    mem.SelectObject(of);
    mem.SelectObject(op);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(old);
}
void CChecklistContentView::beginEdit(int index) {
    if (index < 0 || index >= (int)m_items.size()) return;
    CRect rc; GetClientRect(&rc);
    own::RectI r = own::checklistRowRect({0,0,rc.Width(),rc.Height()}, kMetrics, index);
    own::RectI b = own::checklistBoxRect({0,0,rc.Width(),rc.Height()}, kMetrics, index);
    CRect er(b.x + b.w + 4, r.y, r.x + r.w, r.y + r.h);
    if (m_edit.GetSafeHwnd()) m_edit.DestroyWindow();
    m_edit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, er, this, kInplaceEditId);
    m_edit.SetFont(CFont::FromHandle(own_ui::uiFont(14)));
    m_edit.SetWindowText(u8ToWide(m_items[index].text));
    m_edit.SetFocus();
    m_edit.SetSel(0, -1);
    m_editing = index;
}
void CChecklistContentView::commitEdit() {
    if (m_editing < 0 || !m_edit.GetSafeHwnd()) return;
    CString w; m_edit.GetWindowText(w);
    own::checklistSetText(m_items, (size_t)m_editing, wideToU8(w));
    m_edit.DestroyWindow();
    m_editing = -1;
    m_dirty = true;
    Invalidate(FALSE);
}
void CChecklistContentView::OnCommitEdit() { commitEdit(); }
void CChecklistContentView::OnLButtonDown(UINT, CPoint pt) {
    if (m_editing >= 0) { commitEdit(); return; }
    CRect rc; GetClientRect(&rc);
    auto hit = own::checklistHitTest({0,0,rc.Width(),rc.Height()}, kMetrics, (int)m_items.size(), pt.x, pt.y);
    switch (hit.kind) {
        case own::ChecklistHit::Checkbox:
            own::checklistToggle(m_items, (size_t)hit.index); m_dirty = true; Invalidate(FALSE); break;
        case own::ChecklistHit::Text:
            beginEdit(hit.index); break;
        case own::ChecklistHit::AddRow:
            own::checklistAdd(m_items, ""); m_dirty = true;
            beginEdit((int)m_items.size() - 1); break;
        default: break;
    }
}
