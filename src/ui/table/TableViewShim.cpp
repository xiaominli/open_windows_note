#include "ui/table/TableViewShim.h"
#include <string>

GlobalGraphicObject m_global_graphic_objects;   // 移入控件引用的全局实例
BcgpGlobalUtilsShim globalUtils;                // 顶替 BCGP globalUtils

GlobalGraphicObject::GlobalGraphicObject() {
    m_hPenLineBlack  = ::CreatePen(PS_SOLID, 1, RGB(0x10,0x10,0x10));
    m_hPenLineWhite  = ::CreatePen(PS_SOLID, 1, RGB(0xD0,0xD0,0xD0));
    m_hPenLineWhite2 = ::CreatePen(PS_SOLID, 1, RGB(0x40,0x40,0x40));   // 列分隔线，较暗
    m_hPenLineGray   = ::CreatePen(PS_SOLID, 1, RGB(0x60,0x60,0x60));
    m_hPenDomRowMouseBG      = ::CreatePen(PS_SOLID, 1, COLOR_ROW_MOUSE_BG);
    m_hPenTableRowIntervalBG = ::CreatePen(PS_SOLID, 1, COLOR_ROW_INTERVAL_BG);
    m_hBrushBlack = ::CreateSolidBrush(RGB(0x1C,0x1C,0x1C));
    m_hBrushWhite = ::CreateSolidBrush(RGB(0xF0,0xF0,0xF0));
    m_hBrushRowMouseBG         = ::CreateSolidBrush(COLOR_ROW_MOUSE_BG);
    m_hBrushTableRowIntervalBG = ::CreateSolidBrush(COLOR_ROW_INTERVAL_BG);
    LOGFONTA lf{}; lf.lfHeight = -m_nLabelFontHeight; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, "Microsoft YaHei");
    m_hLabelFont = ::CreateFontIndirectA(&lf);
}
GlobalGraphicObject::~GlobalGraphicObject() {
    HGDIOBJ objs[] = { m_hPenLineBlack,m_hPenLineWhite,m_hPenLineWhite2,m_hPenLineGray,m_hPenDomRowMouseBG,
        m_hPenTableRowIntervalBG,m_hBrushBlack,m_hBrushWhite,m_hBrushRowMouseBG,
        m_hBrushTableRowIntervalBG,m_hLabelFont };
    for (HGDIOBJ o : objs) if (o) ::DeleteObject(o);
}

namespace SWPlotUtil {
CSize quick_estimate_word_size_with_cache(HDC hdc, const char* text, HFONT hFont) {
    HGDIOBJ old = ::SelectObject(hdc, hFont);
    SIZE sz{ 0,0 };
    // 列名是 UTF-8：转宽字符量宽，否则中文按 ANSI 量得错宽
    if (text) {
        int n = ::MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (n > 1) {
            std::wstring w(n - 1, L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, text, -1, &w[0], n);
            ::GetTextExtentPoint32W(hdc, w.c_str(), (int)w.size(), &sz);
        }
    }
    ::SelectObject(hdc, old);
    return CSize(sz.cx, sz.cy);
}
void quick_text(HDC hdc, const char* text, CPoint pt, COLORREF color, HFONT hFont, int /*align*/) {
    if (!text) return;
    HGDIOBJ old = ::SelectObject(hdc, hFont);
    int bk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oc = ::SetTextColor(hdc, color);
    // 文本是 UTF-8：转宽字符再 TextOutW，避免中文按 ANSI 解码乱码
    int n = ::MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (n > 1) {
        std::wstring w(n - 1, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text, -1, &w[0], n);
        ::TextOutW(hdc, pt.x, pt.y, w.c_str(), (int)w.size());   // caller 预置 x（右对齐传 cx-w）
    }
    ::SetTextColor(hdc, oc); ::SetBkMode(hdc, bk); ::SelectObject(hdc, old);
}
void quick_fillrect(HDC hdc, CRect rect, HPEN hPen, HBRUSH hBrush, int /*penWidth*/) {
    HGDIOBJ op = ::SelectObject(hdc, hPen), ob = ::SelectObject(hdc, hBrush);
    ::Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    ::SelectObject(hdc, op); ::SelectObject(hdc, ob);
}
void quick_flookfill_rect(HDC hdc, CRect rect, HPEN /*hPen*/, HBRUSH /*hBrush*/, COLORREF color) {
    HBRUSH b = ::CreateSolidBrush(color);
    RECT r = rect; ::FillRect(hdc, &r, b);
    ::DeleteObject(b);
}
void quick_line(HDC hdc, CPoint a, CPoint b, HPEN hPen) {
    HGDIOBJ op = ::SelectObject(hdc, hPen);
    ::MoveToEx(hdc, a.x, a.y, nullptr); ::LineTo(hdc, b.x, b.y);
    ::SelectObject(hdc, op);
}
void quick_line_path(HDC hdc, CPoint a, CPoint b) {
    HGDIOBJ op = ::SelectObject(hdc, m_global_graphic_objects.m_hPenLineGray);
    ::MoveToEx(hdc, a.x, a.y, nullptr); ::LineTo(hdc, b.x, b.y);
    ::SelectObject(hdc, op);
}
void drawAL(HDC hdc, CPoint a, CPoint b, HPEN hPen, HBRUSH /*hBrush*/) {   // 排序小箭头：一段线近似
    quick_line(hdc, a, b, hPen);
}
}
