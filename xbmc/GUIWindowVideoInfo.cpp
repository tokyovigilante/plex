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
#include "GUIWindow.h"
#include "GUIWindowVideoInfo.h"
#include "Util.h"
#include "Picture.h"
#include "guiImage.h"
#include "StringUtils.h"
#include "GUIWindowVideoBase.h"
#include "GUIWindowVideoFiles.h"
#include "GUIDialogFileBrowser.h"
#include "utils/GUIInfoManager.h"
#include "VideoInfoScanner.h"
#include "Application.h"
#include "VideoInfoTag.h"
#include "GUIWindowManager.h"
#include "GUIDialogOK.h"
#include "GUIDialogYesNo.h"
#include "GUIDialogSelect.h"
#include "GUIDialogProgress.h"
#include "FileSystem/Directory.h"
#include "FileSystem/File.h"
#include "FileItem.h"

using namespace std;
using namespace XFILE;

#define CONTROL_TITLE               20
#define CONTROL_DIRECTOR            21
#define CONTROL_CREDITS             22
#define CONTROL_GENRE               23
#define CONTROL_YEAR                24
#define CONTROL_TAGLINE             25
#define CONTROL_PLOTOUTLINE         26
#define CONTROL_CAST                29
#define CONTROL_RATING_AND_VOTES    30
#define CONTROL_RUNTIME             31
#define CONTROL_MPAARATING          32
#define CONTROL_TITLE_AND_YEAR      33
#define CONTROL_STUDIO              36
#define CONTROL_TOP250              37
#define CONTROL_TRAILER             38


#define CONTROL_IMAGE                3
#define CONTROL_TEXTAREA             4
#define CONTROL_BTN_TRACKS           5
#define CONTROL_BTN_REFRESH          6
#define CONTROL_DISC                 7
#define CONTROL_BTN_PLAY             8
#define CONTROL_BTN_RESUME           9
#define CONTROL_BTN_GET_THUMB       10
#define CONTROL_BTN_PLAY_TRAILER    11
#define CONTROL_BTN_GET_FANART      12

#define CONTROL_LIST                50

CGUIWindowVideoInfo::CGUIWindowVideoInfo(void)
    : CGUIDialog(WINDOW_VIDEO_INFO, "DialogVideoInfo.xml")
{
  m_bRefreshAll = true;
  m_bRefresh = false;
  m_movieItem = new CFileItem;
  m_castList = new CFileItemList;
}

CGUIWindowVideoInfo::~CGUIWindowVideoInfo(void)
{
  delete m_movieItem;
  delete m_castList;
}

bool CGUIWindowVideoInfo::OnMessage(CGUIMessage& message)
{
  switch ( message.GetMessage() )
  {
  case GUI_MSG_WINDOW_DEINIT:
    {
      ClearCastList();
      m_database.Close();
    }
    break;

  case GUI_MSG_WINDOW_INIT:
    {
      m_database.Open();
      m_dlgProgress = (CGUIDialogProgress*)m_gWindowManager.GetWindow(WINDOW_DIALOG_PROGRESS);

      m_bRefresh = false;
      m_bRefreshAll = true;
      CGUIDialog::OnMessage(message);
      m_bViewReview = true;
      CGUIMessage msg(GUI_MSG_LABEL_RESET, GetID(), CONTROL_DISC);
      OnMessage(msg);
      for (int i = 0; i < 1000; ++i)
      {
        CStdString strItem;
        strItem.Format("DVD#%03i", i);
        CGUIMessage msg2(GUI_MSG_LABEL_ADD, GetID(), CONTROL_DISC);
        msg2.SetLabel(strItem);
        OnMessage(msg2);
      }

      SET_CONTROL_HIDDEN(CONTROL_DISC);
/*      CONTROL_DISABLE(CONTROL_DISC);
      int iItem = 0;
      CFileItem movie(m_Movie.m_strFileNameAndPath, false);
      if ( movie.IsOnDVD() )
      {
        SET_CONTROL_VISIBLE(CONTROL_DISC);
        CONTROL_ENABLE(CONTROL_DISC);
        char szNumber[1024];
        int iPos = 0;
        bool bNumber = false;
        for (int i = 0; i < (int)m_Movie.m_strDVDLabel.size();++i)
        {
          char kar = m_Movie.m_strDVDLabel.GetAt(i);
          if (kar >= '0' && kar <= '9' )
          {
            szNumber[iPos] = kar;
            iPos++;
            szNumber[iPos] = 0;
            bNumber = true;
          }
          else
          {
            if (bNumber) break;
          }
        }
        int iDVD = 0;
        if (strlen(szNumber))
        {
          int x = 0;
          while (szNumber[x] == '0' && x < (int)strlen(szNumber) ) x++;
          if (x < (int)strlen(szNumber))
          {
            sscanf(&szNumber[x], "%i", &iDVD);
            if (iDVD < 0 && iDVD >= 1000)
              iDVD = -1;
          }
        }
        if (iDVD <= 0) iDVD = 0;
        iItem = iDVD;

        CGUIMessage msgSet(GUI_MSG_ITEM_SELECT, GetID(), CONTROL_DISC, iItem, 0, NULL);
        OnMessage(msgSet);
      }*/
      Refresh();

      // dont allow refreshing of manual info
      if (m_movieItem->GetVideoInfoTag()->m_strIMDBNumber.Left(2).Equals("xx"))
        CONTROL_DISABLE(CONTROL_BTN_REFRESH);
      // dont allow get thumb for plugin entries
      if (m_movieItem->GetVideoInfoTag()->m_strIMDBNumber.Mid(2).Equals("plugin"))
        CONTROL_DISABLE(CONTROL_BTN_GET_THUMB);
      return true;
    }
    break;


  case GUI_MSG_CLICKED:
    {
      int iControl = message.GetSenderId();
      if (iControl == CONTROL_BTN_REFRESH)
      {
        if (m_movieItem->GetVideoInfoTag()->m_iSeason < 0 && !m_movieItem->GetVideoInfoTag()->m_strShowTitle.IsEmpty()) // tv show
        {
          bool bCanceled=false;
          if (CGUIDialogYesNo::ShowAndGetInput(20377,20378,-1,-1,bCanceled))
          {
            m_bRefreshAll = true;
            m_database.SetPathHash(m_movieItem->GetVideoInfoTag()->m_strPath,"");
          }
          else
            m_bRefreshAll = false;

          if (bCanceled)
            return false;
        }
        m_bRefresh = true;
        Close();
        return true;
      }
      else if (iControl == CONTROL_BTN_TRACKS)
      {
        m_bViewReview = !m_bViewReview;
        Update();
      }
      else if (iControl == CONTROL_BTN_PLAY)
      {
        Play();
      }
      else if (iControl == CONTROL_BTN_RESUME)
      {
        Play(true);
      }
      else if (iControl == CONTROL_BTN_GET_THUMB)
      {
        OnGetThumb();
      }
      else if (iControl == CONTROL_BTN_PLAY_TRAILER)
      {
        PlayTrailer();
      }
      else if (iControl == CONTROL_BTN_GET_FANART)
      {
        OnGetFanart();
      }
/*      else if (iControl == CONTROL_DISC)
      {
        int iItem = 0;
        CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), iControl, 0, 0, NULL);
        OnMessage(msg);
        CStdString strItem = msg.GetLabel();
        if (strItem != "HD" && strItem != "share")
        {
          long lMovieId;
          sscanf(m_Movie.m_strSearchString.c_str(), "%i", &lMovieId);
          if (lMovieId > 0)
          {
            CStdString label;
            //m_database.GetDVDLabel(lMovieId, label);
            int iPos = label.Find("DVD#");
            if (iPos >= 0)
            {
              label.Delete(iPos, label.GetLength());
            }
            label = label.TrimRight(" ");
            label += " ";
            label += strItem;
            //m_database.SetDVDLabel( lMovieId, label);
          }
        }
      }*/
      else if (iControl == CONTROL_LIST)
      {
        int iAction = message.GetParam1();
        if (ACTION_SELECT_ITEM == iAction)
        {
          CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), iControl);
          OnMessage(msg);
          int iItem = msg.GetParam1();
          if (iItem < 0 || iItem >= m_castList->Size())
            break;
          CStdString strItem = m_castList->Get(iItem)->GetLabel();
          CStdString strFind; 
          strFind.Format(" %s ",g_localizeStrings.Get(20347));
          int iPos = strItem.Find(strFind);
          if (iPos == -1)
            iPos = strItem.size();
          CStdString tmp = strItem.Left(iPos);
          OnSearch(tmp);
        }
      }
    }
    break;
  }

  return CGUIDialog::OnMessage(message);
}

void CGUIWindowVideoInfo::SetMovie(const CFileItem *item)
{
  *m_movieItem = *item;
}

void CGUIWindowVideoInfo::Update()
{
  CStdString strTmp;
  strTmp = m_movieItem->GetVideoInfoTag()->m_strTitle; strTmp.Trim();
  SetLabel(CONTROL_TITLE, strTmp);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strDirector; strTmp.Trim();
  SetLabel(CONTROL_DIRECTOR, strTmp);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strStudio; strTmp.Trim();
  SetLabel(CONTROL_STUDIO, strTmp);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strWritingCredits; strTmp.Trim();
  SetLabel(CONTROL_CREDITS, strTmp);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strGenre; strTmp.Trim();
  SetLabel(CONTROL_GENRE, strTmp);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strTagLine; strTmp.Trim();
  SetLabel(CONTROL_TAGLINE, strTmp);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strPlotOutline; strTmp.Trim();
  SetLabel(CONTROL_PLOTOUTLINE, strTmp);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strTrailer; strTmp.Trim();
  SetLabel(CONTROL_TRAILER, strTmp);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strMPAARating; strTmp.Trim();
  SetLabel(CONTROL_MPAARATING, strTmp);

  CStdString strTop250;
  if (m_movieItem->GetVideoInfoTag()->m_iTop250)
    strTop250.Format("%i", m_movieItem->GetVideoInfoTag()->m_iTop250);
  SetLabel(CONTROL_TOP250, strTop250);

  CStdString strYear;
  if (m_movieItem->GetVideoInfoTag()->m_iYear)
    strYear.Format("%i", m_movieItem->GetVideoInfoTag()->m_iYear);
  else  
    strYear = g_infoManager.GetItemLabel(m_movieItem,LISTITEM_PREMIERED);
  SetLabel(CONTROL_YEAR, strYear);

  CStdString strRating_And_Votes;
  if (m_movieItem->GetVideoInfoTag()->m_fRating != 0.0f)  // only non-zero ratings are of interest
    strRating_And_Votes.Format("%03.1f (%s %s)", m_movieItem->GetVideoInfoTag()->m_fRating, m_movieItem->GetVideoInfoTag()->m_strVotes, g_localizeStrings.Get(20350));
  SetLabel(CONTROL_RATING_AND_VOTES, strRating_And_Votes);

  strTmp = m_movieItem->GetVideoInfoTag()->m_strRuntime; strTmp.Trim();
  SetLabel(CONTROL_RUNTIME, strTmp);

  // setup plot text area
  strTmp = m_movieItem->GetVideoInfoTag()->m_strPlot;
  if (!(!m_movieItem->GetVideoInfoTag()->m_strShowTitle.IsEmpty() && m_movieItem->GetVideoInfoTag()->m_iSeason == 0)) // dont apply to tvshows
    if (m_movieItem->GetVideoInfoTag()->m_playCount == 0 && g_guiSettings.GetBool("videolibrary.hideplots"))
      strTmp = g_localizeStrings.Get(20370);

  strTmp.Trim();
  SetLabel(CONTROL_TEXTAREA, strTmp);

  // setup cast list + determine type
  ClearCastList();
  if (!m_movieItem->GetVideoInfoTag()->m_strArtist.IsEmpty())
  { // music video
    CStdStringArray artists;
    StringUtils::SplitString(m_movieItem->GetVideoInfoTag()->m_strArtist, g_advancedSettings.m_videoItemSeparator, artists);
    for (std::vector<CStdString>::const_iterator it = artists.begin(); it != artists.end(); ++it)
    {
      CFileItem *item = new CFileItem(*it);
      if (CFile::Exists(item->GetCachedArtistThumb()))
        item->SetThumbnailImage(item->GetCachedArtistThumb());
      item->SetIconImage("DefaultArtist.png");
      m_castList->Add(item);
    }
    m_castList->SetContent("musicvideos");
  }
  else
  { // movie/show/episode
    for (CVideoInfoTag::iCast it = m_movieItem->GetVideoInfoTag()->m_cast.begin(); it != m_movieItem->GetVideoInfoTag()->m_cast.end(); ++it)
    {
      CStdString character;
      if (it->strRole.IsEmpty())
        character = it->strName;
      else
        character.Format("%s %s %s", it->strName.c_str(), g_localizeStrings.Get(20347).c_str(), it->strRole.c_str());
      CFileItem *item = new CFileItem(it->strName);
      if (CFile::Exists(item->GetCachedActorThumb()))
        item->SetThumbnailImage(item->GetCachedActorThumb());
      item->SetIconImage("DefaultActor.png");
      item->SetLabel(character);
      m_castList->Add(item);
    }
    // determine type:
    if (m_movieItem->m_bIsFolder)
      m_castList->SetContent("tvshows");
    else if (m_movieItem->GetVideoInfoTag()->m_iSeason > -1)
      m_castList->SetContent("episodes");
    else
      m_castList->SetContent("movies");
  }
  CGUIMessage msg(GUI_MSG_LABEL_BIND, GetID(), CONTROL_LIST, 0, 0, m_castList);
  OnMessage(msg);

  if (m_bViewReview)
  {
    if (!m_movieItem->GetVideoInfoTag()->m_strArtist.IsEmpty())
    {
      SET_CONTROL_LABEL(CONTROL_BTN_TRACKS, 133);
    }
    else
    {
      SET_CONTROL_LABEL(CONTROL_BTN_TRACKS, 206);
    }

    SET_CONTROL_HIDDEN(CONTROL_LIST);
    SET_CONTROL_VISIBLE(CONTROL_TEXTAREA);
  }
  else
  {
    SET_CONTROL_LABEL(CONTROL_BTN_TRACKS, 207);

    SET_CONTROL_HIDDEN(CONTROL_TEXTAREA);
    SET_CONTROL_VISIBLE(CONTROL_LIST);
  }

  // Check for resumability
  CGUIWindowVideoFiles *window = (CGUIWindowVideoFiles *)m_gWindowManager.GetWindow(WINDOW_VIDEO_FILES);
  if (window && window->GetResumeItemOffset(m_movieItem) > 0)
  {
    CONTROL_ENABLE(CONTROL_BTN_RESUME);
  }
  else
  {
    CONTROL_DISABLE(CONTROL_BTN_RESUME);
  }

  if (m_movieItem->GetVideoInfoTag()->m_strEpisodeGuide.IsEmpty()) // disable the play button for tv show info
  {
    CONTROL_ENABLE(CONTROL_BTN_PLAY)
  }
  else
  {
    CONTROL_DISABLE(CONTROL_BTN_PLAY)
  }

  // update the thumbnail
  const CGUIControl* pControl = GetControl(CONTROL_IMAGE);
  if (pControl)
  {
    CGUIImage* pImageControl = (CGUIImage*)pControl;
    pImageControl->FreeResources();
    pImageControl->SetFileName(m_movieItem->GetThumbnailImage());
  }
}

void CGUIWindowVideoInfo::Refresh()
{
  // quietly return if Internet lookups are disabled
  if (!g_guiSettings.GetBool("network.enableinternet"))
  {
    Update();
    return ;
  }

  try
  {
    OutputDebugString("Refresh\n");

    CStdString strImage = m_movieItem->GetVideoInfoTag()->m_strPictureURL.GetFirstThumb().m_url;

    CStdString thumbImage = m_movieItem->GetThumbnailImage();
    if (!m_movieItem->HasThumbnail())
      thumbImage = m_movieItem->GetCachedVideoThumb();
    if (!CFile::Exists(thumbImage) && strImage.size() > 0)
    {
      CScraperUrl::DownloadThumbnail(thumbImage,m_movieItem->GetVideoInfoTag()->m_strPictureURL.GetFirstThumb());
      CUtil::DeleteVideoDatabaseDirectoryCache(); // to get them new thumbs to show
    }

    if (!CFile::Exists(thumbImage) )
    {
      thumbImage.Empty();
    }

    m_movieItem->SetThumbnailImage(thumbImage);

    //OutputDebugString("update\n");
    Update();
    //OutputDebugString("updated\n");
  }
  catch (...)
  {}
}
bool CGUIWindowVideoInfo::NeedRefresh() const
{
  return m_bRefresh;
}

bool CGUIWindowVideoInfo::RefreshAll() const
{
  return m_bRefreshAll;
}

/// \brief Search the current directory for a string got from the virtual keyboard
void CGUIWindowVideoInfo::OnSearch(CStdString& strSearch)
{
  if (m_dlgProgress)
  {
    m_dlgProgress->SetHeading(194);
    m_dlgProgress->SetLine(0, strSearch);
    m_dlgProgress->SetLine(1, "");
    m_dlgProgress->SetLine(2, "");
    m_dlgProgress->StartModal();
    m_dlgProgress->Progress();
  }
  CFileItemList items;
  DoSearch(strSearch, items);

  if (items.Size())
  {
    CGUIDialogSelect* pDlgSelect = (CGUIDialogSelect*)m_gWindowManager.GetWindow(WINDOW_DIALOG_SELECT);
    pDlgSelect->Reset();
    pDlgSelect->SetHeading(283);
    items.Sort(SORT_METHOD_LABEL, SORT_ORDER_ASC);

    for (int i = 0; i < (int)items.Size(); i++)
    {
      CFileItem* pItem = items[i];
      pDlgSelect->Add(pItem->GetLabel());
    }

    pDlgSelect->DoModal();

    int iItem = pDlgSelect->GetSelectedLabel();
    if (iItem < 0)
    {
      if (m_dlgProgress) m_dlgProgress->Close();
      return ;
    }

    CFileItem* pSelItem = new CFileItem(*items[iItem]);

    OnSearchItemFound(pSelItem);

    delete pSelItem;
    if (m_dlgProgress) m_dlgProgress->Close();
  }
  else
  {
    if (m_dlgProgress) m_dlgProgress->Close();
    CGUIDialogOK::ShowAndGetInput(194, 284, 0, 0);
  }
}

/// \brief Make the actual search for the OnSearch function.
/// \param strSearch The search string
/// \param items Items Found
void CGUIWindowVideoInfo::DoSearch(CStdString& strSearch, CFileItemList& items)
{
  VECMOVIES movies;
  m_database.GetMoviesByActor(strSearch, movies);
  for (int i = 0; i < (int)movies.size(); ++i)
  {
    CStdString strItem;
    strItem.Format("[%s] %s (%i)", g_localizeStrings.Get(20338), movies[i].m_strTitle, movies[i].m_iYear);  // Movie
    CFileItem *pItem = new CFileItem(strItem);
    *pItem->GetVideoInfoTag() = movies[i];
    pItem->m_strPath = movies[i].m_strFileNameAndPath;
    items.Add(pItem);
  }
  movies.clear();
  m_database.GetTvShowsByActor(strSearch, movies);
  for (int i = 0; i < (int)movies.size(); ++i)
  {
    CStdString strItem;
    strItem.Format("[%s] %s", g_localizeStrings.Get(20364), movies[i].m_strTitle);  // Movie
    CFileItem *pItem = new CFileItem(strItem);
    *pItem->GetVideoInfoTag() = movies[i];
    pItem->m_strPath.Format("videodb://1/%u",movies[i].m_iDbId);
    items.Add(pItem);
  }
  movies.clear();
  m_database.GetEpisodesByActor(strSearch, movies);
  for (int i = 0; i < (int)movies.size(); ++i)
  {
    CStdString strItem;
    strItem.Format("[%s] %s", g_localizeStrings.Get(20359), movies[i].m_strTitle);  // Movie
    CFileItem *pItem = new CFileItem(strItem);
    *pItem->GetVideoInfoTag() = movies[i];
    pItem->m_strPath = movies[i].m_strFileNameAndPath;
    items.Add(pItem);
  }

  CFileItemList mvids;
  m_database.GetMusicVideosByArtist(strSearch, mvids);
  for (int i = 0; i < (int)mvids.Size(); ++i)
  {
    CStdString strItem;
    strItem.Format("[%s] %s", g_localizeStrings.Get(20391), mvids[i]->GetVideoInfoTag()->m_strTitle);  // Movie
    CFileItem *pItem = new CFileItem(strItem);
    *pItem->GetVideoInfoTag() = *mvids[i]->GetVideoInfoTag();
    pItem->m_strPath = mvids[i]->GetVideoInfoTag()->m_strFileNameAndPath;
    items.Add(pItem);
  }
}

/// \brief React on the selected search item
/// \param pItem Search result item
void CGUIWindowVideoInfo::OnSearchItemFound(const CFileItem* pItem)
{
  int iType=0;
  if (pItem->HasVideoInfoTag() && !pItem->GetVideoInfoTag()->m_strShowTitle.IsEmpty()) // tvshow
    iType = 2;
  if (pItem->HasVideoInfoTag() && pItem->GetVideoInfoTag()->m_iSeason > -1 && !pItem->m_bIsFolder) // episode
    iType = 1;
  if (pItem->HasVideoInfoTag() && !pItem->GetVideoInfoTag()->m_strArtist.IsEmpty())
    iType = 3;

  CVideoInfoTag movieDetails;
  if (iType == 0)
    m_database.GetMovieInfo(pItem->m_strPath, movieDetails, pItem->GetVideoInfoTag()->m_iDbId);
  if (iType == 1)
    m_database.GetEpisodeInfo(pItem->m_strPath, movieDetails, pItem->GetVideoInfoTag()->m_iDbId);
  if (iType == 2)
    m_database.GetTvShowInfo(pItem->m_strPath, movieDetails, pItem->GetVideoInfoTag()->m_iDbId);
  if (iType == 3)
    m_database.GetMusicVideoInfo(pItem->m_strPath, movieDetails, pItem->GetVideoInfoTag()->m_iDbId);

  CFileItem item(*pItem);
  *item.GetVideoInfoTag() = movieDetails;
  SetMovie(&item);
  Refresh();
}

void CGUIWindowVideoInfo::ClearCastList()
{
  CGUIMessage msg(GUI_MSG_LABEL_RESET, GetID(), CONTROL_LIST);
  OnMessage(msg);
  m_castList->Clear();
}

void CGUIWindowVideoInfo::Play(bool resume)
{
  CFileItem movie(m_movieItem->GetVideoInfoTag()->m_strFileNameAndPath, false);
  if (m_movieItem->GetVideoInfoTag()->m_strFileNameAndPath.IsEmpty())
    movie.m_strPath = m_movieItem->m_strPath;
  CGUIWindowVideoFiles* pWindow = (CGUIWindowVideoFiles*)m_gWindowManager.GetWindow(WINDOW_VIDEO_FILES);
  if (pWindow)
  {
    // close our dialog
    Close(true);
    if (resume)
      movie.m_lStartOffset = STARTOFFSET_RESUME;
    pWindow->PlayMovie(&movie);
  }
}

void CGUIWindowVideoInfo::OnInitWindow()
{
  CGUIDialog::OnInitWindow();
  // disable button with id 10 as we don't have support for it yet!
//  CONTROL_DISABLE(10);
}

// Get Thumb from user choice.
// Options are:
// 1.  Current thumb
// 2.  IMDb thumb
// 3.  Local thumb
// 4.  No thumb (if no Local thumb is available)
void CGUIWindowVideoInfo::OnGetThumb()
{
  CFileItemList items;

  // Grab the thumbnails from the web
  CStdString strPath;
  CUtil::AddFileToFolder(g_advancedSettings.m_cachePath,"imdbthumbs",strPath);
  CUtil::WipeDir(_P(strPath));
  DIRECTORY::CDirectory::Create(strPath);
  int i=1;
  for (std::vector<CScraperUrl::SUrlEntry>::iterator iter=m_movieItem->GetVideoInfoTag()->m_strPictureURL.m_url.begin();iter != m_movieItem->GetVideoInfoTag()->m_strPictureURL.m_url.end();++iter)
  {
    if (iter->m_type == CScraperUrl::URL_TYPE_SEASON)
      continue;
    CStdString thumbFromWeb;
    CStdString strLabel;
    strLabel.Format("imdbthumb%i.jpg",i);
    CUtil::AddFileToFolder(strPath, strLabel, thumbFromWeb);
    if (CScraperUrl::DownloadThumbnail(thumbFromWeb,*iter))
    {
      CStdString strItemPath;
      strItemPath.Format("thumb://IMDb%i",i++);
      CFileItem *item = new CFileItem(strItemPath, false);
      item->SetThumbnailImage(thumbFromWeb);
      CStdString strLabel;
      item->SetLabel(g_localizeStrings.Get(20015));
      items.Add(item);
    }
  }
  if (CFile::Exists(m_movieItem->GetThumbnailImage()))
  {
    CFileItem *item = new CFileItem("thumb://Current", false);
    item->SetThumbnailImage(m_movieItem->GetThumbnailImage());
    item->SetLabel(g_localizeStrings.Get(20016));
    items.Add(item);
  }

  CStdString cachedLocalThumb;
  CStdString localThumb(m_movieItem->GetUserVideoThumb());
  if (CFile::Exists(localThumb))
  {
    CUtil::AddFileToFolder(g_advancedSettings.m_cachePath, "localthumb.jpg", cachedLocalThumb);
    CPicture pic;
    pic.DoCreateThumbnail(localThumb, cachedLocalThumb);
    CFileItem *item = new CFileItem("thumb://Local", false);
    item->SetThumbnailImage(cachedLocalThumb);
    item->SetLabel(g_localizeStrings.Get(20017));
    items.Add(item);
  }
  else
  { // no local thumb exists, so we are just using the IMDb thumb or cached thumb
    // which is probably the IMDb thumb.  These could be wrong, so allow the user
    // to delete the incorrect thumb
    CFileItem *item = new CFileItem("thumb://None", false);
    item->SetThumbnailImage("defaultVideoBig.png");
    item->SetLabel(g_localizeStrings.Get(20018));
    items.Add(item);
  }

  CStdString result;
  if (!CGUIDialogFileBrowser::ShowAndGetImage(items, g_settings.m_videoSources, g_localizeStrings.Get(20019), result))
    return;   // user cancelled

  if (result == "thumb://Current")
    return;   // user chose the one they have

  // delete the thumbnail if that's what the user wants, else overwrite with the
  // new thumbnail
  CFileItem item(*m_movieItem->GetVideoInfoTag());
  CStdString cachedThumb(item.GetCachedVideoThumb());

  if (result.Mid(0,12) == "thumb://IMDb")
  {
    CStdString strFile;
    CUtil::AddFileToFolder(strPath,"imdbthumb"+result.Mid(12)+".jpg",strFile);
    if (CFile::Exists(strFile))
      CFile::Cache(strFile, cachedThumb);
    else
      result = "thumb://None";
  }
  else if (result == "thumb://Local")
    CFile::Cache(cachedLocalThumb, cachedThumb);
  else if (CFile::Exists(result))
  {
    CPicture pic;
    pic.DoCreateThumbnail(result, cachedThumb);
  }
  else 
    result = "thumb://None";

  if (result == "thumb://None")
  { // cache the default thumb
    CPicture pic;
    pic.CacheSkinImage("defaultVideoBig.png", cachedThumb);
  }

  CUtil::DeleteVideoDatabaseDirectoryCache(); // to get them new thumbs to show
  m_movieItem->SetThumbnailImage(cachedThumb);

  // tell our GUI to completely reload all controls (as some of them
  // are likely to have had this image in use so will need refreshing)
  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_REFRESH_THUMBS);
  g_graphicsContext.SendMessage(msg);
  // Update our screen
  Update();
}

// Allow user to select a Fanart
void CGUIWindowVideoInfo::OnGetFanart()
{
  CFileItemList items;

  // Grab the thumbnails from the web
  CStdString strPath;
  CUtil::AddFileToFolder(g_advancedSettings.m_cachePath,"fanartthumbs",strPath);
  CUtil::WipeDir(strPath);
  DIRECTORY::CDirectory::Create(strPath);
  for (unsigned int i = 0; i < m_movieItem->GetVideoInfoTag()->m_fanart.GetNumFanarts(); i++)
  {
    CStdString thumbFromWeb;
    CStdString strLabel;
    strLabel.Format("fanart_thumb_%i.jpg", i);
    CUtil::AddFileToFolder(strPath, strLabel, thumbFromWeb);
    if (m_movieItem->GetVideoInfoTag()->m_fanart.DownloadThumb(i, thumbFromWeb))
    {
      CStdString strItemPath;
      strItemPath.Format("thumb://FANART_%i",i);
      CFileItem *item = new CFileItem(strItemPath, false);
      item->SetThumbnailImage(thumbFromWeb);
      CStdString strLabel;
      item->SetLabel(g_localizeStrings.Get(20015));
      items.Add(item);
    }
    else
      CLog::Log(LOGDEBUG, "Unable to download fanart thumb #%d", i);
  }

  CFileItem *itemNone = new CFileItem("thumb://None", false);
  itemNone->SetThumbnailImage("defaultVideoBig.png");
  itemNone->SetLabel(g_localizeStrings.Get(20018));
  items.Add(itemNone);

  CStdString result;
  if (!CGUIDialogFileBrowser::ShowAndGetImage(items, g_settings.m_videoSources, g_localizeStrings.Get(20019), result))
    return;   // user cancelled

  // delete the thumbnail if that's what the user wants, else overwrite with the
  // new thumbnail
  CFileItem item(*m_movieItem->GetVideoInfoTag());
  CStdString cachedThumb(item.GetCachedVideoFanart());

  if (result.Mid(0,15) == "thumb://FANART_")
  {
    CStdString strFile;
    CUtil::AddFileToFolder(strPath,"fanart_thumb_"+result.Mid(15)+".jpg",strFile);
    int iFanart = atoi(result.Mid(15).c_str());
    if (CFile::Exists(strFile))
    {
      // set new primary fanart, and update our database accordingly
      m_movieItem->GetVideoInfoTag()->m_fanart.SetPrimaryFanart(iFanart);
      m_database.SetDetailsForTvShow(m_movieItem->m_strPath, *m_movieItem->GetVideoInfoTag());

      // download the fullres fanart image.  TODO: FANART - this could take some time, so should probably be backgrounded
      m_movieItem->GetVideoInfoTag()->m_fanart.DownloadImage(cachedThumb);
    }
    else
      result = "thumb://None";
  }
  else if (CFile::Exists(result))
  { // local file
    CPicture pic;
    pic.CacheImage(result, cachedThumb);
  }
  else
    result = "thumb://None";

  if (result == "thumb://None")
  { // remove the cached art
    if (CFile::Exists(cachedThumb))
      CFile::Delete(cachedThumb);
  }

  CUtil::DeleteVideoDatabaseDirectoryCache(); // to get them new thumbs to show

  // tell our GUI to completely reload all controls (as some of them
  // are likely to have had this image in use so will need refreshing)
  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_REFRESH_THUMBS);
  g_graphicsContext.SendMessage(msg);
  // Update our screen
  Update();
}

void CGUIWindowVideoInfo::PlayTrailer()
{
  CFileItem item;
  item.m_strPath = m_movieItem->GetVideoInfoTag()->m_strTrailer;
  *item.GetVideoInfoTag() = *m_movieItem->GetVideoInfoTag();
  item.GetVideoInfoTag()->m_strTitle.Format("%s (%s)",m_movieItem->GetVideoInfoTag()->m_strTitle.c_str(),g_localizeStrings.Get(20410));
  item.SetThumbnailImage(m_movieItem->GetThumbnailImage());
  
  // Close the dialog.
  Close(true);
  g_application.getApplicationMessenger().PlayFile(item);
}

void CGUIWindowVideoInfo::SetLabel(int iControl, const CStdString &strLabel)
{
  if (strLabel.IsEmpty())
  {
    SET_CONTROL_LABEL(iControl, 416);  // "Not available"
  }
  else
  {
    SET_CONTROL_LABEL(iControl, strLabel);
  }
}

const CStdString& CGUIWindowVideoInfo::GetThumbnail() const 
{ 
  return m_movieItem->GetThumbnailImage(); 
}
