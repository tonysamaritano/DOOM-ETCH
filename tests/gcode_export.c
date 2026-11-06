#include "motion.h"
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t moves[2], pauses, ends, modes[4], sizes, starts, feeds;
    double width, height;
    MotionPoint start, last, boundary;
} CodeReport;
static int near(double a, double b) { return fabs(a - b) < 1e-7; }
static double word(const char *line, char name)
{
    const char *at = strchr(line, name); assert(at);
    char *end;
    double value = strtod(at + 1, &end);
    assert(end > at + 1 && isfinite(value)); return value;
}
static void boundedGridPoint(MotionPoint point, const CodeReport *report)
{
    assert(point.x >= 0 && point.x <= report->width + 1e-8);
    assert(point.y >= 0 && point.y <= report->height + 1e-8);
    assert(fabs(point.x / .05 - round(point.x / .05)) < 1e-6);
    double down = report->height - point.y;
    assert(fabs(down / .05 - round(down / .05)) < 1e-6);
}

static CodeReport inspectCode(FILE *stream)
{
    CodeReport r = {0}; char line[512]; rewind(stream);
    while (fgets(line, sizeof(line), stream)) {
        assert(strcspn(line, "\r\n") <= 80);
        const char *s = line; while (isspace((unsigned char)*s)) ++s;
        if (!strncmp(s, "; ETCH_SIZE ", 12)) {
            r.width = word(s, 'X'); r.height = word(s, 'Y'); ++r.sizes;
        } else if (!strncmp(s, "; ETCH_START ", 13)) {
            r.start = (MotionPoint){word(s, 'X'), word(s, 'Y')}; ++r.starts;
            boundedGridPoint(r.start, &r);
        } else if (*s == 'G') {
            char *end; long code = strtol(s + 1, &end, 10); assert(end > s + 1);
            if (code == 1) {
                assert(r.pauses < 2 && r.sizes == 1);
                r.last = (MotionPoint){word(end, 'X'), word(end, 'Y')};
                boundedGridPoint(r.last, &r); ++r.moves[r.pauses];
            } else {
                const int modes[] = {21, 90, 94, 61}; size_t i;
                for (i = 0; i < 4 && modes[i] != code; ++i) {}
                assert(i < 4); ++r.modes[i];
            }
        } else if (*s == 'M') {
            long code = strtol(s + 1, NULL, 10);
            if (code == 0) { assert(r.pauses == 0); r.boundary = r.last; ++r.pauses; }
            else { assert(code == 2); ++r.ends; }
        } else if (*s == 'F') { assert(word(s, 'F') > 0); ++r.feeds; }
        else assert(!*s || *s == ';');
    }
    assert(!ferror(stream)); return r;
}
