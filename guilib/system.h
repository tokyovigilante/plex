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

#ifdef _XBOX
#include <xtl.h>
#include <xvoice.h>
#include <xonline.h>
#define HAS_XBOX_D3D
#define HAS_RAM_CONTROL
#define HAS_XFONT
#define HAS_FILESYSTEM_CDDA
#define HAS_FILESYSTEM_SMB
#define HAS_FILESYSTEM_RTV
#define HAS_FILESYSTEM_DAAP
#define HAS_FILESYSTEM
#define HAS_GAMEPAD
#define HAS_IR_REMOTE
#define HAS_DVD_DRIVE
#define HAS_XBOX_HARDWARE
#define HAS_XBOX_NETWORK
#define HAS_VIDEO_PLAYBACK
#define HAS_MPLAYER
#define HAS_DVDPLAYER
#define HAS_AC3_CODEC
#define HAS_DTS_CODEC
#define HAS_AC3_CDDA_CODEC
#define HAS_DTS_CDDA_CODEC
#define HAS_WMA_CODEC
#define HAS_XBOX_AUDIO
#define HAS_AUDIO_PASS_THROUGH
#define HAS_WEB_SERVER
#define HAS_FTP_SERVER
#define HAS_TIME_SERVER
#define HAS_VISUALISATION
#define HAS_KARAOKE
#define HAS_KAI_VOICE
#define HAS_CREDITS
#define HAS_MODPLAYER
#define HAS_SYSINFO
#define HAS_SCREENSAVER
#define HAS_MIKMOD
#define HAS_SECTIONS
#define HAS_UPNP
#define HAS_LCD
#define HAS_UNDOCUMENTED
#define HAS_SECTIONS
#define HAS_CDDA_RIPPER
#define HAS_PYTHON
#define HAS_TRAINER
#undef  HAS_SDL
#define HAS_AUDIO
#define HAS_EVENT_SERVER
#else
#undef HAS_XBOX_D3D
#undef HAS_RAM_CONTROL
#undef HAS_XFONT
#undef HAS_FILESYSTEM_CDDA
#undef HAS_FILESYSTEM_SMB
#undef HAS_FILESYSTEM_RTV
#undef HAS_FILESYSTEM_DAAP
#undef HAS_FILESYSTEM
#undef HAS_GAMEPAD
#undef HAS_IR_REMOTE
#undef HAS_DVD_DRIVE
#undef HAS_XBOX_HARDWARE
#undef HAS_XBOX_NETWORK
#ifdef HAS_SDL
#define HAS_VIDEO_PLAYBACK
#define HAS_DVDPLAYER
#else
#undef HAS_VIDEO_PLAYBACK
#undef HAS_DVDPLAYER
#endif
#undef HAS_MPLAYER
#undef HAS_AC3_CODEC
#undef HAS_DTS_CODEC
#undef HAS_AC3_CDDA_CODEC
#undef HAS_DTS_CDDA_CODEC
#define HAS_WMA_CODEC
#undef HAS_XBOX_AUDIO
#undef HAS_AUDIO_PASS_THROUGH
#undef HAS_FTP_SERVER
#define HAS_WEB_SERVER
#undef HAS_TIME_SERVER
#undef HAS_VISUALISATION
#undef HAS_KARAOKE
#undef HAS_KAI_VOICE
#undef HAS_CREDITS
#undef HAS_MODPLAYER
#undef HAS_SYSINFO
#define HAS_SCREENSAVER
#undef HAS_MIKMOD
#undef HAS_SECTIONS
#define HAS_UPNP
#undef HAS_LCD
#undef HAS_UNDOCUMENTED
#undef HAS_SECTIONS
#undef HAS_CDDA_RIPPER
#define HAS_PYTHON
#define HAS_TRAINER
#define HAS_AUDIO
#define HAS_SHOUTCAST
#define HAS_RAR
#undef  HAS_LIRC
#define HAS_KAI

#ifndef _LINUX
// additional includes and defines
#if !(defined(_WINSOCKAPI_) || defined(_WINSOCK_H))
#include <winsock2.h>
#endif
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include "DInput.h"
#include "DSound.h"
#define DSSPEAKER_USE_DEFAULT DSSPEAKER_STEREO
#define LPDIRECTSOUND8 LPDIRECTSOUND
#undef GetFreeSpace
#undef HAS_CCXSTREAM
#endif

#ifdef _LINUX
#ifndef __APPLE__
#define HAS_LCD
#include "../config.h"
#endif
#define HAS_PYTHON
#undef  HAS_TRAINER
#define HAS_WEB_SERVER
#define HAS_EVENT_SERVER
#define HAS_UPNP
#undef  HAS_AUDIO
#define  HAS_SHOUTCAST
#define HAS_SDL
#define HAS_RAR
#ifndef __APPLE__
#define HAS_HAL
#endif
#define HAS_FILESYSTEM_CDDA
#define HAS_FILESYSTEM_SMB
#define HAS_FILESYSTEM
#define HAS_SYSINFO
#define HAS_VIDEO_PLAYBACK
#undef  HAS_MPLAYER
#define HAS_VISUALISATION
#define HAS_DVDPLAYER
#define HAS_DVD_DRIVE
#define HAS_WMA_CODEC
#define HAS_CCXSTREAM
#define HAS_LIRC
#define HAS_AC3_CODEC
#define HAS_DTS_CODEC
#define HAS_CDDA_RIPPER
#define HAS_FILESYSTEM_RTV
#define HAS_FILESYSTEM_DAAP
#define HAS_PERFORMANCE_SAMPLE
#define HAS_LINUX_NETWORK
#undef HAS_KAI

#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include "PlatformInclude.h"
#endif

#ifdef HAS_SDL
#define HAS_SDL_AUDIO
#define HAS_DVD_SWSCALE
#ifndef HAS_SDL_2D
#define HAS_SDL_OPENGL
#ifdef _LINUX
#ifndef __APPLE__
#define HAS_GLX
#endif
#endif
#endif
#ifdef _WIN32
#define _WIN32PC       // precompiler definition for the windows build
#define HAS_AC3_CODEC
#define HAS_DTS_CODEC
#define HAS_CDDA_RIPPER
#define HAS_DVD_SWSCALE
#define HAS_FILESYSTEM
#define HAS_FILESYSTEM_SMB
#define HAS_FILESYSTEM_CDDA
#define HAS_FILESYSTEM_RTV
#define HAS_FILESYSTEM_DAAP
#define HAS_DVD_DRIVE
#define HAS_VISUALISATION
#define HAS_CCXSTREAM
#define HAS_EVENT_SERVER
#define HAS_SHOUTCAST

#undef HAS_SDL_AUDIO   // use dsound for audio on win32
#undef HAS_SCREENSAVER // no screensavers
#undef HAS_PERFORMANCE_SAMPLE // no performance sampling
#undef HAS_LINUX_NETWORK
#undef HAS_KAI
#undef HAS_KAI_VOICE
#undef HAS_TRAINER

#include "../xbmc/win32/PlatformInclude.h"
#endif
#endif

#endif

#if (defined(HAS_XBOX_D3D)  && defined(HAS_SDL))
#error "Cannot have both HAS_XBOX_D3D and HAS_SDL defined simultaneously!"
#endif

#ifndef SVN_REV
#define SVN_REV "Unknown"
#endif
