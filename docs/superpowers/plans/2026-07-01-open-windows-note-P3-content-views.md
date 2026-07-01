# P3 便签内容视图（富文本 / 清单 / 涂鸦）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `CNoteWindow` 内容区从 P2 的占位文字升级为三种可编辑内容视图（富文本 RichEdit、自绘清单、自绘涂鸦），编辑内容防抖持久化回 SQLite。

**Architecture:** 沿用设计文档「可插拔内容视图」：`CNoteWindow` 宿主一个多态 `INoteContentView`（子窗口，铺在标题栏下方、四周留缩放边），按 `note.type` 由工厂选择实现。可测的纯逻辑（内容保存 SQL、搜索文本规整、清单模型/布局、涂鸦几何、内容区矩形）抽成无 HWND 函数走 doctest；窗口/控件行为走手工冒烟清单。富文本用内嵌 RichEdit（唯一非自绘控件），清单/涂鸦一律自绘。

**Tech Stack:** C++17 · MFC 静态链接 · Win32 · GDI+GDI+ · RichEdit（RichEdit20W / `AfxInitRichEdit2`）· SQLite（P1 数据层）· nlohmann/json（P1 已封装于 ChecklistJson/StrokesJson）· doctest。

## Global Constraints

- 语言/工具链：C++17（`/std:c++17`）、MFC **静态链接**（`<UseOfMfc>Static</UseOfMfc>`）、无 PCH、仅 `x64`。
- 编码：所有工程 ClCompile 带 `/utf-8`（根因修复 C4819）；`.cpp` 内中文字面量可直接写；**测试文件里的中文断言用 UTF-8 十六进制转义**（如 `"\xE9\xBB\x84"`）以防编辑器编码问题。
- 命名空间：`src/data`、`src/domain` 及 `src/ui` 下的**纯逻辑**一律 `namespace own`；这些纯逻辑**不得** include `<afxwin.h>`/`<windows.h>`/`<gdiplus.h>`，以便进 tests 工程用 doctest 单测。
- 渲染：GDI + GDI+；**一律自绘**，唯一例外是富文本编辑用的 RichEdit 控件。
- 内容 blob 约定（设计 §4）：富文本=RTF 字节流；清单=`[{"text","checked","order"}]` JSON；涂鸦=`{"strokes":[{"color","width","points":[[x,y],...]}]}` JSON。三者都同步维护 `plain_text`（小写纯文本；涂鸦为空）。
- 容错（设计 §6）：RTF 解析失败→回落纯文本；清单/涂鸦解析失败→当空处理但**保留原始 blob，不得用空覆盖**（实现为“仅在 `IsDirty()` 时才落盘”，未编辑的坏 blob 永不被覆盖）。
- 构建：只能通过 `.sln` 构建（`.vcxproj` 直建会让 `$(SolutionDir)` 解析错）。MSBuild 路径：`"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`（下称 `$MSB`）。
- **每次重建前先杀残留进程**：`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`（单实例 exe 会锁定输出文件导致 LNK1168）。
- 自动化达标线：GUI 任务以「app 工程构建链接通过 + `x64/Debug/tests.exe` 全绿」为准；窗口/控件交互落到 Task 11 手工冒烟清单。
- 每次提交末尾附：`Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`。
- 分支：在 `feat/p2-note-window` 之上继续（P2 尚未合并 main；P3 承接其 `CNoteWindow`），或按执行者选择新建 `feat/p3-content-views` 从 P2 分支切出。执行前确认当前分支非 `main`。

**承接的既有接口（P1/P2，勿重复实现）：**
- P1 领域：`own::Note`（字段 `id/type/contentBlob(std::vector<uint8_t>)/plainText/rect/opacity/...`）、`own::NoteType{RichText=0,Checklist=1,Drawing=2}`、`own::ChecklistItem{std::string text; bool checked; int order;}`、`own::Stroke{uint32_t color; int width; std::vector<std::pair<int,int>> points;}`、`own::RectI{int x,y,w,h;}`。
- P1 序列化：`std::vector<uint8_t> own::serializeChecklist(const std::vector<ChecklistItem>&)`、`std::vector<ChecklistItem> own::deserializeChecklist(const std::vector<uint8_t>&)`、`std::string own::checklistPlainText(const std::vector<ChecklistItem>&)`、`std::vector<uint8_t> own::serializeStrokes(const std::vector<Stroke>&)`、`std::vector<Stroke> own::deserializeStrokes(const std::vector<uint8_t>&)`。
- P1 数据：`own::NoteStore`（持 `Database&`）已有 `insertNote/getNote/updateNote/updateGeometry/updateFlags/query`；本计划 Task 1 新增 `updateContent`。`own::Statement`（`bind/bindBlob/bindNull/step/execDone/columnInt64/columnText/columnBlob`）。
- P2 UI 纯逻辑：`own::TitleBarMetrics/TitleBarRects/layoutTitleBar/hitTestTitleBar`（`kTitleMetrics{28,20,4,4}`）、`own::ResizeEdge/hitTestResizeEdge/applyResize`。
- P2 窗口：`class CNoteWindow : public CWnd`，`bool Create(const own::Note&, own::NoteStore*)`；消息处理 `OnPaint/OnEraseBkgnd/OnLButtonDown/OnMouseMove/OnLButtonUp/OnSetCursor`；私有 `own::Note m_note; own::NoteStore* m_store;`。P2 里内容区仅画占位文字，本计划替换为宿主内容视图。缩放边距硬编码为 `6`。
- P2 应用：`CNoteApp`（`own::Database m_db; std::unique_ptr<own::NoteStore> m_store; CAppHostWindow m_host; std::vector<std::unique_ptr<CNoteWindow>> m_notes;`，`createAndShowNote(const own::Note&)`），`CAppHostWindow`（`static const UINT kHotkeyQuit=1,kHotkeyNew=2; std::function<void()> onNewNote,onQuit;`）。

---

## 文件结构（本计划新增/修改）

**新增（纯逻辑，无 HWND，进 tests + app 两个工程）：**
- `src/domain/SearchText.{h,cpp}` — `searchNormalize`（派生 plain_text 用）。
- `src/domain/ChecklistModel.{h,cpp}` — 清单条目的增删改排序（作用于 `std::vector<ChecklistItem>`）。
- `src/ui/ChecklistLayout.{h,cpp}` — 清单行/勾选框布局与命中测试（纯）。
- `src/ui/DrawingMath.{h,cpp}` — 点到线段距离、笔迹命中（橡皮擦用，纯）。
- `src/ui/ContentLayout.{h,cpp}` — `noteContentRect`（内容区矩形，纯）。

**新增（UI，含 HWND/RichEdit/GDI+，仅进 app 工程）：**
- `src/ui/INoteContentView.h` — 内容视图接口。
- `src/ui/ContentViewFactory.{h,cpp}` — 按 `NoteType` 造视图。
- `src/ui/TextContentView.{h,cpp}` — 富文本（RichEdit 宿主，RTF 读写）。
- `src/ui/ChecklistContentView.{h,cpp}` — 自绘清单（就地 CEdit 编辑）。
- `src/ui/DrawingContentView.{h,cpp}` — 自绘 GDI+ 涂鸦画布。

**修改：**
- `src/data/NoteStore.{h,cpp}` — 加 `updateContent`。
- `src/ui/NoteWindow.{h,cpp}` — 宿主内容视图 + 防抖保存 + 尺寸联动 + 卷起隐藏内容。
- `src/app/NoteApp.{h,cpp}`、`src/app/AppHostWindow.{h,cpp}` — 调 `AfxInitRichEdit2`；临时开发热键新建清单/涂鸦 note（供冒烟；P4 管理器替代）。
- `tests/tests.vcxproj`、`app/open_windows_note_app.vcxproj` — 登记新文件。
- `docs/superpowers/smoke/P3-smoke-checklist.md` — 新增手工冒烟清单。

**范围外（后续计划）：** 富文本可视化格式工具条（P3 用 RichEdit 原生 Ctrl+B/I/U 提供加粗/斜体/下划线，可视工具条留待 P4）；主题着色切换（P4）；管理器正式的新建/类型选择（P4，本计划用临时热键替代）；托盘与正式全局热键（P5）。

---

### Task 1: NoteStore::updateContent（内容落盘，数据层）

**Files:**
- Modify: `src/data/NoteStore.h`（在 `updateFlags` 声明后加一行）、`src/data/NoteStore.cpp`
- Test: `tests/test_notestore_content.cpp`

**Interfaces:**
- Consumes: P1 `Database`/`Statement`/`insertNote`/`getNote`。
- Produces: `bool own::NoteStore::updateContent(int64_t id, const std::vector<uint8_t>& blob, const std::string& plainText, int64_t now);` —— 只更新 `content_blob`/`plain_text`/`updated_at`。

- [ ] **Step 1: 写失败测试**

`tests/test_notestore_content.cpp`:
```cpp
#include "doctest.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include "data/NoteStore.h"

static own::Database freshDb() {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    return db;
}
TEST_CASE("updateContent rewrites blob/plain_text/updated_at only") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note n; n.plainText = "old"; n.contentBlob = {1,2,3};
    int64_t id = store.insertNote(n, 1000);
    std::vector<uint8_t> blob = {9,8,7,6};
    CHECK(store.updateContent(id, blob, "new text", 2000));
    auto got = store.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->contentBlob == blob);
    CHECK(got->plainText == "new text");
    CHECK(got->updatedAt == 2000);
    CHECK(got->rect.w == n.rect.w);   // 其它字段不动
}
TEST_CASE("updateContent accepts empty blob") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note n; int64_t id = store.insertNote(n, 1000);
    std::vector<uint8_t> empty;
    CHECK(store.updateContent(id, empty, "", 3000));
    auto got = store.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->contentBlob.empty());
    CHECK(got->plainText == "");
}
```
加入 tests 工程（`test_notestore_content.cpp`；NoteStore.cpp 已在 tests 工程）。

- [ ] **Step 2: 运行验证失败**

Run:
```bash
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -5
```
Expected: 编译失败（`updateContent` 未声明）。

- [ ] **Step 3: 实现**

`src/data/NoteStore.h`，在 `bool updateFlags(...);` 之后加：
```cpp
    bool updateContent(int64_t id, const std::vector<uint8_t>& blob, const std::string& plainText, int64_t now);
```
`src/data/NoteStore.cpp`，在 `updateFlags` 实现之后加：
```cpp
bool NoteStore::updateContent(int64_t id, const std::vector<uint8_t>& blob,
                              const std::string& plainText, int64_t now) {
    Statement s(db_, "UPDATE notes SET content_blob=?,plain_text=?,updated_at=? WHERE id=?;");
    s.bindBlob(1, blob.data(), blob.size());
    s.bind(2, plainText);
    s.bind(3, now);
    s.bind(4, id);
    s.execDone();
    return true;
}
```

- [ ] **Step 4: 运行验证通过**

Run:
```bash
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -4
```
Expected: 构建通过；tests 全绿（新增 2 个用例）。

- [ ] **Step 5: Commit**

```bash
git add src/data/NoteStore.h src/data/NoteStore.cpp tests/test_notestore_content.cpp tests/tests.vcxproj
git commit -m "feat(data): NoteStore::updateContent for content-only persistence

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: searchNormalize（派生搜索用纯文本，领域层）

**Files:**
- Create: `src/domain/SearchText.h`, `src/domain/SearchText.cpp`
- Test: `tests/test_search_text.cpp`

**Interfaces:**
- Produces: `std::string own::searchNormalize(const std::string& s);` —— 把 ASCII 大写转小写、裁掉首尾空白、把内部连续空白（空格/Tab/换行）折叠为单个空格；非 ASCII 字节（如中文 UTF-8）原样保留。内容视图 Save 时用它从控件纯文本派生 `plain_text`。

- [ ] **Step 1: 写失败测试**

`tests/test_search_text.cpp`:
```cpp
#include "doctest.h"
#include "domain/SearchText.h"
TEST_CASE("searchNormalize lowercases ascii and trims/collapses whitespace") {
    CHECK(own::searchNormalize("  Hello   World \n") == "hello world");
    CHECK(own::searchNormalize("A\tB\r\nC") == "a b c");
    CHECK(own::searchNormalize("") == "");
    CHECK(own::searchNormalize("   ") == "");
}
TEST_CASE("searchNormalize preserves non-ascii bytes") {
    // "黄 Note" -> 中文原样，ASCII 小写；内部空白折叠
    std::string in = "\xE9\xBB\x84  Note";
    CHECK(own::searchNormalize(in) == "\xE9\xBB\x84 note");
}
```
加入 tests 工程（`test_search_text.cpp` + `..\src\domain\SearchText.cpp`）。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败（`domain/SearchText.h` 不存在）。

- [ ] **Step 3: 实现**

`src/domain/SearchText.h`:
```cpp
#pragma once
#include <string>
namespace own {
std::string searchNormalize(const std::string& s);
}
```
`src/domain/SearchText.cpp`:
```cpp
#include "domain/SearchText.h"
namespace own {
static bool isAsciiSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}
std::string searchNormalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool pendingSpace = false;
    for (unsigned char c : s) {
        if (isAsciiSpace(c)) {
            if (!out.empty()) pendingSpace = true;   // 首部空白直接丢
            continue;
        }
        if (pendingSpace) { out.push_back(' '); pendingSpace = false; }
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');   // 仅 ASCII 转小写
        out.push_back((char)c);
    }
    return out;   // 尾部空白因 pendingSpace 未提交而天然裁掉
}
}
```

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -4`
Expected: 构建通过，tests 全绿。

- [ ] **Step 5: Commit**

```bash
git add src/domain/SearchText.h src/domain/SearchText.cpp tests/test_search_text.cpp tests/tests.vcxproj
git commit -m "feat(domain): searchNormalize for plain_text derivation

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: ChecklistModel（清单条目增删改排序，领域层纯逻辑）

**Files:**
- Create: `src/domain/ChecklistModel.h`, `src/domain/ChecklistModel.cpp`
- Test: `tests/test_checklist_model.cpp`

**Interfaces:**
- Consumes: `own::ChecklistItem`（P1 Models.h）。
- Produces（均作用于 `std::vector<ChecklistItem>&`，改后重排 `order` 为 0..n-1）：
  - `void own::checklistToggle(std::vector<ChecklistItem>& items, size_t i);`
  - `void own::checklistAdd(std::vector<ChecklistItem>& items, const std::string& text);`
  - `void own::checklistRemoveAt(std::vector<ChecklistItem>& items, size_t i);`
  - `void own::checklistMove(std::vector<ChecklistItem>& items, size_t from, size_t to);`
  - `void own::checklistSetText(std::vector<ChecklistItem>& items, size_t i, const std::string& text);`
- 约定：下标越界的调用安全无操作；`order` 始终与向量下标一致。

- [ ] **Step 1: 写失败测试**

`tests/test_checklist_model.cpp`:
```cpp
#include "doctest.h"
#include "domain/ChecklistModel.h"
using own::ChecklistItem;
static std::vector<ChecklistItem> mk3() {
    std::vector<ChecklistItem> v;
    own::checklistAdd(v, "a"); own::checklistAdd(v, "b"); own::checklistAdd(v, "c");
    return v;
}
TEST_CASE("add appends with contiguous order") {
    auto v = mk3();
    REQUIRE(v.size() == 3);
    CHECK(v[0].text == "a"); CHECK(v[0].order == 0);
    CHECK(v[2].text == "c"); CHECK(v[2].order == 2);
    CHECK(v[1].checked == false);
}
TEST_CASE("toggle flips only the target") {
    auto v = mk3();
    own::checklistToggle(v, 1);
    CHECK(v[1].checked == true);
    CHECK(v[0].checked == false);
    own::checklistToggle(v, 1);
    CHECK(v[1].checked == false);
}
TEST_CASE("removeAt renumbers order") {
    auto v = mk3();
    own::checklistRemoveAt(v, 0);
    REQUIRE(v.size() == 2);
    CHECK(v[0].text == "b"); CHECK(v[0].order == 0);
    CHECK(v[1].text == "c"); CHECK(v[1].order == 1);
}
TEST_CASE("move reorders and renumbers") {
    auto v = mk3();
    own::checklistMove(v, 2, 0);          // c 提到最前
    CHECK(v[0].text == "c"); CHECK(v[0].order == 0);
    CHECK(v[1].text == "a"); CHECK(v[1].order == 1);
    CHECK(v[2].text == "b"); CHECK(v[2].order == 2);
}
TEST_CASE("out-of-range calls are no-ops") {
    auto v = mk3();
    own::checklistToggle(v, 99);
    own::checklistRemoveAt(v, 99);
    own::checklistMove(v, 0, 99);
    own::checklistSetText(v, 99, "x");
    CHECK(v.size() == 3);
    CHECK(v[0].text == "a");
}
TEST_CASE("setText edits target") {
    auto v = mk3();
    own::checklistSetText(v, 1, "bb");
    CHECK(v[1].text == "bb");
}
```
加入 tests 工程（`test_checklist_model.cpp` + `..\src\domain\ChecklistModel.cpp`）。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败（`domain/ChecklistModel.h` 不存在）。

- [ ] **Step 3: 实现**

`src/domain/ChecklistModel.h`:
```cpp
#pragma once
#include <vector>
#include <string>
#include "domain/Models.h"
namespace own {
void checklistToggle(std::vector<ChecklistItem>& items, size_t i);
void checklistAdd(std::vector<ChecklistItem>& items, const std::string& text);
void checklistRemoveAt(std::vector<ChecklistItem>& items, size_t i);
void checklistMove(std::vector<ChecklistItem>& items, size_t from, size_t to);
void checklistSetText(std::vector<ChecklistItem>& items, size_t i, const std::string& text);
}
```
`src/domain/ChecklistModel.cpp`:
```cpp
#include "domain/ChecklistModel.h"
namespace own {
static void renumber(std::vector<ChecklistItem>& items) {
    for (size_t i = 0; i < items.size(); ++i) items[i].order = (int)i;
}
void checklistToggle(std::vector<ChecklistItem>& items, size_t i) {
    if (i < items.size()) items[i].checked = !items[i].checked;
}
void checklistAdd(std::vector<ChecklistItem>& items, const std::string& text) {
    ChecklistItem it; it.text = text; it.checked = false; it.order = (int)items.size();
    items.push_back(it);
}
void checklistRemoveAt(std::vector<ChecklistItem>& items, size_t i) {
    if (i >= items.size()) return;
    items.erase(items.begin() + i);
    renumber(items);
}
void checklistMove(std::vector<ChecklistItem>& items, size_t from, size_t to) {
    if (from >= items.size() || to >= items.size() || from == to) return;
    ChecklistItem tmp = items[from];
    items.erase(items.begin() + from);
    items.insert(items.begin() + to, tmp);
    renumber(items);
}
void checklistSetText(std::vector<ChecklistItem>& items, size_t i, const std::string& text) {
    if (i < items.size()) items[i].text = text;
}
}
```

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -4`
Expected: 构建通过，tests 全绿。

- [ ] **Step 5: Commit**

```bash
git add src/domain/ChecklistModel.h src/domain/ChecklistModel.cpp tests/test_checklist_model.cpp tests/tests.vcxproj
git commit -m "feat(domain): ChecklistModel edit ops (toggle/add/remove/move/setText)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: ChecklistLayout（清单布局与命中测试，UI 纯逻辑）

**Files:**
- Create: `src/ui/ChecklistLayout.h`, `src/ui/ChecklistLayout.cpp`
- Test: `tests/test_checklist_layout.cpp`

**Interfaces:**
- Consumes: `own::RectI`。
- Produces:
  - `struct own::ChecklistMetrics { int rowHeight; int boxSize; int pad; };`
  - `enum class own::ChecklistHit { None, Checkbox, Text, AddRow };`
  - `struct own::ChecklistHitResult { ChecklistHit kind; int index; };`（`index` = 条目下标；非条目命中为 -1）
  - `RectI own::checklistRowRect(RectI content, ChecklistMetrics m, int index);` —— 第 `index` 行（从 `content` 顶部起，每行高 `rowHeight`），横向铺满 content 宽。
  - `RectI own::checklistBoxRect(RectI content, ChecklistMetrics m, int index);` —— 该行左侧的勾选方框（左留 `pad`，上下居中，边长 `boxSize`）。
  - `ChecklistHitResult own::checklistHitTest(RectI content, ChecklistMetrics m, int itemCount, int px, int py);` —— 命中第 `itemCount` 行（紧邻最后一条之下）→ `AddRow`；命中已有行且点在 box 内→`Checkbox`，否则→`Text`；content 外或更下方→`None`。
- 约定：坐标为 content 相对的同坐标系（与命中点同系）；矩形内判定 `x<=px<x+w && y<=py<y+h`。

- [ ] **Step 1: 写失败测试**

`tests/test_checklist_layout.cpp`:
```cpp
#include "doctest.h"
#include "ui/ChecklistLayout.h"
using own::ChecklistHit;
static own::ChecklistMetrics M{ 24, 16, 4 };   // rowHeight,boxSize,pad
static own::RectI C{ 0, 0, 200, 300 };
TEST_CASE("row rect stacks by rowHeight and spans width") {
    auto r0 = own::checklistRowRect(C, M, 0);
    CHECK(r0.x == 0); CHECK(r0.y == 0); CHECK(r0.w == 200); CHECK(r0.h == 24);
    auto r2 = own::checklistRowRect(C, M, 2);
    CHECK(r2.y == 48);
}
TEST_CASE("box rect sits at left, vertically centered, boxSize square") {
    auto b = own::checklistBoxRect(C, M, 0);
    CHECK(b.x == 4);                       // pad
    CHECK(b.w == 16); CHECK(b.h == 16);
    CHECK(b.y == (24 - 16) / 2);           // 居中 -> 4
}
TEST_CASE("hit test: checkbox / text / add-row / none") {
    // 3 条目：第 0 行 box 命中
    auto hb = own::checklistHitTest(C, M, 3, 8, 12);
    CHECK(hb.kind == ChecklistHit::Checkbox); CHECK(hb.index == 0);
    // 第 1 行文本区（x 越过 box）
    auto ht = own::checklistHitTest(C, M, 3, 120, 24 + 12);
    CHECK(ht.kind == ChecklistHit::Text); CHECK(ht.index == 1);
    // 第 3 行（itemCount 行）= 新增行
    auto ha = own::checklistHitTest(C, M, 3, 50, 3 * 24 + 5);
    CHECK(ha.kind == ChecklistHit::AddRow); CHECK(ha.index == -1);
    // 再往下 = None
    auto hn = own::checklistHitTest(C, M, 3, 50, 10 * 24);
    CHECK(hn.kind == ChecklistHit::None);
}
```
加入 tests 工程（`test_checklist_layout.cpp` + `..\src\ui\ChecklistLayout.cpp`）。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败（`ui/ChecklistLayout.h` 不存在）。

- [ ] **Step 3: 实现**

`src/ui/ChecklistLayout.h`:
```cpp
#pragma once
#include "domain/Models.h"
namespace own {
struct ChecklistMetrics { int rowHeight; int boxSize; int pad; };
enum class ChecklistHit { None, Checkbox, Text, AddRow };
struct ChecklistHitResult { ChecklistHit kind; int index; };
RectI checklistRowRect(RectI content, ChecklistMetrics m, int index);
RectI checklistBoxRect(RectI content, ChecklistMetrics m, int index);
ChecklistHitResult checklistHitTest(RectI content, ChecklistMetrics m, int itemCount, int px, int py);
}
```
`src/ui/ChecklistLayout.cpp`:
```cpp
#include "ui/ChecklistLayout.h"
namespace own {
RectI checklistRowRect(RectI c, ChecklistMetrics m, int index) {
    return { c.x, c.y + index * m.rowHeight, c.w, m.rowHeight };
}
RectI checklistBoxRect(RectI c, ChecklistMetrics m, int index) {
    RectI row = checklistRowRect(c, m, index);
    int y = row.y + (m.rowHeight - m.boxSize) / 2;
    return { c.x + m.pad, y, m.boxSize, m.boxSize };
}
ChecklistHitResult checklistHitTest(RectI c, ChecklistMetrics m, int itemCount, int px, int py) {
    if (px < c.x || px >= c.x + c.w || py < c.y) return { ChecklistHit::None, -1 };
    int row = (py - c.y) / m.rowHeight;
    if (row < 0) return { ChecklistHit::None, -1 };
    if (row == itemCount) return { ChecklistHit::AddRow, -1 };
    if (row > itemCount) return { ChecklistHit::None, -1 };
    RectI box = checklistBoxRect(c, m, row);
    if (px >= box.x && px < box.x + box.w && py >= box.y && py < box.y + box.h)
        return { ChecklistHit::Checkbox, row };
    return { ChecklistHit::Text, row };
}
}
```

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -4`
Expected: 构建通过，tests 全绿。

- [ ] **Step 5: Commit**

```bash
git add src/ui/ChecklistLayout.h src/ui/ChecklistLayout.cpp tests/test_checklist_layout.cpp tests/tests.vcxproj
git commit -m "feat(ui): checklist layout + hit test (pure)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: DrawingMath（涂鸦几何，UI 纯逻辑）

**Files:**
- Create: `src/ui/DrawingMath.h`, `src/ui/DrawingMath.cpp`
- Test: `tests/test_drawing_math.cpp`

**Interfaces:**
- Consumes: `own::Stroke`（P1 Models.h，`points` 为 `std::vector<std::pair<int,int>>`）。
- Produces:
  - `double own::pointSegmentDistance(double px, double py, double ax, double ay, double bx, double by);` —— 点到线段（非直线）最短距离。
  - `int own::strokeHitTest(const std::vector<Stroke>& strokes, int px, int py, double tol);` —— 从后往前（后画的在上）找首个「任一线段距点 ≤ tol」的笔迹，返回其下标；无则 -1。单点笔迹按点距处理。
- 约定：橡皮擦用 `strokeHitTest` 选中整条删除（矢量橡皮，v1 不做局部擦除）。

- [ ] **Step 1: 写失败测试**

`tests/test_drawing_math.cpp`:
```cpp
#include "doctest.h"
#include "ui/DrawingMath.h"
#include "doctest.h"
using own::Stroke;
TEST_CASE("point-segment distance: perpendicular and endpoint clamp") {
    CHECK(own::pointSegmentDistance(5, 3, 0, 0, 10, 0) == doctest::Approx(3.0));
    CHECK(own::pointSegmentDistance(-5, 0, 0, 0, 10, 0) == doctest::Approx(5.0)); // 端点外
    CHECK(own::pointSegmentDistance(2, 0, 0, 0, 10, 0) == doctest::Approx(0.0));
}
TEST_CASE("strokeHitTest picks topmost within tolerance, else -1") {
    Stroke a; a.points = {{0,0},{10,0}};     // 下标 0
    Stroke b; b.points = {{0,50},{10,50}};   // 下标 1（后画，更靠上）
    std::vector<Stroke> s = { a, b };
    CHECK(own::strokeHitTest(s, 5, 2, 4.0) == 0);
    CHECK(own::strokeHitTest(s, 5, 48, 4.0) == 1);
    CHECK(own::strokeHitTest(s, 5, 25, 4.0) == -1);   // 都不在容差内
}
TEST_CASE("strokeHitTest topmost wins when overlapping") {
    Stroke a; a.points = {{0,0},{10,0}};
    Stroke b; b.points = {{0,0},{10,0}};
    std::vector<Stroke> s = { a, b };
    CHECK(own::strokeHitTest(s, 5, 0, 2.0) == 1);      // 取后画的
}
TEST_CASE("single-point stroke uses point distance") {
    Stroke a; a.points = {{5,5}};
    std::vector<Stroke> s = { a };
    CHECK(own::strokeHitTest(s, 6, 5, 2.0) == 0);
    CHECK(own::strokeHitTest(s, 20, 20, 2.0) == -1);
}
```
加入 tests 工程（`test_drawing_math.cpp` + `..\src\ui\DrawingMath.cpp`）。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败（`ui/DrawingMath.h` 不存在）。

- [ ] **Step 3: 实现**

`src/ui/DrawingMath.h`:
```cpp
#pragma once
#include <vector>
#include "domain/Models.h"
namespace own {
double pointSegmentDistance(double px, double py, double ax, double ay, double bx, double by);
int strokeHitTest(const std::vector<Stroke>& strokes, int px, int py, double tol);
}
```
`src/ui/DrawingMath.cpp`:
```cpp
#include "ui/DrawingMath.h"
#include <cmath>
namespace own {
double pointSegmentDistance(double px, double py, double ax, double ay, double bx, double by) {
    double dx = bx - ax, dy = by - ay;
    double len2 = dx*dx + dy*dy;
    double t = 0.0;
    if (len2 > 0.0) {
        t = ((px - ax) * dx + (py - ay) * dy) / len2;
        if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
    }
    double cx = ax + t*dx, cy = ay + t*dy;
    double ex = px - cx, ey = py - cy;
    return std::sqrt(ex*ex + ey*ey);
}
int strokeHitTest(const std::vector<Stroke>& strokes, int px, int py, double tol) {
    for (int i = (int)strokes.size() - 1; i >= 0; --i) {   // 后画的在上
        const auto& pts = strokes[i].points;
        if (pts.empty()) continue;
        if (pts.size() == 1) {
            double ex = px - pts[0].first, ey = py - pts[0].second;
            if (std::sqrt(ex*ex + ey*ey) <= tol) return i;
            continue;
        }
        for (size_t j = 0; j + 1 < pts.size(); ++j) {
            double d = pointSegmentDistance(px, py,
                pts[j].first, pts[j].second, pts[j+1].first, pts[j+1].second);
            if (d <= tol) return i;
        }
    }
    return -1;
}
}
```

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -4`
Expected: 构建通过，tests 全绿。

- [ ] **Step 5: Commit**

```bash
git add src/ui/DrawingMath.h src/ui/DrawingMath.cpp tests/test_drawing_math.cpp tests/tests.vcxproj
git commit -m "feat(ui): drawing math — point-segment distance + stroke hit test (pure)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: noteContentRect（内容区矩形，UI 纯逻辑）

**Files:**
- Create: `src/ui/ContentLayout.h`, `src/ui/ContentLayout.cpp`
- Test: `tests/test_content_layout.cpp`

**Interfaces:**
- Consumes: `own::RectI`。
- Produces: `RectI own::noteContentRect(RectI client, int titleHeight, int resizeMargin);` —— 内容区 = 标题栏之下、左右下各留 `resizeMargin`（给缩放抓手让路）；宽/高不足则钳为 0。
- 约定：`x=client.x+resizeMargin`、`y=client.y+titleHeight`、`w=client.w-2*resizeMargin`、`h=client.h-titleHeight-resizeMargin`。

- [ ] **Step 1: 写失败测试**

`tests/test_content_layout.cpp`:
```cpp
#include "doctest.h"
#include "ui/ContentLayout.h"
TEST_CASE("content rect sits below title bar, inset by resize margin") {
    auto r = own::noteContentRect(own::RectI{0,0,240,200}, 28, 6);
    CHECK(r.x == 6); CHECK(r.y == 28);
    CHECK(r.w == 228);                 // 240 - 12
    CHECK(r.h == 200 - 28 - 6);        // 166
}
TEST_CASE("degenerate sizes clamp to zero") {
    auto r = own::noteContentRect(own::RectI{0,0,4,20}, 28, 6);
    CHECK(r.w == 0);
    CHECK(r.h == 0);
}
```
加入 tests 工程（`test_content_layout.cpp` + `..\src\ui\ContentLayout.cpp`）。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败（`ui/ContentLayout.h` 不存在）。

- [ ] **Step 3: 实现**

`src/ui/ContentLayout.h`:
```cpp
#pragma once
#include "domain/Models.h"
namespace own {
RectI noteContentRect(RectI client, int titleHeight, int resizeMargin);
}
```
`src/ui/ContentLayout.cpp`:
```cpp
#include "ui/ContentLayout.h"
namespace own {
RectI noteContentRect(RectI client, int titleHeight, int resizeMargin) {
    int x = client.x + resizeMargin;
    int y = client.y + titleHeight;
    int w = client.w - 2 * resizeMargin;
    int h = client.h - titleHeight - resizeMargin;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    return { x, y, w, h };
}
}
```

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -4`
Expected: 构建通过，tests 全绿。

- [ ] **Step 5: Commit**

```bash
git add src/ui/ContentLayout.h src/ui/ContentLayout.cpp tests/test_content_layout.cpp tests/tests.vcxproj
git commit -m "feat(ui): noteContentRect helper (pure)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: INoteContentView 接口 + TextContentView（富文本 RichEdit）

**Files:**
- Create: `src/ui/INoteContentView.h`
- Create: `src/ui/TextContentView.h`, `src/ui/TextContentView.cpp`
- Modify: `app/open_windows_note_app.vcxproj`（登记新 .cpp；本任务纯逻辑 .cpp 也一并补进 app 工程：SearchText/ChecklistModel/ChecklistLayout/DrawingMath/ContentLayout）

**Interfaces:**
- Consumes: P1 `own::Note`；Task 2 `own::searchNormalize`。
- Produces:
  - 接口：
    ```cpp
    class INoteContentView {
    public:
        virtual ~INoteContentView() {}
        virtual bool Create(CWnd* parent, const CRect& rc) = 0;   // 建子控件
        virtual void Load(const own::Note& note) = 0;             // 从 blob 填充
        virtual bool Save(std::vector<uint8_t>& outBlob, std::string& outPlain) = 0; // 序列化；无需保存返回 false
        virtual void Reposition(const CRect& rc) = 0;             // 便签 resize 时
        virtual bool IsDirty() const = 0;
        virtual void SetVisible(bool show) = 0;                   // 卷起时隐藏
        virtual void DestroyView() = 0;
    };
    ```
  - `class CTextContentView : public INoteContentView`，内嵌 `CRichEditCtrl m_edit;`，RTF 经 `EM_STREAMIN/EM_STREAMOUT` 读写，`plain` 由 `GetWindowText` + `searchNormalize` 派生；脏标由 `m_edit.GetModify()` 提供。
- **前置**：RichEdit 需在进程内初始化一次（`AfxInitRichEdit2()`）——放在 Task 8 的 `CNoteApp::InitInstance`。本任务只建类，构建即达标（HWND 行为走冒烟）。

- [ ] **Step 1: 写接口头**

`src/ui/INoteContentView.h`:
```cpp
#pragma once
#include <afxwin.h>
#include <vector>
#include <string>
#include <cstdint>
#include "domain/Models.h"
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
};
```

- [ ] **Step 2: 写 TextContentView 头**

`src/ui/TextContentView.h`:
```cpp
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
```

- [ ] **Step 3: 实现 TextContentView**

`src/ui/TextContentView.cpp`:
```cpp
#include "ui/TextContentView.h"
#include "domain/SearchText.h"
#include <algorithm>

// EDITSTREAM 回调：以 std::vector<uint8_t> 作为源/汇
namespace {
struct InCtx  { const std::vector<uint8_t>* buf; size_t pos; };
struct OutCtx { std::vector<uint8_t>* buf; };
DWORD CALLBACK streamInCb(DWORD_PTR cookie, LPBYTE dst, LONG cb, LONG* pcb) {
    auto* c = reinterpret_cast<InCtx*>(cookie);
    size_t remain = c->buf->size() - c->pos;
    LONG n = (LONG)std::min<size_t>((size_t)cb, remain);
    if (n > 0) { memcpy(dst, c->buf->data() + c->pos, (size_t)n); c->pos += (size_t)n; }
    *pcb = n;
    return 0;
}
DWORD CALLBACK streamOutCb(DWORD_PTR cookie, LPBYTE src, LONG cb, LONG* pcb) {
    auto* c = reinterpret_cast<OutCtx*>(cookie);
    if (cb > 0) c->buf->insert(c->buf->end(), src, src + cb);
    *pcb = cb;
    return 0;
}
} // namespace

bool CTextContentView::Create(CWnd* parent, const CRect& rc) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
                | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
    if (!m_edit.Create(style, rc, parent, 0x1001)) return false;
    m_edit.SetEventMask(m_edit.GetEventMask() | ENM_CHANGE);
    m_created = true;
    return true;
}
void CTextContentView::Load(const own::Note& note) {
    if (!m_created) return;
    if (note.contentBlob.empty()) {
        m_edit.SetWindowText(_T(""));
    } else {
        InCtx ctx{ &note.contentBlob, 0 };
        EDITSTREAM es{ (DWORD_PTR)&ctx, 0, streamInCb };
        long read = m_edit.StreamIn(SF_RTF, es);
        if (read <= 0 && es.dwError != 0) {
            // RTF 解析失败 → 回落把原始字节当纯文本显示（不丢内容）
            std::string s((const char*)note.contentBlob.data(), note.contentBlob.size());
            m_edit.SetWindowText(CString(s.c_str()));
        }
    }
    m_edit.SetModify(FALSE);
}
bool CTextContentView::Save(std::vector<uint8_t>& outBlob, std::string& outPlain) {
    if (!m_created) return false;
    outBlob.clear();
    OutCtx ctx{ &outBlob };
    EDITSTREAM es{ (DWORD_PTR)&ctx, 0, streamOutCb };
    m_edit.StreamOut(SF_RTF, es);
    CString w; m_edit.GetWindowText(w);
    CStringA utf8(w);   // 便签正文通常 ASCII/本地页；plain_text 仅供 LIKE 搜索
    outPlain = own::searchNormalize(std::string((LPCSTR)utf8));
    m_edit.SetModify(FALSE);
    return true;
}
void CTextContentView::Reposition(const CRect& rc) {
    if (m_created) m_edit.MoveWindow(rc);
}
bool CTextContentView::IsDirty() const {
    return m_created && const_cast<CRichEditCtrl&>(m_edit).GetModify();
}
void CTextContentView::SetVisible(bool show) {
    if (m_created) m_edit.ShowWindow(show ? SW_SHOW : SW_HIDE);
}
void CTextContentView::DestroyView() {
    if (m_created) { m_edit.DestroyWindow(); m_created = false; }
}
```

- [ ] **Step 4: 登记进 app 工程并构建**

在 `app/open_windows_note_app.vcxproj` 的源 `ItemGroup` 加入（与既有 `..\src\ui\*.cpp` 并列）：
```xml
    <ClCompile Include="..\src\domain\SearchText.cpp" />
    <ClCompile Include="..\src\domain\ChecklistModel.cpp" />
    <ClCompile Include="..\src\ui\ChecklistLayout.cpp" />
    <ClCompile Include="..\src\ui\DrawingMath.cpp" />
    <ClCompile Include="..\src\ui\ContentLayout.cpp" />
    <ClCompile Include="..\src\ui\TextContentView.cpp" />
```
在头 `ItemGroup`（`ClInclude`）加入：
```xml
    <ClInclude Include="..\src\ui\INoteContentView.h" />
    <ClInclude Include="..\src\ui\TextContentView.h" />
```
Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -4
```
Expected: app 与 tests 均链接通过；tests 数量与 Task 6 后一致（本任务未加纯逻辑测试）。

- [ ] **Step 5: Commit**

```bash
git add src/ui/INoteContentView.h src/ui/TextContentView.h src/ui/TextContentView.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui): INoteContentView interface + TextContentView (RichEdit RTF)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: CNoteWindow 宿主内容视图 + 防抖保存（集成富文本端到端）

**Files:**
- Create: `src/ui/ContentViewFactory.h`, `src/ui/ContentViewFactory.cpp`
- Modify: `src/ui/NoteWindow.h`, `src/ui/NoteWindow.cpp`
- Modify: `src/app/NoteApp.cpp`（`AfxInitRichEdit2()`）
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 7 `INoteContentView`/`CTextContentView`；Task 6 `noteContentRect`；Task 1 `NoteStore::updateContent`；P2 `CNoteWindow`。
- Produces:
  - `std::unique_ptr<INoteContentView> own::makeContentView(own::NoteType type);` —— 本任务只识别 `RichText`（其余 `default` 也回落 `CTextContentView`；Checklist/Drawing 在 Task 9/10 接管）。
  - `CNoteWindow` 新增私有 `std::unique_ptr<INoteContentView> m_content;`、常量 `static const UINT kSaveTimer = 1;`；`Create` 尾部造内容视图铺在 `noteContentRect` 并 `Load`，起 800ms 防抖计时器；`OnSize` 重定位内容；`OnTimer` 脏则保存；关闭按钮与 `OnDestroy` 各强制 flush 一次；卷起/展开切换内容视图可见性。
  - 保存动作：`flushContent()` —— `if (m_content && m_content->IsDirty()) { blob/plain=Save(); m_store->updateContent(id, blob, plain, time(nullptr)); }`。

- [ ] **Step 1: 写工厂**

`src/ui/ContentViewFactory.h`:
```cpp
#pragma once
#include <memory>
#include "domain/Models.h"
#include "ui/INoteContentView.h"
namespace own {
std::unique_ptr<INoteContentView> makeContentView(NoteType type);
}
```
`src/ui/ContentViewFactory.cpp`:
```cpp
#include "ui/ContentViewFactory.h"
#include "ui/TextContentView.h"
namespace own {
std::unique_ptr<INoteContentView> makeContentView(NoteType type) {
    switch (type) {
        case NoteType::RichText:
        default:
            return std::make_unique<CTextContentView>();
    }
}
}
```

- [ ] **Step 2: 改 NoteWindow.h（加成员/消息）**

`src/ui/NoteWindow.h`：顶部 include 加 `#include <memory>` 与 `#include "ui/INoteContentView.h"`；在消息声明区加 `OnSize`/`OnTimer`/`OnDestroy`：
```cpp
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
```
私有区加：
```cpp
    static const UINT kSaveTimer = 1;
    void layoutContent();     // 把内容视图移到 noteContentRect
    void flushContent();      // 脏则落盘
    std::unique_ptr<INoteContentView> m_content;
```

- [ ] **Step 3: 改 NoteWindow.cpp（宿主 + 保存 + 尺寸/卷起联动）**

顶部加 include：
```cpp
#include "ui/ContentLayout.h"
#include "ui/ContentViewFactory.h"
#include <ctime>
```
消息映射加：
```cpp
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_WM_DESTROY()
```
在 `Create` 的 `ShowWindow(SW_SHOWNOACTIVATE);` 之后、`return true;` 之前插入：
```cpp
    m_content = own::makeContentView(m_note.type);
    if (m_content) {
        CRect rc; GetClientRect(&rc);
        own::RectI cr = own::noteContentRect({0,0,rc.Width(),rc.Height()}, kTitleMetrics.height, 6);
        m_content->Create(this, CRect(cr.x, cr.y, cr.x+cr.w, cr.y+cr.h));
        m_content->Load(m_note);
        if (m_note.rolledUp) m_content->SetVisible(false);
    }
    SetTimer(kSaveTimer, 800, nullptr);
```
新增成员函数（放在文件末尾）：
```cpp
void CNoteWindow::layoutContent() {
    if (!m_content) return;
    CRect rc; GetClientRect(&rc);
    own::RectI cr = own::noteContentRect({0,0,rc.Width(),rc.Height()}, kTitleMetrics.height, 6);
    m_content->Reposition(CRect(cr.x, cr.y, cr.x+cr.w, cr.y+cr.h));
}
void CNoteWindow::flushContent() {
    if (!m_content || !m_store) return;
    if (!m_content->IsDirty()) return;
    std::vector<uint8_t> blob; std::string plain;
    if (m_content->Save(blob, plain))
        m_store->updateContent(m_note.id, blob, plain, (int64_t)time(nullptr));
}
void CNoteWindow::OnSize(UINT nType, int cx, int cy) {
    CWnd::OnSize(nType, cx, cy);
    layoutContent();
}
void CNoteWindow::OnTimer(UINT_PTR id) {
    if (id == kSaveTimer) flushContent();
    CWnd::OnTimer(id);
}
void CNoteWindow::OnDestroy() {
    KillTimer(kSaveTimer);
    flushContent();
    if (m_content) { m_content->DestroyView(); m_content.reset(); }
    CWnd::OnDestroy();
}
```
在 `OnLButtonDown` 的 **Close** 分支里，`ShowWindow(SW_HIDE);` 之前加一行 `flushContent();`（隐藏前存一次）。
在 **Roll** 分支两条 `SetWindowPos(...)` 之后分别加内容可见性联动：卷起后 `if (m_content) m_content->SetVisible(false);`，展开后 `if (m_content) m_content->SetVisible(true);`。
`OnPaint` 内容区占位文字改为**仅在无内容视图时**画（有内容视图时内容由子控件自绘）：把 `if (!m_note.rolledUp) { …DrawString… }` 整块条件改为 `if (!m_note.rolledUp && !m_content) { …DrawString… }`。

- [ ] **Step 4: 初始化 RichEdit（App 层）**

`src/app/NoteApp.cpp` 的 `InitInstance()` 里、`CWinApp::InitInstance();` 之后加：
```cpp
    AfxInitRichEdit2();   // RichEdit20W 注册，供 CTextContentView 使用
```

- [ ] **Step 5: 登记工厂进 app 工程 + 构建 + 单测**

`app/open_windows_note_app.vcxproj` 源 `ItemGroup` 加 `..\src\ui\ContentViewFactory.cpp`；头组加 `..\src\ui\ContentViewFactory.h`。
Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -4
```
Expected: 构建链接通过；tests 全绿（数量与 Task 6 后一致）。

- [ ] **Step 6: 手工冒烟（best-effort，记录到报告，勿阻塞）**

若 GUI 可用：
```bash
rm -f x64/Debug/notes.db
( ./x64/Debug/open_windows_note.exe & ) ; sleep 2
```
在 welcome 便签内容区点按输入若干字符 → 关闭程序（Ctrl+Alt+Q）→ 再启动 → 文本仍在。无 GUI 会话记录“需人工验证”。收尾 `taskkill //F //IM open_windows_note.exe`。

- [ ] **Step 7: Commit**

```bash
git add src/ui/ContentViewFactory.h src/ui/ContentViewFactory.cpp src/ui/NoteWindow.h src/ui/NoteWindow.cpp src/app/NoteApp.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui): CNoteWindow hosts content view + debounced content save (text end-to-end)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: ChecklistContentView（自绘清单 + 就地编辑）

**Files:**
- Create: `src/ui/ChecklistContentView.h`, `src/ui/ChecklistContentView.cpp`
- Modify: `src/ui/ContentViewFactory.cpp`（`Checklist` → 本视图）
- Modify: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`, `src/app/NoteApp.cpp`（临时开发热键 `Ctrl+Alt+2` 新建清单 note，供冒烟；P4 管理器替代）
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 3 `checklist*` 编辑函数；Task 4 `ChecklistMetrics/checklist*Rect/checklistHitTest`；P1 `serializeChecklist/deserializeChecklist/checklistPlainText`；Task 2 `searchNormalize`；Task 7 `INoteContentView`。
- Produces: `class CChecklistContentView : public CWnd, public INoteContentView` —— 自绘勾选行；单击勾选框切换、单击文本进入就地 `CEdit` 编辑、点“＋新增行”追加并进入编辑；`Save` 用 `serializeChecklist` + `checklistPlainText`。常量 `static const own::ChecklistMetrics kMetrics{24,16,4};`。

- [ ] **Step 1: 写头**

`src/ui/ChecklistContentView.h`:
```cpp
#pragma once
#include <afxwin.h>
#include <vector>
#include "ui/INoteContentView.h"
#include "domain/Models.h"
class CChecklistContentView : public CWnd, public INoteContentView {
public:
    bool Create(CWnd* parent, const CRect& rc) override;
    void Load(const own::Note& note) override;
    bool Save(std::vector<uint8_t>& outBlob, std::string& outPlain) override;
    void Reposition(const CRect& rc) override;
    bool IsDirty() const override;
    void SetVisible(bool show) override;
    void DestroyView() override;
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnCommitEdit();          // 就地编辑提交（EN_KILLFOCUS）
    DECLARE_MESSAGE_MAP()
private:
    void beginEdit(int index);
    void commitEdit();
    std::vector<own::ChecklistItem> m_items;
    std::vector<uint8_t> m_originalBlob;  // 解析失败时保留，防空覆盖
    bool m_dirty = false;
    bool m_created = false;
    CEdit m_edit;                         // 就地编辑器
    int m_editing = -1;                   // 正在编辑的行；-1 无
};
```

- [ ] **Step 2: 实现**

`src/ui/ChecklistContentView.cpp`:
```cpp
#include "ui/ChecklistContentView.h"
#include "ui/ChecklistLayout.h"
#include "domain/ChecklistModel.h"
#include "domain/ChecklistJson.h"
#include "domain/SearchText.h"

static const own::ChecklistMetrics kMetrics{ 24, 16, 4 };
static const UINT kInplaceEditId = 0x2001;

BEGIN_MESSAGE_MAP(CChecklistContentView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_EN_KILLFOCUS(kInplaceEditId, OnCommitEdit)
END_MESSAGE_MAP()

bool CChecklistContentView::Create(CWnd* parent, const CRect& rc) {
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW));
    if (!CWnd::CreateEx(0, cls, _T("Checklist"), WS_CHILD | WS_VISIBLE, rc, parent, 0x1002))
        return false;
    m_created = true;
    return true;
}
void CChecklistContentView::Load(const own::Note& note) {
    m_originalBlob = note.contentBlob;
    m_items = own::deserializeChecklist(note.contentBlob);  // 失败返回空；originalBlob 保留
    m_dirty = false;
    if (m_created) Invalidate(FALSE);
}
bool CChecklistContentView::Save(std::vector<uint8_t>& outBlob, std::string& outPlain) {
    outBlob = own::serializeChecklist(m_items);
    outPlain = own::searchNormalize(own::checklistPlainText(m_items));
    m_dirty = false;
    return true;
}
void CChecklistContentView::Reposition(const CRect& rc) { if (m_created) MoveWindow(rc); }
bool CChecklistContentView::IsDirty() const { return m_dirty; }
void CChecklistContentView::SetVisible(bool show) { if (m_created) ShowWindow(show ? SW_SHOW : SW_HIDE); }
void CChecklistContentView::DestroyView() { if (m_created) { DestroyWindow(); m_created = false; } }

BOOL CChecklistContentView::OnEraseBkgnd(CDC*) { return TRUE; }

void CChecklistContentView::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* old = mem.SelectObject(&bmp);
    mem.FillSolidRect(&rc, RGB(0xFF, 0xF7, 0xB0));           // 背景（与便签底色一致，主题化留 P4）
    CPen pen(PS_SOLID, 1, RGB(0x50, 0x50, 0x50));
    CPen* op = mem.SelectObject(&pen);
    mem.SetBkMode(TRANSPARENT);
    own::RectI content{ 0, 0, rc.Width(), rc.Height() };
    for (size_t i = 0; i < m_items.size(); ++i) {
        own::RectI b = own::checklistBoxRect(content, kMetrics, (int)i);
        mem.Rectangle(b.x, b.y, b.x + b.w, b.y + b.h);       // 勾选框
        if (m_items[i].checked) {                            // 勾：对角线两笔
            mem.MoveTo(b.x + 2, b.y + b.h / 2); mem.LineTo(b.x + b.w / 2, b.y + b.h - 2);
            mem.LineTo(b.x + b.w - 2, b.y + 2);
        }
        own::RectI row = own::checklistRowRect(content, kMetrics, (int)i);
        CRect tr(b.x + b.w + 4, row.y, row.x + row.w, row.y + row.h);
        CString t(m_items[i].text.c_str());
        mem.DrawText(t, tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    own::RectI addRow = own::checklistRowRect(content, kMetrics, (int)m_items.size());
    CRect ar(addRow.x + kMetrics.pad, addRow.y, addRow.x + addRow.w, addRow.y + addRow.h);
    mem.DrawText(_T("+ add"), ar, DT_SINGLELINE | DT_VCENTER);
    mem.SelectObject(op);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(old);
}
void CChecklistContentView::beginEdit(int index) {
    if (index < 0 || index >= (int)m_items.size()) return;
    own::RectI row = own::checklistRowRect({0,0,0,0}, kMetrics, index);
    CRect rc; GetClientRect(&rc);
    own::RectI r = own::checklistRowRect({0,0,rc.Width(),rc.Height()}, kMetrics, index);
    own::RectI b = own::checklistBoxRect({0,0,rc.Width(),rc.Height()}, kMetrics, index);
    CRect er(b.x + b.w + 4, r.y, r.x + r.w, r.y + r.h);
    if (m_edit.GetSafeHwnd()) m_edit.DestroyWindow();
    m_edit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, er, this, kInplaceEditId);
    m_edit.SetWindowText(CString(m_items[index].text.c_str()));
    m_edit.SetFocus();
    m_edit.SetSel(0, -1);
    m_editing = index;
    (void)row;
}
void CChecklistContentView::commitEdit() {
    if (m_editing < 0 || !m_edit.GetSafeHwnd()) return;
    CString w; m_edit.GetWindowText(w);
    CStringA a(w);
    own::checklistSetText(m_items, (size_t)m_editing, std::string((LPCSTR)a));
    m_edit.DestroyWindow();
    m_editing = -1;
    m_dirty = true;
    Invalidate(FALSE);
}
void CChecklistContentView::OnCommitEdit() { commitEdit(); }
void CChecklistContentView::OnLButtonDown(UINT, CPoint pt) {
    if (m_editing >= 0) { commitEdit(); return; }
    CRect rc; GetClientRect(&rc);
    auto hit = own::checklistHitTest({0,0,rc.Width(),rc.Height()}, kMetrics, (int)m_items.size(), pt.x, pt.y);
    switch (hit.kind) {
        case own::ChecklistHit::Checkbox:
            own::checklistToggle(m_items, (size_t)hit.index); m_dirty = true; Invalidate(FALSE); break;
        case own::ChecklistHit::Text:
            beginEdit(hit.index); break;
        case own::ChecklistHit::AddRow:
            own::checklistAdd(m_items, ""); m_dirty = true;
            beginEdit((int)m_items.size() - 1); break;
        default: break;
    }
}
```
> 说明：`m_originalBlob` 在解析失败（返回空）时保留原字节；因 `CNoteWindow::flushContent` 只在 `IsDirty()` 为真时落盘，未编辑的坏 blob 不会被空序列化覆盖，满足设计 §6「保留原始 blob」。删除行/拖动排序（`checklistRemoveAt`/`checklistMove`）本视图暂不接线（右键删除属 P4 交互）；模型与测试已就绪，避免过度实现。

- [ ] **Step 3: 工厂接线 + 临时新建热键**

`src/ui/ContentViewFactory.cpp` 顶部 include 加 `#include "ui/ChecklistContentView.h"`，`switch` 加：
```cpp
        case NoteType::Checklist:
            return std::make_unique<CChecklistContentView>();
```
`src/app/AppHostWindow.h`：加 `static const UINT kHotkeyNewChecklist = 3;` 与 `std::function<void()> onNewChecklist;`。
`src/app/AppHostWindow.cpp`：`Create()` 里加 `::RegisterHotKey(m_hWnd, kHotkeyNewChecklist, MOD_CONTROL|MOD_ALT, '2');`；`OnHotKey` 加 `else if (idHotKey == kHotkeyNewChecklist) { if (onNewChecklist) onNewChecklist(); }`；`OnDestroy` 加 `::UnregisterHotKey(m_hWnd, kHotkeyNewChecklist);`。
`src/app/NoteApp.cpp`：在装配回调处加：
```cpp
    m_host.onNewChecklist = [this]{
        own::Note n; n.type = own::NoteType::Checklist; n.visible = true;
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
    };
```

- [ ] **Step 4: 登记进 app 工程 + 构建 + 单测**

`app/open_windows_note_app.vcxproj` 源组加 `..\src\ui\ChecklistContentView.cpp`；头组加 `..\src\ui\ChecklistContentView.h`。
Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -4
```
Expected: 构建链接通过；tests 全绿（数量不变）。

- [ ] **Step 5: 手工冒烟（记录）** — `Ctrl+Alt+2` 新建清单；点“+ add”加行并输入；勾选切换；重启后条目与勾选保留。

- [ ] **Step 6: Commit**

```bash
git add src/ui/ChecklistContentView.h src/ui/ChecklistContentView.cpp src/ui/ContentViewFactory.cpp src/app/AppHostWindow.h src/app/AppHostWindow.cpp src/app/NoteApp.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui): self-drawn checklist content view + inplace edit

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 10: DrawingContentView（自绘 GDI+ 涂鸦画布）

**Files:**
- Create: `src/ui/DrawingContentView.h`, `src/ui/DrawingContentView.cpp`
- Modify: `src/ui/ContentViewFactory.cpp`（`Drawing` → 本视图）
- Modify: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`, `src/app/NoteApp.cpp`（临时开发热键 `Ctrl+Alt+3` 新建涂鸦 note）
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 5 `strokeHitTest`；P1 `serializeStrokes/deserializeStrokes`；Task 7 `INoteContentView`；GDI+。
- Produces: `class CDrawingContentView : public CWnd, public INoteContentView` —— 顶部一行自绘工具（4 色板 + 橡皮）；画布内鼠标拖动画矢量笔迹；橡皮模式点中整条删除；`Save` 用 `serializeStrokes`，`plain` 为空。常量：`kToolH=22, kSwatch=16, kEraseTol=6.0`；调色板 `{0x000000,0xE03030,0x3060E0,0x30A030}`。

- [ ] **Step 1: 写头**

`src/ui/DrawingContentView.h`:
```cpp
#pragma once
#include <afxwin.h>
#include <vector>
#include "ui/INoteContentView.h"
#include "domain/Models.h"
class CDrawingContentView : public CWnd, public INoteContentView {
public:
    bool Create(CWnd* parent, const CRect& rc) override;
    void Load(const own::Note& note) override;
    bool Save(std::vector<uint8_t>& outBlob, std::string& outPlain) override;
    void Reposition(const CRect& rc) override;
    bool IsDirty() const override;
    void SetVisible(bool show) override;
    void DestroyView() override;
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    DECLARE_MESSAGE_MAP()
private:
    int toolAtPoint(CPoint pt) const;     // 命中哪个工具槽；-1 = 画布
    std::vector<own::Stroke> m_strokes;
    own::Stroke m_cur;                     // 正在画的笔迹
    bool m_drawing = false;
    bool m_eraser = false;
    uint32_t m_color = 0x000000;
    int m_width = 3;
    bool m_dirty = false;
    bool m_created = false;
};
```

- [ ] **Step 2: 实现**

`src/ui/DrawingContentView.cpp`:
```cpp
#include "ui/DrawingContentView.h"
#include "ui/DrawingMath.h"
#include "domain/StrokesJson.h"
#include <gdiplus.h>
using namespace Gdiplus;

static const int kToolH = 22;
static const int kSwatch = 16;
static const double kEraseTol = 6.0;
static const uint32_t kPalette[4] = { 0x000000, 0xE03030, 0x3060E0, 0x30A030 };

BEGIN_MESSAGE_MAP(CDrawingContentView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

bool CDrawingContentView::Create(CWnd* parent, const CRect& rc) {
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_CROSS));
    if (!CWnd::CreateEx(0, cls, _T("Drawing"), WS_CHILD | WS_VISIBLE, rc, parent, 0x1003))
        return false;
    m_created = true;
    return true;
}
void CDrawingContentView::Load(const own::Note& note) {
    m_strokes = own::deserializeStrokes(note.contentBlob);
    m_dirty = false;
    if (m_created) Invalidate(FALSE);
}
bool CDrawingContentView::Save(std::vector<uint8_t>& outBlob, std::string& outPlain) {
    outBlob = own::serializeStrokes(m_strokes);
    outPlain = "";                       // 涂鸦无搜索文本（OCR 属 v2）
    m_dirty = false;
    return true;
}
void CDrawingContentView::Reposition(const CRect& rc) { if (m_created) MoveWindow(rc); }
bool CDrawingContentView::IsDirty() const { return m_dirty; }
void CDrawingContentView::SetVisible(bool show) { if (m_created) ShowWindow(show ? SW_SHOW : SW_HIDE); }
void CDrawingContentView::DestroyView() { if (m_created) { DestroyWindow(); m_created = false; } }

BOOL CDrawingContentView::OnEraseBkgnd(CDC*) { return TRUE; }

int CDrawingContentView::toolAtPoint(CPoint pt) const {
    if (pt.y >= kToolH) return -1;
    int x = 2;
    for (int i = 0; i < 4; ++i) { if (pt.x >= x && pt.x < x + kSwatch) return i; x += kSwatch + 4; }
    if (pt.x >= x && pt.x < x + kSwatch) return 4;   // 橡皮槽
    return -1;
}
void CDrawingContentView::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* old = mem.SelectObject(&bmp);
    {
        Graphics g(mem.GetSafeHdc());
        g.Clear(Color(255, 0xFF, 0xF7, 0xB0));
        // 工具行
        int x = 2;
        for (int i = 0; i < 4; ++i) {
            SolidBrush b(Color(255, (kPalette[i]>>16)&0xFF, (kPalette[i]>>8)&0xFF, kPalette[i]&0xFF));
            g.FillRectangle(&b, x, 2, kSwatch, kSwatch);
            if (!m_eraser && m_color == kPalette[i]) {
                Pen sel(Color(255,0,0,0), 2.0f); g.DrawRectangle(&sel, x, 2, kSwatch, kSwatch);
            }
            x += kSwatch + 4;
        }
        SolidBrush eb(Color(255, 0xEE, 0xEE, 0xEE));
        g.FillRectangle(&eb, x, 2, kSwatch, kSwatch);
        FontFamily ff(L"Segoe UI"); Font f(&ff, 9, FontStyleRegular, UnitPixel);
        SolidBrush tb(Color(255,0x40,0x40,0x40));
        g.DrawString(L"E", 1, &f, PointF((REAL)(x+4),(REAL)3), &tb);
        if (m_eraser) { Pen sel(Color(255,0,0,0), 2.0f); g.DrawRectangle(&sel, x, 2, kSwatch, kSwatch); }
        // 已有笔迹 + 当前笔迹
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        auto drawStroke = [&](const own::Stroke& s) {
            if (s.points.size() < 2) return;
            Pen p(Color(255, (s.color>>16)&0xFF, (s.color>>8)&0xFF, s.color&0xFF), (REAL)s.width);
            p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound); p.SetLineJoin(LineJoinRound);
            std::vector<Point> pts;
            for (auto& pr : s.points) pts.push_back(Point(pr.first, pr.second));
            g.DrawLines(&p, pts.data(), (INT)pts.size());
        };
        for (auto& s : m_strokes) drawStroke(s);
        if (m_drawing) drawStroke(m_cur);
    }
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(old);
}
void CDrawingContentView::OnLButtonDown(UINT, CPoint pt) {
    int tool = toolAtPoint(pt);
    if (tool >= 0) {                          // 点工具行
        if (tool == 4) m_eraser = true;
        else { m_eraser = false; m_color = kPalette[tool]; }
        Invalidate(FALSE);
        return;
    }
    if (m_eraser) {
        int hit = own::strokeHitTest(m_strokes, pt.x, pt.y, kEraseTol);
        if (hit >= 0) { m_strokes.erase(m_strokes.begin() + hit); m_dirty = true; Invalidate(FALSE); }
        return;
    }
    m_drawing = true;
    m_cur = own::Stroke{};
    m_cur.color = m_color; m_cur.width = m_width;
    m_cur.points.push_back({ pt.x, pt.y });
    SetCapture();
}
void CDrawingContentView::OnMouseMove(UINT, CPoint pt) {
    if (!m_drawing) return;
    m_cur.points.push_back({ pt.x, pt.y });
    Invalidate(FALSE);
}
void CDrawingContentView::OnLButtonUp(UINT, CPoint) {
    if (!m_drawing) return;
    m_drawing = false; ReleaseCapture();
    if (m_cur.points.size() >= 2) { m_strokes.push_back(m_cur); m_dirty = true; }
    m_cur = own::Stroke{};
    Invalidate(FALSE);
}
```
> 说明：GDI+ 已由 `CNoteApp::InitInstance` 全局 `GdiplusStartup`（P2），此处直接用。`plain_text` 为空是有意的（涂鸦无文本）；因涂鸦 note 的 `IsDirty` 仅在真正画/擦后为真，未改动的涂鸦不会被落盘覆盖。

- [ ] **Step 3: 工厂接线 + 临时新建热键**

`src/ui/ContentViewFactory.cpp` include 加 `#include "ui/DrawingContentView.h"`，`switch` 加：
```cpp
        case NoteType::Drawing:
            return std::make_unique<CDrawingContentView>();
```
`src/app/AppHostWindow.h`：加 `static const UINT kHotkeyNewDrawing = 4;` 与 `std::function<void()> onNewDrawing;`。
`src/app/AppHostWindow.cpp`：`Create()` 加 `::RegisterHotKey(m_hWnd, kHotkeyNewDrawing, MOD_CONTROL|MOD_ALT, '3');`；`OnHotKey` 加 `else if (idHotKey == kHotkeyNewDrawing) { if (onNewDrawing) onNewDrawing(); }`；`OnDestroy` 加 `::UnregisterHotKey(m_hWnd, kHotkeyNewDrawing);`。
`src/app/NoteApp.cpp`：装配回调处加：
```cpp
    m_host.onNewDrawing = [this]{
        own::Note n; n.type = own::NoteType::Drawing; n.visible = true;
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
    };
```

- [ ] **Step 4: 登记进 app 工程 + 构建 + 单测**

`app/open_windows_note_app.vcxproj` 源组加 `..\src\ui\DrawingContentView.cpp`；头组加 `..\src\ui\DrawingContentView.h`。
Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | tail -6 && ./x64/Debug/tests.exe 2>&1 | tail -4
```
Expected: 构建链接通过；tests 全绿（数量不变）。

- [ ] **Step 5: 手工冒烟（记录）** — `Ctrl+Alt+3` 新建涂鸦；选色画线；橡皮点中删除整条；重启后笔迹保留。

- [ ] **Step 6: Commit**

```bash
git add src/ui/DrawingContentView.h src/ui/DrawingContentView.cpp src/ui/ContentViewFactory.cpp src/app/AppHostWindow.h src/app/AppHostWindow.cpp src/app/NoteApp.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui): self-drawn GDI+ drawing content view + vector strokes

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 11: P3 手工冒烟清单 + 收尾文档

**Files:**
- Create: `docs/superpowers/smoke/P3-smoke-checklist.md`

**Interfaces:** 无代码；记录三种内容视图的人工验证步骤。

- [ ] **Step 1: 写冒烟清单**

`docs/superpowers/smoke/P3-smoke-checklist.md`:
```markdown
# P3 手工冒烟清单（内容视图）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe
说明: Ctrl+Alt+N 新建富文本 / Ctrl+Alt+2 新建清单 / Ctrl+Alt+3 新建涂鸦（后两者为 P3 临时开发热键，P4 管理器替代）

## 富文本
- [ ] 首启 welcome 便签内容区可点入编辑，输入文字
- [ ] Ctrl+B/I/U 切换加粗/斜体/下划线（RichEdit 原生）
- [ ] 编辑后 ~1 秒或关闭程序再启动：文本与格式保留（RTF 往返）
- [ ] 富文本内容出现在搜索缓存（P4 搜索时验证；此处只看不报错）

## 清单
- [ ] Ctrl+Alt+2 新建清单便签
- [ ] 点“+ add”新增一行并输入文字；再加数行
- [ ] 点勾选框切换勾/未勾（勾显示对勾）
- [ ] 点已有行文字进入就地编辑，改字后失焦提交
- [ ] 关闭再启动：条目文本与勾选状态全部保留

## 涂鸦
- [ ] Ctrl+Alt+3 新建涂鸦便签
- [ ] 顶部 4 个色板选色，画布拖动画出对应颜色矢量线
- [ ] 点“E”橡皮，点中某条笔迹整条删除
- [ ] 关闭再启动：笔迹（颜色/粗细/形状）保留

## 通用
- [ ] 拖动标题栏移动、拖边缩放：内容区随之重排（RichEdit/画布跟随）
- [ ] 卷起：内容隐藏只剩标题栏；展开：内容恢复
- [ ] 坏 blob（可手工把某 note 的 content_blob 改乱）：打开不崩、不被空覆盖（未编辑则原样保留）
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/smoke/P3-smoke-checklist.md
git commit -m "docs: P3 content-views manual smoke checklist

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage（对照设计文档）:**
- §2.1 / §3 `INoteContentView` 多态（`Load/Save/OnPaint/OnResize/IsDirty`）→ Task 7 接口（含 `Reposition`=OnResize、`SetVisible`、`DestroyView`）。✓
- §3 `TextContentView`（RichEdit / RTF 读写）→ Task 7；字体/字号/加粗斜体下划线 → P3 用 RichEdit 原生 Ctrl+B/I/U（可视工具条明列范围外，P4）。✓（部分：可视工具条延后，已声明）
- §3 `ChecklistContentView`（自绘勾选行、就地编辑、JSON 序列化）→ Task 3/4/9；拖动排序/删除行的**交互接线**延后 P4（模型 `checklistMove/checklistRemoveAt` 已实现并测试）。✓（已声明）
- §3 `DrawingContentView`（GDI+ 画布、矢量笔迹、画笔颜色/粗细/橡皮、笔迹 JSON）→ Task 5/10。✓
- §4 内容 blob 约定（RTF / 清单 JSON / 涂鸦 JSON）+ 同步 `plain_text` → Task 1/2/7/9/10（涂鸦 plain 为空，符合“涂鸦为空”）。✓
- §5 数据流 2「内容置脏→防抖(~800ms 或失焦)→updateContent(事务)」→ Task 8（800ms 定时器 + 关闭/销毁 flush；`updateContent` 单条 UPDATE，SQLite 自动事务）。✓
- §6 容错「RTF 解析失败回落纯文本」→ Task 7 Load 回落；「清单/涂鸦解析失败当空但保留原始 blob」→ Task 9/10 + Task 8「仅脏才落盘」不覆盖坏 blob。✓
- §7 测试「清单/笔迹 JSON 往返」P1 已覆盖；本计划新增纯逻辑（updateContent、searchNormalize、清单模型/布局、涂鸦几何、内容区矩形）doctest 覆盖；窗口/控件行为手工冒烟（Task 11）。✓
- §1.1「一律自绘，唯一例外 RichEdit」→ 清单/涂鸦自绘，仅富文本用 RichEdit；清单就地编辑用 `CEdit` 属编辑态输入控件（与 RichEdit 同类“编辑时的原生输入控件”例外，非常驻自绘 UI），符合精神。✓

**2. Placeholder scan:** 无 TBD/TODO；每个代码步骤给出可编译代码。延后项（富文本工具条、拖动排序/删除接线、主题着色）均为**明确的范围声明**并指向具体后续计划，非占位符。清单/涂鸦背景色暂用便签底色硬编码（黄），与 P2 一致，主题化 P4。

**3. Type consistency:**
- `INoteContentView` 六个纯虚方法（`Create/Load/Save/Reposition/IsDirty/SetVisible/DestroyView`）在 Task 7/9/10 三个实现类签名一致；`Save(std::vector<uint8_t>&, std::string&)->bool` 一致；`CNoteWindow::flushContent`（Task 8）按此签名调用。✓
- `makeContentView(NoteType)`（Task 8 定义，Task 9/10 扩 `switch`）返回 `std::unique_ptr<INoteContentView>`，`CNoteWindow::m_content` 类型一致。✓
- `ChecklistMetrics{rowHeight,boxSize,pad}`、`checklistRowRect/checklistBoxRect/checklistHitTest`（Task 4）在 Task 9 使用一致；`ChecklistHit` 枚举名一致。✓
- `checklistToggle/Add/RemoveAt/Move/SetText`（Task 3）在 Task 9 调用一致。✓
- `serializeChecklist/checklistPlainText/serializeStrokes/deserializeStrokes`（P1）在 Task 9/10 使用与 P1 头签名一致。✓
- `searchNormalize`（Task 2）在 Task 7/9 使用一致；涂鸦不派生 plain（Task 10 传 `""`）。✓
- `noteContentRect(RectI,int,int)`（Task 6）在 Task 8 两处（Create/layoutContent）调用一致；标题栏高取 `kTitleMetrics.height`、缩放边距 `6`（与 P2 一致）。✓
- `updateContent(id, blob, plain, now)`（Task 1）在 Task 8 调用一致。✓
- 临时热键：`kHotkeyNewChecklist=3`（Task 9）、`kHotkeyNewDrawing=4`（Task 10），与 P2 `kHotkeyQuit=1`/`kHotkeyNew=2` 不冲突；回调 `onNewChecklist/onNewDrawing` 命名一致。✓

**已知限制（执行者须知）:** 无 GUI 会话时窗口/控件交互无法自动化；各 GUI 任务以「app 链接通过 + tests 全绿」为自动化达标线，行为落 Task 11 手工清单。清单的删除/拖动排序、富文本可视工具条、主题着色为有意延后项。
