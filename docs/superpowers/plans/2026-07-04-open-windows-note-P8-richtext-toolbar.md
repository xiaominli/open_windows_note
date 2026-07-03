# P8 富文本工具条 + 字体字号 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 文本便签获得格式工具条（加粗/斜体/下划线/删除线/字号增减/文字色循环，作用于选区），设置弹层新增「默认字号」，并让字符级格式在重开与换主题后存活。

**Architecture:** 纯布局层新增 `FormatBarLayout`（按钮矩形 + 命中测试 + 字号阶梯/调色板循环纯函数，全部 doctest）。`INoteContentView` 加默认空实现 `ApplyFormat(FmtOp)`，仅 `CTextContentView` 实现（RichEdit `EM_SETCHARFORMAT SCF_SELECTION`）。`CNoteWindow` 在标题栏下方为 RichText 便签画一条工具条（GDI+ 自绘，复用主题色），内容区相应下移。**格式保真前提**（本计划的关键行为变更）：`Load()` 的整篇字体归一从 FACE|SIZE|CHARSET 收窄为 FACE|CHARSET（保住用户改的字号）；`ApplyTheme` 的文字色从 `SCF_ALL` 改为 `SCF_DEFAULT`（保住用户改的文字色；4 套内置主题 textColor 全部相同 0x202020，无视觉回归）。默认字号经 `CTextContentView::SetDefaultFontPt` 进程级静态生效于 SCF_DEFAULT，启动时与设置修改时各设一次。

**Tech Stack:** C++17 · MFC 静态链接 · GDI/GDI+ · RichEdit (RICHEDIT50W, CHARFORMAT2W) · SQLite settings 表 · doctest。

## Global Constraints

- 语言/工具链：C++17（`/std:c++17`）、MFC **静态链接**、无 PCH、仅 `x64`。
- 编码：ClCompile 全部 `/utf-8`；中文 UI 字面量在 `_T("")` 里用 `\xXXXX` 转义 + 行尾中文注释；**测试断言只用 ASCII**。
- 命名空间：`src/domain`、纯布局 `src/ui/*Layout.*` 一律 `namespace own` 且**不得** include `<afxwin.h>`/`<windows.h>`（进 tests 工程）。
- 构建：只能通过 `.sln`。MSBuild 路径 `"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`（下称 `$MSB`），参数 `-p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo`。
- **每次重建前先杀残留**：`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`（LNK1168）。
- 自动化达标线：纯逻辑任务=`./x64/Debug/tests.exe` 全绿；GUI 任务=链接通过 + 启动存活 3 秒；视觉/交互落 Task 5 手工冒烟。
- 每次提交末尾附：`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。
- 分支：直接在 `main` 上开发。当前基线：**87 用例 / 363 断言全绿**（P7 完成后）。
- RichEdit 字号单位是 twip（1pt = 20 twip）；`yHeight = pt * 20`。
- 主题色/调色板色值 `0xRRGGBB`；转 `COLORREF` 必须换通道：`RGB((c>>16)&0xFF,(c>>8)&0xFF,c&0xFF)`；GDI+ `Color(255,(c>>16)&0xFF,(c>>8)&0xFF,c&0xFF)`。

**承接的既有接口（勿重复实现）：**
- `own::RectI`（`src/domain/Models.h`）；`own::noteContentRect(RectI client, int titleHeight, int resizeMargin)`（`src/ui/ContentLayout.h`）——`titleHeight` 是内容区顶部偏移，把工具条高度**加进去**即可下移内容。
- `INoteContentView`（`src/ui/INoteContentView.h`）：P7 已有 `virtual void ApplyTheme(uint32_t,uint32_t) {}` 默认空实现——`ApplyFormat` 照此模式。
- `CTextContentView`（`src/ui/TextContentView.h/.cpp`）：`m_edit`(CRichEditCtrl, RICHEDIT50W)、`m_created`、匿名 namespace 里 `applyNoteFont(CRichEditCtrl&, WPARAM scope)`（mask FACE|SIZE|CHARSET，yHeight=200，Create 时 SCF_DEFAULT / Load 后 SCF_ALL）。
- `CNoteWindow`（`src/ui/NoteWindow.cpp`）：`kTitleMetrics{22,16,4,4}`；`Create` 里 `noteContentRect(...,kTitleMetrics.height, 6)`；`layoutContent()` 同式；`OnPaint` 双缓冲 GDI+，主题色已接入（`m_theme`）；`OnLButtonDown` 先测 resize edge 再 `hitTestTitleBar`，`default: break;` 兜底；`m_note.type`（`own::NoteType::RichText/Checklist/Drawing`）。
- 设置弹层（`src/ui/SettingsDialog.cpp`）：`CSettingsWnd` 行模型 `rowCount()=3+热键数`、`rowLabel(i)`/`clickRow(i)` 按索引分支（0=默认主题 1=默认透明度 2=开机自启 3+=热键）；窗口高度在 CreateEx 前由 `rowCount()` 算出，行数增加自动变高。
- `own::SettingsStore`（`src/data/SettingsStore.h`）：`getInt/setInt`。
- `CNoteApp::InitInstance`（`src/app/NoteApp.cpp`）：settings 读取已有先例（`own::SettingsStore st(m_db)`）。

**语义决策：**
- 工具条只对 `NoteType::RichText` 显示；清单/涂鸦便签无工具条（`ApplyFormat` 默认空实现自然忽略）。
- 所有格式操作作用于**当前选区**（`SCF_SELECTION`）；无选区时作用于光标处的插入格式（RichEdit 原生语义，不做特判）。
- 字号：阶梯 {8,9,10,11,12,14,16,18,24}pt，A+/A- 沿阶梯走，两端夹住；选区字号混合（mask 无 CFM_SIZE）时以默认字号为基准。
- 文字色：调色板 {0x202020 墨, 0xC0392B 红, 0x1F6FBF 蓝, 0x1E8449 绿, 0xB7770D 橙} 循环；选区颜色混合或不在板内 → 从第一色开始。
- 格式操作后 `m_edit.SetModify(TRUE)`（确保 800ms 自动保存把格式落盘）。
- **保真变更**：Load 整篇归一不再重置字号；ApplyTheme 文字色只设 SCF_DEFAULT。换主题不再抹字符色（P7 遗留 Minor 就此关闭）。
- 默认字号 settings 键 `default_font_pt`（int，默认 10），循环 {9,10,11,12,14}；只影响**此后新输入/新便签**，已有内容不动。

**本阶段范围外（声明）：** 字体族选择（v1 全局微软雅黑）、高亮/背景色笔、项目符号列表、对齐、格式刷、工具条按钮按下态高亮（自绘 hover 态）、清单条目级格式。导入导出与贴到应用窗口留 P9。

---

## 文件结构

**新增：**
- `src/ui/FormatBarLayout.h/.cpp` — 纯布局 + FmtOp 枚举 + 字号阶梯 + 调色板循环（tests + app）。
- `tests/test_formatbar_layout.cpp` — 上述 doctest。

**修改：**
- `src/ui/INoteContentView.h` — `ApplyFormat` 默认空实现。
- `src/ui/TextContentView.h/.cpp` — ApplyFormat 实现 + SetDefaultFontPt + 保真变更。
- `src/ui/NoteWindow.h/.cpp` — 工具条绘制/命中 + 内容区下移。
- `src/ui/SettingsDialog.cpp` — 默认字号行（插入索引 2，自启后移到 3，热键 4+）。
- `src/app/NoteApp.cpp` — 启动时 SetDefaultFontPt。
- `tests/tests.vcxproj`、`app/open_windows_note_app.vcxproj` — 登记新文件。
- `docs/superpowers/smoke/P8-smoke-checklist.md` — 新增。

---

### Task 1: FormatBarLayout 纯逻辑（布局 + 字号阶梯 + 调色板）

**Files:**
- Create: `src/ui/FormatBarLayout.h`, `src/ui/FormatBarLayout.cpp`
- Test: `tests/test_formatbar_layout.cpp`（新）
- Modify: `tests/tests.vcxproj`, `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Produces（Task 2/3 消费，签名必须逐字一致）:
  - `enum class own::FmtOp { Bold, Italic, Underline, Strike, SizeDown, SizeUp, TextColor };`（值顺序=按钮顺序）
  - `constexpr int own::kFmtOpCount = 7;`
  - `struct own::FormatBarMetrics { int height; int btnSize; int padX; int gap; };`
  - `own::RectI own::formatBarRect(RectI client, int titleHeight, FormatBarMetrics m);` — 紧贴标题栏下方整宽条。
  - `own::RectI own::formatBarButton(RectI bar, FormatBarMetrics m, int index);` — 左起第 index 个按钮，垂直居中。
  - `int own::hitTestFormatBar(RectI bar, FormatBarMetrics m, int count, int px, int py);` — 命中按钮下标，否则 -1。
  - `int own::fontSizeStep(int twips, bool up);` — 沿阶梯 {160,180,200,220,240,280,320,360,480} 走一步，两端夹住；不在阶梯上先取不小于它的最近档（超出 480 视为 480）。
  - `uint32_t own::nextPaletteColor(uint32_t cur);` — 板 {0x202020,0xC0392B,0x1F6FBF,0x1E8449,0xB7770D} 循环；不在板内 → 0x202020。

- [ ] **Step 1: 写失败测试**

`tests/test_formatbar_layout.cpp`（新文件）：
```cpp
#include "doctest.h"
#include "ui/FormatBarLayout.h"

static own::FormatBarMetrics fm() { return { 22, 18, 6, 4 }; }

TEST_CASE("format bar sits under title bar, full width") {
    auto bar = own::formatBarRect({0,0,240,200}, 22, fm());
    CHECK(bar.x == 0);
    CHECK(bar.y == 22);
    CHECK(bar.w == 240);
    CHECK(bar.h == 22);
}
TEST_CASE("buttons lay out left to right with gap") {
    auto bar = own::formatBarRect({0,0,240,200}, 22, fm());
    auto b0 = own::formatBarButton(bar, fm(), 0);
    auto b1 = own::formatBarButton(bar, fm(), 1);
    CHECK(b0.x == 6);                     // padX
    CHECK(b0.y == 22 + 2);                // (22-18)/2 vertical center
    CHECK(b0.w == 18);
    CHECK(b1.x == 6 + 18 + 4);            // prev + btnSize + gap
    CHECK(own::kFmtOpCount == 7);
}
TEST_CASE("hit test maps point to button index or -1") {
    auto bar = own::formatBarRect({0,0,240,200}, 22, fm());
    auto b2 = own::formatBarButton(bar, fm(), 2);
    CHECK(own::hitTestFormatBar(bar, fm(), 7, b2.x + 1, b2.y + 1) == 2);
    CHECK(own::hitTestFormatBar(bar, fm(), 7, 239, 23) == -1);     // right blank area
    CHECK(own::hitTestFormatBar(bar, fm(), 7, 3, 30) == -1);       // in padX gutter
    CHECK(own::hitTestFormatBar(bar, fm(), 7, 10, 5) == -1);       // above the bar
}
TEST_CASE("fontSizeStep walks the ladder and clamps") {
    CHECK(own::fontSizeStep(200, true) == 220);    // 10pt -> 11pt
    CHECK(own::fontSizeStep(200, false) == 180);   // 10pt -> 9pt
    CHECK(own::fontSizeStep(480, true) == 480);    // top clamp
    CHECK(own::fontSizeStep(160, false) == 160);   // bottom clamp
    CHECK(own::fontSizeStep(210, true) == 240);    // off-ladder snaps to 220 then steps
    CHECK(own::fontSizeStep(210, false) == 200);
    CHECK(own::fontSizeStep(9999, true) == 480);   // beyond top treated as 480
    CHECK(own::fontSizeStep(1, false) == 160);     // below bottom treated as 160
}
TEST_CASE("nextPaletteColor cycles and falls back to ink") {
    CHECK(own::nextPaletteColor(0x202020) == 0xC0392B);
    CHECK(own::nextPaletteColor(0xC0392B) == 0x1F6FBF);
    CHECK(own::nextPaletteColor(0xB7770D) == 0x202020);   // wrap
    CHECK(own::nextPaletteColor(0x123456) == 0x202020);   // unknown -> ink
}
```
`tests/tests.vcxproj` ClCompile 组加（挨着 test_titlebar_layout.cpp / TitleBarLayout.cpp 的既有条目）：
```xml
    <ClCompile Include="test_formatbar_layout.cpp" />
    <ClCompile Include="..\src\ui\FormatBarLayout.cpp" />
```

- [ ] **Step 2: 运行验证失败**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error" | head`
Expected: 编译失败（`ui/FormatBarLayout.h` 不存在）。

- [ ] **Step 3: 实现**

`src/ui/FormatBarLayout.h`：
```cpp
#pragma once
#include <cstdint>
#include "domain/Models.h"
namespace own {
// 文本便签格式工具条：纯布局 + 字号阶梯 + 文字色调色板（无 Win32 依赖，进 tests）
enum class FmtOp { Bold, Italic, Underline, Strike, SizeDown, SizeUp, TextColor };
constexpr int kFmtOpCount = 7;
struct FormatBarMetrics { int height; int btnSize; int padX; int gap; };
RectI formatBarRect(RectI client, int titleHeight, FormatBarMetrics m);
RectI formatBarButton(RectI bar, FormatBarMetrics m, int index);
int hitTestFormatBar(RectI bar, FormatBarMetrics m, int count, int px, int py);
int fontSizeStep(int twips, bool up);            // 阶梯 160..480 twip，走一步并夹住
uint32_t nextPaletteColor(uint32_t cur);         // 0xRRGGBB 调色板循环
}
```
`src/ui/FormatBarLayout.cpp`：
```cpp
#include "ui/FormatBarLayout.h"
namespace own {
RectI formatBarRect(RectI client, int titleHeight, FormatBarMetrics m) {
    return { client.x, client.y + titleHeight, client.w, m.height };
}
RectI formatBarButton(RectI bar, FormatBarMetrics m, int index) {
    int x = bar.x + m.padX + index * (m.btnSize + m.gap);
    int y = bar.y + (bar.h - m.btnSize) / 2;
    return { x, y, m.btnSize, m.btnSize };
}
int hitTestFormatBar(RectI bar, FormatBarMetrics m, int count, int px, int py) {
    for (int i = 0; i < count; ++i) {
        RectI b = formatBarButton(bar, m, i);
        if (px >= b.x && px < b.x + b.w && py >= b.y && py < b.y + b.h) return i;
    }
    return -1;
}
static int snapIdx(int twips) {                       // 不小于 twips 的最近档；超顶取顶
    static const int ladder[] = { 160, 180, 200, 220, 240, 280, 320, 360, 480 };
    for (int i = 0; i < 9; ++i) if (twips <= ladder[i]) return i;
    return 8;
}
int fontSizeStep(int twips, bool up) {                // 语义：先 snap 到档位，再走一步，两端夹住
    static const int ladder[] = { 160, 180, 200, 220, 240, 280, 320, 360, 480 };
    int idx = snapIdx(twips);
    if (up)  { if (idx < 8) ++idx; }
    else     { if (idx > 0) --idx; }
    return ladder[idx];
}
uint32_t nextPaletteColor(uint32_t cur) {
    static const uint32_t pal[] = { 0x202020, 0xC0392B, 0x1F6FBF, 0x1E8449, 0xB7770D };
    const int n = sizeof(pal) / sizeof(pal[0]);
    for (int i = 0; i < n; ++i)
        if (pal[i] == cur) return pal[(i + 1) % n];
    return pal[0];
}
}
```
> `fontSizeStep` 语义 =「先 snap 后 step」：snapIdx 取不小于 twips 的最近档（超顶取顶），再按方向走一步并夹住。逐条对照 Step 1 测试：up(200)→220、down(200)→180、up(480)→480、down(160)→160、up(210)：snap 到 220 再走→240、down(210)：snap 到 220 退档→200、up(9999)→480、down(1)→160。全部吻合。

`app/open_windows_note_app.vcxproj`：ClCompile 加 `<ClCompile Include="..\src\ui\FormatBarLayout.cpp" />`，ClInclude 加 `<ClInclude Include="..\src\ui\FormatBarLayout.h" />`。

- [ ] **Step 4: 运行验证通过**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: 全绿（87→92 用例）。

- [ ] **Step 5: Commit**

```bash
git add src/ui/FormatBarLayout.h src/ui/FormatBarLayout.cpp tests/test_formatbar_layout.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(ui): format bar pure layout + font-size ladder + text palette cycle

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: TextContentView 格式引擎 + 默认字号 + 格式保真

**Files:**
- Modify: `src/ui/INoteContentView.h`
- Modify: `src/ui/TextContentView.h`, `src/ui/TextContentView.cpp`

**Interfaces:**
- Consumes: Task 1 `own::FmtOp`/`fontSizeStep`/`nextPaletteColor`。
- Produces（Task 3/4 消费）:
  - `INoteContentView`: `virtual void ApplyFormat(own::FmtOp op) {}`（默认空实现）。
  - `CTextContentView::ApplyFormat(own::FmtOp) override;`
  - `static void CTextContentView::SetDefaultFontPt(int pt);` — 进程级默认字号（影响此后 applyNoteFont 的 SCF_DEFAULT）。
- 行为变更（保真）：
  1. `Load()` 的整篇归一 mask 从 `CFM_FACE|CFM_SIZE|CFM_CHARSET` 收窄为 `CFM_FACE|CFM_CHARSET`（不再重置用户字号）。
  2. `ApplyTheme` 的文字色 `EM_SETCHARFORMAT` 从 `SCF_ALL` 改为 `SCF_DEFAULT`（不再抹字符级颜色）。
- GUI 任务：达标线=链接通过 + tests 全绿（92 不变）+ 启动存活。

- [ ] **Step 1: 接口与实现**

`src/ui/INoteContentView.h`：`#include "domain/Models.h"` 之后加 `#include "ui/FormatBarLayout.h"`；类内 `ApplyTheme` 行后加：
```cpp
    virtual void ApplyFormat(own::FmtOp op) {}                     // 选区格式；默认忽略（清单/涂鸦）
```
`src/ui/TextContentView.h`：public 区加：
```cpp
    void ApplyFormat(own::FmtOp op) override;
    static void SetDefaultFontPt(int pt);          // 默认字号（新输入生效；启动/设置变更时调用）
```
`src/ui/TextContentView.cpp`：
1. 匿名 namespace 里加文件级默认字号并改 `applyNoteFont`：
```cpp
static int s_defaultFontTwips = 200;                 // 10pt；SetDefaultFontPt 更新
// scope=SCF_DEFAULT 时含字号；SCF_ALL（整篇归一）只统一字体族，保住用户改过的字号
static void applyNoteFont(CRichEditCtrl& edit, WPARAM scope) {
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_CHARSET;
    if (scope == SCF_DEFAULT) { cf.dwMask |= CFM_SIZE; cf.yHeight = s_defaultFontTwips; }
    wcscpy_s(cf.szFaceName, L"微软雅黑");   // 微软雅黑
    cf.bCharSet = DEFAULT_CHARSET;
    ::SendMessage(edit.GetSafeHwnd(), EM_SETCHARFORMAT, scope, (LPARAM)&cf);
}
```
（原注释「scope = SCF_DEFAULT（新输入）或 SCF_ALL（整篇统一…）」相应更新。）
2. 文件末尾加：
```cpp
void CTextContentView::SetDefaultFontPt(int pt) {
    if (pt >= 8 && pt <= 24) s_defaultFontTwips = pt * 20;
}
void CTextContentView::ApplyFormat(own::FmtOp op) {
    if (!m_created) return;
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf);
    m_edit.GetSelectionCharFormat(*(CHARFORMAT2*)&cf);
    CHARFORMAT2W set{}; set.cbSize = sizeof(set);
    auto toggle = [&](DWORD effect) {
        set.dwMask = (effect == CFE_BOLD) ? CFM_BOLD
                   : (effect == CFE_ITALIC) ? CFM_ITALIC
                   : (effect == CFE_UNDERLINE) ? CFM_UNDERLINE : CFM_STRIKEOUT;
        // 选区内混合（mask 未含该位）按「未开」处理 -> 统一打开
        bool on = (cf.dwMask & set.dwMask) && (cf.dwEffects & effect);
        set.dwEffects = on ? 0 : effect;
    };
    switch (op) {
        case own::FmtOp::Bold:      toggle(CFE_BOLD); break;
        case own::FmtOp::Italic:    toggle(CFE_ITALIC); break;
        case own::FmtOp::Underline: toggle(CFE_UNDERLINE); break;
        case own::FmtOp::Strike:    toggle(CFE_STRIKEOUT); break;
        case own::FmtOp::SizeUp:
        case own::FmtOp::SizeDown: {
            int cur = (cf.dwMask & CFM_SIZE) ? (int)cf.yHeight : s_defaultFontTwips;  // 混合选区从默认起步
            set.dwMask = CFM_SIZE;
            set.yHeight = own::fontSizeStep(cur, op == own::FmtOp::SizeUp);
            break;
        }
        case own::FmtOp::TextColor: {
            uint32_t curRgb = 0xFFFFFFFF;                    // 无效值 -> 调色板回落首色
            if ((cf.dwMask & CFM_COLOR) && !(cf.dwEffects & CFE_AUTOCOLOR)) {
                COLORREF c = cf.crTextColor;                 // COLORREF 是 0xBBGGRR，转回 0xRRGGBB
                curRgb = ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | GetBValue(c);
            }
            uint32_t next = own::nextPaletteColor(curRgb);
            set.dwMask = CFM_COLOR;
            set.crTextColor = RGB((next >> 16) & 0xFF, (next >> 8) & 0xFF, next & 0xFF);
            break;
        }
    }
    m_edit.SetSelectionCharFormat(*(CHARFORMAT2*)&set);
    m_edit.SetModify(TRUE);                                  // 格式变更计脏，随 800ms 自动保存落盘
    m_edit.SetFocus();                                       // 点工具条后焦点还给编辑器
}
```
> `GetSelectionCharFormat/SetSelectionCharFormat` 的 MFC 签名收 `CHARFORMAT2&`——若编译器对 `CHARFORMAT2W` 直传报错才用上面的强转；能直传就直传（`CHARFORMAT2` 在 UNICODE 工程即 `CHARFORMAT2W`，大概率直传即可，届时删掉强转并在报告里注明）。
3. `ApplyTheme` 里 `EM_SETCHARFORMAT` 的 scope 参数 `SCF_ALL` 改为 `SCF_DEFAULT`，并把上方注释改为：
```cpp
    // 文字色只设默认格式：字符级颜色（工具条设置）在换主题后保留；
    // 4 套内置主题 textColor 相同，因此已有文字无需跟随
```
4. `Load()` 尾部的 `applyNoteFont(m_edit, SCF_ALL);` 行注释更新为 `// 整篇统一字体族（不动字号/颜色/加粗——用户格式保真）`。

- [ ] **Step 2: 构建 + 全绿 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist 2>/dev/null | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿（92 用例不变）；ALIVE。

- [ ] **Step 3: Commit**

```bash
git add src/ui/INoteContentView.h src/ui/TextContentView.h src/ui/TextContentView.cpp
git commit -m "feat(ui): rich text ApplyFormat engine + default font size + format-preserving load/theme

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: NoteWindow 格式工具条（绘制 + 命中 + 内容区下移）

**Files:**
- Modify: `src/ui/NoteWindow.h`, `src/ui/NoteWindow.cpp`

**Interfaces:**
- Consumes: Task 1 `own::formatBarRect/formatBarButton/hitTestFormatBar/FmtOp/kFmtOpCount`；Task 2 `INoteContentView::ApplyFormat`。
- Produces: 无对外新接口（窗口内部行为）。
- GUI 任务：达标线=链接通过 + tests 全绿 + 启动存活；视觉落 Task 5 冒烟。

- [ ] **Step 1: 实现**

`src/ui/NoteWindow.h`：include 区加 `#include "ui/FormatBarLayout.h"`；私有区加：
```cpp
    bool hasFormatBar() const;   // 仅富文本便签显示工具条
    int  contentTop() const;     // 标题栏高 + 工具条高（无工具条时仅标题栏）
```
`src/ui/NoteWindow.cpp`：
1. `kTitleMetrics` 行下方加：
```cpp
static const own::FormatBarMetrics kFmtMetrics{ 22, 18, 6, 4 };   // 高 22 / 钮 18 的格式工具条
```
2. 实现两个 helper（放在 `layout()` 实现旁）：
```cpp
bool CNoteWindow::hasFormatBar() const { return m_note.type == own::NoteType::RichText; }
int  CNoteWindow::contentTop() const {
    return kTitleMetrics.height + (hasFormatBar() ? kFmtMetrics.height : 0);
}
```
3. `Create()` 与 `layoutContent()` 里的 `own::noteContentRect({0,0,rc.Width(),rc.Height()}, kTitleMetrics.height, 6)` 两处都改为：
```cpp
        own::RectI cr = own::noteContentRect({0,0,rc.Width(),rc.Height()}, contentTop(), 6);
```
（`layoutContent()` 中变量名保持原样，只换第二个实参。）
4. `OnPaint()`：换色按钮绘制段之后、标题文字绘制段之前，加工具条绘制：
```cpp
        // 格式工具条（仅富文本、未卷起）：B I U S A- A+ ●
        if (hasFormatBar() && !m_note.rolledUp) {
            own::RectI bar = own::formatBarRect({0,0,rc.Width(),rc.Height()}, kTitleMetrics.height, kFmtMetrics);
            // 条底色 = 标题色与背景色的中间过渡：直接用背景色 + 底部 1px 分隔线
            Pen sep(Color(60, 0x40, 0x40, 0x40), 1.0f);
            g.DrawLine(&sep, bar.x, bar.y + bar.h - 1, bar.x + bar.w, bar.y + bar.h - 1);
            FontFamily ff(L"微软雅黑");   // 微软雅黑
            Font fN(&ff, 12, FontStyleRegular, UnitPixel);
            Font fB(&ff, 12, FontStyleBold, UnitPixel);
            Font fI(&ff, 12, FontStyleItalic, UnitPixel);
            Font fU(&ff, 12, FontStyleUnderline, UnitPixel);
            Font fS(&ff, 12, FontStyleStrikeout, UnitPixel);
            Font fSmall(&ff, 10, FontStyleRegular, UnitPixel);
            SolidBrush ink(Color(255, 0x40, 0x40, 0x40));
            StringFormat cf2; cf2.SetAlignment(StringAlignmentCenter); cf2.SetLineAlignment(StringAlignmentCenter);
            auto drawGlyph = [&](int idx, const wchar_t* s, const Font& f) {
                own::RectI b = own::formatBarButton(bar, kFmtMetrics, idx);
                RectF r2((REAL)b.x, (REAL)b.y, (REAL)b.w, (REAL)b.h);
                g.DrawString(s, -1, &f, r2, &cf2, &ink);
            };
            drawGlyph(0, L"B", fB);
            drawGlyph(1, L"I", fI);
            drawGlyph(2, L"U", fU);
            drawGlyph(3, L"S", fS);
            drawGlyph(4, L"A-", fSmall);
            drawGlyph(5, L"A+", fSmall);
            {   // 文字色按钮：实心圆点
                own::RectI b = own::formatBarButton(bar, kFmtMetrics, 6);
                SolidBrush dot2(Color(255, 0xC0, 0x39, 0x2B));
                g.FillEllipse(&dot2, b.x + 4, b.y + 4, b.w - 8, b.h - 8);
            }
        }
```
5. `OnLButtonDown()`：`auto L = layout();` 行之前（resize edge 检查之后）加工具条命中：
```cpp
    if (hasFormatBar() && !m_note.rolledUp) {
        CRect rc2; GetClientRect(&rc2);
        own::RectI bar = own::formatBarRect({0,0,rc2.Width(),rc2.Height()}, kTitleMetrics.height, kFmtMetrics);
        int idx = own::hitTestFormatBar(bar, kFmtMetrics, own::kFmtOpCount, pt.x, pt.y);
        if (idx >= 0) {
            if (m_content) m_content->ApplyFormat((own::FmtOp)idx);
            return;
        }
        // 命中工具条空白区不做事也不落到标题栏逻辑
        if (pt.y >= bar.y && pt.y < bar.y + bar.h) return;
    }
```
6. 卷起/展开（`TitleHit::Roll` 分支）不用改：卷起高度 = `kTitleMetrics.height`，工具条随内容一起被窗口高度裁掉；绘制与命中都有 `!m_note.rolledUp` 守卫。

- [ ] **Step 2: 构建 + 全绿 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist 2>/dev/null | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿；ALIVE。

- [ ] **Step 3: Commit**

```bash
git add src/ui/NoteWindow.h src/ui/NoteWindow.cpp
git commit -m "feat(ui): format toolbar strip on rich text notes — draw, hit test, content shift

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: 设置「默认字号」行 + 启动接线

**Files:**
- Modify: `src/ui/SettingsDialog.cpp`
- Modify: `src/app/NoteApp.cpp`

**Interfaces:**
- Consumes: Task 2 `CTextContentView::SetDefaultFontPt(int)`；`own::SettingsStore::getInt/setInt`。
- Produces: settings 键 `default_font_pt`（int，默认 10，循环 {9,10,11,12,14}）。
- 行索引变化：0=默认主题 1=默认透明度 **2=默认字号（新）** 3=开机自启 4+=热键。窗口高度由 `rowCount()` 自动加高。
- GUI 任务：达标线=链接通过 + tests 全绿 + 启动存活。

- [ ] **Step 1: SettingsDialog 加行**

`src/ui/SettingsDialog.cpp`：
1. include 区加 `#include "ui/TextContentView.h"`。
2. `rowCount()` 改为：
```cpp
    int rowCount() const { return 4 + (int)hotkeys->bindings().size(); }   // 4 通用行 + 热键行数
```
3. `rowLabel(i)`：`if (i == 2)`（开机自启）改为 `if (i == 3)`；在其前插入：
```cpp
        if (i == 2) {                                  // 默认字号：N pt
            int pt = st.getInt("default_font_pt", 10);
            CString s; s.Format(_T("\x9ED8\x8BA4\x5B57\x53F7\xFF1A%d pt"), pt);   // 默认字号：
            return s;
        }
```
末尾热键分支的 `hotkeys->bindings()[i - 3]` 改为 `[i - 4]`。
4. `clickRow(i)`：`else if (i == 2)`（自启）改为 `else if (i == 3)`；其前插入：
```cpp
        } else if (i == 2) {
            static const int pts[] = { 9, 10, 11, 12, 14 };
            int cur = st.getInt("default_font_pt", 10);
            int idx = 0; for (int k = 0; k < 5; ++k) if (pts[k] == cur) { idx = k; break; }
            int next = pts[(idx + 1) % 5];
            st.setInt("default_font_pt", next);
            CTextContentView::SetDefaultFontPt(next);          // 即时生效于此后新输入/新便签
```
热键分支 `else if (i >= 3)` 改为 `else if (i >= 4)`，其中 `bs[i - 3]`、`(int)k == i - 3` 全部改为 `i - 4`。

- [ ] **Step 2: NoteApp 启动接线**

`src/app/NoteApp.cpp`：include 区加 `#include "ui/TextContentView.h"`（已有则略）。`InitInstance` 里创建便签窗口**之前**（提醒接线段附近，找 `own::SettingsStore` 已有用法；若无合适位置就放在 DB 打开成功之后）加：
```cpp
    {
        own::SettingsStore st(m_db);
        CTextContentView::SetDefaultFontPt(st.getInt("default_font_pt", 10));   // 默认字号先于建窗生效
    }
```

- [ ] **Step 3: 构建 + 全绿 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist 2>/dev/null | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿；ALIVE。

- [ ] **Step 4: Commit**

```bash
git add src/ui/SettingsDialog.cpp src/app/NoteApp.cpp
git commit -m "feat(ui/app): default font size setting row + startup wiring

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: P8 手工冒烟清单

**Files:**
- Create: `docs/superpowers/smoke/P8-smoke-checklist.md`

- [ ] **Step 1: 写清单**

`docs/superpowers/smoke/P8-smoke-checklist.md`:
```markdown
# P8 手工冒烟清单（富文本工具条 + 字体字号）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

## 工具条显示
- [ ] 富文本便签标题栏下方出现工具条：B I U S A- A+ ●（红点），底部有分隔线
- [ ] 清单/涂鸦便签无工具条，内容区位置与 P7 一致
- [ ] 卷起后工具条随内容隐藏，展开恢复；正文不被工具条遮挡

## 选区格式
- [ ] 选中文字点 B/I/U/S：分别加粗/斜体/下划线/删除线；再点一次取消
- [ ] 混合选区（部分加粗）点 B：整段统一为加粗
- [ ] 选中文字点 A+ 连点：字号沿 8/9/10/11/12/14/16/18/24pt 变大，到 24 停；A- 反向到 8 停
- [ ] 点 ●：选中文字循环 墨→红→蓝→绿→橙→墨
- [ ] 无选区时点 B 再输入：新输入的字是粗体（RichEdit 插入格式语义）
- [ ] 点完工具条焦点回到编辑器，能直接继续打字

## 格式持久化（关键回归）
- [ ] 做一段混合格式（粗体+大字号+红色），等 1 秒自动保存，关闭再打开便签：格式完整保留
- [ ] 对带格式的便签点换色按钮切主题：字符级颜色/字号/粗体全部保留，背景正常切换
- [ ] 重启应用后格式仍在

## 默认字号
- [ ] 设置弹层出现「默认字号：10 pt」行（第 3 行，自启上方），点击循环 9/10/11/12/14
- [ ] 设默认字号=14 后新建富文本便签：直接输入的文字是 14pt；旧便签不受影响
- [ ] 重启后设置行显示 14 pt，新便签仍 14pt（持久化）

## 回归
- [ ] 清单勾选/编辑、涂鸦绘制不受影响
- [ ] 便签窗拖动/缩放/置顶/透明度/卷起均正常；工具条不拦截标题栏按钮
- [ ] 跨便签搜索仍能命中带格式文本的内容（plainText 提取不受格式影响）
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/smoke/P8-smoke-checklist.md
git commit -m "docs: P8 rich text toolbar manual smoke checklist

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage：**
- §1.3「**富文本**」→ Task 1/2/3（选区级 B/I/U/S/字号/文字色，落盘走既有 RTF StreamOut）。✓
- §1.3「自定义字体字号」→ v1 语义拍板为「默认字号设置 + 选区字号调节」（Task 2/4）；字体族选择显式列入范围外（全局微软雅黑，P7 字体统一的延续）。✓（范围外声明）
- P7 遗留「SCF_ALL 抹字符色」→ Task 2 保真变更关闭。✓
- P7 遗留「字体字号设置随富文本工具条计划」→ Task 4 兑现。✓

**2. Placeholder scan：** 无 TBD/TODO。Task 1 的 `fontSizeStep` 语义已逐条对照全部 8 个测试用例验证吻合。Task 2 的 CHARFORMAT2 强转标注了「能直传就直传」的双路径与判断依据（编译是否报错），非未决设计。

**3. Type consistency：**
- `FmtOp`/`kFmtOpCount`/`FormatBarMetrics`/`formatBarRect/formatBarButton/hitTestFormatBar/fontSizeStep/nextPaletteColor`（Task 1）↔ Task 2 `ApplyFormat` switch、Task 3 绘制/命中调用。✓
- `ApplyFormat(own::FmtOp)`（Task 2 接口）↔ Task 3 `m_content->ApplyFormat((own::FmtOp)idx)`（enum 值顺序=按钮下标，Task 1 注明）。✓
- `CTextContentView::SetDefaultFontPt(int)`（Task 2）↔ Task 4 两处调用。✓
- settings 键 `default_font_pt`（Task 4 读写一致；Task 2 静态默认 200 twip=10pt 与 getInt 默认 10 一致）。✓
- 行索引迁移（Task 4）：rowCount 3→4、自启 2→3、热键 3→4 偏移，`rowLabel`/`clickRow` 两处同步列出。✓

**已知限制（执行者须知）：** 工具条按钮无按下/悬停高亮、无「当前选区是否加粗」的状态回显（范围外，v1 接受）。`SetSelectionCharFormat` 在无选区时设置的是插入点格式——RichEdit 原生语义，冒烟含此用例。`ApplyFormat` 里 `SetFocus` 把焦点交还编辑器，点击工具条瞬间 RichEdit 若因失焦隐藏选区高亮属正常视觉（格式仍作用于原选区，RichEdit 默认保留选区范围）。清单 in-place 编辑框、涂鸦不受格式接口影响（默认空实现）。