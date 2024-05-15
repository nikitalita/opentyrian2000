#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#import "macos-bundle.h"

extern "C" const char *getBundlePath(void)
{
    NSString *bp = [[NSBundle mainBundle] resourcePath];
    
    return ((const char *)[bp cStringUsingEncoding:NSUTF8StringEncoding]);
}
