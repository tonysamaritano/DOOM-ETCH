// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
/* NULL means valid; diagnostics are constant and identify the offending flag. */
const char *EtchPlayArgumentError(int argc,char **argv);
BOOL EtchPlayNavigation(NSURL *request,BOOL mainFrame,BOOL *started);
NSURL *EtchPlayResource(NSURL *root,NSURL *request,NSString **mime);
@interface EtchPlayView : NSView
- (instancetype)initWithFrame:(NSRect)rect limit:(unsigned)limit;
- (void)waitForModel;
- (void)publishIndices:(const unsigned char *)pixels palette:(const unsigned char *)palette
              metadata:(NSDictionary *)metadata;
- (void)drawEtchWithQueue:(id<MTLCommandQueue>)queue;
- (void)updateVisibility:(BOOL)visible now:(double)now;
- (void)invalidate;
- (void)stop;
@property(nonatomic,readonly) unsigned presented;
@property(nonatomic,readonly) NSString *failure;
@end
