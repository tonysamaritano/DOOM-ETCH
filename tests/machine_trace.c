// SPDX-License-Identifier: GPL-2.0-or-later
#include "machine_playback_test.h"
#include "trace.h"
#include "run.h"
static const char *fixture =
    "; ETCH_SIZE X170.15 Y127.6\n; ETCH_START X0 Y127.6\n"
    "G21\nG90\nF3600\nG1 X100\nG1 X40 Y97.6\n"
    "F600\nG1 X50 Y87.6\nM0\nG0 X10 Y117.6\n"
    "G1 X10 Y117.6\nM0\nM2\n";
static MachinePlayback *record(const char *text, MachineSettings settings, EtchTrace *trace)
{
    FILE *f = tmpfile(); assert(f); assert(fputs(text, f) >= 0); rewind(f);
    MachineProgram parsed = {0}; MachineStepProgram compiled = {0}; MachineParseError error = {0};
    assert(MachineRead(f, &settings, &parsed, &error)); assert(!fclose(f));
    assert(MachineSteppersCompile(&compiled, &parsed, &settings, &error));
    MachinePlayback *p = MachinePlaybackCreateTraced(&compiled, trace, &error); assert(p);
    MachineProgramFree(&parsed); MachineStepProgramFree(&compiled);
    assert(MachineRun(p)); assert(MachinePlaybackDone(p));
    machine_near(trace->clock, MachinePlaybackSeconds(p));
    return p;
}
static void near_sample(MotionPoint actual, MotionPoint expected)
{
    assert(isfinite(actual.x) && isfinite(actual.y));
    if (fabs(actual.x - expected.x) > 1e-5 || fabs(actual.y - expected.y) > 1e-5) {
        fprintf(stderr, "trace (%.17g,%.17g), device (%.17g,%.17g)\n",
                actual.x, actual.y, expected.x, expected.y);
        abort();
    }
}
static int order(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static void compare_samples(const char *text, MachineSettings settings, const EtchTrace *trace)
{
    size_t capacity = 2003 + 8 * trace->count + 3 * trace->erase_count, n = 0;
    double *times = malloc(capacity * sizeof(*times)); assert(times);
    for (int k = 0; k <= 2000; k++) times[n++] = trace->clock * k / 2000;
    times[n++] = trace->clock; assert(n <= capacity);
    qsort(times, n, sizeof(*times), order);
    MachinePlayback *plain = NULL;
    for (size_t i = 0; i < n; i++) {
        plain = play_text(text, &settings);
        assert(MachinePlaybackTick(plain, times[i]));
        MotionPoint cursor, motor;
        assert(EtchTraceSample(trace, times[i], &cursor, &motor));
        EtchStatus status = EtchDeviceGetStatus(MachinePlaybackDevice(plain));
        near_sample(cursor, status.cursor_mm); near_sample(motor, status.motor_mm);
        MachinePlaybackDestroy(plain);
    }
    free(times);
}
static void metadata_and_profiles(void)
{
    MachineSettings settings; MachineSettingsDefault(&settings); EtchTrace trace = {0};
    MachinePlayback *p = record(fixture, settings, &trace);
    assert(trace.count == 5 && trace.erase_count == 2);
    const size_t lines[] = {6, 7, 9, 11, 12};
    for (size_t i = 0; i < trace.count; i++) {
        assert(trace.segments[i].line == lines[i]);
        assert(trace.segments[i].frame == (i < 3 ? 0u : 1u));
    }
    machine_near(trace.segments[0].duration, 143.0 / 75);
    machine_near(trace.segments[0].length, 100);
    machine_near(trace.segments[2].peak, 10);
    assert(trace.segments[4].duration == 0 && trace.segments[4].length == 0);
    const EtchTraceSegment *reverse = &trace.segments[1];
    assert(reverse->slack > 0);
    MotionPoint cursor, motor;
    assert(EtchTraceSample(&trace, reverse->t0 + .001, &cursor, &motor));
    machine_point(cursor, reverse->x0, reverse->y0);
    assert(hypot(motor.x - reverse->motor_x0, motor.y - reverse->motor_y0) > 1e-6);
    compare_samples(fixture, settings, &trace);
    MachinePlaybackDestroy(p); EtchTraceFree(&trace);
}
static void nonuniform_steps_and_empty(void)
{
    MachineSettings settings; MachineSettingsDefault(&settings);
    settings.steps_per_mm[0] = 40; settings.steps_per_mm[1] = 16;
    settings.max_rate_mm_min[1] = 420; settings.acceleration_mm_s2[0] = 47;
    EtchTrace trace = {0}; MachinePlayback *p = record(fixture, settings, &trace);
    compare_samples(fixture, settings, &trace);
    MachinePlaybackDestroy(p); EtchTraceFree(&trace);
    const char *cases[] = {"", "M2\n", "M0\nM0\nM2\n",
        "; ETCH_START X5 Y122.6\nG1 X5 Y122.6 F600\nM0\nG1 X5 Y122.6\nM2\n"};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        p = record(cases[i], settings, &trace);
        compare_samples(cases[i], settings, &trace);
        MotionPoint cursor = {-9, -8}, motor = {-7, -6};
        assert(!EtchTraceSample(&trace, NAN, &cursor, &motor));
        assert(!EtchTraceSample(&trace, -1, &cursor, &motor));
        assert(!EtchTraceSample(&trace, trace.clock + 1, &cursor, &motor));
        machine_point(cursor, -9, -8); machine_point(motor, -7, -6);
        MachinePlaybackDestroy(p); EtchTraceFree(&trace);
    }
}
static void exact_feed_and_erase_boundaries(void)
{
    MachineSettings settings; MachineSettingsDefault(&settings); EtchTrace trace = {0};
    MachinePlayback *recorded = record(fixture, settings, &trace);
    MachinePlayback *plain = play_text(fixture, &settings);
    while (!MachinePlaybackDone(plain)) {
        EtchStatus status = EtchDeviceGetStatus(MachinePlaybackDevice(plain));
        double left = status.erasing ? status.erase_left_s : status.remaining_s;
        assert(MachinePlaybackTick(plain, left));
        status = EtchDeviceGetStatus(MachinePlaybackDevice(plain));
        MotionPoint cursor, motor;
        assert(EtchTraceSample(&trace, MachinePlaybackSeconds(plain), &cursor, &motor));
        near_sample(cursor, status.cursor_mm); near_sample(motor, status.motor_mm);
    }
    machine_near(MachinePlaybackSeconds(plain), trace.clock);
    MachinePlaybackDestroy(plain); MachinePlaybackDestroy(recorded); EtchTraceFree(&trace);
}
int main(void)
{
    metadata_and_profiles(); nonuniform_steps_and_empty(); exact_feed_and_erase_boundaries();
    puts("machine-trace: independent sampled cursor/motor positions within 1e-5 mm");
    return 0;
}
