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
- (instancetype)initWithFrame:(NSRect)rect limit:(unsigned)limit
{
    if (!(self=[super initWithFrame:rect])) return nil;
    NSBundle *bundle=NSBundle.mainBundle;
    NSURL *base=[bundle.bundleURL.pathExtension isEqualToString:@"app"] ?
        bundle.resourceURL : bundle.executableURL.URLByDeletingLastPathComponent;
    _assets=[base URLByAppendingPathComponent:@"etch-play" isDirectory:YES];
    _frames=[[EtchFrameQueue alloc] initWithLimit:limit];
    WKWebViewConfiguration *config=[WKWebViewConfiguration new];
    [config setURLSchemeHandler:self forURLScheme:@"doomrig"];
    [config.userContentController addScriptMessageHandler:self name:@"presentation"];
    _web=[[WKWebView alloc] initWithFrame:self.bounds configuration:config];
    _web.autoresizingMask=NSViewWidthSizable | NSViewHeightSizable;
    _web.navigationDelegate=self;
    [self addSubview:_web];
    [_web loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"doomrig://local/index.html"]]];
    return self;
}
- (void)waitForModel
{
    NSDate *deadline=[NSDate dateWithTimeIntervalSinceNow:45];
    while (!_ready && !_failure && !_stopped && deadline.timeIntervalSinceNow>0)
        [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];
    if (!_ready) I_Error("Cannot load the live rig: %s",(_failure ?: @"WebKit model startup timed out").UTF8String);
}
- (void)publishIndices:(const unsigned char *)pixels palette:(const unsigned char *)palette
              metadata:(NSDictionary *)metadata
{
    if (_stopped || !pixels || !palette) return;
    NSMutableData *data=[NSMutableData dataWithBytes:pixels length:64000];
    [data appendBytes:palette length:1024];
    if (![_frames publishPixels:data metadata:metadata]) _failure=@"Invalid indexed rig frame";
}
- (void)drawEtchWithQueue:(id<MTLCommandQueue>)queue
{
    if (_stopped || !_ready || _reading) return;
    unsigned epoch=_generation;
    __weak EtchPlayView *weakSelf=self;
    _reading=EtchGPUReadPixels(queue,^(NSData *pixels) {
        dispatch_async(dispatch_get_main_queue(),^{
            EtchPlayView *view=weakSelf;
            if (!view || view->_stopped) return;
            view->_reading=NO;
            if (epoch!=view->_generation) return;
            NSDictionary *metadata=@{@"kind":@"bgra",@"mode":@"visual",@"width":@768,@"height":@576};
            if (!pixels || ![view->_frames publishPixels:pixels metadata:metadata])
                view->_failure=@"Cannot publish the Metal rig display";
        });
    });
}
- (void)webView:(WKWebView *)web startURLSchemeTask:(id<WKURLSchemeTask>)task
{
    (void)web;
    if (_stopped) return;
    NSURL *url=task.request.URL; NSString *mime=nil; NSData *data=nil;
    if ([url.scheme isEqualToString:@"doomrig"] && [url.host isEqualToString:@"local"] &&
        [url.path isEqualToString:@"/frame"]) {
        data=[_frames take]; mime=@"application/octet-stream";
    } else {
        NSURL *file=EtchPlayResource(_assets,url,&mime);
        if (file) data=[NSData dataWithContentsOfURL:file options:NSDataReadingMappedIfSafe error:nil];
    }
    if (!data) {
        _failure=[NSString stringWithFormat:@"Missing or invalid live rig resource %@",url.path];
        [task didFailWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist userInfo:nil]];
        return;
    }
    NSHTTPURLResponse *response=[[NSHTTPURLResponse alloc] initWithURL:url statusCode:200
        HTTPVersion:@"HTTP/1.1" headerFields:@{@"Content-Type":mime,@"Cache-Control":@"no-store"}];
    [task didReceiveResponse:response]; [task didReceiveData:data]; [task didFinish];
}
