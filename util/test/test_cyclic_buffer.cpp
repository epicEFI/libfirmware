#include <gtest/gtest.h>

#include "cyclic_buffer.h"
#include "rusefi/true_false.h"

TEST(util, cyclicBufferContains) {
	cyclic_buffer<int> sb;
	sb.add(10);
	ASSERT_EQ(TRUE, sb.contains(10));
	ASSERT_EQ(FALSE, sb.contains(11));
}

TEST(util, cyclicBuffer) {
	cyclic_buffer<int> sb;

	{
		sb.add(10);

		ASSERT_EQ(10, sb.sum(3));

		sb.add(2);
		ASSERT_EQ(12, sb.sum(2));
	}
	{
		sb.clear();

		sb.add(1);
		sb.add(2);
		sb.add(3);
		sb.add(4);

		ASSERT_EQ(4, sb.maxValue(3));
		ASSERT_EQ(4, sb.maxValue(113));
		ASSERT_EQ( 2,  sb.minValue(3)) << "minValue(3)";
		ASSERT_EQ(1, sb.minValue(113));
	}
}

TEST(util, cyclicBufferContainsAfterWrap) {
	cyclic_buffer<int, 4> sb;
	for (int i = 1; i <= 6; i++) {
		sb.add(i);
	}
	// slots now hold 5 6 3 4 and currentIndex is 2 - the old scan stopped there and missed 3 and 4
	EXPECT_TRUE(sb.contains(3));
	EXPECT_TRUE(sb.contains(4));
	EXPECT_TRUE(sb.contains(5));
	EXPECT_TRUE(sb.contains(6));
	EXPECT_FALSE(sb.contains(1)) << "overwritten";
	EXPECT_FALSE(sb.contains(0)) << "cleared slots must not match before they are written";

	sb.clear();
	sb.add(0);
	EXPECT_TRUE(sb.contains(0));
}

TEST(util, cyclicBufferNeverCountsAnElementTwice) {
	cyclic_buffer<int, 4> sb;
	for (int i = 1; i <= 10; i++) {
		sb.add(i);
	}
	// holds 7 8 9 10; asking for more than one lap used to wrap round and add them again
	EXPECT_EQ(34, sb.sum(4));
	EXPECT_EQ(34, sb.sum(8));
	EXPECT_EQ(34, sb.sum(1000));
	EXPECT_EQ(10, sb.maxValue(1000));
	EXPECT_EQ(7, sb.minValue(1000));

	// fewer elements than slots: only what was added
	cyclic_buffer<int, 4> partial;
	partial.add(5);
	partial.add(6);
	EXPECT_EQ(11, partial.sum(4));
	EXPECT_EQ(5, partial.minValue(4));
}

TEST(util, cyclicBufferSizeZeroIsClampedToOne) {
	// a size of 0 used to let add() walk straight off the end of elements[]
	cyclic_buffer<int, 4> sb(0);
	EXPECT_EQ(1, sb.getSize());
	for (int i = 0; i < 10; i++) {
		sb.add(i);
		EXPECT_LT(sb.currentIndex, 4);
	}
	EXPECT_EQ(9, sb.get(0));
	EXPECT_EQ(9, sb.sum(10));

	sb.setSize(0);
	EXPECT_EQ(1, sb.getSize());
}
