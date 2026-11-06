// SPDX-License-Identifier: GPL-2.0-or-later
#include "live.h"
#include "machine_playback_test.h"
static unsigned char kinds[]={MOTION_START,MOTION_OUTLINE,MOTION_OUTLINE};
static MotionPath path(MotionPoint *points,size_t count)
{
    return (MotionPath){points,kinds,count,170.15,127.6,170.15,127.6};
}
static void submit(MachineLive *live,EtchDevice *direct,MotionPoint *points,size_t count)
{
    MotionPath p=path(points,count);MachineParseError error={0};
    assert(MachineLiveNeedsPath(live));
    assert(MachineLiveSubmit(live,&p,&error));
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
