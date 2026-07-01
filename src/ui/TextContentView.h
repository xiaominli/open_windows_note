#pragma once
#include <afxcmn.h>            // CRichEditCtrl
#include "ui/INoteContentView.h"
class CTextContentView : public INoteContentView {
public:
    bool Create(CWnd* parent, const CRect& rc) override;
    void Load(const own::Note& note) override;
    bool Save(std::vector<uint8_t>& outBlob, std::string& outPlain) override;
    void Reposition(const CRect& rc) override;
    bool IsDirty() const override;
    void SetVisible(bool show) override;
    void DestroyView() override;
private:
    CRichEditCtrl m_edit;
    bool m_created = false;
};
