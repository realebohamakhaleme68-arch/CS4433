// Repository / DAO interfaces (design Fig 2E). Services depend on these
// interfaces only, never on SQLite classes (DIP), so tests can use fakes.
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "domain/Models.hpp"
#include "domain/User.hpp"

namespace lms {

struct AuthRecord {
    int userId = 0;
    std::string passwordHash;
    bool active = false;
};

struct AssignmentInfo {          // an assignment together with its course code
    Assignment assignment;
    std::string courseCode;
};

struct SubmissionRow {           // lecturer's marking list
    Submission submission;
    std::string studentName;
    std::optional<Mark> mark;
};

struct ReviewItem {              // HoD moderation list
    Submission submission;
    Mark mark;
    std::string studentName;
    std::string assignmentTitle;
    std::string courseCode;
    std::string lecturerName;
    double maxMark = 0;
    int courseId = 0;
};

struct StudentMarkView {         // what a student sees after release
    Submission submission;
    Mark mark;
    std::string assignmentTitle;
    std::string courseCode;
    double maxMark = 0;
};

class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    virtual std::unique_ptr<User> findById(int id) = 0;
    virtual std::unique_ptr<User> findByUsername(const std::string& username) = 0;
    virtual std::optional<AuthRecord> getAuthRecord(const std::string& username) = 0;
    virtual int save(const User& user, const std::string& passwordHash) = 0;
    virtual void update(const User& user) = 0;
    virtual void updatePassword(int userId, const std::string& passwordHash) = 0;
    virtual void setActive(int userId, bool active) = 0;
    virtual std::vector<std::unique_ptr<User>> listAll() = 0;
    virtual int countActiveAdmins() = 0;
};

class ICourseRepository {
public:
    virtual ~ICourseRepository() = default;
    virtual int createDepartment(const std::string& name) = 0;
    virtual std::optional<Department> findDepartment(int id) = 0;
    virtual std::vector<Department> listDepartments() = 0;
    virtual int createCourse(const Course& c) = 0;
    virtual std::optional<Course> findById(int id) = 0;
    virtual std::vector<Course> listAll() = 0;
    virtual std::vector<Course> findByDepartment(int departmentId) = 0;
    virtual std::vector<Course> findByLecturer(int lecturerId) = 0;
    virtual std::vector<Course> findByStudent(int studentId) = 0;
    virtual void assignLecturer(int courseId, int lecturerId) = 0;
    virtual void enrol(int studentId, int courseId) = 0;
    virtual bool isEnrolled(int studentId, int courseId) = 0;
    virtual std::vector<std::unique_ptr<User>> listStudents(int courseId) = 0;
};

class IAssignmentRepository {
public:
    virtual ~IAssignmentRepository() = default;
    virtual std::optional<Assignment> findById(int id) = 0;
    virtual int create(const Assignment& a) = 0;
    virtual std::vector<Assignment> findByCourse(int courseId) = 0;
    virtual std::vector<AssignmentInfo> findByStudent(int studentId) = 0;
    virtual int saveExtension(const Extension& e) = 0;
    // latest extended_deadline for the student, otherwise the assignment deadline
    virtual std::string findEffectiveDeadline(int assignmentId, int studentId) = 0;
};

class ISubmissionRepository {
public:
    virtual ~ISubmissionRepository() = default;
    virtual std::optional<Submission> findById(int id) = 0;
    virtual std::optional<Submission> findByAssignmentAndStudent(int assignmentId, int studentId) = 0;
    virtual std::vector<SubmissionRow> findByAssignment(int assignmentId) = 0;
    virtual std::vector<Submission> findByStudent(int studentId) = 0;
    virtual int save(const Submission& s) = 0;     // insert
    virtual void update(const Submission& s) = 0;  // new file / version / status
    virtual void updateStatus(int id, SubmissionStatus st) = 0;
    virtual std::vector<ReviewItem> listForReview(int departmentId, std::optional<int> courseId,
                                                  SubmissionStatus status) = 0;
    virtual int countByCourseAndStatus(int courseId, SubmissionStatus status) = 0;
    virtual int countAwaitingReview(int departmentId) = 0;
};

class IMarkRepository {
public:
    virtual ~IMarkRepository() = default;
    virtual std::optional<Mark> findBySubmission(int submissionId) = 0;
    virtual int save(const Mark& m) = 0;
    virtual void update(const Mark& m) = 0;
    virtual std::vector<Mark> findExpired(const std::string& nowUtc) = 0;  // Marked and review window over
    virtual std::vector<StudentMarkView> listReleasedForStudent(int studentId) = 0;
};

class IAuditRepository {
public:
    virtual ~IAuditRepository() = default;
    virtual void append(const AuditLogEntry& e) = 0;
    virtual std::vector<AuditLogEntry> findByMark(int markId) = 0;
};

}  // namespace lms
