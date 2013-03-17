#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtest/gtest.h>

#include "debug.h"

TEST(Hoard, PageSize) {
    ASSERT_EQ(4096, getpagesize());
}
