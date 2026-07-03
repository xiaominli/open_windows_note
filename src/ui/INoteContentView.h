#pragma once
#include <afxwin.h>
#include <vector>
#include <string>
#include <cstdint>
#include "domain/Models.h"
#include "ui/FormatBarLayout.h"
class INoteContentView {
public:
    virtual ~INoteContentView() {}
    virtual bool Create(CWnd* parent, const CRect& rc) = 0;
    virtual void Load(const own::Note& note) = 0;
    virtual bool Save(std::vector<uint8_t>& outBlob, std::string& outPlain) = 0;
    virtual void Reposition(const CRect& rc) = 0;
    virtual bool IsDirty() const = 0;
    virtual void SetVisible(bool show) = 0;
    virtual void DestroyView() = 0;
    virtual void ApplyTheme(uint32_t bgRgb, uint32_t textRgb) {}   // 0xRRGGBB；默认忽略
    virtual void ApplyFormat(own::FmtOp op) {}                     // 选区格式；默认忽略（清单/涂鸦）
};
