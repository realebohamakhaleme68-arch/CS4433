# Coding standard (WBS 1.4.5)

* **Language:** C++17 only (`CMAKE_CXX_EXTENSIONS OFF`). No compiler-specific code outside `#ifdef _WIN32` blocks.
* **Layers:** `util` and `domain` depend on nothing. `data` depends on `domain`. `service` depends on `data` interfaces
  and `domain`. `ui` depends on `service`. **A layer never includes a header from a layer above it.**
* **SQL** appears only in `src/data/SQLiteRepositories.cpp` and `db/schema.sql`. Every value is a bound parameter
  (`Statement::bind`); never build SQL by joining user text (NFR-4).
* **Naming:** classes `PascalCase`; methods and variables `camelCase`; private members end with `_`;
  constants `kPascalCase`; files are named after the main class; interfaces start with `I`.
* **Errors:** throw `ValidationException`, `AuthorizationException` or `DatabaseException` (all derive from
  `AppException`). Controllers catch `AppException`; nothing throws to `main()` for expected problems (NFR-5).
* **Services:** every public method takes `const Session&` first and starts with `authz_.require(...)`.
* **Transactions:** a change plus its audit entry run inside one `Transaction` (RAII).
* **Memory:** no raw `new`/`delete`; use `std::unique_ptr`, `std::shared_ptr` or values.
* **Time:** store UTC ISO-8601 text only; convert with `timeutil` at the UI edge.
* **Warnings:** the build uses `-Wall -Wextra -Wpedantic -Wshadow` (`/W4` on MSVC) and must be warning-free.
* **Format:** `.clang-format` (Google-based, 4 spaces, 120 columns).
* **Comments:** explain *why* or cite the rule (FR/NFR/RE decision); do not repeat the code.
