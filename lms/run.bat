@echo off
REM Starts the LMS in a window that stays open, from the correct folder.
set PATH=C:\msys64\ucrt64\bin;%PATH%
cd /d "%~dp0"
if not exist build\lms.exe (
  echo build\lms.exe not found. Run build.bat first.
  pause
  exit /b 1
)
build\lms.exe --version
echo.
build\lms.exe
echo.
echo (program ended with code %errorlevel%)
pause
