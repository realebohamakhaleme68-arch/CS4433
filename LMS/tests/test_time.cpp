#include <gtest/gtest.h>

#include "util/Exceptions.hpp"
#include "util/TimeUtil.hpp"


using namespace lms;

struct DaysLateCase {
    const char* submitted;
    int expected;
};

class DaysLateTest : public ::testing::TestWithParam<DaysLateCase> {};

TEST_P(DaysLateTest, MatchesBoundaryTable) {
    const std::string deadline = "2026-11-07T12:00:00Z";
    EXPECT_EQ(timeutil::daysLate(GetParam().submitted, deadline), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(UT01, DaysLateTest,
                         ::testing::Values(DaysLateCase{"2026-11-07T12:00:00Z", 0},  // exactly at the deadline
                                           DaysLateCase{"2026-11-07T12:00:01Z", 1},  // +1 s
                                           DaysLateCase{"2026-11-08T12:00:00Z", 1},  // +24 h
                                           DaysLateCase{"2026-11-08T12:00:01Z", 2},  // +24 h 1 s
                                           DaysLateCase{"2026-11-09T12:00:01Z", 3},  // +48 h 1 s
                                           DaysLateCase{"2026-11-10T12:00:00Z", 3},  // +72 h
                                           DaysLateCase{"2026-11-10T12:00:01Z", 4}   // +72 h 1 s
                                           ));

TEST(TimeUtil, InvalidLocalDateThrows) {  // UT-04
    EXPECT_THROW(timeutil::localToUtc("2026-13-45 99:99"), ValidationException);
}