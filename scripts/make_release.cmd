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
