#pragma once
#include <afxwin.h>
#include "domain/Models.h"
#include "ui/TitleBarLayout.h"
#include "ui/ResizeMath.h"
namespace own { class NoteStore; }
class CNoteWindow : public CWnd {
public:
    bool Create(const own::Note& note, own::NoteStore* store);
    int64_t noteId() const { return m_note.id; }
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    DECLARE_MESSAGE_MAP()
private:
    own::TitleBarRects layout() const;   // 用当前 client 尺寸算标题栏
    own::Note m_note;
    own::NoteStore* m_store = nullptr;
    bool m_dragging = false;
    CPoint m_dragAnchorScreen;   // 按下时鼠标屏幕坐标
    CRect  m_dragStartRect;      // 按下时窗口屏幕矩形
    own::ResizeEdge m_resizeEdge = own::ResizeEdge::None;
    bool m_resizing = false;
    CPoint m_resizeAnchorScreen;
    CRect  m_resizeStartRect;
    int m_expandedHeight = 0;    // 卷起前的展开态高度缓存
};
