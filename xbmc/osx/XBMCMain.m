//
//  XBMCMain.m
//  XBMC
//
//  Created by Elan Feingold on 2/27/2008.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "XBMCMain.h"
#import "AppleRemote.h"
#import <ExceptionHandling/ExceptionHandling.h>

@implementation XBMCMain

static XBMCMain *_o_sharedMainInstance = nil;

+ (XBMCMain *)sharedInstance
{
  return _o_sharedMainInstance ? _o_sharedMainInstance : [[self alloc] init];
}

- (id)init
{
  if( _o_sharedMainInstance)
      [self dealloc];
  else
      _o_sharedMainInstance = [super init];

  o_remote = [[AppleRemote alloc] init];
  [o_remote setClickCountEnabledButtons: kRemoteButtonPlay];
  [o_remote setDelegate: _o_sharedMainInstance];
  
  // Start listening in exclusive mode.
  //[o_remote startListening: self];

  [[NSExceptionHandler defaultExceptionHandler] setDelegate:self];
  [[NSExceptionHandler defaultExceptionHandler] setExceptionHandlingMask:
   NSLogUncaughtExceptionMask |
   NSLogUncaughtSystemExceptionMask |
   NSLogUncaughtRuntimeErrorMask |
   NSLogTopLevelExceptionMask |
   NSLogOtherExceptionMask];
  
  return _o_sharedMainInstance;
}

/* Apple Remote callback */
- (void) appleRemoteButton: (AppleRemoteEventIdentifier)buttonIdentifier
               pressedDown: (BOOL) pressedDown
                clickCount: (unsigned int) count
{
    // Pass event to thunk.
    Cocoa_OnAppleRemoteKey(pApplication, buttonIdentifier, pressedDown, count);
}

/* NSExceptionHandling Delegate */
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldLogException:(NSException *)e mask:(unsigned int)aMask 
{
  static BOOL handlingException = NO;
  
  // Check to see if the exceptionHandler is being recursively called...if so then there is an exception being
  // generated in this method.
  if (handlingException) {
    NSLog(@"**Exception** Fatal Error: Exception handler is recursing.");
    NSLog(@"**Exception** %@", [e reason]);
    return NO;
  }
  
  handlingException = YES;
  NSLog(@"FATAL Exception: %@", [e reason]);
  
  NSString *stack = [[e userInfo] objectForKey:NSStackTraceKey];
  if (stack) 
  {
    NSTask *ls = [[NSTask alloc] init];
    NSString *pid = [[NSNumber numberWithInt:[[NSProcessInfo processInfo] processIdentifier]] stringValue];
    NSMutableArray *args = [NSMutableArray arrayWithCapacity:20];
    
    [args addObject:@"-p"];
    [args addObject:pid];
    
    // Note: function addresses are separated by double spaces, not a single space.
    [args addObjectsFromArray:[stack componentsSeparatedByString:@"  "]];
    
    [ls setLaunchPath:@"/usr/bin/atos"];
    [ls setArguments:args];
    [ls launch];
    [ls release];
  }
  handlingException = NO;
  
  return YES;
}

- (void)setApplication: (void*) application;
{
  pApplication = application;
}

@end
