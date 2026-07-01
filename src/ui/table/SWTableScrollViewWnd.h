#pragma once
#include <afxwin.h>
#include <string>
#include <vector>
#include "ui/table/SWTableScrollViewDefs.h"
#include "ui/table/SWInplaceEdit.h"



class SWTableScrollViewWnd;
class ISWTableScrollViewWndCallback
{
public:
	virtual void onTableScrollViewDrawCell(HDC hdc, SWTableScrollViewWnd* sender, int row, int col, CRect rect, int align) = 0;
	virtual void onTableScrollViewLeftMouseClick(SWTableScrollViewWnd* sender, int row, int col) = 0;
	virtual void onTableScrollViewRightMouseClick(SWTableScrollViewWnd* sender, int row, int col) = 0;
	virtual void onTableScrollViewLeftMouseDblClick(SWTableScrollViewWnd* sender, int row, int col) = 0;
	virtual void onTableScrollViewSortColumn(SWTableScrollViewWnd* sender, int col, int order) = 0;
	virtual int onTableScrollViewAutoAdjustColumnWdidth(SWTableScrollViewWnd* sender, int col) = 0;

	// 每帧数据区绘制前后触发一次：调用方可在此一次性加锁拷贝数据、应用排序
	// firstVisibleRow / lastVisibleRow 是 0-based 行索引（闭区间：[first, last)）
	virtual void onTableScrollViewPrepareDraw(SWTableScrollViewWnd* /*sender*/, int /*firstVisibleRow*/, int /*lastVisibleRow*/) {}
	virtual void onTableScrollViewFinishDraw(SWTableScrollViewWnd* /*sender*/) {}

	// ===== 可编辑 cell 相关（阶段 B） =====
	// 取 cell 当前值（进入编辑时初始化 edit 控件文本）
	virtual void onTableScrollViewGetCellEditText(SWTableScrollViewWnd* /*sender*/, int /*row*/, int /*col*/, char* buf, int bufSize)
	{
		if (buf && bufSize > 0) buf[0] = 0;
	}
	// 校验用户输入；返回 false 则不 commit、保持编辑焦点
	virtual bool onTableScrollViewValidateCellEdit(SWTableScrollViewWnd* /*sender*/, int /*row*/, int /*col*/, const char* /*newText*/)
	{
		return true;
	}
	// 提交（校验通过后调用）
	virtual void onTableScrollViewCommitCellEdit(SWTableScrollViewWnd* /*sender*/, int /*row*/, int /*col*/, const char* /*newText*/) {}
	// 在可编辑 cell 上滚动鼠标滚轮：返回 true 表示已处理（表格不滚动），false 走原滚动逻辑
	virtual bool onTableScrollViewCellMouseWheel(SWTableScrollViewWnd* /*sender*/, int /*row*/, int /*col*/, short /*zDelta*/)
	{
		return false;
	}

	// 数据区之外的空白点击：默认空实现；列表类窗口可在此清掉本地选中并广播给关联窗口。
	// ★必须放在接口最末尾，避免新增槽位扰动其它虚方法的 vtable 偏移。
	virtual void onTableScrollViewLeftClickEmpty(SWTableScrollViewWnd* /*sender*/) {}

	// ── M7.A (2026-05-23): 分组行 hook (非侵入) ──
	//   策略窗口按账户分组用; 旧 caller 不实现, 走 isGroupRow 默认 false → 完全等价老路径.
	//   row 是 1-based (与 drawCell row 同口径).
	//   isGroupRow=true 时: OnDraw 跳过分列 drawCell 调用, 改调 drawGroupRow 整行画;
	//                       click 也优先走 onGroupRowClick (toggle 折叠/展开).
	virtual bool onTableScrollViewIsGroupRow(SWTableScrollViewWnd* /*sender*/, int /*row*/) { return false; }
	virtual void onTableScrollViewDrawGroupRow(HDC /*hdc*/, SWTableScrollViewWnd* /*sender*/, int /*row*/, CRect /*rect*/) {}
	virtual void onTableScrollViewGroupRowClick(SWTableScrollViewWnd* /*sender*/, int /*row*/) {}
};


class SWTableScrollViewWnd : public CWnd
{
	//DECLARE_DYNAMIC(SWTableScrollViewWnd)
public:
	ISWTableScrollViewWndCallback* m_pCallback = NULL;

	SWTableScrollViewWnd();
	virtual ~SWTableScrollViewWnd();

	void setTableScrollViewCallback(ISWTableScrollViewWndCallback* callback)
	{
		m_pCallback = callback;
	}
	void setMarginRect(CRect rect);
	void setColumnInfos(const char* windowToken, std::vector<TABLE_VIEW_COLUMN_INFO*> vColumnInfo);
	void setTotalRowCount(int rowCount);

	// 数据变化后触发一次完整的列宽自动重算（回调 + 列名下限）
	void autoResizeColumns();
	int _measureColumnNameWidth(const char* name);

	// 冻结前 N 列：这些列不随横向滚动
	void setFrozenColumnCount(int n);

	int _loadTableColumnWidthFromFile(const char* windowToken, std::vector<TABLE_VIEW_COLUMN_INFO*> vColumnInfo);
	int _saveTableColumnWidthToFile(const char* windowToken, std::vector<TABLE_VIEW_COLUMN_INFO*> vColumnInfo);

	void _processCancelMode(CPoint point);

	void _reCalculateSize();
	void _nextPage();
	void _prevPage();
	void _onRendScrollBar(CDC* pDC, CRect rectScrollBar, CPoint ptOffset);
	void _onRendVertScrollBar(CDC* pDC, CRect rectScrollBar, CPoint ptOffset);
	CRect _getThumbRect(CRect rect);
	CRect _getVertThumbRect(CRect rect);
	int _posFromThumb(int thumb, const CSize& szClamp);
	int _posFromVertThumb(int thumb, const CSize& szClamp);

	TABLE_SCROLL_VIEW_HITTEST _doHitTest(const CPoint& pt);


	void OnAutoAdjustColunWidth();
public:
	int m_rowCount = 0;
	CRect m_rectMargin;  // left top right bottom


	void OnDraw(CDC* pDC, CRect rect);

	// Attributes
public:
	CFont m_captionFont;;
	CFont m_labelFont;;

	HFONT m_hCaptionFont;
	HFONT m_hLabelFont;

	bool m_isAutoColumn = true;
	bool m_isAutoAdjuested = false;
	// 用户已经明确设置过列宽（加载了 JSON 或 拖过列分隔线）。
	// 置位后 _reCalculateSize 不再跑内容自适应，避免数据刷新时把持久化/拖动
	// 的宽度冲掉。OnAutoAdjustColunWidth 菜单命令是唯一会清零它的入口。
	bool m_hasUserColumnWidths = false;

	int columnPandding = 10;

	bool m_isMouseFirstEnter = true;

	char windowToken[64];
	std::vector<TABLE_VIEW_COLUMN_INFO*> m_vColumnInfos;
	int m_nFrozenColumnCount = 0;
	int m_nFrozenColumnsWidth = 0;  // 缓存：前 N 列累计 actualWidth
	int m_singalRowHeight = 40;
	int m_onePageRowCount = 0;
	int m_totalPages = 0;
	int m_currentPage = 0;


	int m_nScrollPos = 0;
	int m_nScrolPage = 0;

	int m_nVertScrollPos = 0;     // 纵向像素偏移
	int m_nVertScrolPage = 0;     // 纵向可视区高度

	CRect m_rectScrollHoriz;
	CRect m_rectScrollVert;
	CRect m_rectThumb;
	CRect m_rectVertThumb;

	TABLE_SCROLL_VIEW_HITTEST	m_HitTest;
	TABLE_SCROLL_VIEW_HITTEST m_HitPressed;
	CPoint			m_ptTrackStart;
	CRect			m_rectTrackThumb;
	CRect			m_rectTrackThumbStart;
	int				m_TrackPos;

	CSize m_totalSize;
	CSize m_clientSize;
	CSize m_actualSize;

	CPoint m_mouse_point;
	int mouse_row = -1;
	int mouse_col = -1;
	int m_current_sel_row = -1;

	CPoint m_mouse_pressed_pt;
	bool m_bMouseTracking = false;
	bool m_bMousePressed = false;
	int m_mouseSelectColumnLine = -1;
	int checkMouseColumnLine(CPoint point);

	// ===== 可编辑 cell（阶段 B） =====
	CSWInplaceEdit m_inplaceEdit;
	bool m_bEditing = false;
	int  m_editRow = -1;  // 1-based（与 mouse_row 对齐）
	int  m_editCol = -1;
	CRect _getCellClientRect(int row, int col);
	void  _beginCellEdit(int row, int col);
	void  _endCellEdit(bool commit);
protected:
	HDC m_hdc_mem = NULL;
	HBITMAP m_bmp_mem = NULL;
	CRect m_cached_hdc_rect;
	void _recreate_mem_dc_if_needed(HDC hdc, const CRect& rect);

public:

	void OnLButtonDown(UINT nFlags, CPoint point);
	void OnLButtonUp(UINT nFlags, CPoint point);
	void OnLButtonDblClk(UINT nFlags, CPoint point);
	void OnRButtonDown(UINT nFlags, CPoint point);
	void OnMouseMove(UINT nFlags, CPoint point);
	BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	void OnTimer(UINT_PTR nIDEvent);
	void OnMouseLeave();
	void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

};

