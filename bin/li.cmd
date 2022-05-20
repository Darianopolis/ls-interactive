@echo off
li-win.exe 1 %temp%\li_output
set /p li_output=<%temp%\li_output
cd /d "%li_output%"