#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtest/gtest.h>

extern "C" {
#include "mm_thread.h"
#include "debug.h"
}

TEST(Hoard, PageSize) {
    ASSERT_EQ(4096, getpagesize());
}

TEST(Hoard, Processors) {
    TRACE("# of Processors: %d", getNumProcessors());
}