// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_gpu.h"
#include <assert.h>
static void waitFor(dispatch_semaphore_t signal)
{
    assert(dispatch_semaphore_wait(signal,dispatch_time(DISPATCH_TIME_NOW,10*NSEC_PER_SEC))==0);
}
int main(void)
{
    @autoreleasepool {
        id<MTLDevice> device=MTLCreateSystemDefaultDevice();assert(device);
        id<MTLCommandQueue> queue=[device newCommandQueue];assert(queue);
        EtchGPUInit(device,1200);assert(!EtchGPUError());
        dispatch_semaphore_t entered=dispatch_semaphore_create(0);
        dispatch_semaphore_t release=dispatch_semaphore_create(0);
        dispatch_semaphore_t finished=dispatch_semaphore_create(0);
        __block NSData *retained=nil;
        assert(EtchGPUReadPixels(queue,^(NSData *data){
            assert(!NSThread.isMainThread && data.length==768*576*4);
            retained=data;dispatch_semaphore_signal(entered);
            waitFor(release);
            const unsigned char *p=data.bytes;
            assert(p[3]==255 && p[data.length-1]==255);
            dispatch_semaphore_signal(finished);
        }));
        waitFor(entered);
        EtchGPUInvalidate();EtchGPUShutdown(); // Must not wait on the blocked consumer.
        assert(retained.length==768*576*4);
        assert(!EtchGPUReadPixels(queue,^(NSData *data){(void)data;assert(0);}));
        dispatch_semaphore_signal(release);waitFor(finished);
        puts("PASS actual offscreen pixels outlive invalidation and main-thread shutdown");
    }
    return 0;
}
