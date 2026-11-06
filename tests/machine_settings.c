#include "steppers.h"
#include "machine_test.h"

static void assumptions(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    MachineParseError error = {0};
    assert(MachineSettingsValidate(&s, &error));
    machine_near(s.width_mm, 170.15); machine_near(s.height_mm, 127.6);
    machine_near(s.backlash_mm, .15); machine_near(s.erase_seconds, 2.5);
    machine_near(s.ghost_fraction, .03); machine_near(s.knob_mm_turn, 20);
    for (int axis = 0; axis < 2; ++axis) {
        machine_near(s.steps_per_mm[axis], 20);
        machine_near(s.max_rate_mm_min[axis], 3600);
        machine_near(s.acceleration_mm_s2[axis], 250);
    }
    EtchConfig c = MachineSettingsConfig(&s);
    machine_near(c.x.step_mm, .05); machine_near(c.y.step_mm, .05);
    machine_near(c.x.speed_mm_s, 60); machine_near(c.y.speed_mm_s, 60);
}

static void assignments(void)
{
    const char *valid[] = {"$100=40", "$101=10", "$110=1200", "$111=600",
        "$120=80", "$121=90", "backlash=0", "ghost=1"};
    const char *bad[] = {"$100=0", "$101=-1", "$110=NaN", "$111=inf",
        "$120=0", "$121=-1", "$102=20", "$100=20x", "$100=20=1",
        "$100", "ghost=1.01", "backlash=-1", "erase=0", "knob_mm_turn=0"};
    MachineSettings s; MachineSettingsDefault(&s); MachineParseError e = {0};
    for (size_t i = 0; i < sizeof valid / sizeof *valid; ++i)
        assert(MachineSettingsParse(&s, valid[i], &e));
    EtchConfig c = MachineSettingsConfig(&s);
    machine_near(c.x.step_mm, .025); machine_near(c.y.step_mm, .1);
    machine_near(c.x.speed_mm_s, 20); machine_near(c.y.speed_mm_s, 10);
    machine_near(c.x.acceleration_mm_s2, 80); machine_near(c.y.acceleration_mm_s2, 90);
    MachineSettings before = s;
    for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i) {
        assert(!MachineSettingsParse(&s, bad[i], &e));
        assert(e.line == 0 && e.message[0]);
        assert(!memcmp(&before, &s, sizeof s));
    }
}

int main(void)
{
    assumptions(); assignments();
    puts("machine-settings: defaults, units and transactional assignments passed");
    return 0;
}
