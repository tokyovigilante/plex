//
//  CocoaUtils.m
//  XBMC
//
//  Created by Elan Feingold on 1/5/2008.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOKitLib.h>
#include "CocoaUtils.h"
#import "XBMCMain.h" 
#include <SDL/SDL.h>

#import <IOKit/graphics/IOGraphicsLib.h>
#import <ApplicationServices/ApplicationServices.h>

extern int GetProcessPid(const char* processName);

#define MAX_DISPLAYS 32
static NSWindow* blankingWindows[MAX_DISPLAYS];

void Cocoa_Initialize(void* pApplication)
{
  // Intialize the Apple remote code.
  [[XBMCMain sharedInstance] setApplication: pApplication];
  
  // Initialize.
  int i;
  for (i=0; i<MAX_DISPLAYS; i++)
    blankingWindows[i] = 0;
}

void Cocoa_DisplayError(const char* strError)
{
  NSAlert *alert = [NSAlert alertWithMessageText:@"Fatal Error"
                    defaultButton:@"OK" alternateButton:nil otherButton:nil
                    informativeTextWithFormat:[NSString stringWithUTF8String:strError]];
                    
  [alert runModal];
  [alert release];
}

void InstallCrashReporter() 
{
  if (access("/Library/InputManagers/Smart Crash Reports/Smart Crash Reports.bundle", R_OK) != 0)
    system([[[NSBundle mainBundle] pathForAuxiliaryExecutable:@"CrashReporter"] UTF8String]);
}

void* InitializeAutoReleasePool()
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  return pool;
}

void DestroyAutoReleasePool(void* aPool)
{
  NSAutoreleasePool* pool = (NSAutoreleasePool* )aPool;
  [pool release];
}

void Cocoa_GL_MakeCurrentContext(void* theContext)
{
  NSOpenGLContext* context = (NSOpenGLContext* )theContext;
  [ context makeCurrentContext ];
}

void Cocoa_GL_ReleaseContext(void* theContext)
{
  NSOpenGLContext* context = (NSOpenGLContext* )theContext;
  [ NSOpenGLContext clearCurrentContext ];
  [ context clearDrawable ];
  [ context release ];
}

NSWindow* childWindow = nil;
NSWindow* mainWindow = nil;

void Cocoa_MakeChildWindow()
{
  NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  NSView* view = [context view];
  NSWindow* window = [view window];

  // Create a child window.
  childWindow = [[NSWindow alloc] initWithContentRect:[window frame]
                                            styleMask:NSBorderlessWindowMask
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
                                          
  [childWindow setContentSize:[view frame].size];
  [childWindow setBackgroundColor:[NSColor blackColor]];
  [window addChildWindow:childWindow ordered:NSWindowAbove];
  mainWindow = window;
  //childWindow.alphaValue = 0.5; 
}

void Cocoa_DestroyChildWindow()
{
  if (childWindow != nil)
  {
    [mainWindow removeChildWindow:childWindow];
    [childWindow close];
    childWindow = nil;
  }
}

void Cocoa_GL_SwapBuffers(void* theContext)
{
  [ (NSOpenGLContext*)theContext flushBuffer ];
}

int Cocoa_GetNumDisplays()
{
  CGDirectDisplayID displayArray[MAX_DISPLAYS];
  CGDisplayCount    numDisplays;

  // Get the list of displays.
  CGGetActiveDisplayList(MAX_DISPLAYS, displayArray, &numDisplays);
  return numDisplays;
}

int Cocoa_GetDisplay(int screen)
{
	CGDirectDisplayID displayArray[MAX_DISPLAYS];
	CGDisplayCount    numDisplays;
  
	// Get the list of displays.
	CGGetActiveDisplayList(MAX_DISPLAYS, displayArray, &numDisplays);
	return displayArray[screen];
}

void Cocoa_GetScreenResolutionOfAnotherScreen(int screen, int* w, int* h)
{
  CFDictionaryRef mode = CGDisplayCurrentMode(Cocoa_GetDisplay(screen));
  CFNumberGetValue(CFDictionaryGetValue(mode, kCGDisplayWidth), kCFNumberSInt32Type, w);
  CFNumberGetValue(CFDictionaryGetValue(mode, kCGDisplayHeight), kCFNumberSInt32Type, h);
}

void Cocoa_GetScreenResolution(int* w, int* h)
{
  // Figure out the screen size.
  CGDirectDisplayID display_id = kCGDirectMainDisplay;
  CFDictionaryRef mode  = CGDisplayCurrentMode(display_id);
  
  CFNumberGetValue(CFDictionaryGetValue(mode, kCGDisplayWidth), kCFNumberSInt32Type, w);
  CFNumberGetValue(CFDictionaryGetValue(mode, kCGDisplayHeight), kCFNumberSInt32Type, h);
}

// get a double value from a dictionary
static double getDictDouble(CFDictionaryRef refDict, CFStringRef key)
{
  double double_value;
  CFNumberRef number_value = (CFNumberRef) CFDictionaryGetValue(refDict, key);
  if (!number_value) // if can't get a number for the dictionary
    return -1;  // fail
  if (!CFNumberGetValue(number_value, kCFNumberDoubleType, &double_value)) // or if cant convert it
    return -1; // fail
  return double_value; // otherwise return the long value
}

double Cocoa_GetScreenRefreshRate(int screen)
{
  // Figure out the refresh rate.
  CFDictionaryRef mode = CGDisplayCurrentMode(Cocoa_GetDisplay(screen));
  return (mode != NULL) ? getDictDouble(mode, kCGDisplayRefreshRate) : 0.0f;
}

void QZ_ChangeWindowSize(int w, int h);

static NSView* windowedView = 0;

void* Cocoa_GL_ResizeWindow(void *theContext, int w, int h)
{
  if (!theContext)
    return 0;
  
  NSOpenGLContext* context = Cocoa_GL_GetCurrentContext();
  NSView* view;
  NSWindow* window;
  
  view = [context view];
  
  // If we're moving to full-screen, we've probably changed contexts already,
  // so it's better to grab the view from the saved one.
  //
  if (windowedView != 0)
  {
    // First, resize that view, though.
    if (view)
    {
       window = [view window];

      [window setContentSize:NSMakeSize(w, h)];
      [window update];
      [view setFrameSize:NSMakeSize(w, h)];
      [context update];
      [window center];
    }
  
    view = windowedView;
    window = [view window];
  }
  
  if (view && w>0 && h>0)
  {
    window = [view window];
    if (window)
    {
      [window setContentSize:NSMakeSize(w, h)];
      [window update];
      [view setFrameSize:NSMakeSize(w, h)];
      [context update];
      [window center];
    }
  }
  
  // HACK to make sure SDL realizes the window size changed to help with mouse pointers.
  QZ_ChangeWindowSize(w, h);  

  return context;
}

void Cocoa_GL_BlankOtherDisplays(int screen)
{
  int numDisplays = Cocoa_GetNumDisplays();
  int i = 0;

  // Blank.
  for (i=0; i<numDisplays; i++)
  {
    if (i != screen && blankingWindows[i] == 0)
    {
      // Get the size.
      NSScreen* pScreen = [[NSScreen screens] objectAtIndex:i];
      NSRect    screenRect = [pScreen frame];
          
      // Build a blanking window.
      screenRect.origin.x = 0.0;
      screenRect.origin.y = 0.0;
      blankingWindows[i] = [[NSWindow alloc] initWithContentRect:screenRect
                                             styleMask:NSBorderlessWindowMask
                                             backing:NSBackingStoreBuffered
                                             defer:NO 
                                             screen:pScreen];
                                            
      [blankingWindows[i] setBackgroundColor:[NSColor blackColor]];
      [blankingWindows[i] setLevel:CGShieldingWindowLevel()];
      [blankingWindows[i] makeKeyAndOrderFront:nil];
    }
  } 
}

void Cocoa_GL_UnblankOtherDisplays(int screen)
{
  int numDisplays = Cocoa_GetNumDisplays();
  int i = 0;

  for (i=0; i<numDisplays; i++)
  {
    if (blankingWindows[i] != 0)
    {
      // Get rid of the blanking window we created.
      [blankingWindows[i] close];
      [blankingWindows[i] release];
      blankingWindows[i] = 0;
    }
  }
}

static NSOpenGLContext* lastOwnedContext = 0;
static NSWindow* lastWindow = NULL;

void Cocoa_GL_SetFullScreen(int screen, int width, int height, bool fs, bool blankOtherDisplays, bool fakeFullScreen)
{
  static NSView* lastView = NULL;
  static int fullScreenDisplay = 0;
  static int lastDisplay = -1;

  // If we're already fullscreen then we must be moving to a different display.
  // Recurse to reset fullscreen mode and then continue.
  //
  if (fs == true && lastDisplay != -1)
    Cocoa_GL_SetFullScreen(0, 0, 0, false, blankOtherDisplays, fakeFullScreen);
  
  NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  
  if (!context)
    return;
  
  if (fs)
  {
    NSScreen* pScreen = [[NSScreen screens] objectAtIndex:screen];
    NSOpenGLContext* newContext = NULL;
  
    // Fade to black to hide resolution-switching flicker and garbage.
  	CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;
    if (CGAcquireDisplayFadeReservation (5, &fade_token) == kCGErrorSuccess )
      CGDisplayFade(fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);

    // Save these values.
    lastView = [context view];
    lastDisplay = screen;
    windowedView = lastView;
    
    if (fakeFullScreen == false)
    {
      // obtain fullscreen pixel format
      NSOpenGLPixelFormat* pixFmt = (NSOpenGLPixelFormat*)Cocoa_GL_GetFullScreenPixelFormat(screen);
      if (!pixFmt)
        return;
      
      // create our new context (sharing with the current one)
      newContext = (NSOpenGLContext*)Cocoa_GL_CreateContext((void*) pixFmt, (void*)context);
      
      // release pixelformat
      [pixFmt release];
      pixFmt = nil;
      
      if (!newContext)
        return;
      
      // Make sure the view is on the screen that we're activating (to hide it).
      NSScreen* pScreen = [[NSScreen screens] objectAtIndex:screen];
      [[lastView window] setFrameOrigin:[pScreen frame].origin];
      
      // clear the current context
      [NSOpenGLContext clearCurrentContext];
              
      // set fullscreen
      [newContext setFullScreen];
      
      // Capture the display before going fullscreen.
      fullScreenDisplay = Cocoa_GetDisplay(screen);
      if (blankOtherDisplays == true)
        CGCaptureAllDisplays();
      else
        CGDisplayCapture(fullScreenDisplay);

      // If we don't hide menu bar, it will get events and interrupt the program.
      if (fullScreenDisplay == kCGDirectMainDisplay)
        HideMenuBar();
    }
    else
    {
      // Get the screen rect of our main display
      NSRect    screenRect = [pScreen frame];
      NSWindow* mainWindow = [[NSWindow alloc] initWithContentRect:screenRect
                                              styleMask:NSBorderlessWindowMask
                                              backing:NSBackingStoreBuffered
                                              defer:NO 
                                              screen:pScreen];
                                              
      [mainWindow setBackgroundColor:[NSColor blackColor]];
      [mainWindow makeKeyAndOrderFront:nil];
      
      // Display our window fairly high...
      [mainWindow setLevel:NSFloatingWindowLevel];
      
      // ...and the original one beneath it and on the same screen.
      [[lastView window] setFrameOrigin:[pScreen frame].origin];
      [[lastView window] setLevel:NSNormalWindowLevel];
          
      NSView* blankView = [[NSView alloc] init];
      [mainWindow setContentView:blankView];
      [mainWindow setContentSize:NSMakeSize(width, height)];
      [mainWindow update];
      [blankView setFrameSize:NSMakeSize(width, height)];
      
      // Obtain windowed pixel format and create a new context.
      NSOpenGLPixelFormat* pixFmt = (NSOpenGLPixelFormat*)Cocoa_GL_GetWindowPixelFormat();
      newContext = (NSOpenGLContext*)Cocoa_GL_CreateContext((void* )pixFmt, (void* )context);
      [pixFmt release];
      
      [newContext setView:blankView];
      
      // Hide the menu bar.
      fullScreenDisplay = Cocoa_GetDisplay(screen);
      if (fullScreenDisplay == kCGDirectMainDisplay)
        HideMenuBar();
          
      // Save the window.
      lastWindow = mainWindow;
      
      // Blank other displays if requested.
      if (blankOtherDisplays)
        Cocoa_GL_BlankOtherDisplays(screen);
    }

    // Hide the mouse.
    [NSCursor hide];
    
    // Release old context if we created it.
    if (lastOwnedContext == context)
      Cocoa_GL_ReleaseContext((void*)context);

    // activate context
    [newContext makeCurrentContext];
    lastOwnedContext = newContext;
    
    if (fade_token != kCGDisplayFadeReservationInvalidToken) 
    {
      CGDisplayFade(fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
      CGReleaseDisplayFadeReservation(fade_token);
    }
  }
  else
  {
  	// Fade to black to hide resolution-switching flicker and garbage.
  	CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;
    if (CGAcquireDisplayFadeReservation (5, &fade_token) == kCGErrorSuccess )
      CGDisplayFade(fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);
    
    // exit fullscreen
    [context clearDrawable];
    
    [NSCursor unhide];
    
    if (fakeFullScreen == false)
    {
      if (fullScreenDisplay == kCGDirectMainDisplay)
        ShowMenuBar();
      
      // release displays
      CGReleaseAllDisplays();
    }
    else
    {
      // Show menubar.
      if (fullScreenDisplay == kCGDirectMainDisplay)
        ShowMenuBar();

      // Get rid of the new window we created.
      [lastWindow close];
      [lastWindow release];
      
      // Unblank.
      if (blankOtherDisplays)
        Cocoa_GL_UnblankOtherDisplays(screen);
    }
    
    // obtain windowed pixel format
    NSOpenGLPixelFormat* pixFmt = (NSOpenGLPixelFormat*)Cocoa_GL_GetWindowPixelFormat();
    if (!pixFmt)
      return;
    
    // create our new context (sharing with the current one)
    NSOpenGLContext* newContext = (NSOpenGLContext*)Cocoa_GL_CreateContext((void* )pixFmt, (void* )context);
    
    // release pixelformat
    [pixFmt release];
    pixFmt = nil;
    
    if (!newContext)
      return;
    
    // Assign view from old context, move back to original screen.
    NSScreen* screen = [[NSScreen screens] objectAtIndex:lastDisplay];
    [newContext setView:lastView];
    [[lastView window] setFrameOrigin:[screen frame].origin];
    
    // Release the fullscreen context.
    if (lastOwnedContext == context)
      Cocoa_GL_ReleaseContext((void*)context);
    
    // Activate context.
    [newContext makeCurrentContext];
    lastOwnedContext = newContext;
    
    if (fade_token != kCGDisplayFadeReservationInvalidToken) 
    {
      CGDisplayFade(fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
      CGReleaseDisplayFadeReservation(fade_token);
    }
    
    // Reset.
    lastView = NULL;
    lastDisplay = -1;
    fullScreenDisplay = 0;
    windowedView = 0;
  }
}

int Cocoa_GetCurrentDisplay()
{
  NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  NSView* view = [context view];
  NSScreen* screen = [[view window] screen];
  
  int numDisplays = Cocoa_GetNumDisplays();
  int i = 0;
  
  for (i=0; i<numDisplays; i++)
  {
    if ([[NSScreen screens] objectAtIndex:i] == screen)
      return i;
  }
  
  return -1;
}

void Cocoa_GL_EnableVSync(bool enable)
{
#if 1
  NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  
  // Flush synchronised with vertical retrace                       
  GLint theOpenGLCPSwapInterval = enable ? 1 : 0;
  [context setValues:&theOpenGLCPSwapInterval forParameter:NSOpenGLCPSwapInterval];
  
#else

  CGLContextObj cglContext;
  cglContext = CGLGetCurrentContext();
  if (cglContext)
  {
    GLint interval;
    if (enable)
      interval = 1;
    else
      interval = 0;
    
    int cglErr = CGLSetParameter(cglContext, kCGLCPSwapInterval, &interval);
    if (cglErr != kCGLNoError)
      printf("ERROR: CGLSetParameter for kCGLCPSwapInterval failed with error %d: %s", cglErr, CGLErrorString(cglErr));
  }
#endif
}

void* Cocoa_GL_GetWindowPixelFormat()
{
  NSOpenGLPixelFormatAttribute wattrs[] =
  {
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAWindow,
    NSOpenGLPFANoRecovery,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFADepthSize, 8,
    //NSOpenGLPFAColorSize, 32,
    //NSOpenGLPFAAlphaSize, 8,
    0
  };
  NSOpenGLPixelFormat* pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:wattrs];
  return (void*)pixFmt;
}

void* Cocoa_GL_GetFullScreenPixelFormat(int screen)
{
  NSOpenGLPixelFormatAttribute fsattrs[] =
  {
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAFullScreen,
    NSOpenGLPFANoRecovery,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFADepthSize, 8,
    NSOpenGLPFAScreenMask,
    CGDisplayIDToOpenGLDisplayMask((CGDirectDisplayID)Cocoa_GetDisplay(screen)),
    0
  };
  NSOpenGLPixelFormat* pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:fsattrs];
  return (void*)pixFmt;
}

void* Cocoa_GL_GetCurrentContext()
{
  NSOpenGLContext* context = [NSOpenGLContext currentContext];
  return (void*)context;
}

void* Cocoa_GL_CreateContext(void* pixFmt, void* shareCtx)
{
  if (!pixFmt)
    return nil;
  NSOpenGLContext* newContext = [[NSOpenGLContext alloc] initWithFormat:pixFmt
                                                           shareContext:(NSOpenGLContext*)shareCtx];

  // Enable GL multithreading if available.                                                           
  //CGLContextObj theCGLContextObj = (CGLContextObj) [newContext CGLContextObj];
  //CGLEnable(theCGLContextObj, kCGLCEMPEngine);

  // Flush synchronised with vertical retrace                       
  GLint theOpenGLCPSwapInterval = 1;
  [newContext setValues:(const GLint*)&theOpenGLCPSwapInterval forParameter:(NSOpenGLContextParameter) NSOpenGLCPSwapInterval];
                                                           
  return newContext;
}

void* Cocoa_GL_ReplaceSDLWindowContext()
{
  NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  NSView* view = [context view];
  
  if (!view)
    return 0;
  
  // disassociate view from context
  [context clearDrawable];
  
  // release the context
  if (lastOwnedContext == context)
    Cocoa_GL_ReleaseContext((void*)context);
  
  // obtain window pixelformat
  NSOpenGLPixelFormat* pixFmt = (NSOpenGLPixelFormat*)Cocoa_GL_GetWindowPixelFormat();
  if (!pixFmt)
    return 0;
  
  NSOpenGLContext* newContext = (NSOpenGLContext*)Cocoa_GL_CreateContext((void*)pixFmt, nil);
  [pixFmt release];
  
  if (!newContext)
    return 0;
  
  // associate with current view
  [newContext setView:view];
  [newContext makeCurrentContext];
  lastOwnedContext = newContext;
  
  return newContext;
}

int Cocoa_DimDisplayNow()
{
  io_registry_entry_t r = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/IOResources/IODisplayWrangler");
  if(!r) return 1;
  int err = IORegistryEntrySetCFProperty(r, CFSTR("IORequestIdle"), kCFBooleanTrue);
  IOObjectRelease(r);
  return err;
}

void Cocoa_UpdateSystemActivity()
{
  UpdateSystemActivity(UsrActivity);   
}

void Cocoa_TurnOffScreenSaver()
{
  if (GetProcessPid("ScreenSaverEngin") != -1)
  {
    NSAppleScript* stopScript = [[NSAppleScript alloc] initWithSource:@"tell application \"/System/Library/Frameworks/ScreenSaver.framework/Versions/A/Resources/ScreenSaverEngine.app\" to quit"];
    [stopScript executeAndReturnError:nil];
  }
}
                   
int Cocoa_SleepSystem()
{
  io_connect_t root_domain;
  mach_port_t root_port;

  if (KERN_SUCCESS != IOMasterPort(MACH_PORT_NULL, &root_port))
    return 1;

  if (0 == (root_domain = IOPMFindPowerManagement(root_port)))    
    return 2;

  if (kIOReturnSuccess != IOPMSleepSystem(root_domain))
    return 3;

  return 0;
}        

void Cocoa_HideMouse()
{
  [NSCursor hide];
}

void Cocoa_GetSmartFolderResults(const char* strFile, void (*CallbackFunc)(void* userData, void* userData2, const char* path), void* userData, void* userData2)
{
  NSString*     filePath = [[NSString alloc] initWithUTF8String:strFile];
  NSDictionary* doc = [[NSDictionary alloc] initWithContentsOfFile:filePath];
  NSString*     raw = [doc objectForKey:@"RawQuery"];
  NSArray*      searchPaths = [[doc objectForKey:@"SearchCriteria"] objectForKey:@"FXScopeArrayOfPaths"];

  if (raw == 0)
    return;

  // Ugh, Carbon from now on...
  MDQueryRef query = MDQueryCreate(kCFAllocatorDefault, (CFStringRef)raw, NULL, NULL);
  if (query)
  {
  	if (searchPaths)
  	  MDQuerySetSearchScope(query, (CFArrayRef)searchPaths, 0);
  	  
    MDQueryExecute(query, 0);

	// Keep track of when we started.
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent(); 
    for (;;)
    {
      CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, YES);
    
      // If we're done or we timed out.
      if (MDQueryIsGatheringComplete(query) == true ||
      	  CFAbsoluteTimeGetCurrent() - startTime >= 5)
      {
        // Stop the query.
        MDQueryStop(query);
      
    	CFIndex count = MDQueryGetResultCount(query);
    	char title[BUFSIZ];
    	int i;
  
    	for (i = 0; i < count; ++i) 
   		{
      	  MDItemRef resultItem = (MDItemRef)MDQueryGetResultAtIndex(query, i);
      	  CFStringRef titleRef = (CFStringRef)MDItemCopyAttribute(resultItem, kMDItemPath);
      
      	  CFStringGetCString(titleRef, title, BUFSIZ, kCFStringEncodingUTF8);
      	  CallbackFunc(userData, userData2, title);
      	  CFRelease(titleRef);
    	}  
    
        CFRelease(query);
    	break;
      }
    }
  }
  
  // Freeing these causes a crash when scanning for new content.
  CFRelease(filePath);
  CFRelease(doc);
}

const char* Cocoa_GetAppVersion()
{
  return [(NSString*)[[NSBundle mainBundle] objectForInfoDictionaryKey:(id)kCFBundleVersionKey] UTF8String];
}

void* Cocoa_GetDisplayPort()
{
  //NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  //NSView* view = [context view];
  //WindowRef refWindow = [[view window] windowRef];
  //return GetWindowPort(refWindow);
  
  WindowRef refWindow = [childWindow windowRef];
  return (void* )GetWindowPort(refWindow);
}


/* Get/set LCD panel brightness */

void Cocoa_GetPanelBrightness(float* brightness)
{
  float panelBrightness = HUGE_VALF;
  CGDisplayErr dErr;
  
  dErr = IODisplayGetFloatParameter(CGDisplayIOServicePort(CGMainDisplayID()), kNilOptions, CFSTR(kIODisplayBrightnessKey), &panelBrightness);
  if (dErr == kIOReturnSuccess)
    *brightness = panelBrightness;
  else
    *brightness = -1.0f;
}

void Cocoa_SetPanelBrightness(float brightness)
{
  if ((brightness >=0.0f) && (brightness <= 1.0f)) {
    IODisplaySetFloatParameter(CGDisplayIOServicePort(CGMainDisplayID()), kNilOptions, CFSTR(kIODisplayBrightnessKey), brightness);    
  }
}
