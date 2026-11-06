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
