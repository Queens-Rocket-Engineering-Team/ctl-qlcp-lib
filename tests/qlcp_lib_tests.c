#include <stdint.h>
#include "unity.h"
#include "qlcp_lib.h"

void setUp() {
    1;
}

void tearDown() {
    1;
}

void test_dummy() {
    TEST_ASSERT_EQUAL(1, 1);
}

int32_t main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_dummy);

    UNITY_END();

    return 0;
}