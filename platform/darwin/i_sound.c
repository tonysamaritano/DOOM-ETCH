/* Native stereo sound effects. Only owned samples reach the audio callback. */
#include "i_sound.h"
#include "w_wad.h"
#include "m_argv.h"
#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define AUDIO_RATE 44100
#define AUDIO_FRAMES 512
#define AUDIO_VOICES 32

typedef struct {
    unsigned char *pcm;
    uint32_t count, rate;
} SoundSample;
typedef struct {
    const SoundSample *sample;
    double cursor, step;
    int left, right, handle;
    uint64_t started;
} SoundVoice;
static SoundSample samples[NUMSFX];
static SoundVoice voices[AUDIO_VOICES];
static pthread_mutex_t soundLock = PTHREAD_MUTEX_INITIALIZER;
static AudioQueueRef soundQueue;
static int soundRunning, nextHandle = 1;
static uint64_t soundSerial;

static int limit(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}

int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char name[9];
    if (sfx->link) sfx = sfx->link;
    snprintf(name, sizeof(name), "ds%.6s", sfx->name);
    int lump = W_CheckNumForName(name);
    return lump >= 0 ? lump : W_CheckNumForName("dspistol");
}

static void loadSample(int id)
{
    int lump = I_GetSfxLumpNum(&S_sfx[id]);
    if (lump < 0) return;
    int bytes = W_LumpLength(lump);
    if (bytes < 8) return;
    unsigned char *raw = malloc((size_t)bytes);
    if (!raw) return;
    W_ReadLump(lump, raw);
    uint32_t rate = raw[2] | (uint32_t)raw[3] << 8;
    uint32_t count = raw[4] | (uint32_t)raw[5] << 8 |
                     (uint32_t)raw[6] << 16 | (uint32_t)raw[7] << 24;
    if (raw[0] != 3 || raw[1] || !rate || !count ||
        count > (uint32_t)bytes - 8) {
        free(raw);
        return;
    }
    /* DMX samples carry sixteen padding bytes at each end. */
    unsigned offset = count > 32 ? 24 : 8;
    if (count > 32) count -= 32;
    memmove(raw, raw + offset, count);
    samples[id] = (SoundSample){raw, count, rate};
}
