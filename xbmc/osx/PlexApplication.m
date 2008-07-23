//
//  PlexApplication.m
//  Plex
//
//  Created by Elan Feingold on 7/16/2008.
//  Copyright 2008 Blue Mandrill Design. All rights reserved.
//
#import <unistd.h>
#include "SDL/SDL.h"
#import "PlexApplication.h"

static int           gArgc;
static const char  **gArgv;
static BOOL          gFinderLaunch;

@implementation PlexApplication

- (void) finishLaunching
{
  [super finishLaunching];
  SDL_main(gArgc, (char** )gArgv);
}

/* Invoked from the Quit menu item */
- (IBAction)quit:(id)sender
{
  /* Post a SDL_QUIT event */
  SDL_Event event;
  event.type = SDL_QUIT;
  SDL_PushEvent(&event);
}

- (IBAction)fullScreenToggle:(id)sender
{
  /* Post an SDL key event */
  SDL_Event event;
  memset(&event, 0, sizeof(event));
  event.type = SDL_KEYDOWN;
  event.key.keysym.unicode = '\\';
  SDL_PushEvent(&event);
}

- (void)sendEvent:(NSEvent *)anEvent 
{
  if(NSKeyDown == [anEvent type] || NSKeyUp == [anEvent type]) 
  {
    if([anEvent modifierFlags] & NSCommandKeyMask)
			[super sendEvent: anEvent];
  }
  else
  {
      [super sendEvent: anEvent];
  }
}

@end

#ifdef main
#  undef main
#endif

/* Main entry point to executable - should *not* be SDL_main! */
int main(int argc, const char **argv)
{
  /* Copy the arguments into a global variable */
  /* This is passed if we are launched by double-clicking */
  if ( argc >= 2 && strncmp (argv[1], "-psn", 4) == 0 ) {
    gArgv = (const char **)malloc(sizeof (char *) * 2);
    gArgv[0] = argv[0];
    gArgv[1] = NULL;
    gArgc = 1;
    gFinderLaunch = YES;
  } 
  else 
  {
    int i;
    gArgc = argc;
    gArgv = (const char **)malloc(sizeof (char *) * (argc+1));
    for (i = 0; i <= argc; i++)
        gArgv[i] = argv[i];
    gFinderLaunch = NO;
  }

  NSApplicationMain(argc, argv);  
  return 0;
}