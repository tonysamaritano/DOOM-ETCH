// SPDX-License-Identifier: GPL-2.0-or-later
#include "etch_sim.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MotionPoint start, end, unit, motor_start;
    double length, slack, travel, acceleration, peak, ramp, duration, begin, finish;
    int direction_x, direction_y;
} Segment;
struct EtchDevice {
    EtchConfig config;
    EtchStatus status;
    Segment *segments;
    size_t index;
    double segment_time, total, erase_time;
    int direction_x, direction_y, erase_requested;
    unsigned next_sample, erase_count;
    unsigned char ink[ETCH_INK_WIDTH * ETCH_INK_HEIGHT];
};
EtchConfig EtchAssumedDefaults(void)
{
    EtchAxisConfig axis = {ETCH_ASSUMED_SPEED_MM_S, ETCH_ASSUMED_ACCELERATION_MM_S2,
                          ETCH_ASSUMED_BACKLASH_MM, ETCH_ASSUMED_STEP_MM};
    return (EtchConfig){axis, axis, MOTION_WIDTH_MM, MOTION_HEIGHT_MM,
                       ETCH_ASSUMED_ERASE_SECONDS, ETCH_ASSUMED_GHOST_FRACTION};
}
static int axis_valid(EtchAxisConfig a, double extent)
{
    return isfinite(a.speed_mm_s) && a.speed_mm_s > 0 &&
        isfinite(a.acceleration_mm_s2) && a.acceleration_mm_s2 > 0 &&
        isfinite(a.backlash_mm) && a.backlash_mm >= 0 &&
        isfinite(a.step_mm) && a.step_mm > 0 && isfinite(extent) && extent > 0 &&
        isfinite(extent / a.step_mm) && extent / a.step_mm <= 0x1p52;
}
static double quantize(double value, double step, double extent)
{
    return fmin(round(value / step), floor(extent / step + 1e-10)) * step;
}
static int point(EtchConfig c, MotionPoint input, MotionPoint *out)
{
    if (!isfinite(input.x) || !isfinite(input.y) || input.x < 0 || input.y < 0 ||
        input.x > c.width_mm || input.y > c.height_mm) return 0;
    *out = (MotionPoint){quantize(input.x,c.x.step_mm,c.width_mm),
                        quantize(input.y,c.y.step_mm,c.height_mm)};
    return 1;
}
EtchDevice *EtchCreate(EtchConfig c, MotionPoint start)
{
    MotionPoint q;
    if (!axis_valid(c.x,c.width_mm) || !axis_valid(c.y,c.height_mm) ||
        !isfinite(c.erase_seconds) || c.erase_seconds <= 0 ||
        !isfinite(c.ghost_fraction) || c.ghost_fraction < 0 || c.ghost_fraction > 1 ||
        !point(c,start,&q)) return NULL;
    EtchDevice *d = calloc(1,sizeof(*d));
    if (d) { d->config=c; d->status.cursor_mm=q; d->status.motor_mm=q; }
    return d;
}
void EtchDestroy(EtchDevice *d)
{
    if (d) { free(d->segments); free(d); }
}
static void axis_limits(EtchAxisConfig a, double u, int previous,
                        double *speed, double *acceleration, double *slack)
{
    if (!u) return;
    *speed=fmin(*speed,a.speed_mm_s/fabs(u));
    *acceleration=fmin(*acceleration,a.acceleration_mm_s2/fabs(u));
    if (previous && previous != (u > 0 ? 1 : -1))
        *slack=fmax(*slack,a.backlash_mm/fabs(u));
}
static int profile(EtchConfig c, Segment *s, double feed, int px, int py)
{
    double dx=s->end.x-s->start.x, dy=s->end.y-s->start.y;
    s->length=hypot(dx,dy); s->direction_x=px; s->direction_y=py;
    if (!s->length) return 1;
    s->unit=(MotionPoint){dx/s->length,dy/s->length};
    if ((dx && !s->unit.x) || (dy && !s->unit.y)) return 0;
    double speed=feed, acceleration=INFINITY;
    axis_limits(c.x,s->unit.x,px,&speed,&acceleration,&s->slack);
    axis_limits(c.y,s->unit.y,py,&speed,&acceleration,&s->slack);
    s->travel=s->length+s->slack; s->acceleration=acceleration;
    if (!isfinite(speed) || speed<=0 || !isfinite(acceleration) || acceleration<=0 ||
        !isfinite(s->travel) || s->travel<=s->slack) return 0;
    if (speed*(speed/acceleration) >= s->travel) {
        s->ramp=sqrt(s->travel)/sqrt(acceleration);
        s->peak=sqrt(s->travel)*sqrt(acceleration); s->duration=2*s->ramp;
    } else {
        s->ramp=speed/acceleration; s->peak=speed;
        s->duration=s->travel/speed+s->ramp;
    }
    if (s->unit.x) s->direction_x=s->unit.x>0 ? 1 : -1;
    if (s->unit.y) s->direction_y=s->unit.y>0 ? 1 : -1;
    return isfinite(s->duration) && s->duration>0 &&
           isfinite(s->peak) && s->peak>0 && isfinite(s->ramp) && s->ramp>0;
}
static int submit(EtchDevice *d,const MotionPoint *p,size_t n,double feed)
{
    if (!d || d->status.drawing || d->status.erasing || !p || !n ||
        n-1>SIZE_MAX/sizeof(Segment)) return 0;
    MotionPoint first;
    if (!point(d->config,p[0],&first) || first.x!=d->status.cursor_mm.x ||
        first.y!=d->status.cursor_mm.y) return 0;
    Segment *s=n>1 ? calloc(n-1,sizeof(*s)) : NULL;
    if (n>1 && !s) return 0;
    MotionPoint cursor=first,motor=d->status.motor_mm;
    int px=d->direction_x,py=d->direction_y;
    double total=0;
    for (size_t i=0;i<n-1;i++) {
        s[i].start=cursor; s[i].motor_start=motor; s[i].begin=total;
        if (!point(d->config,p[i+1],&s[i].end) ||
            !profile(d->config,&s[i],feed,px,py)) { free(s); return 0; }
        total+=s[i].duration; s[i].finish=total;
        motor.x+=s[i].unit.x*s[i].travel; motor.y+=s[i].unit.y*s[i].travel;
        if (!isfinite(total) || !isfinite(motor.x) || !isfinite(motor.y)) {
            free(s); return 0;
        }
        cursor=s[i].end; px=s[i].direction_x; py=s[i].direction_y;
    }
    free(d->segments); d->segments=s; d->index=0; d->segment_time=0;
    d->total=total; d->next_sample=0; d->status.elapsed_s=0;
    d->status.remaining_s=total; d->status.count=n-1; d->status.completed=0;
    d->status.drawing=n>1; d->status.motor_velocity_mm_s=(MotionPoint){0,0};
    return EtchDeviceTick(d,0);
}
int EtchDeviceSubmit(EtchDevice *d,const MotionPoint *p,size_t n,double feed)
{
    return isfinite(feed) && feed>0 ? submit(d,p,n,feed) : 0;
}
int EtchDeviceSubmitHardware(EtchDevice *d,const MotionPoint *p,size_t n)
{
    return submit(d,p,n,INFINITY);
}
