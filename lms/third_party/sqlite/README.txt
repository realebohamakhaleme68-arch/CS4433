OPTIONAL - build without installing SQLite
------------------------------------------
1. Go to https://www.sqlite.org/download.html
2. Under "Source Code" download the "sqlite-amalgamation-XXXXXXX.zip".
3. Copy sqlite3.c and sqlite3.h (and sqlite3ext.h) into THIS folder.
4. Re-run CMake. It detects sqlite3.c here and compiles it into the program,
   so no system SQLite is needed (easiest route on Windows).
