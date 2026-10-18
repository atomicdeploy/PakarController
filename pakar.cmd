@echo off
setlocal
where node.exe >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Node.js20.19 or newer is required. 1>&2
  exit /b 3
)
node.exe "%~dp0tools\pakar.mjs" %*
exit /b %errorlevel%
