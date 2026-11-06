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

static void setVoiceParams(SoundVoice *voice, int volume, int pan, int pitch)
{
    volume = limit(volume, 0, 127);
    pan = limit(pan, 0, 255) + 1;
    voice->left = volume - ((volume * pan * pan) >> 16);
    pan -= 257;
    voice->right = volume - ((volume * pan * pan) >> 16);
    voice->step = (double)voice->sample->rate / AUDIO_RATE *
                 pow(2.0, (limit(pitch, 0, 255) - 128) / 64.0);
}

static void fillAudio(void *context, AudioQueueRef queue,
                      AudioQueueBufferRef buffer)
{
    (void)context;
    int16_t *output = buffer->mAudioData;
    pthread_mutex_lock(&soundLock);
    int running = soundRunning;
    for (int frame = 0; frame < AUDIO_FRAMES; ++frame) {
        int left = 0, right = 0;
        for (int i = 0; i < AUDIO_VOICES; ++i) {
            SoundVoice *voice = &voices[i];
            if (!voice->sample) continue;
            int amplitude = (voice->sample->pcm[(uint32_t)voice->cursor] - 128) * 256;
            left += amplitude * voice->left / 127;
            right += amplitude * voice->right / 127;
            voice->cursor += voice->step;
            if (voice->cursor >= voice->sample->count) voice->sample = NULL;
        }
        *output++ = (int16_t)limit(left, -32768, 32767);
        *output++ = (int16_t)limit(right, -32768, 32767);
    }
    pthread_mutex_unlock(&soundLock);
    buffer->mAudioDataByteSize = AUDIO_FRAMES * 4;
    if (running) AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

void I_ShutdownSound(void)
{
    pthread_mutex_lock(&soundLock);
    soundRunning = 0;
    AudioQueueRef previous = soundQueue;
    soundQueue = NULL;
    pthread_mutex_unlock(&soundLock);
    /* Dispose synchronously: callbacks finish before samples are freed. */
    if (previous) {
        AudioQueueStop(previous, true);
        AudioQueueDispose(previous, true);
    }
    pthread_mutex_lock(&soundLock);
    memset(voices, 0, sizeof(voices));
    for (int i = 0; i < NUMSFX; ++i) {
        free(samples[i].pcm);
        samples[i] = (SoundSample){0};
    }
    pthread_mutex_unlock(&soundLock);
}

void I_InitSound(void)
{
    if (soundQueue || M_CheckParm("-nosound") || M_CheckParm("-nosfx")) return;
    AudioStreamBasicDescription format = {0};
    format.mSampleRate = AUDIO_RATE;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    format.mBytesPerPacket = format.mBytesPerFrame = 4;
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = 16;
    OSStatus status = AudioQueueNewOutput(&format, fillAudio, NULL, NULL, NULL, 0, &soundQueue);
    if (status != noErr) goto failed;
    for (int i = 1; i < NUMSFX; ++i) loadSample(i);
    pthread_mutex_lock(&soundLock);
    soundRunning = 1;
    pthread_mutex_unlock(&soundLock);
    for (int i = 0; i < 3; ++i) {
        AudioQueueBufferRef buffer;
        status = AudioQueueAllocateBuffer(soundQueue, AUDIO_FRAMES * 4, &buffer);
        if (status != noErr) goto failed;
        fillAudio(NULL, soundQueue, buffer);
    }
    status = AudioQueueStart(soundQueue, NULL);
    if (status == noErr) return;
failed:
    fprintf(stderr, "AudioQueue unavailable (%d); continuing silently\n", (int)status);
    I_ShutdownSound();
}

int I_StartSound(int id, int volume, int pan, int pitch, int priority)
{
    (void)priority; /* The engine arbitrates priorities before starting voices. */
    int handle = -1, slot = 0;
    pthread_mutex_lock(&soundLock);
    if (!soundRunning || id <= 0 || id >= NUMSFX || !samples[id].pcm) goto done;
    for (int i = 0; i < AUDIO_VOICES; ++i) {
        if (!voices[i].sample) { slot = i; break; }
        if (voices[i].started < voices[slot].started) slot = i;
    }
    int collision;
    do {
        handle = nextHandle;
        nextHandle = nextHandle == INT_MAX ? 1 : nextHandle + 1;
        collision = 0;
        for (int i = 0; i < AUDIO_VOICES; ++i)
            if (voices[i].sample && voices[i].handle == handle) collision = 1;
    } while (collision);
    voices[slot] = (SoundVoice){.sample = &samples[id], .handle = handle,
                               .started = ++soundSerial};
    setVoiceParams(&voices[slot], volume, pan, pitch);
done:
    pthread_mutex_unlock(&soundLock);
    return handle;
}

void I_StopSound(int handle)
{
    pthread_mutex_lock(&soundLock);
    for (int i = 0; i < AUDIO_VOICES; ++i)
        if (voices[i].handle == handle) voices[i].sample = NULL;
    pthread_mutex_unlock(&soundLock);
}
