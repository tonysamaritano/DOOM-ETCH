/* Exercise startup discovery itself without opening a WAD. */
#include "doomstat.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void IdentifyVersion(void);
extern char *wadfiles[];
char basedefault[1024];
GameMode_t gamemode;
GameMission_t gamemission;
Language_t language;
boolean devparm;
static const char *available;
int M_CheckParm(char *argument) { (void)argument; return 0; }
void I_Error(char *format, ...) { (void)format; abort(); }
int access(const char *path, int mode)
{
    (void)mode;
    return strcmp(path, available) ? -1 : 0;
}
int main(void)
{
    const char *directory = "/short";
    available = "/short/doomu.wad";
    assert(setenv("DOOMWADDIR", directory, 1) == 0);
    IdentifyVersion();
    assert(gamemode == retail);
    assert(strcmp(wadfiles[0], available) == 0);
    return 0;
}
