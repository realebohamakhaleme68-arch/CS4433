#include "service/Services.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "data/DatabaseConnection.hpp"
#include "domain/UserFactory.hpp"
#include "util/InputValidator.hpp"
#include "util/PasswordHasher.hpp"
#include "util/TimeUtil.hpp"

namespace lms {
namespace {

std::string fmt(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}
double round2(double v) { return std::round(v * 100.0) / 100.0; }

struct Ctx {
    Submission sub;
    Assignment asg;
    Course course;
};
Ctx loadCtx(Repos& r, int submissionId) {
    auto sub = r.submissions.findById(submissionId);
    if (!sub) throw ValidationException("Submission not found.");
    auto asg = r.assignments.findById(sub->assignmentId);
    if (!asg) throw ValidationException("Assignment not found.");
    auto c = r.courses.findById(asg->courseId);
    if (!c) throw ValidationException("Course not found.");
    return Ctx{*sub, *asg, *c};
}
Course loadCourse(Repos& r, int courseId) {
    auto c = r.courses.findById(courseId);
    if (!c) throw ValidationException("Course not found.");
    return *c;
}
Assignment loadAssignment(Repos& r, int id) {
    auto a = r.assignments.findById(id);
    if (!a) throw ValidationException("Assignment not found.");
    return *a;
}
int userDept(const Session& s) {
    auto d = s.user().departmentId();
    if (!d) throw AuthorizationException("Your account has no department.");
    return *d;
}
}  // namespace

// ===================================================================== Submission
std::vector<Course> SubmissionService::listMyCourses(const Session& s) {
    authz_.require(s, Permission::VIEW_COURSES);
    return r_.courses.findByStudent(s.user().getUserId());
}

std::vector<StudentAssignmentView> SubmissionService::listMyAssignments(const Session& s) {
    authz_.require(s, Permission::VIEW_COURSES);
    int me = s.user().getUserId();
    std::vector<StudentAssignmentView> out;
    for (auto& info : r_.assignments.findByStudent(me)) {
        StudentAssignmentView v;
        v.effectiveDeadline = r_.assignments.findEffectiveDeadline(info.assignment.id, me);
        v.submission = r_.submissions.findByAssignmentAndStudent(info.assignment.id, me);
        v.info = std::move(info);
        out.push_back(std::move(v));
    }
    return out;
}

SubmissionPreview SubmissionService::preview(const Session& s, int assignmentId) {
    authz_.require(s, Permission::SUBMIT_WORK);
    int me = s.user().getUserId();
    SubmissionPreview p;
    p.assignment = loadAssignment(r_, assignmentId);
    if (!r_.courses.isEnrolled(me, p.assignment.courseId))
        throw AuthorizationException("You are not enrolled in this course.");
    p.effectiveDeadline = r_.assignments.findEffectiveDeadline(assignmentId, me);
    std::string now = timeutil::nowUtc();
    p.daysLate = timeutil::daysLate(now, p.effectiveDeadline);
    p.late = p.daysLate > 0;
    p.existing = r_.submissions.findByAssignmentAndStudent(assignmentId, me);
    if (p.existing) {
        if (!p.existing->canResubmit(now, p.effectiveDeadline))
            throw ValidationException(
                p.existing->status == SubmissionStatus::Submitted || p.existing->status == SubmissionStatus::Late
                    ? "Resubmission is only allowed before the deadline. Your earlier submission is kept."
                    : "This work has already been marked. Resubmission is not allowed.");
        p.isResubmission = true;
    }
    return p;
}

Submission SubmissionService::submit(const Session& s, int assignmentId, const std::string& filePath) {
    authz_.require(s, Permission::SUBMIT_WORK);
    SubmissionPreview p = preview(s, assignmentId);  // enrolment, deadline and resubmission rules
    InputValidator::validateSubmissionFile(filePath);
    int me = s.user().getUserId();
    std::string now = timeutil::nowUtc();
    int version = p.existing ? p.existing->version + 1 : 1;

    // Copy into the storage folder. Old versions stay on disk (history).
    namespace fs = std::filesystem;
    std::string stored;
    try {
        fs::path dir = fs::path(storageDir_) / ("a" + std::to_string(assignmentId)) / ("s" + std::to_string(me));
        fs::create_directories(dir);
        fs::path target = dir / ("v" + std::to_string(version) + "_" + fs::path(filePath).filename().string());
        fs::copy_file(filePath, target, fs::copy_options::overwrite_existing);
        stored = target.generic_string();
    } catch (const fs::filesystem_error& e) {
        throw ValidationException(std::string("Could not store the file: ") + e.code().message());
    }

    Transaction tx;
    Submission sub;
    if (p.existing) {
        sub = *p.existing;
        sub.filePath = stored;
        sub.submittedAt = now;
        sub.version = version;
        sub.daysLate = 0;
        sub.status = SubmissionStatus::Submitted;
        r_.submissions.update(sub);
    } else {
        sub.assignmentId = assignmentId;
        sub.studentId = me;
        sub.filePath = stored;
        sub.submittedAt = now;
        sub.version = 1;
        sub.daysLate = p.daysLate;                       // the penalty itself is applied at marking
        sub.status = p.late ? SubmissionStatus::Late : SubmissionStatus::Submitted;
        sub.id = r_.submissions.save(sub);
    }
    tx.commit();
    return sub;
}

std::vector<StudentMarkView> SubmissionService::listMyReleasedMarks(const Session& s) {
    authz_.require(s, Permission::VIEW_OWN_MARKS);
    return r_.marks.listReleasedForStudent(s.user().getUserId());   // released marks only (BR2)
}

// ===================================================================== Assignment
std::vector<Course> AssignmentService::listMyCourses(const Session& s) {
    authz_.require(s, Permission::CREATE_ASSIGNMENT);
    return r_.courses.findByLecturer(s.user().getUserId());
}
std::vector<Assignment> AssignmentService::listAssignments(const Session& s, int courseId) {
    authz_.require(s, Permission::CREATE_ASSIGNMENT);
    authz_.requireLecturerOf(s, loadCourse(r_, courseId));
    return r_.assignments.findByCourse(courseId);
}
std::vector<std::unique_ptr<User>> AssignmentService::listStudents(const Session& s, int courseId) {
    authz_.require(s, Permission::GRANT_EXTENSION);
    authz_.requireLecturerOf(s, loadCourse(r_, courseId));
    return r_.courses.listStudents(courseId);
}

int AssignmentService::createAssignment(const Session& s, int courseId, const std::string& title,
                                        const std::string& deadlineUtc, double maxMark) {
    authz_.require(s, Permission::CREATE_ASSIGNMENT);
    authz_.requireLecturerOf(s, loadCourse(r_, courseId));
    InputValidator::requireNonEmpty(title, "Title");
    if (!(maxMark > 0 && maxMark <= 1000)) throw ValidationException("Maximum mark must be between 1 and 1000.");
    if (timeutil::toEpoch(deadlineUtc) <= timeutil::toEpoch(timeutil::nowUtc()))
        throw ValidationException("The deadline must be in the future.");
    Assignment a;
    a.courseId = courseId;
    a.title = title;
    a.deadline = deadlineUtc;
    a.maxMark = maxMark;
    a.createdBy = s.user().getUserId();
    return r_.assignments.create(a);
}

int AssignmentService::grantExtension(const Session& s, int assignmentId, int studentId,
                                      const std::string& newDeadlineUtc, const std::string& reason) {
    authz_.require(s, Permission::GRANT_EXTENSION);
    Assignment a = loadAssignment(r_, assignmentId);
    authz_.requireLecturerOf(s, loadCourse(r_, a.courseId));
    InputValidator::requireNonEmpty(reason, "Reason");
    auto student = r_.users.findById(studentId);
    if (!student || student->role() != Role::Student) throw ValidationException("Student not found.");
    if (!r_.courses.isEnrolled(studentId, a.courseId)) throw ValidationException("That student is not enrolled in the course.");
    std::string current = r_.assignments.findEffectiveDeadline(assignmentId, studentId);
    if (timeutil::toEpoch(newDeadlineUtc) <= timeutil::toEpoch(current))
        throw ValidationException("The extended deadline must be later than the current deadline (" +
                                  timeutil::utcToLocal(current) + ").");
    Extension e;
    e.assignmentId = assignmentId;
    e.studentId = studentId;
    e.extendedDeadline = newDeadlineUtc;
    e.reason = reason;
    e.grantedBy = s.user().getUserId();
    return r_.assignments.saveExtension(e);   // append-only: the row is its own audit record
}

// ===================================================================== Marking
void MarkingService::detach(IMarkingObserver* o) {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), o), observers_.end());
}
void MarkingService::notifyObservers(int departmentId, int submissionId) {
    for (auto* o : observers_) o->onMarkSaved(departmentId, submissionId);
}

std::vector<SubmissionRow> MarkingService::listSubmissions(const Session& s, int assignmentId) {
    authz_.require(s, Permission::MARK_SUBMISSION);
    Assignment a = loadAssignment(r_, assignmentId);
    authz_.requireLecturerOf(s, loadCourse(r_, a.courseId));
    return r_.submissions.findByAssignment(assignmentId);
}

Mark MarkingService::markSubmission(const Session& s, int submissionId, double rawMark, const std::string& feedback,
                                    const std::string& reason) {
    authz_.require(s, Permission::MARK_SUBMISSION);
    Ctx c = loadCtx(r_, submissionId);
    authz_.requireLecturerOf(s, c.course);
    if (c.sub.status == SubmissionStatus::Moderated || c.sub.status == SubmissionStatus::Released)
        throw ValidationException("This submission is already moderated or released; only the Head of Department can change it.");
    InputValidator::requireMark(rawMark, c.asg.maxMark);
    InputValidator::requireNonEmpty(feedback, "Feedback");

    auto existing = r_.marks.findBySubmission(submissionId);
    if (existing) InputValidator::requireNonEmpty(reason, "Reason for re-marking");

    double penalty = std::min(c.asg.maxMark, std::max(0.0, strategy_->calculatePenalty(c.sub.daysLate, c.asg.maxMark)));
    double finalMark = std::max(0.0, round2(rawMark - penalty));
    std::string now = timeutil::nowUtc();

    Mark m = existing ? *existing : Mark{};
    m.submissionId = submissionId;
    m.rawMark = rawMark;
    m.penaltyPoints = round2(penalty);
    m.finalMark = finalMark;
    m.feedback = feedback;
    m.markedBy = s.user().getUserId();
    m.markedAt = now;
    m.reviewDueAt = timeutil::addSeconds(now, ModerationService::kReviewWindowSeconds);   // 24 h review window
    m.moderatedBy.reset();
    m.moderatedAt.reset();
    m.releasedAt.reset();

    Transaction tx;   // the mark, the status and the audit entry succeed or fail together
    std::string oldValue;
    if (existing) {
        oldValue = fmt(existing->finalMark);
        r_.marks.update(m);
    } else {
        m.id = r_.marks.save(m);
    }
    r_.submissions.updateStatus(submissionId, SubmissionStatus::Marked);
    audit_.log(s.user().getUserId(), m.id, existing ? "MARK_CHANGED" : "MARK_CREATED", oldValue, fmt(finalMark),
               existing ? reason : "Initial mark");
    tx.commit();
    notifyObservers(c.course.departmentId, submissionId);
    return m;
}

std::vector<AuditLogEntry> MarkingService::auditTrail(const Session& s, int submissionId) {
    Ctx c = loadCtx(r_, submissionId);
    if (s.user().role() == Role::HoD) {
        authz_.require(s, Permission::MODERATE_MARKS);
        authz_.requireDepartment(s, c.course.departmentId);
    } else {
        authz_.require(s, Permission::MARK_SUBMISSION);
        authz_.requireLecturerOf(s, c.course);
    }
    auto m = r_.marks.findBySubmission(submissionId);
    return m ? r_.audit.findByMark(m->id) : std::vector<AuditLogEntry>{};
}

// ===================================================================== Moderation
std::vector<Course> ModerationService::listDepartmentCourses(const Session& s) {
    authz_.require(s, Permission::MODERATE_MARKS);
    return r_.courses.findByDepartment(userDept(s));
}
int ModerationService::countAwaiting(const Session& s) {
    authz_.require(s, Permission::MODERATE_MARKS);
    return r_.submissions.countAwaitingReview(userDept(s));
}
std::vector<ReviewItem> ModerationService::listForReview(const Session& s, std::optional<int> courseId) {
    authz_.require(s, Permission::MODERATE_MARKS);
    if (courseId) authz_.requireDepartment(s, loadCourse(r_, *courseId).departmentId);
    return r_.submissions.listForReview(userDept(s), courseId, SubmissionStatus::Marked);
}
std::vector<ReviewItem> ModerationService::listModerated(const Session& s, std::optional<int> courseId) {
    authz_.require(s, Permission::MODERATE_MARKS);
    if (courseId) authz_.requireDepartment(s, loadCourse(r_, *courseId).departmentId);
    return r_.submissions.listForReview(userDept(s), courseId, SubmissionStatus::Moderated);
}

void ModerationService::approve(const Session& s, int submissionId) {
    adjust(s, submissionId, -1.0, "Approved by Head of Department");
}

void ModerationService::adjust(const Session& s, int submissionId, double newMark, const std::string& reason) {
    authz_.require(s, Permission::MODERATE_MARKS);
    Ctx c = loadCtx(r_, submissionId);
    authz_.requireDepartment(s, c.course.departmentId);
    if (c.sub.status != SubmissionStatus::Marked)
        throw ValidationException("Only submissions with status Marked can be moderated.");
    auto mark = r_.marks.findBySubmission(submissionId);
    if (!mark) throw ValidationException("This submission has no mark yet.");
    InputValidator::requireNonEmpty(reason, "Reason");
    bool approveOnly = newMark < 0;     // approve() passes -1: keep the lecturer's final mark
    if (!approveOnly) InputValidator::requireMark(newMark, c.asg.maxMark);

    std::string old = fmt(mark->finalMark);
    if (!approveOnly) mark->finalMark = round2(newMark);
    mark->moderatedBy = s.user().getUserId();
    mark->moderatedAt = timeutil::nowUtc();

    Transaction tx;
    r_.marks.update(*mark);
    r_.submissions.updateStatus(submissionId, SubmissionStatus::Moderated);
    audit_.log(s.user().getUserId(), mark->id, "MARK_MODERATED", old, fmt(mark->finalMark), reason);
    tx.commit();
}

void ModerationService::reject(const Session& s, int submissionId, const std::string& comment) {
    authz_.require(s, Permission::MODERATE_MARKS);
    Ctx c = loadCtx(r_, submissionId);
    authz_.requireDepartment(s, c.course.departmentId);
    if (c.sub.status != SubmissionStatus::Marked)
        throw ValidationException("Only submissions with status Marked can be returned to the lecturer.");
    InputValidator::requireNonEmpty(comment, "Comment");
    auto mark = r_.marks.findBySubmission(submissionId);
    if (!mark) throw ValidationException("This submission has no mark yet.");

    // The review window restarts so the mark is not auto-released while the lecturer re-marks.
    mark->reviewDueAt = timeutil::addSeconds(timeutil::nowUtc(), kReviewWindowSeconds);
    Transaction tx;
    r_.marks.update(*mark);
    audit_.log(s.user().getUserId(), mark->id, "MARK_REJECTED", fmt(mark->finalMark), fmt(mark->finalMark), comment);
    tx.commit();
}

int ModerationService::release(const Session& s, int courseId) {
    authz_.require(s, Permission::RELEASE_MARKS);
    Course course = loadCourse(r_, courseId);
    authz_.requireDepartment(s, course.departmentId);
    int outstanding = r_.submissions.countByCourseAndStatus(courseId, SubmissionStatus::Marked);
    if (outstanding > 0)
        throw ValidationException(std::to_string(outstanding) +
                                  " submission(s) in this course are not yet moderated. Moderate them first.");
    auto items = r_.submissions.listForReview(userDept(s), courseId, SubmissionStatus::Moderated);
    if (items.empty()) throw ValidationException("There are no moderated marks to release in this course.");

    std::string now = timeutil::nowUtc();
    Transaction tx;
    for (auto& it : items) {
        Mark m = it.mark;
        m.releasedAt = now;
        r_.marks.update(m);
        r_.submissions.updateStatus(it.submission.id, SubmissionStatus::Released);
        audit_.log(s.user().getUserId(), m.id, "MARK_RELEASED", "Moderated", "Released", "Released by Head of Department");
    }
    tx.commit();
    return static_cast<int>(items.size());
}

int ModerationService::processExpiredReviews() {
    std::string now = timeutil::nowUtc();
    auto expired = r_.marks.findExpired(now);
    if (expired.empty()) return 0;
    Transaction tx;
    for (auto& m : expired) {
        m.moderatedBy = kSystemUserId;
        m.moderatedAt = now;
        m.releasedAt = now;
        r_.marks.update(m);
        r_.submissions.updateStatus(m.submissionId, SubmissionStatus::Released);
        audit_.log(kSystemUserId, m.id, "MARK_AUTO_RELEASED", "Marked", "Released",
                   "Automatic release: no HoD action within 24 hours");
    }
    tx.commit();
    return static_cast<int>(expired.size());
}

// ===================================================================== Administration
namespace {
void requireStrongPassword(const std::string& pw) {
    if (!InputValidator::isStrongPassword(pw))
        throw ValidationException("Password must be at least 8 characters and mix letters, numbers and symbols.");
}
}  // namespace

bool AdministrationService::needsFirstAdmin() { return r_.users.countActiveAdmins() == 0; }

int AdministrationService::createFirstAdmin(const std::string& username, const std::string& password,
                                            const std::string& fullName) {
    if (!needsFirstAdmin()) throw AuthorizationException("An administrator already exists.");
    if (!InputValidator::isValidUsername(username))
        throw ValidationException("Username must be 3-32 characters: letters, digits, '.', '_' or '-'.");
    requireStrongPassword(password);
    InputValidator::requireNonEmpty(fullName, "Full name");
    auto admin = UserFactory::create(Role::Admin, 0, username, fullName, "", std::nullopt);
    return r_.users.save(*admin, PasswordHasher::hashPassword(password));
}

std::vector<std::unique_ptr<User>> AdministrationService::listUsers(const Session& s) {
    authz_.require(s, Permission::MANAGE_USERS);
    return r_.users.listAll();
}

int AdministrationService::createUser(const Session& s, Role role, const std::string& username,
                                      const std::string& password, const std::string& fullName,
                                      const std::string& referenceNo, std::optional<int> departmentId) {
    authz_.require(s, Permission::MANAGE_USERS);
    if (!InputValidator::isValidUsername(username))
        throw ValidationException("Username must be 3-32 characters: letters, digits, '.', '_' or '-'.");
    requireStrongPassword(password);
    InputValidator::requireNonEmpty(fullName, "Full name");
    if (r_.users.findByUsername(username)) throw ValidationException("That username is already taken.");
    std::optional<int> dept = (role == Role::Lecturer || role == Role::HoD) ? departmentId : std::nullopt;
    if (dept && !r_.courses.findDepartment(*dept)) throw ValidationException("Department not found.");
    auto user = UserFactory::create(role, 0, username, fullName, referenceNo, dept);   // Factory pattern
    return r_.users.save(*user, PasswordHasher::hashPassword(password));
}

void AdministrationService::updateUser(const Session& s, int userId, const std::string& fullName,
                                       const std::string& referenceNo, std::optional<int> departmentId) {
    authz_.require(s, Permission::MANAGE_USERS);
    if (userId == kSystemUserId) throw ValidationException("The SYSTEM account cannot be changed.");
    auto old = r_.users.findById(userId);
    if (!old) throw ValidationException("User not found.");
    InputValidator::requireNonEmpty(fullName, "Full name");
    std::optional<int> dept = (old->role() == Role::Lecturer || old->role() == Role::HoD) ? departmentId : std::nullopt;
    if (dept && !r_.courses.findDepartment(*dept)) throw ValidationException("Department not found.");
    auto updated = UserFactory::create(old->role(), userId, old->getUsername(), fullName, referenceNo, dept,
                                       old->isActive(), old->getCreatedAt());
    r_.users.update(*updated);
}

void AdministrationService::resetPassword(const Session& s, int userId, const std::string& newPassword) {
    authz_.require(s, Permission::MANAGE_USERS);
    if (userId == kSystemUserId || !r_.users.findById(userId)) throw ValidationException("User not found.");
    requireStrongPassword(newPassword);
    r_.users.updatePassword(userId, PasswordHasher::hashPassword(newPassword));
}

void AdministrationService::setActive(const Session& s, int userId, bool active) {
    authz_.require(s, Permission::MANAGE_USERS);
    if (userId == kSystemUserId) throw ValidationException("The SYSTEM account cannot be changed.");
    auto u = r_.users.findById(userId);
    if (!u) throw ValidationException("User not found.");
    if (!active) {
        if (userId == s.user().getUserId()) throw ValidationException("You cannot deactivate your own account.");
        if (u->role() == Role::Admin && u->isActive() && r_.users.countActiveAdmins() <= 1)
            throw ValidationException("At least one active administrator is required.");
    }
    r_.users.setActive(userId, active);   // accounts are deactivated, never deleted (audit history survives)
}

int AdministrationService::createDepartment(const Session& s, const std::string& name) {
    authz_.require(s, Permission::MANAGE_COURSES);
    InputValidator::requireNonEmpty(name, "Department name");
    return r_.courses.createDepartment(name);
}
std::vector<Department> AdministrationService::listDepartments(const Session& s) {
    authz_.require(s, Permission::MANAGE_COURSES);
    return r_.courses.listDepartments();
}
int AdministrationService::createCourse(const Session& s, const std::string& code, const std::string& title,
                                        int departmentId) {
    authz_.require(s, Permission::MANAGE_COURSES);
    InputValidator::requireNonEmpty(code, "Course code");
    InputValidator::requireNonEmpty(title, "Course title");
    if (!r_.courses.findDepartment(departmentId)) throw ValidationException("Department not found.");
    Course c;
    c.code = code;
    c.title = title;
    c.departmentId = departmentId;
    return r_.courses.createCourse(c);
}
std::vector<Course> AdministrationService::listCourses(const Session& s) {
    authz_.require(s, Permission::MANAGE_COURSES);
    return r_.courses.listAll();
}
void AdministrationService::assignLecturer(const Session& s, int courseId, int lecturerId) {
    authz_.require(s, Permission::MANAGE_COURSES);
    Course c = loadCourse(r_, courseId);
    auto l = r_.users.findById(lecturerId);
    if (!l || l->role() != Role::Lecturer || !l->isActive()) throw ValidationException("Lecturer not found (or inactive).");
    if (l->departmentId() != std::optional<int>(c.departmentId))
        throw ValidationException("The lecturer belongs to a different department than the course.");
    r_.courses.assignLecturer(courseId, lecturerId);
}
void AdministrationService::enrolStudent(const Session& s, int studentId, int courseId) {
    authz_.require(s, Permission::ENROL_STUDENTS);
    loadCourse(r_, courseId);
    auto st = r_.users.findById(studentId);
    if (!st || st->role() != Role::Student || !st->isActive()) throw ValidationException("Student not found (or inactive).");
    if (r_.courses.isEnrolled(studentId, courseId)) throw ValidationException("The student is already enrolled.");
    r_.courses.enrol(studentId, courseId);
}

}  // namespace lms
