#pragma once
#include <string>

namespace lms {
// Wires the layers together (the only place concrete classes are chosen, DIP)
// and runs the login loop. Returns the process exit code.
int runApplication(const std::string& dbPath);
}
