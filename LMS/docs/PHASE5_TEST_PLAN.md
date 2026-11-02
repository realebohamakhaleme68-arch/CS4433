# Phase 5 — Verification and Validation Test Plan

**Project:** Moderated Learning Management System (C++17, SQLite)  
**Module:** CS4433 Software Engineering — Individual Assignment  
**Phase:** 5 — Verification and Validation (12 marks)

## 1. Purpose and scope

This plan verifies the LMS implementation against the approved requirements, use cases and design. It follows the assignment's V-Model levels: unit, integration, system and acceptance testing. The test scope includes the business rules for role-based access, assignment deadlines, extensions, resubmission, late penalties, marking, moderation, mark release, audit logging, password handling and persistence.

The design uses a layered architecture. Business rules are in services and the domain model; SQLite is accessed through repositories. Unit tests therefore exercise isolated domain/service rules, while integration tests exercise services with SQLite repositories and an in-memory SQLite database. Console usability and school-user acceptance remain a separate human evaluation activity.

## 2. Test basis and traceability

| Test level | Artefact verified | Main requirements / use cases | Test approach |
|---|---|---|---|
| Unit | Domain classes, strategies, validators, time utilities, user factory, security services | FR-1, FR-2, FR-8, FR-14; NFR-3, NFR-5; UC-01 / UC-02 rules | GoogleTest; equivalence partitioning and boundary value analysis |
| Integration | Application services + repository interfaces/SQLite implementations + audit logger | FR-3–FR-14; UC-01 Submit Assignment, UC-02 Mark Submission and Write Feedback, UC-03 Moderate Marks | GoogleTest integration scenario using SQLite `:memory:` database; transaction and repository checks |
| System | Complete use-case workflows across controller/service/repository/database boundaries | UC-01, UC-02, UC-03 including alternate and exception flows | Scenario-based tests; automated service-level workflow plus manual console scenarios |
| Acceptance | Stakeholder goals and measurable NFRs | Principal, Lecturer, HoD, Student Representative, IT Officer; NFR-1–NFR-7 | Acceptance checklist and stakeholder sign-off; performance/offline/accessibility checks on target school hardware |

## 3. Entry and exit criteria

| Level | Entry criteria | Exit criteria |
|---|---|---|
| Unit | C++17 source compiles; interfaces and business rules are baselined; test framework configured | All critical unit tests pass; boundary and invalid-input cases covered; no unresolved critical unit defect |
| Integration | Unit-tested components available; SQLite schema initialises; repositories compile | All service/repository integration scenarios pass; transaction rollback and audit consistency verified; no unresolved critical integration defect |
| System | Integrated build available; UC-01, UC-02 and UC-03 specifications approved | Main, alternative and exception flows executed or explicitly recorded as not run; all severity-1 defects fixed and retested |
| Acceptance | System tests pass; release candidate and user instructions available; representative users and target hardware available | Stakeholder acceptance criteria met and signed off, or deviations documented and accepted by supervisor/school |

## 4. Test environment

- OS: Ubuntu Linux test environment (local verification run).
- Compiler: GCC 14.2.0, C++17.
- Build system: CMake 3.31.6.
- Database: SQLite 3.46.1.
- Test framework: GoogleTest 1.14.0, with CTest discovery.
- Build configuration: Debug; `LMS_USE_LIBSODIUM=OFF` because libsodium development files were not installed in the execution environment. Therefore the local password tests exercised the project's salted SHA-256 fallback, not the optional libsodium/Argon2id branch.
- Integration database: SQLite in-memory database (`:memory:`); test submission files are created in a temporary test directory and removed after the workflow.

## 5. Test design techniques

### 5.1 Equivalence partitioning

| Input / condition | Valid equivalence class | Invalid equivalence class |
|---|---|---|
| Username | 3–32 permitted alphanumeric / dot / underscore / hyphen characters | Too short, too long, whitespace or SQL-like punctuation |
| Password | 8–128 characters containing letters, digits and punctuation | Missing a required character class, too short or too long |
| Mark | Numeric mark from 0 through maximum mark inclusive | Negative, above maximum, NaN or non-numeric text |
| Submission file | Existing, non-empty file with an allowed extension and size up to 10 MB | Missing, empty, disallowed extension or over 10 MB |
| User role | Admin, Lecturer, HoD or Student | Unknown role; Lecturer/HoD without required department |

### 5.2 Boundary value analysis — deadlines and late penalty

For a deadline `D`, `daysLate = 0` when submission time is at or before `D`; otherwise `ceil((submittedAt - D) / 24 hours)`. The test points are immediately before the deadline, exactly at the deadline, one second after, exactly 24 hours after, 24 hours plus one second, exactly 48 hours after, 48 hours plus one second, exactly 72 hours after, and 72 hours plus one second.

| Submission time | Expected days late | Expected penalty for max mark 100 |
|---|---:|---:|
| D − 1 second | 0 | 0 |
| D | 0 | 0 |
| D + 1 second | 1 | 10 |
| D + 24 hours | 1 | 10 |
| D + 24 hours + 1 second | 2 | 20 |
| D + 48 hours | 2 | 20 |
| D + 48 hours + 1 second | 3 | 30 |
| D + 72 hours | 3 | 30 |
| D + 72 hours + 1 second | 4 | 100 (final mark zero) |

The effective deadline is the assignment deadline or the latest individual extension, whichever applies. Penalty is applied when the lecturer marks the submission, not when the submission is first stored.

## 6. Automated test inventory

| ID | Test case | Expected result | Implemented in |
|---|---|---|---|
| UT-01 | Deadline boundary table | Correct ceiling calculation at each 24-hour boundary | `tests/test_time.cpp` |
| UT-02 | Invalid local date/time | `ValidationException` | `tests/test_time.cpp` |
| UT-03 | Standard late-penalty strategy | 10% of maximum mark per started day for days 1–3; full maximum deducted from day 4 | `tests/test_core.cpp` |
| UT-04 | No-penalty strategy | Always returns zero deduction | `tests/test_core.cpp` |
| UT-05 | Password and username partitions | Valid values accepted; invalid values rejected | `tests/test_core.cpp` |
| UT-06 | Mark and input parsing boundaries | 0/max accepted; negative, over-max, NaN and malformed values rejected | `tests/test_core.cpp` |
| UT-07 | User factory and permissions | Four correct role subclasses and permission sets; department required for Lecturer/HoD | `tests/test_core.cpp` |
| UT-08 | Salted password hash | Hash differs from plaintext, verifies correct password, rejects wrong password, uses a fresh salt | `tests/test_core.cpp` |
| IT-01 | Full LMS service workflow | Authentication, administration, enrolment, assignment, submission, extension, marking, moderation, release and audit operate together | `tests/test_integration.cpp` (wraps existing workflow) |
| IT-02 | Repository and transaction consistency | Failed transaction rolls back; audit and extension records are append-only | `tests/test_integration.cpp` (existing workflow) |
| IT-03 | 24-hour automatic release | No release before deadline; release at/after 24 hours; audit actor is SYSTEM | `tests/test_integration.cpp` (existing workflow) |

## 7. System and acceptance scenarios

The full automated workflow executes the service-level paths below. Console acceptance tests requiring a human operator are listed separately and are not represented as completed in the local automated run.

| ID | Use-case scenario | Expected result | Automated evidence / status |
|---|---|---|---|
| ST-01 | UC-01: enrolled student submits before deadline | One Submitted record, zero late days, version 1 | IT-01 — Passed |
| ST-02 | UC-01: student resubmits before deadline | Same current submission, version increments, prior file retained | IT-01 — Passed |
| ST-03 | UC-01: submission after deadline | Late submission accepted and daysLate recorded | IT-01 — Passed |
| ST-04 | UC-01: resubmit after deadline / late resubmission | Rejected; current submission remains unchanged | IT-01 — Passed |
| ST-05 | UC-01: invalid file or non-student submits | Validation/authorisation error; no new submission | IT-01 — Passed |
| ST-06 | UC-02: lecturer marks and writes feedback | Mark saved, penalty applied, review window starts, student cannot yet view mark | IT-01 — Passed |
| ST-07 | UC-02: mark out of range or re-mark without reason | Operation rejected | IT-01 — Passed |
| ST-08 | UC-03: HoD approves, adjusts or rejects | State and audit trail updated; rejected work can be re-marked | IT-01 — Passed |
| ST-09 | UC-03: release with outstanding unmoderated work | Release blocked and outstanding work remains | IT-01 — Passed |
| ST-10 | UC-03: release approved marks | Students see only released marks and feedback | IT-01 — Passed |
| ST-11 | No HoD action within 24 hours | System auto-releases; SYSTEM audit record created | IT-03 — Passed |
| AT-01 | Teacher completes common tasks using console menus | Representative teachers complete tasks without developer help | Not run — requires representative users |
| AT-02 | Login with 800 stored users completes within 2 seconds | Measured login time below 2 seconds | Not run — requires 800-user dataset and target hardware |
| AT-03 | Run LMS without internet access | Core workflows remain usable offline | Not run — requires isolated/offline target test |
| AT-04 | Keyboard-only and screen-reader review | Menus and messages are operable and understandable | Not run — requires accessibility review |
| AT-05 | Student confirms mark visibility and feedback rules | No mark visible before release; released mark and feedback visible afterwards | Service rule automated; stakeholder sign-off not run |

## 8. Execution commands

```bash
cmake -S . -B build-phase5-coverage \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLMS_USE_LIBSODIUM=OFF \
  -DLMS_ENABLE_COVERAGE=ON
cmake --build build-phase5-coverage --parallel 2
ctest --test-dir build-phase5-coverage --output-on-failure
python3 scripts/coverage_summary.py build-phase5-coverage
```

GoogleTest is vendored under `third_party/googletest`, so the test build does not need to download the framework.

## 9. Coverage measurement

Coverage is collected with GCC `--coverage` instrumentation. The reported business-logic scope is all compiled `.cpp` files in `src/service/` and `src/domain/`, matching the design's service and domain responsibilities. UI and data-access translation units are excluded from this business-logic percentage; utility translation units are also reported separately rather than silently included.

**Measured result:** 388 executable lines covered out of 505 measured lines = **76.83%** for service + domain implementation files. This exceeds the assignment's 70% target. The report is specific to the Linux/GCC test run and should be regenerated on the final submitted commit.

## 10. Limitations and sign-off

- The 800-record, under-two-second login requirement has not been performance-tested.
- Offline operation and accessibility require separate acceptance checks on the school's target environment.
- The local build used the salted SHA-256 fallback because libsodium was unavailable; the Argon2id branch must be tested in an environment with libsodium installed before claiming that branch has been verified.
- The GitHub Actions workflow has been added, but a hosted run cannot be claimed until the changes are pushed to the repository.
- Final acceptance requires the supervisor / representative users to review the acceptance checklist.
