# P10 v1 收尾打磨 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 冒烟验收前清空历轮审查累积的已接受项：贴窗 UX 缺陷（× 关不掉贴窗便签）、死代码/DRY、TextPrompt 加固、校验补测、Release 构建验证与正式 README——v1 打磨收官。

**Architecture:** 全部是小而独立的修缮，无新子系统。贴窗「×失效」用窗口本地 mute 位解决（× 时置位、setStickyVisible 拒绝 show、列表「打开」清位），不改落库语义。`updateNote` 整行更新器正式删除（三轮审查的事故源，已零调用）。Release|x64 只做构建+测试验证，配置已存在于 .sln。

**Tech Stack:** 同前（C++17 · MFC 静态 · doctest）。

## Global Constraints

- 语言/工具链：C++17、MFC **静态链接**、无 PCH、仅 `x64`。
- 编码：`/utf-8`；中文 UI 字面量 `_T("")` 内 `\xXXXX` + 行尾中文注释；**测试断言只用 ASCII**。
- `src/domain`/`src/data` 不得 include windows.h/afxwin.h（进 tests）。
- 构建：`"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`（下称 `$MSB`）`-p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo`（Task 4 另跑 `-p:Configuration=Release`）。
- **每次重建前**：`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`。
- 存活检查用 `tasklist //FI "IMAGENAME eq open_windows_note.exe"`。
- 提交尾注：`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。
- 分支：main 直上。基线：**102 用例 / 427 断言全绿**（P9 完成后）。

**承接的既有接口：** `CNoteWindow::setStickyVisible/stickTarget`（P9 T4）；`CNoteApp::openOrFocusNote/stickyInitialPass/applyStickyVisibility`（P9）；`StickyWindowWatcher::start`（P9）；`own::validateBackupFile` 与 `tests/test_backup_service.cpp` 的 `kTmp`（P9 T2）；`NoteStore::updateContent(id, blob, plain, now)`（P3 起）；`own_ui::promptText(parent, caption, io, allowEmpty=false)`（P9 修复版）；`SettingsDialog.cpp` 的 WM_QUIT 补发样板（P7 T5）。

**范围外（继续后置）：** toast 多显示器定位、贴窗跟随移动、导入合并模式、GitHub Actions、发布 zip 打包脚本。

---

## 文件结构

**修改：** `src/ui/NoteWindow.h/.cpp`、`src/app/NoteApp.cpp`、`src/services/StickyWindowWatcher.cpp`、`src/data/NoteStore.h/.cpp`、`src/ui/FormatBarLayout.cpp`、`src/ui/TextContentView.cpp`、`src/ui/TextPrompt.cpp`、`tests/test_notestore.cpp`、`tests/test_backup_service.cpp`、`docs/superpowers/smoke/P9-smoke-checklist.md`、`README.md`。无新文件、无 vcxproj 变更。

---

### Task 1: 贴窗 UX 修缮（× 静音位 + 空前台初始遍 + s_inst 时序）

**Files:**
- Modify: `src/ui/NoteWindow.h`, `src/ui/NoteWindow.cpp`
- Modify: `src/app/NoteApp.cpp`（openOrFocusNote、stickyInitialPass）
- Modify: `src/services/StickyWindowWatcher.cpp`（start）
- Modify: `docs/superpowers/smoke/P9-smoke-checklist.md`（补验收行）

**Interfaces:**
- Produces: `CNoteWindow::clearStickyMute()`（public inline）。语义：贴窗便签被 × 关闭后不再被前台匹配唤回（mute）；列表/toast「打开」解除 mute。
- GUI 任务：链接通过 + tests 全绿（102/427 不变）+ 启动存活。

- [ ] **Step 1: NoteWindow mute 位**

`src/ui/NoteWindow.h`：private 加 `bool m_stickyMuted = false;   // 贴窗便签被 × 关闭后静音：前台匹配不再唤回`；public 加：
```cpp
    void clearStickyMute() { m_stickyMuted = false; }   // 列表「打开」等显式打开时解除
```
`src/ui/NoteWindow.cpp`：
1. `TitleHit::Close` 分支里 `m_note.visible = false;` 之后加：
```cpp
            if (!m_note.stickTarget.empty()) m_stickyMuted = true;   // × 后不被贴窗唤回
```
2. `setStickyVisible` 开头 guard 后加：
```cpp
    if (show && m_stickyMuted) return;   // 用户已 × 关：保持隐藏
```

- [ ] **Step 2: NoteApp 两处**

`src/app/NoteApp.cpp`：
1. `openOrFocusNote` 的已有窗分支，`w->ShowWindow(SW_SHOW);` 之后加：
```cpp
        w->clearStickyMute();            // 显式打开：恢复贴窗参与
```
2. `stickyInitialPass` 的 `if (!fg) return;` 改为：
```cpp
    if (!fg) { applyStickyVisibility("", ""); return; }   // 无前台窗：贴窗便签先藏
```

- [ ] **Step 3: Watcher s_inst 时序**

`src/services/StickyWindowWatcher.cpp` `start()`：把 `s_inst = this;` 移到 SetWinEventHook 成功之后：
```cpp
bool StickyWindowWatcher::start() {
    if (m_hook) return true;
    m_hook = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                               nullptr, proc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (m_hook) s_inst = this;           // 钩上才登记：s_inst 非空 <=> 钩子在
    return m_hook != nullptr;
}
```

- [ ] **Step 4: 冒烟清单补行**

`docs/superpowers/smoke/P9-smoke-checklist.md` 的「贴到应用窗口」小节末尾加：
```markdown
- [ ] 贴窗便签点 × 关闭：目标窗再置前台也不再弹出；列表「打开」该便签后贴窗行为恢复
```

- [ ] **Step 5: 构建 + 全绿 + 存活 + Commit**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist //FI "IMAGENAME eq open_windows_note.exe" | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 全绿 102/427；ALIVE。
```bash
git add src/ui/NoteWindow.h src/ui/NoteWindow.cpp src/app/NoteApp.cpp src/services/StickyWindowWatcher.cpp docs/superpowers/smoke/P9-smoke-checklist.md
git commit -m "fix(ui/app): sticky note close-mute — X stays closed until reopened; null-foreground initial hide; hook-then-register

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: 死代码清理 + 校验补测（updateNote 删除 / fN / ladder DRY / 注释去重 / 两道闸补测）

**Files:**
- Modify: `src/data/NoteStore.h`, `src/data/NoteStore.cpp`（删 updateNote）
- Modify: `tests/test_notestore.cpp`（改写 updateNote 用例）
- Modify: `src/ui/NoteWindow.cpp`（删未用 fN）
- Modify: `src/ui/FormatBarLayout.cpp`（ladder DRY）
- Modify: `src/ui/TextContentView.cpp`（合并重复注释）
- Modify: `tests/test_backup_service.cpp`（补两道闸测试）

**Interfaces:** 删除 `NoteStore::updateNote`（已零生产调用——P9 final review 确认；删除前 grep 复核）。其余无接口变化。

- [ ] **Step 1: 改写测试（先测后删）**

`tests/test_notestore.cpp` 的 `TEST_CASE("update refreshes updated_at only")`（现调 updateNote）整体替换为：
```cpp
TEST_CASE("updateContent refreshes content and updated_at only") {
    auto db = freshDb(); own::NoteStore store(db);
    own::Note n; n.title = "a";
    int64_t id = store.insertNote(n, 1000);
    REQUIRE(store.updateContent(id, {1,2}, "hello", 2000));
    auto g2 = store.getNote(id);
    CHECK(g2->plainText == "hello");
    REQUIRE(g2->contentBlob.size() == 2);
    CHECK(g2->title == "a");             // other columns untouched
    CHECK(g2->createdAt == 1000);
    CHECK(g2->updatedAt == 2000);
}
```
（`updateContent` 签名以 `src/data/NoteStore.h` 为准——`flushContent` 有现成调用。）
`tests/test_backup_service.cpp` 末尾追加（`kTmp` 是该文件既有静态）：
```cpp
TEST_CASE("validateBackupFile rejects sqlite file without notes table") {
    std::remove(kTmp);
    { own::Database db; std::string err;
      REQUIRE(db.open(kTmp, &err));
      REQUIRE(db.exec("CREATE TABLE other(x); PRAGMA user_version=1;", &err));
      db.close(); }
    std::string err;
    CHECK_FALSE(own::validateBackupFile(kTmp, &err));
    std::remove(kTmp);
}
TEST_CASE("validateBackupFile rejects db with user_version zero") {
    std::remove(kTmp);
    { own::Database db; std::string err;
      REQUIRE(db.open(kTmp, &err));
      REQUIRE(db.exec("CREATE TABLE notes(id INTEGER PRIMARY KEY);", &err));
      db.close(); }
    std::string err;
    CHECK_FALSE(own::validateBackupFile(kTmp, &err));
    std::remove(kTmp);
}
```

- [ ] **Step 2: 删除与 DRY**

1. `grep -rn "updateNote(" src/ tests/` 复核仅剩 NoteStore.h/.cpp 声明+实现（Step 1 已移走测试调用）后，删 `src/data/NoteStore.h:17` 声明与 `NoteStore.cpp` 里 `updateNote` 整个实现（约 :52-66，以实际为准；给 `updateNoteTheme` 等注释里提到「updateNote 会整行覆盖 blob」的警示语改为「(历史) 整行更新器已删除——补列请走单列更新」，保留告诫语义）。
2. `src/ui/NoteWindow.cpp` OnPaint 工具条块：删除未使用的 `Font fN(&ff, 12, FontStyleRegular, UnitPixel);` 行。
3. `src/ui/FormatBarLayout.cpp`：把 `snapIdx`/`fontSizeStep` 里重复的 ladder 数组收敛为共享文件级常量：
```cpp
static const int kLadder[] = { 160, 180, 200, 220, 240, 280, 320, 360, 480 };
static const int kLadderN = (int)(sizeof(kLadder) / sizeof(kLadder[0]));
static int snapIdx(int twips) {                       // 不小于 twips 的最近档；超顶取顶
    for (int i = 0; i < kLadderN; ++i) if (twips <= kLadder[i]) return i;
    return kLadderN - 1;
}
int fontSizeStep(int twips, bool up) {                // 先 snap 后 step，两端夹住
    int idx = snapIdx(twips);
    if (up)  { if (idx < kLadderN - 1) ++idx; }
    else     { if (idx > 0) --idx; }
    return kLadder[idx];
}
```
4. `src/ui/TextContentView.cpp`：`applyNoteFont` 上方两条语义相近的注释合并为一条（保留信息量最大的表述，删冗余行）。

- [ ] **Step 3: 构建 + 全绿 + Commit**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: 全绿（102→104 用例；断言数以实测为准）。
```bash
git add src/data/NoteStore.h src/data/NoteStore.cpp tests/test_notestore.cpp tests/test_backup_service.cpp src/ui/NoteWindow.cpp src/ui/FormatBarLayout.cpp src/ui/TextContentView.cpp
git commit -m "chore: remove whole-row updateNote (zero callers), DRY font ladder, drop dead locals, validate-gate tests

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: TextPrompt 加固（WM_QUIT 补发 + CreateEx 失败检查）

**Files:**
- Modify: `src/ui/TextPrompt.cpp`

**Interfaces:** 无变化（promptText 签名不动）。行为：模态循环遇 WM_QUIT 时补发（照 `SettingsDialog.cpp` 的样板与注释风格）；CreateEx 失败直接 return false（不进消息循环）。

- [ ] **Step 1: 实现**

`src/ui/TextPrompt.cpp`（先读全文件，适配实际变量名）：
1. CreateEx 调用处改为失败即 `return false;`（若原代码未检查返回值）。
2. 手写消息循环：`GetMessage` 返回 0 时补发并退出（模式照抄 `SettingsDialog.cpp:169-177` 的循环，含注释）：
```cpp
        if (got == 0) {
            ::PostQuitMessage((int)msg.wParam);   // WM_QUIT 不吞：外层 CWinApp::Run 才能退出
            break;
        }
```
（嵌入到该文件实际的循环结构里；若其循环是 `while (!done && GetMessage(...))` 形式，改写为与 SettingsDialog 相同的 `for(;;)` + 显式判 0 结构，行为等价。退出路径仍要走原有的 EnableWindow(TRUE)/DestroyWindow 清理。）

- [ ] **Step 2: 构建 + 全绿 + 存活 + Commit**

Run: 同 Task 1 Step 5 的命令。Expected: 全绿；ALIVE。
```bash
git add src/ui/TextPrompt.cpp
git commit -m "fix(ui): promptText modal loop — re-post WM_QUIT, bail on CreateEx failure

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: Release|x64 构建验证 + 正式 README

**Files:**
- Modify: `README.md`（整体重写）
- Modify（仅在 Release 构建报错时）: `app/open_windows_note_app.vcxproj` / `tests/tests.vcxproj` 的最小修复

**Interfaces:** 无代码接口。交付=Release 构建 0 error、Release tests 全绿、Release app 存活、README 反映 v1 全功能。

- [ ] **Step 1: Release 构建验证**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved" | head`
Expected: 无 error。若有（常见：Release 缺 /utf-8、静态运行库不一致 /MT、优化下的告警升级），做最小 vcxproj 修复并在报告中逐条说明。然后：
`./x64/Release/tests.exe 2>&1 | tail -2` 全绿；`./x64/Release/open_windows_note.exe & sleep 3; tasklist //FI "IMAGENAME eq open_windows_note.exe" | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null` → ALIVE。
> 注意：Release 运行会在 exe 同目录找/建 `notes.db`（x64/Release/）——存活验证无害，结束后可留着。

- [ ] **Step 2: README 重写**

`README.md` 整体替换为：
```markdown
# open_windows_note

Windows 桌面便签（Sticky Notes）。C++17 / MFC 静态链接，单 exe 便携免安装，SQLite 持久化。MIT 开源。

## 功能（v1）

- 三种便签类型：**富文本**（B/I/U/S、字号、文字色工具条）、**清单/待办**、**手绘涂鸦**
- 悬浮置顶便签窗：拖动、缩放、卷起、per-note 置顶、4 档不透明度、4 套配色主题一键循环
- 管理器窗口：全部便签列表、跨便签搜索、分组、标签
- 提醒/闹钟：单次与每日/每周/每月重复、贪睡、自定义提示音
- **贴到应用窗口**：便签跟随目标应用显隐（标题子串或 `class:类名` 匹配）
- 全局热键（可在设置里改键）、系统托盘常驻、开机自启（启动文件夹 .lnk，便携友好）
- 设置弹层：默认主题 / 默认透明度 / 默认字号 / 自启 / 热键
- **备份**：托盘一键导出（`VACUUM INTO` 一致性快照）与导入（校验 → 原库留 .bak → 原子替换 → 自动重启）

## 构建

- Visual Studio 2022（v143，含 MFC），仅 x64。
- 打开 `open_windows_note.sln`，选 `Debug|x64` 或 `Release|x64` 构建。
- 或命令行：`MSBuild open_windows_note.sln -p:Configuration=Release -p:Platform=x64`
- 测试：构建后运行 `x64/<配置>/tests.exe`（doctest，全绿为准）。

## 运行 / 便携

- 直接运行 `open_windows_note.exe`。数据库 `notes.db` 存于 exe 同目录（不可写时回落 `%APPDATA%\open_windows_note\`），拷走目录即带走全部数据。
- 单实例：重复启动会通知已有实例新建一条便签。

## 默认热键（设置里可改）

| 热键 | 动作 |
|---|---|
| Ctrl+Alt+N | 新建便签 |
| Ctrl+Alt+2 | 新建清单 |
| Ctrl+Alt+3 | 新建涂鸦 |
| Ctrl+Alt+M | 显示/隐藏管理器 |
| Ctrl+Alt+H | 显示/隐藏全部便签 |
| Ctrl+Alt+Q | 退出 |

## 目录结构

- `src/domain` 纯逻辑（无 Win32 依赖，随 tests 编译）
- `src/data` SQLite 数据层（Database/NoteStore/迁移/备份）
- `src/services` 托盘、热键、自启、提醒调度、贴窗监听
- `src/ui` 自绘窗口（便签、管理器列表、设置、提示框、toast）
- `src/app` 应用装配（NoteApp/宿主窗/路径）
- `tests` doctest 测试工程

## License

MIT
```

- [ ] **Step 3: Debug 回归 + Commit**

Run: Debug 全套（同 Task 1 Step 5 命令）确认 Release 期间未破坏 Debug。Expected: 全绿；ALIVE。
```bash
git add README.md
# 若 Step 1 改过 vcxproj 一并 add
git commit -m "docs: full v1 README; verify Release|x64 build + tests

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage：** 本计划非新功能，对账对象是历轮审查遗留清单：P9-L1（×失效）→T1；P9-L2（空前台）→T1；P9 s_inst 时序→T1；updateNote 死代码（P7/P9 final review 建议删）→T2；P8 fN/ladder/注释→T2;P9 validate 两道闸补测→T2；promptText WM_QUIT/CreateEx（P7 起遗留）→T3；Release 验证+README（发布前提）→T4。继续后置项已在范围外声明。✓

**2. Placeholder scan：** 无 TBD。Task 3 的「适配实际变量名」「照抄 SettingsDialog 样板」指向具体文件行样板；Task 4 Release 修复是条件步骤（无错则跳过），README 内容完整给出。✓

**3. Type consistency：** `clearStickyMute()`（T1 NoteWindow 产出）↔ T1 openOrFocusNote 调用；`updateContent(id, blob, plain, now)` 用法与 flushContent 现调一致；kTmp/Database/exec 均既有接口。✓

**已知限制：** mute 位是窗口生命周期内的（重启后贴窗便签恢复参与——符合「重启后贴窗关系保留」的冒烟语义）；「打开」解除 mute 依赖 openOrFocusNote 的已有窗分支（toast 打开与列表打开都走它）。Release 首跑会在 x64/Release 下生成 notes.db，属预期。
