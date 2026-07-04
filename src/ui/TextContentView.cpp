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
static int s_defaultFontTwips = 200;                 // 10pt；SetDefaultFontPt 更新
// scope = SCF_DEFAULT（新输入）或 SCF_ALL（整篇统一字体族；不动字号/颜色/加粗——用户格式保真）。
static void applyNoteFont(CRichEditCtrl& edit, WPARAM scope) {
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_CHARSET;
    if (scope == SCF_DEFAULT) { cf.dwMask |= CFM_SIZE; cf.yHeight = s_defaultFontTwips; }
    wcscpy_s(cf.szFaceName, L"微软雅黑");   // 微软雅黑
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
    applyNoteFont(m_edit, SCF_ALL);   // 整篇统一字体族（不动字号/颜色/加粗——用户格式保真）
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
    // 文字色只设默认格式：字符级颜色（工具条设置）在换主题后保留；
    // 4 套内置主题 textColor 相同，因此已有文字无需跟随
    ::SendMessage(m_edit.GetSafeHwnd(), EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
    m_edit.SetModify(mod);   // 换色不算脏
}
void CTextContentView::SetDefaultFontPt(int pt) {
    if (pt >= 8 && pt <= 24) s_defaultFontTwips = pt * 20;
}
void CTextContentView::ApplyFormat(own::FmtOp op) {
    if (!m_created) return;
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf);
    m_edit.GetSelectionCharFormat(cf);
    CHARFORMAT2W set{}; set.cbSize = sizeof(set);
    auto toggle = [&](DWORD effect) {
        set.dwMask = (effect == CFE_BOLD) ? CFM_BOLD
                   : (effect == CFE_ITALIC) ? CFM_ITALIC
                   : (effect == CFE_UNDERLINE) ? CFM_UNDERLINE : CFM_STRIKEOUT;
        // 选区内混合（mask 未含该位）按「未开」处理 -> 统一打开
        bool on = (cf.dwMask & set.dwMask) && (cf.dwEffects & effect);
        set.dwEffects = on ? 0 : effect;
    };
    switch (op) {
        case own::FmtOp::Bold:      toggle(CFE_BOLD); break;
        case own::FmtOp::Italic:    toggle(CFE_ITALIC); break;
        case own::FmtOp::Underline: toggle(CFE_UNDERLINE); break;
        case own::FmtOp::Strike:    toggle(CFE_STRIKEOUT); break;
        case own::FmtOp::SizeUp:
        case own::FmtOp::SizeDown: {
            int cur = (cf.dwMask & CFM_SIZE) ? (int)cf.yHeight : s_defaultFontTwips;  // 混合选区从默认起步
            set.dwMask = CFM_SIZE;
            set.yHeight = own::fontSizeStep(cur, op == own::FmtOp::SizeUp);
            break;
        }
        case own::FmtOp::TextColor: {
            uint32_t curRgb = 0xFFFFFFFF;                    // 无效值 -> 调色板回落首色
            if ((cf.dwMask & CFM_COLOR) && !(cf.dwEffects & CFE_AUTOCOLOR)) {
                COLORREF c = cf.crTextColor;                 // COLORREF 是 0xBBGGRR，转回 0xRRGGBB
                curRgb = ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | GetBValue(c);
            }
            uint32_t next = own::nextPaletteColor(curRgb);
            set.dwMask = CFM_COLOR;
            set.crTextColor = RGB((next >> 16) & 0xFF, (next >> 8) & 0xFF, next & 0xFF);
            break;
        }
    }
    m_edit.SetSelectionCharFormat(set);
    m_edit.SetModify(TRUE);                                  // 格式变更计脏，随 800ms 自动保存落盘
    m_edit.SetFocus();                                       // 点工具条后焦点还给编辑器
}
