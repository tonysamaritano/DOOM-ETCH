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
