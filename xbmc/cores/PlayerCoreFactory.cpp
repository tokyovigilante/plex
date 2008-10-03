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
#include "utils/BitstreamStats.h"
#include "PlayerCoreFactory.h"
#include "dvdplayer/DVDPlayer.h"
#include "paplayer/paplayer.h"
#include "paplayer/DVDPlayerCodec.h"
#include "GUIDialogContextMenu.h"
#include "XBAudioConfig.h"
#include "FileSystem/FileCurl.h"
#include "utils/HttpHeader.h"
#include "GUISettings.h"
#include "URL.h"
#include "GUIWindowManager.h"
#include "FileItem.h"

using namespace AUTOPTR;

CPlayerCoreFactory::CPlayerCoreFactory()
{}
CPlayerCoreFactory::~CPlayerCoreFactory()
{}

/* generic function to make a vector unique, removes later duplicates */
template<typename T> void unique (T &con)
{
  typename T::iterator cur, end;
  cur = con.begin();
  end = con.end();
  while (cur != end)
  {
    typename T::value_type i = *cur;
    end = remove (++cur, end, i);
  }
  con.erase (end, con.end());
} 

IPlayer* CPlayerCoreFactory::CreatePlayer(const CStdString& strCore, IPlayerCallback& callback) const
{ 
  return CreatePlayer( GetPlayerCore(strCore), callback ); 
}

IPlayer* CPlayerCoreFactory::CreatePlayer(const EPLAYERCORES eCore, IPlayerCallback& callback)
{
  switch( eCore )
  {
    case EPC_MPLAYER:
    case EPC_DVDPLAYER: return new CDVDPlayer(callback);
    case EPC_PAPLAYER: return new PAPlayer(callback); // added by dataratt

    default:
       return NULL; 
  }  
}

EPLAYERCORES CPlayerCoreFactory::GetPlayerCore(const CStdString& strCore)
{
  CStdString strCoreLower = strCore;
  strCoreLower.ToLower();

  if (strCoreLower == "dvdplayer" || strCoreLower == "mplayer") return EPC_DVDPLAYER;
  if (strCoreLower == "paplayer" ) return EPC_PAPLAYER;
  return EPC_NONE;
}

CStdString CPlayerCoreFactory::GetPlayerName(const EPLAYERCORES eCore)
{
  switch( eCore )
  {
    case EPC_DVDPLAYER: return "DVDPlayer";
    case EPC_PAPLAYER: return "PAPlayer";
    default: return "";
  }
}

void CPlayerCoreFactory::GetPlayers( VECPLAYERCORES &vecCores )
{
  vecCores.push_back(EPC_DVDPLAYER);
  vecCores.push_back(EPC_PAPLAYER);
}

void CPlayerCoreFactory::GetPlayers( const CFileItem& item, VECPLAYERCORES &vecCores)
{
  CURL url(item.m_strPath);

  CLog::Log(LOGDEBUG,"CPlayerCoreFactor::GetPlayers(%s)",item.m_strPath.c_str());

  // ugly hack for ReplayTV. our filesystem is broken against real ReplayTV's (not the psuevdo DVArchive)
  // it breaks down for small requests. As we can't allow truncated reads for all emulated dll file functions
  // we are often forced to do small reads to fill up the full buffer size wich seems gives garbage back
  if (url.GetProtocol().Equals("rtv"))
    vecCores.push_back(EPC_MPLAYER); // vecCores.push_back(EPC_DVDPLAYER);

  if (url.GetProtocol().Equals("hdhomerun")
  ||  url.GetProtocol().Equals("myth")
  ||  url.GetProtocol().Equals("cmyth")
/*  ||  url.GetProtocol().Equals("rtsp")*/)

    vecCores.push_back(EPC_DVDPLAYER);

  if (url.GetProtocol().Equals("lastfm") ||
      url.GetProtocol().Equals("shout"))
  {
    vecCores.push_back(EPC_PAPLAYER);
  }
   
  if (url.GetProtocol().Equals("mms"))
  {
    vecCores.push_back(EPC_DVDPLAYER);    
  }

  // dvdplayer can play standard rtsp streams
  if (url.GetProtocol().Equals("rtsp") 
  && !url.GetFileType().Equals("rm") 
  && !url.GetFileType().Equals("ra"))
    vecCores.push_back(EPC_DVDPLAYER);

  // only dvdplayer can handle these normally
  if (url.GetFileType().Equals("sdp") ||
      url.GetFileType().Equals("asf"))
    vecCores.push_back(EPC_DVDPLAYER);

  if ( item.IsInternetStream() )
  {
    CStdString content = item.GetContentType();

    if (content == "video/x-flv"
     || content == "video/flv")
      vecCores.push_back(EPC_DVDPLAYER);
    else if (content == "audio/aacp")
      vecCores.push_back(EPC_DVDPLAYER);
    else if (content == "application/sdp")
      vecCores.push_back(EPC_DVDPLAYER);
    else if (content == "application/octet-stream")
    {
      //unknown contenttype, send mp2 to pap
      if( url.GetFileType() == "mp2")
        vecCores.push_back(EPC_PAPLAYER);
    }
  }

  if (((item.IsDVD()) || item.IsDVDFile() || item.IsDVDImage()))
  {
    vecCores.push_back(EPC_DVDPLAYER);
  }

  if( PAPlayer::HandlesType(url.GetFileType()) )
  {
    bool bAdd = true;
    if (url.GetProtocol().Equals("mms"))
    {
       bAdd = false;
    }
    else if (item.IsType(".wma"))
    {
      DVDPlayerCodec codec;
      if (!codec.Init(item.m_strPath,2048))
        bAdd = false;
      codec.DeInit();        
    }

    if (bAdd)
    {
      if( g_guiSettings.GetInt("audiooutput.mode") == AUDIO_ANALOG )
      {
        vecCores.push_back(EPC_PAPLAYER);
      }
#ifdef __APPLE__
	  else if( ( url.GetFileType().Equals("ac3") && g_guiSettings.GetBool("audiooutput.ac3passthrough"))	
#else
      else if( ( url.GetFileType().Equals("ac3") && g_audioConfig.GetAC3Enabled() )
#endif
        ||  ( url.GetFileType().Equals("dts") && g_audioConfig.GetDTSEnabled() ) ) 
      {
        vecCores.push_back(EPC_DVDPLAYER);
      }
      else
      {
        vecCores.push_back(EPC_PAPLAYER);
      }
    }
  }

  //Add all normal players last so you can force them, should you want to
  if ( item.IsAudio() )
    vecCores.push_back(EPC_PAPLAYER);

  vecCores.push_back(EPC_DVDPLAYER);

  /* make our list unique, presevering first added players */
  unique(vecCores);
}

EPLAYERCORES CPlayerCoreFactory::GetDefaultPlayer( const CFileItem& item )
{
  VECPLAYERCORES vecCores;
  GetPlayers(item, vecCores);

  //If we have any players return the first one
  if( vecCores.size() > 0 ) return vecCores.at(0);
  
  return EPC_NONE;
}

EPLAYERCORES CPlayerCoreFactory::SelectPlayerDialog(VECPLAYERCORES &vecCores, float posX, float posY)
{

  CGUIDialogContextMenu *pMenu = (CGUIDialogContextMenu *)m_gWindowManager.GetWindow(WINDOW_DIALOG_CONTEXT_MENU);

  // Reset menu
  pMenu->Initialize();

  // Add all possible players
  auto_aptr<int> btn_Cores(NULL);
  if( vecCores.size() > 0 )
  {    
    btn_Cores = new int[ vecCores.size() ];
    btn_Cores[0] = 0;

    CStdString strCaption;

    //Add default player
    strCaption = CPlayerCoreFactory::GetPlayerName(vecCores[0]);
    strCaption += " (";
    strCaption += g_localizeStrings.Get(13278);
    strCaption += ")";
    btn_Cores[0] = pMenu->AddButton(strCaption);

    //Add all other players
    for( unsigned int i = 1; i < vecCores.size(); i++ )
    {
      strCaption = CPlayerCoreFactory::GetPlayerName(vecCores[i]);
      btn_Cores[i] = pMenu->AddButton(strCaption);
    }
  }

  // Display menu
  if (posX && posY)
    pMenu->SetPosition(posX - pMenu->GetWidth() / 2, posY - pMenu->GetHeight() / 2);
  else
    pMenu->CenterWindow();
  pMenu->DoModal();

  //Check what player we selected
  int btnid = pMenu->GetButton();
  for( unsigned int i = 0; i < vecCores.size(); i++ )
  {
    if( btnid == btn_Cores[i] )
    {
      return vecCores[i];
    }
  }

  return EPC_NONE;
}

EPLAYERCORES CPlayerCoreFactory::SelectPlayerDialog(float posX, float posY)
{
  VECPLAYERCORES vecCores;
  GetPlayers(vecCores);
  return SelectPlayerDialog(vecCores, posX, posY);
}
