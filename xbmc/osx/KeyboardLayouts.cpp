/*!
 \file  KeyboardLayouts.cpp
 \brief
 */

#include "KeyboardLayouts.h"


COSXKeyboardLayouts g_OSXKeyboardLayouts;

COSXKeyboardLayouts::COSXKeyboardLayouts()
{
  c_key.Shift = c_key.Apple = c_key.Ctrl = c_key.Alt = false;
  c_key.SVKey = '\0';
  h_layout = c_layout = -1;
  layouts = NULL;
  
  GetLayouts();

  if(c_layout > -1)
  {
    // may be this
    SetASCIILayout();
    //  
    GetKeyboardShortCut();
  }

}
COSXKeyboardLayouts::~COSXKeyboardLayouts()
{
  if (h_layout != -1) RepareLayout();
  
  CFRelease(layouts);
}
void COSXKeyboardLayouts::HoldLayout()
{
  h_layout = c_layout;
}
void COSXKeyboardLayouts::RepareLayout()
{
  if (h_layout > -1)
    if (h_layout == c_layout) h_layout = -1;
    else if ( TISSelectInputSource((TISInputSourceRef) CFArrayGetValueAtIndex(layouts, h_layout)) == noErr)
    {
      c_layout = h_layout;
      h_layout = -1;
    }
}
bool COSXKeyboardLayouts::Process(SVKey key)
{
   if (c_layout > -1
      
      && c_key.Alt == key.Alt 
      && c_key.Apple == key.Apple
      && c_key.Ctrl == key.Ctrl
      && c_key.Shift == key.Shift
      && c_key.SVKey == key.SVKey)
  {
    if(++c_layout >= CFArrayGetCount(layouts)) c_layout = 0;
    if (TISSelectInputSource((TISInputSourceRef) CFArrayGetValueAtIndex(layouts, c_layout)) == noErr) return true;
    else
    {
      c_layout--;
      return false;
    } 
  }
  else return false;
}

void COSXKeyboardLayouts::GetKeyboardShortCut()
{
	
  CFPropertyListRef p_hotkeys;
  Boolean           result = false;
  
  p_hotkeys = CFPreferencesCopyAppValue(CFSTR( APPLE_HOTKEY_PREF_KEYID ), CFSTR( APPLE_HOTKEY_PREF_APPID ));
  
  if ( p_hotkeys != NULL ) 
  {
    /*if (!(*/result = GetHotkeyPref(p_hotkeys, CFSTR(APPLE_HOTKEY_PREF_HOT1)) /*))
      result = GetHotkeyPref(p_hotkeys, CFSTR(APPLE_HOTKEY_PREF_HOT2)) */;

    CFRelease(p_hotkeys);
  }
  
  
  // DEFAULT
  if (!result)
  {
    c_key.Apple = true;
    c_key.SVKey = 49;
  }
  
}

void COSXKeyboardLayouts::GetLayouts()
{
  CFDictionaryRef props;
  CFStringRef keys[] = {kTISPropertyInputSourceType};
  CFStringRef vals[] = {kTISTypeKeyboardLayout};
  CFRange range = {0,0};
  
  props = CFDictionaryCreate(NULL, (const void **) keys, (const void**) vals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  layouts = TISCreateInputSourceList(props, false);
  if(layouts != NULL && (range.length = CFArrayGetCount(layouts)) > 1) 
  {
    TISInputSourceRef temp;
    c_layout = CFArrayGetFirstIndexOfValue(layouts, range, (temp = TISCopyCurrentKeyboardLayoutInputSource()));
    CFRelease(temp);
  }
  
  CFRelease(props);
}

bool COSXKeyboardLayouts::SetASCIILayout()
{
  TISInputSourceRef temp;
  OSStatus err;
  
  err = TISSelectInputSource( ( temp = TISCopyCurrentASCIICapableKeyboardLayoutInputSource() ) );
  CFRelease(temp);
  if(err == noErr) return true;
  else return false;
}

bool COSXKeyboardLayouts::GetHotkeyPref(CFPropertyListRef p_hotkeys, CFStringRef Pref)
{
  CFDictionaryRef   p_dict = NULL;
  CFBooleanRef      p_bool = NULL;
  CFArrayRef        p_arr  = NULL;
  CFNumberRef       p_num  = NULL;
  long              l_num;
  
  Boolean rt = CFDictionaryGetValueIfPresent((CFDictionaryRef) p_hotkeys, (const void *) Pref, (const void * *) &p_dict);
  if(rt)
  {
    rt = CFDictionaryGetValueIfPresent((CFDictionaryRef) p_dict, (const void *) CFSTR( "enabled" ), (const void * *) &p_bool);
    if ( CFBooleanGetValue(p_bool))
    {
      rt = CFDictionaryGetValueIfPresent((CFDictionaryRef) p_dict, (const void *) CFSTR( "value" ), (const void * *) &p_dict);
      if (rt)
      {
        rt = CFDictionaryGetValueIfPresent((CFDictionaryRef) p_dict, (const void *) CFSTR( "parameters" ), (const void * *) &p_arr);
        if (rt)
        {
          p_num =(CFNumberRef) CFArrayGetValueAtIndex(p_arr, 1);
          if (p_num != NULL)
          {
            rt = CFNumberGetValue(p_num, kCFNumberCharType, &(c_key.SVKey)); // get it .. part 1
            if (rt)
            {
              p_num =(CFNumberRef) CFArrayGetValueAtIndex(p_arr, 2);
              if (p_num != NULL)
              {
                rt = CFNumberGetValue(p_num, kCFNumberLongType, &l_num); // get it .. part 2
                if (rt)
                {
                  c_key.Alt = (l_num & APPLE_HOTKEY_ALT) != 0;
                  c_key.Apple = (l_num & APPLE_HOTKEY_COMMAND) != 0;
                  c_key.Ctrl = (l_num & APPLE_HOTKEY_CTRL) != 0;
                  c_key.Shift = (l_num & APPLE_HOTKEY_SHIFT) != 0;
                  
                  // Mega return
                  return true;
                }
              }
            }
          }
        }
      }
    }
  }
  
  return false;
  
}