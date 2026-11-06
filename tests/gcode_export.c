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

static MotionPath metadata(void)
{
    return (MotionPath){.count = SIZE_MAX, .width = 100, .height = 100,
                        .width_mm = 100, .height_mm = 100};
}
static void quantization(void)
{
    MotionPath p = metadata(); MotionPoint point = {99, 99};
    assert(MotionQuantizePoint(&p, (MotionPoint){1.024, 2.076}, &point));
    assert(near(point.x, 1) && near(point.y, 2.1));
    assert(MotionQuantizePoint(&p, (MotionPoint){.0249, .0251}, &point));
    assert(point.x == 0 && near(point.y, .05));
    assert(MotionQuantizePoint(&p, (MotionPoint){.025, .025}, &point));
    assert(near(point.x, .05) && near(point.y, .05));
    p.width = 320; p.height = 200; p.width_mm = 170.15; p.height_mm = 127.6;
    assert(MotionQuantizePoint(&p, (MotionPoint){1, 1}, &point));
    assert(near(point.x, .55) && near(point.y, .65));
    assert(MotionQuantizePoint(&p, (MotionPoint){320, 200}, &point));
    assert(near(point.x, 170.15) && near(point.y, 127.6));
    p.width_mm = 170.17; p.height_mm = 127.62;
    assert(MotionQuantizePoint(&p, (MotionPoint){320, 200}, &point));
    assert(near(point.x, 170.15) && near(point.y, 127.6));
}

static void failedQuantizationPreservesOutput(void)
{
    MotionPath p = metadata(); MotionPoint point = {7, 11};
    const MotionPoint invalid[] = {{-1, 0}, {0, 101}, {NAN, 0}, {0, INFINITY}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
        assert(!MotionQuantizePoint(&p, invalid[i], &point));
    assert(!MotionQuantizePoint(NULL, (MotionPoint){0, 0}, &point));
    assert(!MotionQuantizePoint(&p, (MotionPoint){0, 0}, NULL));
    p.width = 0; assert(!MotionQuantizePoint(&p, (MotionPoint){0, 0}, &point));
    p = metadata(); p.height_mm = NAN;
    assert(!MotionQuantizePoint(&p, (MotionPoint){0, 0}, &point));
    p = metadata(); p.width_mm = DBL_MAX;
    assert(!MotionQuantizePoint(&p, (MotionPoint){0, 0}, &point));
    assert(point.x == 7 && point.y == 11);
}
static char *contents(FILE *stream)
{
    assert(fflush(stream) == 0 && fseek(stream, 0, SEEK_END) == 0);
    long bytes = ftell(stream); assert(bytes >= 0 && bytes < 16000000);
    char *text = calloc((size_t)bytes + 1, 1); assert(text); rewind(stream);
    assert(fread(text, 1, (size_t)bytes, stream) == (size_t)bytes);
    return text;
}

static MotionPath framePath(MotionPoint *points, unsigned char *kinds, size_t count)
{
    MotionPath path = metadata(); path.points = points; path.kinds = kinds; path.count = count;
    return path;
}
static void combinedFramesAndWrapper(void)
{
    MotionPoint a[] = {{.0249, .0251}, {10.0249, 20.0251}, {10.0249, 20.0251}};
    unsigned char ka[] = {MOTION_START, MOTION_OUTLINE, MOTION_OUTLINE};
    MotionPath first = framePath(a, ka, 3); MotionPoint cursor;
    assert(MotionQuantizePoint(&first, a[2], &cursor));
    assert(near(cursor.x, 10) && near(cursor.y, 20.05));
    MotionPoint b[] = {{cursor.x, cursor.y}, {12.076, 23.024}};
    unsigned char kb[] = {MOTION_START, MOTION_CONNECTOR};
    MotionPath second = framePath(b, kb, 2);
    FILE *combined = tmpfile(); assert(combined);
    assert(MotionWriteGCodeFrame(combined, &first, 3600, 1, 0));
    assert(MotionWriteGCodeFrame(combined, &second, 3600, 0, 1));
    CodeReport r = inspectCode(combined);
    assert(r.pauses == 1 && r.ends == 1 && r.sizes == 1 && r.starts == 1 && r.feeds == 2);
    assert(r.moves[0] == 2 && r.moves[1] == 1); /* The zero-distance G1 survives. */
    for (size_t i = 0; i < 4; ++i) assert(r.modes[i] == 1);
    assert(near(r.boundary.x, cursor.x) && near(100 - r.boundary.y, cursor.y));
    assert(near(r.last.x, 12.1) && near(r.last.y, 77)); fclose(combined);
    FILE *old = tmpfile(), *single = tmpfile(); assert(old && single);
    assert(MotionWriteGCode(old, &first, 3600));
    assert(MotionWriteGCodeFrame(single, &first, 3600, 1, 1));
    char *oldText = contents(old), *singleText = contents(single);
    assert(!strcmp(oldText, singleText));
    free(oldText); free(singleText); fclose(old); fclose(single);
}

static void invalidFrameArguments(void)
{
    MotionPoint points[] = {{0, 0}, {1, 1}};
    unsigned char kinds[] = {MOTION_START, MOTION_OUTLINE};
    MotionPath path = framePath(points, kinds, 2);
    FILE *stream = tmpfile(); assert(stream);
    assert(!MotionWriteGCodeFrame(NULL, &path, 3600, 1, 1));
    assert(!MotionWriteGCodeFrame(stream, NULL, 3600, 1, 1));
    assert(!MotionWriteGCodeFrame(stream, &path, NAN, 1, 1));
    assert(!MotionWriteGCodeFrame(stream, &path, 3600, 2, 1));
    assert(!MotionWriteGCodeFrame(stream, &path, 3600, 1, -1));
    assert(ftell(stream) == 0); fclose(stream);
}
static MotionPoint svgStart(const char *filename)
{
    FILE *stream = fopen(filename, "r"); assert(stream);
    char *svg = contents(stream); fclose(stream);
    const char *path = strstr(svg, "<path"); assert(path);
    const char *data = strstr(path, " d="); assert(data); data += 3;
    assert(*data == '\'' || *data == '"'); ++data;
    while (isspace((unsigned char)*data)) ++data;
    assert(*data++ == 'M'); char *end;
    double x = strtod(data, &end); assert(end != data); data = end;
    while (isspace((unsigned char)*data) || *data == ',') ++data;
    double y = strtod(data, &end); assert(end != data);
    free(svg); return (MotionPoint){x, y};
}
