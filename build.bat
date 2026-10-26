@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem ===== usage: build.bat ["C:\path\to\Geometry Dash"] =====

rem ---- 1. find MSVC ----
set "VCVARS="
for %%E in (Community Professional Enterprise BuildTools) do (
    if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
)
if defined VCVARS goto :have_vs

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :novs
if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%VSDIR%\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS goto :novs

:have_vs
echo [*] Using: !VCVARS!
call "!VCVARS!" >nul
where cl >nul 2>nul
if errorlevel 1 goto :novs

rem ---- 2. download libs ----
if not exist libs mkdir libs

if exist libs\imgui\imgui.cpp goto :have_imgui
echo [*] Downloading ImGui...
curl -L -s -o libs\imgui.zip https://github.com/ocornut/imgui/archive/refs/tags/v1.91.5.zip
powershell -NoProfile -Command "Expand-Archive -Force libs\imgui.zip libs\_t; Move-Item libs\_t\* libs\imgui; Remove-Item libs\_t"
del libs\imgui.zip 2>nul
:have_imgui
if not exist libs\imgui\imgui.cpp goto :dlfail

if exist libs\minhook\src\hook.c goto :have_minhook
echo [*] Downloading MinHook...
curl -L -s -o libs\minhook.zip https://github.com/TsudaKageyu/minhook/archive/refs/heads/master.zip
powershell -NoProfile -Command "Expand-Archive -Force libs\minhook.zip libs\_t; Move-Item libs\_t\* libs\minhook; Remove-Item libs\_t"
del libs\minhook.zip 2>nul
:have_minhook
if not exist libs\minhook\src\hook.c goto :dlfail

rem ---- 3. build ----
if not exist obj mkdir obj
if not exist out mkdir out
echo [*] Building...
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /LD /DNDEBUG /DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD ^
  /I libs\minhook\include /I libs\imgui /I libs\imgui\backends ^
  /Fo:obj\ /Fe:out\XInput1_4.dll ^
  dllmain.cpp proxy.cpp ^
  libs\minhook\src\buffer.c libs\minhook\src\hook.c libs\minhook\src\trampoline.c libs\minhook\src\hde\hde64.c ^
  libs\imgui\imgui.cpp libs\imgui\imgui_draw.cpp libs\imgui\imgui_tables.cpp libs\imgui\imgui_widgets.cpp ^
  libs\imgui\backends\imgui_impl_win32.cpp libs\imgui\backends\imgui_impl_opengl3.cpp ^
  /link /DLL opengl32.lib user32.lib gdi32.lib dwmapi.lib
if errorlevel 1 goto :buildfail

echo.
echo [OK] Built: %CD%\out\XInput1_4.dll

rem ---- 4. copy to game folder ----
set "GD=%~1"
if "!GD!"=="" set "GD=C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash"
if not exist "!GD!\GeometryDash.exe" goto :nocopy
copy /y out\XInput1_4.dll "!GD!\" >nul
echo [OK] Copied to: !GD!
echo Start the game and press INSERT.
exit /b 0

:nocopy
echo [ERROR] Game folder not found: !GD!
echo     Copy out\XInput1_4.dll next to GeometryDash.exe manually,
echo     or run:  build.bat "D:\path\to\Geometry Dash"
exit /b 0

:novs
echo [ERROR] Could not start the MSVC compiler.
echo     Open the folder in cmd and run these two lines, then send me the output:
echo     "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
echo     where cl
exit /b 1

:dlfail
echo [ERROR] Failed to download libs. Check your internet connection and try again.
exit /b 1

:buildfail
echo [ERROR] Build failed. Send me the error text above.
exit /b 1
