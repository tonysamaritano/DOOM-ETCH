// SPDX-License-Identifier: GPL-2.0-or-later
#include "requirements.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void near(double actual, double expected)
{
    assert(isfinite(actual));
    assert(fabs(actual - expected) <= 2e-12 * fmax(1.0, fabs(expected)));
}

static MachineStepProgram fixture(MachineStep *moves, size_t count)
{
    MachineStepProgram p = {0};
    MachineSettingsDefault(&p.settings);
    p.moves = moves;
    p.count = count;
    return p;
}

static MachineRequirements calculate(MachineStepProgram *p,
    size_t begin, size_t end, double fps)
{
    MachineRequirements r = {0};
    MachineParseError error = {0};
    assert(MachineRequirementsCalculate(p, begin, end, fps, &r, &error));
    return r;
}

static void hand_hundred(void)
{
    MachineStep moves[] = {{{2000, 0}, 60, MACHINE_MOVE_DRAW, 7}};
    MachineStepProgram p = fixture(moves, 1);
    for (int fps = 30; fps <= 35; fps += 5) {
        MachineRequirements r = calculate(&p, 0, 1, fps);
        assert(r.segments == 1);
        near(r.path_m, .1);
        near(r.sum_sqrt_m, sqrt(.1));
        near(r.dmax_m, .1);
        near(r.no_stop_m_s, fps == 30 ? 3 : 3.5);
        near(r.no_stop_mach, fps == 30 ? 3.0 / 343 : 3.5 / 343);
        near(r.accel_m_s2, fps == 30 ? 360 : 490);
        near(r.accel_g, fps == 30 ? 360 / 9.80665 : 490 / 9.80665);
        near(r.peak_m_s, fps == 30 ? 6 : 7);
        near(r.peak_mach, fps == 30 ? 6.0 / 343 : 7.0 / 343);
        near(r.knob_rpm, fps == 30 ? 9000 : 10500);
    }
}

static void corner_sensitivity(void)
{
    MachineStep moves[] = {{{1000, 0}, 1, MACHINE_MOVE_DRAW, 2},
        {{1000, 1000}, 0, MACHINE_MOVE_RAPID, 3}};
    MachineStepProgram p = fixture(moves, 2);
    MachineRequirements r = calculate(&p, 0, 2, 30);
    assert(r.segments == 2);
    near(r.path_m, .1);
    near(r.dmax_m, .05);
    near(r.no_stop_m_s, 3);
    near(r.accel_m_s2, 720);
    near(r.peak_m_s, 6);
    near(calculate(&p, 0, 2, 35).accel_m_s2, 980);
}

static void cursor_ranges(void)
{
    MachineStep moves[] = {{{1200, 200}, 60, MACHINE_MOVE_DRAW, 2},
        {{INT64_MAX, INT64_MIN}, 0, MACHINE_MOVE_PAUSE, 3},
        {{1200, 200}, 0, MACHINE_MOVE_RAPID, 4},
        {{200, 200}, .001, MACHINE_MOVE_DRAW, 5},
        {{INT64_MIN, INT64_MAX}, 0, MACHINE_MOVE_END, 6}};
    MachineStepProgram p = fixture(moves, 5);
    p.start_steps[0] = p.start_steps[1] = 200;
    MachineRequirements all = calculate(&p, 0, 5, 30);
    assert(all.segments == 3);
    near(all.path_m, .1);
    near(all.accel_m_s2, 720);
    MachineRequirements tail = calculate(&p, 2, 5, 30);
    assert(tail.segments == 2);
    near(tail.path_m, .05);
    near(tail.accel_m_s2, 180);
    near(tail.peak_m_s, 3);
    MachineRequirements empty = calculate(&p, 1, 2, 30);
    assert(!empty.segments && empty.path_m == 0 && empty.knob_rpm == 0);
}

static void asymmetric_grid(void)
{
    MachineStep moves[] = {{{1240, 352}, 60, MACHINE_MOVE_DRAW, 9}};
    MachineStepProgram p = fixture(moves, 1);
    p.settings.steps_per_mm[0] = 40;
    p.settings.steps_per_mm[1] = 16;
    p.start_steps[0] = 40;
    p.start_steps[1] = 32;
    p.settings.knob_mm_turn = 10;
    p.settings.max_rate_mm_min[0] = 17;
    p.settings.acceleration_mm_s2[1] = 3;
    MachineRequirements r = calculate(&p, 0, 1, 30);
    near(r.path_m, sqrt(1300.0) / 1000);
    near(r.accel_m_s2, 3600 * sqrt(1300.0) / 1000);
    near(r.knob_rpm, sqrt(1300.0) * 180);
}

static void frame_lifecycle(void)
{
    MachineStep moves[] = {{{0, 0}, 0, MACHINE_MOVE_PAUSE, 1},
        {{2000, 0}, 60, MACHINE_MOVE_DRAW, 2},
        {{0, 0}, 0, MACHINE_MOVE_PAUSE, 3},
        {{0, 0}, 0, MACHINE_MOVE_PAUSE, 4},
        {{0, 0}, 0, MACHINE_MOVE_RAPID, 5},
        {{0, 0}, 0, MACHINE_MOVE_PAUSE, 6},
        {{0, 0}, 0, MACHINE_MOVE_END, 7}};
    MachineStepProgram p = fixture(moves, 7);
    MachineRequirementsFrames frames = {0};
    MachineParseError error = {0};
    assert(MachineRequirementsFramesBuild(&frames, &p, 30, &error));
    assert(frames.count == 5 && frames.fps == 30 && frames.knob_mm_turn == 20);
    for (size_t i = 0; i < frames.count; ++i) {
        assert(frames.items[i].frame == i);
        assert(frames.items[i].demand.segments == (i == 1 || i == 3));
        near(frames.items[i].demand.path_m, i == 1 || i == 3 ? .1 : 0);
    }
    MachineRequirementsFrame *old = frames.items;
    assert(!MachineRequirementsFramesBuild(&frames, &p, NAN, &error));
    assert(frames.items == old && frames.count == 5 && frames.fps == 30);
    p.count = 0;
    p.moves = NULL;
    assert(MachineRequirementsFramesBuild(&frames, &p, 35, &error));
    assert(frames.count == 1 && !frames.items[0].demand.segments);
    MachineRequirementsFramesFree(&frames);
    assert(!frames.items && !frames.count && !frames.fps && !frames.knob_mm_turn);
    MachineRequirementsFramesFree(&frames);
}

static void rejected(MachineStepProgram *p, size_t begin, size_t end, double fps)
{
    MachineRequirements r, before;
    memset(&r, 0x35, sizeof(r));
    memcpy(&before, &r, sizeof(r));
    MachineParseError error = {0};
    assert(!MachineRequirementsCalculate(p, begin, end, fps, &r, &error));
    assert(!memcmp(&r, &before, sizeof(r)));
    assert(error.message[0]);
}

static void invalid_ranges_and_numbers(void)
{
    MachineStep move = {{2000, 0}, 60, MACHINE_MOVE_DRAW, 41};
    MachineStepProgram p = fixture(&move, 1);
    const double bad[] = {0, -1, NAN, INFINITY, DBL_MAX, DBL_MIN};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i)
        rejected(&p, 0, 1, bad[i]);
    rejected(&p, 1, 0, 30);
    rejected(&p, 0, 2, 30);
    rejected(NULL, 0, 0, 30);
    assert(!MachineRequirementsCalculate(&p, 0, 1, 30, NULL, NULL));
    p.moves = NULL;
    rejected(&p, 0, 0, 30);
    p.moves = &move;
    p.count = SIZE_MAX;
    rejected(&p, 0, 0, 30);
    p.count = 1;
    p.settings.knob_mm_turn = DBL_MIN;
    rejected(&p, 0, 1, 30);
    p.settings.knob_mm_turn = NAN;
    rejected(&p, 0, 1, 30);
}

static void invalid_programs(void)
{
    MachineStep move = {{2000, 0}, 60, MACHINE_MOVE_DRAW, 41};
    MachineStepProgram p = fixture(&move, 1);
    const int64_t bad[] = {-1, INT64_MAX, INT64_MIN, 3404};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        move.target_steps[0] = bad[i];
        rejected(&p, 0, 0, 30); /* Validate even outside selected range. */
    }
    move.target_steps[0] = 2000;
    move.kind = (MachineMoveKind)99;
    rejected(&p, 0, 1, 30);
    move.kind = MACHINE_MOVE_DRAW;
    move.feed_mm_s = NAN;
    rejected(&p, 0, 1, 30);
    move.feed_mm_s = 60;
    p.start_steps[0] = -1;
    rejected(&p, 0, 1, 30);
    p.start_steps[0] = 0;
    p.settings.steps_per_mm[1] = INFINITY;
    rejected(&p, 0, 1, 30);
}

static void strict_fps(void)
{
    const char *bad[] = {"", "0", "-1", "nan", "inf", " 30", "30 ",
        "30x", "0x1p5", "1e9999", "1e-9999", ".", "--30", "30\n"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        double value = 17;
        assert(!MachineRequirementsParseFPS(bad[i], &value) && value == 17);
    }
    double value = 0;
    assert(MachineRequirementsParseFPS("+3.5e1", &value) && value == 35);
    assert(MachineRequirementsParseFPS(".5", &value) && value == .5);
    assert(!MachineRequirementsParseFPS(NULL, &value));
    assert(!MachineRequirementsParseFPS("30", NULL));
}

static void prevalidated_stream(void)
{
    MachineStep moves[] = {{{2000, 0}, 60, MACHINE_MOVE_DRAW, 1},
        {{0, 0}, 0, MACHINE_MOVE_PAUSE, 2}};
    MachineStepProgram p = fixture(moves, 2);
    MachineRequirementsFrames frames = {0};
    assert(MachineRequirementsFramesBuild(&frames, &p, 30, NULL));
    FILE *file = tmpfile();
    assert(file);
    frames.items[1].demand.peak_m_s = NAN;
    assert(!MachineRequirementsFramesWrite(file, &frames, 1));
    assert(ftell(file) == 0);
    assert(!MachineRequirementsFramesWrite(file, &frames, 0));
    assert(ftell(file) == 0);
    frames.items[1].demand.peak_m_s = 0;
    assert(MachineRequirementsFramesWrite(file, &frames, 1));
    assert(ftell(file) > 0);
    rewind(file);
    assert(fgetc(file) == '[');
    assert(!fseek(file, -1, SEEK_END) && fgetc(file) == ']');
    assert(fputc('\n', file) == '\n'); /* Stream remains caller-owned/open. */
    fclose(file);
    assert(!MachineRequirementsFramesWrite(NULL, &frames, 1));
    MachineRequirementsFramesFree(&frames);
}

static void zero_and_clipped_extent(void)
{
    MachineStep zero = {{0, 0}, 0, MACHINE_MOVE_RAPID, 12};
    MachineStepProgram p = fixture(&zero, 1);
    MachineRequirements r = calculate(&p, 0, 1, DBL_MAX);
    assert(r.segments == 1 && r.path_m == 0 && r.accel_m_s2 == 0);
    r = calculate(&p, 0, 1, DBL_MIN);
    assert(r.no_stop_m_s == 0 && r.peak_m_s == 0);
    p.settings.width_mm = nextafter(.2, 0);
    zero.target_steps[0] = 3; /* Last reachable .15 mm grid point. */
    near(calculate(&p, 0, 1, 30).path_m, .00015);
    zero.target_steps[0] = 4; /* .20 lies outside the custom extent. */
    rejected(&p, 0, 1, 30);
    MachineParseError error = {0};
    assert(!MachineRequirementsCalculate(&p, 0, 1, 30, &r, &error));
    assert(error.line == 12 && error.message[0]);
}

static void invalid_frame_storage(void)
{
    MachineRequirementsFrames f = {0};
    FILE *stream = tmpfile();
    assert(stream);
    assert(!MachineRequirementsFramesValid(NULL));
    assert(!MachineRequirementsFramesWrite(stream, &f, 1));
    f.count = SIZE_MAX;
    assert(!MachineRequirementsFramesValid(&f));
    MachineStepProgram empty = fixture(NULL, 0);
    f.count = 0;
    assert(MachineRequirementsFramesBuild(&f, &empty, 30, NULL));
    assert(MachineRequirementsFramesValid(&f));
    f.items[0].frame = 1;
    assert(!MachineRequirementsFramesWrite(stream, &f, 1));
    assert(ftell(stream) == 0);
    f.items[0].frame = 0;
    f.fps = INFINITY;
    assert(!MachineRequirementsFramesWrite(stream, &f, 0));
    assert(ftell(stream) == 0);
    MachineRequirementsFramesFree(&f);
    fclose(stream);
}

int main(void)
{
    hand_hundred();
    corner_sensitivity();
    cursor_ranges();
    asymmetric_grid();
    frame_lifecycle();
    invalid_ranges_and_numbers();
    invalid_programs();
    strict_fps();
    prevalidated_stream();
    zero_and_clipped_extent();
    invalid_frame_storage();
    return 0;
}
