#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *readSVG(const char *filename)
{
    FILE *stream = fopen(filename, "rb"); assert(stream);
    assert(fseek(stream, 0, SEEK_END) == 0);
    long bytes = ftell(stream); assert(bytes > 0 && bytes < 16000000);
    rewind(stream);
    char *text = calloc((size_t)bytes + 1, 1); assert(text);
    assert(fread(text, 1, (size_t)bytes, stream) == (size_t)bytes);
    assert(fclose(stream) == 0);
    return text;
}
static const char *attribute(const char *tag, const char *name)
{
    const char *end = strchr(tag, '>'); assert(end);
    for (const char *found = tag; (found = strstr(found, name)); ++found) {
        const char *after = found + strlen(name);
        while (isspace((unsigned char)*after)) ++after;
        if (found > tag && isspace((unsigned char)found[-1]) && *after == '=') {
            assert(found < end); ++after;
            while (isspace((unsigned char)*after)) ++after;
            assert(*after == '"' || *after == '\''); return after + 1;
        }
    }
    assert(!"Missing SVG attribute"); return NULL;
}

static double number(const char **text)
{
    while (isspace((unsigned char)**text) || **text == ',') ++*text;
    char *end;
    double value = strtod(*text, &end);
    assert(end != *text && isfinite(value));
    *text = end;
    return value;
}
static size_t validatePath(const char *text)
{
    const char *path = strstr(text, "<path");
    assert(path && !strstr(path + 5, "<path"));
    const char *data = attribute(path, "d");
    char quote = data[-1];
    size_t points = 0;
    for (;;) {
        while (isspace((unsigned char)*data)) ++data;
        if (*data == quote) break;
        assert(*data++ == (points ? 'L' : 'M'));
        double x = number(&data), y = number(&data);
        assert(x >= -1e-8 && x <= 170.15 + 1e-8);
        assert(y >= -1e-8 && y <= 127.6 + 1e-8);
        ++points;
    }
    assert(points > 1);
    return points - 1;
}
