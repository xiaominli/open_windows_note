#pragma once
#include <afxwin.h>
#include "ui/table/SWTableScrollViewDefs.h"

class SWTableScrollViewWnd;

// Inline cell editor for SWTableScrollViewWnd.
// BeginEdit: show, place, set initial text, select all.
// EndEdit(true): validate + commit via callback; EndEdit(false): cancel.
// Enter = commit; ESC / KillFocus = cancel.
class CSWInplaceEdit : public CEdit
{
public:
	SWTableScrollViewWnd* m_pOwner;
	int  m_row;          // 1-based
	int  m_col; 
	int  m_editType;
	bool m_bCommitting;
	bool m_bCancelling;
	HFONT m_hEditFont;   // 编辑框专用字体，比 label 大一号；CreateInplace 中创建，析构删除

	CSWInplaceEdit();
	virtual ~CSWInplaceEdit();

	BOOL CreateInplace(SWTableScrollViewWnd* owner);
	void BeginEdit(int row, int col, int editType, const CRect& clientRect, const char* initText);
	bool EndEdit(bool commit);

protected:
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	DECLARE_MESSAGE_MAP()
};
