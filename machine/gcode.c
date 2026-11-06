// SPDX-License-Identifier: GPL-2.0-or-later
#include "gcode.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static int fail(MachineParseError *e,size_t line,const char *message)
{
    if (e) { e->line=line; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
void MachineProgramFree(MachineProgram *p)
{
    if (p) { free(p->moves); *p=(MachineProgram){0}; }
}
static int append(MachineProgram *p,MachineMove move)
{
    if (p->count==p->capacity) {
        size_t capacity=p->capacity ? p->capacity*2 : 32;
        if (capacity<p->capacity || capacity>SIZE_MAX/sizeof(*p->moves)) return 0;
        MachineMove *next=realloc(p->moves,capacity*sizeof(*next));
        if (!next) return 0;
        p->moves=next; p->capacity=capacity;
    }
    p->moves[p->count++]=move;
    return 1;
}
typedef struct {
    MachineProgram program;
    MotionPoint cursor;
    double feed;
    int mode,ended;
} Parser;
/* Explicit decimal grammar excludes exponents, NaN and infinity. */
static int number(const char **cursor,double *value)
{
    const char *start=*cursor,*p=start;
    if (*p=='+' || *p=='-') p++;
    int digits=0;
    while (isdigit((unsigned char)*p)) { p++; digits=1; }
    if (*p=='.') {
        p++;
        while (isdigit((unsigned char)*p)) { p++; digits=1; }
    }
    if (!digits) return 0;
    char *end;
    *value=strtod(start,&end);
    if (end!=p || !isfinite(*value)) return 0;
    *cursor=p;
    return 1;
}
static int words(const char *row,double values[5],unsigned *seen,unsigned *codes)
{
    for (const char *p=row;*p;) {
        if (*p==' ' || *p=='\t') { p++; continue; }
        if (*p==';') break;
        if (*p=='(') {
            for (p++;*p && *p!=')';p++) if (*p=='(') return 0;
            if (!*p) return 0;
            p++; continue;
        }
        const char *letters="GMXYF";
        const char *word=strchr(letters,toupper((unsigned char)*p));
        if (!word) return 0;
        unsigned index=(unsigned)(word-letters); p++;
        double value;
        if (!number(&p,&value)) return 0;
        if (index==0) {
            unsigned bit=value==0 ? 1 : value==1 ? 2 : value==21 ? 4 :
                value==90 ? 8 : value==94 ? 16 : value==61 ? 32 : 0;
            if (!bit || (*codes & bit) || (bit<=2 && (*codes & 3))) return 0;
            *codes|=bit;
            if (bit<=2) values[0]=value;
        } else {
            if (*seen & (1u<<index)) return 0;
            values[index]=value;
        }
        *seen|=1u<<index;
    }
    return 1;
}
