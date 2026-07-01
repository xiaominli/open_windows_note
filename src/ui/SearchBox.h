#pragma once
#include <afxwin.h>
#include <functional>
#include <string>
class CSearchBox : public CWnd {
public:
    std::function<void(const std::string&)> onChanged;
    bool Create(CWnd* parent, const CRect& rc);
    void Reposition(const CRect& rc);
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC*);
    afx_msg void OnEditChanged();
    DECLARE_MESSAGE_MAP()
private:
    CEdit m_edit;
};
