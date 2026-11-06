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
static int metadata(const char *text,MachineSettings *s,MachineParseOptions *start,
                    MachineParseError *e)
{
    int has_size=0,has_start=0,commands=0; size_t line=1;
    for (const char *p=text;*p;line++) {
        const char *end=strchr(p,'\n'); size_t n=end ? (size_t)(end-p) : strlen(p);
        if (n && p[n-1]=='\r') n--;
        if (n>80) return fail(e,line,"line exceeds 80 bytes");
        char row[81]; memcpy(row,p,n); row[n]=0;
        char *q=row; while (*q==' ' || *q=='\t') q++;
        int size=!strncmp(q,"; ETCH_SIZE",11),position=!strncmp(q,"; ETCH_START",12);
        if ((size && (q[11]==' ' || !q[11])) || (position && (q[12]==' ' || !q[12]))) {
            if (commands) return fail(e,line,"metadata must precede commands");
            if (size ? has_size++ : has_start++) return fail(e,line,"duplicate machine metadata");
            const char *values=q+(size ? 11 : 12); double x,y; int used=0;
            if (sscanf(values," X%lf Y%lf %n",&x,&y,&used)!=2 || !used || values[used] ||
                !isfinite(x) || !isfinite(y)) return fail(e,line,"invalid machine metadata");
            if (size) {
                s->width_mm=x; s->height_mm=y;
                if (!has_start) start->start_cnc_mm.y=y;
                if (!MachineSettingsValidate(s,e)) { if (e) e->line=line; return 0; }
            } else start->start_cnc_mm=(MotionPoint){x,y};
        }
        if (executable(q)) commands=1;
        if (!end) break;
        p=end+1;
    }
    return 1;
}
