/*
 *  CoreAudioAUHAL.cpp
 *  Plex
 *
 *  Created by Ryan Walklin on 9/6/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
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

//#include "CPortAudio.h"
#include "stdafx.h"
#include "CoreAudioAUHAL.h"
#include "AudioContext.h"
#include "Settings.h"
//#include "Util.h"
//#include "XBAudioConfig.h"

void CoreAudioAUHAL::DoWork()
{
	
}

//////////////////////////////////////////////////////////////////////////////
//
// History:
//   12.14.07   ESF  Created.
//
//////////////////////////////////////////////////////////////////////////////
CoreAudioAUHAL::CoreAudioAUHAL(IAudioCallback* pCallback, int iChannels, unsigned int uiSamplesPerSec, unsigned int uiBitsPerSample, bool bResample, const char* strAudioCodec, bool bIsMusic, bool bPassthrough)
{
	CLog::Log(LOGDEBUG,"CoreAudioAUHAL::CoreAudioAUHAL - opening device");
	
	if (iChannels == 0)
		iChannels = 2;
	
	bool bAudioOnAllSpeakers = false;
	
	// This should be in the base class.
	g_audioContext.SetupSpeakerConfig(iChannels, bAudioOnAllSpeakers, bIsMusic);
	g_audioContext.SetActiveDevice(CAudioContext::DIRECTSOUND_DEVICE);
	
	m_bPause = false;
	m_bCanPause = false;
	m_bIsAllocated = false;
	
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
	}
	else
	{
		m_bEncodeAC3 = false;
	}
	
	m_uiChannels = iChannels;
	m_uiSamplesPerSec = uiSamplesPerSec;
	m_uiBitsPerSample = uiBitsPerSample;
	m_bPassthrough = bPassthrough;
	
	m_nCurrentVolume = g_stSettings.m_nVolumeLevel;
	if (!m_bPassthrough)
	//	m_amp.SetVolume(m_nCurrentVolume);
	
	m_dwPacketSize = iChannels*(uiBitsPerSample/8)*512;
	m_dwNumPackets = 16;
	
	/* Open the device */
	CStdString device, deviceuse;
	//if (!m_bPassthrough)
    device = g_guiSettings.GetString("audiooutput.audiodevice");
	//else
	//  device = g_guiSettings.GetString("audiooutput.passthroughdevice");
	
	CLog::Log(LOGINFO, "Asked to open device: [%s]\n", device.c_str());
	
	if (g_guiSettings.GetInt("audiooutput.mode") == AUDIO_DIGITAL && 
#ifdef __APPLE__
		g_guiSettings.GetBool("audiooutput.ac3passthrough") && 
#else
		g_audioConfig.GetAC3Enabled() &&
#endif
		iChannels > 2 &&
		!m_bPassthrough)
	{
/*		m_pStream = CPortAudio::CreateOutputStream(device,
												   SPDIF_CHANNELS, 
												   SPDIF_SAMPLERATE, 
												   SPDIF_SAMPLESIZE,
												   true,
												   g_guiSettings.GetInt("audiooutput.digitalaudiomode") == DIGITAL_COREAUDIO,
												   SPDIF_CHANNELS*(SPDIF_SAMPLESIZE/8)*512);
	}
	else
	{
		m_pStream = CPortAudio::CreateOutputStream(device,
												   m_uiChannels, 
												   m_uiSamplesPerSec, 
												   m_uiBitsPerSample,
												   m_bPassthrough,
												   g_guiSettings.GetInt("audiooutput.digitalaudiomode") == DIGITAL_COREAUDIO,
												   m_dwPacketSize);
	}*/}
    
	// Start the stream.
//	SAFELY(Pa_StartStream(m_pStream));
	
	m_bCanPause = false;
	m_bIsAllocated = true;
	
	
}

//***********************************************************************************************
CoreAudioAUHAL::~CoreAudioAUHAL()
{
	CLog::Log(LOGDEBUG,"CoreAudioAUHAL() dtor");
	Deinitialize();
}


//***********************************************************************************************
HRESULT CoreAudioAUHAL::Deinitialize()
{
	CLog::Log(LOGDEBUG,"CoreAudioAUHAL::Deinitialize");
	/*
	if (m_pStream)
	{
		SAFELY(Pa_StopStream(m_pStream));
		SAFELY(Pa_CloseStream(m_pStream));
	}*/
	if (m_bEncodeAC3)
	{
		ac3encoder_free(&m_ac3encoder);
	}
#warning free device array
	
	m_bIsAllocated = false;
	//m_pStream = 0;
	
 	CLog::Log(LOGDEBUG,"CoreAudioAUHAL::Deinitialize - set active");
	g_audioContext.SetActiveDevice(CAudioContext::DEFAULT_DEVICE);
	
	return S_OK;
}

//***********************************************************************************************
void CoreAudioAUHAL::Flush() 
{/*
	if (m_pStream)
	{
		SAFELY(Pa_AbortStream(m_pStream));
		SAFELY(Pa_StartStream(m_pStream));
	}*/
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
	/*if (m_pStream)
		SAFELY(Pa_StopStream(m_pStream));
	
	m_bPause = false;*/
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
	//m_amp.SetVolume(nVolume);
	return S_OK;
}


//***********************************************************************************************
DWORD CoreAudioAUHAL::GetSpace()
{
#warning replace with buffer check
	//if (!m_pStream)
		return 0;
	
	// Figure out how much space is available.
	//DWORD numFrames = Pa_GetStreamWriteAvailable(m_pStream);
	//return numFrames * (m_uiBitsPerSample/8);
}

//***********************************************************************************************
DWORD CoreAudioAUHAL::AddPackets(unsigned char *data, DWORD len)
{
	//if (!m_pStream) 
	{
		CLog::Log(LOGERROR,"CoreAudioAUHAL::AddPackets - sanity failed. no play handle!");
		return len; 
	}
	/*
	DWORD samplesPassedIn;
	unsigned char* pcmPtr = data;
	
	if (m_bEncodeAC3) // use the raw PCM channel count to get the number of samples to play
	{
		samplesPassedIn = len / (ac3encoder_channelcount(&m_ac3encoder) * m_uiBitsPerSample/8);
	}
	else // the PCM input and stream output should match
	{
		samplesPassedIn = len / (m_uiChannels * m_uiBitsPerSample/8);
	}
	
	// Find out how much space we have available.
	DWORD samplesToWrite  = Pa_GetStreamWriteAvailable(m_pStream);
	
	// Clip to the amount we got passed in. I was using MIN above, but that
	// was a very bad idea since Pa_GetStreamWriteAvailable would get called
	// twice and could return different answers!
	//
	if (samplesToWrite > samplesPassedIn)
		samplesToWrite = samplesPassedIn;
	//#warning AC3 disabled
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
			unsigned char ac3_framebuffer[AC3_SPDIF_FRAME_SIZE];
			memset(ac3_framebuffer, 0, sizeof(ac3_framebuffer));
			
			int buffer_sample_readcount = -1;
			if ((buffer_sample_readcount = ac3encoder_get_encoded_samples(&m_ac3encoder, ac3_framebuffer, samplesToWrite)) != samplesToWrite)
			{
				CLog::Log(LOGERROR, "AC3 output buffer underrun");
			}
			else
			{
				SAFELY(Pa_WriteStream(m_pStream, ac3_framebuffer, samplesToWrite));
			}
			return samplesToWrite * ac3encoder_channelcount(&m_ac3encoder) * (m_uiBitsPerSample/8);
		}
	}
	else
	{
		// Handle volume de-amplification.
		if (!m_bPassthrough)
			m_amp.DeAmplify((short *)pcmPtr, samplesToWrite * m_uiChannels);
		
		// Write data to the stream.
		SAFELY(Pa_WriteStream(m_pStream, pcmPtr, samplesToWrite));	  
		return samplesToWrite * m_uiChannels * (m_uiBitsPerSample/8);
	}*/
}

//***********************************************************************************************
FLOAT CoreAudioAUHAL::GetDelay()
{
	//if (m_pStream == 0)
		return 0.0;
	
	// This always returns zero, so it's totally useless.
	//const PaStreamInfo* streamInfo = Pa_GetStreamInfo(m_pStream);
	//FLOAT delay = (FLOAT)streamInfo->outputLatency;
	
	// For now hardwire to about +15ms from "base", which is what we're observing.
	FLOAT delay = 0.015;
	
	if (g_audioContext.IsAC3EncoderActive())
		delay += 0.049;
	else if (m_bEncodeAC3)
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
	//m_pCallback = pCallback;
}

void CoreAudioAUHAL::UnRegisterAudioCallback()
{
	//m_pCallback = NULL;
}

void CoreAudioAUHAL::WaitCompletion()
{
	
}

void CoreAudioAUHAL::SwitchChannels(int iAudioStream, bool bAudioOnAllSpeakers)
{
    return ;
}

#if 0
/*****************************************************************************
 * Open: open macosx audio output
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0;
    struct aout_sys_t       *p_sys = NULL;
    vlc_value_t             val;
    aout_instance_t         *p_aout = (aout_instance_t *)p_this;
	
    /* Use int here, to match kAudioDevicePropertyDeviceIsAlive
     * property size */
    int                     b_alive = false; 
	
    /* Allocate structure */
    p_aout->output.p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
        return VLC_ENOMEM;
	
    p_sys = p_aout->output.p_sys;
    p_sys->i_default_dev = 0;
    p_sys->i_selected_dev = 0;
    p_sys->i_devices = 0;
    p_sys->b_supports_digital = false;
    p_sys->b_digital = false;
    p_sys->au_component = NULL;
    p_sys->au_unit = NULL;
    p_sys->clock_diff = (mtime_t) 0;
    p_sys->i_read_bytes = 0;
    p_sys->i_total_bytes = 0;
    p_sys->i_hog_pid = -1;
    p_sys->i_stream_id = 0;
    p_sys->i_stream_index = -1;
    p_sys->b_revert = false;
    p_sys->b_changed_mixing = false;
    memset( p_sys->p_remainder_buffer, 0, sizeof(uint8_t) * BUFSIZE );
	
    p_aout->output.pf_play = Play;
	
    aout_FormatPrint( p_aout, "VLC is looking for:", (audio_sample_format_t *)&p_aout->output.output );
	
    /* Persistent device variable */
    if( var_Type( p_aout->p_libvlc, "macosx-audio-device" ) == 0 )
    {
        var_Create( p_aout->p_libvlc, "macosx-audio-device", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    }
	
    /* Build a list of devices */
    if( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout );
    }
	
    /* What device do we want? */
    if( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        msg_Err( p_aout, "audio-device var does not exist. device probe failed." );
        goto error;
    }
	
    p_sys->i_selected_dev = val.i_int & ~AOUT_VAR_SPDIF_FLAG; /* remove SPDIF flag to get the true DeviceID */
    p_sys->b_supports_digital = ( val.i_int & AOUT_VAR_SPDIF_FLAG ) ? true : false;
	
    /* Check if the desired device is alive and usable */
    /* TODO: add a callback to the device to alert us if the device dies */
    i_param_size = sizeof( b_alive );
    err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE,
								 kAudioDevicePropertyDeviceIsAlive,
								 &i_param_size, &b_alive );
	
    if( err != noErr )
    {
        /* Be tolerant, only give a warning here */
        msg_Warn( p_aout, "could not check whether device [0x%x] is alive: %4.4s", (unsigned int)p_sys->i_selected_dev, (char *)&err );
        b_alive = false;
    }
	
    if( b_alive == false )
    {
        msg_Warn( p_aout, "selected audio device is not alive, switching to default device" );
        p_sys->i_selected_dev = p_sys->i_default_dev;
    }
	
    i_param_size = sizeof( p_sys->i_hog_pid );
    err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE,
								 kAudioDevicePropertyHogMode,
								 &i_param_size, &p_sys->i_hog_pid );
	
    if( err != noErr )
    {
        /* This is not a fatal error. Some drivers simply don't support this property */
        msg_Warn( p_aout, "could not check whether device is hogged: %4.4s",
                 (char *)&err );
        p_sys->i_hog_pid = -1;
    }
	
    if( p_sys->i_hog_pid != -1 && p_sys->i_hog_pid != getpid() )
    {
        msg_Err( p_aout, "Selected audio device is exclusively in use by another program." );
        intf_UserFatal( p_aout, false, _("Audio output failed"),
					   _("The selected audio output device is exclusively in "
						 "use by another program.") );
        goto error;
    }
	
    /* Check for Digital mode or Analog output mode */
    if( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) && p_sys->b_supports_digital )
    {
        if( OpenSPDIF( p_aout ) )
            return VLC_SUCCESS;
    }
    else
    {
        if( OpenAnalog( p_aout ) )
            return VLC_SUCCESS;
    }
	
error:
    /* If we reach this, this aout has failed */
    var_Destroy( p_aout, "audio-device" );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Open: open and setup a HAL AudioUnit to do analog (multichannel) audio output
 *****************************************************************************/
static int OpenAnalog( aout_instance_t *p_aout )
{
    struct aout_sys_t           *p_sys = p_aout->output.p_sys;
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
	
    p_sys->au_component = FindNextComponent( NULL, &desc );
    if( p_sys->au_component == NULL )
    {
        msg_Warn( p_aout, "we cannot find our HAL component" );
        return false;
    }
	
    err = OpenAComponent( p_sys->au_component, &p_sys->au_unit );
    if( err != noErr )
    {
        msg_Warn( p_aout, "we cannot open our HAL component" );
        return false;
    }
	
    /* Set the device we will use for this output unit */
    err = AudioUnitSetProperty( p_sys->au_unit,
							   kAudioOutputUnitProperty_CurrentDevice,
							   kAudioUnitScope_Global,
							   0,
							   &p_sys->i_selected_dev,
							   sizeof( AudioDeviceID ));
	
    if( err != noErr )
    {
        msg_Warn( p_aout, "we cannot select the audio device" );
        return false;
    }
	
    /* Get the current format */
    i_param_size = sizeof(AudioStreamBasicDescription);
	
    err = AudioUnitGetProperty( p_sys->au_unit,
							   kAudioUnitProperty_StreamFormat,
							   kAudioUnitScope_Input,
							   0,
							   &DeviceFormat,
							   &i_param_size );
	
    if( err != noErr ) return false;
    else msg_Dbg( p_aout, STREAM_FORMAT_MSG( "current format is: ", DeviceFormat ) );
	
    /* Get the channel layout of the device side of the unit (vlc -> unit -> device) */
    err = AudioUnitGetPropertyInfo( p_sys->au_unit,
                                   kAudioDevicePropertyPreferredChannelLayout,
                                   kAudioUnitScope_Output,
                                   0,
                                   &i_param_size,
                                   NULL );
	
    if( err == noErr )
    {
        layout = (AudioChannelLayout *)malloc( i_param_size);
		
        verify_noerr( AudioUnitGetProperty( p_sys->au_unit,
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
		
        msg_Dbg( p_aout, "layout of AUHAL has %d channels" , (int)layout->mNumberChannelDescriptions );
		
        /* Initialize the VLC core channel count */
        p_aout->output.output.i_physical_channels = 0;
        i_original = p_aout->output.output.i_original_channels & AOUT_CHAN_PHYSMASK;
		
        if( i_original == AOUT_CHAN_CENTER || layout->mNumberChannelDescriptions < 2 )
        {
            /* We only need Mono or cannot output more than 1 channel */
            p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
        }
        else if( i_original == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) || layout->mNumberChannelDescriptions < 3 )
        {
            /* We only need Stereo or cannot output more than 2 channels */
            p_aout->output.output.i_physical_channels = AOUT_CHAN_RIGHT | AOUT_CHAN_LEFT;
        }
        else
        {
            /* We want more than stereo and we can do that */
            for( i = 0; i < layout->mNumberChannelDescriptions; i++ )
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
    }
	
    msg_Dbg( p_aout, "selected %d physical channels for device output", aout_FormatNbChannels( &p_aout->output.output ) );
    msg_Dbg( p_aout, "VLC will output: %s", aout_FormatPrintChannels( &p_aout->output.output ));
	
    memset (&new_layout, 0, sizeof(new_layout));
    switch( aout_FormatNbChannels( &p_aout->output.output ) )
    {
        case 1:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
            break;
        case 2:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
            break;
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
            break;
        case 7:
            /* FIXME: This is incorrect. VLC uses the internal ordering: L R Lm Rm Lr Rr C LFE but this is wrong */
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_6_1_A; // L R C LFE Ls Rs Cs
            break;
        case 8:
            /* FIXME: This is incorrect. VLC uses the internal ordering: L R Lm Rm Lr Rr C LFE but this is wrong */
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_7_1_A; // L R C LFE Ls Rs Lc Rc
            break;
    }
	
    /* Set up the format to be used */
    DeviceFormat.mSampleRate = p_aout->output.output.i_rate;
    DeviceFormat.mFormatID = kAudioFormatLinearPCM;
	
    /* We use float 32. It's the best supported format by both VLC and Coreaudio */
    p_aout->output.output.i_format = VLC_FOURCC( 'f','l','3','2');
    DeviceFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    DeviceFormat.mBitsPerChannel = 32;
    DeviceFormat.mChannelsPerFrame = aout_FormatNbChannels( &p_aout->output.output );
	
    /* Calculate framesizes and stuff */
    DeviceFormat.mFramesPerPacket = 1;
    DeviceFormat.mBytesPerFrame = DeviceFormat.mBitsPerChannel * DeviceFormat.mChannelsPerFrame / 8;
    DeviceFormat.mBytesPerPacket = DeviceFormat.mBytesPerFrame * DeviceFormat.mFramesPerPacket;
	
    /* Set the desired format */
    i_param_size = sizeof(AudioStreamBasicDescription);
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
									   kAudioUnitProperty_StreamFormat,
									   kAudioUnitScope_Input,
									   0,
									   &DeviceFormat,
									   i_param_size ));
	
    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "we set the AU format: " , DeviceFormat ) );
	
    /* Retrieve actual format */
    verify_noerr( AudioUnitGetProperty( p_sys->au_unit,
									   kAudioUnitProperty_StreamFormat,
									   kAudioUnitScope_Input,
									   0,
									   &DeviceFormat,
									   &i_param_size ));
	
    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "the actual set AU format is " , DeviceFormat ) );
	
    /* Do the last VLC aout setups */
    aout_FormatPrepare( &p_aout->output.output );
    p_aout->output.i_nb_samples = 2048;
    aout_VolumeSoftInit( p_aout );
	
    /* set the IOproc callback */
    input.inputProc = (AURenderCallback) RenderCallbackAnalog;
    input.inputProcRefCon = p_aout;
	
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
									   kAudioUnitProperty_SetRenderCallback,
									   kAudioUnitScope_Input,
									   0, &input, sizeof( input ) ) );
	
    input.inputProc = (AURenderCallback) RenderCallbackAnalog;
    input.inputProcRefCon = p_aout;
	
    /* Set the new_layout as the layout VLC will use to feed the AU unit */
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
									   kAudioUnitProperty_AudioChannelLayout,
									   kAudioUnitScope_Input,
									   0, &new_layout, sizeof(new_layout) ) );
	
    if( new_layout.mNumberChannelDescriptions > 0 )
        free( new_layout.mChannelDescriptions );
	
    /* AU initiliaze */
    verify_noerr( AudioUnitInitialize(p_sys->au_unit) );
	
    /* Find the difference between device clock and mdate clock */
    p_sys->clock_diff = - (mtime_t)
	AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000;
    p_sys->clock_diff += mdate();
	
    /* Start the AU */
    verify_noerr( AudioOutputUnitStart(p_sys->au_unit) );
	
    return true;
}

/*****************************************************************************
 * Setup a encoded digital stream (SPDIF)
 *****************************************************************************/
static int OpenSPDIF( aout_instance_t * p_aout )
{
    struct aout_sys_t       *p_sys = p_aout->output.p_sys;
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0, b_mix = 0;
    Boolean                 b_writeable = false;
    AudioStreamID           *p_streams = NULL;
    int                     i = 0, i_streams = 0;
	
    /* Start doing the SPDIF setup proces */
    p_sys->b_digital = true;
	
    /* Hog the device */
    i_param_size = sizeof( p_sys->i_hog_pid );
    p_sys->i_hog_pid = getpid() ;
	
    err = AudioDeviceSetProperty( p_sys->i_selected_dev, 0, 0, FALSE,
								 kAudioDevicePropertyHogMode, i_param_size, &p_sys->i_hog_pid );
	
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set hogmode: [%4.4s]", (char *)&err );
        return false;
    }
	
    /* Set mixable to false if we are allowed to */
    err = AudioDeviceGetPropertyInfo( p_sys->i_selected_dev, 0, FALSE, kAudioDevicePropertySupportsMixing,
									 &i_param_size, &b_writeable );
	
    err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE, kAudioDevicePropertySupportsMixing,
								 &i_param_size, &b_mix );
	
    if( !err && b_writeable )
    {
        b_mix = 0;
        err = AudioDeviceSetProperty( p_sys->i_selected_dev, 0, 0, FALSE,
									 kAudioDevicePropertySupportsMixing, i_param_size, &b_mix );
        p_sys->b_changed_mixing = true;
    }
	
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set mixmode: [%4.4s]", (char *)&err );
        return false;
    }
	
    /* Get a list of all the streams on this device */
    err = AudioDeviceGetPropertyInfo( p_sys->i_selected_dev, 0, FALSE,
									 kAudioDevicePropertyStreams,
									 &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%4.4s]", (char *)&err );
        return false;
    }
	
    i_streams = i_param_size / sizeof( AudioStreamID );
    p_streams = (AudioStreamID *)malloc( i_param_size );
    if( p_streams == NULL )
        return false;
	
    err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE,
								 kAudioDevicePropertyStreams,
								 &i_param_size, p_streams );
	
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%4.4s]", (char *)&err );
        free( p_streams );
        return false;
    }
	
    for( i = 0; i < i_streams && p_sys->i_stream_index < 0 ; i++ )
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
            msg_Err( p_aout, "could not get number of streamformats: [%4.4s]", (char *)&err );
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
            msg_Err( p_aout, "could not get the list of streamformats: [%4.4s]", (char *)&err );
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
			
            p_sys->i_stream_id = p_streams[i];
            p_sys->i_stream_index = i;
			
            if( p_sys->b_revert == false )
            {
                /* Retrieve the original format of this stream first if not done so already */
                i_param_size = sizeof( p_sys->sfmt_revert );
                err = AudioStreamGetProperty( p_sys->i_stream_id, 0,
											 kAudioStreamPropertyPhysicalFormat,
											 &i_param_size,
											 &p_sys->sfmt_revert );
                if( err != noErr )
                {
                    msg_Err( p_aout, "could not retrieve the original streamformat: [%4.4s]", (char *)&err );
                    continue;
                }
                p_sys->b_revert = true;
            }
			
            for( j = 0; j < i_formats; j++ )
            {
                if( p_format_list[j].mFormatID == 'IAC3' ||
				   p_format_list[j].mFormatID == kAudioFormat60958AC3 )
                {
                    if( p_format_list[j].mSampleRate == p_aout->output.output.i_rate )
                    {
                        i_requested_rate_format = j;
                        break;
                    }
                    else if( p_format_list[j].mSampleRate == p_sys->sfmt_revert.mSampleRate )
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
                p_sys->stream_format = p_format_list[i_requested_rate_format];
            else if( i_current_rate_format >= 0 ) /* If not possible, we will try to use the current samplerate of the device */
                p_sys->stream_format = p_format_list[i_current_rate_format];
            else p_sys->stream_format = p_format_list[i_backup_rate_format]; /* And if we have to, any digital format will be just fine (highest rate possible) */
        }
        free( p_format_list );
    }
    free( p_streams );
	
    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "original stream format: ", p_sys->sfmt_revert ) );
	
    if( !AudioStreamChangeFormat( p_aout, p_sys->i_stream_id, p_sys->stream_format ) )
        return false;
	
    /* Set the format flags */
    if( p_sys->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian )
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','b');
    else
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
    p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
    p_aout->output.output.i_frame_length = A52_FRAME_NB;
    p_aout->output.i_nb_samples = p_aout->output.output.i_frame_length;
    p_aout->output.output.i_rate = (unsigned int)p_sys->stream_format.mSampleRate;
    aout_FormatPrepare( &p_aout->output.output );
    aout_VolumeNoneInit( p_aout );
	
    /* Add IOProc callback */
    err = AudioDeviceAddIOProc( p_sys->i_selected_dev,
                               (AudioDeviceIOProc)RenderCallbackSPDIF,
                               (void *)p_aout );
	
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceAddIOProc failed: [%4.4s]", (char *)&err );
        return false;
    }
	
    /* Check for the difference between the Device clock and mdate */
    p_sys->clock_diff = - (mtime_t)
	AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000;
    p_sys->clock_diff += mdate();
	
    /* Start device */
    err = AudioDeviceStart( p_sys->i_selected_dev, (AudioDeviceIOProc)RenderCallbackSPDIF );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceStart failed: [%4.4s]", (char *)&err );
		
        err = AudioDeviceRemoveIOProc( p_sys->i_selected_dev,
									  (AudioDeviceIOProc)RenderCallbackSPDIF );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceRemoveIOProc failed: [%4.4s]", (char *)&err );
        }
        return false;
    }
	
    return true;
}


/*****************************************************************************
 * Probe: Check which devices the OS has, and add them to our audio-device menu
 *****************************************************************************/
static void Probe( aout_instance_t * p_aout )
{
    OSStatus            err = noErr;
    UInt32              i = 0, i_param_size = 0;
    AudioDeviceID       devid_def = 0;
    AudioDeviceID       *p_devices = NULL;
    vlc_value_t         val, text;
	
    struct aout_sys_t   *p_sys = p_aout->output.p_sys;
	
    /* Get number of devices */
    err = AudioHardwareGetPropertyInfo( kAudioHardwarePropertyDevices,
									   &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "Could not get number of devices: [%4.4s]", (char *)&err );
        goto error;
    }
	
    p_sys->i_devices = i_param_size / sizeof( AudioDeviceID );
	
    if( p_sys->i_devices < 1 )
    {
        msg_Err( p_aout, "No audio output devices were found." );
        goto error;
    }
	
    msg_Dbg( p_aout, "system has [%ld] device(s)", p_sys->i_devices );
	
    /* Allocate DeviceID array */
    p_devices = (AudioDeviceID*)malloc( sizeof(AudioDeviceID) * p_sys->i_devices );
    if( p_devices == NULL )
        goto error;
	
    /* Populate DeviceID array */
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDevices,
								   &i_param_size, p_devices );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get the device IDs: [%4.4s]", (char *)&err );
        goto error;
    }
	
    /* Find the ID of the default Device */
    i_param_size = sizeof( AudioDeviceID );
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDefaultOutputDevice,
								   &i_param_size, &devid_def );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get default audio device: [%4.4s]", (char *)&err );
        goto error;
    }
    p_sys->i_default_dev = devid_def;
	
    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER|VLC_VAR_HASCHOICE );
    text.psz_string = (char*)_("Audio Device");
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );
	
    for( i = 0; i < p_sys->i_devices; i++ )
    {
        char *psz_name;
        i_param_size = 0;
		
        /* Retrieve the length of the device name */
        err = AudioDeviceGetPropertyInfo(
										 p_devices[i], 0, false,
										 kAudioDevicePropertyDeviceName,
										 &i_param_size, NULL);
        if( err ) goto error;
		
        /* Retrieve the name of the device */
        psz_name = (char *)malloc( i_param_size );
        err = AudioDeviceGetProperty(
									 p_devices[i], 0, false,
									 kAudioDevicePropertyDeviceName,
									 &i_param_size, psz_name);
        if( err ) goto error;
		
        msg_Dbg( p_aout, "DevID: %#lx DevName: %s", p_devices[i], psz_name );
		
        if( !AudioDeviceHasOutput( p_devices[i]) )
        {
            msg_Dbg( p_aout, "this device is INPUT only. skipping..." );
            continue;
        }
		
        /* Add the menu entries */
        val.i_int = (int)p_devices[i];
        text.psz_string = psz_name;
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        text.psz_string = NULL;
        if( p_sys->i_default_dev == p_devices[i] )
        {
            /* The default device is the selected device normally */
            var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
            var_Set( p_aout, "audio-device", val );
        }
		
        if( AudioDeviceSupportsDigital( p_aout, p_devices[i] ) )
        {
            val.i_int = (int)p_devices[i] | AOUT_VAR_SPDIF_FLAG;
            if( asprintf( &text.psz_string, _("%s (Encoded Output)"), psz_name ) != -1 )
            {
                var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
                free( text.psz_string );
                if( p_sys->i_default_dev == p_devices[i] && config_GetInt( p_aout, "spdif" ) )
                {
                    /* We selected to prefer SPDIF output if available
                     * then this "dummy" entry should be selected */
                    var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
                    var_Set( p_aout, "audio-device", val );
                }
            }
        }
		
        free( psz_name);
    }
	
    /* If a device is already "preselected", then use this device */
    var_Get( p_aout->p_libvlc, "macosx-audio-device", &val );
    if( val.i_int > 0 )
    {
        var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
        var_Set( p_aout, "audio-device", val );
    }
	
    /* If we change the device we want to use, we should renegotiate the audio chain */
    var_AddCallback( p_aout, "audio-device", AudioDeviceCallback, NULL );
	
    /* Attach a Listener so that we are notified of a change in the Device setup */
    err = AudioHardwareAddPropertyListener( kAudioHardwarePropertyDevices,
										   HardwareListener,
										   (void *)p_aout );
    if( err )
        goto error;
	
    free( p_devices );
    return;
	
error:
    var_Destroy( p_aout, "audio-device" );
    free( p_devices );
    return;
}
#endif

#endif
