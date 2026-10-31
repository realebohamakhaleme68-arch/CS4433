@echo off
REM Configures and builds the project (MSYS2 UCRT64 toolchain).
set PATH=C:\msys64\ucrt64\bin;%PATH%
cd /d "%~dp0"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 goto :fail
cmake --build build
if errorlevel 1 goto :fail
echo.
echo BUILD OK. Now run run.bat
pause
exit /b 0
:fail
echo.
echo BUILD FAILED - copy the messages above and send them for help.
pause
exit /b 1
