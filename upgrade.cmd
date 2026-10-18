@echo off
setlocal
call "%~dp0pakar.cmd" dependencies %*
exit /b %errorlevel%
