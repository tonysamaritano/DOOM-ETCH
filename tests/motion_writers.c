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

static size_t svgPoints(const char *svg, MotionPoint *points)
{
    const char *path = strstr(svg, "<path");
    assert(path && !strstr(path + 5, "<path"));
    char data[8192]; attribute(path, "d", data, sizeof(data));
    char *cursor = data;
    size_t count = 0;
    while (*cursor) {
        while (isspace((unsigned char)*cursor)) ++cursor;
        if (!*cursor) break;
        assert(*cursor++ == (count ? 'L' : 'M'));
        assert(count < 16);
        char *end;
        points[count].x = strtod(cursor, &end); assert(end != cursor); cursor = end;
        while (isspace((unsigned char)*cursor) || *cursor == ',') ++cursor;
        points[count].y = strtod(cursor, &end); assert(end != cursor); cursor = end;
        assert(isfinite(points[count].x) && isfinite(points[count].y));
        ++count;
    }
    return count;
}
static void physicalPoints(const MotionPath *path, const MotionPoint *actual, size_t count)
{
    assert(count == path->count);
    for (size_t i = 0; i < count; ++i) {
        assert(fabs(actual[i].x - path->points[i].x * 170.15 / 320) < 0.00001);
        assert(fabs(actual[i].y - path->points[i].y * 127.6 / 200) < 0.00001);
    }
}

static void connectorStyle(const char *svg, const MotionPoint *points)
{
    char outline[64], connector[64], value[64];
    attribute(strstr(svg, "<path"), "stroke", outline, sizeof(outline));
    const char *line = strstr(svg, "<line");
    assert(line && !strstr(line + 5, "<line"));
    attribute(line, "stroke", connector, sizeof(connector));
    assert(strcmp(outline, connector) && strcmp(connector, "none"));
    const char *names[] = {"x1", "y1", "x2", "y2"};
    double expected[] = {points[1].x, points[1].y, points[2].x, points[2].y};
    for (int i = 0; i < 4; ++i) {
        attribute(line, names[i], value, sizeof(value));
        assert(fabs(strtod(value, NULL) - expected[i]) < 0.00001);
    }
}
static void assertGridPoint(MotionPoint point)
{
    assert(point.x >= 0 && point.x <= 170.15 && point.y >= 0 && point.y <= 127.6);
    assert(fabs(point.x / 0.05 - round(point.x / 0.05)) < 1e-7);
    assert(fabs(point.y / 0.05 - round(point.y / 0.05)) < 1e-7);
}
