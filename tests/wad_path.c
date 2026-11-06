/* Exercise startup discovery itself without opening a WAD. */
#include "doomstat.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

void IdentifyVersion(void);
extern char *wadfiles[];
GameMode_t gamemode;
GameMission_t gamemission;
Language_t language;
static const char *available;
int M_CheckParm(char *argument) { (void)argument; return 0; }
void I_Error(char *format, ...) { (void)format; abort(); }
int access(const char *path, int mode)
{
    (void)mode;
    return strcmp(path, available) ? -1 : 0;
}
static void checkPath(const char *directory, const char *name, int index)
{
    char expected[4096];
    assert(snprintf(expected, sizeof(expected), "%s/%s", directory, name) > 0);
    available = expected;
    assert(setenv("DOOMWADDIR", directory, 1) == 0);
    IdentifyVersion();
    assert(strcmp(wadfiles[index], expected) == 0);
}
int main(void)
{
    const char *names[] = {"doom2f.wad", "doom2.wad", "plutonia.wad",
                           "tnt.wad", "doomu.wad", "doom.wad", "doom1.wad"};
    const GameMode_t modes[] = {commercial, commercial, commercial,
                                commercial, retail, registered, shareware};
    char longDirectory[2048];
    memset(longDirectory, 'x', sizeof(longDirectory) - 1);
    longDirectory[sizeof(longDirectory) - 1] = 0;
    for (int i = 0; i < 7; ++i) {
        checkPath("/short", names[i], i * 2);
        assert(gamemode == modes[i]);
        checkPath(longDirectory, names[i], i * 2 + 1);
        assert(gamemode == modes[i]);
    }
    return 0;
}
