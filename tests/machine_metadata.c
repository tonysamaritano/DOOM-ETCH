#include "bridge.h"
#include "machine_test.h"

static int read_text(const char *text, MachineSettings *s, MachineProgram *p,
                     MachineParseError *error)
{
    FILE *f = tmpfile(); assert(f);
    assert(fputs(text, f) >= 0); rewind(f);
    int result = MachineRead(f, s, p, error);
    assert(!ferror(f)); fclose(f);
    return result;
}

int main(void)
{
    MachineSettings s; MachineSettingsDefault(&s);
    MachineProgram p = {0}; MachineParseError e = {0};
    assert(read_text("; Jog to ETCH_START before running\n; Mention ETCH_SIZE Xnan Y1\n"
        "; ETCH_SIZE X1.023 Y1.037\n; ETCH_START X0.1 Y0.937\n"
        "G21\nG90\nG1 X0.2 F60\nM2", &s, &p, &e));
    machine_near(s.width_mm, 1.023); machine_near(s.height_mm, 1.037);
    machine_point(p.start_cnc_mm, .1, .937);
    machine_point(p.moves[0].cnc_mm, .2, .937);
    assert(p.moves[0].line == 7);
    MachineMove *owned = p.moves; MachineSettings before = s;
    assert(!read_text("; ETCH_SIZE Xnan Y1\nG1 X0 F60", &s, &p, &e));
    assert(e.line == 1 && e.message[0]);
    assert(!memcmp(&before, &s, sizeof s)); assert(p.moves == owned);
    assert(p.count == 2); machine_point(p.start_cnc_mm, .1, .937);
    assert(!read_text("; ETCH_SIZE X2 Y2\nG2 X1", &s, &p, &e));
    assert(e.line == 2 && e.message[0]);
    assert(!memcmp(&before, &s, sizeof s) && p.moves == owned);
    MachineProgramFree(&p);
    puts("machine-metadata: dedicated headers and transactional read passed");
    return 0;
}
