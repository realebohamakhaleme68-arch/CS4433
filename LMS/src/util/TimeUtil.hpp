// All times are stored as UTC ISO-8601 text, e.g. 2026-11-07T14:30:00Z.
// Users type and see school local time (Africa/Maseru, UTC+2, no DST).
#pragma once
#include <functional>
#include <string>

namespace lms::timeutil {

constexpr int kLocalOffsetHours = 2;

std::string nowUtc();
void setClockOverride(std::function<std::string()> fn);  // for tests
void clearClockOverride();

long long toEpoch(const std::string& isoUtc);   // throws ValidationException
std::string fromEpoch(long long epoch);
std::string addSeconds(const std::string& isoUtc, long long seconds);

// 0 if submitted <= effectiveDeadline, else ceil(delay / 24h)  (Section 3.1)
int daysLate(const std::string& submittedUtc, const std::string& effectiveDeadlineUtc);

// "YYYY-MM-DD HH:MM" local <-> UTC ISO text
std::string localToUtc(const std::string& local);   // throws ValidationException
std::string utcToLocal(const std::string& isoUtc);

}  // namespace lms::timeutil
