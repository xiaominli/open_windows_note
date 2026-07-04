# 便签标题体验改进 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 便签窗展开时标题栏不再重复正文首行;卷起时仍可识别;补齐"重命名"入口(列表右键 + 标题栏双击)。

**Architecture:** 域层新增纯函数 `noteWindowTitleText`(doctest 覆盖),数据层新增单列更新 `NoteStore::updateTitle`(不动 `updated_at`),UI 层两处接入:`CNoteWindow` 绘制/双击、`CNoteListView` 右键菜单。列表与排序逻辑(`noteTitleText`)不动。

**Tech Stack:** C++17 / MFC / SQLite / doctest(tests.vcxproj)

**Spec:** `docs/superpowers/specs/2026-07-05-note-title-ux-design.md`

## Global Constraints

- 中文串在 UI 源码里一律用 UTF-16 转义(`_T("\x91CD\x547D\x540D")` 风格),与现有 NoteListView.cpp 一致;域层/测试用 UTF-8 字节转义或 ASCII。
- `updateTitle` 只 UPDATE `title` 列,**禁止**触碰 `updated_at`(不扰动"按更新时间排序")。
- 列表右键菜单命令号:「重命名…」= **4**(1 打开 / 2 隐藏 / 3 删除已占用)。
- 权威验证:Windows 上 `tests.exe` 全绿(CI 同款)。Linux 下可用 g++ 跑域层测试加速 TDD 循环,但不作为通过依据。
- 提交信息用中文,格式 `feat:`/`test:` 前缀,结尾 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。

---

### Task 1: 域函数 `noteWindowTitleText`

**Files:**
- Modify: `src/domain/NoteListFormat.h`(第 7 行 `noteTitleText` 声明之后)
- Modify: `src/domain/NoteListFormat.cpp`(第 16 行 `noteTitleText` 定义之后)
- Test: `tests/test_note_list_format.cpp`(文件末尾追加)

**Interfaces:**
- Consumes: 已有 `std::string noteTitleText(const Note& n)`(标题 → 首行40字节 → "(无标题)")。
- Produces: `std::string own::noteWindowTitleText(const Note& n, bool rolledUp)` — Task 3 依赖此签名。

- [x] **Step 1: 写失败测试**

在 `tests/test_note_list_format.cpp` 末尾追加:

```cpp
TEST_CASE("noteWindowTitleText: expanded shows custom title only") {
    Note a; a.title = "Work"; a.plainText = "buy milk";
    CHECK(own::noteWindowTitleText(a, false) == "Work");   // 展开:有标题显标题
    CHECK(own::noteWindowTitleText(a, true)  == "Work");   // 卷起:同样优先标题
    Note b; b.plainText = "buy milk\nline2";
    CHECK(own::noteWindowTitleText(b, false) == "");        // 展开无标题:留空,不重复首行
    CHECK(own::noteWindowTitleText(b, true)  == "buy milk");// 卷起:回落首行
    Note c;
    CHECK(own::noteWindowTitleText(c, false) == "");        // 展开全空:留空
    CHECK(own::noteWindowTitleText(c, true)
          == "(\xE6\x97\xA0\xE6\xA0\x87\xE9\xA2\x98)");     // 卷起全空:(无标题)
}

TEST_CASE("noteWindowTitleText rolled-up truncates like noteTitleText") {
    Note d; d.plainText = std::string(50, 'x');             // 超 40 字节
    CHECK(own::noteWindowTitleText(d, true) == std::string(40, 'x'));
}
```

- [x] **Step 2: 跑测试确认失败(编译错:函数未声明)**

Linux 快速循环(可选):
```bash
g++ -std=c++17 -Isrc -Isrc/third_party/doctest \
    tests/test_main.cpp tests/test_note_list_format.cpp src/domain/NoteListFormat.cpp \
    -o /tmp/claude-1000/-media-sf-windows-note/*/scratchpad/t_fmt && \
    /tmp/claude-1000/-media-sf-windows-note/*/scratchpad/t_fmt
```
Windows(权威):
```bat
msbuild tests\tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```
Expected: 编译失败,`noteWindowTitleText` is not a member of 'own'。

- [x] **Step 3: 最小实现**

`src/domain/NoteListFormat.h`,在 `noteTitleText` 声明后加:

```cpp
// 便签窗标题栏文案:展开只显自定义标题(空则留空,不重复正文首行);
// 卷起时窗口只剩标题栏,回落 首行 → (无标题) 保证可识别。
std::string noteWindowTitleText(const Note& n, bool rolledUp);
```

`src/domain/NoteListFormat.cpp`,在 `noteTitleText` 定义后加:

```cpp
std::string noteWindowTitleText(const Note& n, bool rolledUp) {
    if (!n.title.empty()) return n.title;
    if (!rolledUp) return std::string();
    return noteTitleText(n);
}
```

- [x] **Step 4: 跑测试确认通过**

同 Step 2 命令。Expected: 全部 CHECK 通过,0 failed。

- [x] **Step 5: Commit**

```bash
git add src/domain/NoteListFormat.h src/domain/NoteListFormat.cpp tests/test_note_list_format.cpp
git commit -m "feat: 域函数 noteWindowTitleText——展开留空/卷起回落首行

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: `NoteStore::updateTitle`

**Files:**
- Modify: `src/data/NoteStore.h`(第 19 行 `updateContent` 声明之后)
- Modify: `src/data/NoteStore.cpp`(`updateFlags` 定义之后,约第 65 行)
- Test: `tests/test_notestore.cpp`(文件末尾追加)

**Interfaces:**
- Consumes: 既有 `Statement` 绑定模式(见同文件 `updateFlags`)、测试侧 `freshDb()` 夹具。
- Produces: `bool NoteStore::updateTitle(int64_t id, const std::string& titleU8)` — Task 3/4 依赖此签名。

- [x] **Step 1: 写失败测试**

在 `tests/test_notestore.cpp` 末尾追加:

```cpp
TEST_CASE("updateTitle changes title only, keeps updated_at") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note n; n.plainText = "hello";
    int64_t id = store.insertNote(n, 1000);
    CHECK(store.updateTitle(id, "Work"));
    auto got = store.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->title == "Work");
    CHECK(got->updatedAt == 1000);          // 重命名不扰动"按更新时间排序"
    CHECK(store.updateTitle(id, ""));       // 清空 = 恢复未设置
    CHECK(store.getNote(id)->title == "");
}
```

- [x] **Step 2: 跑测试确认失败**

Windows:
```bat
msbuild tests\tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```
Expected: 编译失败,'updateTitle': is not a member of 'own::NoteStore'。

- [x] **Step 3: 最小实现**

`src/data/NoteStore.h`,`updateContent` 声明后加:

```cpp
    bool updateTitle(int64_t id, const std::string& titleU8);   // 只改 title,不动 updated_at(重命名≠内容编辑)
```

`src/data/NoteStore.cpp`,`updateFlags` 定义后加:

```cpp
bool NoteStore::updateTitle(int64_t id, const std::string& titleU8) {
    Statement s(db_, "UPDATE notes SET title=? WHERE id=?;");
    s.bind(1, titleU8); s.bind(2, id);
    s.execDone(); return true;
}
```

- [x] **Step 4: 跑测试确认通过**

```bat
msbuild tests\tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
tests\x64\Debug\tests.exe
```
Expected: 全绿(含既有用例)。

- [x] **Step 5: Commit**

```bash
git add src/data/NoteStore.h src/data/NoteStore.cpp tests/test_notestore.cpp
git commit -m "feat: NoteStore::updateTitle 单列更新(不动 updated_at)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: 便签窗——标题栏绘制切换 + 双击重命名

**Files:**
- Modify: `src/ui/NoteWindow.h`(消息处理声明区,第 28 行 `OnDestroy` 后)
- Modify: `src/ui/NoteWindow.cpp`(include 区、消息映射、OnPaint 标题块约 156-170 行、新增 OnLButtonDblClk)

**Interfaces:**
- Consumes: Task 1 `own::noteWindowTitleText(const Note&, bool)`;Task 2 `m_store->updateTitle(int64_t, const std::string&)`;既有 `own_ui::promptText(CWnd*, const CString&, CString&, bool allowEmpty)`(`ui/TextPrompt.h`)、`u8ToWideStr`、`own::hitTestTitleBar`/`TitleHit::Drag`、窗口类已带 `CS_DBLCLKS`(NoteWindow.cpp:57)。
- Produces: 无(终端 UI 改动)。

- [x] **Step 1: include 与 wideToU8Str 辅助**

`src/ui/NoteWindow.cpp` include 区加:

```cpp
#include "ui/TextPrompt.h"
```

在 `u8ToWideStr`(约第 36 行)后加对称辅助:

```cpp
static std::string wideToU8Str(const CString& w) {
    if (w.IsEmpty()) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), &s[0], n, nullptr, nullptr);
    return s;
}
```

- [x] **Step 2: OnPaint 标题块改用 noteWindowTitleText**

替换 `NoteWindow.cpp` 约 156-170 行整个标题绘制块(原 `noteTitleText` + `#id` 回落删除):

```cpp
        // 标题栏:展开只显自定义标题(空则留空,纯拖动区);卷起回落 首行 → (无标题)
        {
            std::string t = own::noteWindowTitleText(m_note, m_note.rolledUp);
            if (!t.empty()) {
                std::wstring wt = u8ToWideStr(t);
                FontFamily tf(L"微软雅黑"); Font tfont(&tf, 12, FontStyleRegular, UnitPixel);
                SolidBrush tb(Color(255, 0x40, 0x40, 0x40));
                StringFormat sf; sf.SetTrimming(StringTrimmingEllipsisCharacter);
                sf.SetFormatFlags(StringFormatFlagsNoWrap);
                sf.SetLineAlignment(StringAlignmentCenter);
                RectF tr((REAL)(L.dragArea.x + 8), (REAL)L.dragArea.y,
                         (REAL)(L.dragArea.w - 10), (REAL)L.dragArea.h);
                g.DrawString(wt.c_str(), (int)wt.size(), &tfont, tr, &sf, &tb);
            }
        }
```

- [x] **Step 3: 双击拖动区重命名**

`src/ui/NoteWindow.h` 第 28 行 `OnDestroy` 声明后加:

```cpp
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint pt);
```

`src/ui/NoteWindow.cpp` 消息映射(`ON_WM_DESTROY()` 前后均可)加:

```cpp
    ON_WM_LBUTTONDBLCLK()
```

实现(放在 `OnLButtonDown` 定义之后):

```cpp
void CNoteWindow::OnLButtonDblClk(UINT, CPoint pt) {
    // 双击标题栏拖动区 → 重命名;预填现有标题,清空确认 = 清除自定义标题
    if (own::hitTestTitleBar(layout(), pt.x, pt.y) != own::TitleHit::Drag) return;
    CString io(u8ToWideStr(m_note.title).c_str());
    if (!own_ui::promptText(this, _T("\x91CD\x547D\x540D"), io, /*allowEmpty=*/true)) return;  // 重命名
    m_note.title = wideToU8Str(io);
    if (m_store) m_store->updateTitle(m_note.id, m_note.title);
    Invalidate(FALSE);
}
```

- [x] **Step 4: 编译主工程**

```bat
msbuild open_windows_note.sln /p:Configuration=Debug /p:Platform=x64 /m
```
Expected: 0 error。

- [x] **Step 5: Windows 冒烟**

1. 新建文本便签,输入两行文字 → 标题栏**空白**,无重复首行。
2. 点卷起 → 标题栏显示首行;展开 → 恢复空白。
3. 双击标题栏拖动区 → 弹「重命名」,输入"工作" → 标题栏立即显示"工作";卷起也显示"工作"。
4. 再双击、清空、确认 → 回到 1 的留空状态。
5. 双击按钮区(×/图钉等)不弹重命名;双击正文编辑区不受影响。
6. 重启应用 → 标题持久。

- [x] **Step 6: Commit**

```bash
git add src/ui/NoteWindow.h src/ui/NoteWindow.cpp
git commit -m "feat: 便签窗标题栏展开留空/卷起回落,双击重命名

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: 管理器列表——右键「重命名…」

**Files:**
- Modify: `src/ui/NoteListView.cpp`(菜单构建约 117-118 行之间、命令分发约 183 行 `cmd == 3` 分支后)

**Interfaces:**
- Consumes: Task 2 `m_store->updateTitle`;既有 `own_ui::promptText`、`u8ToWide`/`wideToU8`(同文件 18/25 行)、`INoteWindowHost::refreshNoteWindow` / `closeNoteWindow`。
- Produces: 无(终端 UI 改动)。

- [x] **Step 1: 菜单项**

`NoteListView.cpp` 「打开」(117 行)与「隐藏该便签」之间插入:

```cpp
    menu.AppendMenu(MF_STRING, 4, _T("\x91CD\x547D\x540D\x2026"));       // 重命名…
```

- [x] **Step 2: 命令处理**

`cmd == 3`(删除)分支之后插入:

```cpp
    else if (cmd == 4) {
        CString io = u8ToWide(note->title);
        if (own_ui::promptText(m_table, _T("\x91CD\x547D\x540D"), io, /*allowEmpty=*/true)) {  // 重命名
            m_store->updateTitle(id, wideToU8(io));
            reload();
            // 可见窗口重建刷新标题;隐藏窗口直接销毁,下次「打开」按库里新标题重建
            if (m_host) {
                if (note->visible) m_host->refreshNoteWindow(id);
                else m_host->closeNoteWindow(id);
            }
        }
    }
```

- [x] **Step 3: 编译 + 全量测试**

```bat
msbuild open_windows_note.sln /p:Configuration=Debug /p:Platform=x64 /m
msbuild tests\tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
tests\x64\Debug\tests.exe
```
Expected: 0 error,tests 全绿。

- [x] **Step 4: Windows 冒烟**

1. 列表右键便签 → 「重命名…」预填当前标题,改名后列表标题列立即更新。
2. 该便签窗口若开着 → 标题栏同步显示新标题。
3. 对**隐藏中**的便签重命名 → 便签不会被意外弹出;之后「打开」显示新标题。
4. 列表按标题排序仍正常;按更新时间排序的位置**不因重命名变动**。

- [x] **Step 5: Commit**

```bash
git add src/ui/NoteListView.cpp
git commit -m "feat: 列表右键重命名便签(命令号4,清空恢复默认)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```
