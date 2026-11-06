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
