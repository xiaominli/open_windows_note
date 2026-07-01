#include "ui/TextContentView.h"
#include "domain/SearchText.h"
#include <algorithm>

// EDITSTREAM 回调：以 std::vector<uint8_t> 作为源/汇
namespace {
struct InCtx  { const std::vector<uint8_t>* buf; size_t pos; };
struct OutCtx { std::vector<uint8_t>* buf; };
DWORD CALLBACK streamInCb(DWORD_PTR cookie, LPBYTE dst, LONG cb, LONG* pcb) {
    auto* c = reinterpret_cast<InCtx*>(cookie);
    size_t remain = c->buf->size() - c->pos;
    LONG n = (LONG)std::min<size_t>((size_t)cb, remain);
    if (n > 0) { memcpy(dst, c->buf->data() + c->pos, (size_t)n); c->pos += (size_t)n; }
    *pcb = n;
    return 0;
}
DWORD CALLBACK streamOutCb(DWORD_PTR cookie, LPBYTE src, LONG cb, LONG* pcb) {
    auto* c = reinterpret_cast<OutCtx*>(cookie);
    if (cb > 0) c->buf->insert(c->buf->end(), src, src + cb);
    *pcb = cb;
    return 0;
}
} // namespace

bool CTextContentView::Create(CWnd* parent, const CRect& rc) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
                | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
    if (!m_edit.Create(style, rc, parent, 0x1001)) return false;
    m_edit.SetEventMask(m_edit.GetEventMask() | ENM_CHANGE);
    m_created = true;
    return true;
}
void CTextContentView::Load(const own::Note& note) {
    if (!m_created) return;
    if (note.contentBlob.empty()) {
        m_edit.SetWindowText(_T(""));
    } else {
        InCtx ctx{ &note.contentBlob, 0 };
        EDITSTREAM es{ (DWORD_PTR)&ctx, 0, streamInCb };
        long read = m_edit.StreamIn(SF_RTF, es);
        if (read <= 0 && es.dwError != 0) {
            // RTF 解析失败 → 回落把原始字节当纯文本显示（不丢内容）
            std::string s((const char*)note.contentBlob.data(), note.contentBlob.size());
            m_edit.SetWindowText(CString(s.c_str()));
        }
    }
    m_edit.SetModify(FALSE);
}
bool CTextContentView::Save(std::vector<uint8_t>& outBlob, std::string& outPlain) {
    if (!m_created) return false;
    outBlob.clear();
    OutCtx ctx{ &outBlob };
    EDITSTREAM es{ (DWORD_PTR)&ctx, 0, streamOutCb };
    m_edit.StreamOut(SF_RTF, es);
    CString w; m_edit.GetWindowText(w);
    CStringA utf8(w);   // 便签正文通常 ASCII/本地页；plain_text 仅供 LIKE 搜索
    outPlain = own::searchNormalize(std::string((LPCSTR)utf8));
    m_edit.SetModify(FALSE);
    return true;
}
void CTextContentView::Reposition(const CRect& rc) {
    if (m_created) m_edit.MoveWindow(rc);
}
bool CTextContentView::IsDirty() const {
    return m_created && const_cast<CRichEditCtrl&>(m_edit).GetModify();
}
void CTextContentView::SetVisible(bool show) {
    if (m_created) m_edit.ShowWindow(show ? SW_SHOW : SW_HIDE);
}
void CTextContentView::DestroyView() {
    if (m_created) { m_edit.DestroyWindow(); m_created = false; }
}
