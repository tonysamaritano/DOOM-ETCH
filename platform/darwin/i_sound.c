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
