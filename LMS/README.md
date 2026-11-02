# Moderated Learning Management System (C++17 + SQLite)

Phase 4 – Implementation of the design in *System Design Document* (Phase 3).
Console application, fully offline, one executable + one local database file (`lms.db`).

## 0a. EASIEST on Windows - no CMake at all

1. Install MSYS2 and the packages (section 1, Windows).
2. Unzip this project to `C:\\lms`.
3. Double-click **simple_build.bat** -> must end with `BUILD OK` (creates `lms.exe` in this folder).
4. Double-click **simple_run.bat** -> the program starts. (**simple_test.bat** runs the tests.)

## 0. Quick start on Windows (3 double-clicks)

1. Install the tools from section 1 (MSYS2 route).
2. Double-click **build.bat** - it configures and builds the project. It must end with `BUILD OK`.
3. Double-click **run.bat** - it opens a console window that stays open and starts the program.
   You should see `LMS starting...` and then the first-time setup.
   (**test.bat** runs the automated tests.)

If run.bat shows nothing at all, the program is not starting: open a command prompt in this folder and run
`build\lms.exe --version`, then send the exact output.

## 1. What you need installed

Pick **one** route. After installing, restart VS Code so it sees the tools.

### Windows (recommended: MSYS2 – free)
1. Install MSYS2 from https://www.msys2.org and open the **"MSYS2 UCRT64"** terminal.
2. Run:
   ```
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
                      mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-libsodium \
                      mingw-w64-ucrt-x86_64-pkgconf
   ```
3. Add `C:\msys64\ucrt64\bin` to your Windows **PATH** (Start → "Edit environment variables").
4. In VS Code install the extensions it recommends (C/C++, CMake Tools, SQLite viewer).

*No-install alternative for SQLite:* put `sqlite3.c` and `sqlite3.h` in `third_party/sqlite/` (see the README
there). Without libsodium the program automatically uses a built-in salted SHA-256 hasher.

### Ubuntu / Debian
```
sudo apt install build-essential cmake ninja-build gdb libsqlite3-dev libsodium-dev pkg-config
```
### macOS
```
brew install cmake ninja sqlite libsodium pkg-config
```

## 2. Open, build, run (VS Code)

1. **File → Open Folder…** and choose this `lms` folder.
2. When CMake Tools asks to pick a kit, choose your compiler (e.g. *GCC … ucrt64*).
3. Build: press **Ctrl+Shift+B** (task *Build*) or click **Build** in the status bar.
4. Run: press **F5** (choose *Run LMS* – gdb entry for MinGW/Linux, the `cppvsdbg` entry for Visual Studio).
   The program must start in the project folder; the launch file already sets `cwd` for you.
5. Or from a terminal in the project folder:
   ```
   cmake -S . -B build
   cmake --build build
   ./build/lms            (Windows: build\lms.exe)
   ```
6. Tests: task **Run tests**, or `ctest --test-dir build --output-on-failure`.

Look at the CMake output: it says whether **libsodium (Argon2id)** or the **SHA-256 fallback** is in use,
and the program prints the same line at start-up.

## 3. The database

You do **not** have to create `lms.db` by hand. The schema from your Phase 3 document is stored in
`db/schema.sql`, built into the program, and applied automatically at every start (it is idempotent:
`CREATE ... IF NOT EXISTS`, `INSERT OR IGNORE`). It also creates the reserved `SYSTEM` user (id 1) that is used
for automatic releases.

* To inspect it in VS Code: install **SQLite Viewer / SQLite** extension → right-click `lms.db` → *Open Database*.
* To create an empty file yourself (optional, needs the `sqlite3` command): task *Create empty database…* or
  `sqlite3 lms.db ".read db/schema.sql"`.
* Another database file: `./build/lms path/to/other.db`.
* A copy `lms.db.bak` is made at every start-up (risk R5 in the project plan).
* Uploaded work is copied to `submissions/a<assignment>/s<student>/v<version>_<file>`; old versions stay there.

**Never edit `db/schema.sql` after you have real data** without a migration – change it, delete `lms.db` and
start again only while you are testing.

## 4. First run – quick demo (≈ 5 minutes)

1. The first start asks you to create the **first administrator** (there is no default password).
   Password rule: ≥ 8 characters with letters, digits **and** symbols, e.g. `Admin#2026`.
2. Log in as the administrator and create, in this order:
   department → users (a HoD and a Lecturer in that department, two Students) → course →
   *Assign a lecturer* → *Enrol a student* (twice).
3. Log out, log in as the **Lecturer**: *Create an assignment* (deadline a few minutes from now).
4. Log in as a **Student**: *Submit an assignment* – use any `.txt` or `.pdf` file. After the deadline, submit as the
   second student to see the late warning (10 % per day, zero after 3 days).
5. As the Lecturer: *Mark submissions* – raw mark, feedback. The penalty is applied automatically.
6. As the **HoD**: review, approve / adjust / return, then *Release*. Students now see marks and feedback.
7. If the HoD does nothing for 24 h the mark is released automatically (audited as `SYSTEM`) the next time anybody
   starts the program or logs in.

## 5. Project layout

```
CMakeLists.txt            build (C++17, warnings on, optional libsodium, schema embedded)
db/schema.sql             SQLite schema from the Phase 3 document (tables, keys, constraints, triggers)
src/util                  TimeUtil, PasswordHasher, InputValidator, Exceptions
src/domain                Models, User hierarchy (abstract User + 4 subclasses), UserFactory      (Tier 2)
src/data                  DatabaseConnection (singleton), Statement, I*Repository, SQLite*Repository (Tier 3)
src/service               Session, Authentication/Authorization, AuditLogger, Strategy, Observer, services (Tier 2)
src/ui                    Console, thin controllers per role, Application wiring                    (Tier 1)
tests/test_main.cpp       102 automated checks (penalty boundaries, workflow, RBAC, audit, auto-release)
docs/                     coding standard, implementation notes (traceability, deviations)
.vscode/                  tasks, launch and settings for VS Code
```

## 6. Changing rules (design patterns at work)

* **New late-penalty rule:** add a class implementing `ILatePenaltyStrategy` and pass it to `MarkingService` in
  `src/ui/Application.cpp` – nothing else changes.
* **New role:** add a `User` subclass and one `case` in `UserFactory`.
* **Unit tests with fakes:** services only know the `I*Repository` interfaces.

## 7. Troubleshooting

| Symptom | Fix |
|---|---|
| `Could NOT find SQLite3` | install the SQLite dev package or use `third_party/sqlite/` (see section 1) |
| `cmake` not found | add CMake / MSYS2 `ucrt64\bin` to PATH, restart VS Code |
| Program cannot find `lms.db` | run from the project folder (F5 does this) or pass a path as argument |
| Password typed but nothing shows | normal – the password is hidden while typing |
| Login says "Invalid username or password" for an old account | the account may be deactivated by an administrator |

## 6. Phase 5 — Verification and Validation

The Phase 5 test plan, execution report and defect log are in `docs/`:

- `docs/PHASE5_TEST_PLAN.md` — V-Model mapping, entry/exit criteria, equivalence classes, deadline boundary values and system/acceptance scenarios.
- `docs/PHASE5_TEST_REPORT.md` — actual GoogleTest/CTest execution and GCC coverage results.
- `docs/PHASE5_DEFECT_LOG.md` — defect history and outstanding acceptance checks.

The GoogleTest suite includes unit tests in `tests/test_time.cpp` and `tests/test_core.cpp`, plus an integration workflow in `tests/test_integration.cpp`. The original standalone integration harness is retained as `lms_tests`.

To run the suite:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DLMS_USE_LIBSODIUM=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

To collect GCC line coverage for the business-logic library:

```bash
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug \
  -DLMS_USE_LIBSODIUM=OFF -DLMS_ENABLE_COVERAGE=ON
cmake --build build-coverage --parallel 2
ctest --test-dir build-coverage --output-on-failure
python3 scripts/coverage_summary.py build-coverage
```

The current measured service + domain line coverage is recorded in `docs/PHASE5_TEST_REPORT.md` and `test-results/coverage-summary.txt`. Regenerate it after changing source or tests. The GitHub Actions workflow in `.github/workflows/phase5-tests.yml` builds the project and runs CTest on each push and pull request.
