/*
 *  CoreAudioAUHAL.h
 *  Plex
 *
 *  Created by Ryan Walklin on 9/6/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

/*
 * XBoxMediaPlayer
 * Copyright (c) 2002 d7o3g4q and RUNTiME
 * Portions Copyright (c) by the authors of ffmpeg and xvid
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __COREAUDIO_AUHAL_H__
#define __COREAUDIO_AUHAL_H__

//#include "portaudio.h"
#include "ringbuffer.h"
#include "../mplayer/IDirectSoundRenderer.h"
//#include "../mplayer/IAudioCallback.h"
//#include "../ssrc.h"
//#include "../../utils/PCMAmplifier.h"
extern "C" {
#include "ac3encoder.h"
}

#include "CoreAudioPlexSupport.h"
//#include <AudioUnit/AUComponent.h>

//extern void RegisterAudioCallback(IAudioCallback* pCallback);
//extern void UnRegisterAudioCallback();

class CoreAudioAUHAL : public IDirectSoundRenderer
	{
	public:
		virtual void UnRegisterAudioCallback();
		virtual void RegisterAudioCallback(IAudioCallback* pCallback);
		virtual DWORD GetChunkLen();
		virtual FLOAT GetDelay();
		CoreAudioAUHAL(IAudioCallback* pCallback, int iChannels, unsigned int uiSamplesPerSec, unsigned int uiBitsPerSample, bool bResample, const char* strAudioCodec = "", bool bIsMusic=false, bool bPassthrough = false);
		virtual ~CoreAudioAUHAL();
		
		virtual DWORD AddPackets(unsigned char* data, DWORD len);
		virtual DWORD GetSpace();
		virtual HRESULT Deinitialize();
		virtual HRESULT Pause();
		virtual HRESULT Stop();
		virtual HRESULT Resume();
		
		virtual LONG GetMinimumVolume() const;
		virtual LONG GetMaximumVolume() const;
		virtual LONG GetCurrentVolume() const;
		virtual void Mute(bool bMute);
		virtual HRESULT SetCurrentVolume(LONG nVolume);
		virtual int SetPlaySpeed(int iSpeed);
		virtual void WaitCompletion();
		virtual void DoWork();
		virtual void SwitchChannels(int iAudioStream, bool bAudioOnAllSpeakers);
		virtual void Flush();
		
		static AudioDeviceInfo* GetDeviceArray();
		
		bool IsValid() { return outputBuffer != 0; }
		
	private:
		virtual int CreateOutputStream(const CStdString& strName, int channels, int sampleRate, int bitsPerSample, bool isDigital, bool useCoreAudio, int packetSize);
		virtual int OpenAnalog(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, int sampleRate, int bitsPerSample, bool isDigital, bool useCoreAudio, int packetSize);
		static OSStatus RenderCallbackAnalog(struct CoreAudioDeviceParameters *deviceParameters,
															  int *ioActionFlags,
															  const AudioTimeStamp *inTimeStamp,
															  unsigned int inBusNummer,
															  unsigned int inNumberFrames,
											  AudioBufferList *ioData );

		
		OutRingBuffer* outputBuffer;
		
		//IAudioCallback* m_pCallback;
		
		LONG m_nCurrentVolume;
		DWORD m_dwPacketSize;
		DWORD m_dwNumPackets;
		bool m_bPause;
		bool m_bIsAllocated;
		bool m_bCanPause;
		
		unsigned int m_uiSamplesPerSec;
		unsigned int m_uiBitsPerSample;
		unsigned int m_uiChannels;
		
		bool m_bPassthrough;
		
		bool m_bEncodeAC3;
		AC3Encoder m_ac3encoder;
		
		AudioDeviceArray* deviceArray;
		struct CoreAudioDeviceParameters* deviceParameters;
	};

#endif 

