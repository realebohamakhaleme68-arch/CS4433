#include "domain/Models.hpp"

#include "util/TimeUtil.hpp"

namespace lms {

std::string toString(Role r) {
    switch (r) {
        case Role::Admin: return "Admin";
        case Role::Lecturer: return "Lecturer";
        case Role::HoD: return "HoD";
        case Role::Student: return "Student";
    }
    return "?";
}
Role roleFromString(const std::string& s) {
    if (s == "Admin") return Role::Admin;
    if (s == "Lecturer") return Role::Lecturer;
    if (s == "HoD") return Role::HoD;
    if (s == "Student") return Role::Student;
    throw ValidationException("Unknown role: " + s);
}
std::string toString(SubmissionStatus s) {
    switch (s) {
        case SubmissionStatus::Submitted: return "Submitted";
        case SubmissionStatus::Late: return "Late";
        case SubmissionStatus::Marked: return "Marked";
        case SubmissionStatus::Moderated: return "Moderated";
        case SubmissionStatus::Released: return "Released";
    }
    return "?";
}
SubmissionStatus statusFromString(const std::string& s) {
    if (s == "Submitted") return SubmissionStatus::Submitted;
    if (s == "Late") return SubmissionStatus::Late;
    if (s == "Marked") return SubmissionStatus::Marked;
    if (s == "Moderated") return SubmissionStatus::Moderated;
    if (s == "Released") return SubmissionStatus::Released;
    throw ValidationException("Unknown status: " + s);
}
std::string toString(Permission p) {
    switch (p) {
        case Permission::MANAGE_USERS: return "MANAGE_USERS";
        case Permission::MANAGE_COURSES: return "MANAGE_COURSES";
        case Permission::ENROL_STUDENTS: return "ENROL_STUDENTS";
        case Permission::CREATE_ASSIGNMENT: return "CREATE_ASSIGNMENT";
        case Permission::GRANT_EXTENSION: return "GRANT_EXTENSION";
        case Permission::MARK_SUBMISSION: return "MARK_SUBMISSION";
        case Permission::MODERATE_MARKS: return "MODERATE_MARKS";
        case Permission::RELEASE_MARKS: return "RELEASE_MARKS";
        case Permission::SUBMIT_WORK: return "SUBMIT_WORK";
        case Permission::VIEW_COURSES: return "VIEW_COURSES";
        case Permission::VIEW_OWN_MARKS: return "VIEW_OWN_MARKS";
    }
    return "?";
}

bool Submission::canResubmit(const std::string& now, const std::string& effectiveDeadline) const {
    return status == SubmissionStatus::Submitted &&
           timeutil::toEpoch(now) <= timeutil::toEpoch(effectiveDeadline);
}

}  // namespace lms
