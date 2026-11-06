// SPDX-License-Identifier: GPL-2.0-or-later
#include "bridge.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static int fail(MachineParseError *e,size_t line,const char *message)
{
    if (e) { e->line=line; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
static char *read_text(FILE *stream,MachineParseError *e)
{
    char *text=NULL; size_t count=0,capacity=0,line=1; int c;
    while ((c=fgetc(stream))!=EOF) {
        if (!c) { fail(e,line,"NUL byte in stream"); free(text); return NULL; }
        if (count+1>=capacity) {
            size_t next=capacity ? capacity*2 : 4096;
            if (next<=capacity) { fail(e,line,"input too large"); free(text); return NULL; }
            char *allocation=realloc(text,next);
            if (!allocation) { fail(e,line,"out of memory"); free(text); return NULL; }
            text=allocation; capacity=next;
        }
        text[count++]=(char)c; if (c=='\n') line++;
    }
    if (ferror(stream)) { fail(e,line,"stream read error"); free(text); return NULL; }
    if (!text) text=calloc(1,1);
    if (!text) { fail(e,0,"out of memory"); return NULL; }
    text[count]=0; return text;
}
static int executable(const char *p)
{
    while (*p) {
        if (*p==' ' || *p=='\t') { p++; continue; }
        if (*p=='(') { p=strchr(p+1,')'); if (!p) return 1; p++; continue; }
        return *p!=';';
    }
    return 0;
}
