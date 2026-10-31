@echo off
REM Builds lms.exe with plain g++ (no CMake needed). Needs MSYS2 UCRT64 gcc + sqlite3 installed.
set PATH=C:\msys64\ucrt64\bin;%PATH%
cd /d "%~dp0"
where g++ >nul 2>nul
if errorlevel 1 (
  echo g++ not found. Install MSYS2 and run the pacman command from README.md section 1.
  pause
  exit /b 1
)
echo Compiling... this takes about a minute.
g++ -std=c++17 -O1 -Isrc -Igenerated src/main.cpp src/util/TimeUtil.cpp src/util/PasswordHasher.cpp src/util/InputValidator.cpp src/domain/Models.cpp src/domain/UserFactory.cpp src/data/DatabaseConnection.cpp src/data/SQLiteRepositories.cpp src/service/Session.cpp src/service/Security.cpp src/service/AuditLogger.cpp src/service/Services.cpp src/ui/Console.cpp src/ui/Controllers.cpp src/ui/Application.cpp -lsqlite3 -static-libgcc -static-libstdc++ -o lms.exe
if errorlevel 1 goto :fail
g++ -std=c++17 -O1 -Isrc -Igenerated tests/test_main.cpp src/util/TimeUtil.cpp src/util/PasswordHasher.cpp src/util/InputValidator.cpp src/domain/Models.cpp src/domain/UserFactory.cpp src/data/DatabaseConnection.cpp src/data/SQLiteRepositories.cpp src/service/Session.cpp src/service/Security.cpp src/service/AuditLogger.cpp src/service/Services.cpp src/ui/Console.cpp src/ui/Controllers.cpp src/ui/Application.cpp -lsqlite3 -static-libgcc -static-libstdc++ -o lms_tests.exe
if errorlevel 1 goto :fail
echo.
echo BUILD OK - now double-click simple_run.bat
pause
exit /b 0
:fail
echo.
echo BUILD FAILED - copy ALL the messages above and send them for help.
pause
exit /b 1
