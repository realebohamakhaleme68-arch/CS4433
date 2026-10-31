@echo off
set PATH=C:\msys64\ucrt64\bin;%PATH%
cd /d "%~dp0"
if not exist lms.exe (
  echo lms.exe not found. Run simple_build.bat first.
  pause
  exit /b 1
)
lms.exe
echo.
echo (program ended with code %errorlevel%)
pause
