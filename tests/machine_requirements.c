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
