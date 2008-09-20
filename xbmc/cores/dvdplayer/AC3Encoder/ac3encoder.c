/*
   OSXBMC
   ac3encoder.c  - AC3Encoder
   
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

#include "ac3encoder.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <aften.h>

static const int acmod_to_ch[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };

static const char *acmod_str[8] = {
    "dual mono (1+1)", "mono (1/0)", "stereo (2/0)",
    "3/0", "2/1", "3/1", "2/2", "3/2"
};

static inline int swabdata(char* dst, char* src, int size)
{	
	if( size & 0x1 )
	{
		swab(src, dst, size-1);
		dst+=size-1;
		src+=size-1;
		
		dst[0] = 0x0;
		dst[1] = src[0];
		return size+1;
	}
	else
	{
		swab(src, dst, size);
		return size;
	}
	
}

// call before using the encoder
// initialises the aften context and the I/O buffers
void ac3encoder_init(struct AC3Encoder *encoder, int iChannels, unsigned int uiSamplesPerSec, int uiBitsPerSample, int remap)
{
	rb_init(&encoder->m_inputBuffer, 2048 * 6 * 10); // store up to 10240 six-channel samples
	rb_init(&encoder->m_outputBuffer, AC3_SPDIF_FRAME_SIZE * 5); // store up to 5 AC3 frames
	
	aften_set_defaults(&encoder->m_aftenContext);
	
	encoder->m_aftenContext.channels = iChannels;
	encoder->m_aftenContext.samplerate = uiSamplesPerSec;
	encoder->m_iSampleSize = uiBitsPerSample;
	encoder->remap = remap;
	
	
	encoder->m_aftenContext.params.bitrate = AC3_BITRATE; // set AC3 output bitrate to maximum
	
	encoder->m_aftenContext.acmod = -1;
	encoder->m_aftenContext.lfe = 0;
	switch (iChannels)
	{
		case 1:
			encoder->m_aftenContext.acmod = 1; // mono
			break;
		case 2:
			encoder->m_aftenContext.acmod = 2; // stereo
			break;
		case 3:
			encoder->m_aftenContext.acmod = 2; // stereo with LFE, ie Stereo/Prologic 2.1
			encoder->m_aftenContext.lfe = 1;
			break;
		case 4:
			encoder->m_aftenContext.acmod = 6; // Quadraphonic, ie 2/2
			break;
		case 5:
			encoder->m_aftenContext.acmod = 6; // Quadraphonic with LFE - I don't know if this even exists
			encoder->m_aftenContext.lfe = 1;
			break;
		case 6:
			encoder->m_aftenContext.acmod = 7;
			encoder->m_aftenContext.lfe = 1;
			break;
		default:
			fprintf(stderr, "Invalid number of channels for AC3 (%i)\n", iChannels); 
			break;
	}
	
	encoder->last_frame = 0;
    encoder->got_fs_once = 0;
    encoder->iAC3FrameSize = 0;
    encoder->irawSampleBytesRead = 0;
	
	// print ac3 info to console
	
	if (aften_encode_init(&encoder->m_aftenContext))
	{
		fprintf(stderr, "Error initialising AC3 encoder\n");
		aften_encode_close(&encoder->m_aftenContext);
	}
	else fprintf(stdout, "AC3 encoder initialised with configuration: %d Hz %s %s", 
				   encoder->m_aftenContext.samplerate, 
				   acmod_str[encoder->m_aftenContext.acmod], 
				   (encoder->m_aftenContext.lfe == 1) ? "+ LFE\n" : "\n");
	
	// pre-fill AC3 buffer with silent frame so we can output immediately
	unsigned char *silent_PCM_init = calloc(1, encoder->m_aftenContext.channels * encoder->m_iSampleSize / 8 * AC3_SAMPLES_PER_FRAME); // 1536 silent samples
	if (ac3encoder_write_samples(encoder, silent_PCM_init, AC3_SAMPLES_PER_FRAME) < AC3_SAMPLES_PER_FRAME)
	{
		fprintf(stderr, "Error pre-buffering AC3 encoder\n");
		aften_encode_close(&encoder->m_aftenContext);
	}
	free(silent_PCM_init);
}

// returns number of available AC3 frames
int ac3encoder_write_samples(struct AC3Encoder *encoder, unsigned char *samples, int frames_in)
{
	int input_size = frames_in * encoder->m_aftenContext.channels * encoder->m_iSampleSize / 8;
	rb_write(encoder->m_inputBuffer, samples, input_size);
	
	void (*aften_remap)(void *samples, int n, int ch,
                        A52SampleFormat fmt, int acmod) = NULL;
	
	if(encoder->remap == 1)
        aften_remap = aften_remap_mpeg_to_a52;
    else
        aften_remap = aften_remap_wav_to_a52;
	
	while (ac3coder_get_PCM_samplecount(encoder) >= AC3_SAMPLES_PER_FRAME)
	{
		// copy to output buffer
		encoder->irawSampleBytesRead = AC3_SAMPLES_PER_FRAME * encoder->m_aftenContext.channels * encoder->m_iSampleSize / 8;
		unsigned char *sample_buffer;
		sample_buffer = calloc(1, encoder->irawSampleBytesRead);
		if (rb_read(encoder->m_inputBuffer, sample_buffer, encoder->irawSampleBytesRead) < encoder->irawSampleBytesRead)
		{
			free(sample_buffer);
			return ac3encoder_get_AC3_samplecount(encoder);
		}
		
		if(aften_remap)
            aften_remap(sample_buffer, AC3_SAMPLES_PER_FRAME, encoder->m_aftenContext.channels, encoder->m_aftenContext.sample_format, encoder->m_aftenContext.acmod);
		
		//encode
		unsigned char AC3_frame_buffer[AC3_SPDIF_FRAME_SIZE], oldendian[AC3_SPDIF_FRAME_SIZE];
		do 
		{
			memset(AC3_frame_buffer, 0, AC3_SPDIF_FRAME_SIZE);
			encoder->iAC3FrameSize = aften_encode_frame(&encoder->m_aftenContext, &AC3_frame_buffer[8], sample_buffer, AC3_SAMPLES_PER_FRAME);
			
			AC3_frame_buffer[1] = 0x72; /* sync words */
			AC3_frame_buffer[0] = 0xF8;
			AC3_frame_buffer[3] = 0x1F;
			AC3_frame_buffer[2] = 0x4E;
			AC3_frame_buffer[5] = 0x01;
			AC3_frame_buffer[4] = AC3_frame_buffer[5] & 0x7; /* bsmod */
			AC3_frame_buffer[7] = (encoder->iAC3FrameSize << 3) & 0xFF;
			AC3_frame_buffer[6] = (encoder->iAC3FrameSize >> 5) & 0xFF;
			
			//swabdata((char*)AC3_frame_buffer+8, (char*)oldendian, encoder->iAC3FrameSize);
			
			if(encoder->iAC3FrameSize < 0) 
			{
				fprintf(stderr, "Error encoding frame\n");
				break;
			} 
			else 
			{
				if (encoder->iAC3FrameSize > 0)
				{
					encoder->got_fs_once = 1;// means encoder started outputting samples.
				}
				encoder->last_frame = encoder->irawSampleBytesRead;
			}
		} while (!encoder->got_fs_once);
		rb_write(encoder->m_outputBuffer, AC3_frame_buffer, AC3_SPDIF_FRAME_SIZE);
		free(sample_buffer);
	}
	return ac3encoder_get_AC3_samplecount(encoder);
}

// returns number of samples in input buffer
int ac3coder_get_PCM_samplecount(struct AC3Encoder *encoder)
{
	int samplecount = rb_data_size(encoder->m_inputBuffer) / encoder->m_aftenContext.channels / (encoder->m_iSampleSize / 8);
	return samplecount;
}

// returns number of available AC3 samples
int ac3encoder_get_AC3_samplecount(struct AC3Encoder *encoder)
{
	int available_samples = rb_data_size(encoder->m_outputBuffer) / (SPDIF_SAMPLESIZE / 8) / SPDIF_CHANNELS;

	return available_samples;
}

// returms number of samples
int ac3encoder_get_encoded_samples(struct AC3Encoder *encoder, unsigned char *encoded_samples, int samples_out)
{
	if (samples_out > ac3encoder_get_AC3_samplecount(encoder))
	{
		return -1;
	}
	else
	{
		int samples_read = -1;
		samples_read = rb_read(encoder->m_outputBuffer, encoded_samples, samples_out * SPDIF_CHANNELS * (SPDIF_SAMPLESIZE / 8));
		return samples_read / SPDIF_CHANNELS / (SPDIF_SAMPLESIZE / 8);
	}
}
							 
int ac3encoder_channelcount(struct AC3Encoder *encoder)
{
	return encoder->m_aftenContext.channels;
}

void ac3encoder_flush(struct AC3Encoder *encoder)
{
    //while (encode_frame(..., NULL));
}
			
void ac3encoder_free(struct AC3Encoder *encoder)
{
	rb_free(encoder->m_inputBuffer);
	rb_free(encoder->m_outputBuffer);
#warning need to flush?
	aften_encode_close(&encoder->m_aftenContext);
}
					
					

