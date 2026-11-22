#include "framebuffer.h"
#include "test.h"

void test_framebuffer(void)
{
    /* Four pixels plus three guard bytes that must stay untouched. */
    unsigned char buf[4 * 3 + 3];
    int i;

    for (i = 0; i < (int)sizeof buf; i++)
        buf[i] = 0xEE;

    fb_fill_bgr8(buf, 4, 10, 20, 30);

    for (i = 0; i < 4; i++) {
        CHECK_EQ_INT(buf[i * 3 + 0], 30); /* blue first */
        CHECK_EQ_INT(buf[i * 3 + 1], 20); /* then green */
        CHECK_EQ_INT(buf[i * 3 + 2], 10); /* red last */
    }
    for (i = 12; i < 15; i++)
        CHECK_EQ_INT(buf[i], 0xEE); /* nothing written past the end */

    fb_fill_bgr8(buf, 0, 1, 2, 3); /* zero pixels writes nothing */
    CHECK_EQ_INT(buf[0], 30);
}
