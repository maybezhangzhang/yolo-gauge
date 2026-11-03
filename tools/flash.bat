@echo off
rem Double-click wrapper: flash the board over JTAG and leave the window open.
rem Extra arguments are forwarded, e.g.  flash.bat -RunSeconds 25 -Capture
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash.ps1" %*
echo.
pause
