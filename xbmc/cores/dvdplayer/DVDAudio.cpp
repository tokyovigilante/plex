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
#include "DVDAudio.h"
#ifdef _XBOX
#include "cores/mplayer/ASyncDirectSound.h"
#include "cores/mplayer/ac97directsound.h"
#elif defined(__APPLE__)
#include "CoreAudioAUHAL.h"
#elif _LINUX
#include "ALSADirectSound.h"
#else
#include "cores/mplayer/Win32DirectSound.h"
#endif
#include "../../Util.h"
#include "DVDClock.h"
#include "DVDCodecs/DVDCodecs.h"
#include "DVDPlayerAudio.h"

using namespace std;

CDVDAudio::CDVDAudio(volatile bool &bStop)
  : m_bStop(bStop)
{
  m_pAudioDecoder = NULL;
  m_pCallback = NULL;
  m_iBufferSize = 0;
  m_dwPacketSize = 0;
  m_pBuffer = NULL;
}

CDVDAudio::~CDVDAudio()
{
  CSingleLock lock (m_critSection);
  if (m_pAudioDecoder)
  {
    m_pAudioDecoder->Deinitialize();
    delete m_pAudioDecoder;
  }
  if (m_pBuffer) delete[] m_pBuffer;
}

void CDVDAudio::RegisterAudioCallback(IAudioCallback* pCallback)
{
  CSingleLock lock (m_critSection);
  m_pCallback = pCallback;
  if (m_pCallback && m_pAudioDecoder) m_pAudioDecoder->RegisterAudioCallback(pCallback);
}

void CDVDAudio::UnRegisterAudioCallback()
{
  CSingleLock lock (m_critSection);
  m_pCallback = NULL;
  if (m_pAudioDecoder) m_pAudioDecoder->UnRegisterAudioCallback();
}

bool CDVDAudio::Create(const DVDAudioFrame &audioframe, CodecID codec)
{
  CLog::Log(LOGNOTICE, "Creating audio device with codec id: %i, channels: %i, sample rate: %i, %s", codec, audioframe.channels, audioframe.sample_rate, audioframe.passthrough ? "pass-through" : "no pass-through");

  // if passthrough isset do something else
  CSingleLock lock (m_critSection);

  const char* codecstring="";

  if(codec == CODEC_ID_AAC)
    codecstring = "AAC";
  else if(codec == CODEC_ID_VORBIS)
    codecstring = "Vorbis";
  else if(codec == CODEC_ID_AC3)
    codecstring = "AC3";
  else if(codec == CODEC_ID_DTS)
	codecstring = "DTS";
  else
    codecstring = "PCM";

#ifdef _XBOX
  // we don't allow resampling now, there is a bug in sscc that causes it to return the wrong chunklen.
  if( audioframe.passthrough )
    m_pAudioDecoder = new CAc97DirectSound(m_pCallback, audioframe.channels, audioframe.sample_rate, audioframe.bits_per_sample, true); // true = resample, 128 buffers
  else
    m_pAudioDecoder = new CASyncDirectSound(m_pCallback, audioframe.channels, audioframe.sample_rate, audioframe.bits_per_sample, codecstring);
#elif __APPLE__
//	PortAudioDirectSound* paDecoder = new PortAudioDirectSound(m_pCallback, audioframe.channels, audioframe.sample_rate, audioframe.bits_per_sample, false, codecstring, false, audioframe.passthrough);
    CoreAudioAUHAL* paDecoder = new CoreAudioAUHAL(m_pCallback, audioframe.channels, audioframe.sample_rate, audioframe.bits_per_sample, false, codecstring, false, audioframe.passthrough);
  if (paDecoder->IsValid() == false)
  {
    delete paDecoder;
    paDecoder = 0;
  }
  
  m_pAudioDecoder = paDecoder;

#elif _LINUX

  m_pAudioDecoder = new CALSADirectSound(m_pCallback, audioframe.channels, audioframe.sample_rate, audioframe.bits_per_sample, false, codecstring, false, audioframe.passthrough);

#else

  m_pAudioDecoder = new CWin32DirectSound(m_pCallback, audioframe.channels, audioframe.sample_rate, audioframe.bits_per_sample, false, codecstring, false, audioframe.passthrough);

#endif

  if (!m_pAudioDecoder) return false;

  m_iChannels = audioframe.channels;
  m_iBitrate = audioframe.sample_rate;
  m_iBitsPerSample = audioframe.bits_per_sample;
  m_bPassthrough = audioframe.passthrough;

  m_dwPacketSize = m_pAudioDecoder->GetChunkLen();
  if (m_pBuffer) delete[] m_pBuffer;
  m_pBuffer = new BYTE[m_dwPacketSize];

  
  return true;
}

void CDVDAudio::Destroy()
{
  CSingleLock lock (m_critSection);

  if (m_pAudioDecoder)
  {
    m_pAudioDecoder->Stop();
    m_pAudioDecoder->Deinitialize();
    delete m_pAudioDecoder;
  }

  if (m_pBuffer) delete[] m_pBuffer;
  m_pBuffer = NULL;
  m_dwPacketSize = 0;
  m_pAudioDecoder = NULL;
  m_iBufferSize = 0;
  m_iChannels = 0;
  m_iBitrate = 0;
  m_iBitsPerSample = 0;
  m_bPassthrough = false;
  m_iSpeed = 1;
}

void CDVDAudio::SetSpeed(int iSpeed)
{
  m_iSpeed = abs(iSpeed);

}

DWORD CDVDAudio::AddPacketsRenderer(unsigned char* data, DWORD len)
{ 
  //Since we write same data size each time, we can drop full chunks to simulate a specific playback speed
  //m_iSpeedStep = (m_iSpeedStep+1) % m_iSpeed;
  //if( m_iSpeedStep )
  //  return m_dwPacketSize;
  //else
  CSingleLock lock(m_critSection);

  if(!m_pAudioDecoder)
    return 0;

  DWORD total = len;
  DWORD copied;
  do
  {    
    copied = m_pAudioDecoder->AddPackets(data, len);
    data += copied;
    len -= copied;
    if (len < m_dwPacketSize)
      break;

    lock.Leave();
    Sleep(1);
    lock.Enter();
  } while (!m_bStop);

  return total - len;
}

DWORD CDVDAudio::AddPackets(const DVDAudioFrame &audioframe)
{
  unsigned char* data = audioframe.data;
  DWORD len = audioframe.size;

  DWORD total = len;
  DWORD copied;

  if (m_iBufferSize > 0)
  {
    copied = std::min(m_dwPacketSize - m_iBufferSize, len);

    memcpy(m_pBuffer + m_iBufferSize, data, copied);
    data += copied;
    len -= copied;
    m_iBufferSize += copied;

    if(m_iBufferSize < m_dwPacketSize)
      return copied;

    if(AddPacketsRenderer(m_pBuffer, m_iBufferSize) != m_iBufferSize)
    {
      m_iBufferSize = 0;
      CLog::Log(LOGERROR, "%s - failed to add leftover bytes to render", __FUNCTION__);
      return copied;
    }

    m_iBufferSize = 0;
  }
  
  copied = AddPacketsRenderer(data, len);
  data += copied;
  len -= copied;

  // if we have more data left, save it for the next call to this funtion
  if (len > 0 && !m_bStop)
  {
    CSingleLock lock (m_critSection);
    if(len > m_dwPacketSize)
      CLog::Log(LOGERROR, "%s - More bytes left than can be stored in buffer", __FUNCTION__);

      m_iBufferSize = std::min(len, m_dwPacketSize);
    memcpy(m_pBuffer, data, m_iBufferSize);
    len  -= m_iBufferSize;
    data += m_iBufferSize;
  }
  return total - len;
}

void CDVDAudio::DoWork()
{
  CSingleLock lock (m_critSection);
  if (m_pAudioDecoder) m_pAudioDecoder->DoWork();
}

void CDVDAudio::SetVolume(int iVolume)
{
  CSingleLock lock (m_critSection);
  if (m_pAudioDecoder) m_pAudioDecoder->SetCurrentVolume(iVolume);
}

void CDVDAudio::SetDynamicRangeCompression(long drc)
{
  CSingleLock lock (m_critSection);
  if (m_pAudioDecoder) m_pAudioDecoder->SetDynamicRangeCompression(drc);
}

void CDVDAudio::Pause()
{
  CSingleLock lock (m_critSection);
  if (m_pAudioDecoder) m_pAudioDecoder->Pause();
}

void CDVDAudio::Resume()
{
  CSingleLock lock (m_critSection);
  if (m_pAudioDecoder) m_pAudioDecoder->Resume();
}

double CDVDAudio::GetDelay()
{
  CSingleLock lock (m_critSection);

  double delay = 0.0;
  if(m_pAudioDecoder)
    delay = m_pAudioDecoder->GetDelay();

  DWORD bps = m_iChannels * m_iBitrate * (m_iBitsPerSample>>3);
  if(m_iBufferSize && bps)
    delay += (double)m_iBufferSize / bps;

  return delay * DVD_TIME_BASE;
}

void CDVDAudio::Flush()
{
  CSingleLock lock (m_critSection);

  if (m_pAudioDecoder)
  {
    m_pAudioDecoder->Stop();
    m_pAudioDecoder->Resume();
  }
  m_iBufferSize = 0;
}

bool CDVDAudio::IsValidFormat(const DVDAudioFrame &audioframe)
{
  if(!m_pAudioDecoder)
    return false;

  if(audioframe.passthrough != m_bPassthrough)
    return false;

  if(audioframe.channels != m_iChannels 
  || audioframe.sample_rate != m_iBitrate 
  || audioframe.bits_per_sample != m_iBitsPerSample)
    return false;

  return true;
}
