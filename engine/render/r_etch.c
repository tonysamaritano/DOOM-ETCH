// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "r_etch.h"
#include "silhouette.h"

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
    struct SpriteFrame *next;
} SpriteFrame;
static SpriteFrame *sprite_frames;
static int sprites_enabled = 1;

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
    if (path && !failed && !wall_open)
        success = MotionPlanSegments(path, walls, wall_count,
            (unsigned)width, (unsigned)height, (MotionPoint){0, 0});
    free(walls);
    walls = NULL;
    wall_count = wall_capacity = 0;
    return success;
}

void R_EtchFrameBegin(int w, int h)
{
    if (!active) return;
    failed = 0; wall_open = 0; wall_count = 0;
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

int R_EtchSpritePatch(int lump, const void *patch, size_t bytes,
    double origin_x, double origin_y, double scale_x, double scale_y,
    int first, int last, const short *top, const short *bottom)
{
    if (!R_EtchSpritesActive()) return 1;
    SpriteFrame *frame = sprite_frame(lump, patch, bytes);
    if (!frame) { failed = 1; return 0; }
    SilRects fill = {0};
    SilLines outline = {0};
    SilLines scene = {walls, wall_count, wall_capacity};
    int success = SilProject(&frame->shape, origin_x, origin_y, scale_x, scale_y,
        first, last, top, bottom, (unsigned)width, (unsigned)height, &fill, &outline);
    if (success) success = SilComposite(&scene, &fill, &outline);
    walls = scene.data;
    wall_count = scene.count;
    wall_capacity = scene.capacity;
    SilRectsFree(&fill);
    SilLinesFree(&outline);
    if (!success || wall_count > MAX_SEGMENTS) { failed = 1; return 0; }
    return 1;
}
