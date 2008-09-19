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

#include "DVDStreamInfo.h"

#include "DVDCodecs/DVDCodecs.h"
#include "DVDDemuxers/DVDDemux.h"

CDVDStreamInfo::CDVDStreamInfo()                                                     { extradata = NULL; Clear(); }
CDVDStreamInfo::CDVDStreamInfo(const CDVDStreamInfo &right, bool withextradata )     { extradata = NULL; Clear(); Assign(right, withextradata); }
CDVDStreamInfo::CDVDStreamInfo(const CDemuxStream &right, bool withextradata )       { extradata = NULL; Clear(); Assign(right, withextradata); }

CDVDStreamInfo::~CDVDStreamInfo()
{
  if( extradata && extrasize ) free(extradata);

  extradata = NULL;
  extrasize = 0;
  forPreview = false;
}


void CDVDStreamInfo::Clear()
{
  codec = CODEC_ID_NONE;
  type = STREAM_NONE;

  if( extradata && extrasize ) free(extradata);

  extradata = NULL;
  extrasize = 0;

  fpsscale = 0;
  fpsrate = 0;
  height = 0;
  width = 0;
  aspect = 0.0;

  channels = 0;
  samplerate = 0;
  blockalign = 0;
  bitrate = 0;
  forPreview = false;

  identifier = 0;
}

bool CDVDStreamInfo::Equal(const CDVDStreamInfo& right, bool withextradata)
{
  if( codec != right.codec
  ||  type != right.type ) return false;

  if( withextradata )
  {
    if( extrasize != right.extrasize ) return false;
    if( extrasize )
    {
      if( memcmp(extradata, right.extradata, extrasize) != 0 ) return false;
    }
  }

  // VIDEO
  if( fpsscale != right.fpsscale
  ||  fpsrate != right.fpsrate
  ||  height != right.height
  ||  width != right.width) return false;

  // AUDIO
  if( channels != right.channels
  ||  samplerate != right.samplerate 
  ||  blockalign != right.blockalign
  ||  bitrate != right.bitrate) return false;

  // SUBTITLE
  if( identifier != right.identifier ) return false;

  return true;
}

bool CDVDStreamInfo::Equal(const CDemuxStream& right, bool withextradata)
{
  CDVDStreamInfo info;
  info.Assign(right, withextradata);
  return Equal(info, withextradata);
}


// ASSIGNMENT
void CDVDStreamInfo::Assign(const CDVDStreamInfo& right, bool withextradata)
{
  codec = right.codec;
  type = right.type;

  if( extradata && extrasize ) free(extradata);

  if( withextradata && right.extrasize )
  {
    extrasize = right.extrasize;
    extradata = malloc(extrasize);
    memcpy(extradata, right.extradata, extrasize);
  }
  else
  {
    extrasize = 0;
    extradata = 0;
  }

  // VIDEO
  fpsscale = right.fpsscale;
  fpsrate = right.fpsrate;
  height = right.height;
  width = right.width;
  aspect = right.aspect;

  // AUDIO
  channels = right.channels;
  samplerate = right.samplerate;
  blockalign = right.blockalign;
  bitrate = right.bitrate;

  // SUBTITLE
  identifier = right.identifier;
  
  forPreview = right.forPreview;
}

void CDVDStreamInfo::Assign(const CDemuxStream& right, bool withextradata)
{
  Clear();

  codec = right.codec;
  type = right.type;

  if( withextradata && right.ExtraSize )
  {
    extrasize = right.ExtraSize;
    extradata = malloc(extrasize);
    memcpy(extradata, right.ExtraData, extrasize);
  }

  if( right.type == STREAM_AUDIO )
  {
    const CDemuxStreamAudio *stream = static_cast<const CDemuxStreamAudio*>(&right);
    channels = stream->iChannels;
    samplerate = stream->iSampleRate;
    blockalign = stream->iBlockAlign;
    bitrate = stream->iBitRate;
  }
  else if(  right.type == STREAM_VIDEO )
  {
    const CDemuxStreamVideo *stream = static_cast<const CDemuxStreamVideo*>(&right);
    fpsscale = stream->iFpsScale;
    fpsrate = stream->iFpsRate;
    height = stream->iHeight;
    width = stream->iWidth;
    aspect = stream->fAspect;
  }
  else if(  right.type == STREAM_SUBTITLE )
  {
    const CDemuxStreamSubtitle *stream = static_cast<const CDemuxStreamSubtitle*>(&right);
    identifier = stream->identifier;
  }
  
  forPreview = false;
}
