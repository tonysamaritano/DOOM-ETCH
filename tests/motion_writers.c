#include "motion.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int MotionWriteSVG(FILE *, const MotionPath *);
int MotionWriteGCode(FILE *, const MotionPath *, double);
static MotionPoint writerPoints[] = {
    {0.04, 0.04}, {160.05, 0.04}, {320, 200}, {0.000001, 200}, {320, 0}
};
static unsigned char writerKinds[] = {
    MOTION_START, MOTION_OUTLINE, MOTION_CONNECTOR, MOTION_OUTLINE, MOTION_OUTLINE
};
static MotionPath writerFixture(void)
{
    return (MotionPath){.points = writerPoints, .kinds = writerKinds, .count = 5,
        .width = 320, .height = 200, .width_mm = 170.15, .height_mm = 127.6};
}
static char *readOutput(FILE *stream)
{
    assert(fflush(stream) == 0);
    long bytes = ftell(stream);
    assert(bytes >= 0 && bytes < 65536);
    char *text = calloc((size_t)bytes + 1, 1);
    assert(text); rewind(stream);
    assert(fread(text, 1, (size_t)bytes, stream) == (size_t)bytes);
    return text;
}

/* Read one quoted XML attribute without depending on attribute order. */
static void attribute(const char *element, const char *name, char *value, size_t size)
{
    const char *end = strchr(element, '>');
    assert(end);
    const char *found = element;
    for (;;) {
        found = strstr(found, name);
        assert(found && found < end);
        if (found > element && isspace((unsigned char)found[-1])) break;
        ++found;
    }
    found += strlen(name);
    while (isspace((unsigned char)*found)) ++found;
    assert(*found++ == '=');
    while (isspace((unsigned char)*found)) ++found;
    char quote = *found++;
    assert(quote == '\'' || quote == '"');
    const char *last = strchr(found, quote);
    assert(last && last < end && (size_t)(last - found) < size);
    memcpy(value, found, (size_t)(last - found));
    value[last - found] = 0;
}
static double coordinate(const char *text, char axis)
{
    const char *field = strchr(text, axis);
    assert(field);
    char *end;
    double value = strtod(field + 1, &end);
    assert(end > field + 1 && isfinite(value));
    return value;
}
