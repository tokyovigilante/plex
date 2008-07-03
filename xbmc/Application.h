#pragma once

/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "XBApplicationEx.h"

#include "IMsgTargetCallback.h"
#include "Key.h"

class CFileItem;
class CFileItemList;

#include "GUIDialogSeekBar.h"
#include "GUIDialogKaiToast.h"
#include "GUIDialogVolumeBar.h"
#include "GUIDialogMuteBug.h"
#include "GUIWindowPointer.h"   // Mouse pointer
#include "Settings.h"

#include "utils/Idle.h"
#include "utils/DelayController.h"
#include "cores/IPlayer.h"
#include "cores/PlayerCoreFactory.h"
#include "PlayListPlayer.h"
#include "DetectDVDType.h"
#include "Autorun.h"
#include "utils/Splash.h"
#include "utils/Stopwatch.h"
#include "ApplicationMessenger.h"
#include "utils/Network.h"
#ifdef HAS_PERFORMANCE_SAMPLE
#include "utils/PerformanceStats.h"
#endif
#ifdef _LINUX
#include "linux/LinuxResourceCounter.h"
#endif

class CWebServer;
class CXBFileZilla;
class CSNTPClient;
class CCdgParser;
class CApplicationMessenger;

class CBackgroundPlayer : public CThread
{
public:
  CBackgroundPlayer(const CFileItem &item, int iPlayList);
  virtual ~CBackgroundPlayer();
  virtual void Process();
protected:
  CFileItem *m_item;
  int       m_iPlayList;
};

class CApplication : public CXBApplicationEx, public IPlayerCallback, public IMsgTargetCallback
{
public:
  CApplication(void);
  virtual ~CApplication(void);
  virtual HRESULT Initialize();
  virtual void FrameMove();
  virtual void Render();
  virtual void DoRender();
#ifndef HAS_XBOX_D3D
  virtual void RenderNoPresent();
#endif
  virtual HRESULT Create(HWND hWnd);
  virtual HRESULT Cleanup();
  void StartServices();
  void StopServices();
  void StartIdleThread();
  void StopIdleThread();
#ifdef HAS_KAI
  void StartKai();
  void StopKai();
#endif
  void StartWebServer();
  void StopWebServer();
  void StartFtpServer();
  void StopFtpServer();
  void StartTimeServer();
  void StopTimeServer();
  void StartUPnP();
  void StopUPnP();
  void StartUPnPRenderer();
  void StopUPnPRenderer();
  void StartUPnPClient();
  void StopUPnPClient();
  void StartUPnPServer();
  void StopUPnPServer();
  void StartEventServer();
  void StopEventServer();
  void RefreshEventServer();
  void StartLEDControl(bool switchoff = false);
  void DimLCDOnPlayback(bool dim);
  void PrintXBEToLCD(const char* xbePath);
  void CheckDate();
  DWORD GetThreadId() const { return m_threadID; };
  void Stop();
  void RestartApp();
  void LoadSkin(const CStdString& strSkin);
  void UnloadSkin();
  bool LoadUserWindows(const CStdString& strSkinPath);
  void DelayLoadSkin();
  void CancelDelayLoadSkin();
  void ReloadSkin();
  const CStdString& CurrentFile();
  CFileItem& CurrentFileItem();
  virtual bool OnMessage(CGUIMessage& message);
  const EPLAYERCORES GetCurrentPlayer();
  virtual void OnPlayBackEnded();
  virtual void OnPlayBackStarted();
  virtual void OnPlayBackStopped();
  virtual void OnQueueNextItem();
  bool PlayMedia(const CFileItem& item, int iPlaylist = PLAYLIST_MUSIC);
  bool PlayMediaSync(const CFileItem& item, int iPlaylist = PLAYLIST_MUSIC);
  bool ProcessAndStartPlaylist(const CStdString& strPlayList, PLAYLIST::CPlayList& playlist, int iPlaylist);
  bool PlayFile(const CFileItem& item, bool bRestart = false);
  void StopPlaying();
  void Restart(bool bSamePosition = true);
  void DelayedPlayerRestart();
  void CheckDelayedPlayerRestart();
  void RenderFullScreen();
  void DoRenderFullScreen();
  bool NeedRenderFullScreen();
  bool IsPlaying() const ;
  bool IsPaused() const;
  bool IsPlayingAudio() const ;
  bool IsPlayingVideo() const ;
  bool IsPlayingFullScreenVideo() const ;
  bool OnKey(CKey& key);
  bool OnAction(const CAction &action);
  void RenderMemoryStatus();
#ifdef HAS_XBOX_HARDWARE
  bool MustBlockHDSpinDown(bool bCheckThisForNormalSpinDown = true);
  void CheckNetworkHDSpinDown(bool playbackStarted = false);
  void CheckHDSpindown();
#endif
  void CheckShutdown();
  void CheckDisplaySleep();
  void CheckScreenSaver();   // CB: SCREENSAVER PATCH
  void CheckPlayingProgress();
  void CheckAudioScrobblerStatus();
  void ActivateScreenSaver(bool forceType = false);

  virtual void Process();
  void ProcessSlow();
  void ResetScreenSaver();
  int GetVolume() const;
  void SetVolume(int iPercent);
  void Mute(void);
  int GetPlaySpeed() const;
  void SetPlaySpeed(int iSpeed);
  bool IsButtonDown(DWORD code);
  bool AnyButtonDown();
  bool ResetScreenSaverWindow();
#ifdef HAS_KAI
  void SetKaiNotification(const CStdString& aCaption, const CStdString& aDescription, CGUIImage* aIcon=NULL);
#endif
  double GetTotalTime() const;
  double GetTime() const;
  float GetPercentage() const;
  void SeekPercentage(float percent);
  void SeekTime( double dTime = 0.0 );
  void ResetPlayTime();

  void SaveMusicScanSettings();
  void RestoreMusicScanSettings();
  void CheckMusicPlaylist();

  CApplicationMessenger& getApplicationMessenger();
#ifdef HAS_LINUX_NETWORK
  CNetworkLinux& getNetwork();
#else
  CNetwork& getNetwork();
#endif
#ifdef HAS_PERFORMANCE_SAMPLE
  CPerformanceStats &GetPerformanceStats();
#endif

  CGUIDialogVolumeBar m_guiDialogVolumeBar;
  CGUIDialogSeekBar m_guiDialogSeekBar;
  CGUIDialogKaiToast m_guiDialogKaiToast;
  CGUIDialogMuteBug m_guiDialogMuteBug;
  CGUIWindowPointer m_guiPointer;

  CIdleThread m_idleThread;
  MEDIA_DETECT::CAutorun m_Autorun;
  MEDIA_DETECT::CDetectDVDMedia m_DetectDVDType;
  CDelayController m_ctrDpad;
  CSNTPClient *m_psntpClient;
  CWebServer* m_pWebServer;
  CXBFileZilla* m_pFileZilla;
  IPlayer* m_pPlayer;

  bool m_bSpinDown;
  bool m_bNetworkSpinDown;
  DWORD m_dwSpinDownTime;

  inline bool IsInScreenSaver() { return m_bScreenSave; };
  int m_iScreenSaveLock; // spiff: are we checking for a lock? if so, ignore the screensaver state, if -1 we have failed to input locks

  DWORD m_dwSkinTime;
  bool m_bIsPaused;

  CCdgParser* m_pCdgParser;

  EPLAYERCORES m_eForcedNextPlayer;
  CStdString m_strPlayListFile;

  int GlobalIdleTime();
  void NewFrame();
  void SetQuiet(bool bQuiet);
  void EnablePlatformDirectories(bool enable=true) 
  { 
    m_bPlatformDirectories = enable; 
  }

protected:
  friend class CApplicationMessenger;
  // screensaver
  bool m_bInactive;
  bool m_bDisplaySleeping;
  bool m_bScreenSave;
  CStdString m_screenSaverMode;
  DWORD m_dwSaverTick;
#ifdef __APPLE__
  DWORD m_dwOSXscreensaverTicks;
#endif
#ifndef HAS_SDL
  D3DGAMMARAMP m_OldRamp;
#else
  Uint16 m_OldRampRed[256];
  Uint16 m_OldRampGreen[256];
  Uint16 m_OldRampBlue[256];
#endif

  // timer information
  CStopWatch m_idleTimer;
  CStopWatch m_restartPlayerTimer;
  CStopWatch m_frameTime;
  CStopWatch m_navigationTimer;
  CStopWatch m_slowTimer;

  CFileItem* m_itemCurrentFile;
  CFileItemList* m_currentStack;
  CStdString m_prevMedia;
  CSplash* m_splash;
  DWORD m_threadID;       // application thread ID.  Used in applicationMessanger to know where we are firing a thread with delay from.
  EPLAYERCORES m_eCurrentPlayer;
  bool m_bXboxMediacenterLoaded;
  bool m_bSettingsLoaded;
  bool m_bAllSettingsLoaded;
  bool m_bInitializing;
  bool m_playCountUpdated;
  bool m_bQuiet;
  bool m_bPlatformDirectories;  

  int m_iPlaySpeed;
  int m_currentStackPosition;
  int m_nextPlaylistItem;

  bool m_bPresentFrame;

  char* m_logPath;

#ifdef HAS_SDL
  int        m_frameCount;
  SDL_mutex* m_frameMutex;
  SDL_cond*  m_frameCond;
#endif

  static LONG WINAPI UnhandledExceptionFilter(struct _EXCEPTION_POINTERS *ExceptionInfo);

  void SetHardwareVolume(long hardwareVolume);
  void UpdateLCD();
  void FatalErrorHandler(bool InitD3D, bool MapDrives, bool InitNetwork);
  void InitBasicD3D();

  bool PlayStack(const CFileItem& item, bool bRestart);
  bool SwitchToFullScreen();
  bool ProcessMouse();
  bool ProcessHTTPApiButtons();
  bool ProcessKeyboard();
  bool ProcessRemote(float frameTime);
  bool ProcessGamepad(float frameTime);
  bool ProcessEventServer(float frameTime);
  
  bool ProcessJoystickEvent(const std::string& joystickName, int button, bool isAxis, float fAmount);
  
  void CheckForDebugButtonCombo();
  void StartFtpEmergencyRecoveryMode();
  float NavigationIdleTime();
  void CheckForTitleChange();

  void SaveCurrentFileSettings();

  void InitDirectories();
  CProfile* InitDirectoriesLinux();
  CProfile* InitDirectoriesOSX();
  CProfile* InitDirectoriesWin32();

  CApplicationMessenger m_applicationMessenger;
#ifdef HAS_LINUX_NETWORK
  CNetworkLinux m_network;
#else
  CNetwork    m_network;
#endif
#ifdef HAS_PERFORMANCE_SAMPLE
  CPerformanceStats m_perfStats;
#endif
#ifdef _LINUX
  CLinuxResourceCounter m_resourceCounter;
#endif
  
#ifdef HAS_EVENT_SERVER
  std::map<std::string, std::map<int, float> > m_lastAxisMap;
#endif
};

extern CApplication g_application;
extern CStdString g_LoadErrorStr;
