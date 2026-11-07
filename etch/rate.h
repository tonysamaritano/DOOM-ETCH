// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_ETCH_VISUAL_RATE_H
#define DOOM_ETCH_VISUAL_RATE_H
#include <math.h>
#include <stdio.h>
#define ETCH_VISUAL_MIN_RATE (1.0 / 64.0)
#define ETCH_VISUAL_MAX_RATE 1024.0
static inline double EtchVisualRateClamp(double rate)
{
    return fmax(ETCH_VISUAL_MIN_RATE,fmin(ETCH_VISUAL_MAX_RATE,rate));
}
static inline double EtchVisualRateKey(double rate,int key)
{
    if (key=='+' || key=='=') return EtchVisualRateClamp(rate*2);
    if (key=='-' || key=='_') return EtchVisualRateClamp(rate*.5);
    return rate;
}
/* "1000×" and "1/4×" rather than printf's "1e+03×". */
static inline void EtchVisualRateText(double rate,char *text,size_t size)
{
    if (rate>=1 && rate==floor(rate)) snprintf(text,size,"%.0f×",rate);
    else if (rate<1 && 1/rate==floor(1/rate)) snprintf(text,size,"1/%.0f×",1/rate);
    else snprintf(text,size,"%.3g×",rate);
}
#endif
