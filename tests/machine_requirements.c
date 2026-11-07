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
