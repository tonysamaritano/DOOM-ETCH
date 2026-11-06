// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_packet.h"
#include <assert.h>
static NSDictionary *header(NSData *packet)
{
    assert(packet.length>=4);
    const unsigned char *p=packet.bytes;
    uint32_t n=p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24);
    assert(n && n<=16384 && n<=packet.length-4);
    NSData *json=[packet subdataWithRange:NSMakeRange(4,n)];
    NSDictionary *m=[NSJSONSerialization JSONObjectWithData:json options:0 error:nil];
    assert([m isKindOfClass:NSDictionary.class]);return m;
}
static unsigned char firstPixel(NSData *packet)
{
    const unsigned char *p=packet.bytes;
    uint32_t n=p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24);
    return p[4+n];
}
static NSDictionary *game(void)
{
    return @{@"kind":@"indexed",@"width":@320,@"height":@200,@"mode":@"game"};
}
static NSMutableData *pixels(unsigned char value)
{
    NSMutableData *data=[NSMutableData dataWithLength:65024];
    ((unsigned char *)data.mutableBytes)[0]=value;return data;
}
static void ownership(void)
{
    EtchFrameQueue *q=[[EtchFrameQueue alloc] initWithLimit:0];
    assert([q publishPixels:pixels(1) metadata:game()]);
    NSData *a=[q take];assert([header(a)[@"sequence"] unsignedLongLongValue]==1);
    assert([q take].length==0 && q.presented==0);
    assert([q publishPixels:pixels(2) metadata:game()]);
    NSMutableData *c=pixels(3);NSMutableDictionary *meta=[game() mutableCopy];
    assert([q publishPixels:c metadata:meta]);
    ((unsigned char *)c.mutableBytes)[0]=99;meta[@"width"]=@1;
    assert(![q acknowledge:2] && ![q acknowledge:0]);
    assert([q acknowledge:1] && ![q acknowledge:1]);
    NSData *latest=[q take];assert(firstPixel(latest)==3);
    assert([header(latest)[@"width"] intValue]==320);
    assert([header(latest)[@"sequence"] unsignedLongLongValue]==2);
    assert(firstPixel(a)==1 && q.presented==1);
    [q stop];assert(![q acknowledge:2] && [q take].length==0);
    assert(![q publishPixels:pixels(4) metadata:game()]);
}
static void limit(void)
{
    EtchFrameQueue *q=[[EtchFrameQueue alloc] initWithLimit:90];
    for(unsigned n=1;n<=90;n++) {
        assert([q publishPixels:pixels(n) metadata:game()]);
        NSData *packet=[q take];assert([header(packet)[@"sequence"] unsignedIntValue]==n);
        assert([q publishPixels:pixels(255) metadata:game()]);
        assert([q take].length==0 && ![q acknowledge:n+1]);
        assert([q acknowledge:n] && q.presented==n && ![q acknowledge:n]);
    }
    assert([q take].length==0 && q.presented==90);[q stop];
}
