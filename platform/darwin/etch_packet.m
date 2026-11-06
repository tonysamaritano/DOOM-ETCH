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
NSData *EtchPlayPacket(NSData *pixels,NSDictionary *metadata,uint64_t sequence)
{
    if (!sequence || sequence>9007199254740991ULL || !valid(pixels,metadata)) return nil;
    NSMutableDictionary *m=[metadata mutableCopy];
    m[@"version"]=@1; m[@"sequence"]=@(sequence);
    if (![NSJSONSerialization isValidJSONObject:m]) return nil;
    NSData *json=[NSJSONSerialization dataWithJSONObject:m options:0 error:nil];
    if (!json.length || json.length>16384) return nil;
    uint32_t length=(uint32_t)json.length;
    unsigned char prefix[4]={length&255,(length>>8)&255,(length>>16)&255,(length>>24)&255};
    NSMutableData *packet=[NSMutableData dataWithBytes:prefix length:4];
    [packet appendData:json]; [packet appendData:pixels];
    return [packet copy];
}
@implementation EtchFrameQueue {
    NSData *_pixels,*_metadata;
    uint64_t _delivered,_outstanding;
    unsigned _limit,_presented;
    BOOL _stopped;
}
- (instancetype)initWithLimit:(unsigned)limit
{
    if ((self=[super init])) _limit=limit;
    return self;
}
- (BOOL)publishPixels:(NSData *)pixels metadata:(NSDictionary *)metadata
{
    if (_stopped || !valid(pixels,metadata) || ![NSJSONSerialization isValidJSONObject:metadata]) return NO;
    NSData *json=[NSJSONSerialization dataWithJSONObject:metadata options:0 error:nil];
    if (!json.length || json.length>16384) return NO;
    _pixels=[pixels copy]; _metadata=json;
    return YES;
}
