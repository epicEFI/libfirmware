/**
 * @file test_timer.cpp
 *
 */

#include <rusefi/timer.h>
#include <rusefi/rusefi_time_math.h>

#include <gtest/gtest.h>

#include <limits>

// see mock/lib-time-mocks.cpp
void setTimeNowNt(efitick_t nt);

TEST(util, timerResetStampedInTheFuture) {
	Timer timer;

	efitick_t nowNt = USF2NT((efitick_t)10'000'000); // 10 seconds of uptime
	setTimeNowNt(nowNt);

	// sanity: reset stamped 100us in the past, nothing has elapsed yet
	timer.reset(nowNt - USF2NT((efitick_t)100));
	EXPECT_FALSE(timer.hasElapsedMs(250));

	// ISR stamps the reset 100us AFTER the checking thread sampled "now"
	timer.reset(nowNt + USF2NT((efitick_t)100));

	// getElapsedNt() has a negative-delta guard and correctly clamps to zero
	EXPECT_EQ(0u, timer.getElapsedNt(nowNt));

	// hasElapsedUs() must not wrap the negative delta into "elapsed": a freshly-reset
	// timer has not elapsed, no matter which side of the reset "now" was sampled on
	EXPECT_FALSE(timer.hasElapsedMs(250));

	setTimeNowNt(0);
}

// US_TO_NT_MULTIPLIER is 100 on this host clock (mock/global.h), so 2^32 ticks is ~42.9 s here
// and ~1074 s on STM32 (multiplier 4). Thresholds past that used to be cast to uint32_t, which
// wraps on x86 (a 60 s timer fired at 17.05 s) and saturates on ARM (anything between ~1074 s
// and ~4295 s fired at ~1074 s).
static void expectElapsesAt(float seconds) {
	Timer timer;
	efitick_t const base = USF2NT((efitick_t)10'000'000);
	timer.reset(base);

	float const earlyUs = seconds * 1e6f * 0.999f;
	float const lateUs = seconds * 1e6f * 1.001f;

	setTimeNowNt(base + static_cast<efitick_t>(USF2NT((double)earlyUs)));
	EXPECT_FALSE(timer.hasElapsedSec(seconds)) << seconds << " s fired early";

	setTimeNowNt(base + static_cast<efitick_t>(USF2NT((double)lateUs)));
	EXPECT_TRUE(timer.hasElapsedSec(seconds)) << seconds << " s never fired";

	setTimeNowNt(0);
}

TEST(util, timerBelow32BitTicks) {
	expectElapsesAt(1);
	expectElapsesAt(40);
}

TEST(util, timerAbove32BitTicks) {
	expectElapsesAt(45);
	expectElapsesAt(60);
	expectElapsesAt(1200);
	expectElapsesAt(3600);
	expectElapsesAt(86400);
}

TEST(util, timerEdgeThresholds) {
	Timer timer;
	setTimeNowNt(USF2NT((efitick_t)10'000'000));
	timer.reset();

	// zero, negative and NaN: already elapsed
	EXPECT_TRUE(timer.hasElapsedUs(0));
	EXPECT_TRUE(timer.hasElapsedUs(-1));
	EXPECT_TRUE(timer.hasElapsedUs(std::numeric_limits<float>::quiet_NaN()));

	// beyond any tick count: never elapses, not even for a never-reset timer
	EXPECT_FALSE(timer.hasElapsedUs(std::numeric_limits<float>::infinity()));
	EXPECT_FALSE(timer.hasElapsedUs(std::numeric_limits<float>::max()));
	Timer neverReset;
	EXPECT_FALSE(neverReset.hasElapsedUs(std::numeric_limits<float>::infinity()));

	// a never-reset timer is far in the past, so even a long interval has elapsed
	EXPECT_TRUE(neverReset.hasElapsedSec(3600));

	setTimeNowNt(0);
}
