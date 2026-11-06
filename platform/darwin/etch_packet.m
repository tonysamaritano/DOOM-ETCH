// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_packet.h"
#include <math.h>
static BOOL pair(id value,BOOL positive)
{
    if (![value isKindOfClass:NSArray.class] || [value count]!=2) return NO;
    for (id number in value)
        if (![number isKindOfClass:NSNumber.class] || !isfinite([number doubleValue]) ||
            (positive && [number doubleValue]<=0)) return NO;
    return YES;
}
static BOOL valid(NSData *pixels,NSDictionary *m)
{
    if (![pixels isKindOfClass:NSData.class] || ![m isKindOfClass:NSDictionary.class]) return NO;
    NSString *kind=m[@"kind"],*mode=m[@"mode"];
    if (![kind isKindOfClass:NSString.class] || ![mode isKindOfClass:NSString.class] ||
        ![m[@"width"] isKindOfClass:NSNumber.class] || ![m[@"height"] isKindOfClass:NSNumber.class]) return NO;
    BOOL bgra=[kind isEqualToString:@"bgra"],indexed=[kind isEqualToString:@"indexed"];
    if (!(bgra || indexed) || [m[@"width"] doubleValue]!=(bgra ? 768 : 320) ||
        [m[@"height"] doubleValue]!=(bgra ? 576 : 200) || pixels.length!=(bgra ? 768*576*4 : 64000+1024)) return NO;
    if ([mode isEqualToString:@"physical"]) {
        if (!pair(m[@"cursor_mm"],NO) || !pair(m[@"motor_mm"],NO) || !pair(m[@"travel_mm"],YES) ||
            ![m[@"physical_seconds"] isKindOfClass:NSNumber.class] ||
            ![m[@"knob_mm_turn"] isKindOfClass:NSNumber.class]) return NO;
        double seconds=[m[@"physical_seconds"] doubleValue],knob=[m[@"knob_mm_turn"] doubleValue];
        if (!isfinite(seconds) || seconds<0 || !isfinite(knob) || knob<=0) return NO;
        for (unsigned i=0;i<2;i++) {
            double cursor=[m[@"cursor_mm"][i] doubleValue],travel=[m[@"travel_mm"][i] doubleValue];
            if (cursor<0 || cursor>travel) return NO;
        }
    } else if (!([mode isEqualToString:@"visual"] && bgra) &&
               !([mode isEqualToString:@"game"] && indexed)) return NO;
    return YES;
}
