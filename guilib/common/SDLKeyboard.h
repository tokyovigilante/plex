#ifndef SDL_KEYBOARD_H
#define SDL_KEYBOARD_H

#include "../system.h"

#ifdef HAS_SDL
#include <SDL/SDL.h>

typedef struct
{
  bool Shift;
  bool Ctrl;
  bool Alt;
  bool Apple;
  BYTE SVKey;	
} SVKey ;

class CLowLevelKeyboard
{
public:
  CLowLevelKeyboard();

  void Initialize(HWND hwnd);
  void Reset();
  void Update(SDL_Event& event);
  bool GetShift() { return m_bShift;};
  bool GetCtrl() { return m_bCtrl;};
  bool GetAlt() { return m_bAlt;};
  bool GetRAlt() { return m_bRAlt;};
  char GetAscii() { return m_cAscii;}; // FIXME should be replaced completly by GetUnicode() 
  bool GetApple() { return m_bApple;}; // Get 'Command' button as modificator
  SVKey GetSVKey(); //Get struct SVKEY - VKey with modificators
  WCHAR GetUnicode() { return m_wUnicode;};
  BYTE GetKey() { return m_VKey;};

private:
  bool m_bShift;
  bool m_bCtrl;
  bool m_bAlt;
  bool m_bRAlt;
  char m_cAscii;
  bool m_bApple;
  Uint8 m_KeyCode;
  WCHAR m_wUnicode;
  BYTE m_VKey;
#ifdef HAS_SDL_JOYSTICK
  SDL_Joystick* m_pJoy;
#endif
};

#endif

#endif
