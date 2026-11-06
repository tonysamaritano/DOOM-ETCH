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
