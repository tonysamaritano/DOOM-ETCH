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
typedef struct {
    const char *name;
    double left, top, right, bottom;
    size_t strokes;
} ArtworkRegion;
/* Native status-bar widget locations: the face and large ammo digits. */
static ArtworkRegion artwork[] = {
    {"face", 143, 169, 167, 199, 0}, {"ammo", 12, 171, 44, 187, 0}
};
static void countArtwork(double ax, double ay, double bx, double by)
{
    ax *= 320 / 170.15; bx *= 320 / 170.15;
    ay *= 200 / 127.6; by *= 200 / 127.6;
    double length = hypot(bx - ax, by - ay);
    if (length <= 0.05 || length >= 8) return;
    for (size_t i = 0; i < sizeof(artwork) / sizeof(*artwork); ++i) {
        ArtworkRegion *r = &artwork[i];
        if (fmin(ax, bx) > r->left && fmax(ax, bx) < r->right &&
            fmin(ay, by) > r->top && fmax(ay, by) < r->bottom) ++r->strokes;
    }
}
static size_t validatePath(const char *text)
{
    const char *path = strstr(text, "<path");
    assert(path && !strstr(path + 5, "<path"));
    const char *data = attribute(path, "d");
    char quote = data[-1];
    size_t points = 0;
    double previousX = 0, previousY = 0;
    for (;;) {
        while (isspace((unsigned char)*data)) ++data;
        if (*data == quote) break;
        assert(*data++ == (points ? 'L' : 'M'));
        double x = number(&data), y = number(&data);
        assert(x >= -1e-8 && x <= 170.15 + 1e-8);
        assert(y >= -1e-8 && y <= 127.6 + 1e-8);
        if (points) countArtwork(previousX, previousY, x, y);
        previousX = x; previousY = y;
        ++points;
    }
    assert(points > 1);
    return points - 1;
}

int main(int argc, char **argv)
{
    assert(argc == 2 || (argc == 3 && !strcmp(argv[2], "--hud-regions")));
    char *text = readSVG(argv[1]);
    const char *root = strstr(text, "<svg"); assert(root);
    const char *box = attribute(root, "viewBox");
    assert(fabs(number(&box)) < 1e-8 && fabs(number(&box)) < 1e-8);
    assert(fabs(number(&box) - 170.15) < 1e-8);
    assert(fabs(number(&box) - 127.6) < 1e-8);
    size_t moves = validatePath(text);
    printf("%zu moves within 170.15 x 127.6 mm\n", moves);
    if (argc == 3) {
        for (size_t i = 0; i < sizeof(artwork) / sizeof(*artwork); ++i) {
            printf("HUD %s detail: %zu short interior strokes\n", artwork[i].name, artwork[i].strokes);
            assert(artwork[i].strokes >= 8);
        }
    }
    free(text);
    return 0;
}
