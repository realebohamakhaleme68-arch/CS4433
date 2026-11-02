#include "ui/Controllers.hpp"

#include <cstdio>
#include <functional>
#include <iostream>

#include "ui/Console.hpp"
#include "util/InputValidator.hpp"
#include "util/TimeUtil.hpp"

namespace lms {
using namespace console;

namespace {

std::string num(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}
std::string local(const std::string& utc) {
    return timeutil::utcToLocal(utc);
}

void guarded(const std::function<void()>& action) {
    try {
        action();
    } catch (const Cancelled&) {
        info("Cancelled.");
    } catch (const AppException& e) {
        error(e.what());
    }
}

// Common loop: shows the menu until the user logs out or the session times out.
template <typename Fn>
void sessionLoop(const std::string& title, const std::vector<std::string>& items, Fn&& dispatch,
                 const std::function<void(const Session&)>& banner = nullptr) {
    while (true) {
        Session& s = SessionManager::getInstance().current();
        if (s.isExpired()) {
            info("\nYour session expired after 15 minutes of inactivity. Please log in again.");
            return;
        }
        if (banner) banner(s);
        int choice = menu(title, items, "Log out");
        s.touch();
        if (choice == 0) return;
        guarded([&] { dispatch(choice, s); });
        s.touch();
    }
}

std::string courseLabel(const Course& c) {
    return pad(c.code, 10) + c.title;
}

int pickCourse(const std::vector<Course>& cs) {
    std::vector<std::string> l;
    for (auto& c : cs) l.push_back(courseLabel(c));
    int i = choose("Choose a course", l);
    return i < 0 ? -1 : cs[static_cast<size_t>(i)].id;
}

void printAudit(const std::vector<AuditLogEntry>& trail) {
    if (trail.empty()) {
        info("No audit entries.");
        return;
    }
    for (auto& e : trail)
        info(local(e.changedAt) + "  user#" + std::to_string(e.userId) + "  " + pad(e.actionType, 18) + " " +
             e.oldValue.value_or("-") + " -> " + e.newValue.value_or("-") + "  (" + e.reason + ")");
}

}  // namespace

// ============================================================ Student
void StudentController::run() {
    sessionLoop(
        "Student menu",
        {"View my courses", "View my assignments and deadlines", "Submit an assignment",
         "View released marks and feedback"},
        [&](int choice, const Session& s) {
            switch (choice) {
                case 1: {
                    heading("My courses");
                    auto cs = s_.submissions.listMyCourses(s);
                    if (cs.empty()) info("You are not enrolled in any course.");
                    for (auto& c : cs) info(courseLabel(c));
                    break;
                }
                case 2: {
                    heading("My assignments");
                    auto v = s_.submissions.listMyAssignments(s);
                    if (v.empty()) info("No assignments yet.");
                    for (auto& a : v) {
                        std::string st = a.submission ? toString(a.submission->status) + " (v" +
                                                            std::to_string(a.submission->version) + ")"
                                                      : "Not submitted";
                        info(pad(a.info.courseCode, 9) + pad(a.info.assignment.title, 26) + "due " +
                             local(a.effectiveDeadline) + "  max " + num(a.info.assignment.maxMark) + "  " + st);
                    }
                    break;
                }
                case 3: {
                    auto v = s_.submissions.listMyAssignments(s);
                    std::vector<std::string> labels;
                    for (auto& a : v)
                        labels.push_back(pad(a.info.courseCode, 9) + pad(a.info.assignment.title, 26) + "due " +
                                         local(a.effectiveDeadline));
                    int i = choose("Choose the assignment to submit", labels);
                    if (i < 0) return;
                    int aid = v[static_cast<size_t>(i)].info.assignment.id;
                    SubmissionPreview p = s_.submissions.preview(s, aid);
                    info("Deadline for you: " + local(p.effectiveDeadline));
                    if (p.isResubmission)
                        info("You already submitted (version " + std::to_string(p.existing->version) +
                             "). The new file will replace it; the old version is kept in history.");
                    if (p.late) {
                        if (p.daysLate <= 3)
                            info("WARNING: this work is " + std::to_string(p.daysLate) + " day(s) late. A penalty of " +
                                 std::to_string(p.daysLate * 10) +
                                 "% of the maximum mark will be applied when it is marked.");
                        else
                            info(
                                "WARNING: this work is more than 3 days late. The mark will be zero. It is still "
                                "accepted.");
                        if (!confirm("Submit anyway?")) return;
                    }
                    std::string path = readLine("Path of the file to submit (empty = cancel): ");
                    if (path.empty()) return;
                    Submission sub = s_.submissions.submit(s, aid, path);
                    info("\nReceipt: submission #" + std::to_string(sub.id) + ", version " +
                         std::to_string(sub.version) + ", status " + toString(sub.status) + ", days late " +
                         std::to_string(sub.daysLate) + ", time " + local(sub.submittedAt));
                    break;
                }
                case 4: {
                    heading("Released marks");
                    auto v = s_.submissions.listMyReleasedMarks(s);
                    if (v.empty()) info("No marks have been released yet.");
                    for (auto& m : v) {
                        info(pad(m.courseCode, 9) + pad(m.assignmentTitle, 26) + "mark " + num(m.mark.finalMark) +
                             " / " + num(m.maxMark) +
                             (m.mark.penaltyPoints > 0 ? "  (late penalty " + num(m.mark.penaltyPoints) + ")" : ""));
                        info("    Feedback: " + m.mark.feedback);
                    }
                    break;
                }
            }
        });
}

// ============================================================ Lecturer
void LecturerController::run() {
    sessionLoop(
        "Lecturer menu",
        {"My courses", "Create an assignment", "Grant an extension to a student", "Mark submissions",
         "View audit trail of a mark"},
        [&](int choice, const Session& s) {
            auto courses = s_.assignments.listMyCourses(s);
            switch (choice) {
                case 1: {
                    heading("My courses");
                    if (courses.empty()) info("No courses are assigned to you yet.");
                    for (auto& c : courses) {
                        info(courseLabel(c));
                        for (auto& a : s_.assignments.listAssignments(s, c.id))
                            info("      #" + std::to_string(a.id) + " " + pad(a.title, 26) + "due " +
                                 local(a.deadline) + "  max " + num(a.maxMark));
                    }
                    break;
                }
                case 2: {
                    int cid = pickCourse(courses);
                    if (cid < 0) return;
                    std::string title = readLine("Assignment title: ");
                    std::string deadline = readDateTime("Deadline");
                    double max = readDouble("Maximum mark: ");
                    int id = s_.assignments.createAssignment(s, cid, title, deadline, max);
                    info("Assignment #" + std::to_string(id) + " created.");
                    break;
                }
                case 3: {
                    int cid = pickCourse(courses);
                    if (cid < 0) return;
                    auto asgs = s_.assignments.listAssignments(s, cid);
                    std::vector<std::string> al;
                    for (auto& a : asgs) al.push_back(pad(a.title, 26) + "due " + local(a.deadline));
                    int ai = choose("Choose the assignment", al);
                    if (ai < 0) return;
                    auto studs = s_.assignments.listStudents(s, cid);
                    std::vector<std::string> sl;
                    for (auto& u : studs) sl.push_back(pad(u->getFullName(), 28) + u->referenceNo());
                    int si = choose("Choose the student", sl);
                    if (si < 0) return;
                    std::string newDl = readDateTime("New deadline for this student");
                    std::string reason = readLine("Reason for the extension: ");
                    s_.assignments.grantExtension(s, asgs[static_cast<size_t>(ai)].id,
                                                  studs[static_cast<size_t>(si)]->getUserId(), newDl, reason);
                    info("Extension granted. The work will not count as late before " + local(newDl) + ".");
                    break;
                }
                case 4: {
                    int cid = pickCourse(courses);
                    if (cid < 0) return;
                    auto asgs = s_.assignments.listAssignments(s, cid);
                    std::vector<std::string> al;
                    for (auto& a : asgs) al.push_back(pad(a.title, 26) + "max " + num(a.maxMark));
                    int ai = choose("Choose the assignment to mark", al);
                    if (ai < 0) return;
                    const Assignment& asg = asgs[static_cast<size_t>(ai)];
                    auto rows = s_.marking.listSubmissions(s, asg.id);
                    std::vector<std::string> rl;
                    for (auto& r : rows)
                        rl.push_back(pad(r.studentName, 24) + pad(toString(r.submission.status), 11) + "v" +
                                     std::to_string(r.submission.version) + "  days late " +
                                     std::to_string(r.submission.daysLate) +
                                     (r.mark ? "  final " + num(r.mark->finalMark) : ""));
                    int ri = choose("Submissions for " + asg.title, rl);
                    if (ri < 0) return;
                    const SubmissionRow& row = rows[static_cast<size_t>(ri)];
                    info("File: " + row.submission.filePath);
                    if (row.submission.daysLate > 0)
                        info("This work is " + std::to_string(row.submission.daysLate) +
                             " day(s) late; the penalty is applied automatically.");
                    double raw = readDouble("Mark (0 to " + num(asg.maxMark) + "): ");
                    std::string fb = readLine("Written feedback: ");
                    std::string reason;
                    if (row.mark) reason = readLine("Reason for changing the existing mark: ");
                    Mark m = s_.marking.markSubmission(s, row.submission.id, raw, fb, reason);
                    info("Saved. Raw " + num(m.rawMark) + ", late penalty " + num(m.penaltyPoints) + ", final " +
                         num(m.finalMark) + ". Hidden from the student until released.");
                    info(s_.notifier.lastMessage());
                    info("Review window ends " + local(m.reviewDueAt) + " (auto-release if the HoD takes no action).");
                    break;
                }
                case 5: {
                    int cid = pickCourse(courses);
                    if (cid < 0) return;
                    auto asgs = s_.assignments.listAssignments(s, cid);
                    std::vector<std::string> al;
                    for (auto& a : asgs) al.push_back(a.title);
                    int ai = choose("Choose the assignment", al);
                    if (ai < 0) return;
                    auto rows = s_.marking.listSubmissions(s, asgs[static_cast<size_t>(ai)].id);
                    std::vector<std::string> rl;
                    for (auto& r : rows) rl.push_back(r.studentName);
                    int ri = choose("Choose the student", rl);
                    if (ri < 0) return;
                    printAudit(s_.marking.auditTrail(s, rows[static_cast<size_t>(ri)].submission.id));
                    break;
                }
            }
        });
}

// ============================================================ Head of Department
void HoDController::run() {
    auto banner = [&](const Session& s) {
        int n = s_.moderation.countAwaiting(s);
        if (n > 0) info("\n>> " + std::to_string(n) + " mark(s) await your moderation (auto-release after 24 h).");
    };
    sessionLoop(
        "Head of Department menu",
        {"Review marks awaiting moderation", "Approve marks", "Adjust a mark", "Return a mark to the lecturer",
         "Release moderated marks of a course", "View audit trail of a mark"},
        [&](int choice, const Session& s) {
            auto courses = s_.moderation.listDepartmentCourses(s);
            int cid = pickCourse(courses);
            if (cid < 0) return;
            auto items = s_.moderation.listForReview(s, cid);
            auto labelOf = [](const ReviewItem& r) {
                return pad(r.studentName, 22) + pad(r.assignmentTitle, 20) + "raw " + num(r.mark.rawMark) +
                       "  penalty " + num(r.mark.penaltyPoints) + "  final " + num(r.mark.finalMark) + " / " +
                       num(r.maxMark);
            };
            switch (choice) {
                case 1: {
                    heading("Marks awaiting moderation");
                    if (items.empty()) info("Nothing awaits moderation in this course.");
                    for (auto& r : items) {
                        info(labelOf(r) + "  (lecturer " + r.lecturerName + ", due " + local(r.mark.reviewDueAt) + ")");
                        info("      Feedback: " + r.mark.feedback);
                    }
                    break;
                }
                case 2: {
                    if (items.empty()) {
                        info("Nothing awaits moderation in this course.");
                        return;
                    }
                    std::vector<std::string> l;
                    for (auto& r : items) l.push_back(labelOf(r));
                    l.push_back("** Approve ALL of the above **");
                    int i = choose("Approve", l);
                    if (i < 0) return;
                    if (static_cast<size_t>(i) == items.size()) {
                        for (auto& r : items) s_.moderation.approve(s, r.submission.id);
                        info(std::to_string(items.size()) + " mark(s) approved.");
                    } else {
                        s_.moderation.approve(s, items[static_cast<size_t>(i)].submission.id);
                        info("Mark approved.");
                    }
                    break;
                }
                case 3:
                case 4:
                case 6: {
                    std::vector<std::string> l;
                    for (auto& r : items) l.push_back(labelOf(r));
                    if (choice == 6) {  // audit trail may be needed for already-moderated work too
                        for (auto& r : s_.moderation.listModerated(s, cid)) {
                            items.push_back(r);
                            l.push_back(labelOf(r));
                        }
                    }
                    int i = choose(choice == 3   ? "Adjust which mark?"
                                   : choice == 4 ? "Return which mark?"
                                                 : "Audit trail of which mark?",
                                   l);
                    if (i < 0) return;
                    const ReviewItem& r = items[static_cast<size_t>(i)];
                    if (choice == 3) {
                        double nm = readDouble("New mark (0 to " + num(r.maxMark) + "): ");
                        std::string reason = readLine("Reason for the adjustment: ");
                        s_.moderation.adjust(s, r.submission.id, nm, reason);
                        info("Mark adjusted and moderated.");
                    } else if (choice == 4) {
                        std::string c = readLine("Comment for the lecturer: ");
                        s_.moderation.reject(s, r.submission.id, c);
                        info("Returned to the lecturer. The 24 h review window restarts.");
                    } else {
                        printAudit(s_.marking.auditTrail(s, r.submission.id));
                    }
                    break;
                }
                case 5: {
                    if (!confirm("Release all moderated marks of this course to the students?")) return;
                    int n = s_.moderation.release(s, cid);
                    info(std::to_string(n) + " mark(s) released to students.");
                    break;
                }
            }
        },
        banner);
}

// ============================================================ Administrator
namespace {
Role askRole() {
    int i = choose("Role", {"Student", "Lecturer", "HoD", "Admin"});
    if (i < 0) throw Cancelled{};
    static const Role roles[] = {Role::Student, Role::Lecturer, Role::HoD, Role::Admin};
    return roles[i];
}
void listUsers(AdministrationService& a, const Session& s) {
    heading("Users");
    for (auto& u : a.listUsers(s))
        info("#" + pad(std::to_string(u->getUserId()), 5) + pad(u->getUsername(), 16) + pad(u->roleName(), 10) +
             pad(u->getFullName(), 26) + (u->isActive() ? "active" : "INACTIVE"));
}
int askDepartment(AdministrationService& a, const Session& s) {
    auto createNow = [&]() {
        std::string name = readLine("New department name: ");
        int id = a.createDepartment(s, name);
        info("Department #" + std::to_string(id) + " created.");
        return id;
    };
    auto ds = a.listDepartments(s);
    if (ds.empty()) {
        info("No departments exist yet.");
        if (!confirm("Create a department now?")) throw Cancelled{};
        return createNow();
    }
    std::vector<std::string> l;
    for (auto& d : ds) l.push_back(d.name);
    l.push_back("** Create a new department **");
    int i = choose("Department", l);
    if (i < 0) throw Cancelled{};
    if (static_cast<size_t>(i) == ds.size()) return createNow();
    return ds[static_cast<size_t>(i)].id;
}
}  // namespace

void AdminController::run() {
    sessionLoop("Administrator menu",
                {"Create a user", "List users", "Update a user", "Reset a user's password",
                 "Activate / deactivate a user", "Create a department", "List departments", "Create a course",
                 "List courses", "Assign a lecturer to a course", "Enrol a student in a course"},
                [&](int choice, const Session& s) {
                    auto& a = s_.admin;
                    switch (choice) {
                        case 1: {
                            Role role = askRole();
                            std::optional<int> dept;  // asked first so nothing is typed in vain
                            if (role == Role::Lecturer || role == Role::HoD) dept = askDepartment(a, s);
                            std::string un = readLine("Username: ");
                            std::string name = readLine("Full name: ");
                            std::string ref = role == Role::Student ? readLine("Student number: ")
                                              : role == Role::Admin ? ""
                                                                    : readLine("Employee number: ");
                            std::string password = readPassword("Password: ");
                            int userId = a.createUser(s, role, un, password, name, ref, dept);
                            info("User #" + std::to_string(userId) + " created.");
                            break;
                        }
                        case 2:
                            listUsers(a, s);
                            break;
                        case 3: {
                            listUsers(a, s);
                            int id = readInt("User id to update (0 = cancel): ", 0, 1000000);
                            if (id == 0) return;
                            std::string name = readLine("New full name: ");
                            std::string ref = readLine("New student/employee number (empty = none): ");
                            std::optional<int> dept;
                            for (auto& u : a.listUsers(s))
                                if (u->getUserId() == id && (u->role() == Role::Lecturer || u->role() == Role::HoD))
                                    dept = askDepartment(a, s);
                            a.updateUser(s, id, name, ref, dept);
                            info("User updated.");
                            break;
                        }
                        case 4: {
                            listUsers(a, s);
                            int id = readInt("User id (0 = cancel): ", 0, 1000000);
                            if (id == 0) return;
                            a.resetPassword(s, id, readPassword("New password: "));
                            info("Password changed.");
                            break;
                        }
                        case 5: {
                            listUsers(a, s);
                            int id = readInt("User id (0 = cancel): ", 0, 1000000);
                            if (id == 0) return;
                            bool activate = confirm("Activate this account? (n = deactivate)");
                            a.setActive(s, id, activate);
                            info(activate ? "Account activated." : "Account deactivated (history is kept).");
                            break;
                        }
                        case 6:
                            info("Department #" + std::to_string(a.createDepartment(s, readLine("Department name: "))) +
                                 " created.");
                            break;
                        case 7: {
                            heading("Departments");
                            for (auto& d : a.listDepartments(s)) info("#" + std::to_string(d.id) + " " + d.name);
                            break;
                        }
                        case 8: {
                            std::string code = readLine("Course code: ");
                            std::string title = readLine("Course title: ");
                            int dept = askDepartment(a, s);
                            info("Course #" + std::to_string(a.createCourse(s, code, title, dept)) + " created.");
                            break;
                        }
                        case 9: {
                            heading("Courses");
                            for (auto& c : a.listCourses(s))
                                info("#" + pad(std::to_string(c.id), 4) + courseLabel(c) +
                                     "  lecturer id: " + (c.lecturerId ? std::to_string(*c.lecturerId) : "none"));
                            break;
                        }
                        case 10: {
                            int cid = pickCourse(a.listCourses(s));
                            if (cid < 0) return;
                            listUsers(a, s);
                            int lid = readInt("Lecturer user id (0 = cancel): ", 0, 1000000);
                            if (lid == 0) return;
                            a.assignLecturer(s, cid, lid);
                            info("Lecturer assigned.");
                            break;
                        }
                        case 11: {
                            int cid = pickCourse(a.listCourses(s));
                            if (cid < 0) return;
                            listUsers(a, s);
                            int sid = readInt("Student user id (0 = cancel): ", 0, 1000000);
                            if (sid == 0) return;
                            a.enrolStudent(s, sid, cid);
                            info("Student enrolled.");
                            break;
                        }
                    }
                });
}

}  // namespace lms
