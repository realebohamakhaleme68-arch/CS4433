# Phase 5 — Defect Log

| ID | Severity | Component | Defect / observed behaviour | Cause | Fix applied | Retest result | Status |
|---|---|---|---|---|---|---|---|
| DEF-05-01 | High | `AdminController::run`, `src/ui/Controllers.cpp` | Selecting “Create a user” collected partial details but never called the user-creation service; control fell through to “List users”. The compiler reported implicit fallthrough. | Incomplete `case 1` implementation: missing password prompt, service call and `break`. | Added password prompt; invoked `AdministrationService::createUser`; displayed created user ID; added `break`. | Rebuilt successfully; CTest rerun: 19/19 passed. Service-level user creation is covered by the end-to-end integration scenario. Direct console interaction still needs manual acceptance verification. | Fixed; manual UI acceptance pending |

## Open acceptance / verification items (not software defects)

| ID | Severity | Item | Required action | Status |
|---|---|---|---|---|
| OBS-05-01 | Medium | NFR-1 performance evidence absent | Populate 800 users and measure login time on target school hardware; confirm less than 2 seconds | Open — not run |
| OBS-05-02 | Medium | Offline and accessibility acceptance evidence absent | Run with network disabled; conduct keyboard-only and screen-reader checks | Open — not run |
| OBS-05-03 | Low | Optional Argon2id branch not exercised locally | Build and test with libsodium enabled | Open — dependency unavailable in local test environment |
| OBS-05-04 | Low | Hosted CI result not available yet | Push changes and capture GitHub Actions run | Open — workflow committed locally only |
