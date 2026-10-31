@echo off
set PATH=C:\msys64\ucrt64\bin;%PATH%
cd /d "%~dp0"
build\lms_tests.exe
echo.
echo (tests ended with code %errorlevel%)
pause
