#pragma once
#include <afxwin.h>
#include <vector>
#include "ui/INoteContentView.h"
#include "domain/Models.h"
class CChecklistContentView : public CWnd, public INoteContentView {
public:
    bool Create(CWnd* parent, const CRect& rc) override;
    void Load(const own::Note& note) override;
    bool Save(std::vector<uint8_t>& outBlob, std::string& outPlain) override;
    void Reposition(const CRect& rc) override;
    bool IsDirty() const override;
    void SetVisible(bool show) override;
    void DestroyView() override;
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnCommitEdit();          // 就地编辑提交（EN_KILLFOCUS）
    DECLARE_MESSAGE_MAP()
private:
    void beginEdit(int index);
    void commitEdit();
    std::vector<own::ChecklistItem> m_items;
    std::vector<uint8_t> m_originalBlob;  // 解析失败时保留，防空覆盖
    bool m_dirty = false;
    bool m_created = false;
    CEdit m_edit;                         // 就地编辑器
    int m_editing = -1;                   // 正在编辑的行；-1 无
};
