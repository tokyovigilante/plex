#pragma once

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

class CDVDInputStream;

#ifndef _LINUX
enum CodecID;
#else
#ifdef __APPLE__
extern "C" {
#include "libffmpeg-OSX/log.h"
#include "libffmpeg-OSX/avcodec.h"
}
#else
#include "../../ffmpeg/avcodec.h"
#endif
#endif
enum AVDiscard;

enum StreamType
{
  STREAM_NONE,    // if unknown
  STREAM_AUDIO,   // audio stream
  STREAM_VIDEO,   // video stream
  STREAM_DATA,    // data stream
  STREAM_SUBTITLE // subtitle stream
};

enum StreamSource {
  STREAM_SOURCE_NONE          = 0x000,
  STREAM_SOURCE_DEMUX         = 0x100, 
  STREAM_SOURCE_NAV           = 0x200,
  STREAM_SOURCE_DEMUX_SUB     = 0x300,
  STREAM_SOURCE_TEXT          = 0x400
};

#define STREAM_SOURCE_MASK(a) ((a) & 0xf00)

/*
 * CDemuxStream
 * Base class for all demuxer streams
 */
class CDemuxStream
{
public:
  CDemuxStream()
  {
    iId = 0;
    iPhysicalId = 0;
    codec = (CodecID)0; // CODEC_ID_NONE
    type = STREAM_NONE;
    source = STREAM_SOURCE_NONE;
    iDuration = 0;
    pPrivate = NULL;
    ExtraData = NULL;
    ExtraSize = 0;
    language[0] = 0;
    disabled = false;
  }

  virtual ~CDemuxStream() {}

  virtual void GetStreamInfo(std::string& strInfo)
  {
    strInfo = "";
  }

  virtual void GetStreamName(std::string& strInfo);

  virtual void      SetDiscard(AVDiscard discard);
  virtual AVDiscard GetDiscard();

  int iId;         // most of the time starting from 0
  int iPhysicalId; // id
  CodecID codec;
  StreamType type;
  int source;

  int iDuration; // in mseconds
  void* pPrivate; // private pointer for the demuxer
  void* ExtraData; // extra data for codec to use
  unsigned int ExtraSize; // size of extra data

  char language[4]; // ISO 639 3-letter language code (empty string if undefined)
  bool disabled; // set when stream is disabled. (when no decoder exists)
};

class CDemuxStreamVideo : public CDemuxStream
{
public:
  CDemuxStreamVideo() : CDemuxStream()
  {
    iFpsScale = 0;
    iFpsRate = 0;
    iHeight = 0;
    iWidth = 0;
    fAspect = 0.0;
    type = STREAM_VIDEO;
  }

  virtual ~CDemuxStreamVideo() {}
  int iFpsScale; // scale of 1000 and a rate of 29970 will result in 29.97 fps
  int iFpsRate;
  int iHeight; // height of the stream reported by the demuxer
  int iWidth; // width of the stream reported by the demuxer
  float fAspect; // display aspect of stream
};

class CDemuxStreamAudio : public CDemuxStream
{
public:
  CDemuxStreamAudio() : CDemuxStream()
  {
    iChannels = 0;
    iSampleRate = 0;
    iBlockAlign = 0;
    iBitRate = 0;
    type = STREAM_AUDIO;
  }

  virtual ~CDemuxStreamAudio() {}

  void GetStreamType(std::string& strInfo);

  int iChannels;
  int iSampleRate;
  int iBlockAlign;
  int iBitRate;
};

class CDemuxStreamSubtitle : public CDemuxStream
{
public:
  CDemuxStreamSubtitle() : CDemuxStream()
  {
    identifier = 0;
    type = STREAM_SUBTITLE;
  }

  int identifier;
};

typedef struct DemuxPacket
{
  BYTE* pData;   // data
  int iSize;     // data size
  int iStreamId; // integer representing the stream index
  int iGroupId;  // the group this data belongs to, used to group data from different streams together
  
  double pts; // pts in DVD_TIME_BASE
  double dts; // dts in DVD_TIME_BASE
  double duration; // duration in DVD_TIME_BASE if available
} DemuxPacket;


class CDVDDemux
{
public:

  CDVDDemux() {}
  virtual ~CDVDDemux() {}
  
  
  /*
   * Reset the entire demuxer (same result as closing and opening it)
   */
  virtual void Reset() = 0;
  
  /*
   * Aborts any internal reading that might be stalling main thread
   * NOTICE - this can be called from another thread
   */
  virtual void Abort() = 0;

  /*
   * Flush the demuxer, if any data is kept in buffers, this should be freed now
   */
  virtual void Flush() = 0;
  
  /*
   * Read a packet, returns NULL on error
   * 
   */
  virtual DemuxPacket* Read() = 0;
  
  /*
   * Seek, time in msec calculated from stream start
   */
  virtual bool SeekTime(int time, bool backwords = false, double* startpts = NULL) = 0;

  /*
   * Seek to a specified chapter.
   * startpts can be updated to the point where display should start 
   */
  virtual bool SeekChapter(int chapter, double* startpts = NULL) { return false; }

  /*
   * Get the number of chapters available
   */
  virtual int GetChapterCount() { return 0; }

  /*
   * Get current chapter 
   */
  virtual int GetChapter() { return -1; }

  /*
   * Set the playspeed, if demuxer can handle different
   * speeds of playback
   */
  virtual void SetSpeed(int iSpeed) = 0;

  /*
   * returns the total time in msec
   */
  virtual int GetStreamLength() = 0;
  
  /*
   * returns the stream or NULL on error, starting from 0
   */
  virtual CDemuxStream* GetStream(int iStreamId) = 0;
  
  /*
   * return nr of streams, 0 if none
   */
  virtual int GetNrOfStreams() = 0;
  
  /*
   * returns opened filename
   */
  virtual std::string GetFileName() = 0;
  /*
   * return nr of audio streams, 0 if none
   */
  int GetNrOfAudioStreams();
  
  /*
   * return nr of video streams, 0 if none
   */
  int GetNrOfVideoStreams();
  
  /*
   * return nr of subtitle streams, 0 if none
   */
  int GetNrOfSubtitleStreams();

  /*
   * return the audio stream, or NULL if it does not exist
   */
  CDemuxStreamAudio* GetStreamFromAudioId(int iAudioIndex);

  /*
   * return the video stream, or NULL if it does not exist
   */
  CDemuxStreamVideo* GetStreamFromVideoId(int iVideoIndex);
  
  /*
   * return the subtitle stream, or NULL if it does not exist
   */
  CDemuxStreamSubtitle* GetStreamFromSubtitleId(int iSubtitleIndex);
  
};
