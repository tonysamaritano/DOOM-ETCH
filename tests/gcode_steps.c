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

int main(int argc, char **argv)
{
    assert(argc == 2); FILE *f = fopen(argv[1], "r"); assert(f);
    Counts c = {0}; char line[256]; int start = 0;
    while (fgets(line, sizeof line, f)) {
        double x, y;
        if (sscanf(line, "; ETCH_START X%lf Y%lf", &x, &y) == 2) {
            assert(!start++ && !c.moves);
            c.last[0] = llround(x * 20); c.last[1] = llround(y * 20);
        } else if (sscanf(line, "G1 X%lf Y%lf", &x, &y) == 2) {
            assert(start); endpoint(&c, x, y);
        } else if (!strncmp(line, "M0", 2)) ++c.erasures;
    }
    assert(!ferror(f) && start && c.moves); fclose(f);
    printf("{\"moves\":%" PRIu64 ",\"erases\":%" PRIu64
        ",\"steps_x\":%" PRIu64 ",\"steps_y\":%" PRIu64
        ",\"reversals_x\":%" PRIu64 ",\"reversals_y\":%" PRIu64 "}\n",
        c.moves, c.erasures, c.steps[0], c.steps[1], c.reversals[0], c.reversals[1]);
    return 0;
}
