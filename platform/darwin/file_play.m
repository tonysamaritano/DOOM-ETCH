// SPDX-License-Identifier: GPL-2.0-or-later
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMediaTiming.h>
#import "file_play.h"
#import "metal_backend.h"
#import "keyboard.h"
#include "doomdef.h"
#include "i_video.h"
#include "run.h"
#include "rate.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct {
    const char *filename;
    const char **assignments;
    size_t count;
    double rate;
    unsigned frames;
} FileOptions;
static int rate_value(const char *text,double *rate)
{
    char *end; errno=0; double value=strtod(text,&end);
    if (!*text || end==text || *end || errno==ERANGE || !isfinite(value) ||
        value<ETCH_VISUAL_MIN_RATE || value>ETCH_VISUAL_MAX_RATE) return 0;
    *rate=value; return 1;
}
static int frame_value(const char *text,unsigned *frames)
{
    if (!*text || strspn(text,"0123456789")!=strlen(text)) return 0;
    errno=0; unsigned long value=strtoul(text,NULL,10);
    if (errno==ERANGE || !value || value>UINT_MAX) return 0;
    *frames=(unsigned)value; return 1;
}
