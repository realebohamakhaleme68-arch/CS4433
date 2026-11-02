#include "ui/Application.hpp"

#include <filesystem>
#include <iostream>

#include "data/DatabaseConnection.hpp"
#include "data/SQLiteRepositories.hpp"
#include "service/Services.hpp"
#include "ui/Console.hpp"
#include "ui/Controllers.hpp"
#include "util/InputValidator.hpp"
#include "util/PasswordHasher.hpp"

namespace lms {
using namespace console;

namespace {
void backupDatabase(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(path, ec))   // risk R5: copy lms.db to lms.db.bak at every start-up
        fs::copy_file(path, path + ".bak", fs::copy_options::overwrite_existing, ec);
}

void firstRunSetup(AdministrationService& admin) {
    heading("First-time setup");
    info("No administrator exists yet. Create the first administrator account.");
    while (true) {
        try {
            std::string un = readLine("Username: ");
            std::string name = readLine("Full name: ");
            std::string pw = readPassword("Password (8+ chars, letters+digits+symbols): ");
            if (pw != readPassword("Repeat password: ")) throw ValidationException("The passwords do not match.");
            admin.createFirstAdmin(un, pw, name);
            info("Administrator created. You can now log in.");
            return;
        } catch (const AppException& e) {
            error(e.what());
        }
    }
}
}  // namespace

int runApplication(const std::string& dbPath) {
    try {
        backupDatabase(dbPath);
        auto& db = DatabaseConnection::getInstance();
        db.open(dbPath);
        db.initialiseSchema();   // creates the tables on first run; safe to repeat

        SQLiteUserRepository users;
        SQLiteCourseRepository courses;
        SQLiteAssignmentRepository assignments;
        SQLiteSubmissionRepository submissions;
        SQLiteMarkRepository marks;
        SQLiteAuditRepository audit;
        Repos repos{users, courses, assignments, submissions, marks, audit};

        AuthorizationService authz;
        AuthenticationService auth(users);
        AuditLogger auditLogger(audit);
        StandardDeductionStrategy penalty;           // Strategy: swap here for another rule
        HoDNotifier notifier;                        // Observer

        AdministrationService admin(repos, authz);
        AssignmentService assignmentSvc(repos, authz);
        SubmissionService submissionSvc(repos, authz);
        MarkingService marking(repos, authz, auditLogger, penalty);
        marking.attach(&notifier);
        ModerationService moderation(repos, authz, auditLogger);
        Services svc{auth, admin, assignmentSvc, submissionSvc, marking, moderation, notifier};

        std::cout << "Moderated Learning Management System\n"
                  << "Password hashing: " << (PasswordHasher::usesArgon2id() ? "Argon2id (libsodium)" : "salted SHA-256 (fallback)")
                  << "\n";

        if (admin.needsFirstAdmin()) firstRunSetup(admin);

        while (true) {
            int n = moderation.processExpiredReviews();   // auto-release overdue marks (no background thread)
            if (n > 0) info("[" + std::to_string(n) + " mark(s) were released automatically after the 24 h review window.]");
            int choice = menu("Main menu", {"Log in"}, "Exit");
            if (choice == 0) break;
            std::string un = readLine("Username: ");
            std::string pw = readPassword("Password: ");
            try {
                const Session& s = auth.authenticate(un, pw);
                moderation.processExpiredReviews();
                info("\nWelcome, " + s.user().getFullName() + " (" + s.user().roleName() + ").");
                switch (s.user().role()) {
                    case Role::Student: StudentController(svc).run(); break;
                    case Role::Lecturer: LecturerController(svc).run(); break;
                    case Role::HoD: HoDController(svc).run(); break;
                    case Role::Admin: AdminController(svc).run(); break;
                }
                auth.logout();
                info("Logged out.");
            } catch (const AppException& e) {
                error(e.what());
            }
        }
        db.close();
        return 0;
    } catch (const InputClosed&) {
        std::cout << "\nInput closed. Goodbye.\n";
        DatabaseConnection::getInstance().close();
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

}  // namespace lms
