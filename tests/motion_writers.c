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
        const char *after = found + strlen(name);
        while (isspace((unsigned char)*after)) ++after;
        if (found > element && isspace((unsigned char)found[-1]) && *after == '=') break;
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

typedef struct {
    MotionPoint points[16];
    size_t count;
    int metric, absolute, sizeSeen, startSeen;
    double feed;
} ParsedCode;
static ParsedCode parseCode(char *text)
{
    ParsedCode code = {0};
    for (char *line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        while (isspace((unsigned char)*line)) ++line;
        if (*line == ';') {
            if (strstr(line, "ETCH_SIZE")) {
                assert(fabs(coordinate(line, 'X') - 170.15) < 1e-7);
                assert(fabs(coordinate(line, 'Y') - 127.6) < 1e-7);
                ++code.sizeSeen;
            }
            if (strstr(line, "ETCH_START")) {
                code.points[0] = (MotionPoint){coordinate(line, 'X'), coordinate(line, 'Y')};
                code.count = 1; ++code.startSeen;
            }
            continue;
        }
        char *feed = strchr(line, 'F');
        if (feed) code.feed = strtod(feed + 1, NULL);
        if (*line != 'G') continue;
        long command = strtol(line + 1, NULL, 10);
        if (command == 21) ++code.metric;
        if (command == 90) ++code.absolute;
        if (command == 1) {
            assert(code.startSeen && code.count < 16);
            code.points[code.count++] = (MotionPoint){coordinate(line, 'X'), coordinate(line, 'Y')};
        } else assert(command == 21 || command == 90 || command == 94 || command == 61);
    }
    return code;
}

static void sameTrace(const MotionPoint *svg, const ParsedCode *code, size_t count)
{
    assert(code->sizeSeen == 1 && code->startSeen == 1);
    assert(code->metric == 1 && code->absolute == 1);
    assert(fabs(code->feed - 1234.5) < 1e-7);
    assert(code->count == count);
    for (size_t i = 0; i < count; ++i) {
        MotionPoint machine = code->points[i];
        assertGridPoint(machine);
        assert(fabs(machine.x - svg[i].x) <= 0.05);
        assert(fabs(machine.y - (127.6 - svg[i].y)) <= 0.05);
        if (machine.x == 0) assert(!signbit(machine.x));
        if (machine.y == 0) assert(!signbit(machine.y));
    }
}
static void svgCanvas(const char *svg)
{
    char box[128];
    attribute(strstr(svg, "<svg"), "viewBox", box, sizeof(box));
    const double expected[] = {0, 0, 170.15, 127.6};
    char *cursor = box;
    for (int i = 0; i < 4; ++i) {
        while (isspace((unsigned char)*cursor) || *cursor == ',') ++cursor;
        char *end;
        double actual = strtod(cursor, &end);
        assert(end != cursor && fabs(actual - expected[i]) < 1e-7);
        cursor = end;
    }
    while (isspace((unsigned char)*cursor)) ++cursor;
    assert(!*cursor);
}
static int containsIgnoringCase(const char *text, const char *word)
{
    for (; *text; ++text) {
        size_t i = 0;
        while (text[i] && word[i] &&
               tolower((unsigned char)text[i]) == tolower((unsigned char)word[i])) ++i;
        if (!word[i]) return 1;
    }
    return 0;
}
static void emittedFormatsAgree(void)
{
    MotionPath path = writerFixture();
    FILE *svgStream = tmpfile(), *codeStream = tmpfile();
    assert(svgStream && codeStream);
    assert(MotionWriteSVG(svgStream, &path));
    assert(MotionWriteGCode(codeStream, &path, 1234.5));
    char *svg = readOutput(svgStream), *gcode = readOutput(codeStream);
    MotionPoint points[16];
    size_t count = svgPoints(svg, points);
    svgCanvas(svg);
    physicalPoints(&path, points, count);
    connectorStyle(svg, points);
    assert(containsIgnoringCase(gcode, "assum") && strstr(gcode, "0.05"));
    ParsedCode code = parseCode(gcode);
    sameTrace(points, &code, count);
    free(svg); free(gcode);
    fclose(svgStream); fclose(codeStream);
}

static void writerArgumentFailures(void)
{
    MotionPath valid = writerFixture(), invalid = {0};
    FILE *stream = tmpfile(); assert(stream);
    assert(!MotionWriteSVG(NULL, &valid));
    assert(!MotionWriteSVG(stream, NULL));
    assert(!MotionWriteSVG(stream, &invalid));
    assert(!MotionWriteGCode(NULL, &valid, 100));
    assert(!MotionWriteGCode(stream, NULL, 100));
    assert(!MotionWriteGCode(stream, &invalid, 100));
    const double badFeed[] = {0, -10, NAN, INFINITY};
    for (size_t i = 0; i < sizeof(badFeed) / sizeof(*badFeed); ++i)
        assert(!MotionWriteGCode(stream, &valid, badFeed[i]));
    assert(ftell(stream) == 0);
    fclose(stream);
}
static void startOnlyWriters(void)
{
    MotionPath path = writerFixture(); path.count = 1;
    FILE *svgStream = tmpfile(), *codeStream = tmpfile();
    assert(svgStream && codeStream);
    assert(MotionWriteSVG(svgStream, &path));
    assert(MotionWriteGCode(codeStream, &path, 1234.5));
    char *svg = readOutput(svgStream), *gcode = readOutput(codeStream);
    MotionPoint points[16];
    assert(svgPoints(svg, points) == 1);
    assert(!strstr(svg, "<line"));
    ParsedCode code = parseCode(gcode);
    sameTrace(points, &code, 1);
    free(svg); free(gcode);
    fclose(svgStream); fclose(codeStream);
}
void MotionWriterTests(void)
{
    emittedFormatsAgree();
    startOnlyWriters();
    writerArgumentFailures();
}
