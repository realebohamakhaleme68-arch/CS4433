cmake_minimum_required(VERSION 3.16)
project(ModeratedLMS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # used by clang-tidy / cppcheck / VS Code

option(LMS_USE_LIBSODIUM "Use libsodium (Argon2id) for password hashing when found" ON)
option(LMS_BUILD_TESTS "Build the automated tests" ON)

# ---- 1. Embed db/schema.sql into the program (the app creates lms.db on first run)
file(READ "${CMAKE_SOURCE_DIR}/db/schema.sql" LMS_SCHEMA_SQL)
configure_file("${CMAKE_SOURCE_DIR}/cmake/schema_sql.hpp.in"
               "${CMAKE_BINARY_DIR}/generated/schema_sql.hpp" @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/db/schema.sql")

# ---- 2. SQLite: use the amalgamation in third_party/sqlite if present, else the system library
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c")
    add_library(sqlite3_local STATIC third_party/sqlite/sqlite3.c)
    target_include_directories(sqlite3_local PUBLIC third_party/sqlite)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3_local PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    set(LMS_SQLITE_TARGET sqlite3_local)
    message(STATUS "SQLite: using third_party/sqlite amalgamation")
else()
    find_package(SQLite3 REQUIRED)
    if(TARGET SQLite3::SQLite3)
        set(LMS_SQLITE_TARGET SQLite3::SQLite3)
    else()
        set(LMS_SQLITE_TARGET SQLite::SQLite3)
    endif()
    message(STATUS "SQLite: using system library")
endif()

# ---- 3. Optional libsodium (Argon2id)
set(LMS_SODIUM_LIBS "")
if(LMS_USE_LIBSODIUM)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SODIUM QUIET IMPORTED_TARGET libsodium)
    endif()
    if(TARGET PkgConfig::SODIUM)
        set(LMS_SODIUM_LIBS PkgConfig::SODIUM)
    else()
        find_path(SODIUM_INCLUDE_DIR sodium.h)
        find_library(SODIUM_LIBRARY NAMES sodium libsodium)
        if(SODIUM_INCLUDE_DIR AND SODIUM_LIBRARY)
            add_library(sodium_found INTERFACE)
            target_include_directories(sodium_found INTERFACE ${SODIUM_INCLUDE_DIR})
            target_link_libraries(sodium_found INTERFACE ${SODIUM_LIBRARY})
            set(LMS_SODIUM_LIBS sodium_found)
        endif()
    endif()
endif()
if(LMS_SODIUM_LIBS)
    message(STATUS "Password hashing: libsodium (Argon2id)")
else()
    message(STATUS "Password hashing: built-in salted SHA-256 fallback (install libsodium for Argon2id)")
endif()

# ---- 4. Core library (everything except main.cpp) and the executable
add_library(lms_core STATIC
    src/util/TimeUtil.cpp
    src/util/PasswordHasher.cpp
    src/util/InputValidator.cpp
    src/domain/Models.cpp
    src/domain/UserFactory.cpp
    src/data/DatabaseConnection.cpp
    src/data/SQLiteRepositories.cpp
    src/service/Session.cpp
    src/service/Security.cpp
    src/service/AuditLogger.cpp
    src/service/Services.cpp
    src/ui/Console.cpp
    src/ui/Controllers.cpp
    src/ui/Application.cpp)
target_include_directories(lms_core PUBLIC src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(lms_core PUBLIC ${LMS_SQLITE_TARGET} ${LMS_SODIUM_LIBS})
if(LMS_SODIUM_LIBS)
    target_compile_definitions(lms_core PUBLIC LMS_USE_LIBSODIUM)
endif()
if(MSVC)
    target_compile_options(lms_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(lms_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_core)

# ---- 5. Tests (CTest)
if(LMS_BUILD_TESTS)
    enable_testing()
    add_executable(lms_tests tests/test_main.cpp)
    target_link_libraries(lms_tests PRIVATE lms_core)
    add_test(NAME lms_tests COMMAND lms_tests)
endif()

# ---- 6. MinGW (Windows): link the compiler runtime statically so lms.exe needs no extra DLLs
if(MINGW)
    target_link_options(lms PRIVATE -static-libgcc -static-libstdc++)
    if(LMS_BUILD_TESTS)
        target_link_options(lms_tests PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()
