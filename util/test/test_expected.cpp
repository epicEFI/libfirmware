#include <rusefi/expected.h>

#include <gtest/gtest.h>
#include <cmath>

// expected(unexpected_t) used to initialize only the 1-byte Code member of the
// union with Value, leaving the upper bytes of Value indeterminate - and those
// indeterminate bytes were then COPIED by assignment (m_result = unexpected).
// A sensor that set a valid value and later invalidated could therefore print
// its stale float through showInfo's "%.2f" of an invalid result (field symptom:
// "StoredValue Sensor: valid: No, value: 70.00" for garbage 70).

TEST(Util_Expected, invalidAfterValidDoesNotKeepStaleBytes) {
	expected<float> e{70.0f};
	ASSERT_TRUE(e.Valid);
	ASSERT_EQ(e.Value, 70.0f);

	e = unexpected;

	EXPECT_FALSE(e.Valid);
	EXPECT_EQ(e.Code, UnexpectedCode::Unknown);
	// zero bytes, not a float whose upper bytes are 70.0f's upper bytes
	EXPECT_EQ(e.Value, 0.0f);
}

TEST(Util_Expected, errorCtorLeavesNoStaleBytes) {
	expected<float> e{70.0f};

	e = expected<float>(UnexpectedCode::Timeout);

	EXPECT_FALSE(e.Valid);
	EXPECT_EQ(e.Code, UnexpectedCode::Timeout);
	// Code aliases byte 0, so the aliased float reads as a tiny denormal, not
	// stale data; what matters is that no significant stale bits survive and
	// "%.2f" prints 0.00
	EXPECT_LT(std::fabs(e.Value), 1e-30f);
}
