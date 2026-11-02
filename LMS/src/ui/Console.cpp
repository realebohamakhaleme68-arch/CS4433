#include "ui/Console.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "util/Exceptions.hpp"
#include "util/InputValidator.hpp"
#include "util/TimeUtil.hpp"

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#define LMS_ISATTY _isatty(_fileno(stdin))
#else
#include <termios.h>
#include <unistd.h>
#define LMS_ISATTY isatty(STDIN_FILENO)
#endif

namespace lms::console {
namespace {
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}
}  // namespace

std::string readLine(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) throw InputClosed{};
    return trim(line);
}

std::string readPassword(const std::string& prompt) {
    if (!LMS_ISATTY) return readLine(prompt);
    std::cout << prompt << std::flush;
    std::string pw;
#ifdef _WIN32
    int ch;
    while ((ch = _getch()) != '\r' && ch != '\n') {
        if (ch == 3) throw InputClosed{};
        if (ch == '\b') { if (!pw.empty()) pw.pop_back(); }
        else pw.push_back(static_cast<char>(ch));
    }
#else
    termios oldt{};
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt = oldt;
    newt.c_lflag &= static_cast<tcflag_t>(~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    bool ok = static_cast<bool>(std::getline(std::cin, pw));
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (!ok) throw InputClosed{};
#endif
    std::cout << "\n";
    return pw;
}

int readInt(const std::string& prompt, int min, int max) {
    while (true) {
        std::string t = readLine(prompt);
        try {
            int v = InputValidator::parseInt(t, "Input");
            if (v >= min && v <= max) return v;
        } catch (const AppException&) {}
        error("Please enter a number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
    }
}

double readDouble(const std::string& prompt) {
    while (true) {
        try {
            return InputValidator::parseDouble(readLine(prompt), "Input");
        } catch (const AppException& e) {
            error(e.what());
        }
    }
}

bool confirm(const std::string& prompt) {
    while (true) {
        std::string t = readLine(prompt + " (y/n): ");
        if (t == "y" || t == "Y" || t == "yes") return true;
        if (t == "n" || t == "N" || t == "no") return false;
    }
}

std::string readDateTime(const std::string& prompt) {
    while (true) {
        try {
            return timeutil::localToUtc(readLine(prompt + " [YYYY-MM-DD HH:MM, local time]: "));
        } catch (const AppException& e) {
            error(e.what());
        }
    }
}

void pause() { readLine("\nPress Enter to continue..."); }

void heading(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}
void info(const std::string& msg) { std::cout << msg << "\n"; }
void error(const std::string& msg) { std::cout << "Error: " << msg << "\n"; }

std::string pad(const std::string& s, size_t w) {
    if (s.size() >= w) return s.substr(0, w > 1 ? w - 1 : w) + (w > 1 ? "~" : "");
    return s + std::string(w - s.size(), ' ');
}

int menu(const std::string& title, const std::vector<std::string>& items, const std::string& zeroLabel) {
    heading(title);
    for (size_t i = 0; i < items.size(); ++i) std::cout << "  " << (i + 1) << ". " << items[i] << "\n";
    std::cout << "  0. " << zeroLabel << "\n";
    return readInt("Choose an option: ", 0, static_cast<int>(items.size()));
}

int choose(const std::string& title, const std::vector<std::string>& labels) {
    heading(title);
    if (labels.empty()) {
        info("(nothing to show)");
        return -1;
    }
    for (size_t i = 0; i < labels.size(); ++i) std::cout << "  " << (i + 1) << ". " << labels[i] << "\n";
    std::cout << "  0. Cancel\n";
    return readInt("Choose: ", 0, static_cast<int>(labels.size())) - 1;
}

}  // namespace lms::console
