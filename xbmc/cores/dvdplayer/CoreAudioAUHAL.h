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

#include "pa_ringbuffer.h"
#include "../mplayer/IDirectSoundRenderer.h"
#include "../../utils/PCMAmplifier.h"
#include "ac3encoder.h"

#include "CoreAudioPlexSupport.h"

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
		virtual void SwitchChannels(int iAudioStream, bool bAudioOnAllSpeakers);
		virtual void Flush();
		
		static AudioDeviceInfo* GetDeviceArray();
		
		bool IsValid();
		
	private:
		virtual bool CreateOutputStream(const CStdString& strName, int channels, int sampleRate, int bitsPerSample, bool isDigital, bool useCoreAudio, int packetSize);
		virtual int OpenPCM(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, int sampleRate, int bitsPerSample, bool isDigital, bool useCoreAudio, int packetSize);
		static OSStatus RenderCallbackAnalog(struct CoreAudioDeviceParameters *deviceParameters,
															  int *ioActionFlags,
															  const AudioTimeStamp *inTimeStamp,
															  unsigned int inBusNummer,
															  unsigned int inNumberFrames,
											  AudioBufferList *ioData );
		virtual int OpenSPDIF(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, int sampleRate, int bitsPerSample, bool isDigital, bool useCoreAudio, int packetSize);
		virtual int AudioStreamChangeFormat(CoreAudioDeviceParameters *deviceParameters, AudioStreamID i_stream_id, AudioStreamBasicDescription change_format);
		static OSStatus RenderCallbackSPDIF(AudioDeviceID inDevice,
													 const AudioTimeStamp * inNow,
													 const void * inInputData,
													 const AudioTimeStamp * inInputTime,
													 AudioBufferList * outOutputData,
													 const AudioTimeStamp * inOutputTime,
											void * threadGlobals );
		static OSStatus StreamListener( AudioStreamID inStream,
									   UInt32 inChannel,
									   AudioDevicePropertyID inPropertyID,
									   void * inClientData );
		


		CPCMAmplifier 	m_amp;
		
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
		unsigned char* ac3_framebuffer;
		
		AudioDeviceArray* deviceArray;
		struct CoreAudioDeviceParameters* deviceParameters;
	};

#endif 

