/*
 *  CoreAudioPlexSupport.cpp
 *  Plex
 *
 *  Created by Ryan Walklin on 9/7/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "CoreAudioPlexSupport.h"
#include "stdafx.h"
#include "log.h"

#pragma mark Device Interface

AudioDeviceArray* CoreAudioPlexSupport::GetDeviceArray()
{
	OSStatus            err = noErr;
    UInt32              i = 0, totalDeviceCount = 0, i_param_size = 0;
    AudioDeviceID       devid_def = 0;
    AudioDeviceID       *p_devices = NULL;
    
	/* Get number of devices */
    err = AudioHardwareGetPropertyInfo( kAudioHardwarePropertyDevices,
									   &i_param_size, NULL );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Could not get number of devices: [%4.4s]", (char *)&err );
        return NULL;
		//goto error;
    }
	
    totalDeviceCount = i_param_size / sizeof( AudioDeviceID );
	
    if( totalDeviceCount < 1 )
    {
		CLog::Log(LOGERROR, "No audio output devices were found." );
		return NULL;
        //goto error;
    }
	
	CLog::Log(LOGNOTICE, "System has %ld device(s)", totalDeviceCount );
	
    /* Allocate DeviceID array */
	AudioDeviceArray *newDeviceArray = (AudioDeviceArray*)calloc(1, sizeof(AudioDeviceArray));
	newDeviceArray->device = (AudioDeviceInfo**)calloc(totalDeviceCount, sizeof(AudioDeviceInfo));
	
    p_devices = (AudioDeviceID*)calloc(totalDeviceCount, sizeof(AudioDeviceID));
	
	if( p_devices == NULL )
        return NULL;
	
    /* Populate DeviceID array */
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDevices,
								   &i_param_size, p_devices );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "could not get the device IDs: [%4.4s]", (char *)&err );
        goto error;
    }
	
    /* Find the ID of the default Device */
    i_param_size = sizeof( AudioDeviceID );
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDefaultOutputDevice,
								   &i_param_size, &devid_def );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "could not get default audio device: [%4.4s]", (char *)&err );
        return NULL;
    }
	newDeviceArray->defaultDevice = devid_def;
    
	for( i = 0; i < totalDeviceCount; i++ )
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
		
		CLog::Log(LOGINFO, "DevID: %#lx DevName: %s", p_devices[i], psz_name );
		
        if( !AudioDeviceHasOutput( p_devices[i]) )
        {
			CLog::Log(LOGDEBUG, "Skipping input-only device %i", p_devices[i]);
            continue;
        }
		
		// Add output device IDs to array
		AudioDeviceInfo *currentDevice = (AudioDeviceInfo*)malloc(sizeof(AudioDeviceInfo));
		currentDevice->deviceID = p_devices[i];
		currentDevice->deviceName = psz_name;
		
		if( newDeviceArray->defaultDevice == p_devices[i] )
        {
			CLog::Log(LOGNOTICE, "Selecting default device %s (%i)", 
					  currentDevice->deviceName, 
					  currentDevice->deviceID);
			newDeviceArray->selectedDevice = currentDevice->deviceID;
			newDeviceArray->selectedDeviceIndex = i;
        }
		
        if( AudioDeviceSupportsDigital(p_devices[i]))
        {
			currentDevice->supportsDigital = TRUE;
        }
		else currentDevice->supportsDigital = FALSE;
		
		newDeviceArray->device[newDeviceArray->deviceCount++] = currentDevice;
		currentDevice = NULL;
    }
	
    /* If we change the device we want to use, we should renegotiate the audio chain */
    //var_AddCallback( p_aout, "audio-device", AudioDeviceCallback, NULL );
#warning fix this to track device changes
    /* Attach a Listener so that we are notified of a change in the Device setup */
    //err = AudioHardwareAddPropertyListener( kAudioHardwarePropertyDevices,
	//									   HardwareListener,
	//									   (void *)p_aout );
    //if( err )
    //    goto error;
	
    free( p_devices );
    return newDeviceArray;
	
error:
    //var_Destroy( p_aout, "audio-device" );
    free( p_devices );
#warning need to free strings
    return NULL;
}

/*****************************************************************************
 * AudioStreamSupportsDigital: Check i_stream_id for digital stream support.
 *****************************************************************************/
int CoreAudioPlexSupport::AudioDeviceSupportsDigital(AudioDeviceID i_dev_id)
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamID               *p_streams = NULL;
    int                         i = 0, i_streams = 0;
    bool                  b_return = false;
	
    /* Retrieve all the output streams */
    err = AudioDeviceGetPropertyInfo( i_dev_id, 0, FALSE,
									 kAudioDevicePropertyStreams,
									 &i_param_size, NULL );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "could not get number of streams: [%4.4s]", (char *)&err );
        return false;
    }
	
    i_streams = i_param_size / sizeof( AudioStreamID );
    p_streams = (AudioStreamID *)malloc( i_param_size );
    if( p_streams == NULL )
        return false;
	
    err = AudioDeviceGetProperty( i_dev_id, 0, FALSE,
								 kAudioDevicePropertyStreams,
								 &i_param_size, p_streams );
	
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "could not get number of streams: [%4.4s]", (char *)&err );
        return false;
    }
	
    for( i = 0; i < i_streams; i++ )
    {
        if( AudioStreamSupportsDigital( p_streams[i] ) )
            b_return = true;
    }
	
    free( p_streams );
    return b_return;
}

/*****************************************************************************
 * AudioStreamSupportsDigital: Check i_stream_id for digital stream support.
 *****************************************************************************/
int CoreAudioPlexSupport::AudioStreamSupportsDigital(AudioStreamID i_stream_id )

{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamBasicDescription *p_format_list = NULL;
    int                         i = 0, i_formats = 0;
    bool                  b_return = false;
	
    /* Retrieve all the stream formats supported by each output stream */
    err = AudioStreamGetPropertyInfo( i_stream_id, 0,
									 kAudioStreamPropertyPhysicalFormats,
									 &i_param_size, NULL );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "could not get number of streamformats: [%4.4s]", (char *)&err );
        return false;
    }
	
    i_formats = i_param_size / sizeof( AudioStreamBasicDescription );
    p_format_list = (AudioStreamBasicDescription *)malloc( i_param_size );
    if( p_format_list == NULL )
        return false;
	
    err = AudioStreamGetProperty( i_stream_id, 0,
								 kAudioStreamPropertyPhysicalFormats,
								 &i_param_size, p_format_list );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "could not get the list of streamformats: [%4.4s]", (char *)&err );
        free( p_format_list);
        p_format_list = NULL;
        return false;
    }
	
    for( i = 0; i < i_formats; i++ )
    {
		CLog::Log(LOGDEBUG, STREAM_FORMAT_MSG( "supported format: ", p_format_list[i] ) );
		
        if( p_format_list[i].mFormatID == 'IAC3' ||
		   p_format_list[i].mFormatID == kAudioFormat60958AC3 )
        {
            b_return = true;
        }
    }
	
    free( p_format_list );
    return b_return;
}

/*****************************************************************************
 * AudioDeviceHasOutput: Checks if the Device actually provides any outputs at all
 *****************************************************************************/
int CoreAudioPlexSupport::AudioDeviceHasOutput( AudioDeviceID i_dev_id )
{
    UInt32            dataSize;
    Boolean            isWritable;
    
    AudioDeviceGetPropertyInfo( i_dev_id, 0, FALSE, kAudioDevicePropertyStreams, &dataSize, &isWritable);
    if (dataSize == 0) return FALSE;
	
    return TRUE;
}

