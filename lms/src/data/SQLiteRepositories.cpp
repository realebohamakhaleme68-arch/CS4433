#include "data/SQLiteRepositories.hpp"

#include "data/DatabaseConnection.hpp"
#include "domain/UserFactory.hpp"
#include "util/TimeUtil.hpp"

namespace lms {
namespace {

int lastId() { return static_cast<int>(DatabaseConnection::getInstance().lastInsertId()); }

// ---- row mappers (column order is fixed by the *_COLS constants) ----
const char* USER_COLS = "user_id, username, full_name, role, reference_no, department_id, is_active, created_at";
std::unique_ptr<User> mapUser(const Statement& s) {
    return UserFactory::create(roleFromString(s.colText(3)), s.colInt(0), s.colText(1), s.colText(2), s.colText(4),
                               s.colOptInt(5), s.colInt(6) != 0, s.colText(7));
}

const char* COURSE_COLS = "course_id, course_code, title, department_id, lecturer_id, created_at";
Course mapCourse(const Statement& s) {
    Course c;
    c.id = s.colInt(0); c.code = s.colText(1); c.title = s.colText(2);
    c.departmentId = s.colInt(3); c.lecturerId = s.colOptInt(4); c.createdAt = s.colText(5);
    return c;
}

const char* ASSIGN_COLS = "assignment_id, course_id, title, deadline, max_mark, created_by, created_at";
Assignment mapAssignment(const Statement& s, int o = 0) {
    Assignment a;
    a.id = s.colInt(o); a.courseId = s.colInt(o + 1); a.title = s.colText(o + 2); a.deadline = s.colText(o + 3);
    a.maxMark = s.colDouble(o + 4); a.createdBy = s.colInt(o + 5); a.createdAt = s.colText(o + 6);
    return a;
}

const char* SUB_COLS =
    "submission_id, assignment_id, student_id, file_path, submitted_at, version, days_late, status, created_at, updated_at";
Submission mapSubmission(const Statement& s, int o = 0) {
    Submission x;
    x.id = s.colInt(o); x.assignmentId = s.colInt(o + 1); x.studentId = s.colInt(o + 2);
    x.filePath = s.colText(o + 3); x.submittedAt = s.colText(o + 4); x.version = s.colInt(o + 5);
    x.daysLate = s.colInt(o + 6); x.status = statusFromString(s.colText(o + 7));
    x.createdAt = s.colText(o + 8); x.updatedAt = s.colText(o + 9);
    return x;
}

const char* MARK_COLS =
    "mark_id, submission_id, raw_mark, penalty_points, final_mark, feedback, marked_by, marked_at, review_due_at, "
    "moderated_by, moderated_at, released_at";
Mark mapMark(const Statement& s, int o = 0) {
    Mark m;
    m.id = s.colInt(o); m.submissionId = s.colInt(o + 1); m.rawMark = s.colDouble(o + 2);
    m.penaltyPoints = s.colDouble(o + 3); m.finalMark = s.colDouble(o + 4); m.feedback = s.colText(o + 5);
    m.markedBy = s.colInt(o + 6); m.markedAt = s.colText(o + 7); m.reviewDueAt = s.colText(o + 8);
    m.moderatedBy = s.colOptInt(o + 9); m.moderatedAt = s.colOptText(o + 10); m.releasedAt = s.colOptText(o + 11);
    return m;
}
}  // namespace

// ======================= Users =======================
std::unique_ptr<User> SQLiteUserRepository::findById(int id) {
    Statement s(std::string("SELECT ") + USER_COLS + " FROM Users WHERE user_id = ?1");
    s.bind(1, id);
    return s.step() ? mapUser(s) : nullptr;
}
std::unique_ptr<User> SQLiteUserRepository::findByUsername(const std::string& username) {
    Statement s(std::string("SELECT ") + USER_COLS + " FROM Users WHERE username = ?1");
    s.bind(1, username);
    return s.step() ? mapUser(s) : nullptr;
}
std::optional<AuthRecord> SQLiteUserRepository::getAuthRecord(const std::string& username) {
    Statement s("SELECT user_id, password_hash, is_active FROM Users WHERE username = ?1");  // indexed (NFR-1)
    s.bind(1, username);
    if (!s.step()) return std::nullopt;
    return AuthRecord{s.colInt(0), s.colText(1), s.colInt(2) != 0};
}
int SQLiteUserRepository::save(const User& u, const std::string& hash) {
    Statement s("INSERT INTO Users(username, password_hash, full_name, role, reference_no, department_id, is_active) "
                "VALUES (?1,?2,?3,?4,?5,?6,?7)");
    std::optional<std::string> ref = u.referenceNo().empty() ? std::nullopt : std::optional<std::string>(u.referenceNo());
    s.bind(1, u.getUsername()).bind(2, hash).bind(3, u.getFullName()).bind(4, u.roleName()).bind(5, ref)
        .bind(6, u.departmentId()).bind(7, u.isActive() ? 1 : 0);
    s.run();
    return lastId();
}
void SQLiteUserRepository::update(const User& u) {
    Statement s("UPDATE Users SET full_name = ?1, reference_no = ?2, department_id = ?3 WHERE user_id = ?4");
    std::optional<std::string> ref = u.referenceNo().empty() ? std::nullopt : std::optional<std::string>(u.referenceNo());
    s.bind(1, u.getFullName()).bind(2, ref).bind(3, u.departmentId()).bind(4, u.getUserId());
    s.run();
}
void SQLiteUserRepository::updatePassword(int id, const std::string& hash) {
    Statement s("UPDATE Users SET password_hash = ?1 WHERE user_id = ?2");
    s.bind(1, hash).bind(2, id);
    s.run();
}
void SQLiteUserRepository::setActive(int id, bool active) {
    Statement s("UPDATE Users SET is_active = ?1 WHERE user_id = ?2");
    s.bind(1, active ? 1 : 0).bind(2, id);
    s.run();
}
std::vector<std::unique_ptr<User>> SQLiteUserRepository::listAll() {
    Statement s(std::string("SELECT ") + USER_COLS + " FROM Users WHERE user_id <> 1 ORDER BY role, username");
    std::vector<std::unique_ptr<User>> v;
    while (s.step()) v.push_back(mapUser(s));
    return v;
}
int SQLiteUserRepository::countActiveAdmins() {
    Statement s("SELECT COUNT(*) FROM Users WHERE role = 'Admin' AND is_active = 1");
    s.step();
    return s.colInt(0);
}

// ======================= Departments / Courses =======================
int SQLiteCourseRepository::createDepartment(const std::string& name) {
    Statement s("INSERT INTO Departments(name) VALUES (?1)");
    s.bind(1, name);
    s.run();
    return lastId();
}
std::optional<Department> SQLiteCourseRepository::findDepartment(int id) {
    Statement s("SELECT department_id, name, created_at FROM Departments WHERE department_id = ?1");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return Department{s.colInt(0), s.colText(1), s.colText(2)};
}
std::vector<Department> SQLiteCourseRepository::listDepartments() {
    Statement s("SELECT department_id, name, created_at FROM Departments ORDER BY name");
    std::vector<Department> v;
    while (s.step()) v.push_back(Department{s.colInt(0), s.colText(1), s.colText(2)});
    return v;
}
int SQLiteCourseRepository::createCourse(const Course& c) {
    Statement s("INSERT INTO Courses(course_code, title, department_id, lecturer_id) VALUES (?1,?2,?3,?4)");
    s.bind(1, c.code).bind(2, c.title).bind(3, c.departmentId).bind(4, c.lecturerId);
    s.run();
    return lastId();
}
std::optional<Course> SQLiteCourseRepository::findById(int id) {
    Statement s(std::string("SELECT ") + COURSE_COLS + " FROM Courses WHERE course_id = ?1");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return mapCourse(s);
}
static std::vector<Course> courses(Statement& s) {
    std::vector<Course> v;
    while (s.step()) v.push_back(mapCourse(s));
    return v;
}
std::vector<Course> SQLiteCourseRepository::listAll() {
    Statement s(std::string("SELECT ") + COURSE_COLS + " FROM Courses ORDER BY course_code");
    return courses(s);
}
std::vector<Course> SQLiteCourseRepository::findByDepartment(int d) {
    Statement s(std::string("SELECT ") + COURSE_COLS + " FROM Courses WHERE department_id = ?1 ORDER BY course_code");
    s.bind(1, d);
    return courses(s);
}
std::vector<Course> SQLiteCourseRepository::findByLecturer(int l) {
    Statement s(std::string("SELECT ") + COURSE_COLS + " FROM Courses WHERE lecturer_id = ?1 ORDER BY course_code");
    s.bind(1, l);
    return courses(s);
}
std::vector<Course> SQLiteCourseRepository::findByStudent(int st) {
    Statement s("SELECT c.course_id, c.course_code, c.title, c.department_id, c.lecturer_id, c.created_at "
                "FROM Courses c JOIN Enrolments e ON e.course_id = c.course_id WHERE e.student_id = ?1 "
                "ORDER BY c.course_code");
    s.bind(1, st);
    return courses(s);
}
void SQLiteCourseRepository::assignLecturer(int courseId, int lecturerId) {
    Statement s("UPDATE Courses SET lecturer_id = ?1 WHERE course_id = ?2");
    s.bind(1, lecturerId).bind(2, courseId);
    s.run();
}
void SQLiteCourseRepository::enrol(int studentId, int courseId) {
    Statement s("INSERT INTO Enrolments(student_id, course_id) VALUES (?1,?2)");
    s.bind(1, studentId).bind(2, courseId);
    s.run();
}
bool SQLiteCourseRepository::isEnrolled(int studentId, int courseId) {
    Statement s("SELECT 1 FROM Enrolments WHERE student_id = ?1 AND course_id = ?2");
    s.bind(1, studentId).bind(2, courseId);
    return s.step();
}
std::vector<std::unique_ptr<User>> SQLiteCourseRepository::listStudents(int courseId) {
    Statement s("SELECT u.user_id, u.username, u.full_name, u.role, u.reference_no, u.department_id, u.is_active, "
                "u.created_at FROM Users u JOIN Enrolments e ON e.student_id = u.user_id WHERE e.course_id = ?1 "
                "ORDER BY u.full_name");
    s.bind(1, courseId);
    std::vector<std::unique_ptr<User>> v;
    while (s.step()) v.push_back(mapUser(s));
    return v;
}

// ======================= Assignments =======================
std::optional<Assignment> SQLiteAssignmentRepository::findById(int id) {
    Statement s(std::string("SELECT ") + ASSIGN_COLS + " FROM Assignments WHERE assignment_id = ?1");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return mapAssignment(s);
}
int SQLiteAssignmentRepository::create(const Assignment& a) {
    Statement s("INSERT INTO Assignments(course_id, title, deadline, max_mark, created_by) VALUES (?1,?2,?3,?4,?5)");
    s.bind(1, a.courseId).bind(2, a.title).bind(3, a.deadline).bind(4, a.maxMark).bind(5, a.createdBy);
    s.run();
    return lastId();
}
std::vector<Assignment> SQLiteAssignmentRepository::findByCourse(int courseId) {
    Statement s(std::string("SELECT ") + ASSIGN_COLS + " FROM Assignments WHERE course_id = ?1 ORDER BY deadline");
    s.bind(1, courseId);
    std::vector<Assignment> v;
    while (s.step()) v.push_back(mapAssignment(s));
    return v;
}
std::vector<AssignmentInfo> SQLiteAssignmentRepository::findByStudent(int studentId) {
    Statement s("SELECT a.assignment_id, a.course_id, a.title, a.deadline, a.max_mark, a.created_by, a.created_at, "
                "c.course_code FROM Assignments a JOIN Courses c ON c.course_id = a.course_id "
                "JOIN Enrolments e ON e.course_id = c.course_id WHERE e.student_id = ?1 "
                "ORDER BY c.course_code, a.deadline");
    s.bind(1, studentId);
    std::vector<AssignmentInfo> v;
    while (s.step()) v.push_back(AssignmentInfo{mapAssignment(s), s.colText(7)});
    return v;
}
int SQLiteAssignmentRepository::saveExtension(const Extension& e) {
    Statement s("INSERT INTO Extensions(assignment_id, student_id, extended_deadline, reason, granted_by) "
                "VALUES (?1,?2,?3,?4,?5)");
    s.bind(1, e.assignmentId).bind(2, e.studentId).bind(3, e.extendedDeadline).bind(4, e.reason).bind(5, e.grantedBy);
    s.run();
    return lastId();
}
std::string SQLiteAssignmentRepository::findEffectiveDeadline(int assignmentId, int studentId) {
    Statement ext("SELECT extended_deadline FROM Extensions WHERE assignment_id = ?1 AND student_id = ?2 "
                  "ORDER BY created_at DESC, extension_id DESC LIMIT 1");
    ext.bind(1, assignmentId).bind(2, studentId);
    if (ext.step()) return ext.colText(0);
    Statement a("SELECT deadline FROM Assignments WHERE assignment_id = ?1");
    a.bind(1, assignmentId);
    if (!a.step()) throw ValidationException("Assignment not found.");
    return a.colText(0);
}

// ======================= Submissions =======================
std::optional<Submission> SQLiteSubmissionRepository::findById(int id) {
    Statement s(std::string("SELECT ") + SUB_COLS + " FROM Submissions WHERE submission_id = ?1");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return mapSubmission(s);
}
std::optional<Submission> SQLiteSubmissionRepository::findByAssignmentAndStudent(int a, int st) {
    Statement s(std::string("SELECT ") + SUB_COLS + " FROM Submissions WHERE assignment_id = ?1 AND student_id = ?2");
    s.bind(1, a).bind(2, st);
    if (!s.step()) return std::nullopt;
    return mapSubmission(s);
}
std::vector<SubmissionRow> SQLiteSubmissionRepository::findByAssignment(int assignmentId) {
    Statement s("SELECT s.submission_id, s.assignment_id, s.student_id, s.file_path, s.submitted_at, s.version, "
                "s.days_late, s.status, s.created_at, s.updated_at, u.full_name, "
                "m.mark_id, m.submission_id, m.raw_mark, m.penalty_points, m.final_mark, m.feedback, m.marked_by, "
                "m.marked_at, m.review_due_at, m.moderated_by, m.moderated_at, m.released_at "
                "FROM Submissions s JOIN Users u ON u.user_id = s.student_id "
                "LEFT JOIN Marks m ON m.submission_id = s.submission_id "
                "WHERE s.assignment_id = ?1 ORDER BY u.full_name");
    s.bind(1, assignmentId);
    std::vector<SubmissionRow> v;
    while (s.step()) {
        SubmissionRow r{mapSubmission(s), s.colText(10), std::nullopt};
        if (!s.colIsNull(11)) r.mark = mapMark(s, 11);
        v.push_back(std::move(r));
    }
    return v;
}
std::vector<Submission> SQLiteSubmissionRepository::findByStudent(int studentId) {
    Statement s(std::string("SELECT ") + SUB_COLS + " FROM Submissions WHERE student_id = ?1");
    s.bind(1, studentId);
    std::vector<Submission> v;
    while (s.step()) v.push_back(mapSubmission(s));
    return v;
}
int SQLiteSubmissionRepository::save(const Submission& x) {
    Statement s("INSERT INTO Submissions(assignment_id, student_id, file_path, submitted_at, version, days_late, status) "
                "VALUES (?1,?2,?3,?4,?5,?6,?7)");
    s.bind(1, x.assignmentId).bind(2, x.studentId).bind(3, x.filePath).bind(4, x.submittedAt).bind(5, x.version)
        .bind(6, x.daysLate).bind(7, toString(x.status));
    s.run();
    return lastId();
}
void SQLiteSubmissionRepository::update(const Submission& x) {
    Statement s("UPDATE Submissions SET file_path = ?1, submitted_at = ?2, version = ?3, days_late = ?4, status = ?5, "
                "updated_at = ?6 WHERE submission_id = ?7");
    s.bind(1, x.filePath).bind(2, x.submittedAt).bind(3, x.version).bind(4, x.daysLate).bind(5, toString(x.status))
        .bind(6, timeutil::nowUtc()).bind(7, x.id);
    s.run();
}
void SQLiteSubmissionRepository::updateStatus(int id, SubmissionStatus st) {
    Statement s("UPDATE Submissions SET status = ?1, updated_at = ?2 WHERE submission_id = ?3");
    s.bind(1, toString(st)).bind(2, timeutil::nowUtc()).bind(3, id);
    s.run();
}
std::vector<ReviewItem> SQLiteSubmissionRepository::listForReview(int departmentId, std::optional<int> courseId,
                                                                   SubmissionStatus status) {
    Statement s("SELECT s.submission_id, s.assignment_id, s.student_id, s.file_path, s.submitted_at, s.version, "
                "s.days_late, s.status, s.created_at, s.updated_at, "
                "m.mark_id, m.submission_id, m.raw_mark, m.penalty_points, m.final_mark, m.feedback, m.marked_by, "
                "m.marked_at, m.review_due_at, m.moderated_by, m.moderated_at, m.released_at, "
                "u.full_name, a.title, c.course_code, COALESCE(l.full_name, ''), a.max_mark, c.course_id "
                "FROM Submissions s JOIN Marks m ON m.submission_id = s.submission_id "
                "JOIN Users u ON u.user_id = s.student_id "
                "JOIN Assignments a ON a.assignment_id = s.assignment_id "
                "JOIN Courses c ON c.course_id = a.course_id "
                "LEFT JOIN Users l ON l.user_id = m.marked_by "
                "WHERE c.department_id = ?1 AND s.status = ?2 AND (?3 IS NULL OR c.course_id = ?3) "
                "ORDER BY c.course_code, u.full_name");
    s.bind(1, departmentId).bind(2, toString(status)).bind(3, courseId);
    std::vector<ReviewItem> v;
    while (s.step()) {
        ReviewItem r;
        r.submission = mapSubmission(s, 0);
        r.mark = mapMark(s, 10);
        r.studentName = s.colText(22); r.assignmentTitle = s.colText(23); r.courseCode = s.colText(24);
        r.lecturerName = s.colText(25); r.maxMark = s.colDouble(26); r.courseId = s.colInt(27);
        v.push_back(std::move(r));
    }
    return v;
}
int SQLiteSubmissionRepository::countByCourseAndStatus(int courseId, SubmissionStatus st) {
    Statement s("SELECT COUNT(*) FROM Submissions s JOIN Assignments a ON a.assignment_id = s.assignment_id "
                "WHERE a.course_id = ?1 AND s.status = ?2");
    s.bind(1, courseId).bind(2, toString(st));
    s.step();
    return s.colInt(0);
}
int SQLiteSubmissionRepository::countAwaitingReview(int departmentId) {
    Statement s("SELECT COUNT(*) FROM Submissions s JOIN Assignments a ON a.assignment_id = s.assignment_id "
                "JOIN Courses c ON c.course_id = a.course_id WHERE c.department_id = ?1 AND s.status = 'Marked'");
    s.bind(1, departmentId);
    s.step();
    return s.colInt(0);
}

// ======================= Marks =======================
std::optional<Mark> SQLiteMarkRepository::findBySubmission(int submissionId) {
    Statement s(std::string("SELECT ") + MARK_COLS + " FROM Marks WHERE submission_id = ?1");
    s.bind(1, submissionId);
    if (!s.step()) return std::nullopt;
    return mapMark(s);
}
int SQLiteMarkRepository::save(const Mark& m) {
    Statement s("INSERT INTO Marks(submission_id, raw_mark, penalty_points, final_mark, feedback, marked_by, marked_at, "
                "review_due_at, moderated_by, moderated_at, released_at) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)");
    s.bind(1, m.submissionId).bind(2, m.rawMark).bind(3, m.penaltyPoints).bind(4, m.finalMark).bind(5, m.feedback)
        .bind(6, m.markedBy).bind(7, m.markedAt).bind(8, m.reviewDueAt).bind(9, m.moderatedBy).bind(10, m.moderatedAt)
        .bind(11, m.releasedAt);
    s.run();
    return lastId();
}
void SQLiteMarkRepository::update(const Mark& m) {
    Statement s("UPDATE Marks SET raw_mark = ?1, penalty_points = ?2, final_mark = ?3, feedback = ?4, marked_by = ?5, "
                "marked_at = ?6, review_due_at = ?7, moderated_by = ?8, moderated_at = ?9, released_at = ?10 "
                "WHERE mark_id = ?11");
    s.bind(1, m.rawMark).bind(2, m.penaltyPoints).bind(3, m.finalMark).bind(4, m.feedback).bind(5, m.markedBy)
        .bind(6, m.markedAt).bind(7, m.reviewDueAt).bind(8, m.moderatedBy).bind(9, m.moderatedAt).bind(10, m.releasedAt)
        .bind(11, m.id);
    s.run();
}
std::vector<Mark> SQLiteMarkRepository::findExpired(const std::string& now) {
    Statement s("SELECT m.mark_id, m.submission_id, m.raw_mark, m.penalty_points, m.final_mark, m.feedback, m.marked_by, "
                "m.marked_at, m.review_due_at, m.moderated_by, m.moderated_at, m.released_at "
                "FROM Marks m JOIN Submissions s ON s.submission_id = m.submission_id "
                "WHERE s.status = 'Marked' AND m.released_at IS NULL AND m.review_due_at <= ?1");  // ix_marks_due
    s.bind(1, now);
    std::vector<Mark> v;
    while (s.step()) v.push_back(mapMark(s));
    return v;
}
std::vector<StudentMarkView> SQLiteMarkRepository::listReleasedForStudent(int studentId) {
    Statement s("SELECT s.submission_id, s.assignment_id, s.student_id, s.file_path, s.submitted_at, s.version, "
                "s.days_late, s.status, s.created_at, s.updated_at, "
                "m.mark_id, m.submission_id, m.raw_mark, m.penalty_points, m.final_mark, m.feedback, m.marked_by, "
                "m.marked_at, m.review_due_at, m.moderated_by, m.moderated_at, m.released_at, "
                "a.title, c.course_code, a.max_mark FROM Submissions s "
                "JOIN Marks m ON m.submission_id = s.submission_id "
                "JOIN Assignments a ON a.assignment_id = s.assignment_id "
                "JOIN Courses c ON c.course_id = a.course_id "
                "WHERE s.student_id = ?1 AND s.status = 'Released' ORDER BY c.course_code, a.title");
    s.bind(1, studentId);
    std::vector<StudentMarkView> v;
    while (s.step()) {
        StudentMarkView r;
        r.submission = mapSubmission(s, 0);
        r.mark = mapMark(s, 10);
        r.assignmentTitle = s.colText(22); r.courseCode = s.colText(23); r.maxMark = s.colDouble(24);
        v.push_back(std::move(r));
    }
    return v;
}

// ======================= Audit log =======================
void SQLiteAuditRepository::append(const AuditLogEntry& e) {
    Statement s("INSERT INTO AuditLog(mark_id, user_id, action_type, old_value, new_value, reason) "
                "VALUES (?1,?2,?3,?4,?5,?6)");
    s.bind(1, e.markId).bind(2, e.userId).bind(3, e.actionType).bind(4, e.oldValue).bind(5, e.newValue).bind(6, e.reason);
    s.run();
}
std::vector<AuditLogEntry> SQLiteAuditRepository::findByMark(int markId) {
    Statement s("SELECT log_id, mark_id, user_id, action_type, old_value, new_value, reason, changed_at "
                "FROM AuditLog WHERE mark_id = ?1 ORDER BY log_id");
    s.bind(1, markId);
    std::vector<AuditLogEntry> v;
    while (s.step()) {
        AuditLogEntry e;
        e.id = s.colInt(0); e.markId = s.colInt(1); e.userId = s.colInt(2); e.actionType = s.colText(3);
        e.oldValue = s.colOptText(4); e.newValue = s.colOptText(5); e.reason = s.colText(6); e.changedAt = s.colText(7);
        v.push_back(std::move(e));
    }
    return v;
}

}  // namespace lms
