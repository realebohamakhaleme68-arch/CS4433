// Self-contained tests (no framework needed). They cover the rules that matter
// most: late-penalty boundaries, moderation, auto-release, resubmission, RBAC
// and the audit log, all against an in-memory SQLite database.
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "data/DatabaseConnection.hpp"
#include "data/SQLiteRepositories.hpp"
#include "domain/UserFactory.hpp"
#include "service/Services.hpp"
#include "util/InputValidator.hpp"
#include "util/PasswordHasher.hpp"
#include "util/TimeUtil.hpp"

using namespace lms;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond)                                                                      \
    do {                                                                                 \
        ++g_checks;                                                                      \
        if (!(cond)) { ++g_fail; std::cout << "  FAIL line " << __LINE__ << ": " #cond "\n"; } \
    } while (0)
template <typename E, typename F>
static bool throws(F&& f) {
    try { f(); } catch (const E&) { return true; } catch (...) { return false; }
    return false;
}
static void setNow(const std::string& iso) { timeutil::setClockOverride([iso] { return iso; }); }

static void testTime() {
    std::cout << "time / lateness\n";
    const std::string dl = "2026-11-07T12:00:00Z";
    CHECK(timeutil::daysLate("2026-11-07T12:00:00Z", dl) == 0);   // exactly at the deadline
    CHECK(timeutil::daysLate("2026-11-07T12:00:01Z", dl) == 1);   // +1 s
    CHECK(timeutil::daysLate("2026-11-08T12:00:00Z", dl) == 1);   // +24 h
    CHECK(timeutil::daysLate("2026-11-08T12:00:01Z", dl) == 2);   // +24 h 1 s
    CHECK(timeutil::daysLate("2026-11-09T12:00:00Z", dl) == 2);   // +48 h
    CHECK(timeutil::daysLate("2026-11-09T12:00:01Z", dl) == 3);   // +48 h 1 s
    CHECK(timeutil::daysLate("2026-11-10T12:00:00Z", dl) == 3);   // +72 h
    CHECK(timeutil::daysLate("2026-11-10T12:00:01Z", dl) == 4);   // +72 h 1 s
    CHECK(timeutil::daysLate("2026-11-01T00:00:00Z", dl) == 0);
    CHECK(timeutil::localToUtc("2026-11-07 14:30") == "2026-11-07T12:30:00Z");
    CHECK(timeutil::utcToLocal("2026-11-07T12:30:00Z") == "2026-11-07 14:30");
    CHECK(timeutil::addSeconds("2026-02-28T23:00:00Z", 7200) == "2026-03-01T01:00:00Z");
    CHECK(throws<ValidationException>([] { timeutil::localToUtc("2026-13-45 99:99"); }));
}

static void testPenalty() {
    std::cout << "late-penalty strategy (max 100, raw 80)\n";
    StandardDeductionStrategy s;
    auto fin = [&](int days) { return std::max(0.0, 80.0 - s.calculatePenalty(days, 100)); };
    CHECK(fin(0) == 80);
    CHECK(fin(1) == 70);
    CHECK(fin(2) == 60);
    CHECK(fin(3) == 50);
    CHECK(fin(4) == 0);
    CHECK(fin(9) == 0);
    NoPenaltyStrategy n;
    CHECK(n.calculatePenalty(5, 100) == 0);
}

static void testSecurity() {
    std::cout << "passwords / validation\n";
    CHECK(InputValidator::isStrongPassword("Abcdef1!"));
    CHECK(!InputValidator::isStrongPassword("abcdefgh"));
    CHECK(!InputValidator::isStrongPassword("Ab1!"));
    CHECK(!InputValidator::isStrongPassword("12345678!"));
    std::string h = PasswordHasher::hashPassword("Secret#123");
    CHECK(h.find("Secret#123") == std::string::npos);                  // never plain text
    CHECK(PasswordHasher::verifyPassword(h, "Secret#123"));
    CHECK(!PasswordHasher::verifyPassword(h, "secret#123"));
    CHECK(h != PasswordHasher::hashPassword("Secret#123"));            // random salt
    CHECK(!PasswordHasher::verifyPassword("!", "anything"));           // SYSTEM account can never log in
    CHECK(InputValidator::isValidUsername("john.doe"));
    CHECK(!InputValidator::isValidUsername("a b"));
    CHECK(!InputValidator::isValidUsername("x'; DROP TABLE Users;--"));
}

static std::string makeFile(const std::string& dir, const std::string& name) {
    std::filesystem::create_directories(dir);
    std::string p = dir + "/" + name;
    std::ofstream(p) << "my work\n";
    return p;
}

static void testWorkflow() {
    std::cout << "end-to-end workflow (in-memory database)\n";
    namespace fs = std::filesystem;
    const std::string tmp = "lms_test_tmp";
    { std::error_code ec_; fs::remove_all(tmp, ec_); }

    auto& db = DatabaseConnection::getInstance();
    db.open(":memory:");
    db.initialiseSchema();

    SQLiteUserRepository users; SQLiteCourseRepository courses; SQLiteAssignmentRepository assignments;
    SQLiteSubmissionRepository subs; SQLiteMarkRepository marks; SQLiteAuditRepository audit;
    Repos repos{users, courses, assignments, subs, marks, audit};
    AuthorizationService authz; AuditLogger auditLogger(audit); StandardDeductionStrategy penalty;
    AdministrationService admin(repos, authz); AssignmentService asg(repos, authz);
    SubmissionService sub(repos, authz, tmp + "/store"); MarkingService marking(repos, authz, auditLogger, penalty);
    ModerationService mod(repos, authz, auditLogger);
    AuthenticationService auth(users);
    HoDNotifier notifier; marking.attach(&notifier);

    setNow("2026-11-01T08:00:00Z");
    CHECK(admin.needsFirstAdmin());
    admin.createFirstAdmin("root", "Admin#2026", "Root Admin");
    CHECK(!admin.needsFirstAdmin());
    CHECK(throws<AuthorizationException>([&] { admin.createFirstAdmin("r2", "Admin#2026", "X"); }));

    // login works, wrong password and the SYSTEM account are rejected
    CHECK(throws<ValidationException>([&] { auth.authenticate("root", "wrong"); }));
    CHECK(throws<ValidationException>([&] { auth.authenticate("SYSTEM", "!"); }));
    CHECK(throws<ValidationException>([&] { auth.authenticate("nobody", "x"); }));
    const Session& adminS = auth.authenticate("root", "Admin#2026");
    CHECK(adminS.user().role() == Role::Admin);

    int d1 = admin.createDepartment(adminS, "Computer Science");
    int d2 = admin.createDepartment(adminS, "Mathematics");
    CHECK(throws<ValidationException>([&] { admin.createDepartment(adminS, "Computer Science"); }));
    admin.createUser(adminS, Role::HoD, "hod1", "Hod#12345", "Dr Head", "E1", d1);
    CHECK(throws<ValidationException>([&] { admin.createUser(adminS, Role::HoD, "hod1b", "Hod#12345", "Second", "E9", d1); }));
    admin.createUser(adminS, Role::HoD, "hod2", "Hod#12345", "Dr Other", "E2", d2);
    int lec = admin.createUser(adminS, Role::Lecturer, "lec1", "Lec#12345", "Ms Lecturer", "E3", d1);
    int st1 = admin.createUser(adminS, Role::Student, "stu1", "Stu#12345", "Student One", "S1", std::nullopt);
    int st2 = admin.createUser(adminS, Role::Student, "stu2", "Stu#12345", "Student Two", "S2", std::nullopt);
    int st3 = admin.createUser(adminS, Role::Student, "stu3", "Stu#12345", "Student Three", "S3", std::nullopt);
    CHECK(throws<ValidationException>([&] { admin.createUser(adminS, Role::Student, "weak", "password", "W", "S4", std::nullopt); }));
    CHECK(throws<ValidationException>([&] { admin.createUser(adminS, Role::Student, "stu1", "Stu#12345", "Dup", "S5", std::nullopt); }));
    int course = admin.createCourse(adminS, "CS4433", "Software Engineering", d1);
    admin.assignLecturer(adminS, course, lec);
    for (int s : {st1, st2, st3}) admin.enrolStudent(adminS, s, course);
    CHECK(throws<ValidationException>([&] { admin.enrolStudent(adminS, st1, course); }));

    // keep the Session objects alive for the whole test
    auto sessionOf = [&](const std::string& u, const std::string& p) -> const Session& { return auth.authenticate(u, p); };
    auto mkSession = [&](int id) { return Session(std::shared_ptr<const User>(users.findById(id)), "t"); };
    Session sLec = mkSession(lec), sS1 = mkSession(st1), sS2 = mkSession(st2), sS3 = mkSession(st3);
    (void)sessionOf;
    Session sHod = Session(std::shared_ptr<const User>(users.findByUsername("hod1")), "t");
    Session sHod2 = Session(std::shared_ptr<const User>(users.findByUsername("hod2")), "t");

    // RBAC
    CHECK(throws<AuthorizationException>([&] { admin.createDepartment(sLec, "Hack"); }));
    CHECK(throws<AuthorizationException>([&] { asg.createAssignment(sS1, course, "x", "2026-12-01T10:00:00Z", 100); }));

    int a1 = asg.createAssignment(sLec, course, "Assignment 1", "2026-11-07T12:00:00Z", 100);
    CHECK(throws<ValidationException>([&] { asg.createAssignment(sLec, course, "Past", "2026-10-01T10:00:00Z", 100); }));

    // ---- on-time submission and resubmission before the deadline
    std::string f1 = makeFile(tmp, "work1.txt");
    Submission s1 = sub.submit(sS1, a1, f1);
    CHECK(s1.status == SubmissionStatus::Submitted && s1.daysLate == 0 && s1.version == 1);
    setNow("2026-11-02T09:00:00Z");
    Submission s1b = sub.submit(sS1, a1, f1);
    CHECK(s1b.version == 2 && s1b.id == s1.id && s1b.status == SubmissionStatus::Submitted);

    // ---- extension: student 3 gets until 2026-11-09T12:00:00Z, so 11-08 is not late
    int ext = asg.grantExtension(sLec, a1, st3, "2026-11-09T12:00:00Z", "Medical certificate");
    CHECK(ext > 0);
    CHECK(throws<ValidationException>([&] { asg.grantExtension(sLec, a1, st3, "2026-11-08T12:00:00Z", "earlier"); }));
    CHECK(assignments.findEffectiveDeadline(a1, st3) == "2026-11-09T12:00:00Z");
    CHECK(assignments.findEffectiveDeadline(a1, st1) == "2026-11-07T12:00:00Z");

    // ---- late submission (1 day) and closed resubmission
    setNow("2026-11-07T12:00:01Z");
    CHECK(throws<ValidationException>([&] { sub.submit(sS1, a1, f1); }));   // resubmission after deadline
    Submission s2 = sub.submit(sS2, a1, f1);
    CHECK(s2.status == SubmissionStatus::Late && s2.daysLate == 1);
    CHECK(throws<ValidationException>([&] { sub.submit(sS2, a1, f1); }));   // late work cannot be resubmitted
    Submission s3 = sub.submit(sS3, a1, f1);                                  // extension: not late
    CHECK(s3.status == SubmissionStatus::Submitted && s3.daysLate == 0);
    CHECK(throws<ValidationException>([&] { sub.submit(sS1, a1, tmp + "/missing.txt"); }));
    CHECK(throws<AuthorizationException>([&] { sub.submit(sLec, a1, f1); }));

    // ---- marking applies the penalty; students see nothing yet
    setNow("2026-11-08T08:00:00Z");
    Mark m1 = marking.markSubmission(sLec, s1.id, 80, "Good", "");
    Mark m2 = marking.markSubmission(sLec, s2.id, 80, "OK but late", "");
    Mark m3 = marking.markSubmission(sLec, s3.id, 60, "Fine", "");
    CHECK(m1.finalMark == 80 && m1.penaltyPoints == 0);
    CHECK(m2.finalMark == 70 && m2.penaltyPoints == 10);
    CHECK(m3.finalMark == 60);
    CHECK(m1.reviewDueAt == "2026-11-09T08:00:00Z");                  // marked_at + 24 h
    CHECK(throws<ValidationException>([&] { marking.markSubmission(sLec, s1.id, 101, "x", ""); }));
    CHECK(throws<ValidationException>([&] { marking.markSubmission(sLec, s1.id, 85, "re-mark", ""); }));  // reason needed
    CHECK(throws<AuthorizationException>([&] { marking.markSubmission(sS1, s1.id, 50, "x", ""); }));
    CHECK(notifier.pendingFor(d1) == 3);
    CHECK(sub.listMyReleasedMarks(sS1).empty());
    Mark m1b = marking.markSubmission(sLec, s1.id, 85, "re-marked", "Added bonus question");
    CHECK(m1b.finalMark == 85);

    // ---- moderation: department scope, approve / adjust / reject / release
    CHECK(mod.countAwaiting(sHod) == 3);
    CHECK(throws<AuthorizationException>([&] { mod.listForReview(sHod2, course); }));
    CHECK(throws<AuthorizationException>([&] { mod.approve(sHod2, s1.id); }));
    CHECK(throws<AuthorizationException>([&] { mod.approve(sLec, s1.id); }));
    mod.approve(sHod, s1.id);
    mod.adjust(sHod, s2.id, 75, "Rubric moderation");
    CHECK(throws<ValidationException>([&] { mod.adjust(sHod, s3.id, 60, ""); }));
    mod.reject(sHod, s3.id, "Please re-check question 3");
    CHECK(throws<ValidationException>([&] { mod.release(sHod, course); }));          // s3 still Marked
    CHECK(throws<AuthorizationException>([&] { mod.release(sHod2, course); }));
    marking.markSubmission(sLec, s3.id, 65, "Re-checked", "HoD comment");
    mod.approve(sHod, s3.id);
    CHECK(throws<AuthorizationException>([&] { marking.markSubmission(sLec, s1.id, 10, "x", "y"); }) == false);  // not an auth error ...
    CHECK(throws<ValidationException>([&] { marking.markSubmission(sLec, s1.id, 10, "x", "y"); }));            // ... a state error
    CHECK(mod.release(sHod, course) == 3);
    auto seen = sub.listMyReleasedMarks(sS2);
    CHECK(seen.size() == 1 && seen[0].mark.finalMark == 75);
    CHECK(sub.listMyReleasedMarks(sS1)[0].mark.finalMark == 85);

    // ---- audit trail (FR-14): who, when, old value, new value, reason
    auto trail = audit.findByMark(m1.id);
    CHECK(trail.size() == 4);   // CREATED, CHANGED, MODERATED, RELEASED
    CHECK(trail[0].actionType == "MARK_CREATED" && trail[1].actionType == "MARK_CHANGED");
    CHECK(trail[1].oldValue == std::optional<std::string>("80.00") && trail[1].newValue == std::optional<std::string>("85.00"));
    CHECK(trail[1].reason == "Added bonus question" && !trail[1].changedAt.empty());
    CHECK(trail[3].actionType == "MARK_RELEASED");
    auto trail2 = audit.findByMark(m2.id);
    CHECK(trail2[1].oldValue == std::optional<std::string>("70.00") && trail2[1].newValue == std::optional<std::string>("75.00"));

    // audit log and extensions are append-only (database triggers)
    CHECK(throws<ValidationException>([&] { Statement s("UPDATE AuditLog SET reason = 'tampered'"); s.run(); }) ||
          throws<DatabaseException>([&] { Statement s("UPDATE AuditLog SET reason = 'tampered'"); s.run(); }));
    CHECK(throws<DatabaseException>([&] { Statement s("DELETE FROM Extensions"); s.run(); }) ||
          throws<ValidationException>([&] { Statement s("DELETE FROM Extensions"); s.run(); }));

    // ---- automatic release 24 h after marking when the HoD does nothing
    int a2 = asg.createAssignment(sLec, course, "Assignment 2", "2026-11-12T12:00:00Z", 50);
    setNow("2026-11-16T12:00:00Z");                                         // 4 days late -> zero
    Submission x = sub.submit(sS1, a2, f1);
    CHECK(x.daysLate == 4);
    Mark mx = marking.markSubmission(sLec, x.id, 40, "late", "");
    CHECK(mx.finalMark == 0 && mx.penaltyPoints == 50);
    CHECK(mod.processExpiredReviews() == 0);
    setNow("2026-11-17T11:59:59Z");
    CHECK(mod.processExpiredReviews() == 0);
    setNow("2026-11-17T12:00:00Z");
    CHECK(mod.processExpiredReviews() == 1);
    CHECK(mod.processExpiredReviews() == 0);
    auto tx = audit.findByMark(mx.id);
    CHECK(tx.back().actionType == "MARK_AUTO_RELEASED" && tx.back().userId == kSystemUserId);
    CHECK(sub.listMyReleasedMarks(sS1).size() == 2);

    // ---- account rules
    admin.setActive(adminS, st2, false);
    CHECK(throws<ValidationException>([&] { auth.authenticate("stu2", "Stu#12345"); }));
    CHECK(throws<ValidationException>([&] { admin.setActive(adminS, adminS.user().getUserId(), false); }));

    // ---- transaction rollback leaves no partial data
    int before = static_cast<int>(users.listAll().size());
    CHECK(throws<ValidationException>([&] {
        Transaction t;
        Statement a("INSERT INTO Departments(name) VALUES ('Temp')"); a.run();
        Statement b("INSERT INTO Departments(name) VALUES ('Mathematics')"); b.run();   // duplicate -> throws
        t.commit();
    }));
    CHECK(courses.listDepartments().size() == 2);
    CHECK(static_cast<int>(users.listAll().size()) == before);

    timeutil::clearClockOverride();
    db.close();
    { std::error_code ec_; fs::remove_all(tmp, ec_); }
}

int main() {
    testTime();
    testPenalty();
    testSecurity();
    testWorkflow();
    std::cout << "\n" << (g_checks - g_fail) << "/" << g_checks << " checks passed\n";
    return g_fail == 0 ? 0 : 1;
}
