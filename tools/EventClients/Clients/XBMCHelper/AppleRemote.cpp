/*
 * Copyright (c) 2006-2008 Amit Singh. All Rights Reserved.
 * Copyright (c) 2008 Elan Feingold. All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *     
 *  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 *
 */
#define BINARY_NAME   "Plex"
#define APPNAME       "Plex.app"
#define PROGNAME      "PlexHelper"
#define PROGVERS      "1.1.1"

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/errno.h>
#include <sysexits.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <mach-o/dyld.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>

#include "XBox360.h"
#include "Reactor.h"
#include "../../lib/c++/xbmcclient.h"

#include <iostream>
#include <map>
#include <string>
#include <iterator>
#include <sstream>

using namespace std;

static struct option long_options[] = 
{
    { "help",       no_argument,         0, 'h' },
    { "server",     required_argument,   0, 's' },
    { "universal",  no_argument,         0, 'u' },
    { "timeout",    required_argument,   0, 't' },
    { "verbose",    no_argument,         0, 'v' },
    { "externalConfig", no_argument,     0, 'x' },
    { "appLocation", required_argument,  0, 'a' },
    { "secureInput", required_argument,  0, 'i' },
    { 0, 0, 0, 0 },
};
static const char *options = "hsutvxai";
 
const int REMOTE_SWITCH_COOKIE = 19;
const int IGNORE_CODE = 39;

static int GetBSDProcessList(kinfo_proc **procList, size_t *procCount);
bool isProgramRunning(const char* strProgram, int ignorePid=0);
void startXBMC();
long getTimeInMilliseconds();
void readConfig();
void parseOptions(int argc, char** argv);
void handleSignal(int signal);

#define APPLE_REMOTE "JS0:Apple Remote"

#define kRemoteButtonUp            1
#define kRemoteButtonDown          2
#define kRemoteButtonMenu          6
#define kRemoteButtonPlay          5
#define kRemoteButtonRight         4
#define kRemoteButtonLeft          3
#define kRemoteButtonRight_Hold   10
#define kRemoteButtonLeft_Hold     9
#define kRemoteButtonMenu_Hold    11
#define kRemoteButtonPlay_Hold    12
#define kRemoteControl_Switched   19

#define kDownMask             0x1000
#define kButtonMask           0x00FF

#define kButtonRepeats        0x01
#define kButtonSendsUpEvents  0x02

class ButtonConfig
{
 public:
  ButtonConfig()
    : sendsUpEvent(false)
    , repeats(false) {}
 
  ButtonConfig(int flags)
    : sendsUpEvent(flags & kButtonSendsUpEvents)
    , repeats(flags & kButtonRepeats) {}
 
  bool sendsUpEvent;
  bool repeats;
};

map<string, int> keyMap;
map<string, string> keyMapUniversal;
map<string, bool> keyMapUniversalRepeat;
map<int, ButtonConfig> buttonConfigMap;

bool isUniversalMode = false;
int sequenceTimeout = 500;
bool verbose = false;
string serverAddress = "127.0.0.1";
string appLocation = "/Applications";
bool secureInput = false;
CAddress* pServer = 0;
int sockfd;

class ReactorAppleRemote : public Reactor
{
 public:
 
  ReactorAppleRemote()
    : _needToRelease(false)
  {
  }
 
  virtual void onTimeout()
  {
    if (verbose)
      printf(" -> Timeout!\n");
    
    // Reset the Universal prefix we were working on.
    _itsRemotePrefix = "";
  }
  
  virtual void onCmd(int data)
  { 
    ButtonConfig   config = buttonConfigMap[data & kButtonMask];
    char           strButton[16];
    int            button = data & kButtonMask;

    sprintf(strButton, "%d", button);
  
    if (button == kRemoteButtonMenu)
		{
			// Make sure XBMC is running.
			if (isProgramRunning(BINARY_NAME) == false && serverAddress == "127.0.0.1")
			{
				startXBMC();
        return;
			}
		}
    
    if (isUniversalMode)
    {      
      if (data & kDownMask)
      {
        _itsRemotePrefix += strButton;
        _itsRemotePrefix += "_";
      
        if (verbose && _lastTime != 0)
          printf(" -> Key received after %ld ms.\n", getTimeInMilliseconds()-_lastTime);
      
        string val = keyMapUniversal[_itsRemotePrefix];
        if (val.length() != 0)
        {
          bool isSimpleButton = false;
          if (keyMapUniversalRepeat.count(_itsRemotePrefix) > 0)
          {
            isSimpleButton = true;
            _needToRelease = true;
          }
            
          // Turn off repeat for complex buttons.
          int flags = BTN_DOWN;
          if (isSimpleButton == false)
            flags |= (BTN_NO_REPEAT | BTN_QUEUE);
                    
          // Send the command to XBMC.
          if (verbose)
            printf(" -> Sending command: %s\n", val.c_str());
            
          CPacketBUTTON btn(val.c_str(), "R1", flags);
          btn.Send(sockfd, *pServer);
          _itsRemotePrefix = "";
        
          // Reset the timer.
          resetTimer();
          _lastTime = 0;
        }
        else
        {  
          // We got a prefix. Set a timeout.
          setTimer(sequenceTimeout);
          _lastTime = getTimeInMilliseconds();
        }
      }
      else
      {
        if (_needToRelease)
        {
          _needToRelease = true;
          if (verbose)
            printf(" -> Button release.\n");
          
          // Send the button release.  
          CPacketBUTTON btn;
          btn.Send(sockfd, *pServer);
        }
      }
    }
    else if (isUniversalMode == false)
    {
      // Send the down part.
      if ((config.sendsUpEvent && (data & kDownMask)) || config.sendsUpEvent == false)
      {
        if (verbose)
          printf(" -> Sending button [%s] down.\n", strButton);
        
        int flags = BTN_DOWN;
        if (config.sendsUpEvent == false)
          flags |= (BTN_NO_REPEAT | BTN_QUEUE);
        
        CPacketBUTTON btn(atoi(strButton), APPLE_REMOTE, flags, config.repeats);
        btn.Send(sockfd, *pServer);
      }
    
      // Send the up part.
      if (config.sendsUpEvent && (data & kDownMask) == 0)
      {
        if (verbose)
          printf(" -> Sending button up.\n");
        
        CPacketBUTTON btn; 
        btn.Send(sockfd, *pServer);
      }
    }
  }
  
  virtual void onServerCmd(char* data, char* arg)
  {
    
  }
    
 protected:
 
  string _itsRemotePrefix;
  bool _needToRelease;
  int  _lastTime;
};

ReactorAppleRemote theReactor;

void            usage();
inline          void print_errmsg_if_io_err(int expr, char *msg);
inline          void print_errmsg_if_err(int expr, char *msg);
void            QueueCallbackFunction(void *target, IOReturn result, void *refcon, void *sender);
bool            addQueueCallbacks(IOHIDQueueInterface **hqi);
void            processQueue(IOHIDDeviceInterface **hidDeviceInterface, CFMutableArrayRef cookies);
void            doRun(IOHIDDeviceInterface **hidDeviceInterface, CFMutableArrayRef cookies);
CFMutableArrayRef getHIDCookies(IOHIDDeviceInterface122 **handle);
void            createHIDDeviceInterface(io_object_t hidDevice, IOHIDDeviceInterface ***hdi);
void            setupAndRun();

void usage()
{
    printf("%s (version %s)\n", PROGNAME, PROGVERS);
    printf("   Copyright (c) 2008 Plex. All Rights Reserved.\n");
    printf("   Sends control events to Plex.\n\n");
    printf("Usage: %s [OPTIONS...]\n\nOptions:\n", PROGNAME);
    printf("  -h, --help               Print this help message and exit.\n");
    printf("  -s, --server <addr>      Send events to the specified IP.\n");
    printf("  -u, --universal          Runs in Universal Remote mode.\n");
    printf("  -t, --timeout <ms>       Timeout length for sequences (default: 500ms).\n");
    printf("  -v, --verbose            Prints lots of debugging information.\n");
    printf("  -a, --appLocation <path> The location of the application.\n");
}

inline void print_errmsg_if_io_err(int expr, char *msg)
{
    IOReturn err = (expr);

    if (err != kIOReturnSuccess) {
        fprintf(stderr, "*** %s - %s(%x, %d).\n", msg, mach_error_string(err),
                err, err & 0xffffff);
        fflush(stderr);
        exit(EX_OSERR);
    }
}

inline void print_errmsg_if_err(int expr, char *msg)
{
    if (expr) {
        fprintf(stderr, "*** %s.\n", msg);
        fflush(stderr);
        exit(EX_OSERR);
    }
}

void handleEventWithCookieString(string &cookieString, int sumOfValues)
{
	//printf("Event: %s (%d)\n", cookieString.c_str(), sumOfValues);
	
	if (cookieString.length() == 0)
		return;
		
	if (keyMap.count(cookieString) > 0 && cookieString != "19_")
	{
		int  buttonId = keyMap[cookieString];
		bool down = sumOfValues > 0;

    // Send the message to the button processor.
    theReactor.sendMessage(Reactor::CmdData, buttonId | (kDownMask * down));
  }
}

bool isProgramRunning(const char* strProgram, int ignorePid)
{
	kinfo_proc* mylist = (kinfo_proc *)malloc(sizeof(kinfo_proc));
	size_t mycount = 0;
	bool ret = false;
	
	GetBSDProcessList(&mylist, &mycount);
  for(size_t k = 0; k < mycount && ret == false; k++) 
	{
		kinfo_proc *proc = NULL;
	  proc = &mylist[k];

		if (strcmp(proc->kp_proc.p_comm, strProgram) == 0)
		{
		  if (ignorePid == 0 || ignorePid != proc->kp_proc.p_pid)
			  ret = true;
		}
	}
	free(mylist);
	
	return ret;
}

void startXBMC()
{
	printf("Trying to start Plex.\n");
	
  string app = appLocation + "/" APPNAME;
  string strCmd = "open ";
	strCmd += app;
	strCmd += "&";

	// Start it in the background.
	printf("Got path: [%s]\n", app.c_str());
	system(strCmd.c_str());
}

void QueueCallbackFunction(void *target, IOReturn result, void *refcon, void *sender)
{
    HRESULT               ret = 0;
    AbsoluteTime          zeroTime = {0,0};
    IOHIDQueueInterface **hqi;
		IOHIDEventStruct      event;
		SInt32           		  sumOfValues = 0;
		string           cookieString;

    while (!ret) 
		{
        hqi = (IOHIDQueueInterface **)sender;
        ret = (*hqi)->getNextEvent(hqi, &event, zeroTime, 0);

				if (ret != kIOReturnSuccess)
            continue;

				if ((int)event.elementCookie == REMOTE_SWITCH_COOKIE ||
					  (int)event.elementCookie == IGNORE_CODE)
				{
					// Ignore.
	      } 
				else 
				{
	      	if (((int)event.elementCookie) != 5) 
					{
	         	sumOfValues += event.value;
	
						char str[32];
						sprintf(str, "%d_", (int)
						event.elementCookie);
						cookieString += str;
	        }
				}
		}
		
		handleEventWithCookieString(cookieString, sumOfValues);
}

bool addQueueCallbacks(IOHIDQueueInterface **hqi)
{
    IOReturn               ret;
    CFRunLoopSourceRef     eventSource;
    IOHIDQueueInterface ***privateData;

    privateData = (IOHIDQueueInterface*** )malloc(sizeof(*privateData));
    *privateData = hqi;

    ret = (*hqi)->createAsyncEventSource(hqi, &eventSource);
    if (ret != kIOReturnSuccess)
        return false;

    ret = (*hqi)->setEventCallout(hqi, QueueCallbackFunction, NULL, &privateData);
    if (ret != kIOReturnSuccess)
        return false;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopDefaultMode);
    return true;
}

void processQueue(IOHIDDeviceInterface **hidDeviceInterface, CFMutableArrayRef cookies)
{
	HRESULT               result;
	IOHIDQueueInterface **queue;

	queue = (*hidDeviceInterface)->allocQueue(hidDeviceInterface);
	if (!queue) 
	{
	    fprintf(stderr, "Failed to allocate event queue.\n");
	    return;
	}

	(void)(*queue)->create(queue, 0, 12);
	
	// Add all the cookies to the queue.
	int i;
	printf("Adding %d Apple Remote cookies.\n", (int)CFArrayGetCount(cookies));
	for (i=0; i<CFArrayGetCount(cookies); i++)
    	(void)(*queue)->addElement(queue, (void* )CFArrayGetValueAtIndex(cookies, i), 0);

	addQueueCallbacks(queue);

	result = (*queue)->start(queue);

	CFRunLoopRun();

	result = (*queue)->stop(queue);
	result = (*queue)->dispose(queue);

	(*queue)->Release(queue);
}

//
// The Cocoa run loop.
//
void doRun(IOHIDDeviceInterface **hidDeviceInterface, CFMutableArrayRef cookies)
{
  IOReturn ioReturnValue;

  ioReturnValue = (*hidDeviceInterface)->open(hidDeviceInterface, kIOHIDOptionsTypeSeizeDevice);
  if (ioReturnValue == KERN_SUCCESS)
  {
    processQueue(hidDeviceInterface, cookies);
    
    if (secureInput == true)
    {
      DisableSecureEventInput();
      printf("Disabling secure input.\n");
    }

    ioReturnValue = (*hidDeviceInterface)->close(hidDeviceInterface);
  }
      
  (*hidDeviceInterface)->Release(hidDeviceInterface);
}

CFMutableArrayRef getHIDCookies(IOHIDDeviceInterface122 **handle)
{
  IOHIDElementCookie cookie;
  CFTypeRef          object;
  long               number;
  long               usage;
  long               usagePage;
  CFArrayRef         elements;
  CFDictionaryRef    element;
  IOReturn           result;

	// Create the list of cookies.
	CFMutableArrayRef cookieList = CFArrayCreateMutable(NULL, 0, 0);

  if (!handle || !(*handle))
      return NULL;

  result = (*handle)->copyMatchingElements(handle, NULL, &elements);

  if (result != kIOReturnSuccess) 
  {
      fprintf(stderr, "Failed to copy cookies.\n");
      exit(1);
  }

  CFIndex i;
  for (i = 0; i < CFArrayGetCount(elements); i++) 
  {
      element = (CFDictionaryRef)CFArrayGetValueAtIndex(elements, i);
      object = (CFDictionaryGetValue(element, CFSTR(kIOHIDElementCookieKey)));
      if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID()) continue;
      if(!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number)) continue;

      cookie = (IOHIDElementCookie)number;
      object = CFDictionaryGetValue(element, CFSTR(kIOHIDElementUsageKey));
      if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID()) continue;
      if (!CFNumberGetValue((CFNumberRef)object, kCFNumberLongType, &number)) continue;

      usage = number;
      object = CFDictionaryGetValue(element,CFSTR(kIOHIDElementUsagePageKey));
      if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID()) continue;
      if (!CFNumberGetValue((CFNumberRef)object, kCFNumberLongType, &number)) continue;
      usagePage = number;

		CFArrayAppendValue(cookieList, cookie);
  }

  return cookieList;
}

void createHIDDeviceInterface(io_object_t hidDevice, IOHIDDeviceInterface ***hdi)
{
    io_name_t             className;
    IOCFPlugInInterface **plugInInterface = NULL;
    HRESULT               plugInResult = S_OK;
    SInt32                score = 0;
    IOReturn              ioReturnValue = kIOReturnSuccess;

    // Enable secure input. This ensures we don't lose exclusivity.
    if (secureInput)
    {
      printf("Enabling secure input.\n");
      EnableSecureEventInput();
    }

    ioReturnValue = IOObjectGetClass(hidDevice, className);
    print_errmsg_if_io_err(ioReturnValue, (char* )"Failed to get class name.");

    ioReturnValue = IOCreatePlugInInterfaceForService(
                        hidDevice,
                        kIOHIDDeviceUserClientTypeID,
                        kIOCFPlugInInterfaceID,
                        &plugInInterface,
                        &score);

    if (ioReturnValue != kIOReturnSuccess)
        return;

    plugInResult = (*plugInInterface)->QueryInterface(
                        plugInInterface,
                        CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),
                        (LPVOID*)hdi);
    print_errmsg_if_err(plugInResult != S_OK, (char* )"Failed to create device interface.\n");

    (*plugInInterface)->Release(plugInInterface);
}

IOHIDDeviceInterface **gHidDeviceInterface = 0;

void setupAndRun()
{
    // Get the Apple Remote setup.
    CFMutableDictionaryRef hidMatchDictionary = NULL;
    io_service_t           hidService = (io_service_t)0;
    io_object_t            hidDevice = (io_object_t)0;
    IOHIDDeviceInterface **hidDeviceInterface = NULL;
    IOReturn               ioReturnValue = kIOReturnSuccess;
    CFMutableArrayRef      cookies;
    
    hidMatchDictionary = IOServiceNameMatching("AppleIRController");
    hidService = IOServiceGetMatchingService(kIOMasterPortDefault, hidMatchDictionary);

    if (hidService) 
    {
      hidDevice = (io_object_t)hidService;

      createHIDDeviceInterface(hidDevice, &hidDeviceInterface);
      gHidDeviceInterface = hidDeviceInterface;
      cookies = getHIDCookies((IOHIDDeviceInterface122 **)hidDeviceInterface);
      ioReturnValue = IOObjectRelease(hidDevice);
      print_errmsg_if_io_err(ioReturnValue, (char* )"Failed to release HID.");

      if (hidDeviceInterface == NULL) 
      {
          fprintf(stderr, "No HID.\n");
          return;
      }

      // Start the reactor.
      theReactor.start();

      // Enter the run loop.
      doRun(hidDeviceInterface, cookies);

      if (ioReturnValue == KERN_SUCCESS)
          ioReturnValue = (*hidDeviceInterface)->close(hidDeviceInterface);

      (*hidDeviceInterface)->Release(hidDeviceInterface);
    }
    else
    {
      printf("WARNING: No Apple Remote hardware found.\n");
    }
}

pascal OSStatus switchEventsHandler(EventHandlerCallRef nextHandler,
                                    EventRef switchEvent,
                                    void* userData)
{
  if (verbose)
    printf("New app is frontmost, reopening Apple Remote\n");
    
  if ((*gHidDeviceInterface)->close(gHidDeviceInterface) != KERN_SUCCESS)
    printf("ERROR closing Apple Remote device\n");
    
  if ((*gHidDeviceInterface)->open(gHidDeviceInterface, kIOHIDOptionsTypeSeizeDevice) != KERN_SUCCESS)
    printf("ERROR closing Apple Remote device\n");
  
  if (verbose)
    printf("Done reopening Apple Remote in exclusive mode\n");
  
  return 0;
}                

void* RunEventLoop(void* )
{
  sigset_t signalSet;
  sigemptyset (&signalSet);
  sigaddset (&signalSet, SIGHUP);
  pthread_sigmask(SIG_BLOCK, &signalSet, NULL);
  
  RunApplicationEventLoop();
  printf("Exiting Carbon event loop.\n");
  return 0;
}                    

int main(int argc, char **argv)
{
  // Handle SIGHUP.  
  signal(SIGHUP, handleSignal);
  signal(SIGUSR1, handleSignal);
  
  // We should close all file descriptions.
  for (int i=3; i<256; i++)
    close(i);
  
  // If we're already running, exit right away.
  if (isProgramRunning(PROGNAME, getpid()))
  {
    printf(PROGNAME " is already running.\n");
    exit(-1);
  }

  // Register for events.
  const EventTypeSpec applicationEvents[]  = { { kEventClassApplication, kEventAppFrontSwitched } };
  InstallApplicationEventHandler(NewEventHandlerUPP(switchEventsHandler), GetEventTypeCount(applicationEvents), applicationEvents, 0, NULL);

	// Add the keymappings (works for Leopard only).
	keyMap["31_29_28_18_"] = kRemoteButtonUp;
	keyMap["31_30_28_18_"] = kRemoteButtonDown;
	keyMap["31_20_18_31_20_18_"] = kRemoteButtonMenu;
	keyMap["31_21_18_31_21_18_"] = kRemoteButtonPlay;
	keyMap["31_22_18_31_22_18_"] = kRemoteButtonRight;
	keyMap["31_23_18_31_23_18_"] = kRemoteButtonLeft;
	keyMap["31_18_4_2_"] = kRemoteButtonRight_Hold;
	keyMap["31_18_3_2_"] = kRemoteButtonLeft_Hold;
	keyMap["31_18_31_18_"] = kRemoteButtonMenu_Hold;
	keyMap["35_31_18_35_31_18_"] = kRemoteButtonPlay_Hold;
	keyMap["19_"] = kRemoteControl_Switched;
	
  keyMapUniversal["5_"] = "Select";
  keyMapUniversal["3_"] = "Left";
  keyMapUniversal["9_"] = "Left";
  keyMapUniversal["4_"] = "Right";
  keyMapUniversal["10_"] = "Right";
  keyMapUniversal["1_"] = "Up";
  keyMapUniversal["2_"] = "Down";
  keyMapUniversal["6_"] = "";
  
  // Left/right held, and up/down normal repeat.
  keyMapUniversalRepeat["1_"] = true;
  keyMapUniversalRepeat["2_"] = true;
  keyMapUniversalRepeat["9_"] = true;
  keyMapUniversalRepeat["10_"] = true;
  
  keyMapUniversal["6_3_"] = "Reverse";
  keyMapUniversal["6_4_"] = "Forward";
  keyMapUniversal["6_2_"] = "Back";
  keyMapUniversal["6_6_"] = "Menu";
  keyMapUniversal["6_1_"] = "";
  keyMapUniversal["6_5_"] = "";

  keyMapUniversal["6_5_3_"] = "Title";
  keyMapUniversal["6_5_4_"] = "Info";
  keyMapUniversal["6_5_1_"] = "PagePlus";
  keyMapUniversal["6_5_2_"] = "PageMinus";
  keyMapUniversal["6_5_5_"] = "Display";
	
  keyMapUniversal["6_1_5_"] = "Stop";
  keyMapUniversal["6_1_3_"] = "Power";
  keyMapUniversal["6_1_4_"] = "Zero";
  keyMapUniversal["6_1_1_"] = "Play";
  keyMapUniversal["6_1_2_"] = "Pause";
  
  // Keeps track of which keys send up events and repeat.
  buttonConfigMap[kRemoteButtonUp] = ButtonConfig(kButtonRepeats | kButtonSendsUpEvents);
  buttonConfigMap[kRemoteButtonDown] = ButtonConfig(kButtonRepeats | kButtonSendsUpEvents);
  buttonConfigMap[kRemoteButtonLeft] = ButtonConfig();
  buttonConfigMap[kRemoteButtonRight] = ButtonConfig();
  buttonConfigMap[kRemoteButtonLeft_Hold] = ButtonConfig(kButtonSendsUpEvents);
  buttonConfigMap[kRemoteButtonRight_Hold] = ButtonConfig(kButtonSendsUpEvents);
  buttonConfigMap[kRemoteButtonPlay] = ButtonConfig();
  buttonConfigMap[kRemoteButtonMenu_Hold] = ButtonConfig();
  buttonConfigMap[kRemoteButtonPlay_Hold] = ButtonConfig();
  buttonConfigMap[kRemoteButtonMenu] = ButtonConfig();

  // Parse command line options.
  parseOptions(argc, argv);

	// Socket.
	pServer = new CAddress(serverAddress.c_str());

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    printf("Error creating socket.\n");
    return -1;
  }
  
  // Bind the socket to a specific local port.
  int port = 30000;
  CAddress client(port);
  while (client.Bind(sockfd) == false)
    client.SetPort(++port);
  
  // Start the XBox 360 thread.
  XBox360 xbox360;
  xbox360.start();

  pthread_t thread;
  pthread_create(&thread, 0, RunEventLoop, 0);

  setupAndRun();

  // Wait for the XBox 360 thread to exit.
  xbox360.join();

  return 0;
}

void parseOptions(int argc, char** argv)
{
  optind = 0;

  bool readExternal = false;
  int c, option_index = 0;
  while ((c = getopt_long(argc, argv, options, long_options, &option_index)) != -1) 
	{
    switch (c) 
  	{
    case 'h':
      usage();
      exit(0);
      break;
  	case 's':
  		serverAddress = optarg;
  		break;
  	case 'u':
      isUniversalMode = true;
      break;
    case 't':
      sequenceTimeout = atoi(optarg);
      break;
    case 'v':
      verbose = true;
      break;
    case 'x':
      readExternal = true;
      break;
    case 'a':
      appLocation = optarg;
      break;
    case 'i':
    {
      bool newSecure = (atoi(optarg) == 1) ? true : false;
      if (secureInput == true && newSecure == false)
      {
        printf("Disabling secure event input.\n");
        DisableSecureEventInput(); 
      }
      else if (secureInput == false && newSecure == true)
      {
        printf("Enabling secure event input.\n");
        EnableSecureEventInput(); 
      }
      secureInput = newSecure;
      break;
    }
    default:
      usage();
      exit(1);
      break;
    }
  }
  
  if (readExternal == true)
    readConfig();
}

void readConfig()
{
  // Compute filename.
  string strFile = getenv("HOME");
  strFile += "/Library/Application Support/" BINARY_NAME "/" PROGNAME ".conf";

  // Open file.
  ifstream ifs(strFile.c_str());
  if (!ifs)
    return;

  // Read file.
  stringstream oss;
  oss << ifs.rdbuf();

  if (!ifs && !ifs.eof())
    return;

  // Tokenize.
  string strData(oss.str());
  istringstream is(strData);
  vector<string> args = vector<string>(istream_iterator<string>(is), istream_iterator<string>());
  
  // Convert to char**.
  int argc = args.size() + 1;
  char** argv = new char*[argc + 1];
  int i = 0;
  argv[i++] = (char* )PROGNAME;
  
  for (vector<string>::iterator it = args.begin(); it != args.end(); )
    argv[i++] = (char* )(*it++).c_str();
  argv[i] = 0;
      
  // Parse the arguments.
  parseOptions(argc, argv);
    
  delete[] argv;
}

void handleSignal(int signal)
{
  // Re-read config.
  if (signal == SIGHUP || signal == SIGUSR1)
  {
    // Reset configuration.
    isUniversalMode = false;
    sequenceTimeout = 500;
    verbose = false;
    
    // Read configuration.
    printf("SIGHUP: Reading configuration.\n");
    readConfig();
  }
}

static long long offsetMS = 0;

long getTimeInMilliseconds()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  
  long long ms = tv.tv_sec * 1000 + tv.tv_usec/1000;
  
  if (offsetMS == 0)
    offsetMS = ms;
    
  return (long)(ms - offsetMS);
}

typedef struct kinfo_proc kinfo_proc;

static int GetBSDProcessList(kinfo_proc **procList, size_t *procCount)
    // Returns a list of all BSD processes on the system.  This routine
    // allocates the list and puts it in *procList and a count of the
    // number of entries in *procCount.  You are responsible for freeing
    // this list (use "free" from System framework).
    // On success, the function returns 0.
    // On error, the function returns a BSD errno value.
{
    int                err;
    kinfo_proc *        result;
    bool                done;
    static const int    name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    // Declaring name as const requires us to cast it when passing it to
    // sysctl because the prototype doesn't include the const modifier.
    size_t              length;

    assert( procList != NULL);
    assert(procCount != NULL);

    *procCount = 0;

    // We start by calling sysctl with result == NULL and length == 0.
    // That will succeed, and set length to the appropriate length.
    // We then allocate a buffer of that size and call sysctl again
    // with that buffer.  If that succeeds, we're done.  If that fails
    // with ENOMEM, we have to throw away our buffer and loop.  Note
    // that the loop causes use to call sysctl with NULL again; this
    // is necessary because the ENOMEM failure case sets length to
    // the amount of data returned, not the amount of data that
    // could have been returned.

    result = NULL;
    done = false;
    do {
        assert(result == NULL);

        // Call sysctl with a NULL buffer.

        length = 0;
        err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
                      NULL, &length,
                      NULL, 0);
        if (err == -1) {
            err = errno;
        }

        // Allocate an appropriately sized buffer based on the results
        // from the previous call.

        if (err == 0) {
            result = (kinfo_proc* )malloc(length);
            if (result == NULL) {
                err = ENOMEM;
            }
        }

        // Call sysctl again with the new buffer.  If we get an ENOMEM
        // error, toss away our buffer and start again.

        if (err == 0) {
            err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
                          result, &length,
                          NULL, 0);
            if (err == -1) {
                err = errno;
            }
            if (err == 0) {
                done = true;
            } else if (err == ENOMEM) {
                assert(result != NULL);
                free(result);
                result = NULL;
                err = 0;
            }
        }
    } while (err == 0 && ! done);

    // Clean up and establish post conditions.

    if (err != 0 && result != NULL) {
        free(result);
        result = NULL;
    }
    *procList = result;
    if (err == 0) {
        *procCount = length / sizeof(kinfo_proc);
    }

    assert( (err == 0) == (*procList != NULL) );

    return err;
}
