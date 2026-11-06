// SPDX-License-Identifier: GPL-2.0-or-later
#include "trace.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
void EtchTraceFree(EtchTrace *t)
{
    if (t) { free(t->segments); free(t->erases); free(t->source); *t=(EtchTrace){0}; }
}
static int reserve(void **data,size_t *capacity,size_t needed,size_t size)
{
    if (needed<=*capacity) return 1;
    if (needed>SIZE_MAX/size) return 0;
    size_t next=*capacity ? *capacity : 32;
    while (next<needed) {
        if (next>SIZE_MAX/2) { next=needed; break; }
        next*=2;
    }
    if (next>SIZE_MAX/size) next=needed;
    void *allocation=realloc(*data,next*size);
    if (!allocation) return 0;
    *data=allocation; *capacity=next; return 1;
}
int EtchTraceRecordErase(EtchTrace *t,double duration)
{
    if (!t || t->failed || t->completed || !isfinite(duration) || duration<=0 ||
        !isfinite(t->clock+duration) || t->clock+duration<=t->clock) return 0;
    if (t->erase_count==SIZE_MAX || !reserve((void **)&t->erases,&t->erase_capacity,
        t->erase_count+1,sizeof(*t->erases))) { t->failed=1; return 0; }
    t->erases[t->erase_count++]=(EtchTraceErase){t->clock,duration};
    t->clock+=duration; return 1;
}
int EtchTraceRecordDevice(EtchTrace *t,const EtchDevice *d,const MachineStep *steps,
                          size_t count,size_t frame)
{
    EtchStatus status=EtchDeviceGetStatus(d);
    if (!t || !d || t->failed || t->completed || (count && !steps) ||
        count!=status.count || count>SIZE_MAX-t->count) return 0;
    double duration=status.elapsed_s+status.remaining_s,finish=t->clock+duration;
    if (!isfinite(finish) || (duration>0 && finish<=t->clock)) { t->failed=1; return 0; }
    if (!reserve((void **)&t->segments,&t->capacity,t->count+count,sizeof(*t->segments))) {
        t->failed=1; return 0;
    }
    for (size_t i=0;i<count;i++) {
        EtchTraceSegment segment;
        if (!EtchDeviceProfile(d,i,&segment)) { t->failed=1; return 0; }
        segment.t0+=t->clock; segment.line=steps[i].line; segment.frame=frame;
        t->segments[t->count+i]=segment;
    }
    t->count+=count; t->clock=finish;
    return 1;
}
static double sample_grid(double value,double step,double extent)
{
    double reciprocal=1/step;
    double maximum=isfinite(reciprocal) ? floor(extent*reciprocal) : floor(extent/step);
    double index=isfinite(reciprocal) ? round(value*reciprocal) : round(value/step);
    index=fmin(index,maximum);
    double result=isfinite(reciprocal) ? index/reciprocal : index*step;
    if (result>extent) {
        index=fmax(0,index-1); result=isfinite(reciprocal) ? index/reciprocal : index*step;
    }
    return result;
}
