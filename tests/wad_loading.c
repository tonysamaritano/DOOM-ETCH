#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "i_system.h"
#include "w_wad.h"
#include "z_zone.h"

enum { REJECTED = 41 };

void I_Error(char *message, ...)
{
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    _exit(REJECTED);
}

byte *I_ZoneBase(int *size)
{
    static _Alignas(memblock_t) byte arena[8192];
    *size = sizeof(arena);
    return arena;
}

static void writeWad(char *name, const char *mode, int payload)
{
    unsigned char bytes[] = {
        'P','W','A','D', 1,0,0,0, 13,0,0,0,
        0,
        12,0,0,0, 1,0,0,0, 'T','E','S','T','D','A','T','A'
    };
    size_t length = sizeof(bytes);
    bytes[12] = payload;
    if (!strcmp(mode, "header")) length = 5;
    if (!strcmp(mode, "directory")) bytes[8] = 100;
    if (!strcmp(mode, "count")) memset(bytes + 4, 255, 4);
    if (!strcmp(mode, "payload")) bytes[17] = 70;
    int fd = mkstemps(name, 4);
    assert(fd >= 0);
    assert(write(fd, bytes, length) == (ssize_t)length);
    assert(close(fd) == 0);
}

static void readWads(char **paths, const char *mode)
{
    Z_Init();
    W_InitMultipleFiles(paths);
    if (!strcmp(mode, "index"))
        (void)W_LumpLength(-1);
    if (strcmp(mode, "valid"))
        return;
    int lump = W_GetNumForName("testdata");
    assert(lump == 1 && W_LumpLength(lump) == 1);
    byte *cached = W_CacheLumpNum(lump, PU_STATIC);
    assert(cached[0] == 22 && W_CacheLumpNum(lump, PU_STATIC) == cached);
    byte first;
    W_ReadLump(0, &first);
    assert(first == 11);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char base[] = "/tmp/doom-etch-base-XXXXXX.wad";
    char patch[] = "/tmp/doom-etch-patch-XXXXXX.wad";
    char *paths[] = {base, patch, NULL};
    writeWad(base, "valid", 11);
    writeWad(patch, argv[1], 22);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        readWads(paths, argv[1]);
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    unlink(base);
    unlink(patch);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == (!strcmp(argv[1], "valid") ? 0 : REJECTED));
    return 0;
}
