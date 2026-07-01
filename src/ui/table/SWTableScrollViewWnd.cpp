#include "ui/table/SWTableScrollViewWnd.h"
#include "ui/table/TableViewShim.h"
#include <string>
#include <vector>
#include <algorithm>
#include <math.h>


#define COLUME_MOUSE_SIZING_WIDTH 10

// 原框架头里定义的常量，移入后本地补齐
#define TABLE_SCROLL_VIEW_TIMER_ID 1001
#define WM_DOCKING_WINDOW_AUTO_COLUMN_WIDTH (WM_USER + 217)


//IMPLEMENT_DYNCREATE(SWTableScrollViewWnd, CWnd)

BEGIN_MESSAGE_MAP(SWTableScrollViewWnd, CWnd)
	//{{AFX_MSG_MAP(SWTableScrollViewWnd)
	//}}AFX_MSG_MAP
	ON_WM_CREATE()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_MOUSELEAVE()
	ON_WM_PAINT()
	ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

SWTableScrollViewWnd::SWTableScrollViewWnd()
{
	// 表头与单元格都用 m_hLabelFont（与账户余额等其它窗口一致），避免表头加粗导致
	// 视觉字号偏大；旧值 m_hPlotTitleFont 是同字号但 FW_BOLD
	m_hCaptionFont = m_global_graphic_objects.m_hLabelFont;
	m_hLabelFont = m_global_graphic_objects.m_hLabelFont;

	// 行高 = labelFont 高度 + 上下各 4 像素留白
	m_singalRowHeight = m_global_graphic_objects.m_nLabelFontHeight + 8;

	m_actualSize = CSize(0, 0);

	m_rectMargin.SetRectEmpty();
}

SWTableScrollViewWnd::~SWTableScrollViewWnd()
{
	this->_saveTableColumnWidthToFile(windowToken, this->m_vColumnInfos);

	// columnInfo 所有权归 SWTableScrollViewWnd：循环 delete 后再 swap 释放容量。
	// 外部窗口（dialog/docking）只负责 new + setColumnInfos，不再删除。
	for (auto* pColumn : m_vColumnInfos)
	{
		delete pColumn;
	}
	std::vector<TABLE_VIEW_COLUMN_INFO*>().swap(m_vColumnInfos);

	// 释放旧资源
	if (m_bmp_mem && m_hdc_mem)
	{
		SelectObject(m_hdc_mem, GetStockObject(NULL_BRUSH)); // 先解除 HBITMAP 绑定
		DeleteObject(m_bmp_mem);
		m_bmp_mem = NULL;
	}
	if (m_hdc_mem)
	{
		DeleteDC(m_hdc_mem);
		m_hdc_mem = NULL;
	}
}


void SWTableScrollViewWnd::setMarginRect(CRect rect)
{
	this->m_rectMargin = rect;
	this->_reCalculateSize();
}

int SWTableScrollViewWnd::_measureColumnNameWidth(const char* name)
{
	if (name == NULL || name[0] == 0)
	{
		return 0;
	}
	CDC* pDC = this->GetDC();
	if (pDC == NULL)
	{
		return 0;
	}
	HDC hdc = pDC->GetSafeHdc();
	HFONT hFont = (HFONT)this->m_captionFont.GetSafeHandle();
	HGDIOBJ hOld = NULL;
	if (hFont)
	{
		hOld = SelectObject(hdc, hFont);
	}
	SIZE sz = {0};
	GetTextExtentPoint32A(hdc, name, (int)strlen(name), &sz);
	if (hOld)
	{
		SelectObject(hdc, hOld);
	}
	this->ReleaseDC(pDC);
	return sz.cx;
}

void SWTableScrollViewWnd::autoResizeColumns()
{
	this->m_isAutoAdjuested = false;
	this->_reCalculateSize();
	this->Invalidate();
}

void SWTableScrollViewWnd::setFrozenColumnCount(int n)
{
	if (n < 0) { n = 0; }
	this->m_nFrozenColumnCount = n;
	this->_reCalculateSize();
	this->Invalidate();
}

// row: 1-based；返回 cell 在 client 坐标系下的矩形（用于定位 inplace edit）
CRect SWTableScrollViewWnd::_getCellClientRect(int row, int col)
{
	CRect rectClient;
	GetClientRect(&rectClient);
	CRect rectTable = rectClient;
	rectTable.DeflateRect(m_rectMargin);

	if (col < 0 || col >= (int)m_vColumnInfos.size() || row < 1)
	{
		return CRect(0, 0, 0, 0);
	}

	int firstRow = (m_singalRowHeight > 0) ? (m_nVertScrollPos / m_singalRowHeight) : 0;
	int rowOffsetY = (m_singalRowHeight > 0) ? (m_nVertScrollPos % m_singalRowHeight) : 0;

	// 与 OnDraw 一致：bitmap y = (m_singalRowHeight + 1) + (zeroBasedRow - firstRow) * m_singalRowHeight - rowOffsetY
	int bitmapY = m_singalRowHeight + 1 + ((row - 1) - firstRow) * m_singalRowHeight - rowOffsetY;
	int screenY = rectTable.top + bitmapY;

	float actualLeft = m_vColumnInfos[col]->actualLeft;
	float actualWidth = m_vColumnInfos[col]->actualWidth;
	int screenX;
	if (col < m_nFrozenColumnCount)
	{
		// 冻结列：bitmap 0 → 屏幕 rectTable.left
		screenX = rectTable.left + (int)actualLeft;
	}
	else
	{
		// 滚动列：bitmap (frozenWidth + scrollPos) → 屏幕 (rectTable.left + frozenWidth)
		//      即：bitmap x = actualLeft → 屏幕 x = rectTable.left + actualLeft - scrollPos
		screenX = rectTable.left + (int)actualLeft - m_nScrollPos;
	}

	CRect rc(screenX, screenY, screenX + (int)actualWidth - 1, screenY + m_singalRowHeight - 1);

	// 裁到"数据可见区域"，避免右/下边的 cell 矩形盖到滚动条或溢出表格
	int rightLimit = rectTable.right;
	if (!m_rectScrollVert.IsRectEmpty()) rightLimit = m_rectScrollVert.left;
	int bottomLimit = rectTable.bottom;
	if (!m_rectScrollHoriz.IsRectEmpty()) bottomLimit = m_rectScrollHoriz.top;
	if (rc.right  > rightLimit)  rc.right  = rightLimit;
	if (rc.bottom > bottomLimit) rc.bottom = bottomLimit;

	return rc;
}

void SWTableScrollViewWnd::_beginCellEdit(int row, int col)
{
	if (col < 0 || col >= (int)m_vColumnInfos.size() || row < 1)
	{
		return;
	}

	// 若已有编辑会话 → 先 cancel 旧的
	if (m_bEditing)
	{
		_endCellEdit(false);
	}

	char buf[512] = {0};
	if (m_pCallback)
	{
		m_pCallback->onTableScrollViewGetCellEditText(this, row, col, buf, sizeof(buf));
	}

	CRect rc = _getCellClientRect(row, col);
	int editType = m_vColumnInfos[col]->edit_type;

	m_bEditing = true;
	m_editRow = row;
	m_editCol = col;

	m_inplaceEdit.BeginEdit(row, col, editType, rc, buf);
	this->Invalidate();
}

void SWTableScrollViewWnd::_endCellEdit(bool commit)
{
	if (!m_bEditing)
	{
		return;
	}
	bool actuallyEnded = m_inplaceEdit.EndEdit(commit);
	if (!actuallyEnded)
	{
		// Validate 失败，保持编辑状态
		return;
	}
	m_bEditing = false;
	m_editRow = -1;
	m_editCol = -1;
	this->Invalidate();
}

void SWTableScrollViewWnd::setColumnInfos(const char* windowToken, std::vector<TABLE_VIEW_COLUMN_INFO*> vColumnInfo)
{
	m_vColumnInfos = vColumnInfo;
	this->m_rowCount = 0;

	strcpy(this->windowToken, windowToken);
	int isLoad = this->_loadTableColumnWidthFromFile(windowToken, vColumnInfo);
	if (isLoad == 0)
	{
		// 已成功读取持久化宽度：屏蔽后续内容自适应，避免被覆盖
		this->m_hasUserColumnWidths = true;
	}
	if (isLoad != 0)
	{
		////////////////////////////////////////////////////////////////////////////////////\
		// 字体宽度累加
		CDC* pDC = this->GetDC();
		int nColumnCount = m_vColumnInfos.size();
		for (int i = 0; i < nColumnCount; i++)
		{
			if (m_vColumnInfos[i]->width == 0)
			{
				m_vColumnInfos[i]->width = SWPlotUtil::quick_estimate_word_size_with_cache(pDC->GetSafeHdc(), m_vColumnInfos[i]->columnName, (HFONT)this->m_captionFont.GetSafeHandle()).cx + 20;
			}
		}
		this->_saveTableColumnWidthToFile(windowToken, vColumnInfo);
		this->ReleaseDC(pDC);
	}

	this->_reCalculateSize();
}

void SWTableScrollViewWnd::setTotalRowCount(int rowCount)
{
	m_rowCount = rowCount;

	// 列宽拖动期间冻结自动重排: setTotalRowCount 会清 m_isAutoAdjuested 触发
	// _reCalculateSize 的自动列宽分支, 这条路径会重算 actualWidth, 覆盖用户
	// 按下时的列宽 → OnLButtonUp 的 newPx = actualWidth + delta 语义错位.
	// 等鼠标抬起后再恢复重算.
	if (m_bMousePressed && m_mouseSelectColumnLine > 0)
	{
		return;
	}

	this->m_isAutoAdjuested = false;

	this->_reCalculateSize();
}

int SWTableScrollViewWnd::_loadTableColumnWidthFromFile(const char* /*windowToken*/, std::vector<TABLE_VIEW_COLUMN_INFO*> /*vColumnInfo*/)
{
	return 0;   // P4 不持久化列宽（去 JsonCpp 依赖）；后续可接 SettingsStore
}

int SWTableScrollViewWnd::_saveTableColumnWidthToFile(const char* /*windowToken*/, std::vector<TABLE_VIEW_COLUMN_INFO*> /*vColumnInfo*/)
{
	return 0;
}


void SWTableScrollViewWnd::_processCancelMode(CPoint point)
{
	ReleaseCapture();

	m_ptTrackStart = CPoint(0, 0);

	CPoint ptScreen(point);

	this->ClientToScreen(&ptScreen);

	TABLE_SCROLL_VIEW_HITTEST hit = _doHitTest(ptScreen);

	if (m_HitPressed != TSV_HT_NOWHERE ||
		m_HitTest != hit)
	{
		m_HitTest = hit;
		m_HitPressed = TSV_HT_NOWHERE;

		this->RedrawWindow();
	}
}

void SWTableScrollViewWnd::_nextPage()
{
	m_currentPage++;
	if (m_currentPage >= m_totalPages)
	{
		m_currentPage = m_totalPages - 1;
	}
	this->Invalidate();
}

void SWTableScrollViewWnd::_prevPage()
{
	m_currentPage--;
	if (m_currentPage < 0)
	{
		m_currentPage = 0;
	}
	this->Invalidate();
}

void SWTableScrollViewWnd::_reCalculateSize()
{
	CDC* pDC = this->GetDC();

	CRect clientRect;
	this->GetClientRect(&clientRect);

	/////////////////////////////////////////////////////
	// 对于有margin
	CRect rectTable = clientRect;
	rectTable.DeflateRect(m_rectMargin);

	int nColumnCount = m_vColumnInfos.size();
	int clientW = rectTable.Width();
	if (clientW < 0) { clientW = 0; }

	////////////////////////////////////////////////////////////////////////////////////\
	// width 字段语义 (Cut C方案):
	//   width > 1.0       → 绝对像素 (固定列, 用户拖动后或显式设置)
	//   0 < width <= 1.0  → 占剩余宽度的百分比 (弹性列, 仅初始化默认布局用)
	//   width <= 0        → 由列名 nameMinW 兜底
	// 拖动后: OnLButtonUp 会把被拖动列 width 设为 actualWidth+delta (>1.0 进绝对像素),
	//        其他未拖动的弹性列保持百分比, 重新分配剩余空间.
	// 弹性列分配: 先扣除固定像素列, 剩余按 (w/sumPct) 归一化填满, 不论 sumPct 大小;
	//          最后一个弹性列吸收 floor() 舍入余数, 保证 totalWidth=clientW.
	double sumPct = 0.0;
	int sumFixedPx = 0;
	for (int i = 0; i < nColumnCount; i++)
	{
		float w = m_vColumnInfos[i]->width;
		if (w > 1.0f)      { sumFixedPx += (int)w; }
		else if (w > 0.0f) { sumPct += w; }
	}
	int remainingForPct = clientW - sumFixedPx;
	if (remainingForPct < 0) { remainingForPct = 0; }
	// 归一化: 弹性列总是填满 remainingForPct, 不论 sumPct 是 0.7 / 1.0 / 1.5
	double pctDivisor = (sumPct > 0.0) ? sumPct : 1.0;
	int lastPctIdx = -1;
	if (sumPct > 0.0)
	{
		for (int i = nColumnCount - 1; i >= 0; i--)
		{
			float w = m_vColumnInfos[i]->width;
			if (w > 0.0f && w <= 1.0f) { lastPctIdx = i; break; }
		}
	}
	int distributedPctW = 0;
	int totalWidth = 0;
	for (int i = 0; i < nColumnCount; i++)
	{
		if (i == 0)
		{
			m_vColumnInfos[i]->actualLeft = 0;
		}
		else
		{
			m_vColumnInfos[i]->actualLeft = m_vColumnInfos[i - 1]->actualLeft + m_vColumnInfos[i - 1]->actualWidth;
		}
		float w = m_vColumnInfos[i]->width;
		int colPx = 0;
		if (w > 1.0f)
		{
			colPx = (int)w;
		}
		else if (w > 0.0f)
		{
			if (i == lastPctIdx)
			{
				colPx = remainingForPct - distributedPctW;
				if (colPx < 0) { colPx = 0; }
			}
			else
			{
				colPx = (int)floor((double)remainingForPct * (w / pctDivisor));
				distributedPctW += colPx;
			}
		}
		// 列名宽度兜底 (不修改 width 字段, 保留用户原始百分比/像素语义)
		int nameMinW = this->_measureColumnNameWidth(m_vColumnInfos[i]->columnName) + columnPandding * 2;
		if (colPx < nameMinW) { colPx = nameMinW; }
		m_vColumnInfos[i]->actualWidth = (float)colPx;
		totalWidth += colPx;
	}

	// 防溢出: 固定列被 nameMinW 兜底拉宽后, totalWidth 可能 > clientW,
	// 弹性列首轮分配未感知拉伸量 -> 出现不必要的横向滚动条.
	// 从弹性列尾部依次扣减溢出像素 (每列保底 nameMinW), 把总宽压回 clientW.
	if (clientW > 0 && totalWidth > clientW)
	{
		int overflow = totalWidth - clientW;
		for (int i = nColumnCount - 1; i >= 0 && overflow > 0; i--)
		{
			float w = m_vColumnInfos[i]->width;
			if (!(w > 0.0f && w <= 1.0f)) continue;  // 仅扣弹性列
			int curW = (int)m_vColumnInfos[i]->actualWidth;
			int nameMinW = this->_measureColumnNameWidth(m_vColumnInfos[i]->columnName) + columnPandding * 2;
			int canShrink = curW - nameMinW;
			if (canShrink <= 0) continue;
			int take = (overflow < canShrink) ? overflow : canShrink;
			m_vColumnInfos[i]->actualWidth = (float)(curW - take);
			overflow -= take;
		}
		// 重算 actualLeft 和 totalWidth
		totalWidth = 0;
		for (int i = 0; i < nColumnCount; i++)
		{
			m_vColumnInfos[i]->actualLeft = (i == 0) ? 0.0f
				: m_vColumnInfos[i - 1]->actualLeft + m_vColumnInfos[i - 1]->actualWidth;
			totalWidth += (int)m_vColumnInfos[i]->actualWidth;
		}
	}
	m_totalSize.cx = totalWidth;

	// 自动调整宽度（仅当用户从未手动设置/持久化宽度时；弹性列保留百分比, 不参与自动调整）
	if (this->m_isAutoColumn && this->m_pCallback && !this->m_hasUserColumnWidths)
	{
		if (!this->m_isAutoAdjuested)
		{
			totalWidth = 0;
			for (int i = 0; i < nColumnCount; i++)
			{
				if (i == 0)
				{
					m_vColumnInfos[i]->actualLeft = 0;
				}
				else
				{
					m_vColumnInfos[i]->actualLeft = m_vColumnInfos[i - 1]->actualLeft + m_vColumnInfos[i - 1]->actualWidth;
				}
				float w = m_vColumnInfos[i]->width;
				if (w > 0.0f && w <= 1.0f)
				{
					// 弹性列：保留百分比 width 不被覆盖, actualWidth 已在上面赋好
					totalWidth += (int)m_vColumnInfos[i]->actualWidth;
					continue;
				}
				int contentW = this->m_pCallback->onTableScrollViewAutoAdjustColumnWdidth(this, i);
				int nameW = this->_measureColumnNameWidth(m_vColumnInfos[i]->columnName);
				int innerW = contentW > nameW ? contentW : nameW;
				m_vColumnInfos[i]->width = (float)(innerW + columnPandding * 2);
				m_vColumnInfos[i]->actualWidth = m_vColumnInfos[i]->width;
				totalWidth += (int)m_vColumnInfos[i]->width;
			}
			m_totalSize.cx = totalWidth;
			this->m_isAutoAdjuested = true;
		}
	}



	// 冻结列宽度（此阶段 actualWidth == width，后面放大分支只发生在不需要横滚时，互不影响）
	m_nFrozenColumnsWidth = 0;
	{
		int frozenMax = m_nFrozenColumnCount;
		if (frozenMax > nColumnCount) { frozenMax = nColumnCount; }
		for (int i = 0; i < frozenMax; i++)
		{
			m_nFrozenColumnsWidth += (int)m_vColumnInfos[i]->actualWidth;
		}
	}
	int rollableTotalW = totalWidth - m_nFrozenColumnsWidth;
	int rollableViewW  = rectTable.Width() - m_nFrozenColumnsWidth;
	if (rollableTotalW < 0) { rollableTotalW = 0; }
	if (rollableViewW  < 0) { rollableViewW  = 0; }

	m_rectScrollHoriz.SetRectEmpty();
	if (rollableViewW < rollableTotalW)
	{
		m_rectScrollHoriz = rectTable;
		/////////////////////////////////////////////////////
		// 对于有margin
		m_rectScrollHoriz.bottom -= m_rectMargin.top;
		m_rectScrollHoriz.top = m_rectScrollHoriz.bottom - GetSystemMetrics(SM_CYHSCROLL);
		// 滚动条从冻结列右侧开始
		m_rectScrollHoriz.left = rectTable.left + m_nFrozenColumnsWidth;

		int scrollMax = rollableTotalW - rollableViewW;
		if (m_nScrollPos > scrollMax) { m_nScrollPos = scrollMax; }
		if (m_nScrollPos < 0)         { m_nScrollPos = 0; }
	}
	else
	{
		m_nScrollPos = 0;
	}
	m_nScrolPage = rollableViewW;



	//初始高度
	m_onePageRowCount = floor((rectTable.Height() - m_rectScrollHoriz.Height()) / m_singalRowHeight) - 1;
	if (m_onePageRowCount < 1)
	{
		// 极小窗口保护：避免后续除零 / 负数循环
		m_onePageRowCount = 1;
	}
	m_totalSize.cy = m_singalRowHeight + m_singalRowHeight * m_onePageRowCount;

	m_totalPages = ceil(m_rowCount / (double)m_onePageRowCount);
	if (m_currentPage >= m_totalPages)
	{
		m_currentPage = 0;
	}

	/////////////////////////////////////////////////////////////////////////////////////
	// 竖向滚动条计算
	int dataAreaH = rectTable.Height() - m_singalRowHeight - m_rectScrollHoriz.Height();
	if (dataAreaH < 0)
	{
		dataAreaH = 0;
	}
	int totalDataH = m_rowCount * m_singalRowHeight;
	m_nVertScrolPage = dataAreaH;
	m_rectScrollVert.SetRectEmpty();
	if (totalDataH > dataAreaH && dataAreaH > 0)
	{
		int vScrollW = GetSystemMetrics(SM_CXVSCROLL);
		m_rectScrollVert.top = rectTable.top + m_singalRowHeight;
		m_rectScrollVert.bottom = m_rectScrollVert.top + dataAreaH;
		m_rectScrollVert.right = rectTable.right;
		m_rectScrollVert.left = rectTable.right - vScrollW;
		// 横向滚动条要让出右下角给竖向按钮区
		if (!m_rectScrollHoriz.IsRectEmpty())
		{
			m_rectScrollHoriz.right -= vScrollW;
		}
	}
	// clamp 纵向偏移
	int vMax = totalDataH - m_nVertScrolPage;
	if (vMax < 0) { vMax = 0; }
	if (m_nVertScrollPos > vMax) { m_nVertScrollPos = vMax; }
	if (m_nVertScrollPos < 0) { m_nVertScrollPos = 0; }

	m_actualSize.cx = totalWidth;
	m_actualSize.cy = rectTable.bottom - rectTable.top;

	/////////////////////////////////////////////////////////////////////////////////////

	int client_cx = rectTable.Width();
	// 弹性列 (sumPct > 0) 已在上方分配填满 clientW, 不再走"放大"等比缩放分支
	// (否则固定像素列也会被一起放大, 破坏固定语义)
	if (client_cx <= m_totalSize.cx || sumPct > 0.0)
	{
		// 如果窗口大小小于累计列宽 / 已用弹性列填充
		m_actualSize.cx = totalWidth;
		m_actualSize.cy = rectTable.bottom - rectTable.top;
	}
	else
	{
		// 如果窗口大小大于累计列宽， 放大
		totalWidth = 0;
		for (int i = 0; i < nColumnCount; i++)
		{
			if (i == 0)
			{
				m_vColumnInfos[i]->actualLeft = 0;
			}
			else
			{
				m_vColumnInfos[i]->actualLeft = m_vColumnInfos[i - 1]->actualLeft + m_vColumnInfos[i - 1]->actualWidth;
			}
			m_vColumnInfos[i]->actualWidth = ceil((client_cx / (double)m_totalSize.cx) * m_vColumnInfos[i]->width);
			if (i == nColumnCount - 1)
			{
				m_vColumnInfos[i]->actualWidth = client_cx - totalWidth;
			}
			totalWidth += m_vColumnInfos[i]->actualWidth;
		}
		m_actualSize.cx = totalWidth;
		m_actualSize.cy = rectTable.bottom - rectTable.top;
	}

	this->ReleaseDC(pDC);
}


BOOL SWTableScrollViewWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// 可编辑 cell 优先：把滚轮事件路由给回调（例如滚轮改价差）
	if (mouse_row >= 1 && mouse_col >= 0
		&& mouse_col < (int)m_vColumnInfos.size()
		&& m_vColumnInfos[mouse_col]->can_edit)
	{
		if (m_pCallback && m_pCallback->onTableScrollViewCellMouseWheel(this, mouse_row, mouse_col, zDelta))
		{
			this->Invalidate();
			return TRUE;
		}
	}

	// 进入滚表分支前：若当前正在编辑 → cancel，避免 edit 位置和 cell 脱位
	if (m_bEditing)
	{
		_endCellEdit(false);
	}

	// 像素级纵向滚动：每次 1 行
	int step = m_singalRowHeight;
	if (zDelta > 0)
	{
		m_nVertScrollPos -= step;
	}
	else
	{
		m_nVertScrollPos += step;
	}
	int totalH = m_rowCount * m_singalRowHeight;
	int vMax = totalH - m_nVertScrolPage;
	if (vMax < 0) { vMax = 0; }
	if (m_nVertScrollPos > vMax) { m_nVertScrollPos = vMax; }
	if (m_nVertScrollPos < 0) { m_nVertScrollPos = 0; }
	this->Invalidate();
	return FALSE;
}

void SWTableScrollViewWnd::OnSize(UINT nType, int cx, int cy)
{
	// resize 时取消编辑，避免 edit 位置错位
	if (m_bEditing)
	{
		_endCellEdit(false);
	}
	this->_reCalculateSize();
	this->Invalidate();
}

void SWTableScrollViewWnd::_recreate_mem_dc_if_needed(HDC hdc, const CRect& rect)
{
	// 0/负尺寸保护：GDI 不允许 0 宽/高位图
	if (rect.Width() <= 0 || rect.Height() <= 0)
	{
		return;
	}
	if (m_hdc_mem == NULL || rect != m_cached_hdc_rect)
	{
		// 释放旧资源
		if (m_bmp_mem && m_hdc_mem)
		{
			SelectObject(m_hdc_mem, GetStockObject(NULL_BRUSH)); // 先解除 HBITMAP 绑定
			DeleteObject(m_bmp_mem);
			m_bmp_mem = NULL;
		}
		if (m_hdc_mem)
		{
			DeleteDC(m_hdc_mem);
			m_hdc_mem = NULL;
		}

		// 创建新兼容DC和位图
		m_hdc_mem = CreateCompatibleDC(hdc);
		if (m_hdc_mem)
		{
			m_bmp_mem = CreateCompatibleBitmap(hdc, rect.Width(), rect.Height());
			if (m_bmp_mem)
			{
				SelectObject(m_hdc_mem, m_bmp_mem);
			}
			else
			{
				DeleteDC(m_hdc_mem);
				m_hdc_mem = NULL;
			}
		}

		// 记录当前尺寸
		m_cached_hdc_rect = rect;
	}
}

void SWTableScrollViewWnd::OnDraw(CDC* pDC, CRect rect)
{
	int nColumnCount = m_vColumnInfos.size();
	HDC hdc = pDC->GetSafeHdc();

	/////////////////////////////////////////////////////
	// 对于有margin
	CRect rectTable = rect;
	rectTable.DeflateRect(m_rectMargin);

	// 极小尺寸保护：不可见时不画
	if (rectTable.Width() <= 0 || rectTable.Height() <= 0)
	{
		return;
	}


	// 整个扩展绘图区
	CRect rectPaint;
	rectPaint.SetRect(0, 0, m_actualSize.cx, rectTable.bottom - rectTable.top);
	if (rectTable.Width() > rectPaint.Width())
	{
		rectPaint.right = rectPaint.left + rectTable.Width();
	}

	// 创建内存dc
	_recreate_mem_dc_if_needed(hdc, rectPaint);


	///////////////////////////////////////////////////////////////////////////////////////////////////
	// 双缓冲画图
	CDC cdc_mem;
	cdc_mem.Attach(m_hdc_mem);
	CDC* pDCMem = &cdc_mem;;


	CPoint scrollPos = CPoint(m_nScrollPos, 0);// GetScrollPosition();

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// draw 背景：必须用偏移前的 rectPaint（覆盖整个 bitmap [0, m_actualSize.cx)），
	// 不能用 OffsetRect(-scrollPos) 之后的 rect——那会被 GDI 裁剪到 bitmap 有效范围，
	// 右侧 scrollPos.x 像素留下未初始化内存，横滚时 BitBlt 出来就是白色（拖宽到出现
	// 横滚条之后的列拖动会暴露）
	{
		CRect rectBgFill(0, 0, m_actualSize.cx, rectPaint.bottom - rectPaint.top);
		if (rectBgFill.Width() < rectPaint.Width()) rectBgFill.right = rectPaint.right;
		SWPlotUtil::quick_fillrect(m_hdc_mem, rectBgFill, m_global_graphic_objects.m_hPenLineBlack, m_global_graphic_objects.m_hBrushBlack);
	}
	rectPaint.OffsetRect(-scrollPos);

	int cx = rectPaint.left;
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// draw 表头
	int cy = rectPaint.top + this->m_singalRowHeight / 2 - m_global_graphic_objects.m_nPlotTitleFontHeight / 2;
	for (int i = 0; i < nColumnCount; i++)
	{
		if (m_vColumnInfos[i]->align == 1)
		{
			cx = m_vColumnInfos[i]->actualLeft + columnPandding;
		}
		else if (m_vColumnInfos[i]->align == 0)
		{
			cx = m_vColumnInfos[i]->actualLeft + m_vColumnInfos[i]->actualWidth / 2;
		}
		else if (m_vColumnInfos[i]->align == -1)
		{
			cx = m_vColumnInfos[i]->actualLeft + m_vColumnInfos[i]->actualWidth - columnPandding;
		}

		int w = ((m_vColumnInfos[i]->align >= 0) || (m_vColumnInfos[i]->sort == 0)) ? 0 : 15;
		// 表头列名裁剪到本列矩形，防止窄列名重叠到相邻列
		int savedHeaderDC = SaveDC(m_hdc_mem);
		IntersectClipRect(m_hdc_mem,
			(int)m_vColumnInfos[i]->actualLeft,
			rectPaint.top,
			(int)(m_vColumnInfos[i]->actualLeft + m_vColumnInfos[i]->actualWidth),
			rectPaint.top + m_singalRowHeight);
		SWPlotUtil::quick_text(m_hdc_mem, m_vColumnInfos[i]->columnName, CPoint(cx - w, cy), COLOR_WHITE, m_hCaptionFont, m_vColumnInfos[i]->align);

		int cx2 = m_vColumnInfos[i]->actualLeft + m_vColumnInfos[i]->actualWidth - columnPandding;
		if (m_vColumnInfos[i]->sort == -1)
		{
			SWPlotUtil::drawAL(m_hdc_mem, CPoint(cx2, cy + 20), CPoint(cx2, cy + 10), m_global_graphic_objects.m_hPenLineWhite, m_global_graphic_objects.m_hBrushWhite);
		}
		else if (m_vColumnInfos[i]->sort == 1)
		{
			SWPlotUtil::drawAL(m_hdc_mem, CPoint(cx2, cy + 10), CPoint(cx2, cy + 20), m_global_graphic_objects.m_hPenLineWhite, m_global_graphic_objects.m_hBrushWhite);
		}
		RestoreDC(m_hdc_mem, savedHeaderDC);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	HPEN hPenOld = (HPEN)SelectObject(m_hdc_mem, m_global_graphic_objects.m_hPenLineGray);
	BeginPath(m_hdc_mem); // 开始记录路径

	for (int i = 0; i < nColumnCount; i++)
	{
		if (m_vColumnInfos[i]->align == 1)
		{
			cx = m_vColumnInfos[i]->actualLeft + columnPandding;
		}
		else if (m_vColumnInfos[i]->align == 0)
		{
			cx = m_vColumnInfos[i]->actualLeft + m_vColumnInfos[i]->actualWidth / 2;
		}
		else if (m_vColumnInfos[i]->align == -1)
		{
			cx = m_vColumnInfos[i]->actualLeft + m_vColumnInfos[i]->actualWidth - columnPandding;
		}

		// 列竖线
		if (i != nColumnCount - 1)
		{
			cx = m_vColumnInfos[i]->actualLeft + m_vColumnInfos[i]->actualWidth;
			SWPlotUtil::quick_line_path(m_hdc_mem, CPoint(cx, 0), CPoint(cx, m_singalRowHeight - 1));
		}
	}

	// 最后一根竖线
	cx = m_vColumnInfos[nColumnCount - 1]->actualLeft + m_vColumnInfos[nColumnCount - 1]->actualWidth;
	SWPlotUtil::quick_line_path(m_hdc_mem, CPoint(cx, 0), CPoint(cx, m_singalRowHeight - 1));

	/////////////////////////////////////////////////////
	// 顶部横线
	SWPlotUtil::quick_line_path(m_hdc_mem, CPoint(rectPaint.left, 0), CPoint(cx, 0));// 2, COLOR_ROW_MOUSE_BG);

	// 横线
	cx = m_vColumnInfos[nColumnCount - 1]->actualLeft + m_vColumnInfos[nColumnCount - 1]->actualWidth;
	SWPlotUtil::quick_line_path(m_hdc_mem, CPoint(rectPaint.left, this->m_singalRowHeight), CPoint(cx, this->m_singalRowHeight));// 2, COLOR_ROW_MOUSE_BG);

	EndPath(m_hdc_mem);   // 结束记录
	StrokePath(m_hdc_mem); // 一次性绘制所有路径
	SelectObject(m_hdc_mem, hPenOld);
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	int last_x = cx;

	if (m_rowCount > 0 && m_nVertScrolPage > 0)
	{
		// 像素级连续滚动：从 m_nVertScrollPos 反推首行及行内 y 偏移
		int firstRow = m_nVertScrollPos / m_singalRowHeight;
		int rowOffsetY = m_nVertScrollPos % m_singalRowHeight;
		int visibleRows = (m_nVertScrolPage / m_singalRowHeight) + 2;  // +2 覆盖上下半行
		int rowEnd = firstRow + visibleRows;
		if (rowEnd > m_rowCount) { rowEnd = m_rowCount; }

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// draw 数据（表头下方 5 像素 gap + m_singalRowHeight 的表头本身）
		int dataAreaTop = rectPaint.top + m_singalRowHeight + 1;
		int dataAreaBottom = dataAreaTop + m_nVertScrolPage;
		cy = dataAreaTop - rowOffsetY;

		// 裁剪到数据区，避免首/末行越界覆盖表头 / 横向滚动条
		int savedDC = SaveDC(m_hdc_mem);
		HRGN hRgn = CreateRectRgn(-100000, dataAreaTop, 100000, dataAreaBottom);
		SelectClipRgn(m_hdc_mem, hRgn);
		DeleteObject(hRgn);

		// 每帧数据准备（一次性加锁拷贝 / 排序由调用方实现）
		if (m_pCallback)
		{
			m_pCallback->onTableScrollViewPrepareDraw(this, firstRow, rowEnd);
		}
		for (int i = firstRow; i < rowEnd; i++)
		{
			bool isInRow = (mouse_row - 1 == i) || (m_current_sel_row - 1 == i);
			bool isOddRow = ((i - firstRow) % 2 == 1);

			CRect rectLine(rectPaint.left, cy, rectPaint.right, cy + m_singalRowHeight - 1);
			rectLine.OffsetRect(scrollPos);

			// M7.A (2026-05-23): 分组行整行画 — callback 返 true 时跳过分列 drawCell, 改调 drawGroupRow
			//   row 传 1-based (与 drawCell 同口径); rect 是整行 (left..right × singalRowHeight).
			//   背景填充不走斑马纹, drawGroupRow 内 caller 自决.
			bool isGroup = (m_pCallback && m_pCallback->onTableScrollViewIsGroupRow(this, i + 1));
			if (isGroup)
			{
				CRect rGroup(rectPaint.left, cy, rectPaint.right, cy + m_singalRowHeight - 1);
				m_pCallback->onTableScrollViewDrawGroupRow(m_hdc_mem, this, i + 1, rGroup);
				cy += m_singalRowHeight;
				continue;
			}

			if (isInRow)
			{
				SWPlotUtil::quick_flookfill_rect(m_hdc_mem, rectLine, m_global_graphic_objects.m_hPenDomRowMouseBG, m_global_graphic_objects.m_hBrushRowMouseBG, COLOR_ROW_MOUSE_BG);
			}
			else
			{
				if (isOddRow)
				{
					SWPlotUtil::quick_flookfill_rect(m_hdc_mem, rectLine, m_global_graphic_objects.m_hPenTableRowIntervalBG, m_global_graphic_objects.m_hBrushTableRowIntervalBG, COLOR_ROW_INTERVAL_BG);
				}
				else
				{
					rectLine.bottom += 1;
					SWPlotUtil::quick_fillrect(m_hdc_mem, rectLine, m_global_graphic_objects.m_hPenLineBlack, m_global_graphic_objects.m_hBrushBlack, 1);
				}
			}

			cx = rectPaint.left + m_vColumnInfos[0]->actualLeft;
			for (int j = 0; j < nColumnCount; j++)
			{
				if (m_pCallback)
				{
					CRect r((int)m_vColumnInfos[j]->actualLeft, cy, (int)m_vColumnInfos[j]->actualLeft + (int)m_vColumnInfos[j]->actualWidth - 1, cy + m_singalRowHeight - 1);
					// cell 级高亮：鼠标所在 cell 额外叠一层背景
					if (mouse_row - 1 == i && mouse_col == j)
					{
						HBRUSH hBrushCell = CreateSolidBrush(RGB(70, 80, 130));
						HPEN hPenCell = CreatePen(PS_SOLID, 1, RGB(70, 80, 130));
						SWPlotUtil::quick_fillrect(m_hdc_mem, r, hPenCell, hBrushCell, 1);
						DeleteObject(hBrushCell);
						DeleteObject(hPenCell);
					}
					// 编辑中的 cell 由 inplace edit 覆盖：跳过 per-cell 绘制以避免闪烁
					if (m_bEditing && m_editRow - 1 == i && m_editCol == j)
					{
						continue;
					}
					// 逐 cell 裁剪：防止文本/内容超宽时画到相邻列上
					int savedCellDC = SaveDC(m_hdc_mem);
					IntersectClipRect(m_hdc_mem, r.left, r.top, r.right + 1, r.bottom + 1);
					m_pCallback->onTableScrollViewDrawCell(m_hdc_mem, this, i + 1, j, r, m_vColumnInfos[j]->align);
					RestoreDC(m_hdc_mem, savedCellDC);
				}
			}
			cy += m_singalRowHeight;
		}

		if (m_pCallback)
		{
			m_pCallback->onTableScrollViewFinishDraw(this);
		}

		RestoreDC(m_hdc_mem, savedDC);
	}


	if (m_bMousePressed)
	{
		if (mouse_row == 0)
		{
			if (m_mouseSelectColumnLine > 0)
			{
				int cx = this->m_mouse_point.x + this->m_nScrollPos;
				SWPlotUtil::quick_line(m_hdc_mem, CPoint(cx, rectTable.top), CPoint(cx, rectTable.top + rectTable.Height()), m_global_graphic_objects.m_hPenLineWhite2);
			}
		}
	}

	this->_onRendScrollBar(pDCMem, m_rectScrollHoriz, scrollPos);

	if (!m_rectScrollVert.IsRectEmpty())
	{
		this->_onRendVertScrollBar(pDCMem, m_rectScrollVert, scrollPos);
		// 右下角两滚动条交汇处填黑，避免显示冗余
		if (!m_rectScrollHoriz.IsRectEmpty())
		{
			CRect rectCorner(m_rectScrollHoriz.right, m_rectScrollHoriz.top,
				m_rectScrollVert.right, m_rectScrollHoriz.bottom);
			rectCorner.OffsetRect(scrollPos);
			SWPlotUtil::quick_fillrect(m_hdc_mem, rectCorner,
				m_global_graphic_objects.m_hPenLineBlack,
				m_global_graphic_objects.m_hBrushBlack);
		}
	}

	// 冻结前 N 列：把 bitmap 拆成 3 段 BitBlt（冻结区不加 scrollPos 偏移，滚动区加偏移，滚动条区整条拷）
	int scrollH = m_rectScrollHoriz.IsRectEmpty() ? 0 : m_rectScrollHoriz.Height();
	int dataH = rectTable.Height() - scrollH;
	if (m_nFrozenColumnsWidth > 0 && m_nFrozenColumnsWidth < rectTable.Width())
	{
		// 冻结列区（顶部到横向滚动条上沿）
		BitBlt(hdc, rectTable.left, rectTable.top,
			m_nFrozenColumnsWidth, dataH,
			m_hdc_mem, 0, 0, SRCCOPY);
		// 滚动列区
		BitBlt(hdc, rectTable.left + m_nFrozenColumnsWidth, rectTable.top,
			rectTable.Width() - m_nFrozenColumnsWidth, dataH,
			m_hdc_mem, m_nFrozenColumnsWidth + scrollPos.x, 0, SRCCOPY);
		// 横向滚动条区（若有）
		if (scrollH > 0)
		{
			BitBlt(hdc, rectTable.left, rectTable.top + dataH,
				rectTable.Width(), scrollH,
				m_hdc_mem, scrollPos.x, dataH, SRCCOPY);
		}
	}
	else
	{
		BitBlt(hdc, rectTable.left, rectTable.top, rectTable.Width(), rectTable.Height(), m_hdc_mem, scrollPos.x, scrollPos.y, SRCCOPY);//将内存DC上的图象拷贝到前台
	}

	cdc_mem.Detach();
}

CRect SWTableScrollViewWnd::_getThumbRect(CRect rect)
{
	CRect rectThumb(0, 0, 0, 0);

	if (m_HitPressed == TSV_HT_THUMB)
	{
		rectThumb = m_rectTrackThumb;
	}
	else
	{
		rectThumb = rect;

		int nClientSize = 0;
		int nThumbSize = 0;

		nThumbSize = ::GetSystemMetrics(SM_CXHSCROLL);
		rectThumb.DeflateRect(nThumbSize, 0);
		nClientSize = rectThumb.Width();


		const int c_ScrollMin = globalUtils.ScaleByDPI(4);

		if (nClientSize <= c_ScrollMin)
		{
			rectThumb.SetRectEmpty();
			return rectThumb;
		}

		if (rect.Width() < m_nScrolPage)
		{
			nThumbSize = 0;
		}

		int nThumbPos = nThumbSize;

		// 横向滚动基数 = 非冻结列总宽
		int rollableTotalW = m_actualSize.cx - m_nFrozenColumnsWidth;
		if (rollableTotalW <= 0) { rollableTotalW = 1; }

		if (m_nScrolPage != 0)
		{
			const int c_ScrollThumbMin = globalUtils.ScaleByDPI(8);
			nThumbSize = (std::max)(::MulDiv(nClientSize, m_nScrolPage, rollableTotalW), c_ScrollThumbMin);
		}

		if (nClientSize < nThumbSize || nThumbSize == 0)
		{
			rectThumb.SetRectEmpty();
			return rectThumb;
		}
		else
		{
			//nClientSize -= nThumbSize;
			nThumbPos = ::MulDiv(nClientSize, m_nScrollPos, rollableTotalW);
		}

		rectThumb.left += nThumbPos;
		rectThumb.right = rectThumb.left + nThumbSize;
	}

	return rectThumb;
}

int SWTableScrollViewWnd::_posFromThumb(int thumb, const CSize& szClamp)
{
	const int nRange1 = szClamp.cy - szClamp.cx;
	const int nMin = 0;
	const int nMax = (m_totalSize.cx - m_nFrozenColumnsWidth) - m_nScrolPage;
	const int nRange2 = nMax - nMin;

	if (nRange2 < 0)
	{
		return 0;
	}

	int nPos = nMin +
		(int)(((double)thumb) * ((double)nRange2) / (double)(nRange1)+0.5);

	if (nPos < nMin)
	{
		nPos = nMin;
	}

	if (nMax < nPos)
	{
		nPos = nMax;
	}

	return nPos;
}


void SWTableScrollViewWnd::_onRendScrollBar(CDC* pDC, CRect rectScrollBar, CPoint ptOffset)
{
	CRect rect = rectScrollBar;
	rect.OffsetRect(ptOffset);

	int nThumbSize = ::GetSystemMetrics(SM_CXHSCROLL);

	CRect rectThumb;
	rectThumb = _getThumbRect(rectScrollBar);
	m_rectThumb = rectThumb;
	rectThumb.OffsetRect(ptOffset);

	CRect rectBtn[2];
	rectBtn[0] = rect;


	int nSize = ::GetSystemMetrics(SM_CXHSCROLL);
	if (rect.Width() < nSize * 2)
	{
		nSize = rect.Width() / 2;
	}

	rectBtn[0].right = rect.left + nSize;
	rectBtn[1] = rectBtn[0];
	rectBtn[1].OffsetRect(rect.Width() - rectBtn[1].Width(), 0);

	rect.left += rectBtn[0].Width();
	rect.right -= rectBtn[1].Width();

	if (rectThumb.left < rectBtn[0].right || rectBtn[1].left < rectThumb.right)
	{
		rectThumb.SetRectEmpty();
	}

	bool bHighlighted = false;
	bool bPressed = false;
	bool bHorz = true;
	pDC->FillSolidRect(rectBtn[0], RGB(0x55,0x55,0x55));
	pDC->FillSolidRect(rectBtn[1], RGB(0x55,0x55,0x55));

	if (!rectThumb.IsRectEmpty())
	{
		pDC->FillSolidRect(rectThumb, RGB(0x88,0x88,0x88));

		BOOL bDrawBack = FALSE;
		CRect rectBack(rect);
		if (bHorz)
		{
			rectBack.right = rectThumb.left;
			bDrawBack = rectBack.left < rectBack.right;
		}
		else
		{
			rectBack.bottom = rectThumb.top;
			bDrawBack = rectBack.top < rectBack.bottom;
		}

		if (bDrawBack)
		{
			pDC->FillSolidRect(rectBack, RGB(0x2A,0x2A,0x2A));
		}

		bDrawBack = FALSE;
		rectBack = rect;
		if (bHorz)
		{
			rectBack.left = rectThumb.right;
			bDrawBack = rectBack.left < rectBack.right;
		}
		else
		{
			rectBack.top = rectThumb.bottom;
			bDrawBack = rectBack.top < rectBack.bottom;
		}

		if (bDrawBack)
		{
			pDC->FillSolidRect(rectBack, RGB(0x2A,0x2A,0x2A));
		}
	}
}

CRect SWTableScrollViewWnd::_getVertThumbRect(CRect rect)
{
	CRect rectThumb(0, 0, 0, 0);

	if (m_HitPressed == TSV_HT_VERT_THUMB)
	{
		rectThumb = m_rectTrackThumb;
	}
	else
	{
		rectThumb = rect;

		int nBtnSize = ::GetSystemMetrics(SM_CYVSCROLL);
		rectThumb.DeflateRect(0, nBtnSize);
		int nClientSize = rectThumb.Height();

		const int c_ScrollMin = globalUtils.ScaleByDPI(4);
		if (nClientSize <= c_ScrollMin)
		{
			rectThumb.SetRectEmpty();
			return rectThumb;
		}

		int totalH = m_rowCount * m_singalRowHeight;
		if (totalH <= 0) { totalH = 1; }
		int nThumbSize = 0;
		if (m_nVertScrolPage != 0)
		{
			const int c_ScrollThumbMin = globalUtils.ScaleByDPI(8);
			nThumbSize = (std::max)(::MulDiv(nClientSize, m_nVertScrolPage, totalH), c_ScrollThumbMin);
		}

		if (nClientSize < nThumbSize || nThumbSize == 0)
		{
			rectThumb.SetRectEmpty();
			return rectThumb;
		}

		int nThumbPos = ::MulDiv(nClientSize, m_nVertScrollPos, totalH);
		rectThumb.top += nThumbPos;
		rectThumb.bottom = rectThumb.top + nThumbSize;
	}

	return rectThumb;
}

int SWTableScrollViewWnd::_posFromVertThumb(int thumb, const CSize& szClamp)
{
	const int nRange1 = szClamp.cy - szClamp.cx;
	const int nMin = 0;
	int totalH = m_rowCount * m_singalRowHeight;
	const int nMax = totalH - m_nVertScrolPage;
	const int nRange2 = nMax - nMin;

	if (nRange2 <= 0 || nRange1 <= 0)
	{
		return 0;
	}

	int nPos = nMin + (int)(((double)thumb) * ((double)nRange2) / (double)(nRange1)+0.5);
	if (nPos < nMin) { nPos = nMin; }
	if (nMax < nPos) { nPos = nMax; }
	return nPos;
}

void SWTableScrollViewWnd::_onRendVertScrollBar(CDC* pDC, CRect rectScrollBar, CPoint ptOffset)
{
	CRect rect = rectScrollBar;
	rect.OffsetRect(ptOffset);

	CRect rectThumb = _getVertThumbRect(rectScrollBar);
	m_rectVertThumb = rectThumb;
	rectThumb.OffsetRect(ptOffset);

	CRect rectBtn[2];
	rectBtn[0] = rect;

	int nSize = ::GetSystemMetrics(SM_CYVSCROLL);
	if (rect.Height() < nSize * 2)
	{
		nSize = rect.Height() / 2;
	}

	rectBtn[0].bottom = rect.top + nSize;
	rectBtn[1] = rectBtn[0];
	rectBtn[1].OffsetRect(0, rect.Height() - rectBtn[1].Height());

	rect.top += rectBtn[0].Height();
	rect.bottom -= rectBtn[1].Height();

	if (rectThumb.top < rectBtn[0].bottom || rectBtn[1].top < rectThumb.bottom)
	{
		rectThumb.SetRectEmpty();
	}

	bool bHorz = false;
	pDC->FillSolidRect(rectBtn[0], RGB(0x55,0x55,0x55));
	pDC->FillSolidRect(rectBtn[1], RGB(0x55,0x55,0x55));

	if (!rectThumb.IsRectEmpty())
	{
		pDC->FillSolidRect(rectThumb, RGB(0x88,0x88,0x88));

		CRect rectBack(rect);
		rectBack.bottom = rectThumb.top;
		if (rectBack.top < rectBack.bottom)
		{
			pDC->FillSolidRect(rectBack, RGB(0x2A,0x2A,0x2A));
		}

		rectBack = rect;
		rectBack.top = rectThumb.bottom;
		if (rectBack.top < rectBack.bottom)
		{
			pDC->FillSolidRect(rectBack, RGB(0x2A,0x2A,0x2A));
		}
	}
}

TABLE_SCROLL_VIEW_HITTEST SWTableScrollViewWnd::_doHitTest(const CPoint& pt)
{
	CPoint point(pt);

	//if (!m_bInternal)
	//{
	//	pWnd->ScreenToClient(&point);
	//}

	/////////////////////////////////////////////////////
	// 对于有margin
	if (m_rectMargin.top > 0)
	{
		point.y -= m_rectMargin.top;
	}

	TABLE_SCROLL_VIEW_HITTEST hit = TSV_HT_NOWHERE;

	// 先判竖向滚动条
	if (!m_rectScrollVert.IsRectEmpty() && m_rectScrollVert.PtInRect(point))
	{
		hit = TSV_HT_VERT_CLIENT_UP;

		CRect rectVThumb = m_rectVertThumb;
		CRect rectVBtn[2];
		rectVBtn[0] = m_rectScrollVert;

		int nVSize = ::GetSystemMetrics(SM_CYVSCROLL);
		if (m_rectScrollVert.Height() < nVSize * 2)
		{
			nVSize = m_rectScrollVert.Height() / 2;
		}

		rectVBtn[0].bottom = m_rectScrollVert.top + nVSize;
		rectVBtn[1] = rectVBtn[0];
		rectVBtn[1].OffsetRect(0, m_rectScrollVert.Height() - rectVBtn[1].Height());

		if (rectVThumb.top < rectVBtn[0].bottom || rectVBtn[1].top < rectVThumb.bottom)
		{
			rectVThumb.SetRectEmpty();
		}

		if (rectVBtn[0].PtInRect(point))
		{
			hit = TSV_HT_VERT_BUTTON_UP;
		}
		else if (rectVBtn[1].PtInRect(point))
		{
			hit = TSV_HT_VERT_BUTTON_DOWN;
		}
		else if (!rectVThumb.IsRectEmpty())
		{
			if (rectVThumb.PtInRect(point))
			{
				hit = TSV_HT_VERT_THUMB;
			}
			else if (point.y < rectVThumb.top)
			{
				hit = TSV_HT_VERT_CLIENT_UP;
			}
			else
			{
				hit = TSV_HT_VERT_CLIENT_DOWN;
			}
		}
		return hit;
	}

	CRect rect = m_rectScrollHoriz;

	if (rect.PtInRect(point))
	{
		hit = TSV_HT_CLIENT;


		CRect rectThumb = m_rectThumb;

		CRect rectBtn[2];
		rectBtn[0] = rect;

		int nSize = ::GetSystemMetrics(SM_CXHSCROLL);
		if (rect.Width() < nSize * 2)
		{
			nSize = rect.Width() / 2;
		}

		rectBtn[0].right = rect.left + nSize;
		rectBtn[1] = rectBtn[0];
		rectBtn[1].OffsetRect(rect.Width() - rectBtn[1].Width(), 0);

		rect.left += rectBtn[0].Width();
		rect.right -= rectBtn[1].Width();

		if (rectThumb.left < rectBtn[0].right || rectBtn[1].left < rectThumb.right)
		{
			rectThumb.SetRectEmpty();
		}


		if (rectBtn[0].PtInRect(point))
		{
			hit = TSV_HT_BUTTON_LEFT;
		}
		else
		{
			if (rectBtn[1].PtInRect(point))
			{
				hit = TSV_HT_BUTTON_RIGHT;
			}
			else if (!rectThumb.IsRectEmpty())
			{
				if (rectThumb.PtInRect(point))
				{
					hit = TSV_HT_THUMB;
				}
				else
				{
					if (point.x < rectThumb.left)
					{
						hit = TSV_HT_CLIENT_LEFT;
					}
					else
					{
						hit = TSV_HT_CLIENT_RIGHT;
					}
				}
			}
		}
	}

	return hit;
}

void SWTableScrollViewWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_isMouseFirstEnter)
	{
		m_isMouseFirstEnter = false;
	}

	if (this->m_bMousePressed && !this->m_bMouseTracking)
	{
		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags = TME_LEAVE | TME_HOVER;  // 这里同时也跟踪了Hover事件
		tme.hwndTrack = this->GetSafeHwnd();
		tme.dwHoverTime = 10;

		this->m_bMouseTracking = _TrackMouseEvent(&tme);
	}

	// 减闪烁：仅在 hover 行/列/列分隔线/hit-test 真正变化时才 Invalidate；
	// 否则鼠标在同一 cell 内移动一格像素都触发全帧重绘。
	int prev_mouse_row = mouse_row;
	int prev_mouse_col = mouse_col;
	int prev_select_col_line = m_mouseSelectColumnLine;

	m_mouse_point.x = point.x;
	m_mouse_point.y = point.y;

	// 冻结列区的 x 不加 scrollPos
	int cx = (point.x < m_nFrozenColumnsWidth)
		? point.x
		: (point.x + this->m_nScrollPos);
	int cy = point.y - m_rectMargin.top;
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 行
	int nColumnCount = m_vColumnInfos.size();

	mouse_row = -1;
	if ((cy >= 5) && (cy <= m_singalRowHeight))
	{
		mouse_row = 0;
	}

	// 连续滚动：从 m_nVertScrollPos 推首行
	int firstRow = m_nVertScrollPos / m_singalRowHeight;
	int rowOffsetY = m_nVertScrollPos % m_singalRowHeight;
	int visibleRows = (m_nVertScrolPage / m_singalRowHeight) + 2;
	int rowEnd = firstRow + visibleRows;
	if (rowEnd > m_rowCount) { rowEnd = m_rowCount; }

	int cy2 = m_singalRowHeight + 1 + m_rectMargin.top - rowOffsetY;
	for (int i = firstRow; i < rowEnd; i++)
	{
		if ((point.y >= cy2) && (point.y <= cy2 + m_singalRowHeight))
		{
			mouse_row = i + 1;
			break;
		}
		cy2 += m_singalRowHeight;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 列
	mouse_col = -1;
	for (int i = 0; i < nColumnCount; i++)
	{
		if ((cx >= m_vColumnInfos[i]->actualLeft) && (cx < m_vColumnInfos[i]->actualLeft + m_vColumnInfos[i]->actualWidth))
		{
			mouse_col = i;
		}
	}
	//TracePrint("onMouseMove mouse_row: %d, mouse_col:%d\n", mouse_row, mouse_col);

	// 若坐标落在滚动条区域（竖向条与数据行共 y；横向条因 +2 行越界可能命中假行），
	// 强制把 mouse_row / mouse_col 置 -1，使下方滚动条 hit-test 分支能正常生效
	{
		CPoint htPt(point);
		if (m_rectMargin.top > 0) { htPt.y -= m_rectMargin.top; }
		if ((!m_rectScrollVert.IsRectEmpty()  && m_rectScrollVert.PtInRect(htPt)) ||
			(!m_rectScrollHoriz.IsRectEmpty() && m_rectScrollHoriz.PtInRect(htPt)))
		{
			mouse_row = -1;
			mouse_col = -1;
		}
	}

	HCURSOR hCur = 0;
	if (mouse_row == 0)
	{
		if (!m_bMousePressed)
		{
			m_mouseSelectColumnLine = this->checkMouseColumnLine(point);
		}

		if (m_mouseSelectColumnLine > 0)
		{
			hCur = ::LoadCursor(NULL, IDC_SIZEWE);
		}
		else
		{
			hCur = ::LoadCursor(NULL, IDC_ARROW);
		}
	}
	else
	{
		hCur = ::LoadCursor(NULL, IDC_ARROW);
	}
	::SetCursor(hCur);

	//::PostMessage(m_pParentWnd->GetSafeHwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(point.x, point.y));

	// 处理滚动条
	if (mouse_row == -1)
	{
		TABLE_SCROLL_VIEW_HITTEST hit = _doHitTest(point);
		if (m_HitTest != hit)
		{
			m_HitTest = hit;
			// 改 Invalidate (不 RedrawWindow): 让 WM_PAINT 走双缓冲 + OnEraseBkgnd 抑制,
			// 避免鼠标在滚动条区域 hover 时的同步全表强制重绘闪烁
			this->Invalidate();
		}

		if (m_bMousePressed && (m_HitPressed == TSV_HT_THUMB || m_HitPressed == TSV_HT_VERT_THUMB))
		{
			CSize szClamp(0, 0);
			int nThumbLength = 0;
			int nScroll = 0;

			bool bHorz = (m_HitPressed == TSV_HT_THUMB);
			if (bHorz)
			{
				nScroll = ::GetSystemMetrics(SM_CXHSCROLL);
				szClamp.cx = m_rectScrollHoriz.left;
				szClamp.cy = m_rectScrollHoriz.right;
				nThumbLength = m_rectTrackThumbStart.Width();
			}
			else
			{
				nScroll = ::GetSystemMetrics(SM_CYVSCROLL);
				szClamp.cx = m_rectScrollVert.top;
				szClamp.cy = m_rectScrollVert.bottom;
				nThumbLength = m_rectTrackThumbStart.Height();
			}

			szClamp.cx += nScroll;
			szClamp.cy -= (nScroll + nThumbLength);

			CPoint ptOffset(point - m_ptTrackStart);
			CRect rectNew(m_rectTrackThumbStart);

			if (bHorz)
			{
				if (abs(ptOffset.y) < 150)
				{
					rectNew.OffsetRect(ptOffset.x, 0);

					if (rectNew.left < szClamp.cx)
					{
						rectNew.left = szClamp.cx;
						rectNew.right = rectNew.left + nThumbLength;
					}
					else if (szClamp.cy < rectNew.left)
					{
						rectNew.left = szClamp.cy;
						rectNew.right = rectNew.left + nThumbLength;
					}
				}
			}
			else
			{
				if (abs(ptOffset.x) < 150)
				{
					rectNew.OffsetRect(0, ptOffset.y);

					if (rectNew.top < szClamp.cx)
					{
						rectNew.top = szClamp.cx;
						rectNew.bottom = rectNew.top + nThumbLength;
					}
					else if (szClamp.cy < rectNew.top)
					{
						rectNew.top = szClamp.cy;
						rectNew.bottom = rectNew.top + nThumbLength;
					}
				}
			}

			if (rectNew != m_rectTrackThumb)
			{
				m_rectTrackThumb = rectNew;

				int nPosNew = bHorz
					? _posFromThumb(m_rectTrackThumb.left - szClamp.cx, szClamp)
					: _posFromVertThumb(m_rectTrackThumb.top - szClamp.cx, szClamp);

				if (m_TrackPos != nPosNew)
				{
					m_TrackPos = nPosNew;
					if (bHorz)
					{
						m_nScrollPos = nPosNew;
					}
					else
					{
						m_nVertScrollPos = nPosNew;
					}
				}

				this->RedrawWindow();
			}

		}
	}

	// 仅在 hover 行/列/列分隔线变化时刷新，避免每次像素级 mouse move 全帧重绘
	// 例外: 列拖动期间 (m_bMousePressed && 列分隔线选中), 鼠标 x 每变都必须 Invalidate
	// 让白色辅助线实时跟随 — 否则用户看辅助线"卡住"误以为拖不动
	bool isDraggingColumnLine = (m_bMousePressed && mouse_row == 0 && m_mouseSelectColumnLine > 0);
	if (mouse_row != prev_mouse_row ||
	    mouse_col != prev_mouse_col ||
	    m_mouseSelectColumnLine != prev_select_col_line ||
	    isDraggingColumnLine)
	{
		this->Invalidate();
	}
}

void SWTableScrollViewWnd::OnTimer(UINT_PTR nIDEvent)
{
	switch (nIDEvent)
	{
	case TABLE_SCROLL_VIEW_TIMER_ID:
		CPoint point;
		CRect rect;
		GetCursorPos(&point);
		this->GetWindowRect(&rect);
		if (!rect.PtInRect(point))
		{
			this->KillTimer(TABLE_SCROLL_VIEW_TIMER_ID); // kill self.
			this->OnMouseLeave();
		}
		break;
	}
}

int SWTableScrollViewWnd::checkMouseColumnLine(CPoint point)
{
	int cx = (point.x < m_nFrozenColumnsWidth)
		? point.x
		: (point.x + this->m_nScrollPos);
	int nColumnCount = m_vColumnInfos.size();
	for (int i = 0; i < nColumnCount; i++)
	{
		if (cx >= m_vColumnInfos[i]->actualLeft - COLUME_MOUSE_SIZING_WIDTH && cx <= m_vColumnInfos[i]->actualLeft + COLUME_MOUSE_SIZING_WIDTH)
		{
			return i;
		}
	}

	int lasty = m_vColumnInfos[nColumnCount - 1]->actualLeft + m_vColumnInfos[nColumnCount - 1]->actualWidth;
	if (cx >= lasty - COLUME_MOUSE_SIZING_WIDTH && cx <= lasty + COLUME_MOUSE_SIZING_WIDTH)
	{
		return nColumnCount;
	}

	return -1;
}


void SWTableScrollViewWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	//// 处理单机和双击的区别  300 毫秒
	//MSG message;
	//DWORD st = GetTickCount();
	//while (1)
	//{
	//	if (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
	//	{
	//		::TranslateMessage(&message);
	//		::DispatchMessage(&message);
	//		if (message.message == WM_LBUTTONDBLCLK)
	//		{
	//			return;
	//		}
	//	}
	//	DWORD et = GetTickCount();
	//	if (et - st > 300)
	//	{
	//		break;
	//	}
	//}
	//// 处理单机和双击的区别  300 毫秒
	/////////////////////////////////////////////////////////////////////////////////////////////////////////


	this->m_bMousePressed = true;
	m_mouse_pressed_pt = point;
	//TracePrint("OnLButtonDown\n");

	// 若坐标落在滚动条区域，强制 mouse_row/mouse_col = -1，避免 OnMouseMove 未及时刷新时误判为数据行点击
	{
		CPoint htPt(point);
		if (m_rectMargin.top > 0) { htPt.y -= m_rectMargin.top; }
		if ((!m_rectScrollVert.IsRectEmpty()  && m_rectScrollVert.PtInRect(htPt)) ||
			(!m_rectScrollHoriz.IsRectEmpty() && m_rectScrollHoriz.PtInRect(htPt)))
		{
			mouse_row = -1;
			mouse_col = -1;
		}
	}

	// 排序
	if (mouse_row == 0)
	{
		if (m_mouseSelectColumnLine == -1)
		{
			if (m_vColumnInfos[mouse_col]->can_sort)
			{
				int nColumnCount = m_vColumnInfos.size();
				for (int i = 0; i < nColumnCount; i++)
				{
					if (i != mouse_col)
					{
						m_vColumnInfos[i]->sort = 0;
					}
				}

				int order = m_vColumnInfos[mouse_col]->sort;
				if (order == 0)
				{
					order = 1;
				}
				else if (order == 1)
				{
					order = -1;
				}
				else if (order == -1)
				{
					order = 0;
				}
				m_vColumnInfos[mouse_col]->sort = order;

				if (m_pCallback)
				{
					m_pCallback->onTableScrollViewSortColumn(this, mouse_col, order);
				}
			}
		}
	}
	else if (mouse_row >= 0)
	{
		if (mouse_col != -1)
		{
			this->m_current_sel_row = mouse_row;
			if (m_pCallback)
			{
				// M7.A (2026-05-23): group row 优先走折叠/展开 callback, 不走选行 click
				if (mouse_row >= 1 && m_pCallback->onTableScrollViewIsGroupRow(this, mouse_row))
				{
					m_pCallback->onTableScrollViewGroupRowClick(this, mouse_row);
					this->m_current_sel_row = -1;  // group row 不算选中
					Invalidate(FALSE);
					return;
				}
				m_pCallback->onTableScrollViewLeftMouseClick(this, mouse_row, mouse_col);
			}

			// 单击可编辑列 → 进入 inplace 编辑（在 LeftMouseClick 回调之后，保留选中/排序语义）
			if (mouse_row >= 1 && mouse_col >= 0 && mouse_col < (int)m_vColumnInfos.size()
				&& m_vColumnInfos[mouse_col]->can_edit
				&& m_vColumnInfos[mouse_col]->edit_type != SW_EDIT_NONE)
			{
				_beginCellEdit(mouse_row, mouse_col);
			}
			else if (m_bEditing && (m_editRow != mouse_row || m_editCol != mouse_col))
			{
				// 点击其他非编辑 cell → cancel 旧编辑。
				// 注意：回调里可能已经主动 _beginCellEdit（例如参数窗口对当前 cell 开 inline edit），
				// 此时 m_editRow/m_editCol 已指向当前 cell；如果无条件 _endCellEdit 会把刚开的
				// 编辑会话立刻杀掉（"闪一下就没了"）。所以仅在 editing 的是另一个 cell 时才 cancel。
				_endCellEdit(false);
			}
		}
	}
	else
	{
		if (m_bEditing)
		{
			_endCellEdit(false);
		}
		CPoint ptScreen(point);
		TABLE_SCROLL_VIEW_HITTEST hit = _doHitTest(ptScreen);
		if (hit == TSV_HT_NOWHERE)
		{
			// 点击数据区之外的空白 → 取消选中 + 通知 callback
			this->m_current_sel_row = -1;
			if (m_pCallback) m_pCallback->onTableScrollViewLeftClickEmpty(this);
		}
		if (hit != TSV_HT_NOWHERE)
		{
			this->SetCapture();

			this->RedrawWindow();

			if (hit == TSV_HT_THUMB)
			{
				m_ptTrackStart = point;
				m_rectTrackThumbStart = m_rectThumb;
				m_rectTrackThumb = m_rectTrackThumbStart;
				m_HitPressed = hit;
			}
			else if (hit == TSV_HT_VERT_THUMB)
			{
				m_ptTrackStart = point;
				m_rectTrackThumbStart = m_rectVertThumb;
				m_rectTrackThumb = m_rectTrackThumbStart;
				m_HitPressed = hit;
			}
			else
			{
				m_HitPressed = hit;

				int totalH = m_rowCount * m_singalRowHeight;
				int vMax = totalH - m_nVertScrolPage;
				if (vMax < 0) { vMax = 0; }

				if (m_HitPressed == TSV_HT_BUTTON_LEFT)
				{
					int nSize = ::GetSystemMetrics(SM_CXHSCROLL);
					this->m_nScrollPos -= nSize;
					if (this->m_nScrollPos < 0) { this->m_nScrollPos = 0; }
				}
				else if (m_HitPressed == TSV_HT_BUTTON_RIGHT)
				{
					int nSize = ::GetSystemMetrics(SM_CXHSCROLL);
					this->m_nScrollPos += nSize;
					int hMax = (m_actualSize.cx - m_nFrozenColumnsWidth) - m_nScrolPage;
					if (hMax < 0) { hMax = 0; }
					if (this->m_nScrollPos > hMax)
					{
						this->m_nScrollPos = hMax;
					}
				}
				else if (m_HitPressed == TSV_HT_VERT_BUTTON_UP)
				{
					m_nVertScrollPos -= m_singalRowHeight;
					if (m_nVertScrollPos < 0) { m_nVertScrollPos = 0; }
				}
				else if (m_HitPressed == TSV_HT_VERT_BUTTON_DOWN)
				{
					m_nVertScrollPos += m_singalRowHeight;
					if (m_nVertScrollPos > vMax) { m_nVertScrollPos = vMax; }
				}
				else if (m_HitPressed == TSV_HT_VERT_CLIENT_UP)
				{
					m_nVertScrollPos -= m_nVertScrolPage;
					if (m_nVertScrollPos < 0) { m_nVertScrollPos = 0; }
				}
				else if (m_HitPressed == TSV_HT_VERT_CLIENT_DOWN)
				{
					m_nVertScrollPos += m_nVertScrolPage;
					if (m_nVertScrollPos > vMax) { m_nVertScrollPos = vMax; }
				}
			}
		}
	}
	this->Invalidate();
}

void SWTableScrollViewWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
	this->m_bMousePressed = false;
	//TracePrint("OnLButtonUp\n");

	if (mouse_row == 0)
	{
		if (m_mouseSelectColumnLine > 0)
		{
			// 列拖动语义 (2026-05-23 v4 final, 用户最终描述):
			//   "每个列都有初始宽度, 总初始宽度<窗口宽度就自动撑满;
			//    拉动某一列改变该列初始宽度, 拖完仍<窗口宽度还是撑满;
			//    再拉大其他列, 当总初始宽度>窗口宽度就出滚动条."
			//
			// 实现策略:
			//   - 撑满模式下拖动: 视觉上 col[idx] 加 delta px, 其它列等比缩 delta px 吸收 (总宽=clientW).
			//   - 其它列被压缩到列名最小宽时还吸收不下, 退出撑满进入滚动条模式
			//     (其它列各自 nameMin, col[idx]=newActualIdx, 总宽>clientW → 出滚动条).
			//   - 滚动条模式下拖动: 只动被拖列, 其它列锁固定像素 (保持当前 actualWidth).
			//   - 拖完所有列 width 设为各自 actualWidth (固定像素 >1.0), m_hasUserColumnWidths=true,
			//     _reCalculateSize 走"不放大"分支 (sumFixedPx<=clientW 或 >clientW), 接管 actualLeft
			//     和 scrollbar 状态.
			int delta = point.x - m_mouse_pressed_pt.x;
			int idx = m_mouseSelectColumnLine - 1;
			int nColumnCount = (int)m_vColumnInfos.size();

			if (idx >= 0 && idx < nColumnCount)
			{
				TABLE_VIEW_COLUMN_INFO* col = m_vColumnInfos[idx];
				int nameMinL = this->_measureColumnNameWidth(col->columnName) + columnPandding * 2;
				if (nameMinL < 1) { nameMinL = 1; }

				CRect cr;
				this->GetClientRect(&cr);
				cr.DeflateRect(m_rectMargin);
				int clientW = cr.Width();
				if (clientW < 1) { clientW = 1; }

				bool isFitMode = m_rectScrollHoriz.IsRectEmpty();

				float newActualIdx = col->actualWidth + (float)delta;
				if (newActualIdx < (float)nameMinL) { newActualIdx = (float)nameMinL; }

				if (isFitMode)
				{
					// 撑满模式: 其它列等比缩 delta 吸收
					float sumOtherOld = 0.0f;
					int   sumOtherMin = 0;
					for (int i = 0; i < nColumnCount; i++)
					{
						if (i == idx) continue;
						sumOtherOld += m_vColumnInfos[i]->actualWidth;
						int nmi = this->_measureColumnNameWidth(m_vColumnInfos[i]->columnName) + columnPandding * 2;
						if (nmi < 1) { nmi = 1; }
						sumOtherMin += nmi;
					}
					float sumOtherTarget = (float)clientW - newActualIdx;

					if (sumOtherTarget >= (float)sumOtherMin)
					{
						// 其它列等比缩, 撑满保持
						float scale = (sumOtherOld > 0.01f) ? (sumOtherTarget / sumOtherOld) : 1.0f;
						int lastOther = -1;
						for (int i = nColumnCount - 1; i >= 0; i--)
						{
							if (i != idx) { lastOther = i; break; }
						}
						float runningOther = 0.0f;
						for (int i = 0; i < nColumnCount; i++)
						{
							if (i == idx) continue;
							TABLE_VIEW_COLUMN_INFO* ci = m_vColumnInfos[i];
							int nmi = this->_measureColumnNameWidth(ci->columnName) + columnPandding * 2;
							if (nmi < 1) { nmi = 1; }
							float newOther = (i == lastOther) ? (sumOtherTarget - runningOther) : (ci->actualWidth * scale);
							if (newOther < (float)nmi) { newOther = (float)nmi; }
							int nint = (int)(newOther + 0.5f);
							ci->width = (float)nint;
							ci->actualWidth = (float)nint;
							runningOther += (float)nint;
						}
						// 被拖列吸收最终余数, 保证总宽精确 = clientW (避免 round 累计误差)
						float finalIdx = (float)clientW - runningOther;
						if (finalIdx < (float)nameMinL) { finalIdx = (float)nameMinL; }
						int idxInt = (int)(finalIdx + 0.5f);
						col->width = (float)idxInt;
						col->actualWidth = (float)idxInt;
					}
					else
					{
						// 其它列已被挤到 nameMin 还吸收不下 → 出滚动条
						for (int i = 0; i < nColumnCount; i++)
						{
							if (i == idx) continue;
							TABLE_VIEW_COLUMN_INFO* ci = m_vColumnInfos[i];
							int nmi = this->_measureColumnNameWidth(ci->columnName) + columnPandding * 2;
							if (nmi < 1) { nmi = 1; }
							ci->width = (float)nmi;
							ci->actualWidth = (float)nmi;
						}
						int idxInt = (int)(newActualIdx + 0.5f);
						col->width = (float)idxInt;
						col->actualWidth = (float)idxInt;
					}
				}
				else
				{
					// 滚动条模式: 只动被拖列, 其它列锁当前 actualWidth (固定像素)
					int newInt = (int)(newActualIdx + 0.5f);
					col->width = (float)newInt;
					col->actualWidth = (float)newInt;
					for (int i = 0; i < nColumnCount; i++)
					{
						if (i == idx) continue;
						TABLE_VIEW_COLUMN_INFO* ci = m_vColumnInfos[i];
						int aw = (int)(ci->actualWidth + 0.5f);
						ci->width = (float)aw;
						ci->actualWidth = (float)aw;
					}
				}

				this->m_hasUserColumnWidths = true;
				this->_reCalculateSize();  // 重算 actualLeft / m_totalSize / scrollbar 状态
			}
		}
	}
	else if (mouse_row == -1)
	{
		_processCancelMode(point);
	}

	if (m_mouseSelectColumnLine >= 0)
	{
		m_mouseSelectColumnLine = -1;
		HCURSOR hCur = ::LoadCursor(NULL, IDC_ARROW);
		::SetCursor(hCur);
	}

	this->Invalidate();
}


void SWTableScrollViewWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (mouse_row > 0)
	{
		if (m_pCallback)
		{
			m_pCallback->onTableScrollViewLeftMouseDblClick(this, mouse_row, mouse_col);
		}
	}
	this->Invalidate();
}


void SWTableScrollViewWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
	this->m_bMousePressed = true;
	m_mouse_pressed_pt = point;


}

void SWTableScrollViewWnd::OnMouseLeave()
{
	if (m_bMouseTracking)
	{
		m_bMouseTracking = FALSE;
		m_HitTest = TSV_HT_NOWHERE;

		this->RedrawWindow();
	}
}



int SWTableScrollViewWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// 创建并隐藏内嵌式编辑框
	m_inplaceEdit.CreateInplace(this);

	return 0;
}


void SWTableScrollViewWnd::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect rect;
	GetClientRect(&rect);

	// 双缓冲画图
	///////////////////////////////////////////////////////////////////////////////
	this->OnDraw(&dc, rect);
	///////////////////////////////////////////////////////////////////////////////
}

// 抑制默认背景擦除：message map 已注册 ON_WM_ERASEBKGND，但若没有这个 override
// MFC 会回退到 CWnd::OnEraseBkgnd 用默认背景刷（白色）擦客户区，拖动列边时
// Invalidate→OnEraseBkgnd 之间会闪白条/白块（OnDraw 走完整双缓冲 BitBlt 才覆盖）。
// 返回 TRUE 表示我们自己负责绘制背景（OnDraw 里有 quick_fillrect 黑色填充）。
BOOL SWTableScrollViewWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}


void SWTableScrollViewWnd::OnRButtonUp(UINT nFlags, CPoint point)
{
	this->m_current_sel_row = mouse_row;
	if (mouse_row == 0)
	{
		CMenu menu;
		menu.CreatePopupMenu(); //动态创建弹出式菜单对象

		menu.AppendMenu(MF_STRING, WM_DOCKING_WINDOW_AUTO_COLUMN_WIDTH, _T("自动调整列宽"));

		CPoint pt;
		GetCursorPos(&pt);
		menu.TrackPopupMenu(TPM_RIGHTBUTTON, pt.x, pt.y, this);
		menu.DestroyMenu();
	}
	else
	{
		// mouse_row > 0: 数据行；mouse_row == -1: 空白区域（也要把右键转给 callback，
		// 让宿主窗口在无选中行时也能弹出菜单，例如"新增/刷新"）
		if (m_pCallback)
		{
			m_pCallback->onTableScrollViewRightMouseClick(this, mouse_row, mouse_col);
		}
	}
	this->Invalidate();

	CWnd::OnRButtonUp(nFlags, point);
}


BOOL SWTableScrollViewWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (wParam == WM_DOCKING_WINDOW_AUTO_COLUMN_WIDTH)
	{
		this->OnAutoAdjustColunWidth();
	}

	return CWnd::OnCommand(wParam, lParam);
}


void SWTableScrollViewWnd::OnAutoAdjustColunWidth()
{
	this->m_isAutoColumn = true;
	this->m_isAutoAdjuested = false;
	// 显式菜单命令：用户主动要求按内容自适应，清掉"用户已设宽度"的保护位
	this->m_hasUserColumnWidths = false;

	this->_reCalculateSize();

	this->Invalidate();
}