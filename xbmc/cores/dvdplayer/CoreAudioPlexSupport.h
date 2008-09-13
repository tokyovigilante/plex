#ifndef _CA_SUPPORT_
#define _CA_SUPPORT_

/*
 *  CoreAudioPlexSupport.h
 *  Plex
 *
 *  Created by Ryan Walklin on 9/7/08.
 *  Copyright 2008 Ryan Walklin. All rights reserved.
 *
 */

#import <CoreAudio/AudioHardware.h>

#define STREAM_FORMAT_MSG( pre, sfm ) \
pre "[%ld][%4.4s][%ld][%ld][%ld][%ld][%ld][%ld]", \
(UInt32)sfm.mSampleRate, (char *)&sfm.mFormatID, \
sfm.mFormatFlags, sfm.mBytesPerPacket, \
sfm.mFramesPerPacket, sfm.mBytesPerFrame, \
sfm.mChannelsPerFrame, sfm.mBitsPerChannel

#define STREAM_FORMAT_MSG_FULL( pre, sfm ) \
pre ":\nsamplerate: [%ld]\nFormatID: [%4.4s]\nFormatFlags: [%ld]\nBypesPerPacket: [%ld]\nFramesPerPacket: [%ld]\nBytesPerFrame: [%ld]\nChannelsPerFrame: [%ld]\nBitsPerChannel[%ld]", \
(UInt32)sfm.mSampleRate, (char *)&sfm.mFormatID, \
sfm.mFormatFlags, sfm.mBytesPerPacket, \
sfm.mFramesPerPacket, sfm.mBytesPerFrame, \
sfm.mChannelsPerFrame, sfm.mBitsPerChannel

struct AudioDeviceInfo {
	AudioDeviceID deviceID;
	char *deviceName;
	int supportsDigital;
};

struct AudioDeviceArray {
	AudioDeviceInfo** device;
	AudioDeviceID defaultDevice;
	AudioDeviceID selectedDevice;
	AudioStreamID selectedStream;
	int selectedDeviceIndex;
	int deviceCount;
};

class CoreAudioPlexSupport
	{
	public:
		
		//
		// Get a list of output devices.
		//
		static AudioDeviceArray* GetDeviceArray();
	private:
		static int AudioDeviceHasOutput(AudioDeviceID i_dev_id);
		static int AudioDeviceSupportsDigital(AudioDeviceID i_dev_id);
		static int AudioStreamSupportsDigital(AudioStreamID i_stream_id);


	};

#endif