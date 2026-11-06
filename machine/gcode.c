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
static int parse_row(Parser *p,const char *row,size_t line,MachineParseError *e)
{
    double v[5]={0}; unsigned seen=0,codes=0;
    if (!words(row,v,&seen,&codes)) return fail(e,line,"invalid or duplicate command word");
    if (!seen) return 1;
    if (p->ended) return fail(e,line,"executable text after end");
    if (seen & 2) {
        if (seen!=2 || (v[1]!=0 && v[1]!=2)) return fail(e,line,"unsupported M command");
        MachineMoveKind kind=v[1]==0 ? MACHINE_MOVE_PAUSE : MACHINE_MOVE_END;
        if (!append(&p->program,(MachineMove){p->cursor,0,kind,line}))
            return fail(e,line,"out of memory");
        p->ended=kind==MACHINE_MOVE_END; return 1;
    }
    if (seen & 16) {
        if (v[4]<=0 || !isfinite(v[4]/60) || v[4]/60<=0)
            return fail(e,line,"feed must be positive finite mm/min");
        p->feed=v[4]/60;
    }
    if (codes & 3) p->mode=(int)v[0];
    if (!(seen & 12) && !(codes & 3)) return 1;
    if (p->mode<0) return fail(e,line,"coordinates require a motion mode");
    if (p->mode==1 && p->feed<=0) return fail(e,line,"G1 requires a feed");
    if (seen & 4) p->cursor.x=v[2];
    if (seen & 8) p->cursor.y=v[3];
    MachineMove move={p->cursor,p->mode==0 ? 0 : p->feed,
                     p->mode==0 ? MACHINE_MOVE_RAPID : MACHINE_MOVE_DRAW,line};
    return append(&p->program,move) ? 1 : fail(e,line,"out of memory");
}
