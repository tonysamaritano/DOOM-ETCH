// SPDX-License-Identifier: GPL-2.0-or-later
#import "etch_play.h"
#include <assert.h>
int main(void)
{
    @autoreleasepool {
        NSFileManager *files=NSFileManager.defaultManager;
        NSURL *base=[NSURL fileURLWithPath:[NSTemporaryDirectory()
            stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
        NSURL *root=[base URLByAppendingPathComponent:@"resources"];
        assert([files createDirectoryAtURL:root withIntermediateDirectories:YES attributes:nil error:nil]);
        NSArray *resources=@[@[@"index.html",@"text/html"],@[@"shared/store.js",@"text/javascript"],
            @[@"vendor/three/build/three.module.js",@"text/javascript"],@[@"style.css",@"text/css"],
            @[@"rig.glb",@"model/gltf-binary"],@[@"bezel-note.png",@"image/png"],
            @[@"cheat-note.jpg",@"image/jpeg"]];
        for(NSArray *item in resources) {
            NSURL *file=[root URLByAppendingPathComponent:item[0]];
            assert([files createDirectoryAtURL:file.URLByDeletingLastPathComponent
                withIntermediateDirectories:YES attributes:nil error:nil]);
            assert([[NSData data] writeToURL:file atomically:YES]);
            NSString *mime=nil;
            NSURL *request=[NSURL URLWithString:[@"doomrig://local/" stringByAppendingString:item[0]]];
            assert([EtchPlayResource(root,request,&mime).path isEqualToString:file.path]);
            assert([mime isEqualToString:item[1]]);
        }
        NSString *mime=nil;
        assert(EtchPlayResource(root,[NSURL URLWithString:@"doomrig://local/"],&mime));
        assert([mime isEqualToString:@"text/html"]);
        NSURL *outside=[base URLByAppendingPathComponent:@"outside.js"];
        assert([[NSData data] writeToURL:outside atomically:YES]);
        assert([files createSymbolicLinkAtPath:[root.path stringByAppendingPathComponent:@"escape.js"]
            withDestinationPath:outside.path error:nil]);
        for(NSString *address in @[@"https://local/index.html",@"doomrig://elsewhere/index.html",
            @"doomrig://local:123/index.html",@"doomrig://user@local/index.html",
            @"doomrig://user:password@local/index.html",@"doomrig://local/escape.js",
            @"doomrig://local/%2e%2e/outside.js",@"doomrig://local/%252e%252e/outside.js",
            @"doomrig://local/missing.js",@"doomrig://local/shared/",
            @"doomrig://local/package.json"]) {
            mime=@"unchanged";
            assert(!EtchPlayResource(root,[NSURL URLWithString:address],&mime));
            assert([mime isEqualToString:@"unchanged"]);
        }
        assert([files removeItemAtURL:base error:nil]);
        puts("PASS actual staged resource paths and MIME types");
    }
    return 0;
}
