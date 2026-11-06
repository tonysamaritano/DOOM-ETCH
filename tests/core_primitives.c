#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomtype.h"
#include "m_bbox.h"
#include "m_fixed.h"
#include "m_random.h"
#include "m_swap.h"

void I_Error(char *message, ...)
{
    fprintf(stderr, "%s\n", message);
    abort();
}

static void fixedPoint(void)
{
    assert(FixedMul(3 * FRACUNIT, -FRACUNIT / 2) == -3 * (FRACUNIT / 2));
    assert(FixedDiv(3 * FRACUNIT, 2 * FRACUNIT) == 3 * (FRACUNIT / 2));
    assert(FixedDiv(INT_MIN, INT_MIN) == FRACUNIT);
    assert(FixedDiv(INT_MIN, FRACUNIT) == INT_MIN);
    assert(FixedDiv(FRACUNIT, 0) == INT_MAX);
}

static void randomStreams(void)
{
    int firstCycle[256];
    M_ClearRandom();
    for (int i = 0; i < 256; i++)
        firstCycle[i] = P_Random();
    assert(P_Random() == firstCycle[0]);
    M_ClearRandom();
    for (int i = 0; i < 256; i++) {
        assert(M_Random() == firstCycle[i]);
        if (i % 2 == 0)
            assert(P_Random() == firstCycle[i / 2]);
    }
    assert(firstCycle[0] != firstCycle[1]);
}

static void bounds(void)
{
    fixed_t box[4];
    M_ClearBox(box);
    M_AddToBox(box, 12, -3);
    assert(box[BOXLEFT] == 12 && box[BOXRIGHT] == 12);
    assert(box[BOXTOP] == -3 && box[BOXBOTTOM] == -3);
    M_AddToBox(box, -9, 4);
    assert(box[BOXLEFT] == -9 && box[BOXRIGHT] == 12);
    assert(box[BOXTOP] == 4 && box[BOXBOTTOM] == -3);
}

static void endian(void)
{
    const unsigned char disk[] = {0x42, 0x18, 0x75, 0x23};
    unsigned short word;
    unsigned int value;
    memcpy(&word, disk, sizeof(word));
    memcpy(&value, disk, sizeof(value));
    assert(SwapLONG(0x12345678) == 0x78563412);
    assert(SwapSHORT(0xcabd) == 0xbdca);
    assert(SHORT(word) == 0x1842);
    assert(LONG(value) == 0x23751842);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "fixed")) fixedPoint();
    else if (!strcmp(argv[1], "boxes")) bounds();
    else if (!strcmp(argv[1], "random")) randomStreams();
    else if (!strcmp(argv[1], "endian")) endian();
    else return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
