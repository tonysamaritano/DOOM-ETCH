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
