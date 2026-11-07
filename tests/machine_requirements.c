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
