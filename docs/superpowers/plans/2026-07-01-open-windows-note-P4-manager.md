# P4 管理器窗口 + 便签列表 + 搜索 + 分组标签 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增一个管理器窗口(`CMainFrame`),用**自绘虚拟化表格**列出全部便签,支持文本搜索、双击打开便签窗、右键操作(删除/显隐/改分组/加标签)与分组标签管理。

**Architecture:** 复用参考控件 `SWTableScrollViewWnd` 的**回调式虚拟列表模式**(`setTotalRowCount` + `onPrepareDraw` + `onDraw...Cell` + 命中/右键回调)。因参考实现耦合 BCGPro(商用库)/CTP/内部框架,本计划把它**移入 `src/ui/table/` 并去依赖**:用一个 GDI 垫片(`SWPlotUtil`/`GlobalGraphicObject`/`COLOR_*`)顶替商用绘图库,滚动条改自绘,列宽持久化两函数改为 no-op(去掉 JsonCpp/全局数据依赖)。管理器与便签窗解耦:表格适配器 `CNoteListView` 只依赖 `NoteStore` 与一个 `INoteWindowHost` 接口(由 `CNoteApp` 实现)来开/关/刷新便签窗。可测逻辑(标题派生、相对时间、列表排序)抽成无 HWND 纯函数走 doctest。

**Tech Stack:** C++17 · MFC 静态链接 · Win32 · GDI/GDI+ · SQLite(P1 数据层)· doctest。

## Global Constraints

- 语言/工具链:C++17(`/std:c++17`)、MFC **静态链接**、无 PCH、仅 `x64`。
- 编码:所有工程 ClCompile 带 `/utf-8`;`.cpp` 内中文字面量直接写;**测试文件里的中文断言用 UTF-8 十六进制转义**(如 `"\xE5\x88\x9A\xE5\x88\x9A"`)。
- 命名空间:`src/data`/`src/domain` 及 `src/ui` 下**纯逻辑**一律 `namespace own`,且**不得** include `<afxwin.h>`/`<windows.h>`,以便进 tests 工程 doctest。移入的表格控件(`SWTableScrollViewWnd` 等)是 **MFC UI 代码,保持全局命名空间(无 `own::`)**,仅进 app 工程。
- 渲染:GDI/GDI+;**一律自绘**;唯一原生控件例外是富文本 RichEdit(P3)与「就地编辑」用的 `CEdit`(单元格编辑 / 新建分组标签输入)。**列表禁用 `CListCtrl/CListView`**——用移入的自绘表格。
- 构建:只能通过 `.sln` 构建。MSBuild 路径:`"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`(下称 `$MSB`)。
- **每次重建前先杀残留进程**:`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`(单实例 exe 锁定输出文件 → LNK1168)。
- 自动化达标线:GUI 任务以「app 工程构建链接通过 + `x64/Debug/tests.exe` 全绿」为准;窗口/表格交互落 Task 14 手工冒烟清单。
- 每次提交末尾附:`Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`。
- 分支:在 `feat/p3-content-views` 之上继续,或新建 `feat/p4-manager` 从 P3 分支切出。执行前确认当前分支非 `main`。

**移入控件的去向与授权(设计 §8 备注要求确认):** 参考控件目前在 `docs/temp/tableview/`(不入库)。P4 把 `SWTableScrollViewWnd.{h,cpp}`、`SWTableScrollViewDefs.h`、`SWInplaceEdit.{h,cpp}` 复制进 `src/ui/table/`(**入库**),并**移除**其对 BCGPro/CTP/SWPlotUtil/GlobalGraphicObject/JsonCpp 的依赖(改用本仓自写垫片)。**不复制任何 BCGP/CTP 代码入库**。这些 SW 文件为用户自有资产,随本仓 MIT 一并开源。

**已定的两个务实取舍(执行者须知,可在执行交接时否决):**
1. **管理器窗口框架用系统标准边框**(`WS_OVERLAPPEDWINDOW`,可缩放/最小化/关闭),内部内容(搜索框 + 列表)全自绘。「一律自绘」约束聚焦于列表与便签面——列表已是自绘表格、无 `CListCtrl`,故满足;管理器全自绘 chrome 作为后续 polish。
2. **分组/标签的筛选侧栏**不在 P4;P4 提供分组/标签的**赋值**(右键)与文本搜索过滤;按分组/标签过滤列表留后续。

**承接的既有接口(勿重复实现):**
- P1 领域:`own::Note`(`id/type/title/plainText/themeId/groupId/updatedAt/...`)、`own::NoteType{RichText=0,Checklist=1,Drawing=2}`、`own::Group{int64_t id; std::string name; int orderIdx;}`、`own::Tag{int64_t id; std::string name;}`。
- P1 数据 `own::NoteStore`:`std::vector<Note> query(const NoteQuery&)`(`NoteQuery{std::string search; int64_t groupId=-1; int64_t tagId=-1; bool onlyVisible=false;}`)、`std::optional<Note> getNote(int64_t)`、`bool deleteNote(int64_t)`、`bool updateFlags(int64_t,int,bool,bool,bool)`、`int64_t upsertGroup(Group)`、`std::vector<Group> allGroups()`、`int64_t upsertTag(const std::string&)`、`std::vector<Tag> allTags()`、`bool addTagToNote(int64_t,int64_t)`、`std::vector<Tag> tagsOfNote(int64_t)`、`bool updateNote(const Note&, int64_t)`。
- P2/P3 应用 `CNoteApp`:`own::Database m_db; std::unique_ptr<own::NoteStore> m_store; CAppHostWindow m_host; std::vector<std::unique_ptr<CNoteWindow>> m_notes;`,`void createAndShowNote(const own::Note&)`。`CNoteWindow`:`bool Create(const own::Note&, own::NoteStore*)`、`int64_t noteId() const`。`CAppHostWindow`:`kHotkeyQuit=1/kHotkeyNew=2/kHotkeyNewChecklist=3/kHotkeyNewDrawing=4`,`std::function<void()> onNewNote/onNewChecklist/onNewDrawing/onQuit`。

---

## 文件结构(本计划新增/修改)

**新增 · 移入的自绘表格(全局命名空间,仅进 app 工程,`src/ui/table/`):**
- `TableViewShim.h`, `TableViewShim.cpp` — 顶替商用绘图库的 GDI 垫片(`SWPlotUtil` 命名空间 + `GlobalGraphicObject` + `COLOR_*`)。
- `SWTableScrollViewDefs.h` — 从 `docs/temp` 复制(列信息/命中枚举,零依赖)。
- `SWInplaceEdit.h`, `SWInplaceEdit.cpp` — 从 `docs/temp` 复制并改 include。
- `SWTableScrollViewWnd.h`, `SWTableScrollViewWnd.cpp` — 从 `docs/temp` 复制并去依赖。

**新增 · 纯逻辑(`namespace own`,进 tests + app):**
- `src/domain/NoteListFormat.h`, `src/domain/NoteListFormat.cpp` — `noteTitleText` / `formatRelativeTime` / `sortNoteRows`。

**新增 · 管理器与适配(仅进 app 工程):**
- `src/app/NoteWindowHost.h` — `INoteWindowHost` 接口。
- `src/ui/NoteListView.h`, `src/ui/NoteListView.cpp` — `CNoteListView`(表格回调适配 + 右键菜单)。
- `src/ui/SearchBox.h`, `src/ui/SearchBox.cpp` — `CSearchBox`(自绘搜索框 + 内嵌小 CEdit)。
- `src/ui/TextPrompt.h`, `src/ui/TextPrompt.cpp` — `promptText`(新建分组/标签的极简模态输入)。
- `src/app/MainFrame.h`, `src/app/MainFrame.cpp` — `CMainFrame`(管理器窗口)。

**修改:**
- `src/app/NoteApp.h`, `src/app/NoteApp.cpp` — 实现 `INoteWindowHost`;建/显管理器;`Ctrl+Alt+M` 切换管理器、`Ctrl+Alt+H` 显隐全部。
- `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp` — 加两个热键与回调。
- `tests/tests.vcxproj`、`app/open_windows_note_app.vcxproj` — 登记新文件。
- `docs/superpowers/smoke/P4-smoke-checklist.md` — 新增。

---

### Task 1: 表格控件 GDI 垫片(SWPlotUtil / GlobalGraphicObject / COLOR)

**Files:**
- Create: `src/ui/table/TableViewShim.h`, `src/ui/table/TableViewShim.cpp`

**Interfaces:**
- Produces(全局命名空间,供移入的 `SWTableScrollViewWnd.cpp` 原样调用):
  - `namespace SWPlotUtil { CSize quick_estimate_word_size_with_cache(HDC,const char*,HFONT); void quick_text(HDC,const char*,CPoint,COLORREF,HFONT,int align); void quick_fillrect(HDC,CRect,HPEN,HBRUSH,int penWidth=0); void quick_flookfill_rect(HDC,CRect,HPEN,HBRUSH,COLORREF); void quick_line(HDC,CPoint,CPoint,HPEN); void quick_line_path(HDC,CPoint,CPoint); void drawAL(HDC,CPoint,CPoint,HPEN,HBRUSH); }`
  - `class GlobalGraphicObject { ... };` 与全局 `extern GlobalGraphicObject m_global_graphic_objects;`
  - 宏 `COLOR_WHITE / COLOR_ROW_MOUSE_BG / COLOR_ROW_INTERVAL_BG`。
- 说明:本任务是纯 GDI 代码、无独立单测(HDC 依赖);达标线=Task 2 里被移入控件链接通过。

- [ ] **Step 1: 写垫片头**

`src/ui/table/TableViewShim.h`:
```cpp
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
    HPEN   m_hPenLineBlack = nullptr, m_hPenLineWhite = nullptr, m_hPenLineGray = nullptr;
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
```

- [ ] **Step 2: 写垫片实现**

`src/ui/table/TableViewShim.cpp`:
```cpp
#include "ui/table/TableViewShim.h"

GlobalGraphicObject m_global_graphic_objects;   // 移入控件引用的全局实例

GlobalGraphicObject::GlobalGraphicObject() {
    m_hPenLineBlack = ::CreatePen(PS_SOLID, 1, RGB(0x10,0x10,0x10));
    m_hPenLineWhite = ::CreatePen(PS_SOLID, 1, RGB(0xD0,0xD0,0xD0));
    m_hPenLineGray  = ::CreatePen(PS_SOLID, 1, RGB(0x60,0x60,0x60));
    m_hPenDomRowMouseBG      = ::CreatePen(PS_SOLID, 1, COLOR_ROW_MOUSE_BG);
    m_hPenTableRowIntervalBG = ::CreatePen(PS_SOLID, 1, COLOR_ROW_INTERVAL_BG);
    m_hBrushBlack = ::CreateSolidBrush(RGB(0x1C,0x1C,0x1C));
    m_hBrushWhite = ::CreateSolidBrush(RGB(0xF0,0xF0,0xF0));
    m_hBrushRowMouseBG      = ::CreateSolidBrush(COLOR_ROW_MOUSE_BG);
    m_hBrushTableRowIntervalBG = ::CreateSolidBrush(COLOR_ROW_INTERVAL_BG);
    LOGFONTA lf{}; lf.lfHeight = -m_nLabelFontHeight; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, "Microsoft YaHei");
    m_hLabelFont = ::CreateFontIndirectA(&lf);
}
GlobalGraphicObject::~GlobalGraphicObject() {
    HGDIOBJ objs[] = { m_hPenLineBlack,m_hPenLineWhite,m_hPenLineGray,m_hPenDomRowMouseBG,
        m_hPenTableRowIntervalBG,m_hBrushBlack,m_hBrushWhite,m_hBrushRowMouseBG,
        m_hBrushTableRowIntervalBG,m_hLabelFont };
    for (HGDIOBJ o : objs) if (o) ::DeleteObject(o);
}

namespace SWPlotUtil {
CSize quick_estimate_word_size_with_cache(HDC hdc, const char* text, HFONT hFont) {
    HGDIOBJ old = ::SelectObject(hdc, hFont);
    SIZE sz{ 0,0 };
    if (text) ::GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);
    ::SelectObject(hdc, old);
    return CSize(sz.cx, sz.cy);
}
void quick_text(HDC hdc, const char* text, CPoint pt, COLORREF color, HFONT hFont, int /*align*/) {
    if (!text) return;
    HGDIOBJ old = ::SelectObject(hdc, hFont);
    int bk = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF oc = ::SetTextColor(hdc, color);
    ::TextOutA(hdc, pt.x, pt.y, text, (int)strlen(text));   // caller 预置 x（右对齐传 cx-w）
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
```

- [ ] **Step 3: 编译占位(此文件随 Task 2 一并进 app 工程构建)**

本任务不单独构建;进入 Task 2 后一起编译。若想提前验证语法,可临时把 `TableViewShim.cpp` 加入 app 工程构建一次(见 Task 2 登记步骤)。

- [ ] **Step 4: Commit**

```bash
git add src/ui/table/TableViewShim.h src/ui/table/TableViewShim.cpp
git commit -m "feat(ui/table): GDI shim replacing BCGP/SWPlotUtil for ported table control

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: 移入并去依赖 SWTableScrollViewWnd + SWInplaceEdit

**Files:**
- Create(复制自 `docs/temp/tableview/`,再改): `src/ui/table/SWTableScrollViewDefs.h`, `src/ui/table/SWInplaceEdit.h`, `src/ui/table/SWInplaceEdit.cpp`, `src/ui/table/SWTableScrollViewWnd.h`, `src/ui/table/SWTableScrollViewWnd.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 1 垫片。
- Produces: 可编译的 `class SWTableScrollViewWnd : public CWnd`(接口见其 `.h`:`setTableScrollViewCallback`/`setColumnInfos`/`setTotalRowCount`/`autoResizeColumns`/`setFrozenColumnCount`/`setMarginRect`)与回调接口 `ISWTableScrollViewWndCallback`(`onTableScrollViewDrawCell`/`...LeftMouseDblClick`/`...RightMouseClick`/`...PrepareDraw`/`...SortColumn`/`...AutoAdjustColumnWdidth` 等)。
- 说明:本任务是**移植+去依赖**,不重写控件逻辑。达标线=app 工程链接通过。

- [ ] **Step 1: 复制文件到 `src/ui/table/`**

```bash
cp docs/temp/tableview/SWTableScrollViewDefs.h src/ui/table/
cp docs/temp/tableview/SWInplaceEdit.h         src/ui/table/
cp docs/temp/tableview/SWInplaceEdit.cpp       src/ui/table/
cp docs/temp/tableview/SWTableScrollViewWnd.h  src/ui/table/
cp docs/temp/tableview/SWTableScrollViewWnd.cpp src/ui/table/
```

- [ ] **Step 2: 改 `SWTableScrollViewWnd.h` 的 include 头**

把文件顶部
```cpp
#include "StdAfx.h"
#include <string>
#include "SWTableScrollViewDefs.h"
#include "SWInplaceEdit.h"
```
改为
```cpp
#include <afxwin.h>
#include <string>
#include <vector>
#include "ui/table/SWTableScrollViewDefs.h"
#include "ui/table/SWInplaceEdit.h"
```

- [ ] **Step 3: 改 `SWInplaceEdit.h` 的 include 头**

把
```cpp
#include "StdAfx.h"
#include "SWTableScrollViewDefs.h"
```
改为
```cpp
#include <afxwin.h>
#include "ui/table/SWTableScrollViewDefs.h"
```

- [ ] **Step 4: 改 `SWInplaceEdit.cpp` 的 include 头**

把
```cpp
#include "stdafx.h"
#include "SWInplaceEdit.h"
#include "SWTableScrollViewWnd.h"
```
改为
```cpp
#include "ui/table/SWInplaceEdit.h"
#include "ui/table/SWTableScrollViewWnd.h"
```

- [ ] **Step 5: 改 `SWTableScrollViewWnd.cpp` 的 include 头 + 去掉 extern 声明**

把文件顶部(约 1–15 行)整段外部 include:
```cpp
#include "stdafx.h"
#include "SWTableScrollViewWnd.h"

#include "SWPlotUtil.h"
#include "GlobalGraphicObject.h"
#include "mcbc/BCGPVisualManager.h"
#include "mcbc/BCGPGlobalUtils.h"
#include "mc_core/utils/SWDebug.h"
#include "CTPTradeMaster.h"
```
替换为:
```cpp
#include "ui/table/SWTableScrollViewWnd.h"
#include "ui/table/TableViewShim.h"
#include <string>
#include <vector>
#include <math.h>
```
并**删除**约第 15 行的 `extern GlobalGraphicObject m_global_graphic_objects;`(垫片头已声明)。

- [ ] **Step 6: 列宽持久化两函数改为 no-op(去 JsonCpp / m_global_data / string_format)**

把 `_loadTableColumnWidthFromFile` 整个函数体替换为:
```cpp
int SWTableScrollViewWnd::_loadTableColumnWidthFromFile(const char* /*windowToken*/, std::vector<TABLE_VIEW_COLUMN_INFO*> /*vColumnInfo*/) {
    return 0;   // P4 不持久化列宽（去 JsonCpp 依赖）；后续可接 SettingsStore
}
```
把 `_saveTableColumnWidthToFile` 整个函数体替换为:
```cpp
int SWTableScrollViewWnd::_saveTableColumnWidthToFile(const char* /*windowToken*/, std::vector<TABLE_VIEW_COLUMN_INFO*> /*vColumnInfo*/) {
    return 0;
}
```

- [ ] **Step 7: 滚动条绘制改自绘(替换 BCGP)**

在 `_onRendScrollBar` 与 `_onRendVertScrollBar` 两个函数体内,把每一处:
- `CBCGPVisualManager::GetInstance()->OnScrollBarDrawButton(pDC, R, ...);` → `pDC->FillSolidRect(R, RGB(0x55,0x55,0x55));`
- `CBCGPVisualManager::GetInstance()->OnScrollBarDrawThumb(pDC, R, ...);` → `pDC->FillSolidRect(R, RGB(0x88,0x88,0x88));`
- `CBCGPVisualManager::GetInstance()->OnScrollBarFillBackground(pDC, R, ...);` → `pDC->FillSolidRect(R, RGB(0x2A,0x2A,0x2A));`

(`R` 指该调用第 2 个实参的矩形变量名,如 `rectBtn[0]`/`rectThumb`/`rectBack`;逐处照搬其矩形变量。)搜索确认无残留:构建前 `grep -n "BCGP\|SWPlotUtil\|GlobalGraphicObject\|CTPTradeMaster\|SWDebug\|Json\|string_format\|m_global_data" src/ui/table/SWTableScrollViewWnd.cpp` 只应命中 `m_global_graphic_objects`(垫片提供)。

- [ ] **Step 8: 登记进 app 工程**

`app/open_windows_note_app.vcxproj` 源 `ItemGroup` 加:
```xml
    <ClCompile Include="..\src\ui\table\TableViewShim.cpp" />
    <ClCompile Include="..\src\ui\table\SWInplaceEdit.cpp" />
    <ClCompile Include="..\src\ui\table\SWTableScrollViewWnd.cpp" />
```
头 `ItemGroup` 加:
```xml
    <ClInclude Include="..\src\ui\table\TableViewShim.h" />
    <ClInclude Include="..\src\ui\table\SWTableScrollViewDefs.h" />
    <ClInclude Include="..\src\ui\table\SWInplaceEdit.h" />
    <ClInclude Include="..\src\ui\table\SWTableScrollViewWnd.h" />
```

- [ ] **Step 9: 构建 + 单测**

Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -12 && ./x64/Debug/tests.exe 2>&1 | tail -3
```
Expected: app 工程链接通过(表格控件 + 垫片编入);tests 全绿(数量不变)。若报未解析符号或未知类型,回到 Step 5–7 补对应垫片/替换。

- [ ] **Step 10: Commit**

```bash
git add src/ui/table/ app/open_windows_note_app.vcxproj
git commit -m "feat(ui/table): vendor SWTableScrollViewWnd de-coupled from BCGP/CTP/JsonCpp

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: noteTitleText / formatRelativeTime / sortNoteRows(列表格式化,纯逻辑)

**Files:**
- Create: `src/domain/NoteListFormat.h`, `src/domain/NoteListFormat.cpp`
- Test: `tests/test_note_list_format.cpp`

**Interfaces:**
- Consumes: `own::Note`。
- Produces:
  - `std::string own::noteTitleText(const Note& n);` —— `title` 非空→`title`;否则取 `plainText` 首行前 40 字节(已是搜索小写缓存,可接受);两者皆空→`"(无标题)"`。
  - `std::string own::formatRelativeTime(int64_t nowSec, int64_t thenSec);` —— `<60s`→`"刚刚"`;`<1h`→`"N分钟前"`;`<24h`→`"N小时前"`;`<7d`→`"N天前"`;否则→按 UTC 的 `"YYYY-MM-DD"`(用 `gmtime`,保证测试确定性)。未来时间(then>now)按 `"刚刚"`。
  - `enum class own::NoteSortKey { Title, Updated };`
  - `void own::sortNoteRows(std::vector<Note>& rows, NoteSortKey key, int order);` —— `order>0` 升序、`order<0` 降序、`order==0` 不动;`Title` 按 `noteTitleText` 字典序,`Updated` 按 `updatedAt`。稳定排序。

- [ ] **Step 1: 写失败测试**

`tests/test_note_list_format.cpp`:
```cpp
#include "doctest.h"
#include "domain/NoteListFormat.h"
using own::Note;
TEST_CASE("noteTitleText prefers title, falls back to plainText, then placeholder") {
    Note a; a.title = "Hello";
    CHECK(own::noteTitleText(a) == "Hello");
    Note b; b.plainText = "buy milk";
    CHECK(own::noteTitleText(b) == "buy milk");
    Note c;
    CHECK(own::noteTitleText(c) == "\xE6\x97\xA0\xE6\xA0\x87\xE9\xA2\x98"); // 无标题(带括号见下)
}
TEST_CASE("noteTitleText placeholder is wrapped and first line only") {
    Note c;
    CHECK(own::noteTitleText(c) == "(\xE6\x97\xA0\xE6\xA0\x87\xE9\xA2\x98)"); // (无标题)
    Note d; d.plainText = "line1\nline2";
    CHECK(own::noteTitleText(d) == "line1");
}
TEST_CASE("formatRelativeTime buckets") {
    CHECK(own::formatRelativeTime(1000, 1000) == "\xE5\x88\x9A\xE5\x88\x9A");          // 刚刚
    CHECK(own::formatRelativeTime(1000, 990)  == "\xE5\x88\x9A\xE5\x88\x9A");          // 未来/极近 → 刚刚
    CHECK(own::formatRelativeTime(1000 + 120, 1000) == "2\xE5\x88\x86\xE9\x92\x9F\xE5\x89\x8D");   // 2分钟前
    CHECK(own::formatRelativeTime(1000 + 2*3600, 1000) == "2\xE5\xB0\x8F\xE6\x97\xB6\xE5\x89\x8D"); // 2小时前
    CHECK(own::formatRelativeTime(1000 + 3*86400, 1000) == "3\xE5\xA4\xA9\xE5\x89\x8D");            // 3天前
    CHECK(own::formatRelativeTime(100*86400, 0) == "1970-01-01");                       // 绝对日期(UTC)
}
TEST_CASE("sortNoteRows by updated and title") {
    Note a; a.title="b"; a.updatedAt=200;
    Note b; b.title="a"; b.updatedAt=100;
    std::vector<Note> v = { a, b };
    own::sortNoteRows(v, own::NoteSortKey::Updated, 1);   // 升序
    CHECK(v[0].updatedAt == 100);
    own::sortNoteRows(v, own::NoteSortKey::Updated, -1);  // 降序
    CHECK(v[0].updatedAt == 200);
    own::sortNoteRows(v, own::NoteSortKey::Title, 1);
    CHECK(v[0].title == "a");
}
```
加入 tests 工程(`test_note_list_format.cpp` + `..\src\domain\NoteListFormat.cpp`)。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`domain/NoteListFormat.h` 不存在)。

- [ ] **Step 3: 实现**

`src/domain/NoteListFormat.h`:
```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "domain/Models.h"
namespace own {
std::string noteTitleText(const Note& n);
std::string formatRelativeTime(int64_t nowSec, int64_t thenSec);
enum class NoteSortKey { Title, Updated };
void sortNoteRows(std::vector<Note>& rows, NoteSortKey key, int order);
}
```
`src/domain/NoteListFormat.cpp`:
```cpp
#include "domain/NoteListFormat.h"
#include <algorithm>
#include <ctime>
#include <cstdio>
namespace own {
static std::string firstLine(const std::string& s, size_t maxBytes) {
    size_t nl = s.find('\n');
    std::string line = (nl == std::string::npos) ? s : s.substr(0, nl);
    if (line.size() > maxBytes) line = line.substr(0, maxBytes);
    return line;
}
std::string noteTitleText(const Note& n) {
    if (!n.title.empty()) return n.title;
    if (!n.plainText.empty()) return firstLine(n.plainText, 40);
    return "(\xE6\x97\xA0\xE6\xA0\x87\xE9\xA2\x98)";   // (无标题)
}
std::string formatRelativeTime(int64_t nowSec, int64_t thenSec) {
    int64_t d = nowSec - thenSec;
    if (d < 60) return "\xE5\x88\x9A\xE5\x88\x9A";                                  // 刚刚
    if (d < 3600) return std::to_string(d / 60) + "\xE5\x88\x86\xE9\x92\x9F\xE5\x89\x8D";   // 分钟前
    if (d < 86400) return std::to_string(d / 3600) + "\xE5\xB0\x8F\xE6\x97\xB6\xE5\x89\x8D"; // 小时前
    if (d < 7 * 86400) return std::to_string(d / 86400) + "\xE5\xA4\xA9\xE5\x89\x8D";        // 天前
    time_t t = (time_t)thenSec;
    struct tm g;
#if defined(_WIN32)
    gmtime_s(&g, &t);
#else
    g = *gmtime(&t);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);
    return std::string(buf);
}
void sortNoteRows(std::vector<Note>& rows, NoteSortKey key, int order) {
    if (order == 0) return;
    bool asc = order > 0;
    std::stable_sort(rows.begin(), rows.end(), [&](const Note& a, const Note& b) {
        if (key == NoteSortKey::Updated)
            return asc ? (a.updatedAt < b.updatedAt) : (a.updatedAt > b.updatedAt);
        std::string ta = noteTitleText(a), tb = noteTitleText(b);
        return asc ? (ta < tb) : (ta > tb);
    });
}
}
```

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -4`
Expected: 构建通过,tests 全绿(新增用例)。

- [ ] **Step 5: Commit**

```bash
git add src/domain/NoteListFormat.h src/domain/NoteListFormat.cpp tests/test_note_list_format.cpp tests/tests.vcxproj
git commit -m "feat(domain): note list formatting — title/relative-time/sort (pure)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: INoteWindowHost 接口 + CNoteApp 实现(便签窗托管)

**Files:**
- Create: `src/app/NoteWindowHost.h`
- Modify: `src/app/NoteApp.h`, `src/app/NoteApp.cpp`
- Modify: `app/open_windows_note_app.vcxproj`(登记新头)

**Interfaces:**
- Produces:
  - `class INoteWindowHost { public: virtual ~INoteWindowHost(){} virtual void openOrFocusNote(int64_t id)=0; virtual void refreshNoteWindow(int64_t id)=0; virtual void closeNoteWindow(int64_t id)=0; virtual void setAllNotesVisible(bool show)=0; };`
  - `CNoteApp : public CWinApp, public INoteWindowHost`,实现四方法(在 `m_notes` 里按 `noteId()` 查/开/关便签窗;`refresh`=先关再开;`setAllNotesVisible`=遍历 store 的 note 改 `visible` 并显隐窗口)。
- Consumes: P2 `CNoteWindow`;P1 `NoteStore::getNote/query/updateFlags`;已有 `createAndShowNote`。

- [ ] **Step 1: 写接口头**

`src/app/NoteWindowHost.h`:
```cpp
#pragma once
#include <cstdint>
class INoteWindowHost {
public:
    virtual ~INoteWindowHost() {}
    virtual void openOrFocusNote(int64_t id) = 0;
    virtual void refreshNoteWindow(int64_t id) = 0;
    virtual void closeNoteWindow(int64_t id) = 0;
    virtual void setAllNotesVisible(bool show) = 0;
};
```

- [ ] **Step 2: CNoteApp 继承接口 + 声明方法**

`src/app/NoteApp.h`:顶部 include 加 `#include "app/NoteWindowHost.h"`;类声明改为 `class CNoteApp : public CWinApp, public INoteWindowHost`;在 public 区加:
```cpp
    void openOrFocusNote(int64_t id) override;
    void refreshNoteWindow(int64_t id) override;
    void closeNoteWindow(int64_t id) override;
    void setAllNotesVisible(bool show) override;
private:
    CNoteWindow* findNote(int64_t id);   // 在 m_notes 里查；无则 nullptr
```
(`findNote` 放 private;其余四个 public override。)

- [ ] **Step 3: 实现四方法**

`src/app/NoteApp.cpp` 末尾加:
```cpp
CNoteWindow* CNoteApp::findNote(int64_t id) {
    for (auto& w : m_notes) if (w && w->noteId() == id) return w.get();
    return nullptr;
}
void CNoteApp::openOrFocusNote(int64_t id) {
    if (CNoteWindow* w = findNote(id)) {
        w->ShowWindow(SW_SHOW);
        w->SetWindowPos(&CWnd::wndTopMost, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        return;
    }
    auto full = m_store->getNote(id);
    if (full) { full->visible = true; createAndShowNote(*full); }
}
void CNoteApp::closeNoteWindow(int64_t id) {
    for (auto it = m_notes.begin(); it != m_notes.end(); ++it) {
        if (*it && (*it)->noteId() == id) {
            if ((*it)->GetSafeHwnd()) (*it)->DestroyWindow();
            m_notes.erase(it);
            return;
        }
    }
}
void CNoteApp::refreshNoteWindow(int64_t id) {
    if (findNote(id)) { closeNoteWindow(id); openOrFocusNote(id); }
}
void CNoteApp::setAllNotesVisible(bool show) {
    own::NoteQuery q; auto all = m_store->query(q);
    for (const auto& n : all) {
        m_store->updateFlags(n.id, n.opacity, n.pinned, n.rolledUp, show);
        if (show) openOrFocusNote(n.id);
        else closeNoteWindow(n.id);
    }
}
```

- [ ] **Step 4: 登记头 + 构建 + 单测**

`app/open_windows_note_app.vcxproj` 头组加 `..\src\app\NoteWindowHost.h`。
Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -3
```
Expected: 构建链接通过;tests 全绿(数量不变)。

- [ ] **Step 5: Commit**

```bash
git add src/app/NoteWindowHost.h src/app/NoteApp.h src/app/NoteApp.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(app): INoteWindowHost + CNoteApp note-window management

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: CNoteListView(表格回调适配 + 列/行渲染 + 双击打开)

**Files:**
- Create: `src/ui/NoteListView.h`, `src/ui/NoteListView.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 2 `SWTableScrollViewWnd`/`ISWTableScrollViewWndCallback`/`TABLE_VIEW_COLUMN_INFO`;Task 3 `noteTitleText/formatRelativeTime/sortNoteRows/NoteSortKey`;Task 4 `INoteWindowHost`;P1 `NoteStore`。
- Produces:
  - `class CNoteListView : public ISWTableScrollViewWndCallback`。
  - `void Attach(SWTableScrollViewWnd* table, own::NoteStore* store, INoteWindowHost* host);`
  - `void setSearch(const std::string& text);`
  - `void reload();` —— 快照 `store->query({search})` → 组名/标签/相对时间预算成行 → `setTotalRowCount`。
  - 内部 `struct Row { own::Note note; std::string title, group, tags, updated; };`
- 说明:右键菜单在 Task 8 接线(本任务先留空实现);双击打开便签窗本任务落地。

- [ ] **Step 1: 写头**

`src/ui/NoteListView.h`:
```cpp
#pragma once
#include <vector>
#include <string>
#include "ui/table/SWTableScrollViewWnd.h"
#include "domain/Models.h"
#include "domain/NoteListFormat.h"
namespace own { class NoteStore; }
class INoteWindowHost;

class CNoteListView : public ISWTableScrollViewWndCallback {
public:
    void Attach(SWTableScrollViewWnd* table, own::NoteStore* store, INoteWindowHost* host);
    void setSearch(const std::string& text);
    void reload();
    int64_t rowNoteId(int row1based) const;   // 越界返回 0

    // ISWTableScrollViewWndCallback
    void onTableScrollViewDrawCell(HDC hdc, SWTableScrollViewWnd* s, int row, int col, CRect rect, int align) override;
    void onTableScrollViewLeftMouseClick(SWTableScrollViewWnd*, int, int) override {}
    void onTableScrollViewRightMouseClick(SWTableScrollViewWnd*, int row, int col) override;
    void onTableScrollViewLeftMouseDblClick(SWTableScrollViewWnd*, int row, int col) override;
    void onTableScrollViewSortColumn(SWTableScrollViewWnd*, int col, int order) override;
    int  onTableScrollViewAutoAdjustColumnWdidth(SWTableScrollViewWnd*, int col) override;
protected:
    virtual void onContextMenu(int row) {}    // Task 8 覆盖：弹右键菜单
    struct Row { own::Note note; std::string title, group, tags, updated; };
    std::vector<Row> m_rows;
    SWTableScrollViewWnd* m_table = nullptr;
    own::NoteStore* m_store = nullptr;
    INoteWindowHost* m_host = nullptr;
    std::string m_search;
    own::NoteSortKey m_sortKey = own::NoteSortKey::Updated;
    int m_sortOrder = -1;
    std::vector<TABLE_VIEW_COLUMN_INFO*> m_cols;
};
```

- [ ] **Step 2: 实现**

`src/ui/NoteListView.cpp`:
```cpp
#include "ui/NoteListView.h"
#include "app/NoteWindowHost.h"
#include "data/NoteStore.h"
#include <ctime>
#include <map>

static uint32_t typeMarkerColor(own::NoteType t) {
    switch (t) {
        case own::NoteType::Checklist: return 0x3060E0;
        case own::NoteType::Drawing:   return 0x30A030;
        default:                       return 0xE0C020;   // 富文本=黄
    }
}
void CNoteListView::Attach(SWTableScrollViewWnd* table, own::NoteStore* store, INoteWindowHost* host) {
    m_table = table; m_store = store; m_host = host;
    m_table->setTableScrollViewCallback(this);
    // 列： [标记] 标题 分组 标签 更新
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"",   28.f, 2, 0));
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"\xE6\xA0\x87\xE9\xA2\x98", 220.f, 0, 1)); // 标题
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"\xE5\x88\x86\xE7\xBB\x84",  90.f, 0, 0)); // 分组
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"\xE6\xA0\x87\xE7\xAD\xBE", 120.f, 0, 0)); // 标签
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"\xE6\x9B\xB4\xE6\x96\xB0", 120.f, 0, 1)); // 更新
    m_table->setColumnInfos("note_list", m_cols);
    reload();
}
void CNoteListView::setSearch(const std::string& text) { m_search = text; reload(); }
void CNoteListView::reload() {
    if (!m_store) return;
    own::NoteQuery q; q.search = m_search;
    auto notes = m_store->query(q);
    own::sortNoteRows(notes, m_sortKey, m_sortOrder);
    std::map<int64_t, std::string> gname;
    for (const auto& g : m_store->allGroups()) gname[g.id] = g.name;
    int64_t now = (int64_t)time(nullptr);
    m_rows.clear();
    for (const auto& n : notes) {
        Row r; r.note = n;
        r.title = own::noteTitleText(n);
        auto it = gname.find(n.groupId);
        r.group = (n.groupId != 0 && it != gname.end()) ? it->second : "";
        std::string tags;
        for (const auto& t : m_store->tagsOfNote(n.id)) { if (!tags.empty()) tags += ","; tags += t.name; }
        r.tags = tags;
        r.updated = own::formatRelativeTime(now, n.updatedAt);
        m_rows.push_back(std::move(r));
    }
    if (m_table) { m_table->setTotalRowCount((int)m_rows.size()); m_table->Invalidate(FALSE); }
}
int64_t CNoteListView::rowNoteId(int row) const {
    if (row >= 1 && row <= (int)m_rows.size()) return m_rows[row-1].note.id;
    return 0;
}
void CNoteListView::onTableScrollViewDrawCell(HDC hdc, SWTableScrollViewWnd*, int row, int col, CRect rect, int) {
    if (row < 1 || row > (int)m_rows.size()) return;
    const Row& r = m_rows[row-1];
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, RGB(0xE0,0xE0,0xE0));
    if (col == 0) {                                   // 类型色块
        uint32_t c = typeMarkerColor(r.note.type);
        CRect sw(rect.left + rect.Width()/2 - 5, rect.top + rect.Height()/2 - 5,
                 rect.left + rect.Width()/2 + 5, rect.top + rect.Height()/2 + 5);
        HBRUSH b = ::CreateSolidBrush(RGB((c>>16)&0xFF,(c>>8)&0xFF,c&0xFF));
        RECT rr = sw; ::FillRect(hdc, &rr, b); ::DeleteObject(b);
        return;
    }
    const std::string* txt = nullptr;
    if (col == 1) txt = &r.title; else if (col == 2) txt = &r.group;
    else if (col == 3) txt = &r.tags; else if (col == 4) txt = &r.updated;
    if (!txt) return;
    CRect tr(rect.left + 6, rect.top, rect.right - 4, rect.bottom);
    ::DrawTextA(hdc, txt->c_str(), (int)txt->size(), tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}
void CNoteListView::onTableScrollViewLeftMouseDblClick(SWTableScrollViewWnd*, int row, int) {
    int64_t id = rowNoteId(row);
    if (id && m_host) m_host->openOrFocusNote(id);
}
void CNoteListView::onTableScrollViewRightMouseClick(SWTableScrollViewWnd*, int row, int) {
    if (rowNoteId(row)) onContextMenu(row);
}
void CNoteListView::onTableScrollViewSortColumn(SWTableScrollViewWnd*, int col, int order) {
    if (col == 1) m_sortKey = own::NoteSortKey::Title;
    else if (col == 4) m_sortKey = own::NoteSortKey::Updated;
    else return;
    m_sortOrder = order; reload();
}
int CNoteListView::onTableScrollViewAutoAdjustColumnWdidth(SWTableScrollViewWnd*, int) { return 0; }
```
> 说明:`m_cols` 里 `new` 的列信息按控件约定「外部只 new 不 delete」由控件生命周期持有;进程退出即回收,符合 P4 单管理器窗口的用法。

- [ ] **Step 3: 登记 + 构建 + 单测**

`app/open_windows_note_app.vcxproj` 源组加 `..\src\ui\NoteListView.cpp`;头组加 `..\src\ui\NoteListView.h`。
Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -3
```
Expected: 构建链接通过;tests 全绿。

- [ ] **Step 4: Commit**

```bash
git add src/ui/NoteListView.h src/ui/NoteListView.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui): CNoteListView table adapter — columns/render/dblclick-open

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: CSearchBox(自绘搜索框 + 内嵌小 CEdit)

**Files:**
- Create: `src/ui/SearchBox.h`, `src/ui/SearchBox.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Produces:
  - `class CSearchBox : public CWnd`,自绘边框 + 内嵌一个填充式 `CEdit`;文本变化时回调 `std::function<void(const std::string&)> onChanged;`
  - `bool Create(CWnd* parent, const CRect& rc);` / `void Reposition(const CRect& rc);`
- Consumes: 无(纯 UI)。

- [ ] **Step 1: 写头**

`src/ui/SearchBox.h`:
```cpp
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
```

- [ ] **Step 2: 实现**

`src/ui/SearchBox.cpp`:
```cpp
#include "ui/SearchBox.h"
static const UINT kEditId = 0x3101;
BEGIN_MESSAGE_MAP(CSearchBox, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_EN_CHANGE(kEditId, OnEditChanged)
END_MESSAGE_MAP()
bool CSearchBox::Create(CWnd* parent, const CRect& rc) {
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_WINDOW+1));
    if (!CWnd::CreateEx(0, cls, _T("SearchBox"), WS_CHILD | WS_VISIBLE, rc, parent, 0x3100))
        return false;
    CRect e(4, 3, rc.Width() - 4, rc.Height() - 3);
    m_edit.Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, e, this, kEditId);
    return true;
}
void CSearchBox::Reposition(const CRect& rc) {
    MoveWindow(rc);
    if (m_edit.GetSafeHwnd()) m_edit.MoveWindow(4, 3, rc.Width() - 8, rc.Height() - 6);
}
BOOL CSearchBox::OnEraseBkgnd(CDC*) { return TRUE; }
void CSearchBox::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    dc.FillSolidRect(rc, RGB(0xFF,0xFF,0xFF));
    CBrush border(RGB(0xB0,0xB0,0xB0)); dc.FrameRect(rc, &border);
}
void CSearchBox::OnEditChanged() {
    CString w; m_edit.GetWindowText(w);
    CStringA a(w);
    if (onChanged) onChanged(std::string((LPCSTR)a));
}
```

- [ ] **Step 3: 登记 + 构建 + 单测**

`app/open_windows_note_app.vcxproj` 源组加 `..\src\ui\SearchBox.cpp`;头组加 `..\src\ui\SearchBox.h`。
Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建链接通过;tests 全绿。

- [ ] **Step 4: Commit**

```bash
git add src/ui/SearchBox.h src/ui/SearchBox.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui): self-drawn CSearchBox with embedded edit

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: CMainFrame(管理器窗口:宿主搜索框 + 表格)

**Files:**
- Create: `src/app/MainFrame.h`, `src/app/MainFrame.cpp`
- Modify: `src/app/NoteApp.h`, `src/app/NoteApp.cpp`(建/显管理器)
- Modify: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`(`Ctrl+Alt+M`)
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 5 `CNoteListView`;Task 6 `CSearchBox`;Task 2 `SWTableScrollViewWnd`;Task 4 `INoteWindowHost`;P1 `NoteStore`。
- Produces:
  - `class CMainFrame : public CFrameWnd`。
  - `bool Create(own::NoteStore* store, INoteWindowHost* host);`
  - `void ToggleShow();`(隐↔显)、`void reloadList();`
  - 布局:顶部 `CSearchBox`(高 28),其下 `SWTableScrollViewWnd` 填满;`WM_SIZE` 重排;`WM_CLOSE` → 隐藏(不销毁、不退出)。
- `CNoteApp` 持 `std::unique_ptr<CMainFrame> m_main;`,在 DB 引导后建管理器并 `ShowWindow`;`m_host.onQuit` 仍退出;新增 `Ctrl+Alt+M` → `m_main->ToggleShow()`。

- [ ] **Step 1: 写头**

`src/app/MainFrame.h`:
```cpp
#pragma once
#include <afxwin.h>
#include <memory>
#include "ui/table/SWTableScrollViewWnd.h"
#include "ui/NoteListView.h"
#include "ui/SearchBox.h"
namespace own { class NoteStore; }
class INoteWindowHost;
class CMainFrame : public CFrameWnd {
public:
    bool Create(own::NoteStore* store, INoteWindowHost* host);
    void ToggleShow();
    void reloadList();
protected:
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()
private:
    void layout();
    own::NoteStore* m_store = nullptr;
    INoteWindowHost* m_host = nullptr;
    SWTableScrollViewWnd m_table;
    CNoteListView m_list;
    CSearchBox m_search;
};
```

- [ ] **Step 2: 实现**

`src/app/MainFrame.cpp`:
```cpp
#include "app/MainFrame.h"
#include "data/NoteStore.h"
static const int kSearchH = 28;
BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_SIZE()
    ON_WM_CLOSE()
END_MESSAGE_MAP()
bool CMainFrame::Create(own::NoteStore* store, INoteWindowHost* host) {
    m_store = store; m_host = host;
    if (!CFrameWnd::Create(nullptr, _T("open_windows_note"), WS_OVERLAPPEDWINDOW,
                           CRect(200, 200, 200 + 640, 200 + 480)))
        return false;
    CRect rc; GetClientRect(&rc);
    m_search.Create(this, CRect(0, 0, rc.Width(), kSearchH));
    m_search.onChanged = [this](const std::string& s){ m_list.setSearch(s); };
    m_table.Create(nullptr, _T("table"), WS_CHILD | WS_VISIBLE, CRect(0, kSearchH, rc.Width(), rc.Height()), this, 0x3200);
    m_list.Attach(&m_table, m_store, m_host);
    layout();
    return true;
}
void CMainFrame::layout() {
    CRect rc; GetClientRect(&rc);
    m_search.Reposition(CRect(0, 0, rc.Width(), kSearchH));
    if (m_table.GetSafeHwnd()) m_table.MoveWindow(0, kSearchH, rc.Width(), rc.Height() - kSearchH);
}
void CMainFrame::OnSize(UINT t, int cx, int cy) { CFrameWnd::OnSize(t, cx, cy); layout(); }
void CMainFrame::OnClose() { ShowWindow(SW_HIDE); }   // 关闭=隐藏，不退出（退出走 Ctrl+Alt+Q）
void CMainFrame::ToggleShow() {
    if (IsWindowVisible()) ShowWindow(SW_HIDE);
    else { reloadList(); ShowWindow(SW_SHOW); SetForegroundWindow(); }
}
void CMainFrame::reloadList() { m_list.reload(); }
```

- [ ] **Step 3: CNoteApp 建管理器 + Ctrl+Alt+M**

`src/app/NoteApp.h`:include 加 `#include "app/MainFrame.h"`;私有成员加 `std::unique_ptr<CMainFrame> m_main;`。
`src/app/NoteApp.cpp`:在 `InitInstance` 中 `m_store` 构造之后、显示既有 note 之前,加:
```cpp
    m_main = std::make_unique<CMainFrame>();
    m_main->Create(m_store.get(), this);
    m_main->ShowWindow(SW_SHOW);
    m_host.onToggleManager = [this]{ if (m_main) m_main->ToggleShow(); };
```
`src/app/AppHostWindow.h`:加 `static const UINT kHotkeyManager = 5;` 与 `std::function<void()> onToggleManager;`。
`src/app/AppHostWindow.cpp`:`Create()` 加 `::RegisterHotKey(m_hWnd, kHotkeyManager, MOD_CONTROL|MOD_ALT, 'M');`;`OnHotKey` 加 `else if (idHotKey == kHotkeyManager) { if (onToggleManager) onToggleManager(); }`;`OnDestroy` 加 `::UnregisterHotKey(m_hWnd, kHotkeyManager);`。

- [ ] **Step 4: 登记 + 构建 + 单测**

`app/open_windows_note_app.vcxproj` 源组加 `..\src\app\MainFrame.cpp`;头组加 `..\src\app\MainFrame.h`。
Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -8 && ./x64/Debug/tests.exe 2>&1 | tail -3
```
Expected: 构建链接通过;tests 全绿。

- [ ] **Step 5: 手工冒烟(记录)** — 启动出现管理器窗口,列出既有便签;搜索框输入过滤;双击一行打开便签窗;`Ctrl+Alt+M` 切换管理器显隐;关闭管理器不退出程序。

- [ ] **Step 6: Commit**

```bash
git add src/app/MainFrame.h src/app/MainFrame.cpp src/app/NoteApp.h src/app/NoteApp.cpp src/app/AppHostWindow.h src/app/AppHostWindow.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(app): CMainFrame manager window hosting search + note table

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: 右键菜单 — 打开/删除/隐藏该便签

**Files:**
- Modify: `src/ui/NoteListView.h`, `src/ui/NoteListView.cpp`
- Modify: `src/app/MainFrame.cpp`(把 list 的刷新与 host 连起来,已在 Task 7 完成 Attach;本任务在 list 内实现菜单)

**Interfaces:**
- Consumes: Task 5 `CNoteListView`(`onContextMenu` 虚方法、`rowNoteId`);P1 `NoteStore::deleteNote/getNote/updateFlags`;Task 4 `INoteWindowHost::openOrFocusNote/closeNoteWindow`。
- Produces: `CNoteListView::onContextMenu(int row)` 覆盖为弹出 `CMenu`:打开 / 删除 / 隐藏该便签;命令用 `TrackPopupMenu(TPM_RETURNCMD)` 就地处理,操作后 `reload()`。

- [ ] **Step 1: 声明覆盖 onContextMenu**

`src/ui/NoteListView.h`:把 `protected: virtual void onContextMenu(int row) {}` 改为 `void onContextMenu(int row) override;` 并去掉空体(改为在 .cpp 实现)。同时在头顶部无需新增 include(`<afxwin.h>` 经 table 头传入)。

- [ ] **Step 2: 实现菜单**

`src/ui/NoteListView.cpp` 末尾加:
```cpp
void CNoteListView::onContextMenu(int row) {
    int64_t id = rowNoteId(row);
    if (!id || !m_store) return;
    CMenu menu; menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 1, _T("\x6253\x5F00"));            // 打开
    menu.AppendMenu(MF_STRING, 2, _T("\x9690\x85CF\x8BE5\x4FBF\x7B7E")); // 隐藏该便签
    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    menu.AppendMenu(MF_STRING, 3, _T("\x5220\x9664"));            // 删除
    CPoint pt; ::GetCursorPos(&pt);
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, CWnd::FromHandle(m_table->GetSafeHwnd()));
    if (cmd == 1) { if (m_host) m_host->openOrFocusNote(id); }
    else if (cmd == 2) {
        auto n = m_store->getNote(id);
        if (n) { m_store->updateFlags(id, n->opacity, n->pinned, n->rolledUp, false);
                 if (m_host) m_host->closeNoteWindow(id); reload(); }
    }
    else if (cmd == 3) {
        if (m_host) m_host->closeNoteWindow(id);
        m_store->deleteNote(id);
        reload();
    }
}
```
> 注:菜单项文字用宽字符转义(`\x6253\x5F00`=打开 等),避免 `CMenu::AppendMenu` 的窄/宽字符歧义。

- [ ] **Step 3: 构建 + 单测**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建链接通过;tests 全绿。

- [ ] **Step 4: 手工冒烟(记录)** — 右键一行:打开=弹便签窗;隐藏=便签窗关闭且重启后不自动出现;删除=行消失且数据删除。

- [ ] **Step 5: Commit**

```bash
git add src/ui/NoteListView.h src/ui/NoteListView.cpp
git commit -m "feat(ui): note list right-click menu — open/hide/delete

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: TextPrompt(新建分组/标签的极简模态输入)

**Files:**
- Create: `src/ui/TextPrompt.h`, `src/ui/TextPrompt.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Produces: `bool own_ui::promptText(CWnd* parent, const CString& title, CString& io);` —— 弹一个居中的置顶小窗(一个 `CEdit` + 提示),回车确认返回 true 并把文本写回 `io`,ESC/关闭返回 false。用局部模态消息循环实现(不依赖资源模板)。
- Consumes: 无。

- [ ] **Step 1: 写头**

`src/ui/TextPrompt.h`:
```cpp
#pragma once
#include <afxwin.h>
namespace own_ui {
bool promptText(CWnd* parent, const CString& title, CString& io);
}
```

- [ ] **Step 2: 实现**

`src/ui/TextPrompt.cpp`:
```cpp
#include "ui/TextPrompt.h"
namespace own_ui {
class CPromptWnd : public CWnd {
public:
    CEdit m_edit; bool m_done = false; bool m_ok = false;
    afx_msg void OnKeyDownForward() {}
    virtual BOOL PreTranslateMessage(MSG* pMsg) override {
        if (pMsg->message == WM_KEYDOWN) {
            if (pMsg->wParam == VK_RETURN) { m_ok = true; m_done = true; return TRUE; }
            if (pMsg->wParam == VK_ESCAPE) { m_ok = false; m_done = true; return TRUE; }
        }
        return CWnd::PreTranslateMessage(pMsg);
    }
};
bool promptText(CWnd* parent, const CString& title, CString& io) {
    CPromptWnd w;
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE+1));
    CRect r(0,0,300,90);
    if (parent && parent->GetSafeHwnd()) { CRect pr; parent->GetWindowRect(&pr);
        r.OffsetRect(pr.left + (pr.Width()-300)/2, pr.top + (pr.Height()-90)/2); }
    else r.OffsetRect(400, 300);
    w.CreateEx(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, cls, title,
               WS_POPUP | WS_CAPTION | WS_VISIBLE, r, parent, 0);
    w.m_edit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                    CRect(12, 12, 288, 40), &w, 0x3301);
    w.m_edit.SetWindowText(io);
    w.m_edit.SetFocus();
    w.m_edit.SetSel(0, -1);
    if (parent) parent->EnableWindow(FALSE);
    MSG msg;
    while (!w.m_done && ::GetMessage(&msg, nullptr, 0, 0)) {
        if (!w.PreTranslateMessage(&msg)) { ::TranslateMessage(&msg); ::DispatchMessage(&msg); }
    }
    if (parent) parent->EnableWindow(TRUE);
    if (w.m_ok) w.m_edit.GetWindowText(io);
    w.DestroyWindow();
    return w.m_ok && !io.IsEmpty();
}
}
```

- [ ] **Step 3: 登记 + 构建 + 单测**

`app/open_windows_note_app.vcxproj` 源组加 `..\src\ui\TextPrompt.cpp`;头组加 `..\src\ui\TextPrompt.h`。
Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建链接通过;tests 全绿。

- [ ] **Step 4: Commit**

```bash
git add src/ui/TextPrompt.h src/ui/TextPrompt.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui): promptText minimal modal text input

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 10: 右键子菜单 — 改分组 / 加标签(含新建)

**Files:**
- Modify: `src/ui/NoteListView.cpp`

**Interfaces:**
- Consumes: Task 9 `own_ui::promptText`;P1 `NoteStore::allGroups/upsertGroup/updateNote/allTags/upsertTag/addTagToNote/getNote`。
- Produces: 扩展 `onContextMenu`:加「改分组 ▶」(无分组 / 现有分组 / 新建分组…)与「加标签 ▶」(现有标签 / 新建标签…)。改分组=改 `note.groupId` 后 `updateNote`;加标签=`upsertTag`+`addTagToNote`;新建用 `promptText`。操作后 `reload()`。

- [ ] **Step 1: 扩展菜单构建与命令处理**

`src/ui/NoteListView.cpp`:在文件顶部加 `#include "ui/TextPrompt.h"`。把 `onContextMenu` 改为:
```cpp
void CNoteListView::onContextMenu(int row) {
    int64_t id = rowNoteId(row);
    if (!id || !m_store) return;
    auto note = m_store->getNote(id);
    if (!note) return;

    CMenu menu; menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 1, _T("\x6253\x5F00"));            // 打开
    menu.AppendMenu(MF_STRING, 2, _T("\x9690\x85CF\x8BE5\x4FBF\x7B7E")); // 隐藏该便签

    // 改分组 子菜单：无分组=100，现有=200+i，新建=199
    CMenu grp; grp.CreatePopupMenu();
    grp.AppendMenu(MF_STRING, 100, _T("\x65E0\x5206\x7EC4"));     // 无分组
    auto groups = m_store->allGroups();
    for (size_t i = 0; i < groups.size(); ++i)
        grp.AppendMenu(MF_STRING, 200 + (UINT)i, CString(groups[i].name.c_str()));
    grp.AppendMenu(MF_SEPARATOR, 0, _T(""));
    grp.AppendMenu(MF_STRING, 199, _T("\x65B0\x5EFA\x5206\x7EC4\x2026")); // 新建分组…
    menu.AppendMenu(MF_POPUP, (UINT_PTR)grp.GetSafeHmenu(), _T("\x6539\x5206\x7EC4")); // 改分组

    // 加标签 子菜单：现有=300+i，新建=299
    CMenu tag; tag.CreatePopupMenu();
    auto tags = m_store->allTags();
    for (size_t i = 0; i < tags.size(); ++i)
        tag.AppendMenu(MF_STRING, 300 + (UINT)i, CString(tags[i].name.c_str()));
    if (!tags.empty()) tag.AppendMenu(MF_SEPARATOR, 0, _T(""));
    tag.AppendMenu(MF_STRING, 299, _T("\x65B0\x5EFA\x6807\x7B7E\x2026")); // 新建标签…
    menu.AppendMenu(MF_POPUP, (UINT_PTR)tag.GetSafeHmenu(), _T("\x52A0\x6807\x7B7E")); // 加标签

    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    menu.AppendMenu(MF_STRING, 3, _T("\x5220\x9664"));            // 删除

    CPoint pt; ::GetCursorPos(&pt);
    CWnd* owner = CWnd::FromHandle(m_table->GetSafeHwnd());
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, owner);

    if (cmd == 1) { if (m_host) m_host->openOrFocusNote(id); }
    else if (cmd == 2) {
        m_store->updateFlags(id, note->opacity, note->pinned, note->rolledUp, false);
        if (m_host) m_host->closeNoteWindow(id); reload();
    }
    else if (cmd == 3) { if (m_host) m_host->closeNoteWindow(id); m_store->deleteNote(id); reload(); }
    else if (cmd == 100) { note->groupId = 0; m_store->updateNote(*note, note->updatedAt); reload(); }
    else if (cmd >= 200 && cmd < 299) {
        size_t i = (size_t)(cmd - 200);
        if (i < groups.size()) { note->groupId = groups[i].id; m_store->updateNote(*note, note->updatedAt); reload(); }
    }
    else if (cmd == 199) {
        CString name; if (own_ui::promptText(owner, _T("\x65B0\x5EFA\x5206\x7EC4"), name)) {
            own::Group g; g.name = std::string((LPCSTR)CStringA(name));
            int64_t gid = m_store->upsertGroup(g);
            note->groupId = gid; m_store->updateNote(*note, note->updatedAt); reload();
        }
    }
    else if (cmd >= 300 && cmd < 399) {
        size_t i = (size_t)(cmd - 300);
        if (i < tags.size()) { m_store->addTagToNote(id, tags[i].id); reload(); }
    }
    else if (cmd == 299) {
        CString name; if (own_ui::promptText(owner, _T("\x65B0\x5EFA\x6807\x7B7E"), name)) {
            int64_t tid = m_store->upsertTag(std::string((LPCSTR)CStringA(name)));
            m_store->addTagToNote(id, tid); reload();
        }
    }
}
```
> 说明:改分组把 `note.groupId` 写回并 `updateNote`(用原 `updatedAt` 保持时间不跳);加标签走多对多 `note_tags`。新建分组/标签用 `promptText`。命令号区间不重叠(1–3 基础、100/199/200+ 分组、299/300+ 标签)。

- [ ] **Step 2: 构建 + 单测**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建链接通过;tests 全绿。

- [ ] **Step 3: 手工冒烟(记录)** — 右键 改分组→选/新建分组,分组列更新;右键 加标签→选/新建标签,标签列更新;重启后保留。

- [ ] **Step 4: Commit**

```bash
git add src/ui/NoteListView.cpp
git commit -m "feat(ui): right-click submenus for group/tag assignment (with create)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 11: 显隐全部 + 列表随便签变化刷新

**Files:**
- Modify: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`(`Ctrl+Alt+H`)
- Modify: `src/app/NoteApp.cpp`(接线 `onToggleAll` + 新建 note 后刷新列表)
- Modify: `src/app/MainFrame.h`, `src/app/MainFrame.cpp`(暴露 `reloadList` 给 app,已有)

**Interfaces:**
- Consumes: Task 4 `INoteWindowHost::setAllNotesVisible`;Task 7 `CMainFrame::reloadList`。
- Produces:`Ctrl+Alt+H` 在「全部显示 / 全部隐藏」间切换(用一个 app 层 bool 记忆);新建 note(`Ctrl+Alt+N/2/3`)后调用 `m_main->reloadList()` 让管理器列表即时出现新行。

- [ ] **Step 1: 加 Ctrl+Alt+H 热键**

`src/app/AppHostWindow.h`:加 `static const UINT kHotkeyToggleAll = 6;` 与 `std::function<void()> onToggleAll;`。
`src/app/AppHostWindow.cpp`:`Create()` 加 `::RegisterHotKey(m_hWnd, kHotkeyToggleAll, MOD_CONTROL|MOD_ALT, 'H');`;`OnHotKey` 加 `else if (idHotKey == kHotkeyToggleAll) { if (onToggleAll) onToggleAll(); }`;`OnDestroy` 加 `::UnregisterHotKey(m_hWnd, kHotkeyToggleAll);`。

- [ ] **Step 2: NoteApp 接线**

`src/app/NoteApp.h`:私有加 `bool m_allShown = true;`。
`src/app/NoteApp.cpp`:装配回调处加:
```cpp
    m_host.onToggleAll = [this]{
        m_allShown = !m_allShown;
        setAllNotesVisible(m_allShown);
        if (m_main) m_main->reloadList();
    };
```
并把三个新建回调(`onNewNote`/`onNewChecklist`/`onNewDrawing`)各自末尾加一行 `if (m_main) m_main->reloadList();`(新建后列表刷新)。

- [ ] **Step 3: 构建 + 单测**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建链接通过;tests 全绿。

- [ ] **Step 4: 手工冒烟(记录)** — `Ctrl+Alt+H` 隐藏全部便签窗、再按恢复;`Ctrl+Alt+N/2/3` 新建后管理器列表立刻多一行。

- [ ] **Step 5: Commit**

```bash
git add src/app/AppHostWindow.h src/app/AppHostWindow.cpp src/app/NoteApp.h src/app/NoteApp.cpp
git commit -m "feat(app): Ctrl+Alt+H show/hide-all + live list refresh on new note

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 12: P4 手工冒烟清单 + 收尾文档

**Files:**
- Create: `docs/superpowers/smoke/P4-smoke-checklist.md`

**Interfaces:** 无代码;记录人工验证步骤。

- [ ] **Step 1: 写冒烟清单**

`docs/superpowers/smoke/P4-smoke-checklist.md`:
```markdown
# P4 手工冒烟清单（管理器 + 列表 + 搜索 + 分组标签）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

## 管理器与列表
- [ ] 启动出现管理器窗口，列出全部便签（标记色块/标题/分组/标签/更新时间列）
- [ ] 列表随便签数量增减刷新（Ctrl+Alt+N/2/3 新建后即时多一行）
- [ ] 点标题/更新列头排序（升/降切换），行顺序改变
- [ ] 双击一行：打开或聚焦对应便签窗
- [ ] Ctrl+Alt+M：切换管理器显隐；关闭管理器窗口不退出程序（Ctrl+Alt+Q 才退出）

## 搜索
- [ ] 搜索框输入关键字：列表按 plain_text 过滤；清空恢复全部

## 右键
- [ ] 右键行 → 打开：弹便签窗
- [ ] 右键行 → 隐藏该便签：便签窗关闭，重启后不自动出现
- [ ] 右键行 → 删除：行消失、数据删除、若开着的窗口一并关闭
- [ ] 右键 → 改分组 → 选现有/新建分组：分组列更新，重启保留
- [ ] 右键 → 加标签 → 选现有/新建标签：标签列更新，重启保留

## 显隐全部
- [ ] Ctrl+Alt+H：隐藏全部便签窗；再按恢复

## 表格控件（移植回归）
- [ ] 竖向滚动条可拖动、滚轮滚动；行悬停高亮、斑马纹正常
- [ ] 列分隔线可拖动改列宽（列宽不持久化，重启回默认——已知）
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/smoke/P4-smoke-checklist.md
git commit -m "docs: P4 manager manual smoke checklist

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage(对照设计文档):**
- §2.1/§3 `CMainFrame`(管理窗口容器、总调度)→ Task 7;`CNoteListView` **复用 `SWTableScrollViewWnd`** 并实现 `ISWTableScrollViewWndCallback`(`drawCell` 自绘每行色块/标题/分组/标签/更新时间、`PrepareDraw` 快照、双击开 note、右键菜单)→ Task 2/5/8/10。✓
- §3 `CSearchBox`(自绘搜索框内嵌小 Edit)→ Task 6;跨 note 搜索(`plain_text LIKE`)→ Task 5 `reload` 用 `NoteQuery.search`。✓
- §3 分组/标签:`NoteStore.upsertGroup/allGroups/upsertTag/allTags/addTagToNote`(P1)经右键接入 → Task 10。✓
- §5 数据流 4「setTotalRowCount(筛选后数量);PrepareDraw 快照;drawCell 画列;双击打开;右键 删除/改分组/加标签/…」→ Task 5/8/10。✓
- §5 数据流「一键隐藏/显示全部」→ Task 11。✓
- §6 错误处理「note 跑屏外钳制」由 P2 承担;本计划新增窗口(管理器)用标准边框由系统管理位置,无需钳制。✓
- §1.2「主管理窗 + N 悬浮窗共存;从列表双击打开便签窗」→ Task 4/5/7。✓
- §7 测试:纯逻辑(标题派生/相对时间/排序)doctest → Task 3;表格/窗口行为手工冒烟 → Task 12。✓
- §1.1「一律自绘;列表禁用 CListCtrl」→ 用移入的自绘表格,无 CListCtrl。✓(管理器 chrome 用系统边框,见开头取舍 1,已标注)

**范围外(明确声明):** 托盘常驻/正式全局热键(P5,本计划用临时热键 M/H);贴到应用窗口(P5);提醒调度(P5);导入导出(P5);按分组/标签**过滤**列表的侧栏 UI(P4 只做赋值 + 文本搜索,见取舍 2);列宽持久化(去 JsonCpp 后暂不持久化,Task 12 已在清单标注「已知」);管理器全自绘 chrome(取舍 1)。

**2. Placeholder scan:** 无 TBD/TODO;每个代码步骤给出可编译代码或精确到 call site 的移植改动(Task 2 Step 5–7 列出确切替换)。移植控件不重写 2200 行,而是「复制 + 指定 include/no-op/BCGP 替换」,改动全部展开。

**3. Type consistency:**
- `ISWTableScrollViewWndCallback` 方法签名与移入 `.h` 一致(`onTableScrollViewDrawCell(HDC,SWTableScrollViewWnd*,int,int,CRect,int)` 等)——Task 5 覆盖签名逐一对齐。✓
- `TABLE_VIEW_COLUMN_INFO(char*,float,int,int)` 构造(defs.h)——Task 5 `new` 用四参构造。✓
- `INoteWindowHost` 四方法(Task 4)在 Task 5(dblclick)、Task 8(open/close)、Task 10(open/close)、Task 11(setAllNotesVisible)一致调用。✓
- `CNoteListView::reload/setSearch/rowNoteId/onContextMenu`(Task 5)在 Task 7/8/10 一致使用;`onContextMenu` 由 Task 5 声明虚、Task 8 override、Task 10 扩展。✓
- `noteTitleText/formatRelativeTime/sortNoteRows/NoteSortKey`(Task 3)在 Task 5 一致调用。✓
- `own_ui::promptText(CWnd*,const CString&,CString&)`(Task 9)在 Task 10 一致调用。✓
- `CMainFrame::Create(NoteStore*,INoteWindowHost*)/ToggleShow/reloadList`(Task 7)在 NoteApp(Task 7/11)一致调用。✓
- 热键号不冲突:`kHotkeyQuit=1/kHotkeyNew=2/kHotkeyNewChecklist=3/kHotkeyNewDrawing=4`(P2/P3)+ `kHotkeyManager=5`(Task 7)+ `kHotkeyToggleAll=6`(Task 11)。✓
- 菜单命令号区间不重叠:1–3 基础;100/199/200+ 改分组;299/300+ 加标签(Task 8/10)。✓

**已知限制(执行者须知):** 无 GUI 会话时表格/窗口交互无法自动化;GUI 任务以「app 链接通过 + tests 全绿」为自动化达标线,行为落 Task 12 手工清单。Task 2 的移植若遇未列出的隐藏依赖(如某工具函数),按同样思路加垫片或改 no-op,并在提交信息注明。
