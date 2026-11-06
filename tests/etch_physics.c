#include "etch_sim.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void near(double actual, double expected)
{
    if (fabs(actual - expected) > 1e-8) {
        fprintf(stderr, "physics: %.17g != analytic %.17g\n", actual, expected);
        assert(0);
    }
}

static void point(MotionPoint actual, double x, double y)
{
    near(actual.x, x);
    near(actual.y, y);
}

static void defaults(void)
{
    EtchConfig c = EtchAssumedDefaults();
    near(c.width_mm, 170.15); near(c.height_mm, 127.6);
    near(c.x.speed_mm_s, 60); near(c.y.speed_mm_s, 60);
    near(c.x.acceleration_mm_s2, 250);
    near(c.y.acceleration_mm_s2, 250);
    near(c.x.backlash_mm, .15); near(c.y.backlash_mm, .15);
    near(c.x.step_mm, .05); near(c.y.step_mm, .05);
    near(c.erase_seconds, 2.5); near(c.ghost_fraction, .03);
}

/* Rest-to-rest line motion: feed limits speed, never acceleration. */
static double profile(double length, double speed, double acceleration)
{
    return length < speed * speed / acceleration
        ? 2 * sqrt(length / acceleration)
        : length / speed + speed / acceleration;
}

static double run(EtchConfig c, MotionPoint end, double feed)
{
    MotionPoint p[] = {{0, 0}, end};
    EtchDevice *d = EtchCreate(c, p[0]);
    assert(d);
    assert(feed ? EtchDeviceSubmit(d, p, 2, feed)
                : EtchDeviceSubmitHardware(d, p, 2));
    EtchStatus s = EtchDeviceGetStatus(d);
    double duration = s.remaining_s;
    assert(s.count == 1 && s.completed == 0 && s.drawing);
    assert(EtchDeviceTick(d, duration + 1));
    s = EtchDeviceGetStatus(d);
    near(s.elapsed_s, duration); near(s.remaining_s, 0);
    point(s.cursor_mm, end.x, end.y);
    point(s.motor_velocity_mm_s, 0, 0);
    assert(s.completed == 1 && EtchDeviceReady(d));
    EtchDestroy(d);
    return duration;
}

static void analytical_profiles(void)
{
    EtchConfig c = EtchAssumedDefaults();
    near(run(c, (MotionPoint){100, 0}, 0), 143.0 / 75);
    near(run(c, (MotionPoint){.1, 0}, 0), .04);
    near(run(c, (MotionPoint){100, 100}, 0), 143.0 / 75);
    near(run(c, (MotionPoint){100, 100}, 60), 134 * sqrt(2) / 75);
    c.y.speed_mm_s = 5; c.y.acceleration_mm_s2 = 10;
    near(run(c, (MotionPoint){100, 0}, 0), 143.0 / 75);
    near(run(c, (MotionPoint){0, 10}, 0), 2.5);
    near(run(c, (MotionPoint){10, 10}, 0),
         profile(sqrt(200), 5 * sqrt(2), 10 * sqrt(2)));
}

static void direction_memory(void)
{
    EtchDevice *d = EtchCreate(EtchAssumedDefaults(), (MotionPoint){0, 0});
    MotionPoint outward[] = {{0, 0}, {100, 0}, {100, 10}, {100, 10}};
    assert(d && EtchDeviceSubmitHardware(d, outward, 4));
    assert(EtchDeviceTick(d, 10));
    EtchStatus s = EtchDeviceGetStatus(d);
    assert(s.completed == 3);
    near(s.elapsed_s, profile(100, 60, 250) + profile(10, 60, 250));
    assert(EtchDeviceRequestErase(d));
    assert(EtchDeviceTick(d, 2.5));
    MotionPoint back[] = {{100, 10}, {0, 10}};
    assert(EtchDeviceSubmitHardware(d, back, 2));
    near(EtchDeviceGetStatus(d).remaining_s, profile(100.15, 60, 250));
    assert(EtchDeviceTick(d, 10));
    point(EtchDeviceGetStatus(d).cursor_mm, 0, 10);
    EtchDestroy(d);
}

static void quantized_extent(void)
{
    EtchConfig c = EtchAssumedDefaults();
    c.width_mm = 1.023; c.height_mm = 1.037;
    EtchDevice *d = EtchCreate(c, (MotionPoint){.0249, .0251});
    assert(d);
    point(EtchDeviceGetStatus(d).cursor_mm, 0, .05);
    MotionPoint p[] = {{0, .05}, {1.023, 1.037}};
    assert(EtchDeviceSubmitHardware(d, p, 2));
    assert(EtchDeviceTick(d, 10));
    point(EtchDeviceGetStatus(d).cursor_mm, 1, 1);
    EtchDestroy(d);
}

static void same_status(EtchStatus a, EtchStatus b)
{
    point(a.cursor_mm, b.cursor_mm.x, b.cursor_mm.y);
    point(a.motor_mm, b.motor_mm.x, b.motor_mm.y);
    point(a.motor_velocity_mm_s, b.motor_velocity_mm_s.x, b.motor_velocity_mm_s.y);
    near(a.elapsed_s, b.elapsed_s); near(a.remaining_s, b.remaining_s);
    near(a.erase_left_s, b.erase_left_s);
    assert(a.completed == b.completed && a.count == b.count);
    assert(a.drawing == b.drawing && a.erasing == b.erasing);
}

static void transactional(void)
{
    MotionPoint p[] = {{0, 0}, {100, 0}, {100, 10}};
    EtchDevice *d = EtchCreate(EtchAssumedDefaults(), p[0]);
    assert(d && EtchDeviceSubmitHardware(d, p, 3));
    p[1].x = NAN; p[2].y = NAN; /* Submission must own its copy. */
    assert(EtchDeviceTick(d, .2));
    EtchStatus before = EtchDeviceGetStatus(d);
    unsigned char ink[ETCH_INK_WIDTH * ETCH_INK_HEIGHT];
    memcpy(ink, EtchDeviceInk(d), sizeof ink);
    assert(!EtchDeviceSubmitHardware(d, p, 3));
    assert(!EtchDeviceTick(d, NAN));
    assert(!EtchDeviceTick(d, INFINITY));
    assert(!EtchDeviceTick(d, -.1));
    same_status(before, EtchDeviceGetStatus(d));
    assert(!memcmp(ink, EtchDeviceInk(d), sizeof ink));
    assert(EtchDeviceTick(d, 10));
    point(EtchDeviceGetStatus(d).cursor_mm, 100, 10);
    EtchDestroy(d);
}

static void invalid_idle(void)
{
    EtchConfig c = EtchAssumedDefaults();
    EtchDevice *d = EtchCreate(c, (MotionPoint){0, 0});
    MotionPoint p[] = {{0, 0}, {1, 1}};
    EtchStatus before = EtchDeviceGetStatus(d);
    assert(!EtchDeviceSubmit(d, p, 2, 0));
    assert(!EtchDeviceSubmit(d, p, 2, NAN));
    assert(!EtchDeviceSubmit(d, p, 2, INFINITY));
    assert(!EtchDeviceSubmit(d, p, 2, -1));
    assert(!EtchDeviceSubmitHardware(d, NULL, 2));
    assert(!EtchDeviceSubmitHardware(d, p, 0));
    assert(!EtchDeviceSubmitHardware(d, p, ETCH_MAX_POINTS + 1));
    p[0].x = 1;
    assert(!EtchDeviceSubmitHardware(d, p, 2));
    p[0].x = 0; p[1].x = c.width_mm + 1;
    assert(!EtchDeviceSubmitHardware(d, p, 2));
    p[1].x = NAN;
    assert(!EtchDeviceSubmitHardware(d, p, 2));
    same_status(before, EtchDeviceGetStatus(d));
    EtchDestroy(d);
    c.x.step_mm = 0; assert(!EtchCreate(c, (MotionPoint){0, 0}));
    c = EtchAssumedDefaults(); c.y.acceleration_mm_s2 = NAN;
    assert(!EtchCreate(c, (MotionPoint){0, 0}));
    c = EtchAssumedDefaults(); c.ghost_fraction = 1.01;
    assert(!EtchCreate(c, (MotionPoint){0, 0}));
}

static size_t marks(EtchDevice *d, unsigned char kind)
{
    const unsigned char *ink = EtchDeviceInk(d);
    size_t n = 0;
    for (size_t i = 0; i < ETCH_INK_WIDTH * ETCH_INK_HEIGHT; ++i) {
        assert(ink[i] <= ETCH_INK_GHOST);
        n += ink[i] == kind;
    }
    return n;
}

static void shake_retention(void)
{
    EtchDevice *d = EtchCreate(EtchAssumedDefaults(), (MotionPoint){0, 0});
    MotionPoint p[] = {{0, 0}, {100, 0}, {100, 100}};
    assert(d && EtchDeviceSubmitHardware(d, p, 3));
    assert(EtchDeviceTick(d, 10));
    size_t fresh = marks(d, ETCH_INK_FRESH);
    assert(fresh > 100 && marks(d, ETCH_INK_GHOST) == 0);
    assert(EtchDeviceRequestErase(d));
    assert(EtchDeviceTick(d, 1));
    assert(!EtchDeviceReady(d));
    near(EtchDeviceGetStatus(d).erase_left_s, 1.5);
    assert(EtchDeviceRequestErase(d));
    assert(EtchDeviceTick(d, 1.5));
    assert(EtchDeviceReady(d));
    point(EtchDeviceGetStatus(d).cursor_mm, 100, 100);
    assert(marks(d, ETCH_INK_FRESH) == 0);
    assert(marks(d, ETCH_INK_GHOST) == (size_t)floor(fresh * .03));
    EtchDestroy(d);
}

static void chunk_invariance(void)
{
    MotionPoint p[] = {{0, 0}, {100, 0}, {0, 0}, {0, 100}};
    EtchDevice *a = EtchCreate(EtchAssumedDefaults(), p[0]);
    EtchDevice *b = EtchCreate(EtchAssumedDefaults(), p[0]);
    assert(a && b);
    assert(EtchDeviceSubmitHardware(a, p, 4));
    assert(EtchDeviceSubmitHardware(b, p, 4));
    assert(EtchDeviceTick(a, 10));
    for (int i = 0; i < 10000; ++i) assert(EtchDeviceTick(b, .001));
    same_status(EtchDeviceGetStatus(a), EtchDeviceGetStatus(b));
    assert(!memcmp(EtchDeviceInk(a), EtchDeviceInk(b), 320 * 200));
    for (int shake = 0; shake < 2; ++shake) {
        assert(EtchDeviceRequestErase(a) && EtchDeviceRequestErase(b));
        assert(EtchDeviceTick(a, 3));
        for (int i = 0; i < 3000; ++i) assert(EtchDeviceTick(b, .001));
        same_status(EtchDeviceGetStatus(a), EtchDeviceGetStatus(b));
        assert(!memcmp(EtchDeviceInk(a), EtchDeviceInk(b), 320 * 200));
    }
    EtchDestroy(a); EtchDestroy(b);
}
