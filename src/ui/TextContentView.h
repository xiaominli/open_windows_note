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
    void ApplyTheme(uint32_t bgRgb, uint32_t textRgb) override;
private:
    CRichEditCtrl m_edit;
    bool m_created = false;
    uint32_t m_bgRgb = 0xFFF7B0;
    uint32_t m_textRgb = 0x202020;
};
