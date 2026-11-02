// Domain model (design Fig 2A and 2B) and the enumerations used by it.
#pragma once
#include <optional>
#include <string>

#include "util/Exceptions.hpp"

namespace lms {

constexpr int kSystemUserId = 1;  // reserved SYSTEM account used for automatic actions

enum class Role { Admin, Lecturer, HoD, Student };
enum class SubmissionStatus { Submitted, Late, Marked, Moderated, Released };
enum class Permission {
    MANAGE_USERS, MANAGE_COURSES, ENROL_STUDENTS,
    CREATE_ASSIGNMENT, GRANT_EXTENSION, MARK_SUBMISSION,
    MODERATE_MARKS, RELEASE_MARKS,
    SUBMIT_WORK, VIEW_COURSES, VIEW_OWN_MARKS
};

std::string toString(Role r);
Role roleFromString(const std::string& s);
std::string toString(SubmissionStatus s);
SubmissionStatus statusFromString(const std::string& s);
std::string toString(Permission p);

struct Department {
    int id = 0;
    std::string name;
    std::string createdAt;
};

struct Course {
    int id = 0;
    std::string code;
    std::string title;
    int departmentId = 0;
    std::optional<int> lecturerId;
    std::string createdAt;
};

struct Assignment {
    int id = 0;
    int courseId = 0;
    std::string title;
    std::string deadline;  // UTC ISO-8601
    double maxMark = 0;
    int createdBy = 0;
    std::string createdAt;
};

struct Extension {
    int id = 0;
    int assignmentId = 0;
    int studentId = 0;
    std::string extendedDeadline;
    std::string reason;
    int grantedBy = 0;
    std::string createdAt;
};

struct Submission {
    int id = 0;
    int assignmentId = 0;
    int studentId = 0;
    std::string filePath;
    std::string submittedAt;
    int version = 1;
    int daysLate = 0;
    SubmissionStatus status = SubmissionStatus::Submitted;
    std::string createdAt;
    std::string updatedAt;

    // Resubmission is allowed only before the effective deadline (RE conflict d, FR-7)
    // and only while the work has not been marked yet.
    bool canResubmit(const std::string& nowUtc, const std::string& effectiveDeadlineUtc) const;
};

struct Mark {
    int id = 0;
    int submissionId = 0;
    double rawMark = 0;
    double penaltyPoints = 0;
    double finalMark = 0;
    std::string feedback;
    int markedBy = 0;
    std::string markedAt;
    std::string reviewDueAt;
    std::optional<int> moderatedBy;
    std::optional<std::string> moderatedAt;
    std::optional<std::string> releasedAt;
};

struct AuditLogEntry {
    int id = 0;
    int markId = 0;
    int userId = 0;
    std::string actionType;
    std::optional<std::string> oldValue;
    std::optional<std::string> newValue;
    std::string reason;
    std::string changedAt;
};

}  // namespace lms
