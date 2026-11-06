// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "r_etch.h"
#include "silhouette.h"
#include "hatch.h"

enum { CAPTURE_WIDTH = 320, CAPTURE_HEIGHT = 200 };
enum { MAX_SEGMENTS = 400000 };
typedef struct {
    double top, bottom, top_end, bottom_end;
    int valid;
} WallSample;
static WallSample samples[3][CAPTURE_WIDTH];
static double clip_top[CAPTURE_WIDTH], clip_bottom[CAPTURE_WIDTH];
static double clip_top_step[CAPTURE_WIDTH], clip_bottom_step[CAPTURE_WIDTH];
static MotionSegment *walls;
static size_t wall_count, wall_capacity;
static int failed, wall_open;
static int active;
static int width, height;
typedef struct SpriteFrame {
    int lump;
    Silhouette shape;
    SilLines features;
    int features_ready;
    struct SpriteFrame *next;
} SpriteFrame;
typedef struct {
    int tier, family, row, x;
    MotionSegment line;
} WallHatch;
typedef struct {
    int tier, x;
    const unsigned char *source, *light, *palette;
    double mid, scale, next_scale, center, u, u_end;
} WallTexture;
static WallHatch *wall_hatches;
static size_t hatch_count, hatch_capacity;

static SpriteFrame *sprite_frames;
static int sprites_enabled = 1;
static int hud_enabled = 1, weapon_enabled = 1, hud_capture;
static int wall_textures = 1, sprite_textures = 1, weapon_textures = 1;
static int hatch_pattern = HATCH_CROSS;
void R_EtchTextures(int enabled) { wall_textures = enabled != 0; }
void R_EtchSpriteTextures(int enabled) { sprite_textures = enabled != 0; }
void R_EtchWeaponTextures(int enabled) { weapon_textures = enabled != 0; }
int R_EtchTexturesActive(void) { return active && !failed && wall_textures; }
int R_EtchSpriteTexturesEnabled(void) { return sprite_textures; }
int R_EtchWeaponTexturesEnabled(void) { return weapon_textures; }
void R_EtchHatchPattern(int pattern)
{
    if (pattern >= 0 && pattern < HATCH_PATTERN_COUNT) hatch_pattern = pattern;
}

void R_EtchHUD(int enabled) { hud_enabled = enabled != 0; }
void R_EtchWeapon(int enabled) { weapon_enabled = enabled != 0; }
int R_EtchHUDEnabled(void) { return hud_enabled; }
int R_EtchWeaponActive(void) { return active && !failed && weapon_enabled; }
int R_EtchHUDCapturing(void) { return hud_capture; }
void R_EtchHUDEnd(void) { hud_capture = 0; }
int R_EtchHUDBegin(void)
{
    if (!active || failed || !hud_enabled || width != 320 || (height != 168 && height != 200))
        return 0;
    hud_capture = 1;
    height = 200;
    return 1;
}

void R_EtchSprites(int enabled) { sprites_enabled = enabled != 0; }
int R_EtchSpritesActive(void) { return active && !failed && sprites_enabled; }

static SpriteFrame *sprite_frame(int lump, const void *patch, size_t bytes)
{
    for (SpriteFrame *frame = sprite_frames; frame; frame = frame->next)
        if (frame->lump == lump) return frame;
    SpriteFrame *frame = calloc(1, sizeof(*frame));
    if (!frame) return NULL;
    if (!SilhouetteDecode(&frame->shape, patch, bytes)) {
        free(frame);
        return NULL;
    }
    frame->lump = lump;
    frame->next = sprite_frames;
    sprite_frames = frame;
    return frame;
}

void R_EtchResetSprites(void)
{
    while (sprite_frames) {
        SpriteFrame *next = sprite_frames->next;
        SilhouetteFree(&sprite_frames->shape);
        SilLinesFree(&sprite_frames->features);
        free(sprite_frames);
        sprite_frames = next;
    }
}
int R_EtchBegin(int w, int h)
{
    if (active || w <= 0 || w > CAPTURE_WIDTH || h <= 0 || h > CAPTURE_HEIGHT)
        return 0;
    width = w; height = h;
    active = 1;
    R_EtchFrameBegin(w, h);
    return 1;
}

int R_EtchBuildMotion(MotionPath *path)
{
    if (!active) return 0;
    active = 0;
    int success = 0;
    if (path && !failed && !wall_open && !hud_capture)
        success = MotionPlanSegments(path, walls, wall_count,
            (unsigned)width, (unsigned)height, (MotionPoint){0, 0});
    free(walls);
    free(wall_hatches);
    wall_hatches = NULL;
    hatch_count = hatch_capacity = 0;
    walls = NULL;
    wall_count = wall_capacity = 0;
    hud_capture = 0;
    return success;
}

void R_EtchFrameBegin(int w, int h)
{
    if (!active) return;
    failed = 0; wall_open = 0; wall_count = 0; hud_capture = 0;
    if (w <= 0 || w > CAPTURE_WIDTH || h <= 0 || h > CAPTURE_HEIGHT) {
        failed = 1;
        return;
    }
    width = w; height = h;
    memset(samples, 0, sizeof(samples));
    for (int x = 0; x < width; x++) {
        clip_top[x] = clip_top_step[x] = clip_bottom_step[x] = 0;
        clip_bottom[x] = height;
    }
}

void R_EtchWallBegin(void)
{
    if (!active || failed) return;
    if (wall_open) { failed = 1; return; }
    memset(samples, 0, sizeof(samples));
    hatch_count = 0;
    wall_open = 1;
}

static double clamp(double value, double low, double high)
{
    return fmin(high, fmax(low, value));
}

void R_EtchClipPlane(int x, double top, double topstep, double bottom, double bottomstep)
{
    if (!active || failed || x < 0 || x >= width) return;
    if (!isfinite(top) || !isfinite(bottom) || !isfinite(topstep) || !isfinite(bottomstep)
        || !isfinite(top + topstep) || !isfinite(bottom + bottomstep)) {
        failed = 1;
        return;
    }
    if (top >= clip_top[x]) {
        if (top <= clip_bottom[x]) {
            clip_top[x] = top; clip_top_step[x] = topstep;
        } else {
            clip_top[x] = clip_bottom[x]; clip_top_step[x] = clip_bottom_step[x];
        }
    }
    if (bottom <= clip_bottom[x]) {
        if (bottom >= clip_top[x]) {
            clip_bottom[x] = bottom; clip_bottom_step[x] = bottomstep;
        } else {
            clip_bottom[x] = clip_top[x]; clip_bottom_step[x] = clip_top_step[x];
        }
    }
}

void R_EtchWallColumn(int tier, int x, double top, double bottom,
                      double topstep, double bottomstep)
{
    if (!active || failed || !wall_open || tier < 0 || tier >= 3 || x < 0 || x >= width)
        return;
    if (!isfinite(top) || !isfinite(bottom) || !isfinite(topstep) || !isfinite(bottomstep)
        || !isfinite(top + topstep) || !isfinite(bottom + bottomstep)) {
        failed = 1;
        return;
    }
    double low = clip_top[x], high = clip_bottom[x];
    if (low >= high) return;
    double next_low = clamp(low + clip_top_step[x], 0, height);
    double next_high = clamp(high + clip_bottom_step[x], next_low, height);
    WallSample sample = {
        .top = clamp(top, low, high),
        .bottom = clamp(bottom, low, high),
        .top_end = clamp(top + topstep, next_low, next_high),
        .bottom_end = clamp(bottom + bottomstep, next_low, next_high),
        .valid = 1
    };
    sample.bottom = fmax(sample.top, sample.bottom);
    sample.bottom_end = fmax(sample.top_end, sample.bottom_end);
    if (sample.bottom <= sample.top && sample.bottom_end <= sample.top_end) return;
    samples[tier][x] = sample;
}

static void add_segment(MotionPoint a, MotionPoint b)
{
    if (failed || (a.x == b.x && a.y == b.y)) return;
    if (wall_count) {
        MotionSegment *last = &walls[wall_count - 1];
        double dx = last->b.x - last->a.x, dy = last->b.y - last->a.y;
        double nx = b.x - a.x, ny = b.y - a.y;
        if (last->b.x == a.x && last->b.y == a.y
            && fabs(dx * ny - dy * nx) < 1e-8 && dx * nx + dy * ny > 0) {
            last->b = b;
            return;
        }
    }
    if (wall_count == MAX_SEGMENTS) { failed = 1; return; }
    if (wall_count == wall_capacity) {
        size_t capacity = wall_capacity ? wall_capacity * 2 : 256;
        if (capacity > MAX_SEGMENTS) capacity = MAX_SEGMENTS;
        MotionSegment *next = realloc(walls, capacity * sizeof(*next));
        if (!next) { failed = 1; return; }
        walls = next; wall_capacity = capacity;
    }
    walls[wall_count++] = (MotionSegment){a, b};
}

static void tier_edges(int tier)
{
    for (int side = 0; side < 2; side++) {
        for (int x = 0; x < width; x++) {
            WallSample sample = samples[tier][x];
            if (!sample.valid) continue;
            double from = side ? sample.bottom : sample.top;
            double to = side ? sample.bottom_end : sample.top_end;
            add_segment((MotionPoint){x, from}, (MotionPoint){x + 1, to});
        }
    }
}

static void tier_caps(int tier)
{
    for (int x = 0; x < width; x++) {
        WallSample sample = samples[tier][x];
        if (!sample.valid) continue;
        if (!x || !samples[tier][x - 1].valid) {
            add_segment((MotionPoint){x, sample.top}, (MotionPoint){x, sample.bottom});
        } else {
            WallSample previous = samples[tier][x - 1];
            add_segment((MotionPoint){x, previous.top_end}, (MotionPoint){x, sample.top});
            add_segment((MotionPoint){x, previous.bottom_end}, (MotionPoint){x, sample.bottom});
        }
        if (x + 1 == width || !samples[tier][x + 1].valid)
            add_segment((MotionPoint){x + 1, sample.top_end},
                (MotionPoint){x + 1, sample.bottom_end});
    }
}

static int append_wall_hatch(WallHatch stroke)
{
    if (hatch_count == MAX_SEGMENTS) return 0;
    if (hatch_count == hatch_capacity) {
        size_t capacity = hatch_capacity ? hatch_capacity * 2 : 256;
        if (capacity > MAX_SEGMENTS) capacity = MAX_SEGMENTS;
        WallHatch *next = realloc(wall_hatches, capacity * sizeof(*next));
        if (!next) return 0;
        wall_hatches = next; hatch_capacity = capacity;
    }
    wall_hatches[hatch_count++] = stroke;
    return 1;
}

static int compare_wall_hatch(const void *left, const void *right)
{
    const WallHatch *a = left, *b = right;
    if (a->tier != b->tier) return a->tier < b->tier ? -1 : 1;
    if (a->family != b->family) return a->family < b->family ? -1 : 1;
    if (a->row != b->row) return a->row < b->row ? -1 : 1;
    return a->x < b->x ? -1 : a->x > b->x;
}

static double wall_luminance(const WallTexture *texture, double row)
{
    double sum = 0;
    double footprint = fmin(128, HATCH_SPACING_MM / HATCH_MM_Y / texture->scale);
    for (int sample = 0; sample < 8; ++sample) {
        int v = (int)floor(row + ((sample + 0.5) / 8 - 0.5) * footprint);
        unsigned index = texture->light[texture->source[(unsigned)v & 127]];
        const unsigned char *rgb = texture->palette + 3 * index;
        sum += (0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]) / 8;
    }
    return sum;
}

static int wall_hatch_row(const WallTexture *texture, WallSample sample,
                         int family, double slope, double row, int spacing)
{
    double y = texture->center + (row + slope * texture->u - texture->mid) * texture->scale;
    double end = texture->center
        + (row + slope * texture->u_end - texture->mid) * texture->next_scale;
    double low, high;
    if (!HatchClip(y, end, sample.top + 0.6, sample.top_end + 0.6,
            sample.bottom - 0.6, sample.bottom_end - 0.6, &low, &high)) return 1;
    double texel_row = row + slope * (texture->u + texture->u_end) * 0.5;
    if (wall_luminance(texture, texel_row) >= HatchThreshold((int)(row / spacing), family))
        return 1;
    return append_wall_hatch((WallHatch){texture->tier, family, (int)row, texture->x,
        {{texture->x + low, y + (end - y) * low},
         {texture->x + high, y + (end - y) * high}}});
}

void R_EtchWallEnd(void)
{
    if (!active || failed) return;
    if (!wall_open) { failed = 1; return; }
    for (int tier = 0; tier < 3; tier++) {
        tier_edges(tier);
        tier_caps(tier);
    }
    wall_open = 0;
}

static int hud_features(SpriteFrame *frame, double ox, double oy, double sx, double sy,
    const unsigned char *palette, SilLines *outline)
{
    if (!frame->features_ready) {
        if (!SilFeatures(&frame->shape, palette, &frame->features)) return 0;
        frame->features_ready = 1;
    }
    for (size_t i = 0; i < frame->features.count; ++i) {
        MotionSegment edge = frame->features.data[i];
        edge.a = (MotionPoint){ox + edge.a.x * sx, oy + edge.a.y * sy};
        edge.b = (MotionPoint){ox + edge.b.x * sx, oy + edge.b.y * sy};
        if (edge.a.x < 0 || edge.b.x < 0 || edge.a.x > 320 || edge.b.x > 320
            || edge.a.y < 168 || edge.b.y < 168 || edge.a.y > 200 || edge.b.y > 200)
            continue;
        if (!SilLineAdd(outline, edge.a, edge.b)) return 0;
    }
    return 1;
}

static int capture_patch(SpriteFrame *frame,
    double origin_x, double origin_y, double scale_x, double scale_y,
    int first, int last, const short *top, const short *bottom,
    const unsigned char *palette)
{
    SilRects fill = {0};
    SilLines outline = {0};
    SilLines scene = {walls, wall_count, wall_capacity};
    int success = SilProject(&frame->shape, origin_x, origin_y, scale_x, scale_y,
        first, last, top, bottom, (unsigned)width, (unsigned)height, &fill, &outline);
    if (success && palette)
        success = hud_features(frame, origin_x, origin_y, scale_x, scale_y, palette, &outline);
    if (success) success = SilComposite(&scene, &fill, &outline);
    walls = scene.data;
    wall_count = scene.count;
    wall_capacity = scene.capacity;
    SilRectsFree(&fill);
    SilLinesFree(&outline);
    if (!success || wall_count > MAX_SEGMENTS) { failed = 1; return 0; }
    return 1;
}

int R_EtchSpritePatch(int lump, const void *patch, size_t bytes,
    double origin_x, double origin_y, double scale_x, double scale_y,
    int first, int last, const short *top, const short *bottom)
{
    if (!R_EtchSpritesActive()) return 1;
    SpriteFrame *frame = sprite_frame(lump, patch, bytes);
    if (!frame) { failed = 1; return 0; }
    return capture_patch(frame, origin_x, origin_y, scale_x, scale_y,
        first, last, top, bottom, NULL);
}

int R_EtchOverlayPatch(int lump, const void *patch, size_t bytes,
    double origin_x, double origin_y, double scale_x, double scale_y,
    int first, int last, const short *top, const short *bottom,
    const unsigned char *palette, int hud)
{
    if (hud ? !hud_capture : !R_EtchWeaponActive()) return 1;
    SpriteFrame *frame = sprite_frame(lump, patch, bytes);
    if (!frame || (hud && !palette)) { failed = 1; return 0; }
    return capture_patch(frame, origin_x, origin_y, scale_x, scale_y,
        first, last, top, bottom, hud ? palette : NULL);
}
