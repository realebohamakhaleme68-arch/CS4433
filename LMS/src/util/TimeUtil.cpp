#include "util/TimeUtil.hpp"

#include <chrono>
#include <cstdio>

#include "util/Exceptions.hpp"

namespace lms::timeutil {
namespace {
std::function<std::string()> g_override;

// Howard Hinnant's civil-date algorithms (portable, no timegm needed).
long long daysFromCivil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}
void civilFromDays(long long z, long long& y, unsigned& m, unsigned& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += (m <= 2);
}
bool validDate(int y, int mo, int d, int h, int mi, int s) {
    if (y < 1970 || y > 2200 || mo < 1 || mo > 12 || d < 1 || h < 0 || h > 23 || mi < 0 || mi > 59 ||
        s < 0 || s > 59)
        return false;
    static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maxd = dim[mo - 1];
    if (mo == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) maxd = 29;
    return d <= maxd;
}
}  // namespace

std::string nowUtc() {
    if (g_override) return g_override();
    using namespace std::chrono;
    return fromEpoch(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}
void setClockOverride(std::function<std::string()> fn) { g_override = std::move(fn); }
void clearClockOverride() { g_override = nullptr; }

long long toEpoch(const std::string& iso) {
    int y, mo, d, h, mi, s;
    char z = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d%c", &y, &mo, &d, &h, &mi, &s, &z) != 7 || z != 'Z' ||
        !validDate(y, mo, d, h, mi, s))
        throw ValidationException("Invalid date/time: " + iso);
    return daysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400LL + h * 3600LL +
           mi * 60LL + s;
}
std::string fromEpoch(long long e) {
    long long days = e >= 0 ? e / 86400 : -((-e + 86399) / 86400);
    long long rem = e - days * 86400;
    long long y;
    unsigned m, d;
    civilFromDays(days, y, m, d);
    char buf[96];
    std::snprintf(buf, sizeof buf, "%04lld-%02u-%02uT%02lld:%02lld:%02lldZ", y, m, d, rem / 3600,
                  (rem % 3600) / 60, rem % 60);
    return buf;
}
std::string addSeconds(const std::string& iso, long long sec) { return fromEpoch(toEpoch(iso) + sec); }

int daysLate(const std::string& submitted, const std::string& deadline) {
    long long delay = toEpoch(submitted) - toEpoch(deadline);
    if (delay <= 0) return 0;
    return static_cast<int>((delay + 86399) / 86400);  // ceil
}

std::string localToUtc(const std::string& local) {
    int y, mo, d, h, mi;
    char extra = 0;
    if (std::sscanf(local.c_str(), "%d-%d-%d %d:%d%c", &y, &mo, &d, &h, &mi, &extra) != 5 ||
        !validDate(y, mo, d, h, mi, 0))
        throw ValidationException("Date must look like 2026-11-07 14:30 (year-month-day hour:minute).");
    long long e = daysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400LL + h * 3600LL +
                  mi * 60LL - kLocalOffsetHours * 3600LL;
    return fromEpoch(e);
}
std::string utcToLocal(const std::string& iso) {
    std::string s = fromEpoch(toEpoch(iso) + kLocalOffsetHours * 3600LL);
    return s.substr(0, 10) + " " + s.substr(11, 5);
}

}  // namespace lms::timeutil
