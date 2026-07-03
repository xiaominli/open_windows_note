#pragma once
#include <afxwin.h>
#include <vector>
#include "ui/INoteContentView.h"
#include "domain/Models.h"
class CDrawingContentView : public CWnd, public INoteContentView {
public:
    bool Create(CWnd* parent, const CRect& rc) override;
    void Load(const own::Note& note) override;
    bool Save(std::vector<uint8_t>& outBlob, std::string& outPlain) override;
    void Reposition(const CRect& rc) override;
    bool IsDirty() const override;
    void SetVisible(bool show) override;
    void DestroyView() override;
    void ApplyTheme(uint32_t bgRgb, uint32_t textRgb) override;
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    DECLARE_MESSAGE_MAP()
private:
    int toolAtPoint(CPoint pt) const;     // 命中哪个工具槽；-1 = 画布
    std::vector<own::Stroke> m_strokes;
    own::Stroke m_cur;                     // 正在画的笔迹
    bool m_drawing = false;
    bool m_eraser = false;
    uint32_t m_color = 0x000000;
    int m_width = 3;
    bool m_dirty = false;
    bool m_created = false;
    uint32_t m_bgRgb = 0xFFF7B0;
};
