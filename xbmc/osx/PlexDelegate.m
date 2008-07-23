//
//  PlexDelegate.m
//  XBMC
//
//  Created by Elan Feingold on 7/17/2008.
//  Copyright 2008 Blue Mandrill Design. All rights reserved.
//

#import "PlexDelegate.h"

@implementation PlexDelegate
- (float)appNameLabelFontSize;
{
    return 24.0;
}

- (NSString*)versionString;
{
    NSBundle *mainBundle = [NSBundle mainBundle];
    NSDictionary *infoDict = [mainBundle infoDictionary];
 
    NSString *mainString = [infoDict valueForKey:@"CFBundleShortVersionString"];
    NSString *subString = [infoDict valueForKey:@"CFBundleVersion"];
    return [NSString stringWithFormat:@"Version %@ (%@)", mainString, subString];
}
@end
