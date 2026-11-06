#include "doomtype.h"
#include "z_zone.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>

static byte *arena;
byte *I_ZoneBase(int *size)
{
    *size = 65536;
    arena = malloc((size_t)*size);
    assert(arena);
    return arena;
}
void I_Error(char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}
static void rejectsInvalidOwnership(void)
{
    int channel[2]; assert(pipe(channel) == 0);
    pid_t child = fork(); assert(child >= 0);
    if (child == 0) {
        close(channel[0]); dup2(channel[1], STDERR_FILENO); close(channel[1]);
        Z_Malloc(64, PU_CACHE, NULL);
        _exit(99);
    }
    close(channel[1]);
    char message[1024]; ssize_t bytes = read(channel[0], message, sizeof(message) - 1);
    assert(bytes > 0); message[bytes] = 0; close(channel[0]);
    int status; assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    assert(strstr(message, "owner is required"));
    assert(!strstr(message, "runtime error") && !strstr(message, "AddressSanitizer"));
}
int main(void)
{
    Z_Init();
    int empty = Z_FreeMemory();
    rejectsInvalidOwnership();
    void *items[24];
    for (int i = 0; i < 24; ++i) {
        items[i] = Z_Malloc(7 + i, PU_STATIC, NULL);
        memset(items[i], i + 1, 7 + i);
    }
    for (int i = 0; i < 24; i += 2) Z_Free(items[i]);
    Z_CheckHeap();
    for (int i = 1; i < 24; i += 2) {
        for (int j = 0; j < 7 + i; ++j)
            assert(((byte *)items[i])[j] == i + 1);
        Z_Free(items[i]);
    }
    Z_CheckHeap();
    assert(Z_FreeMemory() == empty); /* Both neighbors coalesce. */
    void *level = NULL;
    Z_Malloc(31, PU_LEVEL, &level);
    assert(level);
    Z_FreeTags(PU_LEVEL, PU_LEVSPEC);
    assert(level == NULL);
    assert(Z_FreeMemory() == empty);
    void *keeper = Z_Malloc(1024, PU_STATIC, NULL);
    memset(keeper, 0xa7, 1024);
    void *cacheA = NULL, *cacheB = NULL;
    Z_Malloc(20000, PU_CACHE, &cacheA);
    Z_Malloc(20000, PU_CACHE, &cacheB);
    void *pressure = Z_Malloc(50000, PU_STATIC, NULL);
    assert(cacheA == NULL && cacheB == NULL);
    for (int i = 0; i < 1024; ++i) assert(((byte *)keeper)[i] == 0xa7);
    Z_CheckHeap();
    Z_Free(pressure);
    Z_Free(keeper);
    assert(Z_FreeMemory() == empty);
    Z_Malloc(27, PU_STATIC, &level);
    Z_ChangeTag2(level, PU_CACHE);
    Z_FreeTags(PU_CACHE, PU_CACHE);
    assert(level == NULL && Z_FreeMemory() == empty);
    free(arena);
    return 0;
}
