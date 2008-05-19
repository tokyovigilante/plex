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

#include "stdafx.h"
#include "ASAPCodec.h"
#include "Util.h"
#include "FileSystem/File.h"

ASAPCodec::ASAPCodec()
{
  m_CodecName = "ASAP";
}

ASAPCodec::~ASAPCodec()
{
}

bool ASAPCodec::Init(const CStdString &strFile, unsigned int filecache)
{
  if (!m_dll.Load())
    return false;

  CStdString strFileToLoad = strFile;
  int song = -1;
  CStdString strExtension;
  CUtil::GetExtension(strFile, strExtension);
  strExtension.MakeLower();
  if (strExtension == ".asapstream")
  {
    CStdString strFileName = CUtil::GetFileName(strFile);
    int iStart = strFileName.ReverseFind('-') + 1;
    song = atoi(strFileName.substr(iStart, strFileName.size() - iStart - 11).c_str()) - 1;
    CStdString strPath = strFile;
    CUtil::GetDirectory(strPath, strFileToLoad);
    CUtil::RemoveSlashAtEnd(strFileToLoad);
  }

  int duration;
  if (!m_dll.asapLoad(strFileToLoad.c_str(), song, &m_Channels, &duration))
    return false;
  m_TotalTime = duration;
  m_SampleRate = 44100;
  m_BitsPerSample = 16;
  return true;
}

void ASAPCodec::DeInit()
{
}

__int64 ASAPCodec::Seek(__int64 iSeekTime)
{
  m_dll.asapSeek((int) iSeekTime);
  return iSeekTime;
}

int ASAPCodec::ReadPCM(BYTE *pBuffer, int size, int *actualsize)
{
  *actualsize = m_dll.asapGenerate(pBuffer, size);
  if (*actualsize < size)
    return READ_EOF;
  return READ_SUCCESS;
}

bool ASAPCodec::CanInit()
{
  return m_dll.CanLoad();
}

bool ASAPCodec::IsSupportedFormat(const CStdString &strExt)
{
  CStdString ext = strExt;
  if (ext[0] == '.')
    ext.erase(0, 1);
  return ext == "sap"
    || ext == "cmc" || ext == "cmr" || ext == "dmc"
    || ext == "mpt" || ext == "mpd" || ext == "rmt"
    || ext == "tmc" || ext == "tm8" || ext == "tm2";
}