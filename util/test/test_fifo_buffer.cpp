#include <gtest/gtest.h>

// fifo_buffer_sync names ChibiOS calls in its template body. The firmware's unit-test build
// defines EFI_UNIT_TEST and stubs them in its own global.h; this standalone build has neither.
#ifndef EFI_UNIT_TEST
#define EFI_UNIT_TEST 1
#define chSysLock() {}
#define chSysUnlock() {}
#define osalThreadDequeueNextI(x, y) {}
#endif

#include "fifo_buffer.h"

TEST(Util_Fifo, firstInFirstOutAcrossTheWrap) {
	fifo_buffer<int, 4> fifo;
	EXPECT_TRUE(fifo.isEmpty());

	EXPECT_TRUE(fifo.put(1));
	EXPECT_TRUE(fifo.put(2));
	EXPECT_TRUE(fifo.put(3));
	EXPECT_EQ(1, fifo.get());

	// the write index wraps here while 2 and 3 are still queued
	EXPECT_TRUE(fifo.put(4));
	EXPECT_TRUE(fifo.put(5));
	EXPECT_TRUE(fifo.isFull());
	EXPECT_FALSE(fifo.put(6)) << "a full FIFO must refuse, not overwrite the oldest";

	EXPECT_EQ(2, fifo.get());
	EXPECT_EQ(3, fifo.get());
	EXPECT_EQ(4, fifo.get());
	EXPECT_EQ(5, fifo.get());
	EXPECT_TRUE(fifo.isEmpty());
	EXPECT_EQ(0, fifo.getCount());
}

TEST(Util_Fifo, getOnEmptyDoesNotMoveTheReadIndex) {
	fifo_buffer<int, 4> fifo;
	fifo.put(7);
	EXPECT_EQ(7, fifo.get());

	// callers must check isEmpty(): an empty get() returns whatever the slot holds
	(void)fifo.get();
	EXPECT_EQ(0, fifo.getCount());

	fifo.put(8);
	EXPECT_EQ(8, fifo.get());
}

TEST(Util_Fifo, multiPutStopsAtTheFirstItemThatDoesNotFit) {
	fifo_buffer<int, 4> fifo;
	int const items[] = { 1, 2, 3, 4, 5, 6 };

	// Not all-or-nothing: the prefix that fits stays queued. ISO-TP's streamAddToTxTimeout()
	// sizes each put to the free space, so a change here must be checked against it.
	EXPECT_FALSE(fifo.put(items, 6));
	EXPECT_EQ(4, fifo.getCount());
	EXPECT_EQ(1, fifo.get());
	EXPECT_EQ(2, fifo.get());

	EXPECT_TRUE(fifo.put(items + 4, 2));
	EXPECT_EQ(3, fifo.get());
	EXPECT_EQ(4, fifo.get());
	EXPECT_EQ(5, fifo.get());
	EXPECT_EQ(6, fifo.get());
}

TEST(Util_Fifo, clearResetsBothEnds) {
	fifo_buffer<int, 4> fifo;
	fifo.put(1);
	fifo.put(2);
	(void)fifo.get();
	fifo.clear();

	EXPECT_TRUE(fifo.isEmpty());
	EXPECT_EQ(0, fifo.currentIndexRead);

	// ISO-TP hands getElements() to the CAN driver as a linear array after a clear()
	fifo.put(9);
	fifo.put(10);
	EXPECT_EQ(9, fifo.getElements()[0]);
	EXPECT_EQ(10, fifo.getElements()[1]);
}

TEST(Util_Fifo, syncGetOnEmptyIsATimeout) {
	fifo_buffer_sync<int, 4> fifo;
	int item = -1;

	// on the ECU an empty get() waits out its timeout and returns false; the unit-test build
	// cannot wait, and used to return true with a stale element instead
	EXPECT_FALSE(fifo.get(item, 0));
	EXPECT_EQ(-1, item);

	EXPECT_TRUE(fifo.put(42));
	EXPECT_TRUE(fifo.get(item, 0));
	EXPECT_EQ(42, item);
	EXPECT_FALSE(fifo.get(item, 0));
}
