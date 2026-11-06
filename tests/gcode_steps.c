#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int64_t last[2]; int direction[2];
    uint64_t steps[2], reversals[2], moves, erasures;
} Counts;

static void endpoint(Counts *c, double x, double y)
{
    double values[] = {x, y};
    for (int axis = 0; axis < 2; ++axis) {
        int64_t next = llround(values[axis] * 20); /* Explicit default steps/mm. */
        int64_t delta = next - c->last[axis];
        c->steps[axis] += (uint64_t)llabs(delta);
        int direction = (delta > 0) - (delta < 0);
        if (direction) {
            c->reversals[axis] += c->direction[axis] && direction != c->direction[axis];
            c->direction[axis] = direction;
        }
        c->last[axis] = next;
    }
    ++c->moves;
}
