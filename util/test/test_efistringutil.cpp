#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include <rusefi/efistringutil.h>

TEST(Util_String, equalsIgnoreCase) {
    ASSERT_FALSE(rusefi::stringutil::strEqualCaseInsensitive("a", "b"));
    ASSERT_TRUE(rusefi::stringutil::strEqualCaseInsensitive("a", "A"));
}

// The parse-error sentinel (ATOI_ERROR_CODE) must surface as NaN, not as a number:
// regression for the absI()==ATOI_ERROR_CODE guards that could never fire.
TEST(Util_String, atoffRejectsNonNumericText) {
    EXPECT_TRUE(std::isnan(rusefi::stringutil::atoff("xyz")));
    EXPECT_TRUE(std::isnan(rusefi::stringutil::atoff(".x")));
    EXPECT_TRUE(std::isnan(rusefi::stringutil::atoff("1.x")));
}

TEST(Util_String, atoffRejectsOverlongText) {
    const std::string tooLong(200, '1');
    EXPECT_TRUE(std::isnan(rusefi::stringutil::atoff(tooLong.c_str())));
}

TEST(Util_String, atoffParsesValidNumbers) {
    EXPECT_FLOAT_EQ(1.0f, rusefi::stringutil::atoff("1.0"));
    EXPECT_FLOAT_EQ(-42.5f, rusefi::stringutil::atoff("-42.5"));
    EXPECT_FLOAT_EQ(-1.5f, rusefi::stringutil::atoff("  -1.5"));
    EXPECT_TRUE(std::isnan(rusefi::stringutil::atoff("nan")));
}
