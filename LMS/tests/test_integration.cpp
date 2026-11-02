// Reuse the project's existing comprehensive in-memory SQLite workflow as a
// GoogleTest integration test while retaining its standalone legacy runner.
#define main lms_legacy_test_main
#include "test_main.cpp"
#undef main

#include <gtest/gtest.h>

TEST(LMSIntegration, FullWorkflowAndPersistenceRules) {
    EXPECT_EQ(lms_legacy_test_main(), 0);
}
