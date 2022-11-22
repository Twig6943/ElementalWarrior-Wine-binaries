
#include "wine/test.h"

static void test_main(void)
{
    trace("was this run \n");
}

START_TEST(dxcore)
{
    test_main();
}