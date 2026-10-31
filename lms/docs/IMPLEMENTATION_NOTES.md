# Phase 4 – Implementation notes

## 1. Requirement → code traceability

| Req. | Where it is implemented | Tested in `tests/test_main.cpp` |
|---|---|---|
| FR-1 login | `AuthenticationService::authenticate`, `Console`, `Application` | wrong password, unknown user, SYSTEM refused |
| FR-2 RBAC | `User::permissions()` (each subclass), `AuthorizationService` (role + department/owner/lecturer scope) | student/lecturer/HoD of another dept refused |
| FR-3 user accounts | `AdministrationService` (`createUser`, `updateUser`, `resetPassword`, `setActive`), `UserFactory` | create, duplicate, weak password, deactivate |
| FR-4 departments, courses, enrolment | `AdministrationService` | duplicate department / enrolment |
| FR-5 assignments | `AssignmentService::createAssignment` | past deadline refused |
| FR-6 view courses/assignments | `SubmissionService::listMyCourses/listMyAssignments` | manual |
| FR-7 submit / resubmit before deadline | `SubmissionService::submit`, `Submission::canResubmit` | version 2 before deadline, refused after |
| FR-8 late penalty | `ILatePenaltyStrategy`, `StandardDeductionStrategy`, `MarkingService` | boundary table 0/1/2/3/4 days |
| FR-9 extension | `AssignmentService::grantExtension`, `findEffectiveDeadline`, append-only trigger | extension makes work on time |
| FR-10 marking + feedback | `MarkingService::markSubmission` | raw/penalty/final, re-mark needs reason |
| FR-11 moderation | `ModerationService::approve/adjust/reject` | approve, adjust, reject, other dept refused |
| FR-12 release | `ModerationService::release`, `processExpiredReviews` | release, 24 h auto-release |
| FR-13 student views released marks | `SubmissionService::listMyReleasedMarks` | hidden before release, visible after |
| FR-14 audit | `AuditLogger` inside the same `Transaction`; `AuditLog` triggers | who/when/old/new/reason; tamper blocked |
| NFR-1 login < 2 s | unique index on `Users.username`; one query | measured ≈ 0.2 s incl. hashing |
| NFR-2 offline | no network code | – |
| NFR-3 salted hashes | `PasswordHasher` (Argon2id with libsodium, salted SHA-256 fallback) | hash ≠ password, random salt |
| NFR-4 parameterised SQL | `Statement::bind` only | injection-style username refused |
| NFR-5 no crashes | exception hierarchy, `guarded()` in controllers, transactions roll back | rollback test |
| NFR-6 simple menus | role-specific console menus | manual |
| NFR-7 portability | C++17, no OS-specific code except password echo (`#ifdef`) | – |

## 2. Where the implementation differs from the Phase 3 document (and why)

| Design | Implementation | Reason |
|---|---|---|
| `UserFactory` listed under Application Services | placed in `src/domain/` | the repository (data tier) must build `User` objects; keeping the factory in the domain avoids the data tier depending on the business tier |
| Permission list has no "view courses" | added `VIEW_COURSES` for Student, Lecturer, HoD, Admin | FR-6 needs a permission to check |
| Static `SessionManager` singleton | kept; `Session` is passed by `const&` to services | as designed |
| HoDNotifier (Observer) | keeps an in-memory counter and message; the HoD menu shows the count from the database | the notifier is lost when the program closes; the database is the source of truth |
| UC-02 alternative flow A3 "save feedback only" | **not implemented** | Phase 3 removed the Draft state; `Marks.raw_mark` is NOT NULL |
| "Return to lecturer" restarts the 24 h window | implemented by moving `review_due_at` 24 h forward | otherwise a rejected mark could be auto-released before it is re-marked |

## 3. Points to confirm with your supervisor

1. **Auto-release after 24 h** (RE conflict a) lets a mark reach students without HoD action. This relaxes the
   Principal's rule "no mark seen before the HoD has checked it". It is implemented as agreed in the RE document.
2. **Clock trust (risk R6):** the program uses the computer's clock offline. A user who changes the system clock can
   influence lateness. A fully offline system cannot prevent this; the audit and immutable `created_at` values make it
   detectable.
3. **Project plan wording:** the WBS says "SRS (12 FR + 6 NFR)" but the Requirements document has **14 FR + 7 NFR** –
   update the plan so the documents agree.

## 4. Manual test script (Phase 5 will formalise this)

| # | Steps | Expected |
|---|---|---|
| 1 | First start | asks for first administrator; weak password refused |
| 2 | Log in with a wrong password 3 times | "Invalid username or password." each time, no crash |
| 3 | Student submits before the deadline, then again | version 2, status Submitted |
| 4 | Student submits after the deadline | warning with percentage, status Late |
| 5 | Same student tries again | "Resubmission is only allowed before the deadline" |
| 6 | Lecturer marks late work (raw 80, max 100, 1 day late) | final 70, penalty 10 |
| 7 | Student opens "released marks" before release | nothing shown |
| 8 | HoD approves, adjusts one mark with a reason, releases | students see marks + feedback |
| 9 | HoD of another department tries the same course | access denied |
| 10 | Lecturer's audit trail | entries with user, time, old, new, reason |
| 11 | Mark, wait > 24 h (or set the PC clock forward), restart | mark released automatically, audited as SYSTEM |
| 12 | Type letters where a number is expected; empty names; wrong file path | clear message, program continues |
