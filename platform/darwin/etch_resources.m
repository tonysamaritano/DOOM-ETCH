// SPDX-License-Identifier: GPL-2.0-or-later
#import <Foundation/Foundation.h>
NSURL *EtchPlayResource(NSURL *root,NSURL *request,NSString **mime)
{
    if (![request.scheme isEqualToString:@"doomrig"] || ![request.host isEqualToString:@"local"] ||
        request.user || request.password || request.port) return nil;
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
BOOL EtchPlayNavigation(NSURL *url,BOOL mainFrame,BOOL *started)
{
    if (!started || *started || !mainFrame ||
        ![url.scheme isEqualToString:@"doomrig"] || ![url.host isEqualToString:@"local"] ||
        url.user || url.password || url.port || url.query || url.fragment ||
        ![url.path isEqualToString:@"/index.html"]) return NO;
    *started=YES;
    return YES;
}
