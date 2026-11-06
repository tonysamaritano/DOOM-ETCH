// SPDX-License-Identifier: GPL-2.0-or-later
#include "live.h"
#include "machine_playback_test.h"
static unsigned char kinds[]={MOTION_START,MOTION_OUTLINE,MOTION_OUTLINE};
static MotionPath path(MotionPoint *points,size_t count)
{
    static MotionPoint source[3];assert(count<=3);
    for(size_t i=0;i<count;i++)source[i]=(MotionPoint){points[i].x*20,points[i].y*20};
    return (MotionPath){source,kinds,count,3403,2552,170.15,127.6};
}
static void submit(MachineLive *live,EtchDevice *direct,MotionPoint *points,size_t count)
{
    MotionPath p=path(points,count);MachineParseError error={0};
    assert(MachineLiveNeedsPath(live));
    int accepted=MachineLiveSubmit(live,&p,&error);
    if(!accepted)fprintf(stderr,"Live submit: line %zu: %s\n",error.line,error.message);
    assert(accepted);
    assert(EtchDeviceSubmit(direct,points,count,60));
}
static void step(MachineLive *live,EtchDevice *direct,double seconds)
{
    assert(MachineLiveTick(live,seconds,1));
    assert(EtchDeviceTick(direct,seconds));
    compare_devices(MachineLiveDevice(live),direct);
}
static void finish(MachineLive *live,EtchDevice *direct)
{
    double left=EtchDeviceGetStatus(direct).remaining_s;
    step(live,direct,left);
    assert(EtchDeviceRequestErase(direct));
    assert(EtchDeviceGetStatus(MachineLiveDevice(live)).erasing);
    step(live,direct,20); // Idle surplus must not enter the physical clock.
    assert(MachineLiveNeedsPath(live) && EtchDeviceReady(direct));
}
int main(void)
{
    MachineSettings settings;MachineSettingsDefault(&settings);
    MotionPoint start={10,20};
    MachineLive *live=MachineLiveCreate(&settings,start);assert(live);
    EtchDevice *direct=EtchCreate(MachineSettingsConfig(&settings),start);assert(direct);
    MotionPoint out[]={{10,20},{110,20}};
    submit(live,direct,out,2);step(live,direct,.5);
    machine_near(MachineLiveSeconds(live),.5);
    MachineLivePause(live,1);assert(MachineLivePaused(live));
    assert(MachineLiveTick(live,1000,1));
    assert(MachineLiveTick(live,1000,0));
    compare_devices(MachineLiveDevice(live),direct);
    machine_near(MachineLiveSeconds(live),.5);
    MachineLivePause(live,0);assert(!MachineLivePaused(live));
    assert(MachineLiveTick(live,1000,0)); // Inactive game time is discarded too.
    compare_devices(MachineLiveDevice(live),direct);
    finish(live,direct);
    double total=100.0/60+60.0/250+2.5;
    machine_near(MachineLiveSeconds(live),total);
    MotionPoint back[]={{110,20},{10,20}};
    submit(live,direct,back,2);step(live,direct,.01);
    EtchStatus status=EtchDeviceGetStatus(MachineLiveDevice(live));
    machine_point(status.cursor_mm,110,20);
    assert(status.motor_mm.x<110); // Backlash moves the knob before the stylus.
    finish(live,direct);
    total+=100.15/60+60.0/250+2.5;
    machine_near(MachineLiveSeconds(live),total);
    MachineParseError error={0};MotionPoint only[]={{10,20}};
    MotionPath empty=path(only,1);
    assert(MachineLiveSubmit(live,&empty,&error) && MachineLiveNeedsPath(live));
    machine_near(MachineLiveSeconds(live),total);
    only[0]=(MotionPoint){0,0};
    empty=path(only,1);
    assert(!MachineLiveSubmit(live,&empty,&error));
    compare_devices(MachineLiveDevice(live),direct);
    MotionPoint corner[]={{10,20},{110,20},{110,120}};
    submit(live,direct,corner,3);step(live,direct,.1);
    assert(MachineLiveErase(live) && EtchDeviceRequestErase(direct));
    step(live,direct,100);
    assert(MachineLiveNeedsPath(live));
    machine_point(EtchDeviceGetStatus(MachineLiveDevice(live)).cursor_mm,110,20);
    total+=100.15/60+60.0/250+2.5;
    machine_near(MachineLiveSeconds(live),total);
    MachineLiveDestroy(live);EtchDestroy(direct);
    puts("machine-live: persistent G-code, backlash and pause agree");
    return 0;
}
