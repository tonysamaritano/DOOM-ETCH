// SPDX-License-Identifier: GPL-2.0-or-later
#include "requirements.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
/* Reporting assumptions; no physical simulator parameters are changed. */
#define ASSUMED_SOUND_M_S 343.0
#define STANDARD_GRAVITY_M_S2 9.80665
static int fail(MachineParseError *e,size_t line,const char *message)
{
    if (e) { e->line=line; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
int MachineRequirementsParseFPS(const char *text,double *out)
{
    if (!text || !out) return 0;
    const char *p=text;
    if (*p=='+' || *p=='-') p++;
    size_t digits=0;
    while (*p>='0' && *p<='9') { p++; digits++; }
    if (*p=='.') {
        p++;
        while (*p>='0' && *p<='9') { p++; digits++; }
    }
    if (!digits) return 0;
    if (*p=='e' || *p=='E') {
        p++;
        if (*p=='+' || *p=='-') p++;
        const char *start=p;
        while (*p>='0' && *p<='9') p++;
        if (start==p) return 0;
    }
    if (*p) return 0;
    errno=0; char *end;
    double value=strtod(text,&end);
    if (*end || errno==ERANGE || !isfinite(value) || value<=0) return 0;
    *out=value; return 1;
}
