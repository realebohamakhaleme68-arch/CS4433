// Application services (design Fig 2D). Every public method takes the current
// Session first and starts with an authorisation check (FR-2).
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "data/Repositories.hpp"
#include "service/AuditLogger.hpp"
#include "service/MarkingObserver.hpp"
#include "service/PenaltyStrategy.hpp"
#include "service/Security.hpp"

namespace lms {

struct Repos {
    IUserRepository& users;
    ICourseRepository& courses;
    IAssignmentRepository& assignments;
    ISubmissionRepository& submissions;
    IMarkRepository& marks;
    IAuditRepository& audit;
};

// ---------------------------------------------------------------- Submission
struct SubmissionPreview {
    Assignment assignment;
    std::string effectiveDeadline;
    bool late = false;
    int daysLate = 0;
    std::optional<Submission> existing;
    bool isResubmission = false;
};
struct StudentAssignmentView {
    AssignmentInfo info;
    std::string effectiveDeadline;
    std::optional<Submission> submission;
};

class SubmissionService {
public:
    SubmissionService(Repos r, AuthorizationService& authz, std::string storageDir = "submissions")
        : r_(r), authz_(authz), storageDir_(std::move(storageDir)) {}
    std::vector<Course> listMyCourses(const Session& s);                         // FR-6
    std::vector<StudentAssignmentView> listMyAssignments(const Session& s);      // FR-6
    SubmissionPreview preview(const Session& s, int assignmentId);               // lateness warning (UC-01 A2/A3)
    Submission submit(const Session& s, int assignmentId, const std::string& filePath);  // FR-7
    std::vector<StudentMarkView> listMyReleasedMarks(const Session& s);          // FR-13
private:
    Repos r_;
    AuthorizationService& authz_;
    std::string storageDir_;
};

// ---------------------------------------------------------------- Assignment
class AssignmentService {
public:
    AssignmentService(Repos r, AuthorizationService& authz) : r_(r), authz_(authz) {}
    std::vector<Course> listMyCourses(const Session& s);
    std::vector<Assignment> listAssignments(const Session& s, int courseId);
    int createAssignment(const Session& s, int courseId, const std::string& title, const std::string& deadlineUtc,
                         double maxMark);                                              // FR-5
    int grantExtension(const Session& s, int assignmentId, int studentId, const std::string& newDeadlineUtc,
                       const std::string& reason);                                     // FR-9
    std::vector<std::unique_ptr<User>> listStudents(const Session& s, int courseId);
private:
    Repos r_;
    AuthorizationService& authz_;
};

// ---------------------------------------------------------------- Marking
class MarkingService {
public:
    MarkingService(Repos r, AuthorizationService& authz, AuditLogger& audit, const ILatePenaltyStrategy& strategy)
        : r_(r), authz_(authz), audit_(audit), strategy_(&strategy) {}
    void setStrategy(const ILatePenaltyStrategy& s) { strategy_ = &s; }
    void attach(IMarkingObserver* o) { observers_.push_back(o); }
    void detach(IMarkingObserver* o);
    std::vector<SubmissionRow> listSubmissions(const Session& s, int assignmentId);
    Mark markSubmission(const Session& s, int submissionId, double rawMark, const std::string& feedback,
                        const std::string& reason);                                   // FR-8, FR-10, FR-14
    std::vector<AuditLogEntry> auditTrail(const Session& s, int submissionId);
private:
    void notifyObservers(int departmentId, int submissionId);
    Repos r_;
    AuthorizationService& authz_;
    AuditLogger& audit_;
    const ILatePenaltyStrategy* strategy_;
    std::vector<IMarkingObserver*> observers_;
};

// ---------------------------------------------------------------- Moderation
class ModerationService {
public:
    static constexpr long long kReviewWindowSeconds = 24 * 3600;
    ModerationService(Repos r, AuthorizationService& authz, AuditLogger& audit) : r_(r), authz_(authz), audit_(audit) {}
    std::vector<Course> listDepartmentCourses(const Session& s);
    std::vector<ReviewItem> listForReview(const Session& s, std::optional<int> courseId);   // MARKED, awaiting HoD
    std::vector<ReviewItem> listModerated(const Session& s, std::optional<int> courseId);
    void approve(const Session& s, int submissionId);                                        // FR-11
    void adjust(const Session& s, int submissionId, double newMark, const std::string& reason);
    void reject(const Session& s, int submissionId, const std::string& comment);
    int release(const Session& s, int courseId);                                             // FR-12
    int processExpiredReviews();   // auto-release after 24 h as user SYSTEM (RE conflict a)
    int countAwaiting(const Session& s);
private:
    Repos r_;
    AuthorizationService& authz_;
    AuditLogger& audit_;
};

// ---------------------------------------------------------------- Administration
class AdministrationService {
public:
    AdministrationService(Repos r, AuthorizationService& authz) : r_(r), authz_(authz) {}
    bool needsFirstAdmin();
    int createFirstAdmin(const std::string& username, const std::string& password, const std::string& fullName);
    std::vector<std::unique_ptr<User>> listUsers(const Session& s);
    int createUser(const Session& s, Role role, const std::string& username, const std::string& password,
                   const std::string& fullName, const std::string& referenceNo, std::optional<int> departmentId);  // FR-3
    void updateUser(const Session& s, int userId, const std::string& fullName, const std::string& referenceNo,
                    std::optional<int> departmentId);
    void resetPassword(const Session& s, int userId, const std::string& newPassword);
    void setActive(const Session& s, int userId, bool active);
    int createDepartment(const Session& s, const std::string& name);                         // FR-4
    std::vector<Department> listDepartments(const Session& s);
    int createCourse(const Session& s, const std::string& code, const std::string& title, int departmentId);
    std::vector<Course> listCourses(const Session& s);
    void assignLecturer(const Session& s, int courseId, int lecturerId);
    void enrolStudent(const Session& s, int studentId, int courseId);
private:
    Repos r_;
    AuthorizationService& authz_;
};

}  // namespace lms
