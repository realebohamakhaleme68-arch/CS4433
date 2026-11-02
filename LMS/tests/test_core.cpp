#include <limits>
#include <gtest/gtest.h>

#include "domain/UserFactory.hpp"
#include "service/PenaltyStrategy.hpp"
#include "util/Exceptions.hpp"
#include "util/InputValidator.hpp"
#include "util/PasswordHasher.hpp"
#include "util/TimeUtil.hpp"

using namespace lms;

TEST(PenaltyStrategy, StandardDeductionUsesMaximumMarkAndStartedDays) {
    StandardDeductionStrategy strategy;
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(0, 100), 0.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(-1, 100), 0.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(1, 100), 10.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(2, 100), 20.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(3, 100), 30.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(4, 100), 100.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(10, 50), 50.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(1, 40), 4.0);
}

TEST(PenaltyStrategy, NoPenaltyStrategyAlwaysReturnsZero) {
    NoPenaltyStrategy strategy;
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(0, 100), 0.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(3, 100), 0.0);
    EXPECT_DOUBLE_EQ(strategy.calculatePenalty(9, 50), 0.0);
}

TEST(InputValidator, PasswordEquivalencePartitions) {
    EXPECT_TRUE(InputValidator::isStrongPassword("Abcdef1!"));
    EXPECT_FALSE(InputValidator::isStrongPassword("abcdefgh"));
    EXPECT_FALSE(InputValidator::isStrongPassword("12345678!"));
    EXPECT_FALSE(InputValidator::isStrongPassword("Ab1!"));
    EXPECT_FALSE(InputValidator::isStrongPassword(std::string(129, 'A') + "1!"));
}

TEST(InputValidator, UsernameBoundariesAndInvalidCharacters) {
    EXPECT_FALSE(InputValidator::isValidUsername("ab"));
    EXPECT_TRUE(InputValidator::isValidUsername("abc"));
    EXPECT_TRUE(InputValidator::isValidUsername("john.doe_1"));
    EXPECT_TRUE(InputValidator::isValidUsername(std::string(32, 'a')));
    EXPECT_FALSE(InputValidator::isValidUsername(std::string(33, 'a')));
    EXPECT_FALSE(InputValidator::isValidUsername("bad name"));
    EXPECT_FALSE(InputValidator::isValidUsername("x'; DROP TABLE Users;--"));
}

TEST(InputValidator, MarkBoundaryAndNonFiniteValues) {
    EXPECT_NO_THROW(InputValidator::requireMark(0, 100));
    EXPECT_NO_THROW(InputValidator::requireMark(100, 100));
    EXPECT_NO_THROW(InputValidator::requireMark(50.5, 100));
    EXPECT_THROW(InputValidator::requireMark(-0.01, 100), ValidationException);
    EXPECT_THROW(InputValidator::requireMark(100.01, 100), ValidationException);
    EXPECT_THROW(InputValidator::requireMark(std::numeric_limits<double>::quiet_NaN(), 100), ValidationException);
}

TEST(InputValidator, ParsingRejectsTrailingAndNonNumericCharacters) {
    EXPECT_EQ(InputValidator::parseInt("800", "count"), 800);
    EXPECT_THROW(InputValidator::parseInt("8x", "count"), ValidationException);
    EXPECT_DOUBLE_EQ(InputValidator::parseDouble("12.5", "mark"), 12.5);
    EXPECT_THROW(InputValidator::parseDouble("12.5x", "mark"), ValidationException);
    EXPECT_THROW(InputValidator::requireNonEmpty("  \t", "title"), ValidationException);
}

TEST(UserFactory, CreatesAllFourPolymorphicRolesWithExpectedPermissions) {
    auto admin = UserFactory::create(Role::Admin, 1, "admin", "Admin", "", std::nullopt, true, "now");
    auto lecturer = UserFactory::create(Role::Lecturer, 2, "lecturer", "Lecturer", "E2", 7, true, "now");
    auto hod = UserFactory::create(Role::HoD, 3, "hod", "HoD", "E3", 7, true, "now");
    auto student = UserFactory::create(Role::Student, 4, "student", "Student", "S4", std::nullopt, true, "now");

    EXPECT_EQ(admin->roleName(), "Admin");
    EXPECT_TRUE(admin->can(Permission::MANAGE_USERS));
    EXPECT_EQ(lecturer->roleName(), "Lecturer");
    EXPECT_TRUE(lecturer->can(Permission::MARK_SUBMISSION));
    EXPECT_EQ(lecturer->departmentId(), 7);
    EXPECT_EQ(hod->roleName(), "HoD");
    EXPECT_TRUE(hod->can(Permission::MODERATE_MARKS));
    EXPECT_EQ(student->roleName(), "Student");
    EXPECT_TRUE(student->can(Permission::SUBMIT_WORK));
    EXPECT_FALSE(student->can(Permission::MANAGE_USERS));
}

TEST(UserFactory, DepartmentRolesRequireDepartment) {
    EXPECT_THROW(UserFactory::create(Role::Lecturer, 2, "lecturer", "Lecturer", "E2", std::nullopt, true, "now"),
                 ValidationException);
    EXPECT_THROW(UserFactory::create(Role::HoD, 3, "hod", "HoD", "E3", std::nullopt, true, "now"),
                 ValidationException);
}

TEST(PasswordHasher, SaltedHashVerifiesWithoutStoringPlainText) {
    const std::string password = "Secret#123";
    const std::string first = PasswordHasher::hashPassword(password);
    const std::string second = PasswordHasher::hashPassword(password);
    EXPECT_EQ(first.find(password), std::string::npos);
    EXPECT_TRUE(PasswordHasher::verifyPassword(first, password));
    EXPECT_FALSE(PasswordHasher::verifyPassword(first, "secret#123"));
    EXPECT_NE(first, second);
    EXPECT_FALSE(PasswordHasher::verifyPassword("!", "anything"));
}
