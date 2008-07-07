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

#include "IMsgTargetCallback.h"

#define PLAYLIST_NONE    -1
#define PLAYLIST_MUSIC   0
#define PLAYLIST_VIDEO   1

class CFileItemList;

namespace PLAYLIST
{
/*!
 \ingroup windows 
 \brief Manages playlist playing.
 */
enum REPEAT_STATE { REPEAT_NONE = 0, REPEAT_ONE, REPEAT_ALL };

class CPlayList;
class CPlayListItem;

class CPlayListPlayer : public IMsgTargetCallback
{

public:
  CPlayListPlayer(void);
  virtual ~CPlayListPlayer(void);
  virtual bool OnMessage(CGUIMessage &message);
  void PlayNext(bool bAutoPlay = false);
  void PlayPrevious();
  void Play();
  void Play(int iSong, bool bAutoPlay = false, bool bPlayPrevious = false);
  int GetCurrentSong() const;
  int GetNextSong();
  void SetCurrentSong(int iSong);
  bool HasChanged();
  void SetCurrentPlaylist(int iPlaylist);
  int GetCurrentPlaylist();
  CPlayList& GetPlaylist(int iPlaylist);
  int RemoveDVDItems();
  void Reset();
  void ClearPlaylist(int iPlaylist);
  void Clear();
  void SetShuffle(int iPlaylist, bool bYesNo);
  bool IsShuffled(int iPlaylist);
  bool HasPlayedFirstFile();
  
  void SetRepeat(int iPlaylist, REPEAT_STATE state);
  REPEAT_STATE GetRepeat(int iPlaylist);

  // add items via the playlist player
  void Add(int iPlaylist, CPlayListItem& item);
  void Add(int iPlaylist, CPlayList& playlist);
  void Add(int iPlaylist, CFileItem *pItem);
  void Add(int iPlaylist, CFileItemList& items);

protected:
  bool Repeated(int iPlaylist);
  bool RepeatedOne(int iPlaylist);
  void ReShuffle(int iPlaylist, int iPosition);
  bool m_bChanged;
  bool m_bPlayedFirstFile;
  int m_iFailedSongs;
  int m_iCurrentSong;
  int m_iCurrentPlayList;
  CPlayList* m_PlaylistMusic;
  CPlayList* m_PlaylistVideo;
  CPlayList* m_PlaylistEmpty;
  REPEAT_STATE m_repeatState[2];
};

}

/*!
 \ingroup windows 
 \brief Global instance of playlist player
 */
extern PLAYLIST::CPlayListPlayer g_playlistPlayer;
