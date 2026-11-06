// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_MACHINE_TRACE_H
#define DOOM_MACHINE_TRACE_H
#include "steppers.h"
typedef struct {
    double t0,duration,ramp,peak,accel,slack,length;
    double x0,y0,ux,uy,motor_x0,motor_y0;
    size_t line,frame;
} EtchTraceSegment;
typedef struct { double t0,duration; } EtchTraceErase;
typedef struct {
    EtchTraceSegment *segments;
    size_t count,capacity;
    EtchTraceErase *erases;
    size_t erase_count,erase_capacity;
    double clock;
    MotionPoint start_mm;
    EtchConfig config;
    char *source; /* Owned exact parsed text when loaded from a file. */
    int failed,completed;
} EtchTrace;
/* Zero-initialize owned traces. A traced playback borrows its trace. */
void EtchTraceFree(EtchTrace *);
/* Internal loader snapshot; returned stream is owned by the caller. */
FILE *EtchTraceSource(FILE *,EtchTrace *,MachineParseError *);
/* Current accepted submission profile; local t0, no line/frame metadata. */
int EtchDeviceProfile(const EtchDevice *,size_t index,EtchTraceSegment *);
/* Finite times in [0,clock], completed traces only. Optional outputs are
 * unchanged on failure. Cursor uses axis step grids; motor is continuous. */
int EtchTraceSample(const EtchTrace *,double time,MotionPoint *cursor,MotionPoint *motor);
int EtchTraceRecordDevice(EtchTrace *,const EtchDevice *,const MachineStep *,size_t count,size_t frame);
int EtchTraceRecordErase(EtchTrace *,double duration);
/* Requires completed playback and matching clocks. Atomic file publication. */
int MachineTraceWrite(const char *filename,const MachineStepProgram *,const EtchTrace *,
                      double seconds,MachineParseError *);
#endif
