#include "ui/table/SWInplaceEdit.h"
#include "ui/table/SWTableScrollViewWnd.h"

BEGIN_MESSAGE_MAP(CSWInplaceEdit, CEdit)
	ON_WM_CHAR()
	ON_WM_GETDLGCODE()
	ON_WM_KEYDOWN()
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

CSWInplaceEdit::CSWInplaceEdit()
{
	m_pOwner = NULL;
	m_row = -1; 
	m_col = -1;
	m_editType = SW_EDIT_NONE;
	m_bCommitting = false;
	m_bCancelling = false;
	m_hEditFont = NULL;
}

CSWInplaceEdit::~CSWInplaceEdit()
{
	if (m_hEditFont)
	{
		::DeleteObject(m_hEditFont);
		m_hEditFont = NULL;
	}
}

BOOL CSWInplaceEdit::CreateInplace(SWTableScrollViewWnd* owner)
{
	m_pOwner = owner;
	// ES_CENTER：文字水平居中；ES_AUTOHSCROLL：输入超出宽度时水平滚动而不是截断。
	// 单行 CEdit 不支持垂直居中样式，垂直居中由 BeginEdit 里缩小并居中窗口矩形实现。
	DWORD dwStyle = WS_CHILD | ES_AUTOHSCROLL | ES_CENTER | WS_BORDER;
	if (!this->Create(dwStyle, CRect(0, 0, 0, 0), owner, 0xA001))
	{
		return FALSE;
	}
	// 自建编辑专用字体：比 label 大一号（18px 字高，微软雅黑），让输入时看得清。
	// 负值代表 character height（不含 leading）；绝对值越大字越大。
	m_hEditFont = ::CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
	                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
	                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
	                            L"微软雅黑");
	if (m_hEditFont)
	{
		this->SendMessage(WM_SETFONT, (WPARAM)m_hEditFont, FALSE);
	}
	else if (owner && owner->m_hLabelFont)
	{
		this->SendMessage(WM_SETFONT, (WPARAM)owner->m_hLabelFont, FALSE);
	}
	this->ShowWindow(SW_HIDE);
	return TRUE;
}

void CSWInplaceEdit::BeginEdit(int row, int col, int editType, const CRect& clientRect, const char* initText)
{
	m_row = row;
	m_col = col;
	m_editType = editType;
	m_bCommitting = false;
	m_bCancelling = false;

	// 已知限制：单行 CEdit 不支持"内部文字垂直居中"——
	//   - EM_SETRECT/SETRECTNP 对单行 edit 是 no-op（MSDN: "Calling EM_SETRECT with a single-line edit has no effect"）
	//   - ES_MULTILINE 仍然顶对齐，无法居中
	// 唯一靠谱的做法：把编辑窗口本身缩到 (字高 + 少量 border padding) 的高度，再在 cell 矩形内上下居中。
	// clientRect 已在 _getCellClientRect 里裁掉滚动条；我们在此基础上再竖直收一下。
	CRect rc = clientRect;
	// 用编辑框实际用的字体（m_hEditFont，比 label 大）量字高，算出居中后的矩形。
	HFONT hMeasureFont = m_hEditFont ? m_hEditFont
	                                 : (m_pOwner ? m_pOwner->m_hLabelFont : NULL);
	if (hMeasureFont)
	{
		HDC hdc = ::GetDC(this->GetSafeHwnd());
		if (hdc)
		{
			HGDIOBJ oldFont = ::SelectObject(hdc, hMeasureFont);
			TEXTMETRIC tm = {0};
			::GetTextMetrics(hdc, &tm);
			::SelectObject(hdc, oldFont);
			::ReleaseDC(this->GetSafeHwnd(), hdc);

			int editHeight = tm.tmHeight + 6; // 字高 + 上下各 3px border/padding
			if (editHeight < rc.Height())
			{
				int margin = (rc.Height() - editHeight) / 2;
				rc.top    += margin;
				rc.bottom  = rc.top + editHeight;
			}
		}
	}

	this->MoveWindow(rc);
	this->ShowWindow(SW_SHOW);
	// SetText 放 Show 之后 + UpdateWindow，避免某些路径下 hidden 时 SetText 不 render
	::SetWindowTextA(this->GetSafeHwnd(), initText ? initText : "");
	this->UpdateWindow();
	this->SetFocus();
	this->SetSel(0, -1);
}

bool CSWInplaceEdit::EndEdit(bool commit)
{
	if (commit)
	{
		char buf[512] = {0};
		::GetWindowTextA(this->GetSafeHwnd(), buf, (int)(sizeof(buf)));
		if (m_pOwner && m_pOwner->m_pCallback)
		{
			if (!m_pOwner->m_pCallback->onTableScrollViewValidateCellEdit(m_pOwner, m_row, m_col, buf))
			{
				MessageBeep(MB_ICONASTERISK);
				this->SetFocus();
				this->SetSel(0, -1);
				return false;
			}
			m_bCommitting = true;
			m_pOwner->m_pCallback->onTableScrollViewCommitCellEdit(m_pOwner, m_row, m_col, buf);
		}
		else
		{
			m_bCommitting = true;
		}
	}
	else
	{
		m_bCancelling = true;
	}

	this->ShowWindow(SW_HIDE);
	m_row = -1;
	m_col = -1;
	m_editType = SW_EDIT_NONE;
	m_bCommitting = false;
	m_bCancelling = false;
	return true;
}

void CSWInplaceEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Pass through: delete / control / non-ASCII (IME / CJK)
	if (nChar == VK_BACK || nChar == VK_RETURN || nChar == VK_ESCAPE || nChar == VK_TAB || nChar > 127)
	{
		CEdit::OnChar(nChar, nRepCnt, nFlags);
		return;
	}

	if (m_editType == SW_EDIT_INT || m_editType == SW_EDIT_FLOAT)
	{
		if (nChar >= '0' && nChar <= '9')
		{
			CEdit::OnChar(nChar, nRepCnt, nFlags);
			return;
		}
		if (nChar == '-')
		{
			int ss = 0, se = 0;
			this->GetSel(ss, se);
			if (ss == 0)
			{
				CEdit::OnChar(nChar, nRepCnt, nFlags);
			}
			return;
		}
		if (m_editType == SW_EDIT_FLOAT && nChar == '.')
		{
			char buf[64] = {0};
			::GetWindowTextA(this->GetSafeHwnd(), buf, (int)sizeof(buf));
			if (strchr(buf, '.') == NULL)
			{
				CEdit::OnChar(nChar, nRepCnt, nFlags);
			}
			return;
		}
		return;
	}

	CEdit::OnChar(nChar, nRepCnt, nFlags);
}

UINT CSWInplaceEdit::OnGetDlgCode()
{
	return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS;
}

void CSWInplaceEdit::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_RETURN)
	{
		// 经 owner 走：owner->_endCellEdit 会清 m_bEditing/m_editRow/m_editCol 并 Invalidate，
		// 让被编辑的 cell 在下一帧正常绘制（OnDraw 里会跳过 m_bEditing 的 cell，不复位就空白）。
		if (m_pOwner) m_pOwner->_endCellEdit(true); else EndEdit(true);
		return;
	}
	if (nChar == VK_ESCAPE)
	{
		if (m_pOwner) m_pOwner->_endCellEdit(false); else EndEdit(false);
		return;
	}
	CEdit::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CSWInplaceEdit::OnKillFocus(CWnd* pNewWnd)
{
	CEdit::OnKillFocus(pNewWnd);
	if (m_bCommitting || m_bCancelling)
	{
		return;
	}
	if (m_pOwner)
	{
		m_pOwner->_endCellEdit(false);
	}
}
