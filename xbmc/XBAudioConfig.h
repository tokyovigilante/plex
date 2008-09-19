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

#define XC_AUDIO_FLAGS 9
#define XC_AUDIO_FLAGS_ENABLE_AC3 1
#define XC_AUDIO_FLAGS_ENABLE_DTS 2

class XBAudioConfig
{
public:
  XBAudioConfig();
  ~XBAudioConfig();

  bool HasDigitalOutput();

  bool GetAC3Enabled();
  void SetAC3Enabled(bool bEnable);
  bool GetDTSEnabled();
  void SetDTSEnabled(bool bEnable);
  bool NeedsSave();
  void Save();

private:
  DWORD m_dwAudioFlags;
};

extern XBAudioConfig g_audioConfig;
