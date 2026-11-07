// SPDX-License-Identifier: GPL-2.0-or-later
#include "trace.h"
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
    double reciprocal=1/step;
    double maximum=isfinite(reciprocal) ? floor(extent*reciprocal) : floor(extent/step);
    double index=isfinite(reciprocal) ? round(value*reciprocal) : round(value/step);
    index=fmin(index,maximum);
    double result=isfinite(reciprocal) ? index/reciprocal : index*step;
    if (result>extent) {
        index=fmax(0,index-1);
        result=isfinite(reciprocal) ? index/reciprocal : index*step;
    }
    return result;
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
        if (!isfinite(total) || (s[i].duration>0 && total<=s[i].begin) ||
            !isfinite(motor.x) || !isfinite(motor.y)) {
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
static double distance_at(const Segment *s,double t,double *velocity)
{
    if (t>=s->duration) { *velocity=0; return s->travel; }
    if (t<s->ramp) { *velocity=s->acceleration*t; return *velocity*t/2; }
    double left=s->duration-t;
    if (left<s->ramp) {
        *velocity=s->acceleration*left;
        return s->travel-*velocity*left/2;
    }
    *velocity=s->peak;
    return s->peak*(t-s->ramp/2);
}
/* Fixed geometric samples, never tick-fragment rasterization: identical ink
 * for every clock partition, bounded independently of the hardware step. */
static void ink_progress(EtchDevice *d,const Segment *s,double drawn)
{
    if (!s->length || drawn<=0) return;
    double dx=(s->end.x-s->start.x)/d->config.width_mm*(ETCH_INK_WIDTH-1);
    double dy=(s->end.y-s->start.y)/d->config.height_mm*(ETCH_INK_HEIGHT-1);
    unsigned samples=(unsigned)ceil(fmax(fabs(dx),fabs(dy)));
    if (!samples) samples=1;
    while (d->next_sample<=samples &&
           (double)d->next_sample/samples <= drawn/s->length) {
        double fraction=(double)d->next_sample++/samples;
        double x=(s->start.x+(s->end.x-s->start.x)*fraction)/d->config.width_mm;
        double y=(s->start.y+(s->end.y-s->start.y)*fraction)/d->config.height_mm;
        unsigned ix=(unsigned)lround(x*(ETCH_INK_WIDTH-1));
        unsigned iy=(unsigned)lround(y*(ETCH_INK_HEIGHT-1));
        d->ink[iy*ETCH_INK_WIDTH+ix]=ETCH_INK_FRESH;
    }
}
static void begin_erase(EtchDevice *d)
{
    d->erase_requested=0; d->erase_time=0; d->status.erasing=1;
    d->status.erase_left_s=d->config.erase_seconds;
}
static void finish_erase(EtchDevice *d)
{
    const unsigned cells=ETCH_INK_WIDTH*ETCH_INK_HEIGHT;
    unsigned marks=0;
    for (unsigned i=0;i<cells;i++) marks+=d->ink[i]!=ETCH_INK_EMPTY;
    unsigned keep=(unsigned)floor(marks*d->config.ghost_fraction);
    unsigned seed=(++d->erase_count%cells)*7919%cells;
    /* 9973 is coprime to 64000: every cell is visited exactly once. */
    for (unsigned i=0;i<cells;i++) {
        unsigned at=(i*9973+seed)%cells;
        if (d->ink[at]) {
            d->ink[at]=keep ? ETCH_INK_GHOST : ETCH_INK_EMPTY;
            if (keep) keep--;
        }
    }
    d->status.erasing=0; d->status.erase_left_s=0;
}
int EtchDeviceRequestErase(EtchDevice *d)
{
    if (!d) return 0;
    if (d->status.erasing || d->erase_requested) return 1;
    if (d->status.drawing) {
        d->erase_requested=1; d->total=d->segments[d->index].finish;
        d->status.remaining_s=d->total-d->status.elapsed_s;
    } else begin_erase(d);
    return 1;
}
static void position_at(EtchDevice *d,const Segment *s)
{
    double velocity, distance=distance_at(s,d->segment_time,&velocity);
    double drawn=d->segment_time>=s->duration ? s->length :
        fmin(s->length,fmax(0,distance-s->slack));
    MotionPoint cursor={s->start.x+s->unit.x*drawn,s->start.y+s->unit.y*drawn};
    d->status.cursor_mm=(MotionPoint){quantize(cursor.x,d->config.x.step_mm,d->config.width_mm),
        quantize(cursor.y,d->config.y.step_mm,d->config.height_mm)};
    d->status.motor_mm=(MotionPoint){s->motor_start.x+s->unit.x*distance,
                                    s->motor_start.y+s->unit.y*distance};
    d->status.motor_velocity_mm_s=(MotionPoint){s->unit.x*velocity,s->unit.y*velocity};
    ink_progress(d,s,drawn);
}
static void complete_segment(EtchDevice *d,const Segment *s)
{
    d->status.cursor_mm=s->end;
    d->status.elapsed_s=s->finish; d->status.completed=++d->index;
    d->direction_x=s->direction_x; d->direction_y=s->direction_y;
    d->segment_time=0; d->next_sample=0;
    if (d->erase_requested || d->index==d->status.count) {
        d->status.drawing=0; d->status.motor_velocity_mm_s=(MotionPoint){0,0};
        if (d->erase_requested) begin_erase(d);
    }
}
int EtchDeviceReady(const EtchDevice *d)
{
    return d && !d->status.drawing && !d->status.erasing;
}
EtchStatus EtchDeviceGetStatus(const EtchDevice *d)
{
    return d ? d->status : (EtchStatus){0};
}
const unsigned char *EtchDeviceInk(const EtchDevice *d) { return d ? d->ink : NULL; }
int EtchDeviceTick(EtchDevice *d,double seconds)
{
    if (!d || !isfinite(seconds) || seconds<0) return 0;
    while (d->status.drawing) {
        Segment *s=&d->segments[d->index];
        double left=s->duration-d->segment_time;
        /* Callers measure time left on the absolute clock, which is coarser than
         * a short segment's local time, so reaching its finish there completes it. */
        int complete=seconds>=left || (seconds>0 && (left-seconds<=1e-12*s->duration
            || s->begin+d->segment_time+seconds>=s->finish));
        double used=complete ? left : seconds;
        d->segment_time=complete ? s->duration : d->segment_time+used;
        seconds=fmax(0,seconds-used);
        position_at(d,s);
        d->status.elapsed_s=s->begin+d->segment_time;
        if (complete) complete_segment(d,s);
        d->status.remaining_s=fmax(0,d->total-d->status.elapsed_s);
        if (!complete) break;
    }
    if (d->status.erasing) {
        double left=d->config.erase_seconds-d->erase_time;
        if (seconds>=left || (seconds>0 && left-seconds<=1e-12*d->config.erase_seconds))
            finish_erase(d);
        else {
            d->erase_time+=seconds;
            d->status.erase_left_s=d->config.erase_seconds-d->erase_time;
        }
    }
    return 1;
}

int EtchDeviceProfile(const EtchDevice *d,size_t index,EtchTraceSegment *out)
{
    if (!d || !out || index>=d->status.count || !d->segments) return 0;
    const Segment *s=&d->segments[index];
    *out=(EtchTraceSegment){s->begin,s->duration,s->ramp,s->peak,s->acceleration,
        s->slack,s->length,s->start.x,s->start.y,s->unit.x,s->unit.y,
        s->motor_start.x,s->motor_start.y,0,0};
    return 1;
}
