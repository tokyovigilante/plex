/*
 *  CoreAudioAUHAL.cpp
 *  Plex
 *
 *  Created by Ryan Walklin on 9/6/08.
 *  Copyright 2008 Ryan Walklin. All rights reserved.
 *  Based on PortAudioDirectSound.cpp/ALSADirectSound.cpp from XBMC
 *  CoreAudio HAL interface code from VLC (www.videolan.org)
 *
 */

#ifdef __APPLE__
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

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnitProperties.h>
#include <AudioUnit/AudioUnitParameters.h>
#include <AudioUnit/AudioOutputUnit.h>
#include <AudioToolbox/AudioFormat.h>

#include "stdafx.h"
#include "CoreAudioAUHAL.h"
#include "AudioContext.h"
#include "Settings.h"


/**
 * High precision date or time interval
 *
 * Store a high precision date or time interval. The maximum precision is the
 * microsecond, and a 64 bits integer is used to avoid overflows (maximum
 * time interval is then 292271 years, which should be long enough for any
 * video). Dates are stored as microseconds since a common date (usually the
 * epoch). Note that date and time intervals can be manipulated using regular
 * arithmetic operators, and that no special functions are required.
 */
typedef int64_t mtime_t;
#define BUFSIZE 0xffffff
#define CA_BUFFER_FACTOR 0.05

struct CoreAudioDeviceParameters
{
    mtime_t                     clock_diff;     /* Difference between VLC clock and Device clock */
	
    /* AUHAL specific */
	AudioDeviceID				device_id; 
    Component                   au_component;   /* The Audiocomponent we use */
    AudioUnit                   au_unit;        /* The AudioUnit we use */
	OutRingBuffer*				outputBuffer;
    uint32_t                    i_read_bytes;
    uint32_t                    i_total_bytes;
	bool						b_digital;      /* Are we running in digital mode? */

	
    /* CoreAudio SPDIF mode specific */
    pid_t                       i_hog_pid;      /* The keep the pid of our hog status */
    AudioStreamID               i_stream_id;    /* The StreamID that has a cac3 streamformat */
    int                         i_stream_index; /* The index of i_stream_id in an AudioBufferList */
    AudioStreamBasicDescription stream_format;  /* The format we changed the stream to */
    AudioStreamBasicDescription sfmt_revert;    /* The original format of the stream */
    bool                  b_revert;       /* Wether we need to revert the stream format */
    bool                  b_changed_mixing;/* Wether we need to set the mixing mode back */
};

CoreAudioAUHAL::CoreAudioAUHAL(IAudioCallback* pCallback, int iChannels, unsigned int uiSamplesPerSec, unsigned int uiBitsPerSample, bool bResample, const char* strAudioCodec, bool bIsMusic, bool bPassthrough)
{
	CLog::Log(LOGDEBUG,"CoreAudioAUHAL::CoreAudioAUHAL - opening device");
	
	if (iChannels == 0)
		iChannels = 2;
	
	g_audioContext.SetActiveDevice(CAudioContext::DIRECTSOUND_DEVICE);
	
	m_bPause = false;
	m_bCanPause = false;
	m_bIsAllocated = false;
	ac3_framebuffer = NULL;
	
	m_dwPacketSize = iChannels*(uiBitsPerSample/8)*512;
	m_dwNumPackets = 16;
	
	if (g_guiSettings.GetInt("audiooutput.mode") == AUDIO_DIGITAL 
		&& g_guiSettings.GetBool("audiooutput.ac3passthrough")
		&& iChannels > 2
		&& !bPassthrough)
	{
		// Enable AC3 passthrough for digital devices
		int mpeg_remapping = 0;
		if (strAudioCodec == "AAC" || strAudioCodec == "DTS") mpeg_remapping = 1; // DTS uses MPEG channel mapping
		ac3encoder_init(&m_ac3encoder, iChannels, uiSamplesPerSec, uiBitsPerSample, mpeg_remapping);
		m_bEncodeAC3 = true;
		m_bPassthrough = true;
		ac3_framebuffer = (unsigned char *)calloc(m_dwPacketSize, 1);
	}
	else
	{
		m_bEncodeAC3 = false;
	}
	
	// set the stream parameters
	m_uiChannels = iChannels;
	m_uiSamplesPerSec = uiSamplesPerSec;
	m_uiBitsPerSample = uiBitsPerSample;
	m_bPassthrough = bPassthrough;
	
	m_nCurrentVolume = g_stSettings.m_nVolumeLevel;
	if (!m_bPassthrough)
		m_amp.SetVolume(m_nCurrentVolume);
	
	/* Open the device */
	CStdString device, deviceuse;
	device = g_guiSettings.GetString("audiooutput.audiodevice");
	
	CLog::Log(LOGINFO, "Asked to open device: [%s]\n", device.c_str());
	
	if (g_guiSettings.GetInt("audiooutput.mode") == AUDIO_DIGITAL && 
		g_guiSettings.GetBool("audiooutput.ac3passthrough") && 
		iChannels > 2 &&
		!m_bPassthrough)
	{
		// Use AC3 encoder
		CreateOutputStream(device,
						   SPDIF_CHANNELS, 
						   SPDIF_SAMPLERATE, 
						   SPDIF_SAMPLESIZE,
						   true,
						   g_guiSettings.GetInt("audiooutput.digitalaudiomode") == DIGITAL_COREAUDIO,
						   SPDIF_CHANNELS*(SPDIF_SAMPLESIZE/8)*512);
	}
	else
	{
		CreateOutputStream(device,
						   m_uiChannels, 
						   m_uiSamplesPerSec, 
						   m_uiBitsPerSample,
						   m_bPassthrough,
						   g_guiSettings.GetInt("audiooutput.digitalaudiomode") == DIGITAL_COREAUDIO,
						   m_dwPacketSize);
	}
    
	m_bCanPause = false;
	m_bIsAllocated = true;
	
	
}

//***********************************************************************************************
CoreAudioAUHAL::~CoreAudioAUHAL()
{
	CLog::Log(LOGDEBUG,"CoreAudioAUHAL() dtor");
	Deinitialize();
}

bool CoreAudioAUHAL::IsValid()
{
	return (deviceParameters->outputBuffer != NULL && m_bIsAllocated); 
}


//***********************************************************************************************
HRESULT CoreAudioAUHAL::Deinitialize()
{
	CLog::Log(LOGDEBUG,"CoreAudioAUHAL::Deinitialize");
	
#warning free structs
	OSStatus            err = noErr;
    UInt32              i_param_size = 0;
	
    if(deviceParameters->au_unit)
    {
        verify_noerr( AudioOutputUnitStop( deviceParameters->au_unit ) );
        verify_noerr( AudioUnitUninitialize( deviceParameters->au_unit ) );
        verify_noerr( CloseComponent( deviceParameters->au_unit ) );
		deviceParameters->au_unit = NULL;
    }
#warning fix digital
#if 0

    if( p_sys->b_digital )
    {
        /* Stop device */
        err = AudioDeviceStop( p_sys->i_selected_dev,
							  (AudioDeviceIOProc)RenderCallbackSPDIF );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceStop failed: [%4.4s]", (char *)&err );
        }
		
        /* Remove IOProc callback */
        err = AudioDeviceRemoveIOProc( p_sys->i_selected_dev,
                                      (AudioDeviceIOProc)RenderCallbackSPDIF );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceRemoveIOProc failed: [%4.4s]", (char *)&err );
        }
		
        if( p_sys->b_revert )
        {
            AudioStreamChangeFormat( p_aout, p_sys->i_stream_id, p_sys->sfmt_revert );
        }
		
        if( p_sys->b_changed_mixing && p_sys->sfmt_revert.mFormatID != kAudioFormat60958AC3 )
        {
            int b_mix;
            Boolean b_writeable;
            /* Revert mixable to true if we are allowed to */
            err = AudioDeviceGetPropertyInfo( p_sys->i_selected_dev, 0, FALSE, kAudioDevicePropertySupportsMixing,
											 &i_param_size, &b_writeable );
			
            err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE, kAudioDevicePropertySupportsMixing,
										 &i_param_size, &b_mix );
			
            if( !err && b_writeable )
            {
                msg_Dbg( p_aout, "mixable is: %d", b_mix );
                b_mix = 1;
                err = AudioDeviceSetProperty( p_sys->i_selected_dev, 0, 0, FALSE,
											 kAudioDevicePropertySupportsMixing, i_param_size, &b_mix );
            }
			
            if( err != noErr )
            {
                msg_Err( p_aout, "failed to set mixmode: [%4.4s]", (char *)&err );
            }
        }
    }
#endif
#warning fix listener
    //err = AudioHardwareRemovePropertyListener( kAudioHardwarePropertyDevices,
	//										  HardwareListener );
	
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "AudioHardwareRemovePropertyListener failed: [%4.4s]", (char *)&err );
    }
	
    if( deviceParameters->i_hog_pid == getpid() )
    {
        deviceParameters->i_hog_pid = -1;
        i_param_size = sizeof( deviceParameters->i_hog_pid );
        err = AudioDeviceSetProperty( deviceParameters->device_id, 0, 0, FALSE,
									 kAudioDevicePropertyHogMode, i_param_size, &deviceParameters->i_hog_pid );
        if( err != noErr ) CLog::Log(LOGERROR, "Could not release hogmode: [%4.4s]", (char *)&err );
    }
	
    //free( p_sys );

	if (m_bEncodeAC3)
	{
		ac3encoder_free(&m_ac3encoder);
	}
	if (ac3_framebuffer != NULL)
	{
		free(ac3_framebuffer);
		ac3_framebuffer = NULL;
	}
#warning free device array
	
	m_bIsAllocated = false;
	
 	CLog::Log(LOGDEBUG,"CoreAudioAUHAL::Deinitialize - set active");
	g_audioContext.SetActiveDevice(CAudioContext::DEFAULT_DEVICE);
	
	return S_OK;
}

//***********************************************************************************************
void CoreAudioAUHAL::Flush() 
{
	CLog::Log(LOGDEBUG, "Flushing %i bytes from buffer", (rb_data_size(deviceParameters->outputBuffer) - rb_free(deviceParameters->outputBuffer)));
	rb_clear(deviceParameters->outputBuffer);
}

//***********************************************************************************************
HRESULT CoreAudioAUHAL::Pause()
{
	if (m_bPause) 
		return S_OK;
	
	m_bPause = true;
	Flush();
	
	return S_OK;
}

//***********************************************************************************************
HRESULT CoreAudioAUHAL::Resume()
{
	// If we are not pause, stream might not be prepared to start flush will do this for us
	if (!m_bPause)
		Flush();
	
	m_bPause = false;
	return S_OK;
}

//***********************************************************************************************
HRESULT CoreAudioAUHAL::Stop()
{
#warning Stop stream
	return S_OK;
}

//***********************************************************************************************
LONG CoreAudioAUHAL::GetMinimumVolume() const
{
	return -60;
}

//***********************************************************************************************
LONG CoreAudioAUHAL::GetMaximumVolume() const
{
	return 60;
}

//***********************************************************************************************
LONG CoreAudioAUHAL::GetCurrentVolume() const
{
	return m_nCurrentVolume;
}

//***********************************************************************************************
void CoreAudioAUHAL::Mute(bool bMute)
{
	if (!m_bIsAllocated) return;
	
	if (bMute)
		SetCurrentVolume(GetMinimumVolume());
	else
		SetCurrentVolume(m_nCurrentVolume);
	
}

//***********************************************************************************************
HRESULT CoreAudioAUHAL::SetCurrentVolume(LONG nVolume)
{
	if (!m_bIsAllocated || m_bPassthrough) return -1;
	m_nCurrentVolume = nVolume;
	m_amp.SetVolume(nVolume);
	return S_OK;
}


//***********************************************************************************************
DWORD CoreAudioAUHAL::GetSpace()
{
	DWORD fakeCeiling, bufferDataSize = rb_data_size(deviceParameters->outputBuffer);
	if (m_bEncodeAC3)
	{
		// limit buffer size to 3 AC3 frames (320 ms)
		bufferDataSize /= (ac3encoder_channelcount(&m_ac3encoder) * m_uiBitsPerSample/8);
		fakeCeiling = AC3_SAMPLES_PER_FRAME * 3;
	}
	else
	{
		// limit buffer size to CA_BUFFER_FACTOR seconds 
		bufferDataSize /= (m_uiChannels * m_uiBitsPerSample/8);
		fakeCeiling = m_uiSamplesPerSec * CA_BUFFER_FACTOR;
	}
	if (bufferDataSize < fakeCeiling)
	{
		return fakeCeiling - bufferDataSize;
	}
	else
	{
		return 0;
	}	
}

//***********************************************************************************************
DWORD CoreAudioAUHAL::AddPackets(unsigned char *data, DWORD len)
{
	// sanity
	if (!deviceParameters && !deviceParameters->outputBuffer)
	{
		CLog::Log(LOGERROR, "No audio output device");
		return -1;
	}
	
	if (len == 0) return len;
	
	int samplesPassedIn, inputByteFactor, outputByteFactor;
	unsigned char* pcmPtr = data;
	
	if (m_bEncodeAC3) // use the raw PCM channel count to get the number of samples to play
	{
		inputByteFactor = ac3encoder_channelcount(&m_ac3encoder) * m_uiBitsPerSample/8;
		outputByteFactor = SPDIF_CHANNELS * m_uiBitsPerSample/8;
	}
	else // the PCM input and stream output should match
	{
		inputByteFactor = m_uiChannels * m_uiBitsPerSample/8;
		outputByteFactor = inputByteFactor;
	}
	samplesPassedIn = len / inputByteFactor;
	
	// Find out how much space we have available and clip to the amount we got passed in. 
	DWORD samplesToWrite  = GetSpace();
	if (samplesToWrite > samplesPassedIn)
	{
		samplesToWrite = samplesPassedIn;
	}
	
	if (m_bEncodeAC3)
	{	  
		int ac3_frame_count = 0;
		if ((ac3_frame_count = ac3encoder_write_samples(&m_ac3encoder, pcmPtr, samplesToWrite)) == 0)
		{
			CLog::Log(LOGERROR, "AC3 output buffer underrun");
			return 0;
		}
		else
		{
			int buffer_sample_readcount = -1;
			if ((buffer_sample_readcount = ac3encoder_get_encoded_samples(&m_ac3encoder, ac3_framebuffer, samplesToWrite)) != samplesToWrite)
			{
				CLog::Log(LOGERROR, "AC3 output buffer underrun");
			}
			else
			{
				rb_write(deviceParameters->outputBuffer, ac3_framebuffer, samplesToWrite * outputByteFactor);
			}
		}
	}
	else
	{
		// Handle volume de-amplification.
		if (!m_bPassthrough)
			m_amp.DeAmplify((short *)pcmPtr, samplesToWrite);
		
		// Write data to the stream.
		rb_write(deviceParameters->outputBuffer, pcmPtr, samplesToWrite * outputByteFactor);
	}
	return samplesToWrite * inputByteFactor;

}

//***********************************************************************************************
FLOAT CoreAudioAUHAL::GetDelay()
{
	// For now hardwire to about +15ms from "base", which is what we're observing.
#warning set programmatically
	FLOAT delay = 1000 * CA_BUFFER_FACTOR;
	
	if (m_bEncodeAC3)
		delay += 0.072; // 3072/48000 = 0.064 (two AC3 packets plus 8ms)
	else
		delay += 0.008;
	
	return delay;
}

//***********************************************************************************************
DWORD CoreAudioAUHAL::GetChunkLen()
{
	return m_dwPacketSize;
}
//***********************************************************************************************
int CoreAudioAUHAL::SetPlaySpeed(int iSpeed)
{
	return 0;
}

void CoreAudioAUHAL::RegisterAudioCallback(IAudioCallback *pCallback)
{

}

void CoreAudioAUHAL::UnRegisterAudioCallback()
{

}

void CoreAudioAUHAL::WaitCompletion()
{
	
}

void CoreAudioAUHAL::SwitchChannels(int iAudioStream, bool bAudioOnAllSpeakers)
{
    return ;
}

/*****************************************************************************
 * Open: open macosx audio output
 *****************************************************************************/
bool CoreAudioAUHAL::CreateOutputStream(const CStdString& strName, int channels, int sampleRate, int bitsPerSample, bool isDigital, bool useCoreAudio, int packetSize)
{
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0;
	int                     b_alive = false; 
	
    /* Allocate structure */
    deviceParameters = (CoreAudioDeviceParameters*)calloc(sizeof(CoreAudioDeviceParameters), 1);
    if (!deviceParameters) return false;
	
	deviceParameters->b_digital = isDigital;
    deviceParameters->i_hog_pid = -1;
    deviceParameters->i_stream_index = -1;
	
    /* Build a list of devices */
    if (deviceArray); // free device array
	deviceArray = CoreAudioPlexSupport::GetDeviceArray();
	if (!deviceArray) return false;
	
	// Pick the default device if one's not pre-selected
	deviceArray->selectedDevice = -1;
	for (int i=0; i < deviceArray->deviceCount; i++)
	{
		if (g_guiSettings.GetString("audiooutput.audiodevice").Equals(deviceArray->device[i]->deviceName))
		{
			deviceArray->selectedDevice = deviceArray->device[i]->deviceID;
			deviceArray->selectedDeviceIndex = i;
		}
	}
	if (deviceArray->selectedDevice == -1)
	{
		deviceArray->selectedDevice = deviceArray->defaultDevice;
	}
    	
    /* Check if the desired device is alive and usable */
    i_param_size = sizeof( b_alive );
    err = AudioDeviceGetProperty(deviceArray->selectedDevice, 0, FALSE,
								 kAudioDevicePropertyDeviceIsAlive,
								 &i_param_size, &b_alive );
	
    if( err != noErr )
    {
        /* Be tolerant, only give a warning here */
		CLog::Log(LOGINFO, "could not check whether device [0x%x] is alive: %4.4s", 
				  (unsigned int)deviceArray->selectedDevice, 
				  (char *)&err );
        b_alive = false;
    }
	
    if( b_alive == false )
    {
		CLog::Log(LOGWARNING, "selected audio device is not alive, switching to default device");
        deviceArray->selectedDevice = deviceArray->defaultDevice;
    }
	
    i_param_size = sizeof(deviceParameters->i_hog_pid);
    err = AudioDeviceGetProperty( deviceArray->selectedDevice, 0, FALSE,
								 kAudioDevicePropertyHogMode,
								 &i_param_size, &deviceParameters->i_hog_pid );
	
    if( err != noErr )
    {
        /* This is not a fatal error. Some drivers simply don't support this property */
		CLog::Log(LOGINFO, "could not check whether device is hogged: %4.4s",
                 (char *)&err );
        deviceParameters->i_hog_pid = -1;
    }
	
    if( deviceParameters->i_hog_pid != -1 && deviceParameters->i_hog_pid != getpid() )
    {
		CLog::Log(LOGERROR, "Selected audio device is exclusively in use by another program.");
		return false;
    }
	
	deviceParameters->device_id = deviceArray->selectedDevice;
	
    /* Check for Digital mode or Analog output mode */
	if (isDigital && 
		deviceArray->device[deviceArray->selectedDeviceIndex]->supportsDigital)
    {
        if (OpenSPDIF(deviceParameters, strName, channels, sampleRate, bitsPerSample, isDigital, useCoreAudio, packetSize)) 
			return true;
    }
    else
    {
        if (OpenPCM(deviceParameters, strName, channels, sampleRate, bitsPerSample, isDigital, useCoreAudio, packetSize))
            return true;
    }
	
error:
    /* If we reach this, this aout has failed */
    //var_Destroy( p_aout, "audio-device" );
    free(deviceParameters);
    //return VLC_EGENERIC;
		return -1;
}

/*****************************************************************************
 * Open: open and setup a HAL AudioUnit to do analog (multichannel) audio output
 *****************************************************************************/
int CoreAudioAUHAL::OpenPCM(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, int sampleRate, int bitsPerSample, bool isDigital, bool useCoreAudio, int packetSize)
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0, i = 0;
    int                         i_original;
    ComponentDescription        desc;
    AudioStreamBasicDescription DeviceFormat;
    AudioChannelLayout          *layout;
    AudioChannelLayout          new_layout;
    AURenderCallbackStruct      input;
	
    /* Lets go find our Component */
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
	
    deviceParameters->au_component = FindNextComponent( NULL, &desc );
    if(deviceParameters->au_component == NULL)
    {
		CLog::Log(LOGERROR, "we cannot find our HAL component");
        return false;
    }
	
    err = OpenAComponent( deviceParameters->au_component, &deviceParameters->au_unit );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "we cannot open our HAL component");
        return false;
    }
	
    /* Set the device we will use for this output unit */
    err = AudioUnitSetProperty(deviceParameters->au_unit,
							   kAudioOutputUnitProperty_CurrentDevice,
							   kAudioUnitScope_Input,
							   0,
							   &deviceArray->selectedDevice,
							   sizeof(AudioDeviceID));
	
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "we cannot select the audio device");
        return false;
    }
	
    /* Get the current format */
    i_param_size = sizeof(AudioStreamBasicDescription);
	
    err = AudioUnitGetProperty(deviceParameters->au_unit,
							   kAudioUnitProperty_StreamFormat,
							   kAudioUnitScope_Input,
							   0,
							   &deviceParameters->sfmt_revert,
							   &i_param_size );
	
    if( err != noErr ) return false;
    else CLog::Log(LOGINFO, STREAM_FORMAT_MSG("current format is: ", deviceParameters->sfmt_revert) );
#if 0
    /* Get the channel layout of the device side of the unit (vlc -> unit -> device) */
    err = AudioUnitGetPropertyInfo( deviceParameters->au_unit,
                                   kAudioDevicePropertyPreferredChannelLayout,
                                   kAudioUnitScope_Output,
                                   0,
                                   &i_param_size,
                                   NULL );
	
    if( err == noErr )
    {
        layout = (AudioChannelLayout *)malloc( i_param_size);
		
        verify_noerr( AudioUnitGetProperty( deviceParameters->au_unit,
										   kAudioDevicePropertyPreferredChannelLayout,
										   kAudioUnitScope_Output,
										   0,
										   layout,
										   &i_param_size ));
		
        /* We need to "fill out" the ChannelLayout, because there are multiple ways that it can be set */
        if( layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap)
        {
            /* bitmap defined channellayout */
            verify_noerr( AudioFormatGetProperty( kAudioFormatProperty_ChannelLayoutForBitmap,
												 sizeof( UInt32), &layout->mChannelBitmap,
												 &i_param_size,
												 layout ));
        }
        else if( layout->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions )
        {
            /* layouttags defined channellayout */
            verify_noerr( AudioFormatGetProperty( kAudioFormatProperty_ChannelLayoutForTag,
												 sizeof( AudioChannelLayoutTag ), &layout->mChannelLayoutTag,
												 &i_param_size,
												 layout ));
        }
		
		CLog::Log(LOGINFO, "layout of AUHAL has %d channels" , (int)layout->mNumberChannelDescriptions );
#endif
        /* Initialize the VLC core channel count */
        //deviceParameters->output.output.i_physical_channels = 0;
        //i_original = p_aout->output.output.i_original_channels & AOUT_CHAN_PHYSMASK;
		
        //if( i_original == AOUT_CHAN_CENTER || layout->mNumberChannelDescriptions < 2 )
        {
            /* We only need Mono or cannot output more than 1 channel */
          //  p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
        }
        //else if( i_original == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) || layout->mNumberChannelDescriptions < 3 )
        {
            /* We only need Stereo or cannot output more than 2 channels */
          //  p_aout->output.output.i_physical_channels = AOUT_CHAN_RIGHT | AOUT_CHAN_LEFT;
        }
        //else
        {
            /* We want more than stereo and we can do that */
            /*for( i = 0; i < layout->mNumberChannelDescriptions; i++ )
            {
                msg_Dbg( p_aout, "this is channel: %d", (int)layout->mChannelDescriptions[i].mChannelLabel );
				
                switch( layout->mChannelDescriptions[i].mChannelLabel )
                {
                    case kAudioChannelLabel_Left:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_LEFT;
                        continue;
                    case kAudioChannelLabel_Right:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_RIGHT;
                        continue;
                    case kAudioChannelLabel_Center:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_CENTER;
                        continue;
                    case kAudioChannelLabel_LFEScreen:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_LFE;
                        continue;
                    case kAudioChannelLabel_LeftSurround:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_REARLEFT;
                        continue;
                    case kAudioChannelLabel_RightSurround:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_REARRIGHT;
                        continue;
                    case kAudioChannelLabel_RearSurroundLeft:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_MIDDLELEFT;
                        continue;
                    case kAudioChannelLabel_RearSurroundRight:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_MIDDLERIGHT;
                        continue;
                    case kAudioChannelLabel_CenterSurround:
                        p_aout->output.output.i_physical_channels |= AOUT_CHAN_REARCENTER;
                        continue;
                    default:
                        msg_Warn( p_aout, "unrecognized channel form provided by driver: %d", (int)layout->mChannelDescriptions[i].mChannelLabel );
                }
            }
            if( p_aout->output.output.i_physical_channels == 0 )
            {
                p_aout->output.output.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
                msg_Err( p_aout, "You should configure your speaker layout with Audio Midi Setup Utility in /Applications/Utilities. Now using Stereo mode." );
                intf_UserFatal( p_aout, false, _("Audio device is not configured"),
							   _("You should configure your speaker layout with "
								 "the \"Audio Midi Setup\" utility in /Applications/"
								 "Utilities. Stereo mode is being used now.") );
            }
        }
        free( layout );
    }
    else
    {
        msg_Warn( p_aout, "this driver does not support kAudioDevicePropertyPreferredChannelLayout. BAD DRIVER AUTHOR !!!" );
        p_aout->output.output.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }*/
		}
    //msg_Dbg( p_aout, "selected %d physical channels for device output", aout_FormatNbChannels( &p_aout->output.output ) );
    //msg_Dbg( p_aout, "VLC will output: %s", aout_FormatPrintChannels( &p_aout->output.output ));
#if 0
    memset (&new_layout, 0, sizeof(new_layout));
    switch(channels)
    {
        case 1:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
            break;
        case 2:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
            break;
			/*
        case 3:
            if( p_aout->output.output.i_physical_channels & AOUT_CHAN_CENTER )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_7; // L R C
            }
            else if( p_aout->output.output.i_physical_channels & AOUT_CHAN_LFE )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_4; // L R LFE
            }
            break;
        case 4:
            if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_CENTER | AOUT_CHAN_LFE ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_10; // L R C LFE
            }
            else if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_3; // L R Ls Rs
            }
            else if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_CENTER | AOUT_CHAN_REARCENTER ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_3; // L R C Cs
            }
            break;
        case 5:
            if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_CENTER ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_19; // L R Ls Rs C
            }
            else if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_LFE ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_18; // L R Ls Rs LFE
            }
            break;
        case 6:
            if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_LFE ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_20; // L R Ls Rs C LFE
            }
            else
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_AudioUnit_6_0; // L R Ls Rs C Cs
            }
            break;*/
            }
#endif
    /* Set up the format to be used */
	DeviceFormat.mSampleRate = sampleRate;
    DeviceFormat.mFormatID = kAudioFormatLinearPCM;
	
    DeviceFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    DeviceFormat.mBitsPerChannel = bitsPerSample;
	DeviceFormat.mChannelsPerFrame = channels;
	
    /* Calculate framesizes and stuff */
    DeviceFormat.mFramesPerPacket = 1;
    DeviceFormat.mBytesPerFrame = DeviceFormat.mBitsPerChannel * DeviceFormat.mChannelsPerFrame / 8;
    DeviceFormat.mBytesPerPacket = DeviceFormat.mBytesPerFrame * DeviceFormat.mFramesPerPacket;
	
    /* Set the desired format */
    i_param_size = sizeof(AudioStreamBasicDescription);
    verify_noerr( AudioUnitSetProperty(deviceParameters->au_unit,
									   kAudioUnitProperty_StreamFormat,
									   kAudioUnitScope_Input,
									   0,
									   &DeviceFormat,
									   i_param_size ));
	
	CLog::Log(LOGINFO, STREAM_FORMAT_MSG( "we set the AU format: " , DeviceFormat ) );
	
    /* Retrieve actual format */
    verify_noerr( AudioUnitGetProperty(deviceParameters->au_unit,
									   kAudioUnitProperty_StreamFormat,
									   kAudioUnitScope_Input,
									   0,
									   &deviceParameters->stream_format,
									   &i_param_size ));
	
	CLog::Log(LOGINFO, STREAM_FORMAT_MSG( "the actual set AU format is " , DeviceFormat ) );

    /* set the IOproc callback */
	input.inputProc = (AURenderCallback) RenderCallbackAnalog;
	input.inputProcRefCon = deviceParameters;
	
    verify_noerr( AudioUnitSetProperty(deviceParameters->au_unit,
									   kAudioUnitProperty_SetRenderCallback,
									   kAudioUnitScope_Global,
									   0, &input, sizeof(input)));
	
#if 0
    /* Set the new_layout as the layout VLC will use to feed the AU unit */
    verify_noerr( AudioUnitSetProperty(deviceParameters->au_unit,
									   kAudioUnitProperty_AudioChannelLayout,
									   kAudioUnitScope_Input,
									   0, &new_layout, sizeof(new_layout) ) );
    if( new_layout.mNumberChannelDescriptions > 0 )
        free( new_layout.mChannelDescriptions );
#endif
    /* AU initiliaze */
    verify_noerr( AudioUnitInitialize(deviceParameters->au_unit) );
	
    /* Find the difference between device clock and mdate clock */
    deviceParameters->clock_diff = - (mtime_t)
	AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000;
    //deviceParameters->clock_diff += mdate();
	
	// initialise the CoreAudio sink buffer
	rb_init(&deviceParameters->outputBuffer, (sampleRate/2) * channels * (bitsPerSample/8)); // buffer 0.5 sec (use 0.1)
	
	
    /* Start the AU */
    verify_noerr( AudioOutputUnitStart(deviceParameters->au_unit) );
	
    return true;
}

/*****************************************************************************
 * RenderCallbackAnalog: This function is called everytime the AudioUnit wants
 * us to provide some more audio data.
 * Don't print anything during normal playback, calling blocking function from
 * this callback is not allowed.
 *****************************************************************************/
OSStatus CoreAudioAUHAL::RenderCallbackAnalog(struct CoreAudioDeviceParameters *deviceParameters,
									  int *ioActionFlags,
									  const AudioTimeStamp *inTimeStamp,
									  unsigned int inBusNumber,
									  unsigned int inNumberFrames,
									  AudioBufferList *ioData )
{
    AudioTimeStamp  host_time;
    mtime_t         current_date = 0;
    uint32_t        i_mData_bytes = 0;
	
	
	
    //host_time.mFlags = kAudioTimeStampHostTimeValid;
    //AudioDeviceTranslateTime( deviceParameters->device_id, inTimeStamp, &host_time );
	
    /* Check for the difference between the Device clock and mdate */
    //p_sys->clock_diff = - (mtime_t)
	//AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000;
    //p_sys->clock_diff += mdate();
	
    //current_date = deviceParameters->clock_diff +
	//AudioConvertHostTimeToNanos( host_time.mHostTime ) / 1000;
	//- ((mtime_t) 1000000 / p_aout->output.output.i_rate * 31 ); // 31 = Latency in Frames. retrieve somewhere
	
    // initial calc
	int framesToWrite = inNumberFrames;
	int framesAvailable = rb_data_size(deviceParameters->outputBuffer);
	int i, currentPos = 0, underrunLength = 0;
	
	framesAvailable /= deviceParameters->stream_format.mBytesPerFrame;
	
	if (framesToWrite > framesAvailable)
	{
		framesToWrite = framesAvailable;
	}
	
	currentPos = framesToWrite * deviceParameters->stream_format.mBytesPerFrame;
	underrunLength = currentPos + ((inNumberFrames - framesToWrite) * deviceParameters->stream_format.mBytesPerFrame);
	
	// write as many frames as possible from buffer
	rb_read(deviceParameters->outputBuffer, 
			(uint8_t *)ioData->mBuffers[0].mData,  
			framesToWrite * deviceParameters->stream_format.mBytesPerFrame);
	
	// write silence to any remainder
	for (i=currentPos; i < underrunLength; i+=sizeof(int16_t))
	{
		*((int16_t *)(ioData->mBuffers[0].mData)+i) = INT16_MIN;	
	}
	
    return( noErr );
}
		
/*****************************************************************************
 * Setup a encoded digital stream (SPDIF)
 *****************************************************************************/
int CoreAudioAUHAL::OpenSPDIF(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, int sampleRate, int bitsPerSample, bool isDigital, bool useCoreAudio, int packetSize)

{
	OSStatus                err = noErr;
    UInt32                  i_param_size = 0, b_mix = 0;
    Boolean                 b_writeable = false;
    AudioStreamID           *p_streams = NULL;
    int                     i = 0, i_streams = 0;
	
    /* Start doing the SPDIF setup proces */
    deviceParameters->b_digital = true;
	
    /* Hog the device */
    i_param_size = sizeof(deviceParameters->i_hog_pid);
    deviceParameters->i_hog_pid = getpid();
	
    err = AudioDeviceSetProperty(deviceParameters->device_id, 0, 0, FALSE,
								 kAudioDevicePropertyHogMode, i_param_size, &deviceParameters->i_hog_pid);
	
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Failed to set hogmode: [%4.4s]", (char *)&err );
        return false;
    }
	
    /* Set mixable to false if we are allowed to */
    err = AudioDeviceGetPropertyInfo(deviceParameters->device_id, 0, FALSE, kAudioDevicePropertySupportsMixing,
									 &i_param_size, &b_writeable );
	
    err = AudioDeviceGetProperty(deviceParameters->device_id, 0, FALSE, kAudioDevicePropertySupportsMixing,
								 &i_param_size, &b_mix );
	
    if( !err && b_writeable )
    {
        b_mix = 0;
        err = AudioDeviceSetProperty( deviceParameters->device_id, 0, 0, FALSE,
									 kAudioDevicePropertySupportsMixing, i_param_size, &b_mix );
        deviceParameters->b_changed_mixing = true;
    }
	
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Failed to set mixmode: [%4.4s]", (char *)&err );
        return false;
    }
	
    /* Get a list of all the streams on this device */
    err = AudioDeviceGetPropertyInfo(deviceParameters->device_id, 0, FALSE,
									 kAudioDevicePropertyStreams,
									 &i_param_size, NULL );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Could not get number of streams: [%4.4s]", (char *)&err );
        return false;
    }
	
    i_streams = i_param_size / sizeof( AudioStreamID );
    p_streams = (AudioStreamID *)malloc( i_param_size );
    if( p_streams == NULL )
        return false;
	
    err = AudioDeviceGetProperty(deviceParameters->device_id, 0, FALSE,
								 kAudioDevicePropertyStreams,
								 &i_param_size, p_streams );
	
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Could not get number of streams: [%4.4s]", (char *)&err );
        free( p_streams );
        return false;
    }
	
    for( i = 0; i < i_streams && deviceParameters->i_stream_index < 0 ; i++ )
    {
        /* Find a stream with a cac3 stream */
        AudioStreamBasicDescription *p_format_list = NULL;
        int                         i_formats = 0, j = 0;
        bool                  b_digital = false;
		
        /* Retrieve all the stream formats supported by each output stream */
        err = AudioStreamGetPropertyInfo( p_streams[i], 0,
										 kAudioStreamPropertyPhysicalFormats,
										 &i_param_size, NULL );
        if( err != noErr )
        {
			CLog::Log(LOGERROR, "Could not get number of streamformats: [%4.4s]", (char *)&err );
            continue;
        }
		
        i_formats = i_param_size / sizeof( AudioStreamBasicDescription );
        p_format_list = (AudioStreamBasicDescription *)malloc( i_param_size );
        if( p_format_list == NULL )
            continue;
		
        err = AudioStreamGetProperty( p_streams[i], 0,
									 kAudioStreamPropertyPhysicalFormats,
									 &i_param_size, p_format_list );
        if( err != noErr )
        {
			CLog::Log(LOGERROR, "Could not get the list of streamformats: [%4.4s]", (char *)&err );
            free( p_format_list );
            continue;
        }
		
        /* Check if one of the supported formats is a digital format */
        for( j = 0; j < i_formats; j++ )
        {
            if( p_format_list[j].mFormatID == 'IAC3' ||
			   p_format_list[j].mFormatID == kAudioFormat60958AC3 )
            {
                b_digital = true;
                break;
            }
        }
		
        if( b_digital )
        {
            /* if this stream supports a digital (cac3) format, then go set it. */
            int i_requested_rate_format = -1;
            int i_current_rate_format = -1;
            int i_backup_rate_format = -1;
			
            deviceParameters->i_stream_id = p_streams[i];
            deviceParameters->i_stream_index = i;
			
            if(deviceParameters->b_revert == false )
            {
                /* Retrieve the original format of this stream first if not done so already */
                i_param_size = sizeof(deviceParameters->sfmt_revert );
                err = AudioStreamGetProperty(deviceParameters->device_id, 0,
											 kAudioStreamPropertyPhysicalFormat,
											 &i_param_size,
											 &deviceParameters->sfmt_revert );
                if( err != noErr )
                {
					CLog::Log(LOGERROR, "Could not retrieve the original streamformat: [%4.4s]", (char *)&err );
                    //continue;
                }
                else deviceParameters->b_revert = true;
            }
			
            for( j = 0; j < i_formats; j++ )
            {
                if( p_format_list[j].mFormatID == 'IAC3' ||
				   p_format_list[j].mFormatID == kAudioFormat60958AC3 )
                {
                    if( p_format_list[j].mSampleRate == sampleRate)
                    {
                        i_requested_rate_format = j;
                        break;
                    }
                    else if( p_format_list[j].mSampleRate == deviceParameters->sfmt_revert.mSampleRate )
                    {
                        i_current_rate_format = j;
                    }
                    else
                    {
                        if( i_backup_rate_format < 0 || p_format_list[j].mSampleRate > p_format_list[i_backup_rate_format].mSampleRate )
                            i_backup_rate_format = j;
                    }
                }
				
            }
			
            if( i_requested_rate_format >= 0 ) /* We prefer to output at the samplerate of the original audio */
                deviceParameters->stream_format = p_format_list[i_requested_rate_format];
            else if( i_current_rate_format >= 0 ) /* If not possible, we will try to use the current samplerate of the device */
                deviceParameters->stream_format = p_format_list[i_current_rate_format];
            else deviceParameters->stream_format = p_format_list[i_backup_rate_format]; /* And if we have to, any digital format will be just fine (highest rate possible) */
        }
        free( p_format_list );
    }
    free( p_streams );
	
	CLog::Log(LOGINFO, STREAM_FORMAT_MSG("original stream format: ", deviceParameters->sfmt_revert ) );
	
    if( !AudioStreamChangeFormat(deviceParameters, deviceParameters->i_stream_id, deviceParameters->stream_format))
        return false;
	
    /* Set the format flags */
    //p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
    //p_aout->output.output.i_frame_length = A52_FRAME_NB;
    //p_aout->output.i_nb_samples = p_aout->output.output.i_frame_length;
    //p_aout->output.output.i_rate = (unsigned int)p_sys->stream_format.mSampleRate;
    //aout_FormatPrepare( &p_aout->output.output );
    //aout_VolumeNoneInit( p_aout );
	
	// initialise the CoreAudio sink buffer
	rb_init(&deviceParameters->outputBuffer, AC3_SPDIF_FRAME_SIZE * 15); // buffer 15 frames
	
	
    /* Add IOProc callback */
	AudioDeviceIOProcID sInputIOProcID;
    err = AudioDeviceCreateIOProcID(deviceParameters->device_id,
									(AudioDeviceIOProc)RenderCallbackSPDIF,
									deviceParameters,
									&sInputIOProcID);	
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "AudioDeviceAddIOProcID failed: [%4.4s]", (char *)&err );
        return false;
    }
	
    /* Check for the difference between the Device clock and mdate */
    deviceParameters->clock_diff = - (mtime_t)
	AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000;
    //deviceParameters->clock_diff += mdate();
	
    /* Start device */
    err = AudioDeviceStart(deviceParameters->device_id, (AudioDeviceIOProc)RenderCallbackSPDIF );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "AudioDeviceStart failed: [%4.4s]", (char *)&err );
		
        err = AudioDeviceDestroyIOProcID(deviceParameters->device_id,
										 (AudioDeviceIOProc)RenderCallbackSPDIF);
        if( err != noErr )
        {
			CLog::Log(LOGERROR, "AudioDeviceRemoveIOProc failed: [%4.4s]", (char *)&err );
        }
        return false;
    }
	
    return true;
}

/*****************************************************************************
 * AudioStreamChangeFormat: Change i_stream_id to change_format
 *****************************************************************************/
int CoreAudioAUHAL::AudioStreamChangeFormat(CoreAudioDeviceParameters *deviceParameters, AudioStreamID i_stream_id, AudioStreamBasicDescription change_format)
{
    OSStatus            err = noErr;
    UInt32              i_param_size = 0;
    int i;
	
    CLog::Log(LOGINFO, STREAM_FORMAT_MSG( "setting stream format: ", change_format ));
	
    /* change the format */
    err = AudioStreamSetProperty( i_stream_id, 0, 0,
								 kAudioStreamPropertyPhysicalFormat,
								 sizeof( AudioStreamBasicDescription ),
								 &change_format );
    if( err != noErr )
    {
        CLog::Log(LOGERROR, "could not set the stream format: [%4.4s]", (char *)&err );
        return false;
    }
	
    /* The AudioStreamSetProperty is not only asynchronious (requiring the locks)
     * it is also not atomic in its behaviour.
     * Therefore we check 5 times before we really give up.*/
    for( i = 0; i < 5; i++ )
    {
        AudioStreamBasicDescription actual_format;
		usleep(20);	
        i_param_size = sizeof( AudioStreamBasicDescription );
        err = AudioStreamGetProperty( i_stream_id, 0,
									 kAudioStreamPropertyPhysicalFormat,
									 &i_param_size,
									 &actual_format );
		
        CLog::Log(LOGDEBUG, STREAM_FORMAT_MSG( "actual format in use: ", actual_format ) );
        if( actual_format.mSampleRate == change_format.mSampleRate &&
		   actual_format.mFormatID == change_format.mFormatID &&
		   actual_format.mFramesPerPacket == change_format.mFramesPerPacket )
        {
            /* The right format is now active */
            break;
        }
        /* We need to check again */
    }
	
    return true;
}

/*****************************************************************************
 * RenderCallbackSPDIF: callback for SPDIF audio output
 *****************************************************************************/
OSStatus CoreAudioAUHAL::RenderCallbackSPDIF(AudioDeviceID inDevice,
                                    const AudioTimeStamp * inNow,
                                    const void * inInputData,
                                    const AudioTimeStamp * inInputTime,
                                    AudioBufferList * outOutputData,
                                    const AudioTimeStamp * inOutputTime,
                                    void * threadGlobals )
{
    //aout_buffer_t * p_buffer;
    mtime_t         current_date;
	
    //aout_instance_t * p_aout = (aout_instance_t *)threadGlobals;
	CoreAudioDeviceParameters *deviceParameters = (CoreAudioDeviceParameters *)threadGlobals;
    //struct aout_sys_t * p_sys = p_aout->output.p_sys;
	
    /* Check for the difference between the Device clock and mdate */
    //p_sys->clock_diff = - (mtime_t)
	//AudioConvertHostTimeToNanos( inNow->mHostTime ) / 1000;
    //p_sys->clock_diff += mdate();
	
    //current_date = p_sys->clock_diff +
	//AudioConvertHostTimeToNanos( inOutputTime->mHostTime ) / 1000;
	//- ((mtime_t) 1000000 / p_aout->output.output.i_rate * 31 ); // 31 = Latency in Frames. retrieve somewhere
	
    //p_buffer = aout_OutputNextBuffer( p_aout, current_date, true );
	
#define BUFFER outOutputData->mBuffers[deviceParameters->i_stream_index]
    //if( p_buffer != NULL )
    {
      //  if( (int)BUFFER.mDataByteSize != (int)p_buffer->i_nb_bytes)
        //    msg_Warn( p_aout, "bytesize: %d nb_bytes: %d", (int)BUFFER.mDataByteSize, (int)p_buffer->i_nb_bytes );
		
        /* move data into output data buffer */
        //memcpy( BUFFER.mData, p_buffer->p_buffer, p_buffer->i_nb_bytes );
        //aout_BufferFree( p_buffer );
    }
   
	
	if (BUFFER.mDataByteSize > rb_data_size(deviceParameters->outputBuffer)) // we can't write a frame, send silence
	{
        memset( BUFFER.mData, 0, BUFFER.mDataByteSize );
    }
	else // write a frame
	{
		rb_read(deviceParameters->outputBuffer, (uint8_t *)BUFFER.mData, BUFFER.mDataByteSize);
	}
#undef BUFFER
    return( noErr );
}

#endif

