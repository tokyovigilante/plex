//
//  PlexApplication.h
//  Plex
//
//  Created by Elan Feingold on 7/16/2008.
//  Copyright 2008 Blue Mandrill Design. All rights reserved.
//
//
#import <Cocoa/Cocoa.h>

@interface PlexApplication : NSApplication 
- (IBAction) quit:(id)sender;
- (IBAction)fullScreenToggle:(id)sender;
- (void) finishLaunching;
@end
