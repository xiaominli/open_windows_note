#pragma once
#include <afxwin.h>

// 颜色宏（原框架取自主题；此处给中性深色 UI 值）
#define COLOR_WHITE           RGB(0xE0,0xE0,0xE0)
#define COLOR_ROW_MOUSE_BG    RGB(0x33,0x55,0x88)
#define COLOR_ROW_INTERVAL_BG RGB(0x26,0x26,0x26)

// 顶替商用绘图库：签名与移入控件的 call site 对齐
namespace SWPlotUtil {
    CSize quick_estimate_word_size_with_cache(HDC hdc, const char* text, HFONT hFont);
    void  quick_text(HDC hdc, const char* text, CPoint pt, COLORREF color, HFONT hFont, int align);
    void  quick_fillrect(HDC hdc, CRect rect, HPEN hPen, HBRUSH hBrush, int penWidth = 0);
    void  quick_flookfill_rect(HDC hdc, CRect rect, HPEN hPen, HBRUSH hBrush, COLORREF color);
    void  quick_line(HDC hdc, CPoint a, CPoint b, HPEN hPen);
    void  quick_line_path(HDC hdc, CPoint a, CPoint b);
    void  drawAL(HDC hdc, CPoint a, CPoint b, HPEN hPen, HBRUSH hBrush);
}

// 顶替原框架的全局 GDI 资源池
class GlobalGraphicObject {
public:
    HPEN   m_hPenLineBlack = nullptr, m_hPenLineWhite = nullptr, m_hPenLineWhite2 = nullptr, m_hPenLineGray = nullptr;
    HPEN   m_hPenDomRowMouseBG = nullptr, m_hPenTableRowIntervalBG = nullptr;
    HBRUSH m_hBrushBlack = nullptr, m_hBrushWhite = nullptr;
    HBRUSH m_hBrushRowMouseBG = nullptr, m_hBrushTableRowIntervalBG = nullptr;
    HFONT  m_hLabelFont = nullptr;
    int    m_nLabelFontHeight = 16;
    int    m_nPlotTitleFontHeight = 18;
    GlobalGraphicObject();
    ~GlobalGraphicObject();
};
extern GlobalGraphicObject m_global_graphic_objects;

// 顶替 BCGPGlobalUtils 的 globalUtils（仅用到 ScaleByDPI）
struct BcgpGlobalUtilsShim {
    int ScaleByDPI(int x) const { return x; }   // P4 不做 DPI 缩放，恒等
};
extern BcgpGlobalUtilsShim globalUtils;
