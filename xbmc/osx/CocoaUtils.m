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

#import <libSmartCrashReports/SmartCrashReportsInstall.h>
#import <libSmartCrashReports/SmartCrashReportsAPI.h>

void Cocoa_Initialize(void* pApplication)
{
  UnsanitySCR_RegisterMatchSpecifier(CFSTR("org.xbmc.XBMC"), CFSTR("OSXBMC"), NULL, CFSTR("SCR-76B35BCAB9"), NULL);  
  
  Boolean authenticationWillBeRequired = FALSE;
  if (UnsanitySCR_CanInstall(&authenticationWillBeRequired))
    UnsanitySCR_Install(authenticationWillBeRequired ? kUnsanitySCR_GlobalInstall : 0);
  
  // Intialize the Apple remote code.
  [[XBMCMain sharedInstance] setApplication: pApplication];
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

void Cocoa_GL_SwapBuffers(void* theContext)
{
  [ (NSOpenGLContext*)theContext flushBuffer ];
}

#define MAX_DISPLAYS 32

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

void Cocoa_GL_ResizeWindow(void *theContext, int w, int h)
{
  if (!theContext)
    return;
  
  NSOpenGLContext* context = Cocoa_GL_GetCurrentContext();
  NSView* view;
  NSWindow* window;
  
  view = [context view];
  if (view && w>0 && h>0)
  {
    window = [view window];
    if (window)
    {
      [window setContentSize:NSMakeSize(w,h)];
      [window update];
      [view setFrameSize:NSMakeSize(w, h)];
      [context update];
      [window center];
    }
  }
}

static NSOpenGLContext* lastOwnedContext = 0;

void Cocoa_GL_SetFullScreen(int screen, int width, int height, bool fs, bool blankOtherDisplays)
{
  static NSView* lastView = NULL;
  static int fullScreenDisplay = 0;
  static NSScreen* lastScreen = NULL;

  // If we're already fullscreen then we must be moving to a different display.
  // Recurse to reset fullscreen mode and then continue.
  //
  if (fs == true && lastScreen != NULL)
	Cocoa_GL_SetFullScreen(0, 0, 0, false, blankOtherDisplays);
  
  NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  
  if (!context)
    return;
  
  if (fs)
  {
    // Fade to black to hide resolution-switching flicker and garbage.
  	CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;
    if (CGAcquireDisplayFadeReservation (5, &fade_token) == kCGErrorSuccess )
      CGDisplayFade(fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);
    
    // obtain fullscreen pixel format
    NSOpenGLPixelFormat* pixFmt = (NSOpenGLPixelFormat*)Cocoa_GL_GetFullScreenPixelFormat(screen);
    if (!pixFmt)
      return;
    
    // create our new context (sharing with the current one)
    NSOpenGLContext* newContext = (NSOpenGLContext*)Cocoa_GL_CreateContext((void*) pixFmt, (void*)context);
    
    // release pixelformat
    [pixFmt release];
    pixFmt = nil;
    
    if (!newContext)
      return;
    
    // Save and make sure the view is on the screen that we're activating (to hide it).
    lastView = [context view];
    lastScreen = [[lastView window] screen];
    NSScreen* pScreen = [[NSScreen screens] objectAtIndex:screen];
    [[lastView window] setFrameOrigin:[pScreen frame].origin];
    
    // clear the current context
    [NSOpenGLContext clearCurrentContext];
    
    // Capture the display before going fullscreen.
    fullScreenDisplay = Cocoa_GetDisplay(screen);
    if (blankOtherDisplays == true)
      CGCaptureAllDisplays();
    else
      CGDisplayCapture(fullScreenDisplay);
    
    // Hide the mouse.
    [NSCursor hide];
    
    // If we don't hide menu bar, it will get events and interrupt the program.
    if (fullScreenDisplay == kCGDirectMainDisplay)
      HideMenuBar();
    
    // set fullscreen
    [newContext setFullScreen];
    
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
    
    if (fullScreenDisplay == kCGDirectMainDisplay)
      ShowMenuBar();
    
    // release displays
    CGReleaseAllDisplays();
    
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
    
    // assign view from old context, move back to original screen.
    [newContext setView:lastView];
    [[lastView window] setFrameOrigin:[lastScreen frame].origin];
    
    // release the fullscreen context
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
    
    // Reset.
    lastView = NULL;
    lastScreen = NULL;
    fullScreenDisplay = 0;
  }
}

void Cocoa_GL_EnableVSync(bool enable)
{
#if 1
  NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  
  // Flush synchronised with vertical retrace                       
  GLint theOpenGLCPSwapInterval = enable ? 1 : 0;
  [context setValues:(const GLint*)&theOpenGLCPSwapInterval forParameter:(NSOpenGLContextParameter) NSOpenGLCPSwapInterval];
  
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

void Cocoa_GL_ReplaceSDLWindowContext()
{
  NSOpenGLContext* context = (NSOpenGLContext*)Cocoa_GL_GetCurrentContext();
  NSView* view = [context view];
  
  if (!view)
    return;
  
  // disassociate view from context
  [context clearDrawable];
  
  // release the context
  if (lastOwnedContext == context)
    Cocoa_GL_ReleaseContext((void*)context);
  
  // obtain window pixelformat
  NSOpenGLPixelFormat* pixFmt = (NSOpenGLPixelFormat*)Cocoa_GL_GetWindowPixelFormat();
  if (!pixFmt)
    return;
  
  NSOpenGLContext* newContext = (NSOpenGLContext*)Cocoa_GL_CreateContext((void*)pixFmt, nil);
  [pixFmt release];
  
  if (!newContext)
    return;
  
  // associate with current view
  [newContext setView:view];
  [newContext makeCurrentContext];
  lastOwnedContext = newContext;
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