// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_packet.h"
#import "etch_play.h"
#include <assert.h>
int main(void)
{
    @autoreleasepool {
        BOOL started=NO;
        NSURL *url=[NSURL URLWithString:@"doomrig://local/index.html"];
        assert(!EtchPlayNavigation(url,NO,&started) && !started);
        assert(EtchPlayNavigation(url,YES,&started) && started);
        assert(!EtchPlayNavigation(url,YES,&started));
        started=NO;
        assert(!EtchPlayNavigation([NSURL URLWithString:@"doomrig://user@local/index.html"],YES,&started));
        assert(!started);
        EtchFrameQueue *queue=[[EtchFrameQueue alloc] initWithLimit:0];
        NSData *pixels=[NSMutableData dataWithLength:64000+1024];
        NSDictionary *metadata=@{@"kind":@"indexed",@"mode":@"game",@"width":@320,@"height":@200};
        assert([queue publishPixels:pixels metadata:metadata] && [queue take].length);
        assert([queue acknowledgementHealthyAt:0 visible:YES]);
        assert([queue acknowledgementHealthyAt:5 visible:YES]);
        assert([queue acknowledgementHealthyAt:1000 visible:NO]);
        assert([queue acknowledgementHealthyAt:2000 visible:NO]);
        assert([queue acknowledgementHealthyAt:2001 visible:YES]);
        assert([queue acknowledgementHealthyAt:2005 visible:YES]);
        assert(![queue acknowledgementHealthyAt:2012 visible:YES]);
        assert([queue acknowledge:1]);
        assert([queue publishPixels:pixels metadata:metadata] && [queue take].length);
        assert([queue acknowledgementHealthyAt:3000 visible:YES]);
        assert([queue acknowledgementHealthyAt:3010 visible:YES]);
        assert([queue acknowledge:2]);
        assert([queue acknowledgementHealthyAt:4000 visible:YES]);
        [queue stop];
    }
    return 0;
}
