# Phase 5 — Verification and Validation Test Report

**Project:** Moderated Learning Management System (C++17 + SQLite)  
**Report date:** 3 November 2026  
**Test build:** `build-phase5-coverage` (Debug, GCC 14.2.0, CMake 3.31.6)  
**Test framework:** GoogleTest 1.14.0 + CTest

## 1. Executive result

The LMS source archive was extracted, configured and rebuilt in a Linux environment. The original automated test arrangement had GoogleTest configured but only a small GoogleTest source file for time utility checks; the larger end-to-end workflow was a separate self-contained `CHECK`-based executable. The test suite was extended so the existing workflow is also run through GoogleTest and additional unit tests cover penalty strategies, input validation, user creation and password hashing.

**Result of the executed coverage build:** 19/19 CTest tests passed (100%); 0 test failures; total test execution time 23.01 seconds. GCC line coverage for the service and domain implementation files was 76.83% (388/505 lines), above the brief's 70% target.

## 2. Environment and procedure

- Ubuntu Linux execution environment.
- GCC 14.2.0, C++17.
- CMake 3.31.6.
- SQLite 3.46.1.
- GoogleTest 1.14.0, vendored under `third_party/googletest`.
- libsodium was not installed; CMake was explicitly configured with `LMS_USE_LIBSODIUM=OFF`, so the local hash test exercised the project's salted SHA-256 fallback.
- Coverage was enabled with `LMS_ENABLE_COVERAGE=ON` and GCC `--coverage` flags.

Commands:

```bash
cmake -S . -B build-phase5-coverage -DCMAKE_BUILD_TYPE=Debug \
  -DLMS_USE_LIBSODIUM=OFF -DLMS_ENABLE_COVERAGE=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-phase5-coverage --parallel 2
ctest --test-dir build-phase5-coverage --output-on-failure
python3 scripts/coverage_summary.py build-phase5-coverage
```

## 3. Test execution summary

| Result | Count |
|---|---:|
| Tests discovered by CTest | 19 |
| Passed | 19 |
| Failed | 0 |
| Skipped | 0 |
| Pass rate | 100% |
| Total execution time | 23.01 seconds |

The 19 tests consist of 10 newly grouped GoogleTest unit/integration cases plus 7 parameterised deadline-boundary instances and one invalid-date test, and the existing legacy workflow executable. CTest counts each parameterised instance separately.

## 4. Test evidence by area

| Area | Evidence | Result |
|---|---|---|
| Deadline calculation | At, before and after deadline; 24/48/72-hour boundaries and one-second overrun | Passed |
| Penalty strategy | 0, 1, 2, 3 and 4+ days; different maximum marks; no-penalty strategy | Passed |
| Input validation | Password and username equivalence partitions; mark range; NaN; integer/double parsing; whitespace | Passed |
| Role model | Admin, Lecturer, HoD and Student factory construction and permissions; missing department rejection | Passed |
| Password hashing | Salted output, correct password, incorrect password, random salt and SYSTEM sentinel | Passed (fallback implementation only) |
| Assignment workflow | Creation, student submission, resubmission, extension, late submission, late-work resubmission rejection | Passed |
| Marking/moderation | Raw mark, deduction, feedback, re-mark reason, HoD approval/adjustment/rejection, department access | Passed |
| Release and audit | Student visibility after release, audit old/new/reason fields, automatic 24-hour release as SYSTEM | Passed |
| Persistence/transactions | In-memory SQLite repositories, append-only audit/extensions, rollback after duplicate insert | Passed |

## 5. Coverage report

Coverage was calculated for `.cpp` implementation files in `src/service/` and `src/domain/` only. These are the service and domain parts of the business-logic tier described by the system design document.

| Source file | Lines executed | Line coverage |
|---|---:|---:|
| `src/domain/Models.cpp` | 28 / 45 | 62.22% |
| `src/domain/UserFactory.cpp` | 12 / 13 | 92.31% |
| `src/service/AuditLogger.cpp` | 11 / 11 | 100.00% |
| `src/service/Security.cpp` | 23 / 29 | 79.31% |
| `src/service/Services.cpp` | 304 / 394 | 77.16% |
| `src/service/Session.cpp` | 10 / 13 | 76.92% |
| **Total business-logic scope** | **388 / 505** | **76.83%** |

Additional utility coverage (not included in the business-logic total): `InputValidator.cpp` 47/48 (97.92%); `PasswordHasher.cpp` 81/85 (95.29%); `TimeUtil.cpp` 60/62 (96.77%).

The 70% target is met for the stated service + domain scope. The relatively lower `Models.cpp` coverage is an improvement opportunity and should be considered in the next test iteration.

## 6. Defects found and retest

One high-severity presentation-layer defect was identified during compiler-warning review. In `AdminController::run`, menu option 1 collected user role, department, username, name and reference number but did not request a password or call `AdministrationService::createUser`; it then fell through into menu option 2. This meant the console's Create a user action could not create a user. The compiler also emitted an implicit-fallthrough warning at the switch case.

**Correction applied:** prompt for the password, call `createUser`, display the created user identifier, and add an explicit `break`. The project was rebuilt and the complete 19-test suite was rerun successfully after the correction. The existing service integration test covers user creation through the service, but direct interactive console verification of this exact menu path remains a manual acceptance check.

## 7. Outstanding acceptance checks

These checks have not been executed and must not be described as passed:

1. Login response time with 800 stored user records, measured against the 2-second NFR.
2. Offline operation on the target school computer with network access disabled.
3. Keyboard-only and screen-reader usability review.
4. Teacher/student representative acceptance and supervisor sign-off.
5. Hosted GitHub Actions execution after pushing the new workflow.
6. Optional libsodium/Argon2id password branch test on a machine with libsodium development files installed.

## 8. Conclusion

The local automated verification build and tests succeeded. The service + domain business-logic coverage result is 76.83%, meeting the assignment's 70% target. The automated integration workflow exercises the central LMS assessment lifecycle and persistence safeguards. One console user-creation defect was corrected and the project rebuilt and retested. Phase 5's automated testing evidence is prepared; final user acceptance and target-hardware non-functional checks remain outstanding and should be completed before claiming full system acceptance.
