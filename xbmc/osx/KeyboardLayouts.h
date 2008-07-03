/*!
 \file  KeyboardLayouts.h
 \brief
 */

#ifndef _KEYBOARD_LAYOUTS_H_
#define _KEYBOARD_LAYOUTS_H_ 

#import <Carbon/Carbon.h>
#import <CoreFoundation/CoreFoundation.h>
#import "../guilib/common/SDLKeyboard.h"

#define APPLE_HOTKEY_COMMAND 0x100000
#define APPLE_HOTKEY_ALT     0x80000
#define APPLE_HOTKEY_CTRL    0x40000
#define APPLE_HOTKEY_SHIFT   0x20000

#define APPLE_HOTKEY_PREF_APPID  "com.apple.symbolichotkeys"
#define APPLE_HOTKEY_PREF_KEYID  "AppleSymbolicHotKeys"
#define APPLE_HOTKEY_PREF_HOT1   "60"
#define APPLE_HOTKEY_PREF_HOT2   "61"

class COSXKeyboardLayouts
  {
  public:
    COSXKeyboardLayouts();
    ~COSXKeyboardLayouts();
    void HoldLayout();
    void RepareLayout();
    bool Process(SVKey key);
    bool SetASCIILayout();
    
  protected:
    CFArrayRef layouts;
    CFIndex h_layout; // Hold layout
    CFIndex c_layout; // index of current layout into 'layouts'
    SVKey c_key;      // hotkey from user defaults for change keyboard layouts
    
    void GetLayouts();
    void GetKeyboardShortCut();
    bool GetHotkeyPref(CFPropertyListRef p_hotkeys, CFStringRef Pref);
  };

extern COSXKeyboardLayouts g_OSXKeyboardLayouts;

#endif