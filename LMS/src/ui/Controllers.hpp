// Thin controllers (design Presentation tier): read input, call ONE service
// method, show the result, and catch AppException. No business rule lives here.
#pragma once
#include "service/Services.hpp"

namespace lms {

struct Services {
    AuthenticationService& auth;
    AdministrationService& admin;
    AssignmentService& assignments;
    SubmissionService& submissions;
    MarkingService& marking;
    ModerationService& moderation;
    HoDNotifier& notifier;
};

class StudentController {
public:
    explicit StudentController(Services& s) : s_(s) {}
    void run();
private:
    Services& s_;
};
class LecturerController {
public:
    explicit LecturerController(Services& s) : s_(s) {}
    void run();
private:
    Services& s_;
};
class HoDController {
public:
    explicit HoDController(Services& s) : s_(s) {}
    void run();
private:
    Services& s_;
};
class AdminController {
public:
    explicit AdminController(Services& s) : s_(s) {}
    void run();
private:
    Services& s_;
};

}  // namespace lms
