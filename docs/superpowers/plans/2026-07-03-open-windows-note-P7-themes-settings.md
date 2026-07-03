# P7 主题 + 设置弹层 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 便签接入主题系统（标题栏换色按钮循环 4 内置主题，替换硬编码黄色），新增自绘设置弹层（默认主题/默认透明度/开机自启/热键改键——补上 P5 遗留的改键 UI），并收编 P6 最终审查的遗留卫生项。

**Architecture:** 数据层补 `NoteStore` 主题读取 + 专用 `updateNoteTheme`（**禁止**用 `updateNote` 换主题——它整行覆盖 `content_blob`，窗口里的 blob 快照是旧的，会把正文冲掉）。领域层加纯函数 `nextThemeId`（循环取下一主题）。表现层：`TitleBarLayout` 加第 5 个按钮（纯逻辑 doctest），`INoteContentView` 加默认空实现的 `ApplyTheme(bg,text)`，三个内容视图各自落实；`CNoteWindow` 按 `note.themeId` 取色绘制。设置弹层仿 `promptText` 的自绘模态 CWnd + 手写消息循环，行级点击即时生效；改键行复用既有纯函数 `parseHotkey`/`findHotkeyConflicts` 校验后写 settings 并整体重注册。

**Tech Stack:** C++17 · MFC 静态链接 · GDI/GDI+ · SQLite（themes/settings 表已存在，无迁移）· doctest。

## Global Constraints

- 语言/工具链：C++17（`/std:c++17`）、MFC **静态链接**、无 PCH、仅 `x64`。
- 编码：所有工程 ClCompile 带 `/utf-8`；中文 UI 字面量在 `_T("")` 宽字符串里用 `\xXXXX` 转义 + 行尾注释原文（代码库惯例）；**测试文件断言只用 ASCII**（内置主题名是中文——测试断言用 id/颜色值，不断言名字）。
- 命名空间：`src/domain` 纯逻辑一律 `namespace own`，**不得** include `<afxwin.h>`/`<windows.h>`。`src/ui`/`src/services` 仅进 app 工程。
- 颜色约定：`Theme` 色值是 `0xRRGGBB`；GDI `COLORREF` 是 `0x00BBGGRR`——**必须转换**：`RGB((c>>16)&0xFF,(c>>8)&0xFF,c&0xFF)`；GDI+ `Color(255,(c>>16)&0xFF,(c>>8)&0xFF,c&0xFF)`。
- 构建：只能通过 `.sln`。MSBuild：`"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`（下称 `$MSB`），参数 `-p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo`。
- **每次重建前先杀残留**：`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`（LNK1168）。
- 自动化达标线：纯逻辑/数据任务=`./x64/Debug/tests.exe` 全绿；GUI 任务=app 链接通过 + 启动存活；视觉/交互行为落 Task 7 手工冒烟。
- 每次提交末尾附：`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。
- 分支：直接在 `main` 上开发。当前基线：81 用例全绿（P6 完成 + UI 字体修复后）。

**承接的既有接口（勿重复实现）：**
- `own::Theme`（`src/domain/Models.h`）：`{int64_t id; std::string name; uint32_t bgColor, titleColor, textColor; bool isBuiltin;}`。
- 内置主题（P1 播种，`Migrations.cpp`）：黄 `{0xFFF7B0,0xF2D24A,0x202020}`、粉 `{0xFFD9E8,0xF2A0C4,0x202020}`（近似，以 DB 实际为准，测试只断言第一条黄）、蓝、绿，`is_builtin=1`，id 1..4。
- `own::NoteStore`（`src/data/NoteStore.h`）：既有 CRUD；`freshDb()`（`tests/test_notestore.cpp`）建库即播种主题。
- `own::SettingsStore`（`src/data/SettingsStore.h`）：`getString/setString/getInt/setInt`。
- `own::parseHotkey(const std::string&, Hotkey&)` / `formatHotkey` / `findHotkeyConflicts(vector<Hotkey>)`（`src/domain/Hotkey.h`）。
- `HotkeyManager`（`src/services/HotkeyManager.h`）：`add/loadAndRegister(HWND, SettingsStore&)/unregisterAll(HWND)/bindings()`；`CNoteApp` 私有成员 `m_hotkeys`，六个绑定 name = `new/new_checklist/new_drawing/manager/toggle_all/quit`，默认 `Ctrl+Alt+N/2/3/M/H/Q`。
- `own_svc::autostartIsEnabled()/autostartSetEnabled(bool)`（`src/services/AutostartManager.h`）。
- `own_ui::promptText(CWnd*, const CString&, CString&)`（`src/ui/TextPrompt.h`）；`own_ui::uiFont(int px)`（`src/ui/UiFont.h`）。
- `own::layoutTitleBar/hitTestTitleBar`（`src/ui/TitleBarLayout.h`）：现有 4 按钮 close/roll/pin/opacity + dragArea；`CNoteWindow` 的 `kTitleMetrics{22,16,4,4}`。
- `CAppHostWindow::showTrayMenu()`（`src/app/AppHostWindow.cpp`）：托盘菜单命令 id 1..6 已占用，**本计划用 7=设置**。
- u8↔wide 转换 helper 惯例：各 `.cpp` 内 static 局部副本（`NoteListView.cpp`/`ReminderToast.cpp` 有样例）。

**语义决策：**
- 换主题**只写 theme_id**（`updateNoteTheme`），不动其它列。
- `note.themeId=0` 或查不到 → 回落内置黄（硬编码兜底 `{0xFFF7B0,0xF2D24A,0x202020}`），不崩。
- 设置弹层各行**点击即时生效**（写 settings / 切自启 / 重注册热键），无 OK/取消；ESC 或关闭按钮退出。
- 默认主题/透明度只影响**新建**便签；已开便签不受影响。
- 改键：输入非法格式或与其它绑定冲突 → 弹提示、不写库、不重注册。
- v1 不做自定义主题增删（内置 4 套循环）；主题选择即标题栏循环按钮 + 设置里循环默认值。

**本阶段范围外（声明）：** 自定义主题编辑器、字体字号设置（随富文本工具条计划）、透明度滑块（保留现有 4 档循环）、贴到应用窗口、导入导出。P6 遗留中的「toast 退出泄漏 Debug 噪音」「toast 多显示器定位」继续后置（后者依赖多屏专项）。

---

## 文件结构

**新增：**
- `src/domain/ThemeRules.h/.cpp` — `nextThemeId` 纯函数（tests + app）。
- `src/ui/SettingsDialog.h/.cpp` — 自绘设置弹层（仅 app）。

**修改：**
- `src/data/NoteStore.h/.cpp` — `allThemes/getTheme/updateNoteTheme`。
- `src/ui/TitleBarLayout.h/.cpp` — themeBtn + `TitleHit::Theme`。
- `src/ui/INoteContentView.h` — `ApplyTheme` 默认空实现。
- `src/ui/TextContentView.h/.cpp`、`ChecklistContentView.h/.cpp`、`DrawingContentView.h/.cpp` — 落实 ApplyTheme。
- `src/ui/NoteWindow.h/.cpp` — 主题取色绘制 + 换色按钮。
- `src/ui/NoteListView.cpp` — reload N+1 收敛（卫生项）。
- `src/app/AppHostWindow.h/.cpp` — 托盘「设置」项 + KillTimer（卫生项）。
- `src/app/NoteApp.h/.cpp` — 新建默认值 + 设置弹层接线 + 成员序注释（卫生项）。
- `tests/test_reminder_rules.cpp`、`tests/test_notestore.cpp`、`tests/test_titlebar_layout.cpp`、新增 `tests/test_theme_rules.cpp`。
- 两个 vcxproj 登记新文件。
- `docs/superpowers/smoke/P7-smoke-checklist.md` — 新增。

---

### Task 1: P6 遗留卫生项

**Files:**
- Modify: `src/app/AppHostWindow.cpp`（OnDestroy）
- Modify: `src/app/NoteApp.h`（成员注释）
- Modify: `src/ui/NoteListView.cpp`（reload N+1）
- Test: `tests/test_reminder_rules.cpp`（追加）

**Interfaces:** 无新接口；行为不变（reload 优化语义等价）。

- [ ] **Step 1: 写补测（腐坏 recurrence 的 dismiss 兜底）**

`tests/test_reminder_rules.cpp` 末尾追加：
```cpp
TEST_CASE("resolveReminderDismiss disables reminder when recurrence value is corrupt") {
    // DB 腐坏契约：computeNextDue 对未知 recurrence 返回 0 -> 兜底禁用
    own::Reminder r; r.dueAt = 1000; r.enabled = true; r.snoozeUntil = 500;
    r.recurrence = (own::Recurrence)99;
    auto x = own::resolveReminderDismiss(r, 1000);
    CHECK_FALSE(x.enabled);
    CHECK(x.snoozeUntil == 0);
}
```

- [ ] **Step 2: 构建跑测**（该分支已实现，特征化补测，应直接通过）

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: 全绿（81→82 用例）。

- [ ] **Step 3: KillTimer + 成员序注释 + reload 收敛**

`src/app/AppHostWindow.cpp` 的 `OnDestroy()` 改为：
```cpp
void CAppHostWindow::OnDestroy() {
    KillTimer(kReminderTimerId);   // 显式清理（窗口销毁本会自动杀，防御式）
    m_tray.remove();
    CWnd::OnDestroy();
}
```
`src/app/NoteApp.h` 中 `ReminderScheduler m_reminders;` 行改为：
```cpp
    ReminderScheduler m_reminders;   // 注意：声明在 m_host/m_store 之后，先于二者析构；
                                     // 若未来加析构逻辑勿在其中触碰 store/host
```
`src/ui/NoteListView.cpp`：文件顶部 include 区加 `#include <set>`；`reload()` 里把逐行 `remindersOfNote` 循环替换——先在 `int64_t now = ...` 行之前加：
```cpp
    std::set<int64_t> remNotes;   // 一次查询代替每行 remindersOfNote（N+1）
    for (const auto& rem : m_store->enabledReminders()) remNotes.insert(rem.noteId);
```
再把行内的
```cpp
        for (const auto& rem : m_store->remindersOfNote(n.id))
            if (rem.enabled) { r.title = "\xE2\x8F\xB0 " + r.title; break; }   // ⏰ 有提醒
```
替换为：
```cpp
        if (remNotes.count(n.id)) r.title = "\xE2\x8F\xB0 " + r.title;   // ⏰ 有提醒
```

- [ ] **Step 4: 构建 + 全绿 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist 2>/dev/null | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 全绿 82 用例；ALIVE。

- [ ] **Step 5: Commit**

```bash
git add tests/test_reminder_rules.cpp src/app/AppHostWindow.cpp src/app/NoteApp.h src/ui/NoteListView.cpp
git commit -m "chore: P6 deferred hygiene — corrupt-recurrence doctest, KillTimer, reload prefetch

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: 主题数据访问 + 循环纯函数

**Files:**
- Modify: `src/data/NoteStore.h`, `src/data/NoteStore.cpp`
- Create: `src/domain/ThemeRules.h`, `src/domain/ThemeRules.cpp`
- Test: `tests/test_notestore.cpp`（追加）, `tests/test_theme_rules.cpp`（新）
- Modify: `tests/tests.vcxproj`, `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Produces（Task 4/5 消费）:
  - `std::vector<Theme> own::NoteStore::allThemes();` — 按 id 升序。
  - `std::optional<Theme> own::NoteStore::getTheme(int64_t id);`
  - `bool own::NoteStore::updateNoteTheme(int64_t noteId, int64_t themeId);` — **只**改 `theme_id` 列。
  - `int64_t own::nextThemeId(const std::vector<Theme>& themes, int64_t currentId);` — 返回列表中 currentId 的下一个（到尾回绕）；currentId 不在列表（含 0）→ 返回第一个；空列表 → 0。

- [ ] **Step 1: 写失败测试**

`tests/test_notestore.cpp` 末尾追加（断言只用 id/色值，不碰中文名）：
```cpp
TEST_CASE("themes are seeded and readable") {
    auto db = freshDb(); own::NoteStore s(db);
    auto ts = s.allThemes();
    REQUIRE(ts.size() == 4);
    CHECK(ts[0].id == 1);
    CHECK(ts[0].bgColor == 0xFFF7B0u);      // 内置黄
    CHECK(ts[0].titleColor == 0xF2D24Au);
    CHECK(ts[0].textColor == 0x202020u);
    CHECK(ts[0].isBuiltin);
    auto one = s.getTheme(ts[1].id);
    REQUIRE(one.has_value());
    CHECK(one->id == ts[1].id);
    CHECK(one->bgColor == ts[1].bgColor);
    CHECK_FALSE(s.getTheme(9999).has_value());
}

TEST_CASE("updateNoteTheme changes only theme_id") {
    auto db = freshDb(); own::NoteStore s(db);
    own::Note n; n.contentBlob = {1,2,3}; n.plainText = "keep";
    int64_t id = s.insertNote(n, 1000);
    CHECK(s.updateNoteTheme(id, 3));
    auto back = s.getNote(id);
    REQUIRE(back.has_value());
    CHECK(back->themeId == 3);
    CHECK(back->plainText == "keep");                 // 其它列不动
    REQUIRE(back->contentBlob.size() == 3);
    CHECK(back->contentBlob[2] == 3);
}
```
`tests/test_theme_rules.cpp`（新文件）：
```cpp
#include "doctest.h"
#include "domain/ThemeRules.h"

static std::vector<own::Theme> mk3() {
    std::vector<own::Theme> v(3);
    v[0].id = 1; v[1].id = 2; v[2].id = 3;
    return v;
}
TEST_CASE("nextThemeId cycles through list") {
    auto v = mk3();
    CHECK(own::nextThemeId(v, 1) == 2);
    CHECK(own::nextThemeId(v, 2) == 3);
    CHECK(own::nextThemeId(v, 3) == 1);       // 回绕
}
TEST_CASE("nextThemeId falls back to first when current unknown") {
    auto v = mk3();
    CHECK(own::nextThemeId(v, 0) == 1);       // 未设置主题
    CHECK(own::nextThemeId(v, 42) == 1);
    CHECK(own::nextThemeId({}, 1) == 0);      // 空列表
}
```
`tests/tests.vcxproj` ClCompile 组加：
```xml
    <ClCompile Include="test_theme_rules.cpp" />
    <ClCompile Include="..\src\domain\ThemeRules.cpp" />
```

- [ ] **Step 2: 运行验证失败**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error" | head`
Expected: 编译失败（`allThemes`/`ThemeRules.h` 不存在）。

- [ ] **Step 3: 实现**

`src/data/NoteStore.h` 的 reminders 区之后加：
```cpp
    // ---- themes ----
    std::vector<Theme> allThemes();
    std::optional<Theme> getTheme(int64_t id);
    bool updateNoteTheme(int64_t noteId, int64_t themeId);   // 只改 theme_id（updateNote 会整行覆盖 blob）
```
`src/data/NoteStore.cpp` 末尾（namespace 内，参考同文件 reminders 的 Statement 用法）加：
```cpp
static Theme readTheme(Statement& s) {
    Theme t;
    t.id = s.colInt64(0);
    t.name = s.colText(1);
    t.bgColor = (uint32_t)s.colInt64(2);
    t.titleColor = (uint32_t)s.colInt64(3);
    t.textColor = (uint32_t)s.colInt64(4);
    t.isBuiltin = s.colInt64(5) != 0;
    return t;
}
std::vector<Theme> NoteStore::allThemes() {
    std::vector<Theme> out;
    Statement s(db_, "SELECT id,name,bg_color,title_color,text_color,is_builtin FROM themes ORDER BY id;");
    while (s.step()) out.push_back(readTheme(s));
    return out;
}
std::optional<Theme> NoteStore::getTheme(int64_t id) {
    Statement s(db_, "SELECT id,name,bg_color,title_color,text_color,is_builtin FROM themes WHERE id=?;");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return readTheme(s);
}
bool NoteStore::updateNoteTheme(int64_t noteId, int64_t themeId) {
    Statement s(db_, "UPDATE notes SET theme_id=? WHERE id=?;");
    s.bind(1, themeId); s.bind(2, noteId);
    s.execDone();
    return true;
}
```
> `Statement` 的列读取方法名以 `src/data/Statement.h` 为准（reminders 的 `readRow`/`remindersOfNote` 实现里有现成用法——照抄其命名，如 `colInt64/colText` 若名不符则用实际名）。

`src/domain/ThemeRules.h`：
```cpp
#pragma once
#include <cstdint>
#include <vector>
#include "domain/Models.h"
namespace own {
// 主题循环：返回 themes 里 currentId 的下一个（回绕）；找不到→第一个；空→0
int64_t nextThemeId(const std::vector<Theme>& themes, int64_t currentId);
}
```
`src/domain/ThemeRules.cpp`：
```cpp
#include "domain/ThemeRules.h"
namespace own {
int64_t nextThemeId(const std::vector<Theme>& themes, int64_t currentId) {
    if (themes.empty()) return 0;
    for (size_t i = 0; i < themes.size(); ++i)
        if (themes[i].id == currentId)
            return themes[(i + 1) % themes.size()].id;
    return themes[0].id;
}
}
```
`app/open_windows_note_app.vcxproj`：ClCompile 加 `<ClCompile Include="..\src\domain\ThemeRules.cpp" />`，ClInclude 加 `<ClInclude Include="..\src\domain\ThemeRules.h" />`。

- [ ] **Step 4: 运行验证通过**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: 全绿（82→86 用例）。

- [ ] **Step 5: Commit**

```bash
git add src/data/NoteStore.h src/data/NoteStore.cpp src/domain/ThemeRules.h src/domain/ThemeRules.cpp tests/test_notestore.cpp tests/test_theme_rules.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(data/domain): theme read access + theme-id-only update + cycle rule (pure)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: 标题栏第 5 按钮（换色）——纯布局

**Files:**
- Modify: `src/ui/TitleBarLayout.h`, `src/ui/TitleBarLayout.cpp`
- Test: `tests/test_titlebar_layout.cpp`（追加/修改）

**Interfaces:**
- Produces（Task 4 消费）:
  - `own::TitleHit` 加枚举值 `Theme`。
  - `own::TitleBarRects` 加成员 `RectI themeBtn;` —— 位于 opacity 左侧（从右到左第 5 个）；`dragArea` 右界相应左移到 themeBtn 之前。
  - `hitTestTitleBar` 命中 themeBtn 返回 `TitleHit::Theme`。

- [ ] **Step 1: 写失败测试**

`tests/test_titlebar_layout.cpp` 末尾追加：
```cpp
TEST_CASE("theme button sits left of opacity and hit-tests") {
    auto r = mk();
    CHECK(r.themeBtn.w == 20);
    CHECK(r.themeBtn.x < r.opacityBtn.x);                       // 从右数第 5 个
    CHECK(r.dragArea.x + r.dragArea.w <= r.themeBtn.x);         // 拖动区不与按钮重叠
    CHECK(own::hitTestTitleBar(r, r.themeBtn.x+2, r.themeBtn.y+2) == TitleHit::Theme);
}
```

- [ ] **Step 2: 运行验证失败** — `themeBtn`/`Theme` 未定义 → 编译失败。

- [ ] **Step 3: 实现**

`src/ui/TitleBarLayout.h`：
```cpp
enum class TitleHit { None, Drag, Close, Pin, Roll, Opacity, Theme };
```
`TitleBarRects` 改为：
```cpp
struct TitleBarRects { RectI titleBar, closeBtn, pinBtn, rollBtn, opacityBtn, themeBtn, dragArea; };
```
`src/ui/TitleBarLayout.cpp` 的 `layoutTitleBar`：在 `place(r.opacityBtn);` 之后加 `place(r.themeBtn);`，并把 dragArea 行改为：
```cpp
    int dragRight = r.themeBtn.x - m.btnGap;            // 拖动区到最左钮之前
```
`hitTestTitleBar`：在 `Opacity` 检查后加：
```cpp
    if (inRect(r.themeBtn, px, py))   return TitleHit::Theme;
```

- [ ] **Step 4: 构建 + 全绿**（app 里 NoteWindow 尚未处理 Theme 命中——switch 有 default，不会编译失败）

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: 全绿（+1 用例）。

- [ ] **Step 5: Commit**

```bash
git add src/ui/TitleBarLayout.h src/ui/TitleBarLayout.cpp tests/test_titlebar_layout.cpp
git commit -m "feat(ui): title bar theme button slot (pure layout)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: 便签窗主题化（取色绘制 + 换色 + 内容视图跟色）

**Files:**
- Modify: `src/ui/INoteContentView.h`
- Modify: `src/ui/TextContentView.h/.cpp`, `src/ui/ChecklistContentView.h/.cpp`, `src/ui/DrawingContentView.h/.cpp`
- Modify: `src/ui/NoteWindow.h`, `src/ui/NoteWindow.cpp`

**Interfaces:**
- Consumes: Task 2 `allThemes/getTheme/updateNoteTheme/nextThemeId`；Task 3 `TitleHit::Theme`/`themeBtn`。
- Produces:
  - `INoteContentView` 加虚函数：`virtual void ApplyTheme(uint32_t bgRgb, uint32_t textRgb) {}`（默认空实现，参数 0xRRGGBB）。
  - 三个视图实现：Text=`EM_SETBKGNDCOLOR`+全文字色；Checklist/Drawing=记成员色重绘。
- 说明：GUI 任务，达标线=链接通过+启动存活；换色行为落 Task 7 冒烟。

- [ ] **Step 1: 接口 + 三视图**

`src/ui/INoteContentView.h` 类内加：
```cpp
    virtual void ApplyTheme(uint32_t bgRgb, uint32_t textRgb) {}   // 0xRRGGBB；默认忽略
```
`src/ui/TextContentView.h`：类内 public 加 `void ApplyTheme(uint32_t bgRgb, uint32_t textRgb) override;`，私有加 `uint32_t m_textRgb = 0x202020;`。
`src/ui/TextContentView.cpp` 末尾加（并在 `Load` 的 `applyNoteFont(m_edit, SCF_ALL);` 之后追加一行 `ApplyTheme(m_bgRgb, m_textRgb);` —— 为此把两色都记成员：头文件再加 `uint32_t m_bgRgb = 0xFFF7B0;`）：
```cpp
void CTextContentView::ApplyTheme(uint32_t bgRgb, uint32_t textRgb) {
    if (!m_created) return;
    m_bgRgb = bgRgb; m_textRgb = textRgb;
    COLORREF bg = RGB((bgRgb>>16)&0xFF, (bgRgb>>8)&0xFF, bgRgb&0xFF);
    ::SendMessage(m_edit.GetSafeHwnd(), EM_SETBKGNDCOLOR, 0, (LPARAM)bg);
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = RGB((textRgb>>16)&0xFF, (textRgb>>8)&0xFF, textRgb&0xFF);
    BOOL mod = m_edit.GetModify();
    ::SendMessage(m_edit.GetSafeHwnd(), EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    m_edit.SetModify(mod);   // 换色不算脏
}
```
`src/ui/ChecklistContentView.h`：public 加 `void ApplyTheme(uint32_t bgRgb, uint32_t textRgb) override;`，私有加 `uint32_t m_bgRgb = 0xFFF7B0; uint32_t m_textRgb = 0x202020;`。
`src/ui/ChecklistContentView.cpp`：`OnPaint` 里 `mem.FillSolidRect(&rc, RGB(0xFF,0xF7,0xB0));` 改为：
```cpp
    mem.FillSolidRect(&rc, RGB((m_bgRgb>>16)&0xFF, (m_bgRgb>>8)&0xFF, m_bgRgb&0xFF));
```
`mem.SetBkMode(TRANSPARENT);` 之后加：
```cpp
    mem.SetTextColor(RGB((m_textRgb>>16)&0xFF, (m_textRgb>>8)&0xFF, m_textRgb&0xFF));
```
文件末尾加：
```cpp
void CChecklistContentView::ApplyTheme(uint32_t bgRgb, uint32_t textRgb) {
    m_bgRgb = bgRgb; m_textRgb = textRgb;
    if (m_created) Invalidate(FALSE);
}
```
`src/ui/DrawingContentView.h`：public 加 `void ApplyTheme(uint32_t bgRgb, uint32_t textRgb) override;`，私有加 `uint32_t m_bgRgb = 0xFFF7B0;`。
`src/ui/DrawingContentView.cpp`：`g.Clear(Color(255, 0xFF, 0xF7, 0xB0));` 改为：
```cpp
        g.Clear(Color(255, (BYTE)((m_bgRgb>>16)&0xFF), (BYTE)((m_bgRgb>>8)&0xFF), (BYTE)(m_bgRgb&0xFF)));
```
文件末尾加：
```cpp
void CDrawingContentView::ApplyTheme(uint32_t bgRgb, uint32_t) {
    m_bgRgb = bgRgb;
    if (m_created) Invalidate(FALSE);
}
```
> 若 DrawingContentView 的成员创建标志名不是 `m_created`，以该文件实际为准。

- [ ] **Step 2: NoteWindow 主题取色 + 换色按钮**

`src/ui/NoteWindow.h`：include 区加 `#include "domain/Models.h"`（已有则略）；私有成员加：
```cpp
    own::Theme m_theme;          // 当前生效主题（含回落默认）
    void loadTheme();            // 按 m_note.themeId 取色，取不到回落内置黄
```
`src/ui/NoteWindow.cpp`：include 加 `#include "domain/ThemeRules.h"`。
`Create()` 里 `m_note = note; m_store = store;` 之后加 `loadTheme();`；`m_content->Load(m_note);` 之后加：
```cpp
        m_content->ApplyTheme(m_theme.bgColor, m_theme.textColor);
```
文件内加：
```cpp
void CNoteWindow::loadTheme() {
    m_theme = own::Theme{};                       // 默认即内置黄 {0xFFF7B0,0xF2D24A,0x202020}
    if (m_store && m_note.themeId != 0) {
        if (auto t = m_store->getTheme(m_note.themeId)) m_theme = *t;
    }
}
```
`OnPaint()` 里两处取色改主题：
```cpp
        SolidBrush bg(Color(255, (BYTE)((m_theme.bgColor>>16)&0xFF), (BYTE)((m_theme.bgColor>>8)&0xFF), (BYTE)(m_theme.bgColor&0xFF)));
```
```cpp
        SolidBrush title(Color(255, (BYTE)((m_theme.titleColor>>16)&0xFF), (BYTE)((m_theme.titleColor>>8)&0xFF), (BYTE)(m_theme.titleColor&0xFF)));
```
按钮符号区（`g.DrawEllipse(&pen, L.opacityBtn...` 之后）加换色按钮图形——三条竖色带示意调色板：
```cpp
        // 换色按钮：三竖条色板示意
        SolidBrush c1(Color(255,0xE0,0x60,0x60)), c2(Color(255,0x60,0xB0,0x60)), c3(Color(255,0x60,0x70,0xE0));
        int tw = (L.themeBtn.w - 8) / 3;
        g.FillRectangle(&c1, L.themeBtn.x+4,        L.themeBtn.y+4, tw, L.themeBtn.h-8);
        g.FillRectangle(&c2, L.themeBtn.x+4+tw,     L.themeBtn.y+4, tw, L.themeBtn.h-8);
        g.FillRectangle(&c3, L.themeBtn.x+4+tw*2,   L.themeBtn.y+4, tw, L.themeBtn.h-8);
```
`OnLButtonDown` 的 switch 里 `case own::TitleHit::Opacity:` 之后加：
```cpp
        case own::TitleHit::Theme: {
            if (!m_store) return;
            auto themes = m_store->allThemes();
            int64_t next = own::nextThemeId(themes, m_note.themeId);
            if (next == 0) return;
            m_note.themeId = next;
            m_store->updateNoteTheme(m_note.id, next);   // 只写 theme_id，不碰 blob
            loadTheme();
            if (m_content) m_content->ApplyTheme(m_theme.bgColor, m_theme.textColor);
            Invalidate(FALSE);
            return;
        }
```

- [ ] **Step 3: 构建 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist 2>/dev/null | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿（数量不变）；ALIVE。

- [ ] **Step 4: Commit**

```bash
git add src/ui/INoteContentView.h src/ui/TextContentView.h src/ui/TextContentView.cpp src/ui/ChecklistContentView.h src/ui/ChecklistContentView.cpp src/ui/DrawingContentView.h src/ui/DrawingContentView.cpp src/ui/NoteWindow.h src/ui/NoteWindow.cpp
git commit -m "feat(ui): note theming — themed paint, theme-cycle button, content views follow

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: 设置弹层（默认主题/透明度/自启）+ 托盘入口 + 新建默认值

**Files:**
- Create: `src/ui/SettingsDialog.h`, `src/ui/SettingsDialog.cpp`
- Modify: `src/app/AppHostWindow.h/.cpp`（托盘菜单 id 7 + `onOpenSettings` 回调）
- Modify: `src/app/NoteApp.cpp`（接线 + 新建默认值）
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 2 `allThemes/nextThemeId`；`SettingsStore`；`own_svc::autostart*`；`own_ui::uiFont`。
- Produces（Task 6 扩展同一对话框）:
  - `namespace own_ui { void showSettingsDialog(own::Database& db, own::NoteStore& store, HotkeyManager& hotkeys, HWND hotkeyHwnd); }`（Task 5 先不使用 hotkeys/hotkeyHwnd 参数，但签名一步到位，Task 6 填充改键区）。
  - settings 键：`default_theme_id`（int，0=未设）、`default_opacity`（int，默认 255）。
  - `CAppHostWindow` 加 `std::function<void()> onOpenSettings;`，托盘菜单在「开机自启」之后加 id=7 的「设置…」。
- 对话框行为：自绘窗口 360×300，行高 34；三行：`默认主题：<名>`（点击循环 allThemes + 写 `default_theme_id`）、`默认透明度：<100%/80%/60%/40%>`（点击循环 255→204→153→102 写 `default_opacity`）、`开机自启：<开/关>`（点击切换 autostartSetEnabled）。ESC/右上×退出。仿 `TextPrompt.cpp` 的 `CPromptWnd` 手写模态循环（栈对象 + `PostNcDestroy(){}` 空实现 + `EnableWindow` 禁父——无父窗时跳过禁用）。

- [ ] **Step 1: 写 SettingsDialog**

`src/ui/SettingsDialog.h`：
```cpp
#pragma once
#include <afxwin.h>
namespace own { class Database; class NoteStore; }
class HotkeyManager;
namespace own_ui {
// 自绘设置弹层：默认主题/默认透明度/开机自启（+P7 Task6 的热键改键区）。
// 行级点击即时生效；ESC/关闭退出。模态（手写消息循环）。
void showSettingsDialog(own::Database& db, own::NoteStore& store,
                        HotkeyManager& hotkeys, HWND hotkeyHwnd);
}
```
`src/ui/SettingsDialog.cpp`：
```cpp
#include "ui/SettingsDialog.h"
#include "ui/UiFont.h"
#include "data/SettingsStore.h"
#include "data/NoteStore.h"
#include "domain/ThemeRules.h"
#include "services/AutostartManager.h"
#include "services/HotkeyManager.h"
#include <string>
#include <vector>

namespace own_ui {

static CString u8ToWide(const std::string& s) {
    if (s.empty()) return CString();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CString w;
    if (n > 0) { ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.GetBuffer(n), n); w.ReleaseBuffer(n); }
    return w;
}

static const int kRowH = 34, kPad = 12, kWidth = 360;

class CSettingsWnd : public CWnd {
public:
    own::Database* db = nullptr;
    own::NoteStore* store = nullptr;
    HotkeyManager* hotkeys = nullptr;
    HWND hotkeyHwnd = nullptr;
    bool done = false;
    int rowCount() const { return 3; }               // Task 6 扩展为 3 + 热键行数
    CRect rowRect(int i) const { return CRect(kPad, kPad + i * kRowH, kWidth - kPad, kPad + (i + 1) * kRowH - 4); }

    BOOL PreTranslateMessage(MSG* m) override {
        if (m->message == WM_KEYDOWN && m->wParam == VK_ESCAPE) { done = true; return TRUE; }
        return CWnd::PreTranslateMessage(m);
    }
    void PostNcDestroy() override {}                  // 栈对象

    CString rowLabel(int i) {
        own::SettingsStore st(*db);
        if (i == 0) {                                 // 默认主题：<名>
            int64_t tid = st.getInt("default_theme_id", 0);
            CString name = _T("(\x672A\x8BBE)");      // (未设)
            if (tid != 0) { if (auto t = store->getTheme(tid)) name = u8ToWide(t->name); }
            return _T("\x9ED8\x8BA4\x4E3B\x9898\xFF1A") + name;                    // 默认主题：
        }
        if (i == 1) {                                 // 默认透明度：<n%>
            int op = st.getInt("default_opacity", 255);
            int pct = (op * 100 + 127) / 255;
            CString s; s.Format(_T("\x9ED8\x8BA4\x900F\x660E\x5EA6\xFF1A%d%%"), pct); // 默认透明度：
            return s;
        }
        bool on = own_svc::autostartIsEnabled();      // 开机自启：开/关
        return CString(_T("\x5F00\x673A\x81EA\x542F\xFF1A")) + (on ? _T("\x5F00") : _T("\x5173"));
    }
    void clickRow(int i) {
        own::SettingsStore st(*db);
        if (i == 0) {
            auto themes = store->allThemes();
            int64_t cur = st.getInt("default_theme_id", 0);
            int64_t next = own::nextThemeId(themes, cur);
            if (next != 0) st.setInt("default_theme_id", (int)next);
        } else if (i == 1) {
            static const int steps[] = { 255, 204, 153, 102 };
            int cur = st.getInt("default_opacity", 255);
            int idx = 0; for (int k = 0; k < 4; ++k) if (steps[k] == cur) { idx = k; break; }
            st.setInt("default_opacity", steps[(idx + 1) % 4]);
        } else if (i == 2) {
            own_svc::autostartSetEnabled(!own_svc::autostartIsEnabled());
        }
        Invalidate(FALSE);
    }

protected:
    afx_msg void OnPaint() {
        CPaintDC dc(this);
        CRect rc; GetClientRect(&rc);
        dc.FillSolidRect(rc, RGB(45, 45, 48));
        dc.SetBkMode(TRANSPARENT);
        CFont* old = dc.SelectObject(CFont::FromHandle(uiFont(16)));
        for (int i = 0; i < rowCount(); ++i) {
            CRect r = rowRect(i);
            dc.FillSolidRect(r, RGB(62, 62, 66));
            dc.Draw3dRect(r, RGB(90, 90, 96), RGB(30, 30, 32));
            dc.SetTextColor(RGB(0xE0, 0xE0, 0xE0));
            CRect tr = r; tr.left += 10;
            dc.DrawText(rowLabel(i), tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        dc.SelectObject(old);
    }
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    afx_msg void OnLButtonUp(UINT, CPoint pt) {
        for (int i = 0; i < rowCount(); ++i)
            if (rowRect(i).PtInRect(pt)) { clickRow(i); return; }
    }
    afx_msg void OnClose() { done = true; }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CSettingsWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

void showSettingsDialog(own::Database& db, own::NoteStore& store,
                        HotkeyManager& hotkeys, HWND hotkeyHwnd) {
    CSettingsWnd w;
    w.db = &db; w.store = &store; w.hotkeys = &hotkeys; w.hotkeyHwnd = hotkeyHwnd;
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW));
    int h = kPad * 2 + w.rowCount() * kRowH + 30;
    CRect r(0, 0, kWidth, h);
    r.OffsetRect(400, 260);
    w.CreateEx(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, cls,
               _T("\x8BBE\x7F6E"),                                   // 设置
               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, r, nullptr, 0);
    MSG msg;
    while (!w.done && ::GetMessage(&msg, nullptr, 0, 0)) {
        if (!w.PreTranslateMessage(&msg)) { ::TranslateMessage(&msg); ::DispatchMessage(&msg); }
    }
    w.DestroyWindow();
}

} // namespace own_ui
```

- [ ] **Step 2: 托盘入口 + 接线 + 新建默认值**

`src/app/AppHostWindow.h` 回调区加：
```cpp
    std::function<void()> onOpenSettings;        // 托盘「设置…」
```
`src/app/AppHostWindow.cpp` 的 `showTrayMenu()`：`menu.AppendMenu(autostartFlag, 5, ...)` 之后（退出分隔线之前）加：
```cpp
    menu.AppendMenu(MF_STRING, 7, _T("\x8BBE\x7F6E\x2026"));               // 设置…
```
switch 里加：
```cpp
        case 7: if (onOpenSettings) onOpenSettings(); break;
```
`src/app/NoteApp.cpp`：include 加 `#include "ui/SettingsDialog.h"`；在 `m_host.isAutostartEnabled = ...;` 之后加：
```cpp
    m_host.onOpenSettings = [this]{
        own_ui::showSettingsDialog(m_db, *m_store, m_hotkeys, m_host.GetSafeHwnd());
    };
```
三个新建 lambda（onNewNote/onNewChecklist/onNewDrawing）里，`insertNote` 之前各加（以 onNewNote 为例，另两处同样两行）：
```cpp
        own::SettingsStore st(m_db);
        n.themeId = st.getInt("default_theme_id", 0);
        n.opacity = st.getInt("default_opacity", 255);
```
（`NoteApp.cpp` 已 include `data/SettingsStore.h`。）
`app/open_windows_note_app.vcxproj`：ClCompile 加 `<ClCompile Include="..\src\ui\SettingsDialog.cpp" />`，ClInclude 加 `<ClInclude Include="..\src\ui\SettingsDialog.h" />`。

- [ ] **Step 3: 构建 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist 2>/dev/null | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿；ALIVE。

- [ ] **Step 4: Commit**

```bash
git add src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp src/app/AppHostWindow.h src/app/AppHostWindow.cpp src/app/NoteApp.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui/app): settings dialog (default theme/opacity/autostart) + tray entry + new-note defaults

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 6: 设置弹层——热键改键区（P5 遗留改键 UI）

**Files:**
- Modify: `src/ui/SettingsDialog.cpp`

**Interfaces:**
- Consumes: `HotkeyManager::bindings()`（`const std::vector<HkBinding>&`，`HkBinding{int id; std::string name; std::string defBinding; own::Hotkey hk; bool registered;}`）、`unregisterAll(HWND)`、`loadAndRegister(HWND, SettingsStore&)`；`own::parseHotkey/formatHotkey/findHotkeyConflicts`；`own_ui::promptText`。
- 行为：设置窗在 3 个通用行之后列出全部热键行 `名称：当前绑定`（当前绑定 = settings `hotkey.<name>` 覆盖值，缺省用 defBinding）。点击行 → `promptText` 预填当前绑定 → `parseHotkey` 校验（非法弹「热键格式无效」）→ 与其余绑定 `findHotkeyConflicts` 冲突检测（冲突弹「与其它热键冲突」）→ 合法则 `setString("hotkey."+name)` + `unregisterAll(hotkeyHwnd)` + `loadAndRegister(hotkeyHwnd, settings)` → 重绘。

- [ ] **Step 1: 实现**

`src/ui/SettingsDialog.cpp` 顶部 include 区加：
```cpp
#include "domain/Hotkey.h"
#include "ui/TextPrompt.h"
```
`CSettingsWnd` 内加显示名映射与热键行逻辑——把 `rowCount/rowLabel/clickRow` 按下面替换/扩展：
```cpp
    static CString hkDisplayName(const std::string& name) {
        if (name == "new")           return _T("\x70ED\x952E\xB7\x65B0\x5EFA\x4FBF\x7B7E");   // 热键·新建便签
        if (name == "new_checklist") return _T("\x70ED\x952E\xB7\x65B0\x5EFA\x6E05\x5355");   // 热键·新建清单
        if (name == "new_drawing")   return _T("\x70ED\x952E\xB7\x65B0\x5EFA\x6D82\x9E26");   // 热键·新建涂鸦
        if (name == "manager")       return _T("\x70ED\x952E\xB7\x7BA1\x7406\x5668");         // 热键·管理器
        if (name == "toggle_all")    return _T("\x70ED\x952E\xB7\x663E\x9690\x5168\x90E8");   // 热键·显隐全部
        if (name == "quit")          return _T("\x70ED\x952E\xB7\x9000\x51FA");               // 热键·退出
        return CString(name.c_str());
    }
    std::string bindingText(const HkBinding& b) {     // settings 覆盖优先
        own::SettingsStore st(*db);
        return st.getString("hotkey." + b.name, b.defBinding);
    }
    int rowCount() const { return 3 + (int)hotkeys->bindings().size(); }
```
`rowLabel(int i)`：原有 `i==0/1/2` 分支保留，末尾（自启分支之前判断）改为——`i>=3` 时：
```cpp
        if (i >= 3) {
            const auto& b = hotkeys->bindings()[i - 3];
            return hkDisplayName(b.name) + _T("\xFF1A") + CString(bindingText(b).c_str()); // ：
        }
```
`clickRow(int i)`：末尾加分支：
```cpp
        else if (i >= 3) {
            const auto& bs = hotkeys->bindings();
            const auto& b = bs[i - 3];
            CString io(bindingText(b).c_str());
            if (!own_ui::promptText(this, _T("\x8F93\x5165\x70ED\x952E (\x5982 Ctrl+Alt+N)"), io)) return; // 输入热键 (如 …)
            CStringA a(io);                                    // 热键串全 ASCII
            std::string s((LPCSTR)a);
            own::Hotkey parsed;
            if (!own::parseHotkey(s, parsed)) {
                AfxMessageBox(_T("\x70ED\x952E\x683C\x5F0F\x65E0\x6548"));            // 热键格式无效
                return;
            }
            std::vector<own::Hotkey> all;                       // 冲突检测：候选 + 其余现值
            all.push_back(parsed);
            own::SettingsStore st(*db);
            for (size_t k = 0; k < bs.size(); ++k) {
                if ((int)k == i - 3) continue;
                own::Hotkey other;
                if (own::parseHotkey(st.getString("hotkey." + bs[k].name, bs[k].defBinding), other))
                    all.push_back(other);
            }
            if (!own::findHotkeyConflicts(all).empty()) {
                AfxMessageBox(_T("\x4E0E\x5176\x5B83\x70ED\x952E\x51B2\x7A81"));      // 与其它热键冲突
                return;
            }
            st.setString("hotkey." + b.name, own::formatHotkey(parsed));   // 规范化写回
            hotkeys->unregisterAll(hotkeyHwnd);
            hotkeys->loadAndRegister(hotkeyHwnd, st);
            Invalidate(FALSE);
        }
```
> `promptText` 会 `EnableWindow(FALSE)` 禁用本窗再恢复——本窗即父窗，安全。改键行点击后立即生效并落库，无需重启。

- [ ] **Step 2: 构建 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist 2>/dev/null | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿；ALIVE。

- [ ] **Step 3: Commit**

```bash
git add src/ui/SettingsDialog.cpp
git commit -m "feat(ui): hotkey rebind rows in settings dialog — validate, conflict-check, live re-register

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 7: P7 手工冒烟清单

**Files:**
- Create: `docs/superpowers/smoke/P7-smoke-checklist.md`

- [ ] **Step 1: 写清单**

`docs/superpowers/smoke/P7-smoke-checklist.md`:
```markdown
# P7 手工冒烟清单（主题 + 设置弹层）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

## 便签主题
- [ ] 便签标题栏出现三竖条「换色」按钮（透明度圆圈左侧）
- [ ] 连点换色：背景/标题栏按 黄→粉→蓝→绿→黄 循环，正文/清单/涂鸦背景跟随
- [ ] 富文本便签换色后文字颜色正常、内容未丢失、未被标记为脏（不触发无谓保存）
- [ ] 换色后重启：颜色保持（theme_id 持久化）
- [ ] 换色便签的正文内容重启后完整（updateNoteTheme 不碰 blob 的回归检查）

## 设置弹层
- [ ] 托盘右键 →「设置…」弹出深色设置窗；ESC / 右上 × 关闭
- [ ] 「默认主题」点击循环 4 主题名；「默认透明度」循环 100%/80%/60%/40%；「开机自启」开↔关（启动夹 .lnk 同步出现/删除）
- [ ] 设默认主题=蓝、默认透明度=80% 后 Ctrl+Alt+N 新建：新便签蓝底、半透明；已开旧便签不受影响
- [ ] 重启后设置窗显示的默认值与上次一致（settings 持久化）

## 热键改键
- [ ] 设置窗列出 6 个热键行，显示当前绑定
- [ ] 点击「热键·新建便签」输入 Ctrl+Alt+J：立即生效（Ctrl+Alt+J 新建，Ctrl+Alt+N 失效），无需重启
- [ ] 重启后 Ctrl+Alt+J 仍生效（settings 持久化）
- [ ] 输入 `abc`：弹「热键格式无效」，原绑定不变
- [ ] 输入与「退出」相同的组合：弹「与其它热键冲突」，原绑定不变

## 卫生项回归
- [ ] 有提醒的便签 ⏰ 前缀仍正常显示（reload 改为一次查询后）
- [ ] 列表行数较多时滚动流畅无异常
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/smoke/P7-smoke-checklist.md
git commit -m "docs: P7 themes/settings manual smoke checklist

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage：**
- §1.3「背景色 + 多套配色主题」→ Task 2（数据）+ Task 3（按钮位）+ Task 4（绘制/循环/持久化）。✓
- §3「弹层：设置 / 主题选择（自绘 CWnd）」→ Task 5（自绘设置窗；主题选择=标题栏循环 + 默认主题行，v1 简化已声明）。✓
- §3 `CNoteWindow`「换色」按钮 → Task 3/4。✓
- §6「热键注册冲突→提示并允许改键」（P5 遗留）→ Task 6（格式/冲突校验 + 即时重注册）。✓
- §1.3「不透明度调节」默认值入设置 → Task 5（per-note 4 档循环已在 P2 有）。✓
- P6 最终审查遗留（doctest / KillTimer / 成员序注释 / reload N+1）→ Task 1；toast 泄漏与多屏定位显式后置（范围外声明）。✓
- 字体字号设置：范围外声明（随富文本工具条计划）。✓

**2. Placeholder scan：** 无 TBD/TODO；两处「以实际为准」均为既有代码事实核对指引（Statement 列读取方法名、DrawingContentView 创建标志名），并指明查证位置，非未决设计。

**3. Type consistency：**
- `allThemes()→vector<Theme>` / `getTheme(int64_t)→optional<Theme>` / `updateNoteTheme(int64_t,int64_t)→bool`（Task 2）↔ Task 4 `CNoteWindow`、Task 5 `rowLabel/clickRow` 调用。✓
- `nextThemeId(const vector<Theme>&, int64_t)→int64_t`（Task 2）↔ Task 4 换色、Task 5 默认主题循环。✓
- `TitleHit::Theme`/`themeBtn`（Task 3）↔ Task 4 `OnLButtonDown`/`OnPaint`。✓
- `ApplyTheme(uint32_t,uint32_t)`（Task 4 接口）↔ 三视图 override 与 `CNoteWindow` 两处调用。✓
- `showSettingsDialog(Database&, NoteStore&, HotkeyManager&, HWND)`（Task 5 签名）↔ Task 5 NoteApp 接线、Task 6 在同文件内扩展。✓
- `HkBinding.name/defBinding`、`bindings()`（P5 既有）↔ Task 6 使用。✓
- settings 键 `default_theme_id`/`default_opacity`（Task 5 写）↔ Task 5 新建 lambda 读，键名一致。✓

**已知限制（执行者须知）：** 设置窗/换色为 GUI 行为，自动化达标线=链接+启动存活，行为落 Task 7 冒烟。`CSettingsWnd` 每次 `rowLabel` 现查 settings/DB（无缓存）——弹层低频、行少，接受。改键立即重注册：若新键被系统占用，`RegisterHotKey` 失败仅 DebugView 日志（P5 语义），行仍显示新值——冒烟中可观察；更完善的失败回滚随后续打磨。`Statement` 列读取方法名与 `DrawingContentView` 成员名需按实际文件核对（有既有用法可抄）。
