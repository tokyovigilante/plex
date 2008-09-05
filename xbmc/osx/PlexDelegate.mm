//
//  PlexDelegate.m
//  XBMC
//
//  Created by Elan Feingold on 7/17/2008.
//  Copyright 2008 Blue Mandrill Design. All rights reserved.
//
#define BOOL CPP_BOOL
#include "stdafx.h"
#include "Application.h"
#include "FileItem.h"
#undef BOOL
#undef DEBUG
#import "PlexDelegate.h"

extern int           gArgc;
extern const char  **gArgv;

extern BOOL gFinderLaunch;
extern BOOL gCalledAppMainline;

extern CApplication g_application;

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

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
  const char *temparg;
  size_t arglen;
  char *arg;
  const char **newargv;

  if (gCalledAppMainline)
  {
    // Tell the application to open the file.
    CStdString strPath([filename UTF8String]);
    CFileItem item(strPath, false);
    g_application.getApplicationMessenger().PlayFile(item, false);
  
    return TRUE;
  }

  if (!gFinderLaunch)  /* MacOS is passing command line args. */
      return FALSE;

  temparg = [filename UTF8String];
  arglen = strlen(temparg) + 1;
  arg = (char *)malloc(arglen);
  if (arg == NULL)
      return FALSE;

  newargv = (const char **)realloc(gArgv, sizeof (char *) * (gArgc + 2));
  if (newargv == NULL)
  {
      free(arg);
      return FALSE;
  }
  gArgv = newargv;

  strncpy(arg, temparg, arglen);
  gArgv[gArgc++] = arg;
  gArgv[gArgc] = NULL;
  return TRUE;
}
@end
