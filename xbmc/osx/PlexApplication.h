//
//  PlexApplication.h
//  Plex
//
//  Created by Elan Feingold on 7/16/2008.
//  Copyright 2008 Blue Mandrill Design. All rights reserved.
//
//
#import <Cocoa/Cocoa.h>
#include <SDL/SDL.h>

#define PLEX_MESSAGE SDL_USEREVENT

@interface PlexApplication : NSApplication
- (void)terminate:(id)sender;
- (IBAction) quit:(id)sender;
- (IBAction)fullScreenToggle:(id)sender;
- (void) finishLaunching;
@end
