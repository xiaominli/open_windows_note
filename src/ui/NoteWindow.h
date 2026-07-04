#pragma once
#include <afxwin.h>
#include <memory>
#include "domain/Models.h"
#include "ui/TitleBarLayout.h"
#include "ui/ResizeMath.h"
#include "ui/INoteContentView.h"
#include "ui/FormatBarLayout.h"
namespace own { class NoteStore; }
class CNoteWindow : public CWnd {
public:
    bool Create(const own::Note& note, own::NoteStore* store);
    int64_t noteId() const { return m_note.id; }
    void flushNow() { flushContent(); }   // 导出备份前确保最新内容落盘
    const std::string& stickTarget() const { return m_note.stickTarget; }
    void setStickyVisible(bool show);    // 贴窗瞬态显隐：不写 visible 标志、不落库
    void clearStickyMute() { m_stickyMuted = false; }   // 列表「打开」等显式打开时解除
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg int  OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint pt);
    DECLARE_MESSAGE_MAP()
private:
    static const UINT kSaveTimer = 1;
    own::TitleBarRects layout() const;   // 用当前 client 尺寸算标题栏
    void layoutContent();     // 把内容视图移到 noteContentRect
    void flushContent();      // 脏则落盘
    bool hasFormatBar() const;   // 仅富文本便签显示工具条
    int  contentTop() const;     // 标题栏高 + 工具条高（无工具条时仅标题栏）
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
    std::unique_ptr<INoteContentView> m_content;
    own::Theme m_theme;          // 当前生效主题（含回落默认）
    void loadTheme();            // 按 m_note.themeId 取色，取不到回落内置黄
    bool m_stickyMuted = false;   // 贴窗便签被 × 关闭后静音：前台匹配不再唤回
};
