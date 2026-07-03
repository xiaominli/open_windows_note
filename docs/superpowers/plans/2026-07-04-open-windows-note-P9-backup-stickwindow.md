# P9 导入/导出备份 + 贴到应用窗口 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 托盘一键导出/导入整库备份（导入=校验→备份现库→替换→自重启），便签可「贴到应用窗口」——目标窗口在前台时便签显示、否则隐藏（标题/类名匹配，spec §1.3/§3 `StickyWindowWatcher`/`BackupService`，v1 收官件）。

**Architecture:** 备份走数据层：`own::BackupService`（`src/data/`，无 Win32 依赖、可进 tests）用 `VACUUM INTO` 导出、开库跑 `integrity_check`+schema 探测做校验；导入编排在 `CNoteApp`（冲洗窗口→关库→备份现库→覆盖→释放单例互斥→ShellExecute 自重启）。贴窗走服务层：`StickyWindowWatcher`（`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`，忽略本进程事件）把前台窗口标题/类名（UTF-8）回调给 `CNoteApp`，逐个开着的便签窗按纯函数 `own::matchesStickTarget` 决定**瞬态**显隐（不写 `visible` 标志）。匹配规则、SQL 字面量转义、备份文件名格式全部纯函数 TDD。列表右键新增「贴到窗口…」，落库走专用 `updateNoteStick`（沿用 updateNoteTheme/Group 的单列更新模式——**禁止**整行 `updateNote`）。

**Tech Stack:** C++17 · MFC 静态链接 · SQLite（VACUUM INTO ≥3.27，amalgamation 满足）· SetWinEventHook · CFileDialog · doctest。

## Global Constraints

- 语言/工具链：C++17（`/std:c++17`）、MFC **静态链接**、无 PCH、仅 `x64`。
- 编码：ClCompile 全部 `/utf-8`；中文 UI 字面量在 `_T("")` 里用 `\xXXXX` 转义 + 行尾中文注释；**测试断言只用 ASCII**。
- 命名空间：`src/domain`、`src/data` 一律 `namespace own` 且**不得** include `<afxwin.h>`/`<windows.h>`（都进 tests 工程）。`src/services`/`src/ui`/`src/app` 仅进 app 工程。
- 构建：只能通过 `.sln`。MSBuild 路径 `"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`（下称 `$MSB`），参数 `-p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo`。
- **每次重建前先杀残留**：`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`（LNK1168）。
- 应用存活检查用 `tasklist //FI "IMAGENAME eq open_windows_note.exe"`（`tasklist | grep` 会因代码页误报 DEAD）。
- 自动化达标线：纯逻辑/数据任务=`./x64/Debug/tests.exe` 全绿；GUI 任务=链接通过 + 启动存活 3 秒；交互行为落 Task 6 手工冒烟。
- 每次提交末尾附：`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。
- 分支：直接在 `main` 上开发。当前基线：**92 用例 / 388 断言全绿**（P8 完成后）。

**承接的既有接口（勿重复实现）：**
- `own::Note.stickTarget`（`src/domain/Models.h:27`，UTF-8，空=不贴）与 `notes.stick_target` 列（P1 建表即有）——`readRow/insertNote/updateNote` 已带此列，**缺的只是单列更新与消费方**。
- `own::Database`（`src/data/Database.h`）：`open/exec/close/integrityOk/userVersion/handle`。`own::Statement`（`src/data/Statement.h`）：列读取方法 `columnInt64/columnText`（P7 已核实）。
- `own::NoteStore`：`updateNoteTheme/updateNoteGroup` 单列更新样板（`src/data/NoteStore.cpp` 末尾）；`getNote/query`。
- `own::searchNormalize(const std::string&)`（`src/domain/SearchText.h`）——大小写折叠用它（列表搜索同款）。
- `CNoteApp`（`src/app/NoteApp.h/.cpp`）：`m_notes`（vector<unique_ptr<CNoteWindow>>，仅存**开着的**窗）、`createAndShowNote/findNote/openOrFocusNote/closeNoteWindow/refreshNoteWindow`（refresh=关窗重开、从 DB 取新）、`m_singleton` 互斥、`m_gdiplusToken`、`ExitInstance`。DB 路径 `own::resolveDbPathWin()`（`src/app/AppPaths.h`）、开库 `own::openDatabaseAtPath`（`src/app/DbBootstrap.h`）。
- `CNoteWindow`：`noteId()`、私有 `flushContent()`（脏才落盘）、`m_note`；`Create` 里 `ShowWindow(SW_SHOWNOACTIVATE)`。
- `CAppHostWindow::showTrayMenu()`：菜单 id 1..7 已占用（7=设置…）；回调成员是 `std::function` 惯例。
- `own_ui::promptText(CWnd*, const CString&, CString&)`；`NoteListView.cpp` 已有 `u8ToWide/wideToU8` 静态助手与右键菜单 id 段：1/2/3、100/199/200+i、299/300+i、400-419（提醒）——**本计划用 430**。
- tests 的 `freshDb()`（`tests/test_notestore.cpp`）：开内存库 + 迁移。

**语义决策：**
- **贴窗匹配**：`pattern` 以 `"class:"` 开头 → 大小写不敏感**子串**匹配窗口类名（去掉前缀后）；否则大小写不敏感子串匹配窗口标题；空 pattern → 不匹配。折叠用 `searchNormalize`。
- **贴窗显隐是瞬态的**：只 `ShowWindow`，不写 `visible` 标志、不落库；仅作用于**当前开着窗**的便签。前台切到本进程自己的窗口时**忽略事件**（否则点便签会把便签自己藏掉）。
- 设了贴窗的便签在启动时若无窗则补建窗，随后立即按当前前台窗做一次初始显隐。
- 清除贴窗目标（输入空串）→ 便签恢复常显（refresh 重开）。
- **导出**：`VACUUM INTO`（目标文件已存在则先删——CFileDialog 已确认过覆盖）；导出前冲洗所有开着的便签（`flushNow`）。
- **导入=替换+自重启**（spec 允许「替换/合并」取替换）：校验失败即止；确认框警告「将替换当前全部数据并重启」；现库先复制为 `<db>.bak` 再覆盖；释放单例互斥后 `ShellExecuteW` 自身 + `PostQuitMessage`。
- 备份文件名默认 `notes-backup-YYYYMMDD-HHMM.db`（本地时间；纯函数只做格式化，localtime 在调用方）。

**本阶段范围外（声明）：** 导入合并模式、备份加密/压缩、贴到网页 URL（v2）、贴窗跟随目标窗口移动（v1 仅前台显隐，与 spec §5.7 一致）、多目标贴窗（一便签一 pattern）。

---

## 文件结构

**新增：**
- `src/domain/StickyRules.h/.cpp` — `matchesStickTarget` 纯函数（tests + app）。
- `src/domain/BackupRules.h/.cpp` — `escapeSqlLiteral` + `defaultBackupName` 纯函数（tests + app）。
- `src/data/BackupService.h/.cpp` — `exportBackup`/`validateBackupFile`（tests + app，无 Win32）。
- `src/services/StickyWindowWatcher.h/.cpp` — WinEvent 钩子（仅 app）。
- `tests/test_sticky_backup_rules.cpp`、`tests/test_backup_service.cpp` — doctest。

**修改：**
- `src/data/NoteStore.h/.cpp` — `updateNoteStick`。
- `src/app/AppHostWindow.h/.cpp` — 托盘 8=导出备份… 9=导入备份… + 回调。
- `src/app/NoteApp.h/.cpp` — 导出/导入编排 + 贴窗接线。
- `src/ui/NoteWindow.h/.cpp` — `stickTarget()/setStickyVisible()/flushNow()`。
- `src/ui/NoteListView.cpp` — 菜单 430「贴到窗口…」。
- `tests/test_notestore.cpp` — updateNoteStick 用例。
- 两个 vcxproj 登记新文件。
- `docs/superpowers/smoke/P9-smoke-checklist.md` — 新增。

---

### Task 1: 纯规则 + 单列更新（matchesStickTarget / escapeSqlLiteral / defaultBackupName / updateNoteStick）

**Files:**
- Create: `src/domain/StickyRules.h`, `src/domain/StickyRules.cpp`, `src/domain/BackupRules.h`, `src/domain/BackupRules.cpp`
- Modify: `src/data/NoteStore.h`, `src/data/NoteStore.cpp`
- Test: `tests/test_sticky_backup_rules.cpp`（新）, `tests/test_notestore.cpp`（追加）
- Modify: `tests/tests.vcxproj`, `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Produces（Task 2/3/4/5 消费，签名逐字一致）:
  - `bool own::matchesStickTarget(const std::string& titleU8, const std::string& classU8, const std::string& pattern);`
  - `std::string own::escapeSqlLiteral(const std::string& s);` — 单引号翻倍（`'`→`''`），其余原样。
  - `std::string own::defaultBackupName(int year, int month, int day, int hour, int minute);` — `notes-backup-YYYYMMDD-HHMM.db`，字段零填充。
  - `bool own::NoteStore::updateNoteStick(int64_t noteId, const std::string& target);` — **只**改 `stick_target` 列。

- [ ] **Step 1: 写失败测试**

`tests/test_sticky_backup_rules.cpp`（新文件）：
```cpp
#include "doctest.h"
#include "domain/StickyRules.h"
#include "domain/BackupRules.h"

TEST_CASE("stick pattern matches window title case-insensitively as substring") {
    CHECK(own::matchesStickTarget("Untitled - Notepad", "NotepadClass", "notepad"));
    CHECK(own::matchesStickTarget("MY REPORT.docx - Word", "OpusApp", "report"));
    CHECK_FALSE(own::matchesStickTarget("Calculator", "AppFrame", "notepad"));
}
TEST_CASE("class: prefix matches window class, not title") {
    CHECK(own::matchesStickTarget("anything", "ConsoleWindowClass", "class:consolewindow"));
    CHECK_FALSE(own::matchesStickTarget("class:consolewindow in title", "Other", "class:consolewindow"));
    CHECK_FALSE(own::matchesStickTarget("x", "ConsoleWindowClass", "class:opusapp"));
}
TEST_CASE("empty pattern never matches") {
    CHECK_FALSE(own::matchesStickTarget("Untitled - Notepad", "NotepadClass", ""));
}
TEST_CASE("class: prefix with empty rest never matches") {
    CHECK_FALSE(own::matchesStickTarget("t", "c", "class:"));
}
TEST_CASE("escapeSqlLiteral doubles single quotes only") {
    CHECK(own::escapeSqlLiteral("plain") == "plain");
    CHECK(own::escapeSqlLiteral("o'brien") == "o''brien");
    CHECK(own::escapeSqlLiteral("''") == "''''");
    CHECK(own::escapeSqlLiteral("") == "");
}
TEST_CASE("defaultBackupName formats zero-padded local fields") {
    CHECK(own::defaultBackupName(2026, 7, 4, 9, 5) == "notes-backup-20260704-0905.db");
    CHECK(own::defaultBackupName(2026, 12, 31, 23, 59) == "notes-backup-20261231-2359.db");
}
```
`tests/test_notestore.cpp` 末尾追加：
```cpp
TEST_CASE("updateNoteStick changes only stick_target") {
    auto db = freshDb(); own::NoteStore s(db);
    own::Note n; n.contentBlob = {5,6}; n.plainText = "keep";
    int64_t id = s.insertNote(n, 1000);
    CHECK(s.updateNoteStick(id, "class:Notepad"));
    auto back = s.getNote(id);
    REQUIRE(back.has_value());
    CHECK(back->stickTarget == "class:Notepad");
    CHECK(back->plainText == "keep");
    REQUIRE(back->contentBlob.size() == 2);
    CHECK(s.updateNoteStick(id, ""));                 // clear
    CHECK(s.getNote(id)->stickTarget.empty());
}
```
`tests/tests.vcxproj` ClCompile 组加：
```xml
    <ClCompile Include="test_sticky_backup_rules.cpp" />
    <ClCompile Include="..\src\domain\StickyRules.cpp" />
    <ClCompile Include="..\src\domain\BackupRules.cpp" />
```

- [ ] **Step 2: 运行验证失败**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error" | head`
Expected: 编译失败（头文件/方法不存在）。

- [ ] **Step 3: 实现**

`src/domain/StickyRules.h`：
```cpp
#pragma once
#include <string>
namespace own {
// 贴窗匹配："class:" 前缀 -> 类名子串匹配；否则标题子串匹配；大小写不敏感（searchNormalize 折叠）
bool matchesStickTarget(const std::string& titleU8, const std::string& classU8, const std::string& pattern);
}
```
`src/domain/StickyRules.cpp`：
```cpp
#include "domain/StickyRules.h"
#include "domain/SearchText.h"
namespace own {
bool matchesStickTarget(const std::string& titleU8, const std::string& classU8, const std::string& pattern) {
    if (pattern.empty()) return false;
    static const std::string kClassPrefix = "class:";
    if (pattern.rfind(kClassPrefix, 0) == 0) {
        std::string p = searchNormalize(pattern.substr(kClassPrefix.size()));
        if (p.empty()) return false;
        return searchNormalize(classU8).find(p) != std::string::npos;
    }
    std::string p = searchNormalize(pattern);
    if (p.empty()) return false;
    return searchNormalize(titleU8).find(p) != std::string::npos;
}
}
```
`src/domain/BackupRules.h`：
```cpp
#pragma once
#include <string>
namespace own {
std::string escapeSqlLiteral(const std::string& s);                       // ' -> ''（SQL 字符串字面量）
std::string defaultBackupName(int year, int month, int day, int hour, int minute);
}
```
`src/domain/BackupRules.cpp`：
```cpp
#include "domain/BackupRules.h"
#include <cstdio>
namespace own {
std::string escapeSqlLiteral(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) { out += c; if (c == '\'') out += '\''; }
    return out;
}
std::string defaultBackupName(int year, int month, int day, int hour, int minute) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "notes-backup-%04d%02d%02d-%02d%02d.db",
                  year, month, day, hour, minute);
    return buf;
}
}
```
`src/data/NoteStore.h`：`updateNoteGroup` 声明旁加：
```cpp
    bool updateNoteStick(int64_t noteId, const std::string& target);   // 只改 stick_target（updateNote 会整行覆盖 blob）
```
`src/data/NoteStore.cpp`：`updateNoteGroup` 实现旁加（bind 文本列的用法照抄同文件 `insertNote` 对 `stickTarget` 的 bind）：
```cpp
bool NoteStore::updateNoteStick(int64_t noteId, const std::string& target) {
    Statement s(db_, "UPDATE notes SET stick_target=? WHERE id=?;");
    s.bind(1, target); s.bind(2, noteId);
    s.execDone();
    return true;
}
```
`app/open_windows_note_app.vcxproj`：ClCompile 加 `StickyRules.cpp`/`BackupRules.cpp`，ClInclude 加对应 .h（照 ThemeRules 条目的位置与写法）。

- [ ] **Step 4: 运行验证通过**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: 全绿（92→99 用例）。

- [ ] **Step 5: Commit**

```bash
git add src/domain/StickyRules.h src/domain/StickyRules.cpp src/domain/BackupRules.h src/domain/BackupRules.cpp src/data/NoteStore.h src/data/NoteStore.cpp tests/test_sticky_backup_rules.cpp tests/test_notestore.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(domain/data): stick-target matching, backup name/escape rules, stick_target-only update

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: BackupService（导出 + 校验，数据层可测）

**Files:**
- Create: `src/data/BackupService.h`, `src/data/BackupService.cpp`
- Test: `tests/test_backup_service.cpp`（新）
- Modify: `tests/tests.vcxproj`, `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 1 `own::escapeSqlLiteral`；`own::Database`/`own::Statement`。
- Produces（Task 3 消费）:
  - `bool own::exportBackup(Database& db, const std::string& destPathU8, std::string* err);` — 目标已存在先 `std::remove`；`VACUUM INTO '<escaped>'`。
  - `bool own::validateBackupFile(const std::string& pathU8, std::string* err);` — 能打开 + `integrity_check` ok + 存在 `notes` 表 + `user_version >= 1`。
- 注意：`std::remove`/`std::snprintf` 足够，**不引入 windows.h**（本文件进 tests）。`Database::open` 对不存在的文件会新建空库——所以 `validateBackupFile` 必须先探测文件存在（`fopen` "rb" 探测），否则「校验不存在的路径」会误报通过且残留空文件。

- [ ] **Step 1: 写失败测试**

`tests/test_backup_service.cpp`（新文件）：
```cpp
#include "doctest.h"
#include "data/BackupService.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include "data/NoteStore.h"
#include <cstdio>

static const char* kTmp = "test_backup_tmp.db";

TEST_CASE("exportBackup writes a valid, openable backup with data") {
    std::remove(kTmp);
    own::Database db;
    std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::runMigrations(db, &err));
    own::NoteStore s(db);
    own::Note n; n.plainText = "backup me";
    int64_t id = s.insertNote(n, 1000);

    CHECK(own::exportBackup(db, kTmp, &err));
    CHECK(own::validateBackupFile(kTmp, &err));

    own::Database back;
    REQUIRE(back.open(kTmp, &err));
    own::NoteStore bs(back);
    auto got = bs.getNote(id);
    REQUIRE(got.has_value());
    CHECK(got->plainText == "backup me");
    back.close();
    std::remove(kTmp);
}
TEST_CASE("exportBackup overwrites an existing destination") {
    std::remove(kTmp);
    { FILE* f = fopen(kTmp, "wb"); REQUIRE(f); fputs("junk", f); fclose(f); }
    own::Database db;
    std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::runMigrations(db, &err));
    CHECK(own::exportBackup(db, kTmp, &err));
    CHECK(own::validateBackupFile(kTmp, &err));
    std::remove(kTmp);
}
TEST_CASE("validateBackupFile rejects missing and junk files") {
    std::string err;
    CHECK_FALSE(own::validateBackupFile("no_such_file_here.db", &err));
    std::remove(kTmp);
    { FILE* f = fopen(kTmp, "wb"); REQUIRE(f); fputs("this is not sqlite", f); fclose(f); }
    CHECK_FALSE(own::validateBackupFile(kTmp, &err));
    std::remove(kTmp);
}
```
> `runMigrations` 的实际函数名/签名以 `src/data/Migrations.h` 为准（`freshDb()` 里有现成调用可抄）；对不上就改测试里的调用，语义不变。

`tests/tests.vcxproj` ClCompile 组加：
```xml
    <ClCompile Include="test_backup_service.cpp" />
    <ClCompile Include="..\src\data\BackupService.cpp" />
```

- [ ] **Step 2: 运行验证失败** — `data/BackupService.h` 不存在 → 编译失败。

- [ ] **Step 3: 实现**

`src/data/BackupService.h`：
```cpp
#pragma once
#include <string>
namespace own {
class Database;
// 备份：导出 = VACUUM INTO（原子、可在开库状态执行）；校验 = 开库 + integrity_check + schema 探测
bool exportBackup(Database& db, const std::string& destPathU8, std::string* err);
bool validateBackupFile(const std::string& pathU8, std::string* err);
}
```
`src/data/BackupService.cpp`：
```cpp
#include "data/BackupService.h"
#include "data/Database.h"
#include "data/Statement.h"
#include "domain/BackupRules.h"
#include <cstdio>

namespace own {

bool exportBackup(Database& db, const std::string& destPathU8, std::string* err) {
    std::remove(destPathU8.c_str());                 // VACUUM INTO 要求目标不存在（覆盖已在文件对话框确认）
    return db.exec("VACUUM INTO '" + escapeSqlLiteral(destPathU8) + "';", err);
}

bool validateBackupFile(const std::string& pathU8, std::string* err) {
    { // Database::open 会新建不存在的文件——先探测存在性，避免误报 + 残留空库
        FILE* f = fopen(pathU8.c_str(), "rb");
        if (!f) { if (err) *err = "file not found"; return false; }
        fclose(f);
    }
    Database db;
    if (!db.open(pathU8, err)) return false;
    if (!db.integrityOk()) { if (err) *err = "integrity check failed"; return false; }
    if (db.userVersion() < 1) { if (err) *err = "not an open_windows_note database"; return false; }
    Statement s(db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='notes';");
    if (!s.step() || s.columnInt64(0) != 1) {
        if (err) *err = "notes table missing";
        return false;
    }
    return true;
}

} // namespace own
```
> `Statement` 构造/step/columnInt64 的确切用法照抄 `NoteStore.cpp`；`integrityOk/userVersion` 是 `Database` 现成方法。若 junk 文件让 `open` 成功而 `integrity_check` 报错路径不同（SQLite 对非库文件常在首次查询才报 SQLITE_NOTADB），`integrityOk()` 内部就是查询，会覆盖到；万一 `integrityOk` 对 NOTADB 抛不出 false，就在 `userVersion()<1` 与 notes 表探测兜住——三道闸必过其一，测试是仲裁。

`app/open_windows_note_app.vcxproj`：ClCompile 加 `BackupService.cpp`，ClInclude 加 `BackupService.h`。

- [ ] **Step 4: 运行验证通过**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: 全绿（99→102 用例）。

- [ ] **Step 5: Commit**

```bash
git add src/data/BackupService.h src/data/BackupService.cpp tests/test_backup_service.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(data): backup service — VACUUM INTO export + three-gate validate

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: 托盘导出/导入入口 + NoteApp 编排（导入=替换+自重启）

**Files:**
- Modify: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`
- Modify: `src/app/NoteApp.h`, `src/app/NoteApp.cpp`
- Modify: `src/ui/NoteWindow.h`（加 `flushNow`）

**Interfaces:**
- Consumes: Task 1 `defaultBackupName`；Task 2 `exportBackup/validateBackupFile`；`resolveDbPathWin`。
- Produces:
  - `CAppHostWindow`：`std::function<void()> onExportBackup, onImportBackup;` 托盘 8=导出备份… 9=导入备份…。
  - `CNoteWindow`：`void flushNow() { flushContent(); }`（public，导出前冲洗）。
  - `CNoteApp`：私有 `void doExportBackup(); void doImportBackup();`。
- GUI 任务：达标线=链接通过 + tests 全绿（102 不变）+ 启动存活；对话框行为落 Task 6 冒烟。

- [ ] **Step 1: 托盘菜单 + 回调**

`src/app/AppHostWindow.h` 回调区（`onOpenSettings` 旁）加：
```cpp
    std::function<void()> onExportBackup;        // 托盘「导出备份…」
    std::function<void()> onImportBackup;        // 托盘「导入备份…」
```
`src/app/AppHostWindow.cpp` 的 `showTrayMenu()`：id 7（设置…）行之后加：
```cpp
    menu.AppendMenu(MF_STRING, 8, _T("\x5BFC\x51FA\x5907\x4EFD\x2026"));   // 导出备份…
    menu.AppendMenu(MF_STRING, 9, _T("\x5BFC\x5165\x5907\x4EFD\x2026"));   // 导入备份…
```
switch 里加：
```cpp
        case 8: if (onExportBackup) onExportBackup(); break;
        case 9: if (onImportBackup) onImportBackup(); break;
```

- [ ] **Step 2: NoteWindow 冲洗口**

`src/ui/NoteWindow.h` public 区加：
```cpp
    void flushNow() { flushContent(); }   // 导出备份前确保最新内容落盘
```

- [ ] **Step 3: NoteApp 导出/导入编排**

`src/app/NoteApp.h` private 区加：
```cpp
    void doExportBackup();
    void doImportBackup();
```
`src/app/NoteApp.cpp`：include 区加
```cpp
#include "data/BackupService.h"
#include "domain/BackupRules.h"
#include <afxdlgs.h>   // CFileDialog
```
文件内加静态助手（`NoteApp.cpp` 目前无 u8↔wide 助手，新增文件级静态，惯例同 `NoteWindow.cpp` 的 `u8ToWideStr`）：
```cpp
static std::wstring u8ToW(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static std::string wToU8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
```
实现两个编排（放在 `setAllNotesVisible` 之后）：
```cpp
void CNoteApp::doExportBackup() {
    for (auto& w : m_notes) if (w) w->flushNow();          // 备份含最新内容
    time_t now = time(nullptr);
    tm lt{}; localtime_s(&lt, &now);
    std::string name = own::defaultBackupName(lt.tm_year + 1900, lt.tm_mon + 1,
                                              lt.tm_mday, lt.tm_hour, lt.tm_min);
    CFileDialog dlg(FALSE, _T("db"), CString(name.c_str()),
                    OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
                    _T("SQLite \x6570\x636E\x5E93 (*.db)|*.db|\x5168\x90E8\x6587\x4EF6 (*.*)|*.*||"));  // 数据库/全部文件
    if (dlg.DoModal() != IDOK) return;
    std::string dest = wToU8((LPCWSTR)dlg.GetPathName());
    std::string err;
    if (own::exportBackup(m_db, dest, &err)) {
        AfxMessageBox(_T("\x5BFC\x51FA\x6210\x529F") + CString(_T("\x3002")));         // 导出成功。
    } else {
        AfxMessageBox(_T("\x5BFC\x51FA\x5931\x8D25\xFF1A") + CString(err.c_str()));    // 导出失败：
    }
}
void CNoteApp::doImportBackup() {
    CFileDialog dlg(TRUE, _T("db"), nullptr, OFN_FILEMUSTEXIST,
                    _T("SQLite \x6570\x636E\x5E93 (*.db)|*.db|\x5168\x90E8\x6587\x4EF6 (*.*)|*.*||"));  // 数据库/全部文件
    if (dlg.DoModal() != IDOK) return;
    std::string src = wToU8((LPCWSTR)dlg.GetPathName());
    std::string err;
    if (!own::validateBackupFile(src, &err)) {
        AfxMessageBox(_T("\x65E0\x6548\x7684\x5907\x4EFD\x6587\x4EF6\xFF1A") + CString(err.c_str()));  // 无效的备份文件：
        return;
    }
    if (AfxMessageBox(_T("\x5BFC\x5165\x5C06\x66FF\x6362\x5F53\x524D\x5168\x90E8\x6570\x636E\x5E76\x91CD\x542F\x5E94\x7528\xFF0C\x662F\x5426\x7EE7\x7EED\xFF1F"),  // 导入将替换当前全部数据并重启应用，是否继续？
                      MB_YESNO | MB_ICONWARNING) != IDYES)
        return;
    // 停掉会碰 store 的定时轮询，再拆窗、关库（顺序：先消费方后 DB）
    m_host.onReminderTick = []{};
    m_notes.clear();                                       // 析构链走 flushContent 落盘
    if (m_main) { m_main->DestroyWindow(); m_main.reset(); }
    m_store.reset();
    m_db.close();
    std::string cur = own::resolveDbPathWin();
    std::wstring wCur = u8ToW(cur), wSrc = u8ToW(src), wBak = u8ToW(cur + ".bak");
    ::CopyFileW(wCur.c_str(), wBak.c_str(), FALSE);        // 现库兜底备份（失败不阻断——可能首启无库）
    if (!::CopyFileW(wSrc.c_str(), wCur.c_str(), FALSE)) {
        AfxMessageBox(_T("\x66FF\x6362\x6570\x636E\x5E93\x6587\x4EF6\x5931\x8D25\xFF0C\x5DF2\x4FDD\x7559\x539F\x5E93\x3002\x5E94\x7528\x5373\x5C06\x9000\x51FA\x3002"));  // 替换数据库文件失败，已保留原库。应用即将退出。
        ::PostQuitMessage(0);                              // 库已关，无法继续运行——退出（原文件未动）
        return;
    }
    if (m_singleton) { ::CloseHandle(m_singleton); m_singleton = nullptr; }   // 先释放单例锁再拉新进程
    wchar_t exe[MAX_PATH]{};
    ::GetModuleFileNameW(nullptr, exe, MAX_PATH);
    ::ShellExecuteW(nullptr, L"open", exe, nullptr, nullptr, SW_SHOWNORMAL);
    ::PostQuitMessage(0);
}
```
`InitInstance` 里 `m_host.onOpenSettings = ...` 之后加接线：
```cpp
    m_host.onExportBackup = [this]{ doExportBackup(); };
    m_host.onImportBackup = [this]{ doImportBackup(); };
```
> `ExitInstance` 里的 `m_notes.clear()`/`CloseHandle(m_singleton)` 对已清空的 vector / 已置空的句柄是安全 no-op（`m_singleton` 判空后关闭），无需改动；核对属实后在报告注明。

- [ ] **Step 4: 构建 + 全绿 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist //FI "IMAGENAME eq open_windows_note.exe" | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿；ALIVE。

- [ ] **Step 5: Commit**

```bash
git add src/app/AppHostWindow.h src/app/AppHostWindow.cpp src/app/NoteApp.h src/app/NoteApp.cpp src/ui/NoteWindow.h
git commit -m "feat(app): tray export/import backup — flush-and-vacuum export, validate-replace-restart import

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: StickyWindowWatcher 服务 + NoteApp 贴窗接线

**Files:**
- Create: `src/services/StickyWindowWatcher.h`, `src/services/StickyWindowWatcher.cpp`
- Modify: `src/ui/NoteWindow.h`（`stickTarget()/setStickyVisible()`）, `src/ui/NoteWindow.cpp`
- Modify: `src/app/NoteApp.h`, `src/app/NoteApp.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 1 `own::matchesStickTarget`。
- Produces（Task 5 依赖其运行时行为）:
  - `StickyWindowWatcher`：`bool start(); void stop(); std::function<void(const std::string& titleU8, const std::string& classU8)> onForeground;`（本进程前台事件被吞掉，不回调）。
  - `CNoteWindow`：`const std::string& stickTarget() const; void setStickyVisible(bool show);`（瞬态 ShowWindow，不落库）。
  - `CNoteApp`：私有 `void applyStickyVisibility(const std::string& titleU8, const std::string& classU8); void stickyInitialPass();`。
- GUI 任务：达标线=链接通过 + tests 全绿 + 启动存活。

- [ ] **Step 1: Watcher 服务**

`src/services/StickyWindowWatcher.h`：
```cpp
#pragma once
#include <windows.h>
#include <functional>
#include <string>
// 前台窗口切换监听（SetWinEventHook, OUTOFCONTEXT：回调走本线程消息循环）。
// 本进程自己的窗口成为前台时不回调——否则点便签会触发「不匹配→隐藏自己」。
class StickyWindowWatcher {
public:
    ~StickyWindowWatcher() { stop(); }
    bool start();
    void stop();
    std::function<void(const std::string& titleU8, const std::string& classU8)> onForeground;
private:
    static void CALLBACK proc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD);
    HWINEVENTHOOK m_hook = nullptr;
};
```
`src/services/StickyWindowWatcher.cpp`：
```cpp
#include "services/StickyWindowWatcher.h"

static StickyWindowWatcher* s_inst = nullptr;   // 单实例：WinEvent 回调无用户指针

static std::string wToU8(const wchar_t* w, int len) {
    if (len <= 0) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w, len, &s[0], n, nullptr, nullptr);
    return s;
}

bool StickyWindowWatcher::start() {
    if (m_hook) return true;
    s_inst = this;
    m_hook = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                               nullptr, proc, 0, 0, WINEVENT_OUTOFCONTEXT);
    return m_hook != nullptr;   // 失败=优雅降级：贴窗不工作，其余功能不受影响
}
void StickyWindowWatcher::stop() {
    if (m_hook) { ::UnhookWinEvent(m_hook); m_hook = nullptr; }
    if (s_inst == this) s_inst = nullptr;
}
void CALLBACK StickyWindowWatcher::proc(HWINEVENTHOOK, DWORD, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    if (!hwnd || !s_inst || !s_inst->onForeground) return;
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ::GetCurrentProcessId()) return;   // 自家窗口置前不算
    wchar_t title[256]{}; int tl = ::GetWindowTextW(hwnd, title, 256);
    wchar_t cls[256]{};   int cl = ::GetClassNameW(hwnd, cls, 256);
    s_inst->onForeground(wToU8(title, tl), wToU8(cls, cl));
}
```
`app/open_windows_note_app.vcxproj`：ClCompile 加 `StickyWindowWatcher.cpp`，ClInclude 加 `.h`（照 ReminderScheduler 条目位置）。

- [ ] **Step 2: NoteWindow 瞬态显隐口**

`src/ui/NoteWindow.h` public 区加：
```cpp
    const std::string& stickTarget() const { return m_note.stickTarget; }
    void setStickyVisible(bool show);    // 贴窗瞬态显隐：不写 visible 标志、不落库
```
`src/ui/NoteWindow.cpp` 末尾加：
```cpp
void CNoteWindow::setStickyVisible(bool show) {
    if (!GetSafeHwnd()) return;
    if (!!IsWindowVisible() == show) return;
    ShowWindow(show ? SW_SHOWNOACTIVATE : SW_HIDE);
}
```

- [ ] **Step 3: NoteApp 接线**

`src/app/NoteApp.h`：include 区加 `#include "services/StickyWindowWatcher.h"`；private 加：
```cpp
    void applyStickyVisibility(const std::string& titleU8, const std::string& classU8);
    void stickyInitialPass();            // 启动时按当前前台窗做一次显隐
    StickyWindowWatcher m_sticky;        // 声明在 m_notes 之前无妨：ExitInstance 显式 stop
```
`src/app/NoteApp.cpp`：include 加 `#include "domain/StickyRules.h"`。实现（放 `setAllNotesVisible` 后）：
```cpp
void CNoteApp::applyStickyVisibility(const std::string& titleU8, const std::string& classU8) {
    for (auto& w : m_notes) {
        if (!w) continue;
        const std::string& t = w->stickTarget();
        if (t.empty()) continue;
        w->setStickyVisible(own::matchesStickTarget(titleU8, classU8, t));
    }
}
void CNoteApp::stickyInitialPass() {
    HWND fg = ::GetForegroundWindow();
    if (!fg) return;
    DWORD pid = 0; ::GetWindowThreadProcessId(fg, &pid);
    if (pid == ::GetCurrentProcessId()) { applyStickyVisibility("", ""); return; }  // 自家前台：贴窗先藏
    wchar_t title[256]{}; int tl = ::GetWindowTextW(fg, title, 256);
    wchar_t cls[256]{};   int cl = ::GetClassNameW(fg, cls, 256);
    std::wstring wt(title, title + (tl > 0 ? tl : 0)), wc(cls, cls + (cl > 0 ? cl : 0));
    applyStickyVisibility(wToU8(wt), wToU8(wc));
}
```
（`wToU8` 是 Task 3 加的文件级静态助手；若 Task 3 尚未合入按其定义先加。）
`InitInstance` 里、`for (const auto& n : notes) createAndShowNote(n);` 之后加：
```cpp
    // 贴窗便签即使当前不可见也要有窗participate（瞬态显隐需要窗存在）
    {
        own::NoteQuery qa; auto all = m_store->query(qa);
        for (const auto& n : all)
            if (!n.stickTarget.empty() && !findNote(n.id)) createAndShowNote(n);
    }
    m_sticky.onForeground = [this](const std::string& t, const std::string& c) {
        applyStickyVisibility(t, c);
    };
    m_sticky.start();                    // 失败即降级：贴窗静默不工作
    stickyInitialPass();
```
`ExitInstance` 里 `m_hotkeys.unregisterAll(...)` 之前加：
```cpp
    m_sticky.stop();
```
`doImportBackup` 的 `m_notes.clear();` 之前加一行 `m_sticky.stop();`（拆窗期间不再收前台回调）。

- [ ] **Step 4: 构建 + 全绿 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist //FI "IMAGENAME eq open_windows_note.exe" | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿；ALIVE。

- [ ] **Step 5: Commit**

```bash
git add src/services/StickyWindowWatcher.h src/services/StickyWindowWatcher.cpp src/ui/NoteWindow.h src/ui/NoteWindow.cpp src/app/NoteApp.h src/app/NoteApp.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(services/app): stick-to-window — foreground watcher, transient note visibility

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: 列表菜单「贴到窗口…」

**Files:**
- Modify: `src/ui/NoteListView.cpp`

**Interfaces:**
- Consumes: Task 1 `NoteStore::updateNoteStick`；`own_ui::promptText`；既有 `INoteWindowHost`（`m_host->refreshNoteWindow/openOrFocusNote`）。
- 菜单 id：**430**（现用段 1/2/3、100/199/200+i、299/300+i、400-419，430 空闲）。

- [ ] **Step 1: 实现**

`src/ui/NoteListView.cpp`：
1. `onContextMenu` 菜单构造处，提醒子菜单挂载行（`menu.AppendMenu(MF_POPUP, ..., remLabel);`）之后加：
```cpp
    menu.AppendMenu(MF_STRING, 430,
        note->stickTarget.empty()
            ? _T("\x8D34\x5230\x7A97\x53E3\x2026")                                    // 贴到窗口…
            : _T("\x8D34\x5230\x7A97\x53E3\xFF08\x5DF2\x8BBE\xFF09\x2026"));          // 贴到窗口（已设）…
```
2. 命令分发处（提醒命令段之后、`else if (cmd == 3)` 删除段风格一致）加：
```cpp
    else if (cmd == 430) {                          // 贴到窗口：输入标题子串或 class:类名；空=取消贴窗
        CString io = u8ToWide(note->stickTarget);
        if (own_ui::promptText(m_table, _T("\x7A97\x53E3\x6807\x9898\x5B50\x4E32\x6216 class:\x7C7B\x540D\xFF08\x7A7A=\x53D6\x6D88\xFF09"), io)) {  // 窗口标题子串或 class:类名（空=取消）
            std::string t = wideToU8(io);
            m_store->updateNoteStick(id, t);        // 只写 stick_target，不碰 blob
            if (m_host) {
                m_host->refreshNoteWindow(id);      // 开着则重开取新 target
                if (!t.empty()) m_host->openOrFocusNote(id);   // 没开则建窗以参与贴窗显隐
            }
            reload();
        }
    }
```
> `promptText` 的父窗参数按文件里既有调用抄（若现有用法传 `m_table` 以外的窗体就保持一致）；`u8ToWide/wideToU8` 是该文件既有静态助手。`note` 是菜单打开时 `getNote(id)` 的快照——本命令只据它预填输入框与选菜单文案，落库走单列 `updateNoteStick`，无整行覆盖风险。

- [ ] **Step 2: 构建 + 全绿 + 启动存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; ./x64/Debug/open_windows_note.exe & sleep 3; tasklist //FI "IMAGENAME eq open_windows_note.exe" | grep -qi open_windows_note && echo ALIVE || echo DEAD; taskkill //F //IM open_windows_note.exe 2>/dev/null`
Expected: 链接通过；tests 全绿；ALIVE。

- [ ] **Step 3: Commit**

```bash
git add src/ui/NoteListView.cpp
git commit -m "feat(ui): stick-to-window context menu — prompt pattern, stick_target-only persist

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 6: P9 手工冒烟清单

**Files:**
- Create: `docs/superpowers/smoke/P9-smoke-checklist.md`

- [ ] **Step 1: 写清单**

`docs/superpowers/smoke/P9-smoke-checklist.md`:
```markdown
# P9 手工冒烟清单（导入/导出备份 + 贴到应用窗口）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

## 导出备份
- [ ] 托盘右键出现「导出备份…」「导入备份…」（设置…下方）
- [ ] 便签里输入新内容后立即导出（不等 1 秒）：另开工具查看备份文件包含刚输入的内容（导出前冲洗生效）
- [ ] 默认文件名形如 notes-backup-20260704-1530.db；选择已存在文件会提示覆盖，确认后导出成功
- [ ] 导出的 .db 用 sqlite3 工具能打开，notes 表数据完整

## 导入备份
- [ ] 选一个非 SQLite 文件（如 .txt 改名 .db）：提示「无效的备份文件」，应用不受影响
- [ ] 选合法备份：弹「将替换当前全部数据并重启」警告；取消则一切照旧
- [ ] 确认导入：应用自动退出并重启，重启后显示的是备份里的数据
- [ ] 原库以 notes.db.bak 形式留在 exe 目录（导入前的兜底备份）
- [ ] 导入后再新建/编辑便签、热键、提醒均正常（完整重启验证）

## 贴到应用窗口
- [ ] 列表右键出现「贴到窗口…」；输入 notepad（先开一个记事本）
- [ ] 记事本置前台：便签出现；切到其它程序：便签消失；来回切换稳定
- [ ] 点便签自身/管理器：便签不消失（本进程前台事件被忽略）
- [ ] 输入 class:CabinetWClass 贴到资源管理器窗口：切前台同样生效（类名匹配）
- [ ] 菜单项变为「贴到窗口（已设）…」；再次打开预填当前模式；清空输入确认：便签恢复常显
- [ ] 重启应用：贴窗关系保留，且启动时便签按当前前台窗正确显/隐（含目标不在前台时隐藏）
- [ ] 贴窗便签的显/隐不写库：贴窗隐藏状态下重启，若目标窗在前台便签仍能出现

## 回归
- [ ] 显示全部/隐藏全部、便签关闭按钮行为正常（与贴窗瞬态显隐互不破坏落库语义）
- [ ] 换主题/换分组/设提醒菜单原有项不受 430 新增项影响
- [ ] 测试套件全绿（102 用例）
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/smoke/P9-smoke-checklist.md
git commit -m "docs: P9 backup + stick-to-window manual smoke checklist

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage：**
- §1.3「导入 / 导出备份」+ §3 `BackupService`「导出=复制 DB…导入=校验后替换/合并」→ Task 2（VACUUM INTO 导出 + 三闸校验）+ Task 3（托盘入口、替换+自重启、现库 .bak 兜底）。合并模式显式范围外。✓
- §1.3「贴到应用窗口（按标题/类名匹配）」+ §3 `StickyWindowWatcher`「SetWinEventHook(EVENT_SYSTEM_FOREGROUND)；前台窗口标题/类名匹配 stick_target→显隐对应 note；失败优雅降级」→ Task 1（匹配纯函数）+ Task 4（钩子服务/瞬态显隐/降级 start 失败不阻断）+ Task 5（设置入口）。§5.7 数据流一致。✓
- §7 冒烟「贴窗口；导入导出往返」→ Task 6。✓

**2. Placeholder scan：** 无 TBD/TODO。三处「以实际为准」均指向具体核对位置（Migrations 函数名抄 freshDb、Statement 用法抄 NoteStore.cpp、promptText 父窗抄同文件既有调用），非未决设计。

**3. Type consistency：**
- `matchesStickTarget(title, class, pattern)`（Task 1）↔ Task 4 `applyStickyVisibility` 调用参数顺序一致（title, class, target）。✓
- `escapeSqlLiteral`（Task 1）↔ Task 2 `exportBackup` 调用。✓
- `defaultBackupName(y,mo,d,h,mi)`（Task 1）↔ Task 3 `localtime_s` 字段换算（+1900/+1）调用。✓
- `exportBackup(Database&, string, string*)`/`validateBackupFile(string, string*)`（Task 2）↔ Task 3 调用。✓
- `updateNoteStick(int64_t, const std::string&)`（Task 1）↔ Task 5 调用。✓
- `StickyWindowWatcher::start/stop/onForeground`（Task 4 内部产出与接线同任务）；`CNoteWindow::stickTarget()/setStickyVisible(bool)/flushNow()`（Task 3/4 产出）↔ Task 3 `doExportBackup` 用 `flushNow`、Task 4 `applyStickyVisibility` 用前两者。✓
- 托盘 id 8/9（Task 3）不与 1..7 冲突；列表菜单 430（Task 5）不与 1/2/3、100/199/200+i(≤299)、299/300+i(≤399)、400-419 冲突。✓

**已知限制（执行者须知）：** 贴窗显隐仅对**开着窗**的便签生效（启动补窗 + 设置时 openOrFocus 已覆盖主路径；用户手动「隐藏该便签」后窗被销毁，贴窗随之失效直到重开——冒烟含常显恢复用例）。`setAllNotesVisible(隐藏全部)` 会销毁贴窗便签的窗（贴窗暂停），「显示全部」恢复。导入流程中若有提醒 toast 开着，toast 持有的 store 指针在极小窗口期内悬空——确认框到退出仅数毫秒且随进程终止，v1 接受（final review 可复核）。`GetWindowTextW` 上限 256 字符（超长标题截断——子串匹配语义下影响可忽略）。
