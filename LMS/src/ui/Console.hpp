// Console helpers: all keyboard input and screen formatting live here so the
// controllers stay thin (design Presentation tier).
#pragma once
#include <string>
#include <vector>

namespace lms::console {

struct InputClosed {};  // thrown when stdin reaches end-of-file
struct Cancelled {};    // user chose "0. Cancel": shown as a friendly message, not an error
std::string readLine(const std::string& prompt);
std::string readPassword(const std::string& prompt);       // no echo when stdin is a terminal
int readInt(const std::string& prompt, int min, int max);  // repeats until valid
double readDouble(const std::string& prompt);
bool confirm(const std::string& prompt);              // y / n
std::string readDateTime(const std::string& prompt);  // returns UTC ISO text
void pause();

void heading(const std::string& title);
void info(const std::string& msg);
void error(const std::string& msg);
std::string pad(const std::string& s, size_t width);
// Shows a numbered menu. Returns 1..n, or 0 for "back / logout".
int menu(const std::string& title, const std::vector<std::string>& items, const std::string& zeroLabel = "Back");
// Shows numbered labels and returns the 0-based index, or -1 if the user cancels with 0.
int choose(const std::string& title, const std::vector<std::string>& labels);

}  // namespace lms::console
