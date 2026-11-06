// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_play.h"
#import "etch_packet.h"
#import "etch_gpu.h"
#import <WebKit/WebKit.h>
#include "i_system.h"
#include <string.h>
#include <math.h>
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
@interface EtchPlayView () <WKURLSchemeHandler,WKScriptMessageHandler,WKNavigationDelegate>
@end
@implementation EtchPlayView {
    WKWebView *_web;
    NSURL *_assets;
    EtchFrameQueue *_frames;
    NSString *_failure;
    BOOL _ready,_stopped,_reading,_navigationStarted;
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
    _generation++;
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
        !url.user && !url.password && !url.port && [url.path isEqualToString:@"/frame"]) {
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
- (void)webView:(WKWebView *)web stopURLSchemeTask:(id<WKURLSchemeTask>)task
{
    // Scheme responses complete synchronously; no deferred task can outlive cancellation.
    (void)web; (void)task;
}
- (void)userContentController:(WKUserContentController *)controller didReceiveScriptMessage:(WKScriptMessage *)message
{
    (void)controller;
    if (_stopped || message.webView!=_web || !message.frameInfo.isMainFrame ||
        ![message.body isKindOfClass:NSDictionary.class]) return;
    NSURL *origin=message.frameInfo.request.URL;
    if (![origin.scheme isEqualToString:@"doomrig"] || ![origin.host isEqualToString:@"local"] ||
        origin.user || origin.password || origin.port) return;
    NSDictionary *body=message.body;
    if ([body[@"type"] isEqual:@"ready"]) _ready=YES;
    else if ([body[@"type"] isEqual:@"presented"]) {
        NSNumber *sequence=body[@"sequence"];
        BOOL number=[sequence isKindOfClass:NSNumber.class] &&
            CFGetTypeID((__bridge CFTypeRef)sequence)!=CFBooleanGetTypeID();
        double value=number ? sequence.doubleValue : 0;
        if (isfinite(value) && value>0 && value<=9007199254740991.0 && floor(value)==value)
            [_frames acknowledge:(uint64_t)value];
    } else if ([body[@"type"] isEqual:@"error"])
        _failure=[body[@"message"] description] ?: @"Live rig script failed";
}
- (void)webView:(WKWebView *)web decidePolicyForNavigationAction:(WKNavigationAction *)action
    decisionHandler:(void (^)(WKNavigationActionPolicy))decision
{
    (void)web;
    NSURL *url=action.request.URL;
    BOOL local=!_stopped && EtchPlayNavigation(url,action.targetFrame.isMainFrame,&_navigationStarted);
    if (!local && !_stopped) _failure=@"Unexpected live rig navigation or reload";
    decision(local ? WKNavigationActionPolicyAllow : WKNavigationActionPolicyCancel);
}
- (void)webViewWebContentProcessDidTerminate:(WKWebView *)web
{
    (void)web; _failure=@"Live rig WebContent process terminated";
}
- (void)webView:(WKWebView *)web didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    (void)web; (void)navigation; if (!_stopped) _failure=error.localizedDescription;
}
- (void)webView:(WKWebView *)web didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    (void)web; (void)navigation; if (!_stopped) _failure=error.localizedDescription;
}
- (void)updateVisibility:(BOOL)visible now:(double)now
{
    if (!_stopped && ![_frames acknowledgementHealthyAt:now visible:visible])
        _failure=@"Live rig render acknowledgement timed out";
}
- (void)invalidate
{
    _generation++; [_frames discardLatest];
}
- (void)stop
{
    _stopped=YES; _generation++; [_frames stop];
    [_web.configuration.userContentController removeScriptMessageHandlerForName:@"presentation"];
    [_web stopLoading]; _web.navigationDelegate=nil; [_web removeFromSuperview]; _web=nil;
}
@end
