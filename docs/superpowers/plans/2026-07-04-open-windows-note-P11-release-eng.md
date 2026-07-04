# P11 发布工程 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给 v1 补齐发布件：exe 版本资源（文件属性可见 1.0.0）、一键打包脚本（Release 构建 + zip）、GitHub Actions CI（构建 + doctest），最后打本地 tag `v1.0.0`。

**Architecture:** 版本资源走标准 VERSIONINFO `.rc`（app 工程加 ResourceCompile 项，版本号唯一源 `app/version.h` 供 rc 与未来 UI 复用）。打包用 `scripts/make_release.cmd`（MSBuild Release + PowerShell `Compress-Archive`，产物进 git 忽略的 `dist/`）。CI 用 windows-2022 runner（自带 VS2022 + MFC 组件），构建 Debug 跑 doctest + 构建 Release 传 exe 工件。

**Tech Stack:** rc.exe/VERSIONINFO · cmd + PowerShell Compress-Archive · GitHub Actions（windows-2022, microsoft/setup-msbuild）。

## Global Constraints

- 构建：`"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"`（`$MSB`）`-p:Platform=x64 -m -v:m -nologo`，配置按任务指明。
- **每次重建前**：`taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null`。
- 存活检查用 `tasklist //FI "IMAGENAME eq open_windows_note.exe"`。
- 编码：新 `.rc` 保存为带 BOM 的 UTF-8 或纯 ASCII（rc.exe 对无 BOM UTF-8 中文会乱码——**本计划 rc 内容全 ASCII**，公司名/描述用英文，规避整个问题）。
- 版本号唯一源：`app/version.h`（`OWN_VER_*` 宏）；脚本/CI 里的 `1.0.0` 字样必须与其一致。
- 提交尾注：`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。
- 分支：main 直上。基线：104 用例 / 435 断言全绿（Debug+Release）。
- CI 文件无法本地执行——验证=YAML 结构自检 + 步骤命令与本地已验证命令一致；真实跑通留到 push 后（报告中注明）。

**承接的既有接口：** `open_windows_note.sln`（两工程 Debug/Release|x64 均绿）；`app/open_windows_note_app.vcxproj`（手编 XML，无 .rc 项）；`README.md`/`LICENSE` 已就绪；`.gitignore`（需确认忽略 `dist/`，没有就加）。

**范围外（声明）：** 应用图标 .ico（无美术资产，托盘继续系统图标）；自动更新；安装器（本应用主打便携 zip）；GitHub Release 自动发布（CI 只传构建工件）；push/tag 推送（由用户执行）。

---

### Task 1: 版本资源（version.h + app.rc + vcxproj）

**Files:**
- Create: `app/version.h`, `app/app.rc`
- Modify: `app/open_windows_note_app.vcxproj`

**Interfaces:**
- Produces: `OWN_VER_MAJOR/MINOR/PATCH/BUILD`、`OWN_VER_STRING "1.0.0"`（Task 2/3 的 1.0.0 字样对齐它）。

- [ ] **Step 1: 写文件**

`app/version.h`：
```c
#pragma once
// 版本唯一源：rc 与代码共用。发版时只改这里。
#define OWN_VER_MAJOR 1
#define OWN_VER_MINOR 0
#define OWN_VER_PATCH 0
#define OWN_VER_BUILD 0
#define OWN_VER_STRING "1.0.0"
```
`app/app.rc`（纯 ASCII；不依赖 afxres.h，用 winres.h）：
```rc
#include <winres.h>
#include "version.h"

VS_VERSION_INFO VERSIONINFO
 FILEVERSION OWN_VER_MAJOR,OWN_VER_MINOR,OWN_VER_PATCH,OWN_VER_BUILD
 PRODUCTVERSION OWN_VER_MAJOR,OWN_VER_MINOR,OWN_VER_PATCH,OWN_VER_BUILD
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS 0x40004L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", "open_windows_note project"
            VALUE "FileDescription", "open_windows_note - portable desktop sticky notes"
            VALUE "FileVersion", OWN_VER_STRING
            VALUE "InternalName", "open_windows_note"
            VALUE "LegalCopyright", "MIT License"
            VALUE "OriginalFilename", "open_windows_note.exe"
            VALUE "ProductName", "open_windows_note"
            VALUE "ProductVersion", OWN_VER_STRING
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
```
`app/open_windows_note_app.vcxproj`：ClInclude 组加 `<ClInclude Include="version.h" />`；新加一个 ItemGroup：
```xml
  <ItemGroup>
    <ResourceCompile Include="app.rc" />
  </ItemGroup>
```
（位置照既有 ItemGroup 排布；路径相对 vcxproj 所在的 app/ 目录，故不带 `..\`。）

- [ ] **Step 2: 双配置构建 + 属性验证 + 存活**

Run: `taskkill //F //IM open_windows_note.exe 2>/dev/null; taskkill //F //IM tests.exe 2>/dev/null; "$MSB" open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; "$MSB" open_windows_note.sln -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo 2>&1 | grep -iE "error"|head; ./x64/Debug/tests.exe 2>&1 | tail -2; powershell -NoProfile -Command "(Get-Item 'x64/Release/open_windows_note.exe').VersionInfo.FileVersion"`
Expected: 两配置 0 error；tests 全绿；PowerShell 输出 `1.0.0.0`。

- [ ] **Step 3: Commit**

```bash
git add app/version.h app/app.rc app/open_windows_note_app.vcxproj
git commit -m "build: VERSIONINFO resource 1.0.0 with single-source version.h

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: 打包脚本 scripts/make_release.cmd

**Files:**
- Create: `scripts/make_release.cmd`
- Modify: `.gitignore`（若未忽略 `dist/` 则加一行 `dist/`）

**Interfaces:**
- Consumes: Task 1 的版本号（脚本从 `app\version.h` 解析 `OWN_VER_STRING`，不硬编码）。
- Produces: `dist\open_windows_note-v<ver>-x64.zip`（含 open_windows_note.exe / README.md / LICENSE）。

- [ ] **Step 1: 写脚本**

`scripts/make_release.cmd`：
```bat
@echo off
setlocal enabledelayedexpansion
rem 一键发布包：Release 构建 + zip（exe/README/LICENSE）。在仓库根或 scripts\ 下执行均可。
cd /d "%~dp0.."

for /f tokens^=3 %%v in ('findstr /c:"#define OWN_VER_STRING" app\version.h') do set RAWVER=%%v
set VER=%RAWVER:"=%
if "%VER%"=="" ( echo [ERROR] cannot read OWN_VER_STRING from app\version.h & exit /b 1 )

set MSB="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not exist %MSB% ( echo [ERROR] MSBuild not found: %MSB% & exit /b 1 )

taskkill /F /IM open_windows_note.exe >nul 2>&1
taskkill /F /IM tests.exe >nul 2>&1
%MSB% open_windows_note.sln -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo
if errorlevel 1 ( echo [ERROR] Release build failed & exit /b 1 )

x64\Release\tests.exe >nul
if errorlevel 1 ( echo [ERROR] tests failed & exit /b 1 )

set STAGE=dist\stage
if exist dist rmdir /s /q dist
mkdir %STAGE%
copy /y x64\Release\open_windows_note.exe %STAGE% >nul
copy /y README.md %STAGE% >nul
copy /y LICENSE %STAGE% >nul

set ZIP=dist\open_windows_note-v%VER%-x64.zip
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 ( echo [ERROR] zip failed & exit /b 1 )
rmdir /s /q %STAGE%

echo [OK] %ZIP%
endlocal
```
`.gitignore`：确认/追加 `dist/`。

- [ ] **Step 2: 跑通验证**

Run（git-bash 里调 cmd）: `cmd //c scripts\\make_release.cmd && ls -la dist/ && powershell -NoProfile -Command "(Get-ChildItem dist/*.zip).Length"`
Expected: `[OK] dist\open_windows_note-v1.0.0-x64.zip`；zip 存在且 > 1MB（静态 MFC exe）；`git status` 不出现 dist/（已忽略）。

- [ ] **Step 3: Commit**

```bash
git add scripts/make_release.cmd .gitignore
git commit -m "build: one-shot release packaging script (Release build + tests + zip)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: GitHub Actions CI + 打 tag

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: 本地已验证的构建/测试命令；windows-2022 镜像自带 VS2022（含 MFC/ATL 组件）。
- 完成后本地打 `git tag v1.0.0`（**不 push**——由用户决定）。

- [ ] **Step 1: 写 workflow**

`.github/workflows/ci.yml`：
```yaml
name: ci
on:
  push:
    branches: [ main ]
    tags: [ 'v*' ]
  pull_request:

jobs:
  build-test:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4
      - uses: microsoft/setup-msbuild@v2
      - name: Build Debug
        run: msbuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo
      - name: Run tests
        run: x64\Debug\tests.exe
      - name: Build Release
        run: msbuild open_windows_note.sln -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo
      - name: Run tests (Release)
        run: x64\Release\tests.exe
      - name: Upload exe artifact
        uses: actions/upload-artifact@v4
        with:
          name: open_windows_note-x64
          path: x64/Release/open_windows_note.exe
```

- [ ] **Step 2: 自检**

- YAML 缩进/结构目检；步骤命令与本地已跑通的 MSBuild/tests 命令一致（仅路径分隔符差异，runner 的 shell 是 pwsh，`x64\Debug\tests.exe` 可执行）。
- 报告中注明：真实 CI 跑通需 push 后在 GitHub 观察，本任务无法本地执行。

- [ ] **Step 3: Commit + tag**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: GitHub Actions — build both configs, run doctest, upload Release exe

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
git tag -a v1.0.0 -m "v1.0.0 - full v1 feature scope (P1-P10)"
```
（tag 只建本地；push 由用户执行。）

---

## Self-Review

**1. Coverage：** 发布件四项（版本资源/打包/CI/tag）各有任务；图标、安装器、自动 Release 显式范围外。✓
**2. Placeholder scan：** 无 TBD；rc 全 ASCII 规避 rc.exe 编码坑；脚本版本号解析自 version.h 非硬编码。✓
**3. 一致性：** `OWN_VER_STRING "1.0.0"` ↔ 脚本解析 ↔ tag `v1.0.0` ↔ zip 名 `v%VER%`；vcxproj ResourceCompile 相对路径（app/ 下不带 `..\`）与 ClCompile 惯例（带 `..\` 指向 src/）不同——因 app.rc 就在 app/ 目录，正确。✓

**已知限制：** CI 未经真实运行验证（push 后观察；windows-2022 含 MFC 组件是镜像文档事实，但首跑仍可能踩镜像差异）。cmd 脚本的 findstr 解析对 version.h 格式敏感（`#define OWN_VER_STRING "1.0.0"` 单空格分隔——version.h 与脚本同计划维护，可接受）。
