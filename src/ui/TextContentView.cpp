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
// RichEdit 不设置字体时用系统默认（System 点阵字体），中文渲染发虚。
// scope = SCF_DEFAULT（新输入）或 SCF_ALL（整篇统一；当前无格式工具条，安全）。
static void applyNoteFont(CRichEditCtrl& edit, WPARAM scope) {
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_CHARSET;
    wcscpy_s(cf.szFaceName, L"微软雅黑");   // 微软雅黑
    cf.yHeight = 200;                        // 10pt（twip）
    cf.bCharSet = DEFAULT_CHARSET;
    ::SendMessage(edit.GetSafeHwnd(), EM_SETCHARFORMAT, scope, (LPARAM)&cf);
}
} // namespace

bool CTextContentView::Create(CWnd* parent, const CRect& rc) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
                | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
    // 优先用 RichEdit 4.1 (RICHEDIT50W / Msftedit.dll)：riched20 的中文 IME 组字有已知问题。
    HWND h = ::CreateWindowExW(0, L"RICHEDIT50W", L"", style,
                               rc.left, rc.top, rc.Width(), rc.Height(),
                               parent->GetSafeHwnd(), (HMENU)(UINT_PTR)0x1001,
                               ::AfxGetInstanceHandle(), nullptr);
    if (h) {
        m_edit.Attach(h);
    } else if (!m_edit.Create(style, rc, parent, 0x1001)) {  // 回落 RichEdit 2.0
        return false;
    }
    m_edit.SetEventMask(m_edit.GetEventMask() | ENM_CHANGE);
    applyNoteFont(m_edit, SCF_DEFAULT);
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
    applyNoteFont(m_edit, SCF_ALL);   // 旧内容 RTF 里带的 System 字体也统一掉
    ApplyTheme(m_bgRgb, m_textRgb);
    m_edit.SetModify(FALSE);
}
bool CTextContentView::Save(std::vector<uint8_t>& outBlob, std::string& outPlain) {
    if (!m_created) return false;
    outBlob.clear();
    OutCtx ctx{ &outBlob };
    EDITSTREAM es{ (DWORD_PTR)&ctx, 0, streamOutCb };
    m_edit.StreamOut(SF_RTF, es);
    CString w; m_edit.GetWindowText(w);
    // 转 UTF-8（与列表 u8ToWide / 搜索缓存一致）；CStringA 会按 ANSI 转，中文会乱码
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), nullptr, 0, nullptr, nullptr);
    std::string u8(n > 0 ? n : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), &u8[0], n, nullptr, nullptr);
    outPlain = own::searchNormalize(u8);
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
void CTextContentView::ApplyTheme(uint32_t bgRgb, uint32_t textRgb) {
    if (!m_created) return;
    m_bgRgb = bgRgb; m_textRgb = textRgb;
    COLORREF bg = RGB((bgRgb>>16)&0xFF, (bgRgb>>8)&0xFF, bgRgb&0xFF);
    ::SendMessage(m_edit.GetSafeHwnd(), EM_SETBKGNDCOLOR, 0, (LPARAM)bg);
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = RGB((textRgb>>16)&0xFF, (textRgb>>8)&0xFF, textRgb&0xFF);
    BOOL mod = m_edit.GetModify();
    ::SendMessage(m_edit.GetSafeHwnd(), EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    m_edit.SetModify(mod);   // 换色不算脏
}
