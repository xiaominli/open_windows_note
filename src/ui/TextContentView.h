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
    void ApplyFormat(own::FmtOp op) override;
    static void SetDefaultFontPt(int pt);          // 默认字号（新输入生效；启动/设置变更时调用）
private:
    CRichEditCtrl m_edit;
    bool m_created = false;
    uint32_t m_bgRgb = 0xFFF7B0;
    uint32_t m_textRgb = 0x202020;
};
