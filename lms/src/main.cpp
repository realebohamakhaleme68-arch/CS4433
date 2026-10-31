// Moderated Learning Management System - entry point.
// Usage: lms [path-to-database]   (default: lms.db in the current folder)
//        lms --version            (prints a line and exits: quick "does it start?" check)
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "ui/Application.hpp"

int main(int argc, char** argv) {
    std::cout << std::unitbuf;   // every character is shown immediately
    std::cout << "LMS starting..." << std::endl;
    try {
        std::string arg = argc > 1 ? argv[1] : "";
        if (arg == "--version") {
            std::cout << "Moderated LMS 1.0.0 (C++17)" << std::endl;
            return 0;
        }
        std::string db = arg.empty() ? "lms.db" : arg;
        std::error_code ec;
        std::cout << "Working folder: " << std::filesystem::current_path(ec).string() << std::endl;
        std::cout << "Database file : " << std::filesystem::absolute(db, ec).string() << std::endl;
        return lms::runApplication(db);
    } catch (const std::exception& e) {
        std::cout << "FATAL: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "FATAL: unknown error" << std::endl;
    }
    std::cout << "Press Enter to close...";
    std::string dummy;
    std::getline(std::cin, dummy);
    return 1;
}
