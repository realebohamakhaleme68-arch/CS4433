// SQLite implementations of the repository interfaces. This is the only code
// that contains SQL, and every statement uses bound parameters (NFR-4).
#pragma once
#include "data/Repositories.hpp"

namespace lms {

class SQLiteUserRepository : public IUserRepository {
public:
    std::unique_ptr<User> findById(int id) override;
    std::unique_ptr<User> findByUsername(const std::string& username) override;
    std::optional<AuthRecord> getAuthRecord(const std::string& username) override;
    int save(const User& user, const std::string& passwordHash) override;
    void update(const User& user) override;
    void updatePassword(int userId, const std::string& passwordHash) override;
    void setActive(int userId, bool active) override;
    std::vector<std::unique_ptr<User>> listAll() override;
    int countActiveAdmins() override;
};

class SQLiteCourseRepository : public ICourseRepository {
public:
    int createDepartment(const std::string& name) override;
    std::optional<Department> findDepartment(int id) override;
    std::vector<Department> listDepartments() override;
    int createCourse(const Course& c) override;
    std::optional<Course> findById(int id) override;
    std::vector<Course> listAll() override;
    std::vector<Course> findByDepartment(int departmentId) override;
    std::vector<Course> findByLecturer(int lecturerId) override;
    std::vector<Course> findByStudent(int studentId) override;
    void assignLecturer(int courseId, int lecturerId) override;
    void enrol(int studentId, int courseId) override;
    bool isEnrolled(int studentId, int courseId) override;
    std::vector<std::unique_ptr<User>> listStudents(int courseId) override;
};

class SQLiteAssignmentRepository : public IAssignmentRepository {
public:
    std::optional<Assignment> findById(int id) override;
    int create(const Assignment& a) override;
    std::vector<Assignment> findByCourse(int courseId) override;
    std::vector<AssignmentInfo> findByStudent(int studentId) override;
    int saveExtension(const Extension& e) override;
    std::string findEffectiveDeadline(int assignmentId, int studentId) override;
};

class SQLiteSubmissionRepository : public ISubmissionRepository {
public:
    std::optional<Submission> findById(int id) override;
    std::optional<Submission> findByAssignmentAndStudent(int assignmentId, int studentId) override;
    std::vector<SubmissionRow> findByAssignment(int assignmentId) override;
    std::vector<Submission> findByStudent(int studentId) override;
    int save(const Submission& s) override;
    void update(const Submission& s) override;
    void updateStatus(int id, SubmissionStatus st) override;
    std::vector<ReviewItem> listForReview(int departmentId, std::optional<int> courseId,
                                          SubmissionStatus status) override;
    int countByCourseAndStatus(int courseId, SubmissionStatus status) override;
    int countAwaitingReview(int departmentId) override;
};

class SQLiteMarkRepository : public IMarkRepository {
public:
    std::optional<Mark> findBySubmission(int submissionId) override;
    int save(const Mark& m) override;
    void update(const Mark& m) override;
    std::vector<Mark> findExpired(const std::string& nowUtc) override;
    std::vector<StudentMarkView> listReleasedForStudent(int studentId) override;
};

class SQLiteAuditRepository : public IAuditRepository {
public:
    void append(const AuditLogEntry& e) override;
    std::vector<AuditLogEntry> findByMark(int markId) override;
};

}  // namespace lms
