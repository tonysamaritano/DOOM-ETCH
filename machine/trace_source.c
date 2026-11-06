// SPDX-License-Identifier: GPL-2.0-or-later
#include "trace.h"
#include <stdint.h>
#include <stdlib.h>
/* Retain the exact bytes that MachineRead will consume, never reread the file. */
FILE *EtchTraceSource(FILE *input,EtchTrace *trace,MachineParseError *error)
{
    char *text=NULL; size_t count=0,capacity=0; int byte;
    while ((byte=fgetc(input))!=EOF) {
        if (count+1>=capacity) {
            size_t next=capacity ? capacity*2 : 4096;
            if (next<=capacity) goto failure;
            char *allocation=realloc(text,next);
            if (!allocation) goto failure;
            text=allocation; capacity=next;
        }
        text[count++]=(char)byte;
    }
    if (ferror(input)) goto failure;
    if (!text) text=calloc(1,1);
    if (!text) goto failure;
    text[count]=0;
    FILE *copy=tmpfile();
    if (!copy) goto failure;
    if (fwrite(text,1,count,copy)!=count || fflush(copy) || fseek(copy,0,SEEK_SET)) {
        fclose(copy); goto failure;
    }
    trace->source=text;
    return copy;
failure:
    free(text);
    if (error) { error->line=0; snprintf(error->message,sizeof(error->message),"cannot snapshot G-code source"); }
    return NULL;
}
