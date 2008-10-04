/*
   OSXBMC
   ac3encoder.h  - AC3Encoder (header)
   
   Copyright (c) 2008, Ryan Walklin (ryanwalklin@gmail.com)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public Licensse as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

 */
#pragma once
#include "ringbuffer.h"
#include "aften-types.h"
#include "stdint.h"

//These values are forced to allow spdif out
#define SPDIF_SAMPLESIZE 16
#define SPDIF_CHANNELS 2
#define SPDIF_SAMPLERATE 48000
#define AC3_SAMPLES_PER_FRAME 1536
#define AC3_SPDIF_FRAME_SIZE AC3_SAMPLES_PER_FRAME * SPDIF_CHANNELS * SPDIF_SAMPLESIZE / 8
#define AC3_BITRATE 640

struct AC3Encoder
{
	struct OutRingBuffer *m_inputBuffer;
	struct OutRingBuffer *m_outputBuffer;
	AftenContext m_aftenContext;
	int m_iSampleSize;
	
	// encoder flags
	int last_frame;
    int got_fs_once;
    int iAC3FrameSize;
    int irawSampleBytesRead;	
	int remap;
	
	CCriticalSection critSection;
};

void ac3encoder_init(struct AC3Encoder *encoder, int iChannels, unsigned int uiSamplesPerSec, int uiBitsPerSample, int remap);
int ac3encoder_write_samples(struct AC3Encoder *encoder, uint8_t *samples, int samples_in);
int ac3coder_get_PCM_samplecount(struct AC3Encoder *encoder);
int ac3encoder_get_AC3_samplecount(struct AC3Encoder *encoder);
int ac3encoder_get_encoded_samples(struct AC3Encoder *encoder, uint8_t *encoded_samples, int samples_out);
int ac3encoder_channelcount(struct AC3Encoder *encoder);
void ac3encoder_flush(struct AC3Encoder *encoder);
void ac3encoder_free(struct AC3Encoder *encoder);
