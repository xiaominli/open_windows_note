#pragma once
#include <afxwin.h>
#include <functional>

// 管理器顶部「新建」图标工具条：文档页=文本 / 勾选框=清单 / 画笔=涂鸦。
// 与便签窗标题栏一致的 GDI+ 自绘扁平图标，悬停高亮。
class CNewNoteBar : public CWnd {
public:
    std::function<void()> onNewText, onNewChecklist, onNewDrawing;
    bool Create(CWnd* parent, const CRect& rc);
    void Reposition(const CRect& rc);
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC*);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg LRESULT OnMouseLeave(WPARAM, LPARAM);
    DECLARE_MESSAGE_MAP()
private:
    static CRect btnRect(int i);      // 第 i 个图标的矩形
    int hitTest(CPoint pt) const;     // 0/1/2,未命中 -1
    int m_hover = -1;
    bool m_tracking = false;          // TrackMouseEvent 已挂
};
