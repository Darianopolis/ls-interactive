@echo off
li-win.exe %temp%\li_output
set /p li_output=<%temp%\li_output
cd /d "%li_output%"