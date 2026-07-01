# open_windows_note — P2: 便签窗框架 + 应用启动 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 做出一个能启动的 MFC 应用:解析 `notes.db` 路径并打开数据库,把所有可见 note 显示为可拖动/缩放/自绘标题栏的无边框、置顶、可透明悬浮窗,窗口的几何与状态实时持久化,重启后原样恢复;临时热键 `Ctrl+Alt+N` 新建便签。

**Architecture:** 在 P1 数据层(`own::Database`/`NoteStore`,已在 main)之上加 UI/应用层。所有易错的几何/路径/设置逻辑抽成**无 HWND 纯函数/仓储**用 doctest 覆盖(路径决策、标题栏布局与命中测试、缩放矩形运算、SettingsStore);MFC 外壳(`CNoteApp`/`CAppHostWindow`/`CNoteWindow`)只做绘制与消息路由,走**手工冒烟清单**。渲染 GDI/GDI+,一律自绘,窗口用 `WS_POPUP` 无边框 + `WS_EX_LAYERED`(`SetLayeredWindowAttributes` 整窗透明)+ `WS_EX_TOPMOST` + `WS_EX_TOOLWINDOW`。

**Tech Stack:** C++17 / MFC(**静态链接**,应用工程)· 复用 P1 的 SQLite/json/doctest vendored 源 · GDI + GDI+ · Visual Studio 2022 / MSBuild(路径见 Global Constraints)。

## Global Constraints

- 平台 Windows 10+,**x64**;语言标准 **C++17**(`/std:c++17`)。
- MSBuild 不在 PATH,统一用:`"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`。
- 解决方案 `open_windows_note.sln`(已存在,含 `tests` 工程)。**本计划新增一个 MFC 应用工程 `app/open_windows_note_app.vcxproj`**,输出 `open_windows_note.exe`。
- 应用工程:`<UseOfMfc>Static</UseOfMfc>`、`<CharacterSet>Unicode</CharacterSet>`、`/utf-8`、`<SubSystem>Windows</SubSystem>`;**不使用预编译头**(每个 MFC 文件各自 `#include <afxwin.h>`),避免与不含 MFC 的 data/domain/sqlite 源混用 PCH。附加包含目录同 tests:`$(SolutionDir)src;$(SolutionDir)src\third_party\sqlite;$(SolutionDir)src\third_party\json`。预处理器:`SQLITE_THREADSAFE=1;SQLITE_DEFAULT_MEMSTATUS=0;_CRT_SECURE_NO_WARNINGS;WIN32;_WINDOWS`。链接 GDI+ (`gdiplus.lib`)。
- 数据文件名固定 **`notes.db`**;便携 `<exe目录>\notes.db`,不可写回落 `%APPDATA%\open_windows_note\notes.db`。
- **分层纪律**:`src/data`、`src/domain` 仍**禁止** include 任何 MFC/`windows.h` GUI 头(P1 已建立)。新增的**纯逻辑** UI 辅助(`src/ui/*Layout`、`*ResizeMath`、`src/app/AppPaths`)也保持无 HWND,只用标准库 + `domain/Models.h`,以便进 tests 工程用 doctest 测试。只有真正的窗口类(`CNoteWindow`/`CAppHostWindow`/`CNoteApp`,放 `src/app`/`src/ui` 且文件名带 `Wnd`/`App`)才 include MFC。
- 时间用 `int64_t` Unix 秒;“当前时间”一律参数注入,纯逻辑内禁止直接调 `time(nullptr)`。
- 复用 P1:`own::Database`、`own::NoteStore`、`own::Note`、`own::RectI`、`own::migrate`、`own::clampRectToWorkArea`。不重写它们。
- 提交信息末尾附:`Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`。
- 范围外(不在 P2):内容视图富文本/清单/涂鸦(P3)、管理器窗口与搜索(P4)、系统托盘/正式全局热键/提醒/贴窗口(P5)、打包(P6)。P2 的热键与单实例是**临时脚手架**,P5 会替换。

---

## File Structure

```
open_windows_note/
├─ open_windows_note.sln                 # 加入新 app 工程
├─ app/
│  └─ open_windows_note_app.vcxproj      # MFC 静态应用工程
├─ src/
│  ├─ app/
│  │  ├─ AppPaths.h / AppPaths.cpp        # chooseDbPath 纯函数 + resolveDbPath Win32 包装
│  │  ├─ NoteApp.h / NoteApp.cpp          # CNoteApp : CWinApp 入口/引导
│  │  └─ AppHostWindow.h / .cpp           # CAppHostWindow 隐藏宿主窗:热键/单实例/持有 note 窗
│  ├─ data/
│  │  └─ SettingsStore.h / .cpp           # 设置键值仓储(基于 Database)
│  └─ ui/
│     ├─ TitleBarLayout.h / .cpp          # 标题栏布局 + 命中测试(纯)
│     ├─ ResizeMath.h / .cpp              # 缩放边命中 + 矩形运算(纯)
│     ├─ MonitorEnum.h / .cpp             # enumMonitorWorkAreas() Win32 薄封装
│     └─ NoteWindow.h / NoteWindow.cpp    # CNoteWindow : CWnd 悬浮便签窗
├─ tests/                                 # 纯逻辑测试进 tests 工程
│  ├─ test_apppaths.cpp
│  ├─ test_settings_store.cpp
│  ├─ test_titlebar_layout.cpp
│  └─ test_resize_math.cpp
└─ docs/superpowers/plans/…
```

**测试归属**:`AppPaths`(纯部分)、`SettingsStore`、`TitleBarLayout`、`ResizeMath` 编进 **tests 工程**(doctest)。MFC 源(`NoteApp`/`AppHostWindow`/`NoteWindow`/`MonitorEnum`)只编进 **app 工程**,不进 tests(它们依赖 MFC/HWND)。

---

### Task 1: MFC 应用工程脚手架 + 可启动空壳

**Files:**
- Create: `app/open_windows_note_app.vcxproj`
- Create: `src/app/NoteApp.h`, `src/app/NoteApp.cpp`
- Create: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`
- Modify: `open_windows_note.sln`

**Interfaces:**
- Consumes: 无(P1 源本任务先不链接,下一批任务再加)。
- Produces:
  - `class CNoteApp : public CWinApp`,全局实例 `theApp`;`InitInstance()` 创建隐藏宿主窗并注册临时退出热键。
  - `class CAppHostWindow : public CWnd`,消息宿主(顶层、隐藏);常量 `static const UINT kHotkeyQuit = 1;`;窗口类名 `L"OwnAppHost"`。
  - 可执行文件 `open_windows_note.exe`,启动后仅有一个隐藏窗;`Ctrl+Alt+Q` 退出。

- [ ] **Step 1: 手写 app 工程 vcxproj**

`app/open_windows_note_app.vcxproj`(x64 Debug/Release,MFC 静态,无 PCH,/utf-8,链接 gdiplus)。关键片段(每个 config 的 ClCompile 都要有):
```xml
<PropertyGroup Label="Configuration" Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <ConfigurationType>Application</ConfigurationType>
  <UseOfMfc>Static</UseOfMfc>
  <CharacterSet>Unicode</CharacterSet>
  <PlatformToolset>v143</PlatformToolset>
</PropertyGroup>
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <ClCompile>
    <LanguageStandard>stdcpp17</LanguageStandard>
    <AdditionalIncludeDirectories>$(SolutionDir)src;$(SolutionDir)src\third_party\sqlite;$(SolutionDir)src\third_party\json;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    <PreprocessorDefinitions>SQLITE_THREADSAFE=1;SQLITE_DEFAULT_MEMSTATUS=0;_CRT_SECURE_NO_WARNINGS;WIN32;_WINDOWS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    <PrecompiledHeader>NotUsing</PrecompiledHeader>
    <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
  </ClCompile>
  <Link><SubSystem>Windows</SubSystem><AdditionalDependencies>gdiplus.lib;%(AdditionalDependencies)</AdditionalDependencies></Link>
</ItemDefinitionGroup>
```
`ItemGroup` 先只含:`src\app\NoteApp.cpp`、`src\app\AppHostWindow.cpp`(ClCompile)。Release 配置同构(优化默认)。

- [ ] **Step 2: 写 CAppHostWindow(隐藏宿主)**

`src/app/AppHostWindow.h`:
```cpp
#pragma once
#include <afxwin.h>
class CAppHostWindow : public CWnd {
public:
    static const UINT kHotkeyQuit = 1;
    bool Create();                 // 创建隐藏顶层窗并注册退出热键
protected:
    afx_msg void OnHotKey(UINT idHotKey, UINT fuModifiers, UINT vk);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()
};
```
`src/app/AppHostWindow.cpp`:
```cpp
#include "app/AppHostWindow.h"

BEGIN_MESSAGE_MAP(CAppHostWindow, CWnd)
    ON_WM_HOTKEY()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

bool CAppHostWindow::Create() {
    LPCTSTR cls = AfxRegisterWndClass(0);
    if (!CreateEx(0, cls, _T("OwnAppHost"), WS_POPUP, CRect(0,0,0,0), NULL, 0))
        return false;
    // 临时退出热键 Ctrl+Alt+Q（P5 会替换整套热键方案）
    ::RegisterHotKey(m_hWnd, kHotkeyQuit, MOD_CONTROL | MOD_ALT, 'Q');
    return true;
}
void CAppHostWindow::OnHotKey(UINT idHotKey, UINT, UINT) {
    if (idHotKey == kHotkeyQuit) ::PostQuitMessage(0);
}
void CAppHostWindow::OnDestroy() {
    ::UnregisterHotKey(m_hWnd, kHotkeyQuit);
    CWnd::OnDestroy();
}
```

- [ ] **Step 3: 写 CNoteApp 入口**

`src/app/NoteApp.h`:
```cpp
#pragma once
#include <afxwin.h>
#include "app/AppHostWindow.h"
class CNoteApp : public CWinApp {
public:
    BOOL InitInstance() override;
    int  ExitInstance() override;
private:
    CAppHostWindow m_host;
};
```
`src/app/NoteApp.cpp`:
```cpp
#include "app/NoteApp.h"

CNoteApp theApp;   // 唯一全局实例，MFC 从此提供 WinMain

BOOL CNoteApp::InitInstance() {
    CWinApp::InitInstance();
    if (!m_host.Create()) return FALSE;
    m_pMainWnd = &m_host;      // 隐藏宿主作为主窗，消息循环随其存活
    return TRUE;
}
int CNoteApp::ExitInstance() {
    return CWinApp::ExitInstance();
}
```

- [ ] **Step 4: 加入解决方案并构建**

把 app 工程加入 `open_windows_note.sln`(新 Project GUID + `Debug|x64`/`Release|x64` 映射)。
Run:
```bash
MSB="/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m 2>&1 | tail -5
ls -la x64/Debug/open_windows_note.exe
```
Expected: 编译链接 0 error;`open_windows_note.exe` 存在。

- [ ] **Step 5: 冒烟启动(best-effort)**

Run(启动后台、等 2 秒、确认进程在、再结束):
```bash
( ./x64/Debug/open_windows_note.exe & echo $! > /tmp/own.pid ); sleep 2
if kill -0 "$(cat /tmp/own.pid)" 2>/dev/null; then echo "LAUNCH OK (alive)"; else echo "PROCESS EXITED EARLY"; fi
taskkill //F //PID "$(cat /tmp/own.pid)" 2>/dev/null || true
```
Expected: `LAUNCH OK (alive)`(进程存活,无立即崩溃)。若无 GUI 会话导致无法启动,记录为环境限制并在报告说明——构建成功即视为本任务达成,交互留待手工冒烟。

- [ ] **Step 6: Commit**

```bash
git add app/ src/app/NoteApp.* src/app/AppHostWindow.* open_windows_note.sln
git commit -m "feat(app): MFC static app skeleton with hidden host window + quit hotkey

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: DB 路径决策纯函数 + Win32 包装

**Files:**
- Create: `src/app/AppPaths.h`, `src/app/AppPaths.cpp`
- Test: `tests/test_apppaths.cpp`

**Interfaces:**
- Consumes: 无。
- Produces:
  - `struct own::DbPathChoice { std::string path; bool portable; };`
  - `own::DbPathChoice own::chooseDbPath(const std::string& exeDir, const std::string& appDataDir, bool exeDirWritable);` —— 纯:`exeDirWritable` 为真→`{exeDir + "\\notes.db", true}`;否则→`{appDataDir + "\\open_windows_note\\notes.db", false}`。用 `\\` 连接(Windows)。
  - `std::string own::resolveDbPathWin();` —— Win32 包装(仅 app 工程用):取 exe 目录、`%APPDATA%`,探测 exe 目录可写,调用 `chooseDbPath`,并确保回落目录存在。**此函数不进 tests 工程**(它碰文件系统/Win32),只 `chooseDbPath` 进测试。

- [ ] **Step 1: 写失败测试(仅测纯函数)**

`tests/test_apppaths.cpp`:
```cpp
#include "doctest.h"
#include "app/AppPaths.h"

TEST_CASE("chooseDbPath prefers portable exe dir when writable") {
    auto c = own::chooseDbPath("C:\\apps\\own", "C:\\Users\\u\\AppData\\Roaming", true);
    CHECK(c.portable == true);
    CHECK(c.path == "C:\\apps\\own\\notes.db");
}
TEST_CASE("chooseDbPath falls back to appdata when exe dir not writable") {
    auto c = own::chooseDbPath("C:\\Program Files\\own", "C:\\Users\\u\\AppData\\Roaming", false);
    CHECK(c.portable == false);
    CHECK(c.path == "C:\\Users\\u\\AppData\\Roaming\\open_windows_note\\notes.db");
}
```
把 `tests/test_apppaths.cpp` 加入 `tests/tests.vcxproj`;**同时把 `..\src\app\AppPaths.cpp` 加入 tests 工程**——但注意 `AppPaths.cpp` 含 `resolveDbPathWin()`(用 windows.h)。为保持 tests 工程无 MFC/GUI 依赖,**把纯函数与 Win32 包装拆成两个编译单元**:`AppPaths.cpp`(仅 `chooseDbPath`,无 windows.h,进 tests + app)与 `AppPathsWin.cpp`(`resolveDbPathWin`,含 windows.h,仅进 app)。据此建文件。

- [ ] **Step 2: 运行验证失败**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m && ./x64/Debug/tests.exe`
Expected: 编译失败(`app/AppPaths.h` 不存在)。

- [ ] **Step 3: 实现**

`src/app/AppPaths.h`:
```cpp
#pragma once
#include <string>
namespace own {
struct DbPathChoice { std::string path; bool portable; };
DbPathChoice chooseDbPath(const std::string& exeDir, const std::string& appDataDir, bool exeDirWritable);
std::string resolveDbPathWin();   // 定义在 AppPathsWin.cpp（仅 app 工程编译）
}
```
`src/app/AppPaths.cpp`(纯,进 tests + app):
```cpp
#include "app/AppPaths.h"
namespace own {
DbPathChoice chooseDbPath(const std::string& exeDir, const std::string& appDataDir, bool exeDirWritable) {
    if (exeDirWritable) return { exeDir + "\\notes.db", true };
    return { appDataDir + "\\open_windows_note\\notes.db", false };
}
}
```
`src/app/AppPathsWin.cpp`(Win32,仅 app 工程):
```cpp
#include "app/AppPaths.h"
#include <windows.h>
#include <shlobj.h>
#include <string>
namespace own {
static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
static std::wstring exeDirW() {
    wchar_t buf[MAX_PATH]; ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf); size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}
static bool dirWritableW(const std::wstring& dir) {
    std::wstring probe = dir + L"\\.own_write_test.tmp";
    HANDLE h = ::CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    ::CloseHandle(h); return true;
}
std::string resolveDbPathWin() {
    std::wstring exe = exeDirW();
    wchar_t appdata[MAX_PATH]{};
    ::SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata);
    auto choice = chooseDbPath(wideToUtf8(exe), wideToUtf8(std::wstring(appdata)), dirWritableW(exe));
    if (!choice.portable) {   // 确保回落目录存在
        std::wstring d = std::wstring(appdata) + L"\\open_windows_note";
        ::CreateDirectoryW(d.c_str(), nullptr);
    }
    return choice.path;
}
}
```
把 `AppPaths.cpp` 加入 tests 工程与 app 工程;`AppPathsWin.cpp` 仅加入 app 工程。

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m && ./x64/Debug/tests.exe`
Expected: PASS(2 个新测试),app 工程仍构建通过。

- [ ] **Step 5: Commit**

```bash
git add src/app/AppPaths.* src/app/AppPathsWin.cpp tests/test_apppaths.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(app): DB path resolution (portable + %APPDATA% fallback)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: SettingsStore(键值设置仓储)

**Files:**
- Create: `src/data/SettingsStore.h`, `src/data/SettingsStore.cpp`
- Test: `tests/test_settings_store.cpp`

**Interfaces:**
- Consumes: P1 `own::Database`、`own::Statement`、`own::migrate`(建 `settings(key TEXT PRIMARY KEY, value TEXT)`)。
- Produces `class own::SettingsStore`(持 `Database&`):
  - `std::string getString(const std::string& key, const std::string& def);`
  - `void setString(const std::string& key, const std::string& value);`(UPSERT)
  - `int getInt(const std::string& key, int def);`(解析失败返回 def)
  - `void setInt(const std::string& key, int value);`

- [ ] **Step 1: 写失败测试**

`tests/test_settings_store.cpp`:
```cpp
#include "doctest.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include "data/SettingsStore.h"

static own::Database freshDb() {
    own::Database db; std::string err;
    REQUIRE(db.open(":memory:", &err));
    REQUIRE(own::migrate(db, &err));
    return db;
}
TEST_CASE("settings string get default then set/get") {
    auto db = freshDb(); own::SettingsStore s(db);
    CHECK(s.getString("theme", "黄") == "黄");
    s.setString("theme", "蓝");
    CHECK(s.getString("theme", "黄") == "蓝");
    s.setString("theme", "绿");            // upsert 覆盖
    CHECK(s.getString("theme", "黄") == "绿");
}
TEST_CASE("settings int roundtrip and default on missing/garbage") {
    auto db = freshDb(); own::SettingsStore s(db);
    CHECK(s.getInt("opacity", 255) == 255);
    s.setInt("opacity", 128);
    CHECK(s.getInt("opacity", 255) == 128);
    s.setString("opacity", "notanumber");
    CHECK(s.getInt("opacity", 255) == 255); // 解析失败回默认
}
```
加入 tests 工程(`test_settings_store.cpp` + `..\src\data\SettingsStore.cpp`)。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`data/SettingsStore.h` 不存在)。

- [ ] **Step 3: 实现**

`src/data/SettingsStore.h`:
```cpp
#pragma once
#include <string>
namespace own {
class Database;
class SettingsStore {
public:
    explicit SettingsStore(Database& db) : db_(db) {}
    std::string getString(const std::string& key, const std::string& def);
    void setString(const std::string& key, const std::string& value);
    int getInt(const std::string& key, int def);
    void setInt(const std::string& key, int value);
private:
    Database& db_;
};
}
```
`src/data/SettingsStore.cpp`:
```cpp
#include "data/SettingsStore.h"
#include "data/Database.h"
#include "data/Statement.h"
#include <cstdlib>
namespace own {
std::string SettingsStore::getString(const std::string& key, const std::string& def) {
    Statement s(db_, "SELECT value FROM settings WHERE key=?;");
    s.bind(1, key);
    return s.step() ? s.columnText(0) : def;
}
void SettingsStore::setString(const std::string& key, const std::string& value) {
    Statement s(db_, "INSERT INTO settings(key,value) VALUES(?,?) "
                     "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
    s.bind(1, key); s.bind(2, value); s.execDone();
}
int SettingsStore::getInt(const std::string& key, int def) {
    Statement s(db_, "SELECT value FROM settings WHERE key=?;");
    s.bind(1, key);
    if (!s.step()) return def;
    std::string v = s.columnText(0);
    if (v.empty()) return def;
    char* end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || *end != '\0') return def;   // 非纯整数→默认
    return (int)n;
}
void SettingsStore::setInt(const std::string& key, int value) {
    setString(key, std::to_string(value));
}
}
```
加入 tests 工程与 app 工程。

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/data/SettingsStore.* tests/test_settings_store.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(data): SettingsStore key/value over settings table

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: 标题栏布局与命中测试(纯)

**Files:**
- Create: `src/ui/TitleBarLayout.h`, `src/ui/TitleBarLayout.cpp`
- Test: `tests/test_titlebar_layout.cpp`

**Interfaces:**
- Consumes: `own::RectI`(P1 Models.h)。
- Produces:
  - `enum class own::TitleHit { None, Drag, Close, Pin, Roll, Opacity };`
  - `struct own::TitleBarMetrics { int height; int btnSize; int btnGap; int pad; };`
  - `struct own::TitleBarRects { RectI titleBar, closeBtn, pinBtn, rollBtn, opacityBtn, dragArea; };`
  - `TitleBarRects own::layoutTitleBar(RectI client, TitleBarMetrics m);` —— 标题栏在顶部高 `m.height`,宽满 client;从右到左依次排 `close, roll, pin, opacity` 四个 `btnSize` 方钮(每个上下居中,右侧留 `pad`,钮间 `btnGap`);`dragArea` = 标题栏内按钮左侧的剩余区域。
  - `TitleHit own::hitTestTitleBar(const TitleBarRects& r, int px, int py);` —— 点在某钮内→对应枚举;在 dragArea 内→`Drag`;否则→`None`(含标题栏以下的内容区)。
- 约定:坐标为 client 相对坐标;`RectI{x,y,w,h}`;点在矩形内判定 `x<=px<x+w && y<=py<y+h`。

- [ ] **Step 1: 写失败测试**

`tests/test_titlebar_layout.cpp`:
```cpp
#include "doctest.h"
#include "ui/TitleBarLayout.h"
using own::TitleHit;

static own::TitleBarRects mk() {
    own::TitleBarMetrics m{ 28, 20, 4, 4 };     // height,btnSize,gap,pad
    return own::layoutTitleBar(own::RectI{0,0,240,200}, m);
}
TEST_CASE("title bar spans width at top") {
    auto r = mk();
    CHECK(r.titleBar.x == 0); CHECK(r.titleBar.y == 0);
    CHECK(r.titleBar.w == 240); CHECK(r.titleBar.h == 28);
}
TEST_CASE("close is rightmost button, buttons within title bar") {
    auto r = mk();
    CHECK(r.closeBtn.x + r.closeBtn.w == 240 - 4);        // 右侧留 pad
    CHECK(r.closeBtn.w == 20); CHECK(r.closeBtn.h == 20);
    CHECK(r.closeBtn.y >= 0); CHECK(r.closeBtn.y + r.closeBtn.h <= 28);
    // 顺序 从右到左: close, roll, pin, opacity —— x 递减
    CHECK(r.rollBtn.x < r.closeBtn.x);
    CHECK(r.pinBtn.x  < r.rollBtn.x);
    CHECK(r.opacityBtn.x < r.pinBtn.x);
}
TEST_CASE("hit test maps points to controls") {
    auto r = mk();
    CHECK(own::hitTestTitleBar(r, r.closeBtn.x+2, r.closeBtn.y+2) == TitleHit::Close);
    CHECK(own::hitTestTitleBar(r, r.pinBtn.x+2,   r.pinBtn.y+2)   == TitleHit::Pin);
    CHECK(own::hitTestTitleBar(r, 10, 10) == TitleHit::Drag);      // 左侧空白=拖动
    CHECK(own::hitTestTitleBar(r, 10, 100) == TitleHit::None);     // 内容区
}
```
加入 tests 工程(`test_titlebar_layout.cpp` + `..\src\ui\TitleBarLayout.cpp`)。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`ui/TitleBarLayout.h` 不存在)。

- [ ] **Step 3: 实现**

`src/ui/TitleBarLayout.h`:
```cpp
#pragma once
#include "domain/Models.h"
namespace own {
enum class TitleHit { None, Drag, Close, Pin, Roll, Opacity };
struct TitleBarMetrics { int height; int btnSize; int btnGap; int pad; };
struct TitleBarRects { RectI titleBar, closeBtn, pinBtn, rollBtn, opacityBtn, dragArea; };
TitleBarRects layoutTitleBar(RectI client, TitleBarMetrics m);
TitleHit hitTestTitleBar(const TitleBarRects& r, int px, int py);
}
```
`src/ui/TitleBarLayout.cpp`:
```cpp
#include "ui/TitleBarLayout.h"
namespace own {
static bool inRect(const RectI& r, int px, int py) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}
TitleBarRects layoutTitleBar(RectI client, TitleBarMetrics m) {
    TitleBarRects r{};
    r.titleBar = { client.x, client.y, client.w, m.height };
    int y = client.y + (m.height - m.btnSize) / 2;
    int x = client.x + client.w - m.pad - m.btnSize;   // close 最右
    auto place = [&](RectI& btn){ btn = { x, y, m.btnSize, m.btnSize }; x -= (m.btnSize + m.btnGap); };
    place(r.closeBtn);
    place(r.rollBtn);
    place(r.pinBtn);
    place(r.opacityBtn);
    int dragRight = r.opacityBtn.x - m.btnGap;          // 拖动区到最左钮之前
    r.dragArea = { client.x, client.y, dragRight - client.x, m.height };
    return r;
}
TitleHit hitTestTitleBar(const TitleBarRects& r, int px, int py) {
    if (inRect(r.closeBtn, px, py))   return TitleHit::Close;
    if (inRect(r.rollBtn, px, py))    return TitleHit::Roll;
    if (inRect(r.pinBtn, px, py))     return TitleHit::Pin;
    if (inRect(r.opacityBtn, px, py)) return TitleHit::Opacity;
    if (inRect(r.dragArea, px, py))   return TitleHit::Drag;
    return TitleHit::None;
}
}
```
加入 tests 工程与 app 工程。

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/ui/TitleBarLayout.* tests/test_titlebar_layout.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(ui): title bar layout + hit test (pure)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: 缩放边命中与矩形运算(纯)

**Files:**
- Create: `src/ui/ResizeMath.h`, `src/ui/ResizeMath.cpp`
- Test: `tests/test_resize_math.cpp`

**Interfaces:**
- Consumes: `own::RectI`。
- Produces:
  - `enum class own::ResizeEdge { None, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };`
  - `ResizeEdge own::hitTestResizeEdge(RectI client, int px, int py, int margin);` —— 点在距边 `margin` 内→对应边/角(角优先)。
  - `RectI own::applyResize(RectI start, ResizeEdge edge, int dx, int dy, int minW, int minH);` —— 按边拖动 `(dx,dy)` 得新矩形,保证 `w>=minW && h>=minH`(收缩到下限时锁边)。

- [ ] **Step 1: 写失败测试**

`tests/test_resize_math.cpp`:
```cpp
#include "doctest.h"
#include "ui/ResizeMath.h"
using own::ResizeEdge;

TEST_CASE("hit test edges and corners with margin") {
    own::RectI c{0,0,200,150};
    CHECK(own::hitTestResizeEdge(c, 199, 75, 6) == ResizeEdge::Right);
    CHECK(own::hitTestResizeEdge(c, 1, 75, 6)   == ResizeEdge::Left);
    CHECK(own::hitTestResizeEdge(c, 100, 149, 6)== ResizeEdge::Bottom);
    CHECK(own::hitTestResizeEdge(c, 198, 148, 6)== ResizeEdge::BottomRight); // 角优先
    CHECK(own::hitTestResizeEdge(c, 100, 75, 6) == ResizeEdge::None);        // 内部
}
TEST_CASE("apply resize right/bottom grows size") {
    own::RectI r = own::applyResize({10,10,200,150}, ResizeEdge::Right, 30, 0, 80, 60);
    CHECK(r.x==10); CHECK(r.y==10); CHECK(r.w==230); CHECK(r.h==150);
    r = own::applyResize({10,10,200,150}, ResizeEdge::Bottom, 0, 25, 80, 60);
    CHECK(r.h==175);
}
TEST_CASE("apply resize left moves origin and shrinks, respecting min width") {
    own::RectI r = own::applyResize({100,10,200,150}, ResizeEdge::Left, 50, 0, 80, 60);
    CHECK(r.x==150); CHECK(r.w==150);
    // 收缩超过下限：宽锁 80，x 停在 start.x+start.w-minW
    own::RectI r2 = own::applyResize({100,10,200,150}, ResizeEdge::Left, 500, 0, 80, 60);
    CHECK(r2.w==80); CHECK(r2.x==220);   // 100+200-80
}
```
加入 tests 工程(`test_resize_math.cpp` + `..\src\ui\ResizeMath.cpp`)。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`ui/ResizeMath.h` 不存在)。

- [ ] **Step 3: 实现**

`src/ui/ResizeMath.h`:
```cpp
#pragma once
#include "domain/Models.h"
namespace own {
enum class ResizeEdge { None, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };
ResizeEdge hitTestResizeEdge(RectI client, int px, int py, int margin);
RectI applyResize(RectI start, ResizeEdge edge, int dx, int dy, int minW, int minH);
}
```
`src/ui/ResizeMath.cpp`:
```cpp
#include "ui/ResizeMath.h"
namespace own {
ResizeEdge hitTestResizeEdge(RectI c, int px, int py, int margin) {
    bool L = px >= c.x && px < c.x + margin;
    bool R = px < c.x + c.w && px >= c.x + c.w - margin;
    bool T = py >= c.y && py < c.y + margin;
    bool B = py < c.y + c.h && py >= c.y + c.h - margin;
    if (T && L) return ResizeEdge::TopLeft;
    if (T && R) return ResizeEdge::TopRight;
    if (B && L) return ResizeEdge::BottomLeft;
    if (B && R) return ResizeEdge::BottomRight;
    if (L) return ResizeEdge::Left;
    if (R) return ResizeEdge::Right;
    if (T) return ResizeEdge::Top;
    if (B) return ResizeEdge::Bottom;
    return ResizeEdge::None;
}
RectI applyResize(RectI s, ResizeEdge e, int dx, int dy, int minW, int minH) {
    int left = s.x, top = s.y, right = s.x + s.w, bottom = s.y + s.h;
    switch (e) {
        case ResizeEdge::Left:  left += dx; break;
        case ResizeEdge::Right: right += dx; break;
        case ResizeEdge::Top:   top += dy; break;
        case ResizeEdge::Bottom:bottom += dy; break;
        case ResizeEdge::TopLeft:     left += dx; top += dy; break;
        case ResizeEdge::TopRight:    right += dx; top += dy; break;
        case ResizeEdge::BottomLeft:  left += dx; bottom += dy; break;
        case ResizeEdge::BottomRight: right += dx; bottom += dy; break;
        default: break;
    }
    if (right - left < minW) {              // 锁最小宽，固定未拖动的那条边
        if (e==ResizeEdge::Left||e==ResizeEdge::TopLeft||e==ResizeEdge::BottomLeft) left = right - minW;
        else right = left + minW;
    }
    if (bottom - top < minH) {
        if (e==ResizeEdge::Top||e==ResizeEdge::TopLeft||e==ResizeEdge::TopRight) top = bottom - minH;
        else bottom = top + minH;
    }
    return { left, top, right - left, bottom - top };
}
}
```
加入 tests 工程与 app 工程。

- [ ] **Step 4: 运行验证通过**

Run: `"$MSB" ... && ./x64/Debug/tests.exe`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add src/ui/ResizeMath.* tests/test_resize_math.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(ui): resize edge hit-test + rect math (pure)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: DB 引导接入 CNoteApp(打开/迁移/完整性/损坏处理)

**Files:**
- Modify: `src/app/NoteApp.h`, `src/app/NoteApp.cpp`
- Create: `src/app/DbBootstrap.h`, `src/app/DbBootstrap.cpp`
- Modify: `app/open_windows_note_app.vcxproj`(加入 P1 数据源 + 新文件)

**Interfaces:**
- Consumes: `own::resolveDbPathWin()`(Task 2)、P1 `Database`/`migrate`/`integrityOk`。
- Produces:
  - `bool own::openDatabaseAtPath(const std::string& path, own::Database& outDb, std::string* err);` —— 打开→`integrityOk` 失败则把该文件改名 `<path>.corrupt.<n>` 备份后重建→`migrate`→成功返回 true。空路径/无法打开→false+err。
  - `CNoteApp` 持有 `own::Database m_db;` 并在 `InitInstance` 里引导;失败弹 `AfxMessageBox` 并返回 FALSE。
- **本任务首次把 P1 数据层源编入 app 工程**:`src\data\Database.cpp`、`Statement.cpp`、`Migrations.cpp`、`NoteStore.cpp`、`SettingsStore.cpp`、`src\third_party\sqlite\sqlite3.c`(PCH 关闭,已是工程默认)。

- [ ] **Step 1: 写 DbBootstrap 单测(可测部分:重建逻辑用真实临时文件)**

在 tests 工程新增 `tests/test_db_bootstrap.cpp`(注意 `openDatabaseAtPath` 无 windows.h,可进 tests):
```cpp
#include "doctest.h"
#include "app/DbBootstrap.h"
#include "data/Database.h"
#include "data/NoteStore.h"
#include "data/Statement.h"
#include <cstdio>

TEST_CASE("openDatabaseAtPath creates and migrates a fresh file") {
    std::string path = "test_boot_tmp.db";
    std::remove(path.c_str());
    own::Database db; std::string err;
    REQUIRE(own::openDatabaseAtPath(path, db, &err));
    // migrate 建了 themes（内置>=4）
    own::Statement s(db, "SELECT COUNT(*) FROM themes WHERE is_builtin=1;");
    REQUIRE(s.step()); CHECK(s.columnInt64(0) >= 4);
    db.close();
    std::remove(path.c_str());
}
```
加入 tests 工程(`test_db_bootstrap.cpp` + `..\src\app\DbBootstrap.cpp`)。

- [ ] **Step 2: 运行验证失败**

Expected: 编译失败(`app/DbBootstrap.h` 不存在)。

- [ ] **Step 3: 实现 DbBootstrap**

`src/app/DbBootstrap.h`:
```cpp
#pragma once
#include <string>
namespace own {
class Database;
bool openDatabaseAtPath(const std::string& path, Database& outDb, std::string* err);
}
```
`src/app/DbBootstrap.cpp`(仅标准库 + 数据层,无 windows.h → 可进 tests):
```cpp
#include "app/DbBootstrap.h"
#include "data/Database.h"
#include "data/Migrations.h"
#include <cstdio>
#include <string>
namespace own {
static void backupCorrupt(const std::string& path) {
    for (int i = 0; i < 1000; ++i) {
        std::string cand = path + ".corrupt." + std::to_string(i);
        if (std::rename(path.c_str(), cand.c_str()) == 0) return;  // 改名成功即备份
    }
    std::remove(path.c_str());   // 实在改不动就删掉，宁可重建也不卡启动
}
bool openDatabaseAtPath(const std::string& path, Database& outDb, std::string* err) {
    if (path.empty()) { if (err) *err = "empty db path"; return false; }
    if (!outDb.open(path, err)) return false;
    if (path != ":memory:" && !outDb.integrityOk()) {   // 损坏：备份+重建
        outDb.close();
        backupCorrupt(path);
        if (!outDb.open(path, err)) return false;
    }
    return migrate(outDb, err);
}
}
```
加入 tests 工程与 app 工程。

- [ ] **Step 4: 接入 CNoteApp**

`NoteApp.h` 增 `#include "data/Database.h"` 与成员 `own::Database m_db;`;`NoteApp.cpp` 的 `InitInstance` 在建 host 之前引导 DB:
```cpp
#include "app/AppPaths.h"
#include "app/DbBootstrap.h"
// ...
BOOL CNoteApp::InitInstance() {
    CWinApp::InitInstance();
    std::string path = own::resolveDbPathWin();
    std::string err;
    if (!own::openDatabaseAtPath(path, m_db, &err)) {
        CStringA msg = ("无法打开数据库:\n" + path + "\n" + err).c_str();
        AfxMessageBox(CString(msg));
        return FALSE;
    }
    if (!m_host.Create()) return FALSE;
    m_pMainWnd = &m_host;
    return TRUE;
}
```
把 P1 数据层 5 个 .cpp + `sqlite3.c` 加入 app 工程的 ClCompile(sqlite3.c 保持 PCH 关闭)。

- [ ] **Step 5: 运行验证通过 + 构建 app**

Run:
```bash
"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m 2>&1 | tail -5
./x64/Debug/tests.exe 2>&1 | tail -3
```
Expected: tests PASS(含新 bootstrap 测试);app 工程链接通过(sqlite/数据层进 app)。

- [ ] **Step 6: 冒烟(best-effort)**

Run:启动 exe,确认在 exe 目录或 `%APPDATA%\open_windows_note` 生成 `notes.db`:
```bash
( ./x64/Debug/open_windows_note.exe & echo $! > /tmp/own.pid ); sleep 2
ls -la x64/Debug/notes.db 2>/dev/null || ls -la "$APPDATA/open_windows_note/notes.db" 2>/dev/null || echo "(db not found — record env note)"
taskkill //F //PID "$(cat /tmp/own.pid)" 2>/dev/null || true
```
Expected: `notes.db` 出现。GUI 会话不可用时记录为环境限制。

- [ ] **Step 7: Commit**

```bash
git add src/app/DbBootstrap.* src/app/NoteApp.* tests/test_db_bootstrap.cpp tests/tests.vcxproj app/open_windows_note_app.vcxproj
git commit -m "feat(app): DB bootstrap — open/integrity/corrupt-recreate/migrate, wired into CNoteApp

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: CNoteWindow 外壳(无边框/分层/置顶 + 自绘背景与标题栏)

**Files:**
- Create: `src/ui/MonitorEnum.h`, `src/ui/MonitorEnum.cpp`
- Create: `src/ui/NoteWindow.h`, `src/ui/NoteWindow.cpp`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Consumes: P1 `own::Note`/`NoteStore`/`clampRectToWorkArea`;Task 4 `TitleBarLayout`;GDI+。
- Produces:
  - `std::vector<own::RectI> own::enumMonitorWorkAreas();`(Win32 薄封装)。
  - `class CNoteWindow : public CWnd`:
    - `bool Create(const own::Note& note, own::NoteStore* store);` —— 建无边框分层置顶窗,按 note 几何(经 `clampRectToWorkArea` 钳制)显示,应用 `opacity`。
    - `int64_t noteId() const;`
    - 内部持 `own::Note m_note;`、`own::NoteStore* m_store;`。
    - 自绘:`OnPaint` 双缓冲画背景(主题色)+ 标题栏 + 四个按钮图标 + 占位内容区文字(`note.plainText` 前若干字或 "（空）")。`OnEraseBkgnd` 返回 TRUE。
- 常量:`static const own::TitleBarMetrics kTitleMetrics{28,20,4,4};` `static const int kMinW=120, kMinH=80, kResizeMargin=6;`

- [ ] **Step 1: 写 MonitorEnum(无独立单测,由 clampRect 的 P1 测试间接覆盖;此处只保证可编译并被 Task 后续使用)**

`src/ui/MonitorEnum.h`:
```cpp
#pragma once
#include <vector>
#include "domain/Models.h"
namespace own { std::vector<RectI> enumMonitorWorkAreas(); }
```
`src/ui/MonitorEnum.cpp`:
```cpp
#include "ui/MonitorEnum.h"
#include <windows.h>
namespace own {
static BOOL CALLBACK cb(HMONITOR h, HDC, LPRECT, LPARAM p) {
    MONITORINFO mi{ sizeof(mi) };
    if (::GetMonitorInfo(h, &mi)) {
        RECT r = mi.rcWork;
        reinterpret_cast<std::vector<RectI>*>(p)->push_back(
            { r.left, r.top, r.right - r.left, r.bottom - r.top });
    }
    return TRUE;
}
std::vector<RectI> enumMonitorWorkAreas() {
    std::vector<RectI> out;
    ::EnumDisplayMonitors(nullptr, nullptr, cb, reinterpret_cast<LPARAM>(&out));
    return out;
}
}
```

- [ ] **Step 2: 写 CNoteWindow 头**

`src/ui/NoteWindow.h`:
```cpp
#pragma once
#include <afxwin.h>
#include "domain/Models.h"
#include "ui/TitleBarLayout.h"
namespace own { class NoteStore; }
class CNoteWindow : public CWnd {
public:
    bool Create(const own::Note& note, own::NoteStore* store);
    int64_t noteId() const { return m_note.id; }
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
private:
    own::TitleBarRects layout() const;   // 用当前 client 尺寸算标题栏
    own::Note m_note;
    own::NoteStore* m_store = nullptr;
};
```

- [ ] **Step 3: 实现创建 + 自绘**

`src/ui/NoteWindow.cpp`(核心;GDI+ 初始化在 Task 前置于 CNoteApp,见下 Step 4 说明):
```cpp
#include "ui/NoteWindow.h"
#include "ui/MonitorEnum.h"
#include "data/NoteStore.h"
#include <gdiplus.h>
using namespace Gdiplus;

static const own::TitleBarMetrics kTitleMetrics{ 28, 20, 4, 4 };

BEGIN_MESSAGE_MAP(CNoteWindow, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

bool CNoteWindow::Create(const own::Note& note, own::NoteStore* store) {
    m_note = note; m_store = store;
    // 越界钳制到可见工作区
    own::RectI clamped = own::clampRectToWorkArea(note.rect, own::enumMonitorWorkAreas());
    m_note.rect = clamped;
    LPCTSTR cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW));
    if (!CreateEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, cls, _T("Note"),
                  WS_POPUP, CRect(clamped.x, clamped.y, clamped.x+clamped.w, clamped.y+clamped.h),
                  nullptr, 0))
        return false;
    ::SetLayeredWindowAttributes(m_hWnd, 0, (BYTE)m_note.opacity, LWA_ALPHA);
    ShowWindow(SW_SHOWNOACTIVATE);
    return true;
}
own::TitleBarRects CNoteWindow::layout() const {
    CRect rc; GetClientRect(&rc);
    return own::layoutTitleBar({0,0,rc.Width(), rc.Height()}, kTitleMetrics);
}
BOOL CNoteWindow::OnEraseBkgnd(CDC*) { return TRUE; }   // 防闪烁，全在 OnPaint 画

static Color fromRGB(uint32_t c) { return Color(255,(c>>16)&0xFF,(c>>8)&0xFF,c&0xFF); }

void CNoteWindow::OnPaint() {
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    // 双缓冲
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* old = mem.SelectObject(&bmp);
    {
        Graphics g(mem.GetSafeHdc());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        // 背景（主题色，暂用 note 的 theme 对应色的简化：黄底，P3/主题接入后替换）
        SolidBrush bg(Color(255, 0xFF, 0xF7, 0xB0));
        g.FillRectangle(&bg, 0, 0, rc.Width(), rc.Height());
        // 标题栏
        auto L = own::layoutTitleBar({0,0,rc.Width(),rc.Height()}, kTitleMetrics);
        SolidBrush title(Color(255, 0xF2, 0xD2, 0x4A));
        g.FillRectangle(&title, L.titleBar.x, L.titleBar.y, L.titleBar.w, L.titleBar.h);
        // 按钮图标（简单符号：× 关闭，─ 卷起，📌用小方块表示 pin，○ 透明）
        Pen pen(Color(255,0x40,0x40,0x40), 2.0f);
        auto drawX = [&](const own::RectI& b){ g.DrawLine(&pen, b.x+4,b.y+4,b.x+b.w-4,b.y+b.h-4);
                                               g.DrawLine(&pen, b.x+b.w-4,b.y+4,b.x+4,b.y+b.h-4); };
        drawX(L.closeBtn);
        g.DrawLine(&pen, L.rollBtn.x+4, L.rollBtn.y+L.rollBtn.h/2, L.rollBtn.x+L.rollBtn.w-4, L.rollBtn.y+L.rollBtn.h/2);
        SolidBrush dot(Color(255,0x40,0x40,0x40));
        g.FillRectangle(&dot, L.pinBtn.x+6, L.pinBtn.y+4, 6, L.pinBtn.h-8);
        g.DrawEllipse(&pen, L.opacityBtn.x+3, L.opacityBtn.y+3, L.opacityBtn.w-6, L.opacityBtn.h-6);
        // 占位内容
        FontFamily ff(L"Segoe UI"); Font font(&ff, 12, FontStyleRegular, UnitPixel);
        SolidBrush text(Color(255,0x20,0x20,0x20));
        std::string body = m_note.plainText.empty() ? std::string("(empty)") : m_note.plainText;
        std::wstring w(body.begin(), body.end());   // ASCII 占位；真正内容 P3 处理
        g.DrawString(w.c_str(), (int)w.size(), &font, PointF((REAL)6, (REAL)(kTitleMetrics.height+6)), &text);
    }
    dc.BitBlt(0,0,rc.Width(),rc.Height(), &mem, 0,0, SRCCOPY);
    mem.SelectObject(old);
}
```
> 说明:主题色此处暂硬编码(黄),Task 10 之后可从 `m_note.theme_id` 查 themes 表着色;本任务只要求窗口显示 + 自绘可见。占位内容仅 ASCII(中文内容渲染属 P3 RichEdit)。

- [ ] **Step 4: 在 CNoteApp 初始化 GDI+ 并显示已存在的可见 note**

`NoteApp.h` 增 `ULONG_PTR m_gdiplusToken=0;` 与 `std::vector<std::unique_ptr<CNoteWindow>> m_notes;`(include `<memory>`, `<vector>`, `ui/NoteWindow.h`, `data/NoteStore.h`)。
`InitInstance` 在 DB 引导成功后:
```cpp
Gdiplus::GdiplusStartupInput gsi;
Gdiplus::GdiplusStartup(&m_gdiplusToken, &gsi, nullptr);
// 显示所有 visible=1 的 note
own::NoteStore store(m_db);
own::NoteQuery q; q.onlyVisible = true;
for (const auto& n : store.query(q)) {
    auto w = std::make_unique<CNoteWindow>();
    if (w->Create(n, /*store owner set later in Task 8*/ nullptr))
        m_notes.push_back(std::move(w));
}
```
`ExitInstance` 调 `Gdiplus::GdiplusShutdown(m_gdiplusToken);`。
> 注:此时若 DB 无 visible note,则不显示任何窗口(正常)。Task 11 加首启欢迎 note 后即可见。为本任务能手工验证,可临时用 sqlite 命令或下条任务的热键;构建+单测通过即达标。

把 `MonitorEnum.cpp`、`NoteWindow.cpp` 加入 app 工程。

- [ ] **Step 5: 构建 + 单测**

Run: `"$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: app 链接通过(GDI+/MFC),tests 仍全绿(本任务未改纯逻辑,数量不变)。

- [ ] **Step 6: Commit**

```bash
git add src/ui/MonitorEnum.* src/ui/NoteWindow.* src/app/NoteApp.* app/open_windows_note_app.vcxproj
git commit -m "feat(ui): CNoteWindow frameless/layered/topmost self-drawn shell + monitor clamp

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: 标题栏拖动移动 + 几何持久化

**Files:**
- Modify: `src/ui/NoteWindow.h`, `src/ui/NoteWindow.cpp`

**Interfaces:**
- Consumes: Task 7 `CNoteWindow`;Task 4 `hitTestTitleBar`;P1 `NoteStore::updateGeometry`。
- Produces:`CNoteWindow` 处理 `WM_LBUTTONDOWN`/`WM_MOUSEMOVE`/`WM_LBUTTONUP`:在 `TitleHit::Drag` 区按下→`SetCapture` 记录锚点;移动→`SetWindowPos` 移动窗口;抬起→`ReleaseCapture` 并 `m_store->updateGeometry(id, 当前RectI, "")` 持久化。`Create` 的 store 参数真正保存到 `m_store`(修正 Task 7 里传 nullptr 的临时做法——本任务改为传入真实 store)。

- [ ] **Step 1: 改 Create 传入真实 store + 加拖动状态**

`NoteWindow.h` 增私有成员:
```cpp
    bool m_dragging = false;
    CPoint m_dragAnchorScreen;   // 按下时鼠标屏幕坐标
    CRect  m_dragStartRect;      // 按下时窗口屏幕矩形
```
消息映射加 `ON_WM_LBUTTONDOWN() ON_WM_MOUSEMOVE() ON_WM_LBUTTONUP()`,声明三个 handler。
`NoteApp.cpp` Task 7 里 `w->Create(n, nullptr)` 改为持久 store:把 `own::NoteStore` 提升为 `CNoteApp` 成员 `std::unique_ptr<own::NoteStore> m_store;`(在 DB 引导后 `m_store = std::make_unique<own::NoteStore>(m_db);`),创建窗口时传 `w->Create(n, m_store.get())`。

- [ ] **Step 2: 实现拖动**

`NoteWindow.cpp`:
```cpp
void CNoteWindow::OnLButtonDown(UINT, CPoint pt) {
    auto L = layout();
    if (own::hitTestTitleBar(L, pt.x, pt.y) == own::TitleHit::Drag) {
        m_dragging = true;
        ::GetCursorPos(&m_dragAnchorScreen);
        GetWindowRect(&m_dragStartRect);
        SetCapture();
    }
}
void CNoteWindow::OnMouseMove(UINT, CPoint) {
    if (!m_dragging) return;
    CPoint cur; ::GetCursorPos(&cur);
    int nx = m_dragStartRect.left + (cur.x - m_dragAnchorScreen.x);
    int ny = m_dragStartRect.top  + (cur.y - m_dragAnchorScreen.y);
    SetWindowPos(nullptr, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
void CNoteWindow::OnLButtonUp(UINT, CPoint) {
    if (!m_dragging) return;
    m_dragging = false; ReleaseCapture();
    CRect r; GetWindowRect(&r);
    m_note.rect = { r.left, r.top, r.Width(), r.Height() };
    if (m_store) m_store->updateGeometry(m_note.id, m_note.rect, "");
}
```

- [ ] **Step 3: 构建 + 单测**

Run: `"$MSB" ... -m 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建通过,tests 全绿(数量不变)。

- [ ] **Step 4: 手工冒烟(记录到报告,勿阻塞)**

若有 GUI:启动→出现 note→拖标题栏移动→关闭再启动→位置保留。无 GUI 会话则记录“需人工验证”。

- [ ] **Step 5: Commit**

```bash
git add src/ui/NoteWindow.* src/app/NoteApp.*
git commit -m "feat(ui): drag note by title bar + persist geometry

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: 边缘缩放 + 尺寸持久化

**Files:**
- Modify: `src/ui/NoteWindow.h`, `src/ui/NoteWindow.cpp`

**Interfaces:**
- Consumes: Task 5 `hitTestResizeEdge`/`applyResize`;`NoteStore::updateGeometry`。
- Produces:`CNoteWindow` 处理 `WM_SETCURSOR`(边缘显示缩放光标)与鼠标缩放:非标题区且命中边→按下进入缩放,`applyResize` 算新屏幕矩形,`SetWindowPos` 应用;抬起持久化。常量 `kMinW=120,kMinH=80,kResizeMargin=6`。

- [ ] **Step 1: 加缩放状态 + 光标**

`NoteWindow.h` 增:
```cpp
    own::ResizeEdge m_resizeEdge = own::ResizeEdge::None;
    bool m_resizing = false;
    CPoint m_resizeAnchorScreen;
    CRect  m_resizeStartRect;
```
include `"ui/ResizeMath.h"`;消息映射加 `ON_WM_SETCURSOR()`;声明 `OnSetCursor`。

- [ ] **Step 2: 实现缩放(改 OnLButtonDown/Move/Up 增加缩放分支)**

`NoteWindow.cpp`:
```cpp
BOOL CNoteWindow::OnSetCursor(CWnd*, UINT, UINT) {
    CPoint pt; ::GetCursorPos(&pt); ScreenToClient(&pt);
    CRect rc; GetClientRect(&rc);
    auto e = own::hitTestResizeEdge({0,0,rc.Width(),rc.Height()}, pt.x, pt.y, 6);
    LPCTSTR c = IDC_ARROW;
    switch (e) {
        case own::ResizeEdge::Left: case own::ResizeEdge::Right: c = IDC_SIZEWE; break;
        case own::ResizeEdge::Top: case own::ResizeEdge::Bottom: c = IDC_SIZENS; break;
        case own::ResizeEdge::TopLeft: case own::ResizeEdge::BottomRight: c = IDC_SIZENWSE; break;
        case own::ResizeEdge::TopRight: case own::ResizeEdge::BottomLeft: c = IDC_SIZENESW; break;
        default: break;
    }
    ::SetCursor(::LoadCursor(nullptr, c));
    return TRUE;
}
```
在 `OnLButtonDown` 顶部(标题命中判断之前)加缩放优先:
```cpp
    CRect rc; GetClientRect(&rc);
    auto edge = own::hitTestResizeEdge({0,0,rc.Width(),rc.Height()}, pt.x, pt.y, 6);
    if (edge != own::ResizeEdge::None) {
        m_resizing = true; m_resizeEdge = edge;
        ::GetCursorPos(&m_resizeAnchorScreen); GetWindowRect(&m_resizeStartRect);
        SetCapture(); return;
    }
```
在 `OnMouseMove` 加:
```cpp
    if (m_resizing) {
        CPoint cur; ::GetCursorPos(&cur);
        int dx = cur.x - m_resizeAnchorScreen.x, dy = cur.y - m_resizeAnchorScreen.y;
        own::RectI start{ m_resizeStartRect.left, m_resizeStartRect.top,
                          m_resizeStartRect.Width(), m_resizeStartRect.Height() };
        own::RectI nr = own::applyResize(start, m_resizeEdge, dx, dy, 120, 80);
        SetWindowPos(nullptr, nr.x, nr.y, nr.w, nr.h, SWP_NOZORDER | SWP_NOACTIVATE);
        Invalidate(FALSE);
        return;
    }
```
在 `OnLButtonUp` 加:
```cpp
    if (m_resizing) {
        m_resizing = false; ReleaseCapture();
        CRect r; GetWindowRect(&r);
        m_note.rect = { r.left, r.top, r.Width(), r.Height() };
        if (m_store) m_store->updateGeometry(m_note.id, m_note.rect, "");
        return;
    }
```

- [ ] **Step 3: 构建 + 单测**

Run: `"$MSB" ... -m 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建通过,tests 全绿。

- [ ] **Step 4: 手工冒烟(记录)** — 边缘拖动缩放、到下限锁死、重启尺寸保留。

- [ ] **Step 5: Commit**

```bash
git add src/ui/NoteWindow.*
git commit -m "feat(ui): edge/corner resize with min-size + persist size

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 10: 标题栏按钮 — 关闭 / pin / 卷起 / 透明度 + flags 持久化

**Files:**
- Modify: `src/ui/NoteWindow.h`, `src/ui/NoteWindow.cpp`

**Interfaces:**
- Consumes: Task 4 `hitTestTitleBar`;`NoteStore::updateFlags`/`deleteNote`。
- Produces:`CNoteWindow` 在 `WM_LBUTTONDOWN` 命中按钮时执行:
  - **Close**:隐藏窗口 + `updateFlags(id, opacity, pinned, rolledUp, /*visible*/false)`(P2 关闭=隐藏,不删数据;删除属 P4 管理器)。
  - **Pin**:切换 `m_note.pinned`,`SetWindowPos(HWND_TOPMOST/HWND_NOTOPMOST)`,`updateFlags`。
  - **Roll**:切换 `m_note.rolledUp`;卷起时把窗口高设为标题栏高、展开时恢复上次高度(记 `m_expandedHeight`);`updateFlags`。
  - **Opacity**:在 {255,204,153,102} 间循环,`SetLayeredWindowAttributes`,`updateFlags`。
- 新增私有 `int m_expandedHeight = 0;`(展开态高度缓存)。

- [ ] **Step 1: 加按钮处理**

`NoteWindow.h` 增 `int m_expandedHeight = 0;`。`NoteWindow.cpp` 在 `OnLButtonDown` 的标题命中分支替换为:
```cpp
    auto L = layout();
    switch (own::hitTestTitleBar(L, pt.x, pt.y)) {
        case own::TitleHit::Close: {
            ShowWindow(SW_HIDE);
            m_note.visible = false;
            if (m_store) m_store->updateFlags(m_note.id, m_note.opacity, m_note.pinned, m_note.rolledUp, false);
            return;
        }
        case own::TitleHit::Pin: {
            m_note.pinned = !m_note.pinned;
            SetWindowPos(m_note.pinned ? &wndTopMost : &wndNoTopMost, 0,0,0,0,
                         SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
            if (m_store) m_store->updateFlags(m_note.id, m_note.opacity, m_note.pinned, m_note.rolledUp, m_note.visible);
            Invalidate(FALSE); return;
        }
        case own::TitleHit::Roll: {
            CRect r; GetWindowRect(&r);
            if (!m_note.rolledUp) {
                m_expandedHeight = r.Height();
                SetWindowPos(nullptr,0,0,r.Width(), kTitleMetricsHeight(), SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                m_note.rolledUp = true;
            } else {
                int h = m_expandedHeight > 0 ? m_expandedHeight : 200;
                SetWindowPos(nullptr,0,0,r.Width(), h, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                m_note.rolledUp = false;
            }
            if (m_store) m_store->updateFlags(m_note.id, m_note.opacity, m_note.pinned, m_note.rolledUp, m_note.visible);
            Invalidate(FALSE); return;
        }
        case own::TitleHit::Opacity: {
            static const int steps[] = {255,204,153,102};
            int idx = 0; for (int i=0;i<4;++i) if (steps[i]==m_note.opacity) { idx=i; break; }
            m_note.opacity = steps[(idx+1)%4];
            ::SetLayeredWindowAttributes(m_hWnd, 0, (BYTE)m_note.opacity, LWA_ALPHA);
            if (m_store) m_store->updateFlags(m_note.id, m_note.opacity, m_note.pinned, m_note.rolledUp, m_note.visible);
            return;
        }
        case own::TitleHit::Drag: {
            m_dragging = true; ::GetCursorPos(&m_dragAnchorScreen); GetWindowRect(&m_dragStartRect); SetCapture(); return;
        }
        default: break;
    }
```
加一个小 helper `static int kTitleMetricsHeight(){ return 28; }`(或直接用 `kTitleMetrics.height`)。

- [ ] **Step 2: 卷起后只画标题栏**

`OnPaint` 里内容绘制加保护:`if (!m_note.rolledUp) { …画内容… }`(卷起时 client 高度≈标题栏高,内容自然不显;此判断避免负尺寸绘制)。

- [ ] **Step 3: 构建 + 单测**

Run: `"$MSB" ... -m 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建通过,tests 全绿。

- [ ] **Step 4: 手工冒烟(记录)** — 四个按钮各自生效;关闭后重启不再显示(visible=0);pin/卷起/透明重启后保留。

- [ ] **Step 5: Commit**

```bash
git add src/ui/NoteWindow.*
git commit -m "feat(ui): title-bar buttons (close/pin/roll/opacity) + persist flags

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 11: 新建热键 + 首启欢迎 note + 单实例

**Files:**
- Modify: `src/app/AppHostWindow.h`, `src/app/AppHostWindow.cpp`, `src/app/NoteApp.h`, `src/app/NoteApp.cpp`

**Interfaces:**
- Consumes:P1 `NoteStore::insertNote`/`allNotes`/`query`;Task 7-10 `CNoteWindow`。
- Produces:
  - `CAppHostWindow` 新增 `static const UINT kHotkeyNew = 2;` 注册 `Ctrl+Alt+N`;`WM_HOTKEY` → 让 App 新建一条 note 并弹窗。
  - `CAppHostWindow` 暴露回调:持 `CNoteApp*`,或用注册消息通知 App。简化:`CAppHostWindow` 持 `std::function<void()> onNewNote;` 与 `std::function<void()> onQuit;`,由 `CNoteApp::InitInstance` 装配。
  - 单实例:`CNoteApp::InitInstance` 开头 `CreateMutex(nullptr,FALSE,L"open_windows_note_singleton")`;若 `ERROR_ALREADY_EXISTS`,用 `FindWindow(L"OwnAppHost"...)`/注册消息通知已有实例新建一条 note,然后 `return FALSE` 退出。
  - 首启:引导后若 `store.query({onlyVisible})` 为空,插入一条欢迎 note(标题“欢迎”,plainText 提示按 Ctrl+Alt+N 新建、Ctrl+Alt+Q 退出),再显示。

- [ ] **Step 1: HostWindow 加新建热键 + 回调**

`AppHostWindow.h`:
```cpp
#include <functional>
// class 内:
    static const UINT kHotkeyNew = 2;
    std::function<void()> onNewNote;
    std::function<void()> onQuit;
```
`AppHostWindow.cpp` `Create()` 注册:`::RegisterHotKey(m_hWnd, kHotkeyNew, MOD_CONTROL|MOD_ALT, 'N');` 与已有 Quit;`OnHotKey`:
```cpp
void CAppHostWindow::OnHotKey(UINT id, UINT, UINT) {
    if (id == kHotkeyQuit) { if (onQuit) onQuit(); else ::PostQuitMessage(0); }
    else if (id == kHotkeyNew) { if (onNewNote) onNewNote(); }
}
```
`OnDestroy` 也 `UnregisterHotKey(..., kHotkeyNew)`。

- [ ] **Step 2: App 装配回调 + 新建逻辑 + 首启欢迎 + 单实例**

`NoteApp.h` 增私有方法 `void createAndShowNote(const own::Note& seed);` 与 `HANDLE m_singleton = nullptr;`。
`NoteApp.cpp`:
```cpp
BOOL CNoteApp::InitInstance() {
    CWinApp::InitInstance();
    m_singleton = ::CreateMutex(nullptr, FALSE, _T("open_windows_note_singleton"));
    if (m_singleton && ::GetLastError() == ERROR_ALREADY_EXISTS) {
        // 通知已有实例新建一条，然后退出
        HWND h = ::FindWindow(nullptr, _T("OwnAppHost"));
        if (h) ::PostMessage(h, WM_HOTKEY, CAppHostWindow::kHotkeyNew, 0);
        return FALSE;
    }
    // …(Task 6 的 DB 引导 + Task 7 的 GDI+ 启动 + m_store 构造)…
    m_host.onNewNote = [this]{
        own::Note n; n.type = own::NoteType::RichText; n.visible = true;
        n.plainText = "new note";
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));  // App 层可用 time()
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
    };
    m_host.onQuit = []{ ::PostQuitMessage(0); };
    if (!m_host.Create()) return FALSE;
    m_pMainWnd = &m_host;
    // 显示已有可见 note；首启为空则建欢迎 note
    own::NoteQuery q; q.onlyVisible = true;
    auto notes = m_store->query(q);
    if (notes.empty()) {
        own::Note w; w.visible=true; w.plainText="welcome - Ctrl+Alt+N new / Ctrl+Alt+Q quit";
        int64_t id = m_store->insertNote(w, (int64_t)time(nullptr));
        auto full = m_store->getNote(id); if (full) notes.push_back(*full);
    }
    for (const auto& n : notes) createAndShowNote(n);
    return TRUE;
}
void CNoteApp::createAndShowNote(const own::Note& seed) {
    auto w = std::make_unique<CNoteWindow>();
    if (w->Create(seed, m_store.get())) m_notes.push_back(std::move(w));
}
int CNoteApp::ExitInstance() {
    if (m_singleton) ::CloseHandle(m_singleton);
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
    return CWinApp::ExitInstance();
}
```
> 注:`FindWindow` 按窗口标题 `OwnAppHost` 查找(Task 1 用该标题创建 host)。`WM_HOTKEY` 转发是简化的进程间通知,P5 会换成正式方案。

- [ ] **Step 3: 构建 + 单测**

Run: `"$MSB" ... -m 2>&1 | tail -5 && ./x64/Debug/tests.exe 2>&1 | tail -3`
Expected: 构建通过,tests 全绿。

- [ ] **Step 4: 手工冒烟(记录)** — 首启出现欢迎 note;`Ctrl+Alt+N` 新建;再启动(第二实例)转发新建并退出;`Ctrl+Alt+Q` 退出。

- [ ] **Step 5: Commit**

```bash
git add src/app/AppHostWindow.* src/app/NoteApp.*
git commit -m "feat(app): new-note hotkey, first-run welcome note, single-instance

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 12: 手工冒烟清单 + P2 收尾文档

**Files:**
- Create: `docs/superpowers/smoke/P2-smoke-checklist.md`

**Interfaces:** 无代码;记录人工验证步骤,供执行者与用户对照。

- [ ] **Step 1: 写冒烟清单**

`docs/superpowers/smoke/P2-smoke-checklist.md`,逐条可勾:
```markdown
# P2 手工冒烟清单（open_windows_note.exe）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

- [ ] 首次启动：出现一条“welcome”悬浮便签，置顶、无标准边框
- [ ] 数据库文件生成在 exe 目录（便携）或 %APPDATA%\open_windows_note（只读目录回落）
- [ ] 拖动标题栏空白处：窗口跟随移动
- [ ] 拖动四边/四角：窗口缩放；缩到最小锁死（≥120x80）
- [ ] 关闭按钮(×)：便签隐藏；重启后不再出现（visible=0）
- [ ] pin 按钮：切换置顶/非置顶（用另一窗口盖上验证）
- [ ] 卷起按钮(─)：折叠为标题栏；再点展开恢复原高
- [ ] 透明按钮(○)：在 100%/80%/60%/40% 间循环
- [ ] Ctrl+Alt+N：新建一条便签
- [ ] 移动/缩放/pin/卷起/透明后关闭程序再启动：状态全部保留
- [ ] 再次启动 exe（第二实例）：不新开进程，转发为“新建一条”，随即退出
- [ ] Ctrl+Alt+Q：程序退出
- [ ] 多显示器：把便签拖到副屏，重启后仍在可见区；拔掉副屏后重启，便签回到主屏可见区（clampRectToWorkArea）
```

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/smoke/P2-smoke-checklist.md
git commit -m "docs: P2 manual smoke checklist

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review

**Spec coverage(对照设计文档):**
- §1.1 MFC 静态 + /utf-8 + 便携单 exe → Task 1、Global Constraints。✓
- §1.2 主管理窗 + N 悬浮窗:P2 只做**悬浮窗**与隐藏宿主;管理窗属 P4(已在范围外声明)。✓
- §4 `notes.db` 路径(便携+`%APPDATA%` 回落)→ Task 2;DB 打开/`integrity_check`/`.corrupt` 备份重建/迁移 → Task 6;`settings` 表读写 `SettingsStore` → Task 3。✓
- §3 组件:`CNoteApp`(Task 1/6/11)、`CNoteWindow`(Task 7-10)、几何/flags 持久化 → Task 8/9/10;多屏 `clampRectToWorkArea` → Task 7;自绘标题栏(拖/关/卷/pin/透明)+ resize → Task 4/5/7/8/9/10。✓
- §6 错误处理:DB 打开失败弹框 + 回落 → Task 6;损坏备份重建 → Task 6;off-screen 钳制 → Task 7;单实例互斥 → Task 11。✓
- §7 测试:纯逻辑 doctest(路径/设置/标题栏/缩放/bootstrap)→ Task 2-6;窗口行为手工冒烟 → Task 12。✓
- 渲染 GDI/GDI+、一律自绘、`WS_EX_LAYERED`+`WS_EX_TOPMOST`+`WS_POPUP` → Task 7。✓

**Placeholder scan:** 无 TBD/TODO;每个代码步骤给出可编译代码。主题着色在 Task 7 明确标注“暂硬编码黄,主题接入留待后续”并非占位符,而是范围声明(themes 读取 API 属 P4;P2 不承诺主题切换 UI)。占位内容区仅 ASCII(中文渲染属 P3 RichEdit),已在 Task 7 注明。

**Type consistency:** `own::RectI`(P1)贯穿;`TitleBarRects`/`TitleHit`(Task 4)在 Task 7/8/10 一致使用;`ResizeEdge`/`applyResize`(Task 5)在 Task 9 一致;`chooseDbPath`/`DbPathChoice`(Task 2)、`SettingsStore`(Task 3)、`openDatabaseAtPath`(Task 6)、`CNoteWindow::Create(Note, NoteStore*)`(Task 7 起)签名一致;`kTitleMetrics`(28,20,4,4)全程一致。`m_store` 由 Task 8 起为 `CNoteApp` 持有的 `unique_ptr<NoteStore>`,窗口持裸指针,一致。

**范围外(明确留后续):** 主题色切换/themes 读取(P4)、真正内容渲染(P3)、正式全局热键与托盘、进程间通知的健壮方案(P5)。P2 的 `Ctrl+Alt+N/Q` 与 `WM_HOTKEY` 转发是脚手架,已在计划中标注将被 P5 替换。

**已知限制(执行者须知):** 无 GUI 会话时,窗口交互无法自动化验证;各 GUI 任务以“app 工程构建链接通过 + tests 全绿”为自动化达标线,窗口行为落到 Task 12 手工清单,由用户执行。
