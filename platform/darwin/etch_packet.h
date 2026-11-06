// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <Foundation/Foundation.h>
/* Immutable LE JSON-length + metadata + packed pixels, bounded to one display. */
NSData *EtchPlayPacket(NSData *pixels,NSDictionary *metadata,uint64_t sequence);
/* Main-thread transport: one delivered frame and one replaceable latest frame. */
@interface EtchFrameQueue : NSObject
- (instancetype)initWithLimit:(unsigned)limit;
- (BOOL)publishPixels:(NSData *)pixels metadata:(NSDictionary *)metadata;
- (NSData *)take;
- (BOOL)acknowledge:(uint64_t)sequence;
- (void)discardLatest;
- (void)stop;
@property(nonatomic,readonly) unsigned presented;
@end
