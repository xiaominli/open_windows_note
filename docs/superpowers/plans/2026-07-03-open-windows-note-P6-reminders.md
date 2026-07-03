# P6 提醒/闹钟 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让提醒真正落地：到期轮询 → 右下角自绘通知窗（打开/贪睡/关闭）+ 提示音，并在管理器列表右键提供「设提醒」编辑入口（预设/自定义时间/重复/取消）。

**Architecture:** 领域层补齐纯函数（到期挑选 `pickDueReminders`、关闭/贪睡的状态迁移 `resolveReminderDismiss/Snooze`、本地时间文本 `parseLocalDateTime/formatLocalDateTime/nextDayAt`），全部走 doctest。服务层新增 `ReminderScheduler`（无 HWND，靠外部驱动 `poll(now)`，回调 `onFire`；活动中的提醒不重复触发）。表现层新增自绘通知窗 `CReminderToast`（堆上自持有、右下角堆叠、`PlaySound` 回落 `MessageBeep`）。轮询定时器寄宿在既有常驻隐藏窗 `CAppHostWindow`（`SetTimer` 30s，规格 §5「UI 线程 SetTimer 轮询」）。提醒编辑不做日期控件——用列表右键子菜单（预设 10分钟/1小时/明天9:00 + 自定义文本 `YYYY-MM-DD HH:MM` 走既有 `promptText`）。

**Tech Stack:** C++17 · MFC 静态链接 · Win32（SetTimer / PlaySound / CMenu）· SQLite（既有 reminders 表）· doctest。

## Global Constraints

- 语言/工具链：C++17（`/std:c++17`）、MFC **静态链接**、无 PCH、仅 `x64`。
- 编码：所有工程 ClCompile 带 `/utf-8`；`.cpp` 内中文 UI 字面量沿用现有惯例——`_T("")` 宽字符串用 `\xXXXX` 转义 + 行尾注释标注原文；**测试文件断言只用 ASCII**。
- 命名空间：`src/domain` 纯逻辑一律 `namespace own`，**不得** include `<afxwin.h>`/`<windows.h>`（要进 tests 工程 doctest）；`<ctime>` 允许（`ReminderRules.cpp` 已有先例）。服务层 `src/services/` 与 `src/ui/` 是 Win32/MFC 代码，仅进 app 工程。
- 构建：只能通过 `.sln` 构建。MSBuild：`"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`（下称 `$MSB`）。
- **每次重建前先杀残留**：`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`（单实例 exe 锁输出 → LNK1168）。
- 自动化达标线：纯逻辑任务=`x64/Debug/tests.exe` 全绿；服务/GUI 任务=app 工程链接通过 + 启动不崩；通知窗/声音/菜单的实际行为落 Task 7 手工冒烟。
- 每次提交末尾附：`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。
- 分支：直接在 `main` 上开发（单人、已收敛）。提交粒度按任务走。

**承接的既有接口（勿重复实现）：**
- `own::Reminder`（`src/domain/Models.h`）：`{int64_t id, noteId, dueAt; Recurrence recurrence; int recurInterval; int64_t snoozeUntil; std::string soundPath; bool enabled;}`；`enum class Recurrence : int { None=0, Daily=1, Weekly=2, Monthly=3 }`。
- `own::isDue(const Reminder&, int64_t now)` / `computeNextDue(const Reminder&, int64_t firedAt)` / `snooze(int64_t now, int minutes)`（`src/domain/ReminderRules.h`，P1 完成，已有测试）。
- `own::NoteStore`（`src/data/NoteStore.h`）：`insertReminder(Reminder)→int64_t`、`updateReminder(const Reminder&)→bool`、`deleteReminder(int64_t)→bool`、`remindersOfNote(int64_t)→vector<Reminder>`、`enabledReminders()→vector<Reminder>`、`getNote(int64_t)→optional<Note>`。
- `own::noteTitleText(const Note&)→std::string`（`src/domain/NoteListFormat.h`）。
- `CAppHostWindow`（`src/app/AppHostWindow.h`）：常驻隐藏顶层窗，已有托盘/热键派发；本计划给它加一个 30s 定时器。
- `INoteWindowHost`（`src/app/NoteWindowHost.h`）：`openOrFocusNote(int64_t)` 等。
- `own_ui::promptText(CWnd* parent, const CString& title, CString& io)→bool`（`src/ui/TextPrompt.h`）。
- `CNoteListView::onContextMenu(int row)`（`src/ui/NoteListView.cpp`）：既有右键菜单，命令 id 已占用 1/2/3、100/199/200+、299/300+；**本计划用 400–419**。

**语义决策（实现按此执行）：**
- 通知「关闭」：一次性提醒 → `enabled=0`；重复提醒 → `dueAt=computeNextDue(r, now)`、`snoozeUntil=0`。「打开」= 打开便签 + 同「关闭」的落库。「贪睡」= `snoozeUntil=now+10min`，其余不动。
- 每条 note 的 UI 只编辑**第一条 enabled 提醒**（schema 允许多条；多提醒管理超出 v1 UI 范围）。
- 启动时立即 `poll` 一次：错过的过期提醒开机即弹（预期行为，写进冒烟清单）。
- 提示音：`soundPath` 非空 → `PlaySoundW(SND_FILENAME|SND_ASYNC|SND_NODEFAULT)`，失败或为空 → `MessageBeep(MB_OK)`（规格 §6）。v1 无声音选择 UI，`sound_path` 手工写库验证。

**本阶段范围外（声明）：** 提醒声音选择 UI、多提醒管理 UI、日期选择控件（随「设置弹层」计划）；列表新增独立提醒列（用标题前缀 ⏰ 指示，规格 §3 提醒图标的最小满足）。

---

## 文件结构（本计划新增/修改）

**新增 · 纯逻辑（`namespace own`，进 tests + app）：**
- `src/domain/DateTimeText.h`, `src/domain/DateTimeText.cpp` — 本地时间 `"YYYY-MM-DD HH:MM"` 解析/格式化 + 明天 HH:MM 预设。
- `src/domain/ReminderRules.h/.cpp`（修改）— 加 `pickDueReminders` / `resolveReminderDismiss` / `resolveReminderSnooze`。

**新增 · 服务层（仅进 app）：**
- `src/services/ReminderScheduler.h`, `src/services/ReminderScheduler.cpp` — 到期轮询 + 活动集去重 + `onFire` 回调。

**新增 · 表现层（仅进 app）：**
- `src/ui/ReminderToast.h`, `src/ui/ReminderToast.cpp` — 自绘右下角通知窗（堆上自持有）。

**修改：**
- `src/app/AppHostWindow.h/.cpp` — 30s 提醒定时器 + `onReminderTick` 回调。
- `src/app/NoteApp.h/.cpp` — 持有 `ReminderScheduler`，接线 toast/定时器/启动首查。
- `src/ui/NoteListView.cpp` — 右键「设提醒」子菜单 + 标题 ⏰ 前缀。
- `tests/test_reminder_rules.cpp`、`tests/test_notestore.cpp`、新增 `tests/test_datetime_text.cpp`。
- `tests/tests.vcxproj`、`app/open_windows_note_app.vcxproj` — 登记新文件；app 链接加 `winmm.lib`。
- `docs/superpowers/smoke/P6-smoke-checklist.md` — 新增。

---

### Task 1: 提醒状态迁移 + 到期挑选（纯逻辑）

**Files:**
- Modify: `src/domain/ReminderRules.h`, `src/domain/ReminderRules.cpp`
- Test: `tests/test_reminder_rules.cpp`（追加）, `tests/test_notestore.cpp`（追加）

**Interfaces:**
- Consumes: 既有 `own::isDue` / `computeNextDue` / `snooze`；`own::Reminder`。
- Produces（全 HWND-free，后续 Task 3/4 消费）:
  - `std::vector<Reminder> own::pickDueReminders(const std::vector<Reminder>& rs, int64_t now, const std::vector<int64_t>& skipIds);` — 返回 `isDue(r,now)` 且 id 不在 `skipIds` 里的项。
  - `Reminder own::resolveReminderDismiss(Reminder r, int64_t now);` — `snoozeUntil=0`；无重复→`enabled=false`；有重复→`dueAt=computeNextDue(r,now)`（结果为 0 时兜底 `enabled=false`）。
  - `Reminder own::resolveReminderSnooze(Reminder r, int64_t now, int minutes);` — `snoozeUntil=snooze(now,minutes)`，其余字段不动。

- [ ] **Step 1: 写失败测试**

`tests/test_reminder_rules.cpp` 末尾追加：
```cpp
TEST_CASE("computeNextDue monthly characterization: Jan31 +1mo drifts to Mar 3") {
    // P1 遗留决策：addMonthsUtc 溢出规整（2026-01-31 + 1mo → 2026-03-03 UTC）。
    // 1769817600 = 2026-01-31 00:00:00Z, 1772496000 = 2026-03-03 00:00:00Z
    own::Reminder r; r.recurrence = own::Recurrence::Monthly; r.recurInterval = 1;
    r.dueAt = 1769817600;
    CHECK(own::computeNextDue(r, 1769817600) == 1772496000);
}

TEST_CASE("resolveReminderDismiss disables one-shot and clears snooze") {
    own::Reminder r; r.dueAt = 1000; r.snoozeUntil = 500;
    r.recurrence = own::Recurrence::None; r.enabled = true;
    auto x = own::resolveReminderDismiss(r, 2000);
    CHECK_FALSE(x.enabled);
    CHECK(x.snoozeUntil == 0);
    CHECK(x.dueAt == 1000);          // 一次性不动 dueAt
}

TEST_CASE("resolveReminderDismiss advances recurring reminder") {
    own::Reminder r; r.dueAt = 1000; r.snoozeUntil = 999;
    r.recurrence = own::Recurrence::Daily; r.recurInterval = 1; r.enabled = true;
    auto x = own::resolveReminderDismiss(r, 1000);
    CHECK(x.enabled);
    CHECK(x.dueAt == 1000 + 86400);
    CHECK(x.snoozeUntil == 0);
}

TEST_CASE("resolveReminderSnooze only sets snoozeUntil") {
    own::Reminder r; r.dueAt = 1000; r.enabled = true;
    auto x = own::resolveReminderSnooze(r, 2000, 10);
    CHECK(x.snoozeUntil == 2000 + 600);
    CHECK(x.dueAt == 1000);
    CHECK(x.enabled);
}

TEST_CASE("pickDueReminders filters not-due and skip ids") {
    std::vector<own::Reminder> rs(3);
    rs[0].id = 1; rs[0].dueAt = 100;  rs[0].enabled = true;
    rs[1].id = 2; rs[1].dueAt = 100;  rs[1].enabled = true;   // 在 skip 里
    rs[2].id = 3; rs[2].dueAt = 9999; rs[2].enabled = true;   // 未到期
    auto due = own::pickDueReminders(rs, 200, { 2 });
    REQUIRE(due.size() == 1);
    CHECK(due[0].id == 1);
}
```
`tests/test_notestore.cpp` 末尾追加（关闭 P1 最终评审建议的 updateReminder/deleteReminder 直测缺口；`freshDb()` 该文件已有）：
```cpp
TEST_CASE("reminder update and delete roundtrip") {
    auto db = freshDb(); own::NoteStore s(db);
    int64_t nid = s.insertNote(own::Note{}, 1000);
    own::Reminder r; r.noteId = nid; r.dueAt = 5000;
    r.id = s.insertReminder(r);
    r.dueAt = 7000; r.recurrence = own::Recurrence::Daily;
    r.snoozeUntil = 6000; r.enabled = false;
    CHECK(s.updateReminder(r));
    auto back = s.remindersOfNote(nid);
    REQUIRE(back.size() == 1);
    CHECK(back[0].dueAt == 7000);
    CHECK(back[0].recurrence == own::Recurrence::Daily);
    CHECK(back[0].snoozeUntil == 6000);
    CHECK_FALSE(back[0].enabled);
    CHECK(s.deleteReminder(r.id));
    CHECK(s.remindersOfNote(nid).empty());
}
```

- [ ] **Step 2: 运行验证失败**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error" | head`
Expected: 编译失败（`pickDueReminders`/`resolveReminderDismiss`/`resolveReminderSnooze` 未声明）。Monthly 特征化与 NoteStore 往返用例是对既有代码的补测，实现后应直接通过。

- [ ] **Step 3: 实现**

`src/domain/ReminderRules.h` 改为：
```cpp
#pragma once
#include <cstdint>
#include <vector>
#include "domain/Models.h"
namespace own {
bool isDue(const Reminder& r, int64_t now);
int64_t computeNextDue(const Reminder& r, int64_t firedAt);
int64_t snooze(int64_t now, int minutes);
// P6: 到期挑选与通知按钮的状态迁移（纯函数，落库由调用方做）
std::vector<Reminder> pickDueReminders(const std::vector<Reminder>& rs, int64_t now,
                                       const std::vector<int64_t>& skipIds);
Reminder resolveReminderDismiss(Reminder r, int64_t now);
Reminder resolveReminderSnooze(Reminder r, int64_t now, int minutes);
}
```
`src/domain/ReminderRules.cpp` 末尾（namespace 内）追加：
```cpp
std::vector<Reminder> pickDueReminders(const std::vector<Reminder>& rs, int64_t now,
                                       const std::vector<int64_t>& skipIds) {
    std::vector<Reminder> out;
    for (const auto& r : rs) {
        bool skip = false;
        for (int64_t id : skipIds) if (id == r.id) { skip = true; break; }
        if (!skip && isDue(r, now)) out.push_back(r);
    }
    return out;
}

Reminder resolveReminderDismiss(Reminder r, int64_t now) {
    r.snoozeUntil = 0;
    if (r.recurrence == Recurrence::None) { r.enabled = false; return r; }
    int64_t next = computeNextDue(r, now);
    if (next > 0) r.dueAt = next; else r.enabled = false;
    return r;
}

Reminder resolveReminderSnooze(Reminder r, int64_t now, int minutes) {
    r.snoozeUntil = snooze(now, minutes);
    return r;
}
```

- [ ] **Step 4: 运行验证通过**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建通过，tests 全绿（71 用例 → 77 用例）。

- [ ] **Step 5: Commit**

```bash
git add src/domain/ReminderRules.h src/domain/ReminderRules.cpp tests/test_reminder_rules.cpp tests/test_notestore.cpp
git commit -m "feat(domain): reminder due-pick + dismiss/snooze transitions (pure)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: 本地时间文本（纯逻辑）

**Files:**
- Create: `src/domain/DateTimeText.h`, `src/domain/DateTimeText.cpp`
- Test: `tests/test_datetime_text.cpp`
- Modify: `tests/tests.vcxproj`

**Interfaces:**
- Produces（Task 6 菜单消费）:
  - `bool own::parseLocalDateTime(const std::string& s, int64_t& out);` — 严格 `"YYYY-MM-DD HH:MM"`（4-2-2 空格 2:2，无尾随字符），本地时区 → Unix 秒。字段越界（月>12、时>23、分>59）返回 false；日仅查 1..31（`mktime` 会规整 2 月 31 → 3 月 3，接受）。
  - `std::string own::formatLocalDateTime(int64_t t);` — 逆操作，同形状。
  - `int64_t own::nextDayAt(int64_t now, int hour, int minute);` — 本地时区「明天 hour:minute」。

- [ ] **Step 1: 写失败测试**

`tests/test_datetime_text.cpp`（新文件；用往返断言规避测试机时区差异）：
```cpp
#include "doctest.h"
#include "domain/DateTimeText.h"

TEST_CASE("parse/format local datetime roundtrip") {
    int64_t t = 0;
    REQUIRE(own::parseLocalDateTime("2026-07-03 18:30", t));
    CHECK(own::formatLocalDateTime(t) == "2026-07-03 18:30");
    int64_t t2 = 0;
    REQUIRE(own::parseLocalDateTime("2026-07-03 18:31", t2));
    CHECK(t2 - t == 60);
}

TEST_CASE("parseLocalDateTime rejects malformed input") {
    int64_t t = 0;
    CHECK_FALSE(own::parseLocalDateTime("", t));
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03", t));           // 缺时间
    CHECK_FALSE(own::parseLocalDateTime("2026-7-3 18:30", t));       // 位数不足
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03 18:30:00", t));  // 尾随秒
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03T18:30", t));     // 非空格分隔
    CHECK_FALSE(own::parseLocalDateTime("2026-13-01 10:00", t));     // 月越界
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03 24:00", t));     // 时越界
    CHECK_FALSE(own::parseLocalDateTime("2026-07-03 10:60", t));     // 分越界
}

TEST_CASE("nextDayAt lands on tomorrow at given local time") {
    int64_t now = 0;
    REQUIRE(own::parseLocalDateTime("2026-07-03 23:59", now));
    CHECK(own::formatLocalDateTime(own::nextDayAt(now, 9, 0)) == "2026-07-04 09:00");
    REQUIRE(own::parseLocalDateTime("2026-07-03 01:00", now));
    CHECK(own::formatLocalDateTime(own::nextDayAt(now, 9, 0)) == "2026-07-04 09:00");
}
```
`tests/tests.vcxproj` 的 ClCompile 组加：
```xml
    <ClCompile Include="test_datetime_text.cpp" />
    <ClCompile Include="..\src\domain\DateTimeText.cpp" />
```

- [ ] **Step 2: 运行验证失败**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error" | head`
Expected: 编译失败（`domain/DateTimeText.h` 不存在）。

- [ ] **Step 3: 实现**

`src/domain/DateTimeText.h`:
```cpp
#pragma once
#include <cstdint>
#include <string>
namespace own {
// 本地时区 "YYYY-MM-DD HH:MM" ↔ Unix 秒（提醒编辑的文本格式）
bool parseLocalDateTime(const std::string& s, int64_t& out);
std::string formatLocalDateTime(int64_t t);
int64_t nextDayAt(int64_t now, int hour, int minute);   // 明天 hour:minute（本地）
}
```
`src/domain/DateTimeText.cpp`:
```cpp
#include "domain/DateTimeText.h"
#include <ctime>
#include <cstdio>
namespace own {

static bool readInt(const char*& p, int digits, int& out) {
    out = 0;
    for (int i = 0; i < digits; ++i) {
        if (p[i] < '0' || p[i] > '9') return false;
        out = out * 10 + (p[i] - '0');
    }
    p += digits;
    return true;
}
static bool localTm(int64_t t, std::tm& out) {
    time_t tt = (time_t)t;
#if defined(_WIN32)
    return localtime_s(&out, &tt) == 0;
#else
    std::tm* p = localtime(&tt); if (!p) return false; out = *p; return true;
#endif
}

bool parseLocalDateTime(const std::string& s, int64_t& out) {
    const char* p = s.c_str();
    int y, mo, d, h, mi;
    if (!readInt(p, 4, y)  || *p++ != '-' ||
        !readInt(p, 2, mo) || *p++ != '-' ||
        !readInt(p, 2, d)  || *p++ != ' ' ||
        !readInt(p, 2, h)  || *p++ != ':' ||
        !readInt(p, 2, mi) || *p != '\0') return false;
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59) return false;
    std::tm g{};
    g.tm_year = y - 1900; g.tm_mon = mo - 1; g.tm_mday = d;
    g.tm_hour = h; g.tm_min = mi; g.tm_isdst = -1;
    time_t t = mktime(&g);
    if (t == (time_t)-1) return false;
    out = (int64_t)t;
    return true;
}

std::string formatLocalDateTime(int64_t t) {
    std::tm g{};
    if (!localTm(t, g)) return "";
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min);
    return buf;
}

int64_t nextDayAt(int64_t now, int hour, int minute) {
    std::tm g{};
    if (!localTm(now, g)) return now + 86400;
    g.tm_mday += 1; g.tm_hour = hour; g.tm_min = minute; g.tm_sec = 0; g.tm_isdst = -1;
    time_t t = mktime(&g);
    return t == (time_t)-1 ? now + 86400 : (int64_t)t;
}

} // namespace own
```

- [ ] **Step 4: 运行验证通过**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建通过，tests 全绿（+3 用例）。

- [ ] **Step 5: Commit**

```bash
git add src/domain/DateTimeText.h src/domain/DateTimeText.cpp tests/test_datetime_text.cpp tests/tests.vcxproj
git commit -m "feat(domain): local datetime parse/format + next-day preset (pure)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: ReminderScheduler（到期轮询服务）

**Files:**
- Create: `src/services/ReminderScheduler.h`, `src/services/ReminderScheduler.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 1 `own::pickDueReminders`；`own::NoteStore::enabledReminders/getNote/deleteReminder`。
- Produces（Task 5 消费）:
  - `class ReminderScheduler { void attach(own::NoteStore*); std::function<void(const own::Reminder&, const own::Note&)> onFire; void poll(int64_t now); void markResolved(int64_t reminderId); };`
  - `poll`：到期且不在活动集 → 加入活动集并回调 `onFire`；note 已被删（孤儿提醒）→ 直接 `deleteReminder` 清理。`markResolved`：通知窗关闭后移出活动集（贪睡/推进后的下次到期得以再触发）。
- 说明：类本身无 HWND、无定时器（由 host 的 `SetTimer` 驱动），但依赖 NoteStore 且触发路径是 GUI——达标线=app 链接通过。

- [ ] **Step 1: 写头**

`src/services/ReminderScheduler.h`:
```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "domain/Models.h"
namespace own { class NoteStore; }

// 到期提醒轮询：外部定时驱动 poll(now)。到期项回调 onFire；
// 通知未关（活动集内）不重复触发，关闭后 markResolved 移出。
class ReminderScheduler {
public:
    void attach(own::NoteStore* store) { m_store = store; }
    std::function<void(const own::Reminder&, const own::Note&)> onFire;
    void poll(int64_t now);
    void markResolved(int64_t reminderId);
private:
    own::NoteStore* m_store = nullptr;
    std::vector<int64_t> m_active;   // 已弹通知、尚未关闭的提醒 id
};
```

- [ ] **Step 2: 实现**

`src/services/ReminderScheduler.cpp`:
```cpp
#include "services/ReminderScheduler.h"
#include "data/NoteStore.h"
#include "domain/ReminderRules.h"
#include <algorithm>

void ReminderScheduler::poll(int64_t now) {
    if (!m_store || !onFire) return;
    auto due = own::pickDueReminders(m_store->enabledReminders(), now, m_active);
    for (const auto& r : due) {
        auto note = m_store->getNote(r.noteId);
        if (!note) { m_store->deleteReminder(r.id); continue; }   // 孤儿提醒清理
        m_active.push_back(r.id);
        onFire(r, *note);
    }
}

void ReminderScheduler::markResolved(int64_t reminderId) {
    m_active.erase(std::remove(m_active.begin(), m_active.end(), reminderId), m_active.end());
}
```

- [ ] **Step 3: 登记 + 构建**

`app/open_windows_note_app.vcxproj`：ClCompile 组加（`ReminderRules.cpp`/`DateTimeText.cpp` 此前只在 tests 工程，app 从本任务起消费，一并登记）：
```xml
    <ClCompile Include="..\src\domain\ReminderRules.cpp" />
    <ClCompile Include="..\src\domain\DateTimeText.cpp" />
    <ClCompile Include="..\src\services\ReminderScheduler.cpp" />
```
ClInclude 组加：
```xml
    <ClInclude Include="..\src\domain\ReminderRules.h" />
    <ClInclude Include="..\src\domain\DateTimeText.h" />
    <ClInclude Include="..\src\services\ReminderScheduler.h" />
```
Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved|未解析"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: app 链接通过；tests 全绿（数量不变）。

- [ ] **Step 4: Commit**

```bash
git add src/services/ReminderScheduler.h src/services/ReminderScheduler.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(services): ReminderScheduler — due-poll with in-flight dedup

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: CReminderToast（自绘通知窗）

**Files:**
- Create: `src/ui/ReminderToast.h`, `src/ui/ReminderToast.cpp`
- Modify: `app/open_windows_note_app.vcxproj`（加 `winmm.lib`）

**Interfaces:**
- Consumes: Task 1 `own::resolveReminderDismiss/resolveReminderSnooze`；`own::NoteStore::updateReminder`；`INoteWindowHost::openOrFocusNote`；`own::noteTitleText`。
- Produces（Task 5 消费）:
  - `static bool CReminderToast::show(const own::Reminder& r, const own::Note& note, own::NoteStore* store, INoteWindowHost* host, std::function<void(int64_t)> onClosed);`
  - 堆上自持有（`PostNcDestroy` → `delete this`）；工作区右下角，多个通知向上堆叠；`WS_EX_TOPMOST|WS_EX_TOOLWINDOW`、`SW_SHOWNOACTIVATE` 不抢焦点；创建即播声（`soundPath` → `PlaySoundW`，空/失败 → `MessageBeep`）。三键：打开（开便签+按关闭落库）/ 贪睡 10 分 / 关闭；任一键落库后回调 `onClosed(reminderId)` 并自毁。

- [ ] **Step 1: 写头**

`src/ui/ReminderToast.h`:
```cpp
#pragma once
#include <afxwin.h>
#include <functional>
#include "domain/Models.h"
namespace own { class NoteStore; }
class INoteWindowHost;

// 自绘提醒通知窗：工作区右下角弹出、向上堆叠，[打开][贪睡10分][关闭]。
// 堆上自持有：DestroyWindow → PostNcDestroy → delete this。
class CReminderToast : public CWnd {
public:
    static bool show(const own::Reminder& r, const own::Note& note,
                     own::NoteStore* store, INoteWindowHost* host,
                     std::function<void(int64_t)> onClosed);
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    afx_msg void OnDestroy();
    void PostNcDestroy() override { delete this; }
    DECLARE_MESSAGE_MAP()
private:
    CRect btnRect(int i) const;      // 0=打开 1=贪睡 2=关闭
    void closeToast();               // onClosed 回调 + DestroyWindow
    own::Reminder m_rem;
    own::Note m_note;
    own::NoteStore* m_store = nullptr;
    INoteWindowHost* m_host = nullptr;
    std::function<void(int64_t)> m_onClosed;
    static int s_live;               // 存活通知数 → 堆叠槽位
};
```

- [ ] **Step 2: 实现**

`src/ui/ReminderToast.cpp`:
```cpp
#include "ui/ReminderToast.h"
#include "app/NoteWindowHost.h"
#include "data/NoteStore.h"
#include "domain/ReminderRules.h"
#include "domain/NoteListFormat.h"
#include <mmsystem.h>
#include <ctime>

int CReminderToast::s_live = 0;
static const int kW = 300, kH = 96;

static CString u8ToWide(const std::string& s) {
    if (s.empty()) return CString();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CString w;
    if (n > 0) { ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.GetBuffer(n), n); w.ReleaseBuffer(n); }
    return w;
}
static void playReminderSound(const std::string& pathU8) {
    if (!pathU8.empty()) {
        CString w = u8ToWide(pathU8);
        if (::PlaySoundW(w, nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
    }
    ::MessageBeep(MB_OK);   // 规格 §6：提示音缺失回落
}

BEGIN_MESSAGE_MAP(CReminderToast, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

bool CReminderToast::show(const own::Reminder& r, const own::Note& note,
                          own::NoteStore* store, INoteWindowHost* host,
                          std::function<void(int64_t)> onClosed) {
    CReminderToast* t = new CReminderToast();
    t->m_rem = r; t->m_note = note; t->m_store = store; t->m_host = host;
    t->m_onClosed = std::move(onClosed);
    LPCTSTR cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW));
    RECT wa{ 0, 0, 1280, 800 };
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int slot = s_live;
    CRect rc(0, 0, kW, kH);
    rc.OffsetRect(wa.right - kW - 12, wa.bottom - kH - 12 - slot * (kH + 8));
    if (!t->CreateEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, cls, _T("reminder"),
                     WS_POPUP, rc, nullptr, 0)) { delete t; return false; }
    ++s_live;
    t->ShowWindow(SW_SHOWNOACTIVATE);
    t->UpdateWindow();
    playReminderSound(r.soundPath);
    return true;
}

CRect CReminderToast::btnRect(int i) const {
    CRect c; GetClientRect(&c);
    const int bw = 90, bh = 26, gap = 8;
    int x0 = c.right - 3 * bw - 2 * gap - 8;
    return CRect(x0 + i * (bw + gap), c.bottom - bh - 8,
                 x0 + i * (bw + gap) + bw, c.bottom - 8);
}

BOOL CReminderToast::OnEraseBkgnd(CDC*) { return TRUE; }

void CReminderToast::OnPaint() {
    CPaintDC dc(this);
    CRect c; GetClientRect(&c);
    dc.FillSolidRect(c, RGB(45, 45, 48));
    dc.Draw3dRect(c, RGB(90, 90, 96), RGB(20, 20, 22));
    dc.SetBkMode(TRANSPARENT);
    CFont* old = dc.SelectObject(CFont::FromHandle((HFONT)::GetStockObject(DEFAULT_GUI_FONT)));
    dc.SetTextColor(RGB(0xF2, 0xD2, 0x4A));
    CRect hd(10, 8, c.right - 10, 26);
    dc.DrawText(_T("\x23F0 \x63D0\x9192"), hd, DT_SINGLELINE | DT_VCENTER);   // ⏰ 提醒
    dc.SetTextColor(RGB(0xE0, 0xE0, 0xE0));
    CRect bd(10, 28, c.right - 10, 52);
    CString title = u8ToWide(own::noteTitleText(m_note));
    dc.DrawText(title, bd, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    static const LPCTSTR labels[3] = {
        _T("\x6253\x5F00"),                    // 打开
        _T("\x8D2A\x7761 10 \x5206"),          // 贪睡 10 分
        _T("\x5173\x95ED"),                    // 关闭
    };
    for (int i = 0; i < 3; ++i) {
        CRect b = btnRect(i);
        dc.FillSolidRect(b, RGB(62, 62, 66));
        dc.Draw3dRect(b, RGB(90, 90, 96), RGB(30, 30, 32));
        dc.DrawText(labels[i], b, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
    dc.SelectObject(old);
}

void CReminderToast::OnLButtonUp(UINT, CPoint pt) {
    int64_t now = (int64_t)time(nullptr);
    if (btnRect(0).PtInRect(pt)) {                 // 打开：开便签 + 按“关闭”落库
        if (m_host) m_host->openOrFocusNote(m_note.id);
        if (m_store) m_store->updateReminder(own::resolveReminderDismiss(m_rem, now));
        closeToast();
    } else if (btnRect(1).PtInRect(pt)) {          // 贪睡 10 分钟
        if (m_store) m_store->updateReminder(own::resolveReminderSnooze(m_rem, now, 10));
        closeToast();
    } else if (btnRect(2).PtInRect(pt)) {          // 关闭：一次性禁用 / 重复推进
        if (m_store) m_store->updateReminder(own::resolveReminderDismiss(m_rem, now));
        closeToast();
    }
}

void CReminderToast::closeToast() {
    if (m_onClosed) m_onClosed(m_rem.id);
    DestroyWindow();                               // → PostNcDestroy → delete this
}

void CReminderToast::OnDestroy() {
    --s_live;
    CWnd::OnDestroy();
}
```

- [ ] **Step 3: 登记 + 链接 winmm**

`app/open_windows_note_app.vcxproj`：ClCompile 组加 `<ClCompile Include="..\src\ui\ReminderToast.cpp" />`；ClInclude 组加 `<ClInclude Include="..\src\ui\ReminderToast.h" />`。
两个 `ItemDefinitionGroup`（Debug/Release，第 73/94 行附近）的 `<AdditionalDependencies>` 改为：
```xml
      <AdditionalDependencies>gdiplus.lib;ole32.lib;winmm.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

- [ ] **Step 4: 构建**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved|未解析"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: app 链接通过（`PlaySoundW` 由 winmm 提供）；tests 全绿。

- [ ] **Step 5: Commit**

```bash
git add src/ui/ReminderToast.h src/ui/ReminderToast.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(ui): self-drawn reminder toast — open/snooze/dismiss + sound fallback

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: 集成 — host 定时器 + NoteApp 接线

**Files:**
- Modify: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`
- Modify: `src/app/NoteApp.h`, `src/app/NoteApp.cpp`

**Interfaces:**
- Consumes: Task 3 `ReminderScheduler`；Task 4 `CReminderToast::show`；既有 `CAppHostWindow` / `CNoteApp` / `CMainFrame::reloadList`。
- Produces:
  - `CAppHostWindow`：`static const UINT kReminderTimerId = 1;`、`std::function<void()> onReminderTick;`、`void startReminderTimer();`（30s `SetTimer`）、`OnTimer` 按 id 派发。
  - `CNoteApp`：持有 `ReminderScheduler m_reminders;`；`InitInstance` 末尾 attach + 接线 onFire→toast、onReminderTick→poll，启动即 poll 一次。

- [ ] **Step 1: 改 `CAppHostWindow`**

`src/app/AppHostWindow.h` 类内 public 区加：
```cpp
    static const UINT kReminderTimerId = 1;
    std::function<void()> onReminderTick;        // 30s 提醒轮询滴答
    void startReminderTimer();
```
protected 消息处理区加：
```cpp
    afx_msg void OnTimer(UINT_PTR nIDEvent);
```
`src/app/AppHostWindow.cpp` 消息映射（`BEGIN_MESSAGE_MAP` 内）加一行：
```cpp
    ON_WM_TIMER()
```
实现（文件末尾追加）：
```cpp
void CAppHostWindow::startReminderTimer() {
    SetTimer(kReminderTimerId, 30 * 1000, nullptr);   // 规格 §5：UI 线程 SetTimer 轮询
}
void CAppHostWindow::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == kReminderTimerId) {
        if (onReminderTick) onReminderTick();
        return;
    }
    CWnd::OnTimer(nIDEvent);
}
```

- [ ] **Step 2: 改 `CNoteApp`**

`src/app/NoteApp.h`：include 区加 `#include "services/ReminderScheduler.h"`；私有成员 `HotkeyManager m_hotkeys;` 下加：
```cpp
    ReminderScheduler m_reminders;
```
`src/app/NoteApp.cpp`：顶部 include 加：
```cpp
#include "ui/ReminderToast.h"
```
`InitInstance()` 里 `for (const auto& n : notes) createAndShowNote(n);` 之后、`return TRUE;` 之前加：
```cpp
    // 提醒：调度器接线 + host 30s 轮询 + 启动即查一次（错过的过期提醒开机即弹）
    m_reminders.attach(m_store.get());
    m_reminders.onFire = [this](const own::Reminder& r, const own::Note& n) {
        CReminderToast::show(r, n, m_store.get(), this, [this](int64_t rid) {
            m_reminders.markResolved(rid);
            if (m_main) m_main->reloadList();   // ⏰ 前缀/时间随落库刷新
        });
    };
    m_host.onReminderTick = [this] { m_reminders.poll((int64_t)time(nullptr)); };
    m_host.startReminderTimer();
    m_reminders.poll((int64_t)time(nullptr));
```

- [ ] **Step 3: 构建 + 启动冒烟**

Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved|未解析" | head
./x64/Debug/tests.exe 2>&1 | tail -2
./x64/Debug/open_windows_note.exe &
sleep 3
tasklist 2>/dev/null | grep -qi open_windows_note && echo "ALIVE (scheduler wired, no crash)" || echo "NOT running"
taskkill //F //IM open_windows_note.exe 2>/dev/null
```
Expected: 链接通过；tests 全绿；启动存活。

- [ ] **Step 4: Commit**

```bash
git add src/app/AppHostWindow.h src/app/AppHostWindow.cpp src/app/NoteApp.h src/app/NoteApp.cpp
git commit -m "feat(app): reminder polling timer on host + scheduler/toast wiring

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 6: 列表右键「设提醒」菜单 + ⏰ 指示

**Files:**
- Modify: `src/ui/NoteListView.cpp`

**Interfaces:**
- Consumes: Task 2 `own::parseLocalDateTime/formatLocalDateTime/nextDayAt`；`own::NoteStore` reminder CRUD；`own_ui::promptText`。
- Produces: 右键菜单命令 id 400–419（本文件私有）：401=10分钟后、402=1小时后、403=明天9:00、404=自定义时间…、410..413=重复 无/每天/每周/每月、419=取消提醒。语义：预设/自定义 = 设 `dueAt`+`snoozeUntil=0`+`enabled=1`（无提醒则新建）；重复项仅当已有提醒时可用（否则灰）；每 note 只编辑第一条 enabled 提醒。

- [ ] **Step 1: 加 include 与 ⏰ 前缀**

`src/ui/NoteListView.cpp` 顶部 include 区加：
```cpp
#include "domain/DateTimeText.h"
```
`reload()` 里 `r.title = own::noteTitleText(n);` 之后加：
```cpp
        for (const auto& rem : m_store->remindersOfNote(n.id))
            if (rem.enabled) { r.title = "\xE2\x8F\xB0 " + r.title; break; }   // ⏰ 有提醒
```

- [ ] **Step 2: 加「设提醒」子菜单**

`onContextMenu(int row)` 里，「加标签」子菜单 append 之后、`menu.AppendMenu(MF_SEPARATOR, ...)`（删除项前的分隔线）之前，插入：
```cpp
    // 设提醒 子菜单：预设=401..404，重复=410..413，取消=419
    auto rems = m_store->remindersOfNote(id);
    const own::Reminder* cur = nullptr;
    for (const auto& x : rems) if (x.enabled) { cur = &x; break; }
    CMenu rem; rem.CreatePopupMenu();
    rem.AppendMenu(MF_STRING, 401, _T("10 \x5206\x949F\x540E"));                 // 10 分钟后
    rem.AppendMenu(MF_STRING, 402, _T("1 \x5C0F\x65F6\x540E"));                  // 1 小时后
    rem.AppendMenu(MF_STRING, 403, _T("\x660E\x5929 9:00"));                     // 明天 9:00
    rem.AppendMenu(MF_STRING, 404, _T("\x81EA\x5B9A\x4E49\x65F6\x95F4\x2026"));  // 自定义时间…
    rem.AppendMenu(MF_SEPARATOR, 0, _T(""));
    UINT recBase = cur ? MF_STRING : (MF_STRING | MF_GRAYED);
    own::Recurrence curRec = cur ? cur->recurrence : own::Recurrence::None;
    rem.AppendMenu(recBase | ((cur && curRec == own::Recurrence::None) ? MF_CHECKED : 0),
                   410, _T("\x4E0D\x91CD\x590D"));                               // 不重复
    rem.AppendMenu(recBase | (curRec == own::Recurrence::Daily   ? MF_CHECKED : 0),
                   411, _T("\x6BCF\x5929"));                                     // 每天
    rem.AppendMenu(recBase | (curRec == own::Recurrence::Weekly  ? MF_CHECKED : 0),
                   412, _T("\x6BCF\x5468"));                                     // 每周
    rem.AppendMenu(recBase | (curRec == own::Recurrence::Monthly ? MF_CHECKED : 0),
                   413, _T("\x6BCF\x6708"));                                     // 每月
    rem.AppendMenu(MF_SEPARATOR, 0, _T(""));
    rem.AppendMenu(cur ? MF_STRING : (MF_STRING | MF_GRAYED),
                   419, _T("\x53D6\x6D88\x63D0\x9192"));                         // 取消提醒
    CString remLabel = _T("\x8BBE\x63D0\x9192");                                 // 设提醒
    if (cur) remLabel += _T(" (") + u8ToWide(own::formatLocalDateTime(cur->dueAt)) + _T(")");
    menu.AppendMenu(MF_POPUP, (UINT_PTR)rem.GetSafeHmenu(), remLabel);
```

- [ ] **Step 3: 加命令处理**

`onContextMenu` 末尾（`cmd == 299` 分支之后）追加：
```cpp
    else if (cmd >= 401 && cmd <= 404) {
        int64_t now = (int64_t)time(nullptr);
        int64_t due = 0;
        bool have = false;
        if (cmd == 401)      { due = now + 600;  have = true; }
        else if (cmd == 402) { due = now + 3600; have = true; }
        else if (cmd == 403) { due = own::nextDayAt(now, 9, 0); have = true; }
        else {                                                    // 404 自定义时间…
            CString io = u8ToWide(own::formatLocalDateTime(cur ? cur->dueAt : now + 3600));
            if (own_ui::promptText(m_table,
                    _T("\x63D0\x9192\x65F6\x95F4 (YYYY-MM-DD HH:MM)"), io)) {    // 提醒时间
                if (own::parseLocalDateTime(wideToU8(io), due)) have = true;
                else AfxMessageBox(
                    _T("\x65F6\x95F4\x683C\x5F0F\x65E0\x6548\x3002\x5E94\x4E3A YYYY-MM-DD HH:MM")); // 时间格式无效。应为…
            }
        }
        if (have) {
            if (cur) {
                own::Reminder r = *cur;
                r.dueAt = due; r.snoozeUntil = 0; r.enabled = true;
                m_store->updateReminder(r);
            } else {
                own::Reminder r; r.noteId = id; r.dueAt = due;
                m_store->insertReminder(r);
            }
            reload();
        }
    }
    else if (cmd >= 410 && cmd <= 413 && cur) {
        own::Reminder r = *cur;
        r.recurrence = (own::Recurrence)(cmd - 410);
        m_store->updateReminder(r);
        reload();
    }
    else if (cmd == 419 && cur) { m_store->deleteReminder(cur->id); reload(); }
```

- [ ] **Step 4: 构建 + 启动冒烟**

Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved|未解析" | head
./x64/Debug/tests.exe 2>&1 | tail -2
./x64/Debug/open_windows_note.exe &
sleep 3
tasklist 2>/dev/null | grep -qi open_windows_note && echo "ALIVE" || echo "NOT running"
taskkill //F //IM open_windows_note.exe 2>/dev/null
```
Expected: 链接通过；tests 全绿；启动存活。

- [ ] **Step 5: Commit**

```bash
git add src/ui/NoteListView.cpp
git commit -m "feat(ui): note list reminder submenu (presets/custom/recurrence) + alarm marker

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 7: P6 手工冒烟清单

**Files:**
- Create: `docs/superpowers/smoke/P6-smoke-checklist.md`

- [ ] **Step 1: 写清单**

`docs/superpowers/smoke/P6-smoke-checklist.md`:
```markdown
# P6 手工冒烟清单（提醒/闹钟）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

## 设提醒（管理器列表右键）
- [ ] 右键行 →「设提醒」→「10 分钟后」：子菜单标题变为「设提醒 (YYYY-MM-DD HH:MM)」，标题列出现 ⏰ 前缀
- [ ] 「自定义时间…」：预填当前值；输入 `2030-01-01 08:00` 生效；输入 `abc` 弹「时间格式无效」
- [ ] 「明天 9:00」：菜单时间显示为明天 09:00
- [ ] 无提醒时「不重复/每天/每周/每月/取消提醒」灰不可点；设提醒后可点，勾选态与当前重复一致
- [ ] 「取消提醒」：⏰ 消失，菜单时间后缀消失

## 触发与通知
- [ ] 自定义设 1 分钟后到期 → ≤30 秒轮询误差内右下角弹自绘通知（⏰ 提醒 + 便签标题 + 三键），伴随系统提示音（无 sound_path 时 MessageBeep）
- [ ] 通知不抢焦点（弹出时正在打字的窗口不失焦）
- [ ] [打开]：便签窗弹出/聚焦；通知消失；一次性提醒 ⏰ 消失（enabled=0）
- [ ] [贪睡 10 分]：通知消失；10 分钟后（±30s）再次弹出
- [ ] [关闭] 一次性：不再弹，⏰ 消失
- [ ] [关闭] 重复(每天)：菜单里 dueAt 推进 +1 天，⏰ 保留
- [ ] 两条 note 同时到期：通知右下角向上堆叠、互不遮挡，各自可独立操作
- [ ] 通知未关时不重复触发（等一个轮询周期不新弹同一提醒）

## 声音与重启
- [ ] `sqlite3 notes.db "update reminders set sound_path='C:\\Windows\\Media\\Alarm01.wav'"` 后触发：播放该 wav
- [ ] sound_path 指向不存在文件：回落 MessageBeep，不崩
- [ ] 设过期时间（如 1 分钟后）→ 退出程序 → 等到期后再启动：启动即弹该提醒（开机补触发）
- [ ] 重启后 ⏰ 前缀与菜单时间与库中一致（持久化）
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/smoke/P6-smoke-checklist.md
git commit -m "docs: P6 reminders manual smoke checklist

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage（对照设计文档）：**
- §1.3「提醒/闹钟（重复 + 贪睡 + 自定义提示音）」→ 重复=Task 1 迁移 + Task 6 菜单；贪睡=Task 1/4；自定义提示音=Task 4 `PlaySoundW`（选择 UI 声明范围外，`sound_path` 手工写库冒烟）。✓
- §3 `ReminderScheduler`「UI 线程 SetTimer 轮询到期提醒→自绘通知窗+PlaySound；computeNextDue 处理重复/贪睡」→ Task 3（poll）+ Task 5（host SetTimer 30s）+ Task 4（自绘 toast + 声音）。✓
- §5.6 数据流「定时器→命中(due_at<=now && enabled && 过snooze)→通知+声音→打开/贪睡(重算 snooze_until)/关闭(重复算下次 due_at,否则 enabled=0)」→ `isDue`（既有）+ `resolveReminderDismiss/Snooze`（Task 1）逐条对应。✓
- §5.4「右键→…设提醒…」→ Task 6 子菜单。✓
- §3 `CNoteListView`「提醒图标」→ 标题 ⏰ 前缀（最小满足，独立列声明范围外）。✓
- §6「提示音文件缺失→回落 MessageBeep」→ Task 4 `SND_NODEFAULT` 失败回落。✓
- §7「提醒逻辑纯函数、now 注入、doctest」→ Task 1/2 全部纯函数注入 now；P1 遗留的 Monthly 特征化测试与 updateReminder/deleteReminder 直测在 Task 1 补齐。✓

**2. Placeholder scan:** 无 TBD/TODO；每步给出可编译代码或精确到行的改动位置。

**3. Type consistency:**
- `pickDueReminders(vector<Reminder>, int64_t, vector<int64_t>)`（Task 1）↔ Task 3 `poll` 调用（`m_active` 即 `vector<int64_t>`）。✓
- `resolveReminderDismiss/Snooze(Reminder, int64_t[, int])→Reminder`（Task 1）↔ Task 4 `updateReminder(own::resolve...)`。✓
- `parseLocalDateTime(string,int64_t&)/formatLocalDateTime(int64_t)/nextDayAt(int64_t,int,int)`（Task 2）↔ Task 6 调用。✓
- `ReminderScheduler::attach/onFire/poll/markResolved`（Task 3）↔ Task 5 接线；`onFire(const Reminder&, const Note&)` ↔ `CReminderToast::show(r, n, ...)`（Task 4）。✓
- `CReminderToast::show(..., std::function<void(int64_t)>)` 的 `onClosed(rid)` ↔ Task 5 `markResolved(rid)`。✓
- `CAppHostWindow::kReminderTimerId/onReminderTick/startReminderTimer`（Task 5 内自洽）。✓

**已知限制（执行者须知）：** 通知窗/声音/定时器行为无法自动化，GUI 任务以「链接通过 + 启动存活」为自动化达标线，行为落 Task 7 手工清单。`m_rem` 在 toast 存活期间是快照——若期间用户经菜单改了该提醒，按钮落库会覆盖菜单改动（v1 接受，窗口极短）。⏰(U+23F0) 依赖字体回退，若显示为方框，改用 `"[!] "` 前缀即可（只动 Task 6 Step 1 一行）。`nextDayAt`/`parseLocalDateTime` 依赖本地时区 DST 规整（`tm_isdst=-1`），中国时区无 DST 无影响。
