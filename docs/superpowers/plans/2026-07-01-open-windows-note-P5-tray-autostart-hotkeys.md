# P5 系统托盘 + 开机自启 + 全局热键正式化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给程序加系统托盘常驻(图标+右键菜单+资源管理器重启自愈)、开机自启(启动夹 .lnk)、以及把 P2/P3 临时写死的全局热键升级为「设置持久化 + 冲突安全」的正式热键系统。

**Architecture:** 新增服务层 `src/services/`。把热键的「解析/格式化/冲突检测」抽成无 HWND 纯函数(`src/domain/Hotkey`)走 doctest;`HotkeyManager`(服务)读 `SettingsStore` 的绑定覆盖、冲突安全地 `RegisterHotKey`,注册仍复用既有的 `kHotkey*` id,派发逻辑保留在 `CAppHostWindow::OnHotKey`(改动最小)。`TrayIcon` 包 `Shell_NotifyIcon`;托盘寄宿在常驻隐藏窗 `CAppHostWindow`(而非规格 §3 说的 `CMainFrame`——因为我们的生命周期模型里 `CMainFrame` 关闭=隐藏、真正贯穿进程生命周期的是隐藏 host)。`AutostartManager` 用 `IShellLink` 在启动夹建/删快捷方式。

**Tech Stack:** C++17 · MFC 静态链接 · Win32(Shell_NotifyIcon / RegisterHotKey / IShellLink COM)· SQLite(settings 表)· doctest。

## Global Constraints

- 语言/工具链:C++17(`/std:c++17`)、MFC **静态链接**、无 PCH、仅 `x64`。
- 编码:所有工程 ClCompile 带 `/utf-8`;`.cpp` 内中文字面量直接写;**测试文件里的中文/字符串断言用 ASCII**(热键字符串本身全 ASCII,无需转义)。
- 命名空间:`src/domain` 纯逻辑一律 `namespace own`,**不得** include `<afxwin.h>`/`<windows.h>`(要进 tests 工程 doctest)。故热键纯逻辑**自定义** modifier 位标志(不用 `MOD_*`)与整型 vk。服务层(`src/services`)是 Win32 代码,可用 `<windows.h>`,仅进 app 工程。
- 渲染/UI:本阶段无自绘表面新增;托盘菜单用 `CMenu`(系统菜单,允许)。
- 构建:只能通过 `.sln` 构建。MSBuild:`"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`(下称 `$MSB`)。
- **每次重建前先杀残留**:`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`(单实例 exe 锁输出 → LNK1168)。
- 自动化达标线:纯逻辑任务=`x64/Debug/tests.exe` 全绿;服务/GUI 任务=app 工程链接通过 + 启动不崩;托盘/自启/热键的实际行为落 Task 7 手工冒烟。
- 每次提交末尾附:`Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`。
- 分支:直接在 `main` 上开发(单人、已收敛)。提交粒度按任务走。

**承接的既有接口(勿重复实现):**
- `own::SettingsStore`(P2,`src/data/SettingsStore.h`):`std::string getString(const std::string& key, const std::string& def)`、`void setString(const std::string& key, const std::string& value)`、`int getInt(const std::string& key, int def)`、`void setInt(const std::string& key, int value)`。
- `CAppHostWindow`(P2/P4,`src/app/AppHostWindow.h`):常驻隐藏顶层窗;`static const UINT kHotkeyQuit=1/kHotkeyNew=2/kHotkeyNewChecklist=3/kHotkeyNewDrawing=4/kHotkeyManager=5/kHotkeyToggleAll=6`;`bool Create()`;回调 `onNewNote/onNewChecklist/onNewDrawing/onQuit/onToggleManager/onToggleAll`;`OnHotKey(id,...)` 按 id 派发;`OnDestroy` 注销热键。
- `CNoteApp`(`src/app/NoteApp.h`):`own::Database m_db; std::unique_ptr<NoteStore> m_store; CAppHostWindow m_host; std::unique_ptr<CMainFrame> m_main;` + `INoteWindowHost` 实现(`setAllNotesVisible(bool)`)。`InitInstance` 装配 host 回调、建 DB、建管理器。

**本阶段范围外(声明):** 交互式「改键对话框」UI(冲突时弹窗让用户改)——P5 只做**设置持久化 + 冲突安全跳过 + 日志**,改键 UI 随「主题+字体+设置弹层」计划做;贴到应用窗口(`StickyWindowWatcher`)、提醒调度、导入导出、主题配色——各自独立计划。托盘图标暂用系统 stock 图标(`IDI_APPLICATION`),自定义 .ico 资源后续再加。

---

## 文件结构(本计划新增/修改)

**新增 · 纯逻辑(`namespace own`,进 tests + app):**
- `src/domain/Hotkey.h`, `src/domain/Hotkey.cpp` — `parseHotkey`/`formatHotkey`/`findHotkeyConflicts` + 位标志/结构体。

**新增 · 服务层(Win32,仅进 app 工程,`src/services/`):**
- `src/services/HotkeyManager.h`, `src/services/HotkeyManager.cpp` — 设置驱动、冲突安全的热键注册。
- `src/services/AutostartManager.h`, `src/services/AutostartManager.cpp` — 启动夹 .lnk 建/删/查(IShellLink）。
- `src/services/TrayIcon.h`, `src/services/TrayIcon.cpp` — `Shell_NotifyIcon` 包装(add/modify/remove/reAdd)。

**修改:**
- `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp` — 移除写死的 `RegisterHotKey`(交给 HotkeyManager);寄宿托盘(托盘回调 + `TaskbarCreated` 重建 + 托盘右键菜单);新增 `onSetAllVisible/onToggleAutostart/isAutostartEnabled` 回调。
- `src/app/NoteApp.h`, `src/app/NoteApp.cpp` — 持有 `HotkeyManager`;`InitInstance` 装绑定并注册、创建托盘、接线自启回调;`ExitInstance` 注销热键 + 删托盘。
- `tests/tests.vcxproj`、`app/open_windows_note_app.vcxproj` — 登记新文件;app 链接加 `ole32.lib`。
- `docs/superpowers/smoke/P5-smoke-checklist.md` — 新增。

---

### Task 1: 热键解析/格式化(纯逻辑)

**Files:**
- Create: `src/domain/Hotkey.h`, `src/domain/Hotkey.cpp`
- Test: `tests/test_hotkey.cpp`
- Modify: `tests/tests.vcxproj`, `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Produces(全 HWND-free):
  - `enum own::HotkeyMods { kModCtrl=1, kModAlt=2, kModShift=4, kModWin=8 };`
  - `struct own::Hotkey { unsigned mods=0; int vk=0; };`
  - `bool own::parseHotkey(const std::string& s, Hotkey& out);` —— 解析 `"Ctrl+Alt+N"` 形式;大小写不敏感;修饰符 ctrl/control、alt、shift、win/super;键支持 `A`–`Z`(vk=大写字符码)、`0`–`9`(vk=字符码)、`F1`–`F12`(vk=0x70..0x7B)。缺键或未知键返回 false。
  - `std::string own::formatHotkey(const Hotkey& h);` —— 固定顺序 `Ctrl+`/`Alt+`/`Shift+`/`Win+` 再拼键名;`A`–`Z`/`0`–`9` 原样,`0x70..0x7B`→`F1..F12`。

- [ ] **Step 1: 写失败测试**

`tests/test_hotkey.cpp`:
```cpp
#include "doctest.h"
#include "domain/Hotkey.h"
using own::Hotkey; using own::parseHotkey; using own::formatHotkey;

TEST_CASE("parseHotkey letters/digits/function keys") {
    Hotkey h;
    CHECK(parseHotkey("Ctrl+Alt+N", h));
    CHECK(h.mods == (own::kModCtrl | own::kModAlt));
    CHECK(h.vk == 'N');
    CHECK(parseHotkey("ctrl+shift+F5", h));
    CHECK(h.mods == (own::kModCtrl | own::kModShift));
    CHECK(h.vk == 0x74);                 // VK_F5
    CHECK(parseHotkey("Alt+9", h));
    CHECK(h.vk == '9');
    CHECK(parseHotkey("Win+M", h));
    CHECK(h.mods == own::kModWin);
}
TEST_CASE("parseHotkey rejects missing/unknown key") {
    Hotkey h;
    CHECK_FALSE(parseHotkey("", h));
    CHECK_FALSE(parseHotkey("Ctrl", h));        // 只有修饰符
    CHECK_FALSE(parseHotkey("Ctrl+Alt+", h));
    CHECK_FALSE(parseHotkey("Ctrl+Foo", h));
}
TEST_CASE("formatHotkey fixed order + roundtrip") {
    CHECK(formatHotkey(Hotkey{own::kModCtrl | own::kModAlt, 'N'}) == "Ctrl+Alt+N");
    CHECK(formatHotkey(Hotkey{own::kModCtrl | own::kModShift, 0x74}) == "Ctrl+Shift+F5");
    Hotkey h; REQUIRE(parseHotkey("Win+Shift+F12", h));
    CHECK(formatHotkey(h) == "Shift+Win+F12");   // 固定顺序：Ctrl,Alt,Shift,Win
}
```
在 `tests/tests.vcxproj` 加 `<ClCompile Include="test_hotkey.cpp" />` 和 `<ClCompile Include="..\src\domain\Hotkey.cpp" />`。

- [ ] **Step 2: 运行验证失败**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error" | head`
Expected: 编译失败(`domain/Hotkey.h` 不存在）。

- [ ] **Step 3: 实现**

`src/domain/Hotkey.h`:
```cpp
#pragma once
#include <string>
namespace own {
enum HotkeyMods { kModCtrl = 1, kModAlt = 2, kModShift = 4, kModWin = 8 };
struct Hotkey { unsigned mods = 0; int vk = 0; };
bool parseHotkey(const std::string& s, Hotkey& out);
std::string formatHotkey(const Hotkey& h);
}
```
`src/domain/Hotkey.cpp`:
```cpp
#include "domain/Hotkey.h"
#include <vector>
#include <cctype>
namespace own {
static std::string lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"); if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t"); return s.substr(a, b - a + 1);
}
static bool parseKey(const std::string& tokRaw, int& vk) {
    std::string t = trim(tokRaw);
    if (t.empty()) return false;
    if (t.size() == 1) {
        char c = t[0];
        if (c >= 'a' && c <= 'z') { vk = c - 'a' + 'A'; return true; }
        if (c >= 'A' && c <= 'Z') { vk = c; return true; }
        if (c >= '0' && c <= '9') { vk = c; return true; }
        return false;
    }
    std::string lt = lower(t);
    if (lt[0] == 'f' && lt.size() >= 2) {              // F1..F12
        int n = 0; for (size_t i = 1; i < lt.size(); ++i) { if (!isdigit((unsigned char)lt[i])) return false; n = n * 10 + (lt[i] - '0'); }
        if (n >= 1 && n <= 12) { vk = 0x70 + (n - 1); return true; }
    }
    return false;
}
bool parseHotkey(const std::string& s, Hotkey& out) {
    Hotkey h;
    std::vector<std::string> toks; std::string cur;
    for (char c : s) { if (c == '+') { toks.push_back(cur); cur.clear(); } else cur.push_back(c); }
    toks.push_back(cur);
    if (toks.empty()) return false;
    for (size_t i = 0; i < toks.size(); ++i) {
        std::string t = lower(trim(toks[i]));
        bool isLast = (i + 1 == toks.size());
        if (!isLast) {
            if (t == "ctrl" || t == "control") h.mods |= kModCtrl;
            else if (t == "alt") h.mods |= kModAlt;
            else if (t == "shift") h.mods |= kModShift;
            else if (t == "win" || t == "super") h.mods |= kModWin;
            else return false;                          // 非末位必须是修饰符
        } else {
            if (!parseKey(toks[i], h.vk)) return false; // 末位必须是有效键
        }
    }
    if (h.vk == 0) return false;
    out = h; return true;
}
std::string formatHotkey(const Hotkey& h) {
    std::string s;
    if (h.mods & kModCtrl) s += "Ctrl+";
    if (h.mods & kModAlt) s += "Alt+";
    if (h.mods & kModShift) s += "Shift+";
    if (h.mods & kModWin) s += "Win+";
    if ((h.vk >= 'A' && h.vk <= 'Z') || (h.vk >= '0' && h.vk <= '9')) s += (char)h.vk;
    else if (h.vk >= 0x70 && h.vk <= 0x7B) s += "F" + std::to_string(h.vk - 0x70 + 1);
    else s += "?";
    return s;
}
}
```
在 `app/open_windows_note_app.vcxproj` 源组加 `<ClCompile Include="..\src\domain\Hotkey.cpp" />`,头组加 `<ClInclude Include="..\src\domain\Hotkey.h" />`。

- [ ] **Step 4: 运行验证通过**

Run: `taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建通过,tests 全绿(新增用例)。

- [ ] **Step 5: Commit**

```bash
git add src/domain/Hotkey.h src/domain/Hotkey.cpp tests/test_hotkey.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(domain): hotkey parse/format (pure)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: 热键冲突检测(纯逻辑)

**Files:**
- Modify: `src/domain/Hotkey.h`, `src/domain/Hotkey.cpp`
- Modify: `tests/test_hotkey.cpp`

**Interfaces:**
- Produces: `std::vector<std::pair<int,int>> own::findHotkeyConflicts(const std::vector<Hotkey>& hs);` —— 返回所有 `(i,j)`(`i<j`)使 `hs[i]` 与 `hs[j]` 的 `(mods,vk)` 完全相同。用于注册前发现重复绑定,保留第一个、跳过后续。

- [ ] **Step 1: 追加失败测试**

在 `tests/test_hotkey.cpp` 末尾加:
```cpp
TEST_CASE("findHotkeyConflicts reports duplicate (mods,vk) pairs") {
    using own::findHotkeyConflicts;
    std::vector<Hotkey> hs = {
        {own::kModCtrl, 'N'},           // 0
        {own::kModAlt,  'N'},           // 1  (mods 不同，不冲突)
        {own::kModCtrl, 'N'},           // 2  与 0 冲突
        {own::kModCtrl | own::kModAlt, 'M'}, // 3
    };
    auto c = findHotkeyConflicts(hs);
    REQUIRE(c.size() == 1);
    CHECK(c[0].first == 0);
    CHECK(c[0].second == 2);
}
TEST_CASE("findHotkeyConflicts empty when all unique") {
    using own::findHotkeyConflicts;
    std::vector<Hotkey> hs = { {own::kModCtrl,'A'}, {own::kModCtrl,'B'} };
    CHECK(findHotkeyConflicts(hs).empty());
}
```

- [ ] **Step 2: 运行验证失败** — `findHotkeyConflicts` 未定义 → 编译失败。

- [ ] **Step 3: 实现**

`src/domain/Hotkey.h` 加 include 与声明:
```cpp
#include <vector>
#include <utility>
```
```cpp
std::vector<std::pair<int,int>> findHotkeyConflicts(const std::vector<Hotkey>& hs);
```
`src/domain/Hotkey.cpp` 末尾(namespace 内)加:
```cpp
std::vector<std::pair<int,int>> findHotkeyConflicts(const std::vector<Hotkey>& hs) {
    std::vector<std::pair<int,int>> out;
    for (int i = 0; i < (int)hs.size(); ++i)
        for (int j = i + 1; j < (int)hs.size(); ++j)
            if (hs[i].mods == hs[j].mods && hs[i].vk == hs[j].vk)
                out.push_back({ i, j });
    return out;
}
```

- [ ] **Step 4: 运行验证通过** — 构建 + `./x64/Debug/tests.exe` 全绿。

- [ ] **Step 5: Commit**

```bash
git add src/domain/Hotkey.h src/domain/Hotkey.cpp tests/test_hotkey.cpp
git commit -m "feat(domain): hotkey conflict detection (pure)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: HotkeyManager(设置驱动、冲突安全的注册)

**Files:**
- Create: `src/services/HotkeyManager.h`, `src/services/HotkeyManager.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: Task 1/2 `own::Hotkey`/`parseHotkey`/`findHotkeyConflicts`;`own::SettingsStore`。
- Produces:
  - `struct HkBinding { int id; std::string name; std::string defBinding; own::Hotkey hk; bool registered=false; };`
  - `class HotkeyManager { void add(int id, const std::string& name, const std::string& defBinding); void loadAndRegister(HWND hwnd, own::SettingsStore& settings); void unregisterAll(HWND hwnd); const std::vector<HkBinding>& bindings() const; };`
  - `loadAndRegister`:每个 binding 读 `settings.getString("hotkey."+name, defBinding)`→`parseHotkey`(失败则解析 `defBinding`)→汇总;`findHotkeyConflicts` 标记后出现者跳过;非跳过者 `RegisterHotKey(hwnd,id,winMods,vk)`,成败写 `registered`;失败 `OutputDebugStringA` 记日志。
- 说明:注册用既有 `kHotkey*` id;派发保留在 `CAppHostWindow::OnHotKey`。达标线=app 链接通过。

- [ ] **Step 1: 写头**

`src/services/HotkeyManager.h`:
```cpp
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "domain/Hotkey.h"
namespace own { class SettingsStore; }

struct HkBinding {
    int id;
    std::string name;        // settings key 后缀 & 展示名
    std::string defBinding;  // 默认绑定字符串，如 "Ctrl+Alt+N"
    own::Hotkey hk;
    bool registered = false;
};
class HotkeyManager {
public:
    void add(int id, const std::string& name, const std::string& defBinding);
    void loadAndRegister(HWND hwnd, own::SettingsStore& settings);
    void unregisterAll(HWND hwnd);
    const std::vector<HkBinding>& bindings() const { return m_b; }
private:
    std::vector<HkBinding> m_b;
};
```

- [ ] **Step 2: 实现**

`src/services/HotkeyManager.cpp`:
```cpp
#include "services/HotkeyManager.h"
#include "data/SettingsStore.h"

static UINT toWinMods(unsigned m) {
    UINT w = 0;
    if (m & own::kModCtrl)  w |= MOD_CONTROL;
    if (m & own::kModAlt)   w |= MOD_ALT;
    if (m & own::kModShift) w |= MOD_SHIFT;
    if (m & own::kModWin)   w |= MOD_WIN;
    return w;
}
void HotkeyManager::add(int id, const std::string& name, const std::string& defBinding) {
    HkBinding b; b.id = id; b.name = name; b.defBinding = defBinding;
    m_b.push_back(b);
}
void HotkeyManager::loadAndRegister(HWND hwnd, own::SettingsStore& settings) {
    // 1) 解析每个绑定（设置覆盖优先，解析失败回落默认）
    for (auto& b : m_b) {
        std::string s = settings.getString("hotkey." + b.name, b.defBinding);
        if (!own::parseHotkey(s, b.hk))
            own::parseHotkey(b.defBinding, b.hk);
    }
    // 2) 冲突检测：后出现者跳过
    std::vector<own::Hotkey> hs; hs.reserve(m_b.size());
    for (auto& b : m_b) hs.push_back(b.hk);
    std::vector<bool> skip(m_b.size(), false);
    for (auto& pr : own::findHotkeyConflicts(hs)) skip[pr.second] = true;
    // 3) 注册
    for (size_t i = 0; i < m_b.size(); ++i) {
        auto& b = m_b[i];
        if (skip[i]) {
            ::OutputDebugStringA(("[hotkey] skip conflicting binding: " + b.name + "\n").c_str());
            b.registered = false;
            continue;
        }
        b.registered = ::RegisterHotKey(hwnd, b.id, toWinMods(b.hk.mods), (UINT)b.hk.vk) != FALSE;
        if (!b.registered)
            ::OutputDebugStringA(("[hotkey] RegisterHotKey failed: " + b.name + "\n").c_str());
    }
}
void HotkeyManager::unregisterAll(HWND hwnd) {
    for (auto& b : m_b) if (b.registered) { ::UnregisterHotKey(hwnd, b.id); b.registered = false; }
}
```

- [ ] **Step 3: 登记 + 构建**

`app/open_windows_note_app.vcxproj` 源组加 `<ClCompile Include="..\src\services\HotkeyManager.cpp" />`;头组加 `<ClInclude Include="..\src\services\HotkeyManager.h" />`。并在两个 `ItemDefinitionGroup` 的 `<AdditionalIncludeDirectories>` 已含 `$(SolutionDir)src`——`services/` 在 `src` 下,include 路径 `services/HotkeyManager.h` 可解析,无需改。
Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: app 链接通过;tests 全绿(数量不变)。

- [ ] **Step 4: Commit**

```bash
git add src/services/HotkeyManager.h src/services/HotkeyManager.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(services): HotkeyManager — settings-backed, conflict-safe registration

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: AutostartManager(启动夹 .lnk)

**Files:**
- Create: `src/services/AutostartManager.h`, `src/services/AutostartManager.cpp`
- Modify: `app/open_windows_note_app.vcxproj`(加 `ole32.lib`)

**Interfaces:**
- Produces:
  - `namespace own_svc { bool autostartIsEnabled(); bool autostartSetEnabled(bool on); }`
  - `autostartIsEnabled()`:启动夹下 `open_windows_note.lnk` 是否存在。
  - `autostartSetEnabled(true)`:用 `IShellLinkW`+`IPersistFile` 在 `FOLDERID_Startup` 建指向当前 exe 的快捷方式(工作目录=exe 目录);`false`:删除该 .lnk。返回是否成功。
- 说明:COM 操作,无法纯单测;达标线=链接通过 + Task 7 手工冒烟。

- [ ] **Step 1: 写头**

`src/services/AutostartManager.h`:
```cpp
#pragma once
namespace own_svc {
bool autostartIsEnabled();
bool autostartSetEnabled(bool on);
}
```

- [ ] **Step 2: 实现**

`src/services/AutostartManager.cpp`:
```cpp
#include "services/AutostartManager.h"
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <string>

static std::wstring startupLnkPath() {
    PWSTR dir = nullptr;
    std::wstring path;
    if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &dir)) && dir) {
        path = dir; path += L"\\open_windows_note.lnk";
    }
    if (dir) ::CoTaskMemFree(dir);
    return path;
}
static std::wstring exePath() {
    wchar_t buf[MAX_PATH]; DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf, n);
}
namespace own_svc {
bool autostartIsEnabled() {
    std::wstring lnk = startupLnkPath();
    if (lnk.empty()) return false;
    DWORD a = ::GetFileAttributesW(lnk.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
bool autostartSetEnabled(bool on) {
    std::wstring lnk = startupLnkPath();
    if (lnk.empty()) return false;
    if (!on) {
        ::DeleteFileW(lnk.c_str());
        return !autostartIsEnabled();
    }
    HRESULT hrInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool ok = false;
    IShellLinkW* link = nullptr;
    if (SUCCEEDED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_IShellLinkW, (void**)&link)) && link) {
        std::wstring exe = exePath();
        std::wstring dir = exe.substr(0, exe.find_last_of(L"\\/"));
        link->SetPath(exe.c_str());
        link->SetWorkingDirectory(dir.c_str());
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&pf)) && pf) {
            ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
            pf->Release();
        }
        link->Release();
    }
    if (hrInit == S_OK || hrInit == S_FALSE) ::CoUninitialize();
    return ok;
}
}
```

- [ ] **Step 3: 登记 + 链接 ole32**

`app/open_windows_note_app.vcxproj` 源组加 `<ClCompile Include="..\src\services\AutostartManager.cpp" />`;头组加 `<ClInclude Include="..\src\services\AutostartManager.h" />`。
两个 `ItemDefinitionGroup`(Debug/Release)的 `<Link>` 里把 `<AdditionalDependencies>` 改为含 `ole32.lib`,例如 Debug:
```xml
      <AdditionalDependencies>gdiplus.lib;ole32.lib;%(AdditionalDependencies)</AdditionalDependencies>
```
(`shell32` 的 `SHGetKnownFolderPath` 由 MFC 静态链接已带;若报未解析,追加 `shell32.lib`。)

- [ ] **Step 4: 构建**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|未解析|unresolved"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: app 链接通过;tests 全绿。若 `SHGetKnownFolderPath`/`CoCreateInstance` 报未解析,按 Step 3 追加 `shell32.lib`/确认 `ole32.lib`。

- [ ] **Step 5: Commit**

```bash
git add src/services/AutostartManager.h src/services/AutostartManager.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(services): AutostartManager — startup-folder .lnk via IShellLink

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: TrayIcon(Shell_NotifyIcon 包装)

**Files:**
- Create: `src/services/TrayIcon.h`, `src/services/TrayIcon.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Produces:
  - `class TrayIcon { bool add(HWND owner, UINT callbackMsg, UINT id, HICON icon, const wchar_t* tip); void modifyTip(const wchar_t* tip); void reAdd(); void remove(); bool added() const; };`
  - `add`:填 `NOTIFYICONDATAW`(`NIF_MESSAGE|NIF_ICON|NIF_TIP`),`Shell_NotifyIconW(NIM_ADD)`;记住参数供 `reAdd`(资源管理器重启后)。`remove`=`NIM_DELETE`。
- 说明:达标线=链接通过。

- [ ] **Step 1: 写头**

`src/services/TrayIcon.h`:
```cpp
#pragma once
#include <windows.h>
#include <shellapi.h>
class TrayIcon {
public:
    bool add(HWND owner, UINT callbackMsg, UINT id, HICON icon, const wchar_t* tip);
    void modifyTip(const wchar_t* tip);
    void reAdd();
    void remove();
    bool added() const { return m_added; }
private:
    NOTIFYICONDATAW m_nid{};
    bool m_added = false;
};
```

- [ ] **Step 2: 实现**

`src/services/TrayIcon.cpp`:
```cpp
#include "services/TrayIcon.h"
#include <wchar.h>

bool TrayIcon::add(HWND owner, UINT callbackMsg, UINT id, HICON icon, const wchar_t* tip) {
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = owner;
    m_nid.uID = id;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = callbackMsg;
    m_nid.hIcon = icon;
    if (tip) wcsncpy_s(m_nid.szTip, tip, _TRUNCATE);
    m_added = ::Shell_NotifyIconW(NIM_ADD, &m_nid) != FALSE;
    return m_added;
}
void TrayIcon::modifyTip(const wchar_t* tip) {
    if (!m_added) return;
    if (tip) wcsncpy_s(m_nid.szTip, tip, _TRUNCATE);
    ::Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}
void TrayIcon::reAdd() {
    // 资源管理器重启后重新添加（TaskbarCreated 时调用）
    m_added = ::Shell_NotifyIconW(NIM_ADD, &m_nid) != FALSE;
}
void TrayIcon::remove() {
    if (!m_added) return;
    ::Shell_NotifyIconW(NIM_DELETE, &m_nid);
    m_added = false;
}
```

- [ ] **Step 3: 登记 + 构建**

`app/open_windows_note_app.vcxproj` 源组加 `<ClCompile Include="..\src\services\TrayIcon.cpp" />`;头组加 `<ClInclude Include="..\src\services\TrayIcon.h" />`。
Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved|未解析"|head; ./x64/Debug/tests.exe 2>&1 | tail -2`
Expected: app 链接通过(`Shell_NotifyIconW` 由 shell32 提供,MFC 已带)。

- [ ] **Step 4: Commit**

```bash
git add src/services/TrayIcon.h src/services/TrayIcon.cpp app/open_windows_note_app.vcxproj
git commit -m "feat(services): TrayIcon Shell_NotifyIcon wrapper

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: 集成 — 托盘寄宿 host + 热键走 HotkeyManager + 自启接线

**Files:**
- Modify: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`
- Modify: `src/app/NoteApp.h`, `src/app/NoteApp.cpp`

**Interfaces:**
- Consumes: Task 3 `HotkeyManager`;Task 4 `own_svc::autostartIsEnabled/autostartSetEnabled`;Task 5 `TrayIcon`;既有 `CAppHostWindow` 回调 + `kHotkey*` id;`CNoteApp::setAllNotesVisible`。
- Produces:
  - `CAppHostWindow`:去掉 `Create()` 里的 `RegisterHotKey`/`OnDestroy` 里的 `UnregisterHotKey`(交给 HotkeyManager);寄宿 `TrayIcon m_tray`,处理托盘回调消息(左双击=显示管理器、右键=弹托盘菜单)与 `TaskbarCreated` 重建;新增回调 `std::function<void(bool)> onSetAllVisible`、`std::function<void()> onToggleAutostart`、`std::function<bool()> isAutostartEnabled`;新增 `bool createTray();`。
  - `CNoteApp`:持有 `HotkeyManager m_hotkeys`;`InitInstance` 里 `add` 六个绑定并在 host 创建后 `loadAndRegister(m_host.GetSafeHwnd(), settingsStore)`;创建托盘;接线 `onSetAllVisible/onToggleAutostart/isAutostartEnabled`;`ExitInstance` 里 `m_hotkeys.unregisterAll(m_host.GetSafeHwnd())` + `m_host` 删托盘。

- [ ] **Step 1: 改 `CAppHostWindow` 头**

`src/app/AppHostWindow.h` 顶部 include 加 `#include "services/TrayIcon.h"`。类内:
- 删掉 6 个 `static const UINT kHotkey*` 吗?**不删**——`OnHotKey` 仍按 id 派发、`CNoteApp` 建绑定时要用它们。保留。
- 在回调区加:
```cpp
    std::function<void(bool)> onSetAllVisible;   // 托盘“显示/隐藏全部”
    std::function<void()> onToggleAutostart;     // 托盘“开机自启”切换
    std::function<bool()> isAutostartEnabled;    // 菜单勾选状态查询
```
- 在 public 加 `bool createTray();`。
- 在 protected 消息处理加:
```cpp
    afx_msg LRESULT OnTrayCallback(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTaskbarCreated(WPARAM wParam, LPARAM lParam);
    void showTrayMenu();
```
- 私有成员加 `TrayIcon m_tray;`。

- [ ] **Step 2: 改 `CAppHostWindow` 实现**

`src/app/AppHostWindow.cpp` 顶部加 include:
```cpp
#include "services/AutostartManager.h"
```
定义托盘回调消息与 TaskbarCreated:
```cpp
static const UINT WM_TRAY_CALLBACK = WM_APP + 1;
static UINT WM_TASKBARCREATED = ::RegisterWindowMessageW(L"TaskbarCreated");
```
消息映射改为:
```cpp
BEGIN_MESSAGE_MAP(CAppHostWindow, CWnd)
    ON_WM_HOTKEY()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_TRAY_CALLBACK, &CAppHostWindow::OnTrayCallback)
    ON_REGISTERED_MESSAGE(WM_TASKBARCREATED, &CAppHostWindow::OnTaskbarCreated)
END_MESSAGE_MAP()
```
`Create()` 去掉全部 `RegisterHotKey` 行(改由 HotkeyManager),只保留建窗:
```cpp
bool CAppHostWindow::Create() {
    LPCTSTR cls = AfxRegisterWndClass(0);
    if (!CreateEx(0, cls, _T("OwnAppHost"), WS_POPUP, CRect(0, 0, 0, 0), NULL, 0))
        return false;
    return true;
}
```
`OnHotKey` 保持不变(仍按 id 派发到 onNewNote 等)。
`OnDestroy` 去掉全部 `UnregisterHotKey`(HotkeyManager 在 ExitInstance 注销),改为删托盘:
```cpp
void CAppHostWindow::OnDestroy() {
    m_tray.remove();
    CWnd::OnDestroy();
}
```
新增托盘相关:
```cpp
bool CAppHostWindow::createTray() {
    HICON icon = ::LoadIcon(nullptr, IDI_APPLICATION);
    return m_tray.add(m_hWnd, WM_TRAY_CALLBACK, 1, icon, L"open_windows_note");
}
LRESULT CAppHostWindow::OnTaskbarCreated(WPARAM, LPARAM) {
    m_tray.reAdd();   // 资源管理器重启后重建图标
    return 0;
}
LRESULT CAppHostWindow::OnTrayCallback(WPARAM, LPARAM lParam) {
    if (LOWORD(lParam) == WM_LBUTTONDBLCLK) { if (onToggleManager) onToggleManager(); }
    else if (LOWORD(lParam) == WM_RBUTTONUP) { showTrayMenu(); }
    return 0;
}
void CAppHostWindow::showTrayMenu() {
    CMenu menu; menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 1, _T("\x65B0\x5EFA\x4FBF\x7B7E"));         // 新建便签
    menu.AppendMenu(MF_STRING, 2, _T("\x663E\x793A\x7BA1\x7406\x5668"));   // 显示管理器
    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    menu.AppendMenu(MF_STRING, 3, _T("\x663E\x793A\x5168\x90E8"));         // 显示全部
    menu.AppendMenu(MF_STRING, 4, _T("\x9690\x85CF\x5168\x90E8"));         // 隐藏全部
    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    UINT autostartFlag = MF_STRING | ((isAutostartEnabled && isAutostartEnabled()) ? MF_CHECKED : MF_UNCHECKED);
    menu.AppendMenu(autostartFlag, 5, _T("\x5F00\x673A\x81EA\x542F"));     // 开机自启
    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    menu.AppendMenu(MF_STRING, 6, _T("\x9000\x51FA"));                     // 退出
    CPoint pt; ::GetCursorPos(&pt);
    ::SetForegroundWindow(m_hWnd);   // 托盘菜单必需，否则菜单不消失
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    ::PostMessage(m_hWnd, WM_NULL, 0, 0);
    switch (cmd) {
        case 1: if (onNewNote) onNewNote(); break;
        case 2: if (onToggleManager) onToggleManager(); break;
        case 3: if (onSetAllVisible) onSetAllVisible(true); break;
        case 4: if (onSetAllVisible) onSetAllVisible(false); break;
        case 5: if (onToggleAutostart) onToggleAutostart(); break;
        case 6: if (onQuit) onQuit(); break;
    }
}
```

- [ ] **Step 3: 改 `CNoteApp` 头**

`src/app/NoteApp.h` include 加 `#include "services/HotkeyManager.h"`;私有成员加 `HotkeyManager m_hotkeys;`。

- [ ] **Step 4: 改 `CNoteApp` 实现**

`src/app/NoteApp.cpp` 顶部加:
```cpp
#include "services/AutostartManager.h"
#include "data/SettingsStore.h"
```
在 `m_host.onQuit = ...;` 之后、`if (!m_host.Create())` 之前,接线托盘/自启回调:
```cpp
    m_host.onSetAllVisible = [this](bool show){ setAllNotesVisible(show); if (m_main) m_main->reloadList(); };
    m_host.onToggleAutostart = []{ own_svc::autostartSetEnabled(!own_svc::autostartIsEnabled()); };
    m_host.isAutostartEnabled = []{ return own_svc::autostartIsEnabled(); };
```
在 `if (!m_host.Create()) return FALSE;` 之后、`m_pMainWnd = &m_host;` 附近,注册热键 + 建托盘:
```cpp
    {
        own::SettingsStore settings(m_db);
        m_hotkeys.add(CAppHostWindow::kHotkeyNew,          "new",          "Ctrl+Alt+N");
        m_hotkeys.add(CAppHostWindow::kHotkeyNewChecklist, "new_checklist","Ctrl+Alt+2");
        m_hotkeys.add(CAppHostWindow::kHotkeyNewDrawing,   "new_drawing",  "Ctrl+Alt+3");
        m_hotkeys.add(CAppHostWindow::kHotkeyManager,      "manager",      "Ctrl+Alt+M");
        m_hotkeys.add(CAppHostWindow::kHotkeyToggleAll,    "toggle_all",   "Ctrl+Alt+H");
        m_hotkeys.add(CAppHostWindow::kHotkeyQuit,         "quit",         "Ctrl+Alt+Q");
        m_hotkeys.loadAndRegister(m_host.GetSafeHwnd(), settings);
    }
    m_host.createTray();
```
> `SettingsStore` 直接以 `m_db` 构造(与 P2 用法一致;`SettingsStore(Database&)`)。若 `SettingsStore` 无此构造,改用现有获取方式——见 `src/data/SettingsStore.h` 构造签名。
在 `ExitInstance()` 里,`m_notes.clear();` 之前加注销:
```cpp
    m_hotkeys.unregisterAll(m_host.GetSafeHwnd());
```

- [ ] **Step 5: 构建 + 冒烟**

Run:
```bash
taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error|unresolved|未解析" | head
./x64/Debug/tests.exe 2>&1 | tail -2
./x64/Debug/open_windows_note.exe &
sleep 3
tasklist 2>/dev/null | grep -qi open_windows_note && echo "ALIVE (tray+hotkeys registered, no crash)" || echo "NOT running"
taskkill //F //IM open_windows_note.exe 2>/dev/null
```
Expected: app 链接通过;tests 全绿;启动存活(托盘图标出现、热键注册、无崩溃)。

- [ ] **Step 6: 手工冒烟(记录)** — 托盘图标出现;右键托盘菜单 新建/显示管理器/显示全部/隐藏全部/开机自启(勾选态)/退出 各生效;左双击托盘=显示管理器;Ctrl+Alt+N/2/3/M/H/Q 热键仍工作;勾开机自启后启动夹出现 `open_windows_note.lnk`、取消则删除。

- [ ] **Step 7: Commit**

```bash
git add src/app/AppHostWindow.h src/app/AppHostWindow.cpp src/app/NoteApp.h src/app/NoteApp.cpp
git commit -m "feat(app): tray icon on host + HotkeyManager-driven hotkeys + autostart wiring

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: P5 手工冒烟清单

**Files:**
- Create: `docs/superpowers/smoke/P5-smoke-checklist.md`

- [ ] **Step 1: 写清单**

`docs/superpowers/smoke/P5-smoke-checklist.md`:
```markdown
# P5 手工冒烟清单（托盘 + 开机自启 + 全局热键）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

## 系统托盘
- [ ] 启动后通知区出现托盘图标（悬停提示 open_windows_note）
- [ ] 左键双击托盘：切换管理器窗口显隐
- [ ] 右键托盘弹菜单：新建便签 / 显示管理器 / 显示全部 / 隐藏全部 / 开机自启(可勾) / 退出
- [ ] 菜单“新建便签/显示管理器/显示全部/隐藏全部/退出”各自生效
- [ ] 杀掉并重启 explorer.exe 后，托盘图标自动重新出现（TaskbarCreated 自愈）

## 全局热键
- [ ] Ctrl+Alt+N 新建富文本 / Ctrl+Alt+2 清单 / Ctrl+Alt+3 涂鸦
- [ ] Ctrl+Alt+M 切换管理器 / Ctrl+Alt+H 显隐全部 / Ctrl+Alt+Q 退出
- [ ] 在 settings 表把 hotkey.new 改成别的组合（如 Ctrl+Alt+J）后重启：新键生效
- [ ] 两个绑定填成相同组合：后者被跳过（前者仍可用，程序不崩，DebugView 有 skip 日志）

## 开机自启
- [ ] 托盘菜单勾“开机自启”：启动夹（shell:startup）出现 open_windows_note.lnk，指向本 exe
- [ ] 取消勾选：该 .lnk 被删除
- [ ] 重启菜单，勾选态与实际 .lnk 一致
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/smoke/P5-smoke-checklist.md
git commit -m "docs: P5 tray/autostart/hotkeys manual smoke checklist

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage(对照设计文档):**
- §1.3「系统托盘常驻」→ Task 5(TrayIcon)+ Task 6(寄宿 host、菜单、`TaskbarCreated` 自愈,对应 §6「资源管理器重启→重新添加托盘图标」)。✓
- §1.3 / §3「HotkeyManager:`RegisterHotKey`→路由 `WM_HOTKEY`;冲突则提示改键」→ Task 1/2(纯逻辑解析+冲突)+ Task 3(设置驱动、冲突安全注册)+ Task 6(接入)。**冲突「提示改键」的交互 UI** 本阶段降级为「冲突安全跳过 + 日志」,已在范围外声明,改键 UI 随设置弹层计划。✓(部分,按声明)
- §3「AutostartManager:开机自启 启动文件夹 `.lnk`(少碰注册表)」→ Task 4。✓
- §5.8「热键:`WM_HOTKEY`→CMainFrame(新建/显隐全部)」→ 派发保留在 host 的 `OnHotKey`(我们架构里 host 承担生命周期),等价满足。✓
- §6「热键注册冲突→提示并允许改键」→ 冲突安全跳过 + 日志(改键 UI 声明后置)。✓(按声明)
- §6「资源管理器重启→`TaskbarCreated` 重新添加托盘」→ Task 6 `OnTaskbarCreated`。✓
- §7 测试策略「纯函数注入、doctest」→ Task 1/2 热键纯逻辑 doctest;托盘/自启/热键注册手工冒烟(Task 7)。✓

**范围外(明确声明):** 交互式改键对话框(→设置弹层计划);贴到应用窗口 `StickyWindowWatcher`、提醒 `ReminderScheduler`、导入导出 `BackupService`、主题配色——各自独立计划;自定义托盘 .ico 资源(暂用 stock `IDI_APPLICATION`)。

**2. Placeholder scan:** 无 TBD/TODO;每步给出可编译代码或精确到行的改动。唯一「按实际签名确认」提示在 Task 6 Step 4 的 `SettingsStore(Database&)` 构造——已注明去 `src/data/SettingsStore.h` 核对(P2 该类即以 `Database&` 构造,见 SettingsStore 用法)。

**3. Type consistency:**
- `own::Hotkey{unsigned mods;int vk;}` / `kMod*` / `parseHotkey` / `formatHotkey` / `findHotkeyConflicts`(Task 1/2)在 Task 3 `HotkeyManager` 一致消费。✓
- `HotkeyManager::add(int,string,string)` / `loadAndRegister(HWND, SettingsStore&)` / `unregisterAll(HWND)`(Task 3)在 Task 6 `CNoteApp` 一致调用;id 用既有 `CAppHostWindow::kHotkey*`。✓
- `own_svc::autostartIsEnabled()/autostartSetEnabled(bool)`(Task 4)在 Task 6 一致调用。✓
- `TrayIcon::add(HWND,UINT,UINT,HICON,const wchar_t*)/reAdd()/remove()`(Task 5)在 Task 6 `CAppHostWindow` 一致使用。✓
- `CAppHostWindow` 新增回调 `onSetAllVisible(bool)/onToggleAutostart()/isAutostartEnabled()->bool` + `createTray()`(Task 6 Step 1/2)与 `CNoteApp` 接线(Step 4)一致。✓
- `WM_TRAY_CALLBACK`(WM_APP+1)与 `Shell_NotifyIcon.uCallbackMessage` 一致;`ON_REGISTERED_MESSAGE`/`ON_MESSAGE` 处理函数签名 `LRESULT(WPARAM,LPARAM)` 一致。✓

**已知限制(执行者须知):** 托盘/自启/热键的真实行为无法自动化,GUI/服务任务以「链接通过 + 启动存活」为自动化达标线,行为落 Task 7 手工清单。若 Task 4 链接报 `SHGetKnownFolderPath`/`CoCreateInstance` 未解析,追加 `shell32.lib`(`ole32.lib` 已在 Task 3 加);若 `SettingsStore` 构造签名与假设不符,按其头文件调整 Task 6 的构造方式。
