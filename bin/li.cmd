@echo off
icd.exe %temp%\icd_output
set /p icd_output=<%temp%\icd_output
cd /d "%icd_output%"