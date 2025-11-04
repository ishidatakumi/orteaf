#include "orteaf/version/version.h"

#include <gtest/gtest.h>

TEST(Version, ConstantsExposeProjectVersion) {
    EXPECT_EQ(orteaf::version::major(), 0);
    EXPECT_EQ(orteaf::version::minor(), 1);
    EXPECT_EQ(orteaf::version::patch(), 0);
    EXPECT_STREQ(orteaf::version::string(), "0.1.0");
}
