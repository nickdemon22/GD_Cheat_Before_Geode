@echo off
setlocal
cd /d "%~dp0"

rem ===== usage: push.bat ["commit message"] =====

set "REPO=https://github.com/nickdemon22/GD_Cheat_Before_Geode.git"
set "MSG=%~1"
if "%MSG%"=="" set "MSG=Update"

where git >nul 2>nul
if errorlevel 1 goto :nogit

rem ---- .gitignore (created once) ----
if exist .gitignore goto :have_ignore
(
echo libs/
echo obj/
echo out/
echo diag.txt
echo *.log
) > .gitignore
:have_ignore

rem ---- repo + remote ----
if not exist .git git init >nul
git remote get-url origin >nul 2>nul
if errorlevel 1 (git remote add origin "%REPO%") else (git remote set-url origin "%REPO%")

rem ---- remote default branch (main by default) ----
set "BR=main"
for /f "tokens=2" %%b in ('git ls-remote --symref origin HEAD 2^>nul ^| findstr /b "ref:"') do set "BR=%%b"
set "BR=%BR:refs/heads/=%"
echo [*] Branch: %BR%

git rev-parse --verify HEAD >nul 2>nul
if errorlevel 1 (git symbolic-ref HEAD refs/heads/%BR%) else (git branch -M %BR%)

rem ---- commit ----
git add -A
git diff --cached --quiet
if errorlevel 1 git commit -m "%MSG%"

rem ---- merge with remote (keeps local files on conflicts) ----
git ls-remote --exit-code --heads origin %BR% >nul 2>nul
if errorlevel 1 goto :push
echo [*] Merging with remote...
git pull --no-rebase --allow-unrelated-histories -X ours --no-edit origin %BR%
if errorlevel 1 goto :mergefail

:push
echo [*] Pushing...
git push -u origin %BR%
if errorlevel 1 goto :pushfail
echo.
echo [OK] Done: %REPO%
exit /b 0

:nogit
echo [ERROR] Git not found. Install it: winget install Git.Git
exit /b 1

:mergefail
echo [ERROR] Merge failed. Run "git status" and resolve conflicts manually.
exit /b 1

:pushfail
echo [ERROR] Push failed. Check your GitHub login or token and try again.
exit /b 1
