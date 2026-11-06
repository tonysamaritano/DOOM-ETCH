// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_play.h"
#import "etch_packet.h"
#import "etch_gpu.h"
#import <WebKit/WebKit.h>
#include "i_system.h"
#include <string.h>
static BOOL argument(int argc,char **argv,const char *name)
{
    for (int i=1;i<argc;i++) if (!strcmp(argv[i],name)) return YES;
    return NO;
}
const char *EtchPlayArgumentError(int argc,char **argv)
{
    BOOL play=argument(argc,argv,"--play-on-etch");
    BOOL physical=argument(argc,argv,"--etch-via-gcode");
    if (play && argument(argc,argv,"--etch-gcode"))
        return "--play-on-etch conflicts with standalone --etch-gcode";
    if (!play && physical) return "--etch-via-gcode requires --play-on-etch";
    if (!play && argument(argc,argv,"--fullscreen")) return "--fullscreen requires --play-on-etch";
    if (play && argument(argc,argv,"--etch-time-scale"))
        return "--etch-time-scale is only valid with standalone --etch-gcode";
    if (physical && argument(argc,argv,"--etch-gpu-rate"))
        return "--etch-gpu-rate is a visual control; --etch-via-gcode is fixed physical 1x";
    if (physical && argument(argc,argv,"--etch-gpu-instant"))
        return "--etch-gpu-instant is a visual control; --etch-via-gcode is fixed physical 1x";
    if (physical && argument(argc,argv,"--etch-gpu"))
        return "--etch-gpu conflicts with physical --etch-via-gcode";
    return NULL;
}
NSURL *EtchPlayResource(NSURL *root,NSURL *request,NSString **mime)
{
    if (![request.scheme isEqualToString:@"doomrig"] || ![request.host isEqualToString:@"local"]) return nil;
    NSString *path=request.path.stringByRemovingPercentEncoding;
    if (!path || [path.pathComponents containsObject:@".."]) return nil;
    NSString *relative=[path stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"/"]];
    if (!relative.length) relative=@"index.html";
    NSURL *file=[[root URLByAppendingPathComponent:relative] URLByResolvingSymlinksInPath];
    NSString *base=root.URLByResolvingSymlinksInPath.path;
    if (![file.path hasPrefix:[base stringByAppendingString:@"/"]]) return nil;
    NSDictionary *types=@{@"html":@"text/html",@"js":@"text/javascript",@"css":@"text/css",
        @"glb":@"model/gltf-binary",@"png":@"image/png",@"jpg":@"image/jpeg",@"jpeg":@"image/jpeg"};
    NSString *type=types[file.pathExtension.lowercaseString];
    BOOL directory=NO;
    if (!type || ![NSFileManager.defaultManager fileExistsAtPath:file.path isDirectory:&directory] || directory) return nil;
    if (mime) *mime=type;
    return file;
}
@interface EtchPlayView () <WKURLSchemeHandler,WKScriptMessageHandler,WKNavigationDelegate>
@end
@implementation EtchPlayView {
    WKWebView *_web;
    NSURL *_assets;
    EtchFrameQueue *_frames;
    NSString *_failure;
    BOOL _ready,_stopped,_reading;
    unsigned _generation;
}
- (unsigned)presented { return _frames.presented; }
- (NSString *)failure { return _failure; }
