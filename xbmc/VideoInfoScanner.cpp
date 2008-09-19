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
#include "VideoInfoScanner.h"
#include "FileSystem/DirectoryCache.h"
#include "Util.h"
#include "NfoFile.h"
#include "utils/RegExp.h"
#include "utils/md5.h"
#include "Picture.h"
#include "FileSystem/StackDirectory.h"
#include "xbox/XKGeneral.h"
#include "utils/IMDB.h"
#include "utils/GUIInfoManager.h"
#include "FileSystem/File.h"
#include "GUIDialogProgress.h"
#include "Settings.h"
#include "FileItem.h"

#define REGEXSAMPLEFILE "[-\\._ ](sample|trailer)[-\\._ ]"

using namespace std;
using namespace DIRECTORY;
using namespace XFILE;

namespace VIDEO 
{

  CVideoInfoScanner::CVideoInfoScanner()
  {
    m_bRunning = false;
    m_pObserver = NULL;
    m_bCanInterrupt = false;
    m_currentItem=0;
    m_itemCount=0;
    m_bClean=false;
  }

  CVideoInfoScanner::~CVideoInfoScanner()
  {
  }

  void CVideoInfoScanner::Process()
  {
    try
    {
      DWORD dwTick = timeGetTime();

      m_database.Open();

      if (m_pObserver)
        m_pObserver->OnStateChanged(PREPARING);

      m_bCanInterrupt = true;

      CLog::Log(LOGDEBUG, "%s - Starting scan", __FUNCTION__);

      // Reset progress vars
      m_currentItem=0;
      m_itemCount=-1;

      // Create the thread to count all files to be scanned
      SetPriority(THREAD_PRIORITY_IDLE);
      CThread fileCountReader(this);    
      if (m_pObserver)
        fileCountReader.Create();

      // Database operations should not be canceled
      // using Interupt() while scanning as it could
      // result in unexpected behaviour.
      m_bCanInterrupt = false;

      bool bCancelled = false;
      for(std::map<CStdString,VIDEO::SScanSettings>::iterator it = m_pathsToScan.begin(); it != m_pathsToScan.end(); it++)
      {
        if(!DoScan(it->first, it->second))
        {
          bCancelled = true;
          break;
        }
      }

      if (!bCancelled)
      {
        if (m_bClean)
          m_database.CleanDatabase(m_pObserver,&m_pathsToClean);
        else
        {
          if (m_pObserver)
            m_pObserver->OnStateChanged(COMPRESSING_DATABASE);
          m_database.Compress(false);
        }
      }

      fileCountReader.StopThread();

      m_database.Close();
      CLog::Log(LOGDEBUG, "%s - Finished scan", __FUNCTION__);

      dwTick = timeGetTime() - dwTick;
      CStdString strTmp, strTmp1;
      StringUtils::SecondsToTimeString(dwTick / 1000, strTmp1);
      strTmp.Format("My Videos: Scanning for video info using worker thread, operation took %s", strTmp1);
      CLog::Log(LOGNOTICE, "%s", strTmp.c_str());

      m_bRunning = false;
      if (m_pObserver)
        m_pObserver->OnFinished();
    }
    catch (...)
    {
      CLog::Log(LOGERROR, "VideoInfoScanner: Exception while scanning.");
    }
  }

  void CVideoInfoScanner::Start(const CStdString& strDirectory, const SScraperInfo& info, const SScanSettings& settings, bool bUpdateAll)
  {
    m_strStartDir = strDirectory;
    m_bUpdateAll = bUpdateAll;
    m_info = info;
    m_pathsToScan.clear();
    m_pathsToClean.clear();
    CScraperParser::ClearCache();

    if (strDirectory.IsEmpty())
    { // scan all paths in the database.  We do this by scanning all paths in the db, and crossing them off the list as
      // we go.
      m_database.Open();
      m_database.GetPaths(m_pathsToScan);
      m_bClean = g_advancedSettings.m_bVideoLibraryCleanOnUpdate;
      m_database.Close();
    }
    else
    {
      m_pathsToScan.insert(pair<CStdString,SScanSettings>(strDirectory,settings));
      m_bClean = false;
    }

    StopThread();
    Create();
    m_bRunning = true;
  }

  bool CVideoInfoScanner::IsScanning()
  {
    return m_bRunning;
  }

  void CVideoInfoScanner::Stop()
  {
    if (m_bCanInterrupt)
      m_database.Interupt();

    StopThread();
  }

  void CVideoInfoScanner::SetObserver(IVideoInfoScannerObserver* pObserver)
  {
    m_pObserver = pObserver;
  }

  bool CVideoInfoScanner::DoScan(const CStdString& strDirectory, SScanSettings settings)
  {
    if (m_bUpdateAll)
    {
      if (m_pObserver)
        m_pObserver->OnStateChanged(REMOVING_OLD);

      m_database.RemoveContentForPath(strDirectory);
    }

    if (m_pObserver)
    {
      m_pObserver->OnDirectoryChanged(strDirectory);
      m_pObserver->OnSetTitle(g_localizeStrings.Get(20415));
    }

    // load subfolder
    CFileItemList items;
    int iFound;
    bool bSkip=false;
    m_database.GetScraperForPath(strDirectory,m_info,settings, iFound);
    if (m_info.strContent.IsEmpty() || m_info.strContent.Equals("None"))
      bSkip = true;

    CStdString hash, dbHash;
    if (m_info.strContent.Equals("movies"))
    {
      if (m_pObserver)
        m_pObserver->OnStateChanged(FETCHING_MOVIE_INFO);

      CDirectory::GetDirectory(strDirectory,items,g_stSettings.m_videoExtensions);
      items.m_strPath = strDirectory;
      int iOldStack = g_stSettings.m_iMyVideoStack;
      g_stSettings.m_iMyVideoStack = STACK_SIMPLE;
      items.Stack();
      g_stSettings.m_iMyVideoStack = iOldStack;
      int numFilesInFolder = GetPathHash(items, hash);

      if (!m_database.GetPathHash(strDirectory, dbHash) || dbHash != hash)
      { // path has changed - rescan
        if (dbHash.IsEmpty())
          CLog::Log(LOGDEBUG, "%s Scanning dir '%s' as not in the database", __FUNCTION__, strDirectory.c_str());
        else
          CLog::Log(LOGDEBUG, "%s Rescanning dir '%s' due to change", __FUNCTION__, strDirectory.c_str());
      }
      else
      {
        CLog::Log(LOGDEBUG, "%s Skipping dir '%s' due to no change", __FUNCTION__, strDirectory.c_str());
        m_currentItem += numFilesInFolder;

        // notify our observer of our progress
        if (m_pObserver)
        {
          if (m_itemCount>0)
            m_pObserver->OnSetProgress(m_currentItem, m_itemCount);
          m_pObserver->OnDirectoryScanned(strDirectory);
        }
        bSkip = true;
      }
    }
    else if (m_info.strContent.Equals("tvshows"))
    {
      if (m_pObserver)
        m_pObserver->OnStateChanged(FETCHING_TVSHOW_INFO);

      if (iFound == 1 && !settings.parent_name_root)
      {
        CDirectory::GetDirectory(strDirectory,items,g_stSettings.m_videoExtensions);
        items.m_strPath = strDirectory;
        GetPathHash(items, hash);
        bSkip = true;
        if (!m_database.GetPathHash(strDirectory, dbHash) || dbHash != hash)
        {
          m_database.SetPathHash(strDirectory, hash);
          bSkip = false;
        }
        else
          items.Clear();
      }
      else
      {
        CFileItemPtr item(new CFileItem(CUtil::GetFileName(strDirectory)));
        item->m_strPath = strDirectory;
        item->m_bIsFolder = true;
        items.Add(item);
        CUtil::GetParentPath(item->m_strPath,items.m_strPath);
      }
    }
    else if (m_info.strContent.Equals("musicvideos"))
    {
      if (m_pObserver)
        m_pObserver->OnStateChanged(FETCHING_MUSICVIDEO_INFO);

      CDirectory::GetDirectory(strDirectory,items,g_stSettings.m_videoExtensions);
      items.m_strPath = strDirectory;

      int numFilesInFolder = GetPathHash(items, hash);
      if (!m_database.GetPathHash(strDirectory, dbHash) || dbHash != hash)
      { // path has changed - rescan
        if (dbHash.IsEmpty())
          CLog::Log(LOGDEBUG, "%s Scanning dir '%s' as not in the database", __FUNCTION__, strDirectory.c_str());
        else
          CLog::Log(LOGDEBUG, "%s Rescanning dir '%s' due to change", __FUNCTION__, strDirectory.c_str());
      }
      else
      {
        CLog::Log(LOGDEBUG, "%s Skipping dir '%s' due to no change", __FUNCTION__, strDirectory.c_str());
        m_currentItem += numFilesInFolder;

        // notify our observer of our progress
        if (m_pObserver)
        {
          if (m_itemCount>0)
            m_pObserver->OnSetProgress(m_currentItem, m_itemCount);
          m_pObserver->OnDirectoryScanned(strDirectory);
        }
        bSkip = true;
      }
    }

    CLog::Log(LOGDEBUG,"Hash[%s,%s]:DB=[%s],Computed=[%s]",
      m_info.strContent.c_str(),strDirectory.c_str(),dbHash.c_str(),hash.c_str());

    if (!m_info.settings.GetPluginRoot() && m_info.settings.GetSettings().IsEmpty()) // check for settings, if they are around load defaults - to workaround the nastyness
    {
      CScraperParser parser;
      CStdString strPath;
      if (!m_info.strContent.IsEmpty())
        strPath=_P("q:\\system\\scrapers\\video\\"+m_info.strPath);
      if (!strPath.IsEmpty() && parser.Load(strPath) && parser.HasFunction("GetSettings"))
      {
        m_info.settings.LoadSettingsXML(_P("q:\\system\\scrapers\\video\\"+m_info.strPath));
        m_info.settings.SaveFromDefault();
      }
    }

    if (!bSkip)
    {
      RetrieveVideoInfo(items,settings.parent_name_root,m_info);
      if (!m_bStop && (m_info.strContent.Equals("movies") || m_info.strContent.Equals("musicvideos")))
      {
        m_database.SetPathHash(strDirectory, hash);
        m_pathsToClean.push_back(m_database.GetPathId(strDirectory));
      }
    }

    if (m_pObserver)
      m_pObserver->OnDirectoryScanned(strDirectory);
    CLog::Log(LOGDEBUG, "%s - Finished dir: %s", __FUNCTION__, strDirectory.c_str());

    for (int i = 0; i < items.Size(); ++i)
    {
      CFileItemPtr pItem = items[i];

      if (m_bStop)
        break;
      // if we have a directory item (non-playlist) we then recurse into that folder
      if (pItem->m_bIsFolder && !pItem->GetLabel().Equals("sample") && !pItem->GetLabel().Equals("subs") && !pItem->IsParentFolder() && !pItem->IsPlayList() && settings.recurse > 0 && !m_info.strContent.Equals("tvshows")) // do not recurse for tv shows - we have already looked recursively for episodes
      {
        CStdString strPath=pItem->m_strPath;
        SScanSettings settings2;
        settings2.recurse = settings.recurse-1;
        settings2.parent_name_root = settings.parent_name;
        settings2.parent_name = settings.parent_name;
        if (!DoScan(strPath,settings2))
        {
          m_bStop = true;
        }
      }
    }
    return !m_bStop;
  }

  bool CVideoInfoScanner::RetrieveVideoInfo(CFileItemList& items, bool bDirNames, const SScraperInfo& info, bool bRefresh, CScraperUrl* pURL, CGUIDialogProgress* pDlgProgress)
  {
    CStdString strMovieName;
    CIMDB IMDB;
    IMDB.SetScraperInfo(info);
    CRegExp regExSample;
    
    if (!regExSample.RegComp(REGEXSAMPLEFILE))
    {
      CLog::Log(LOGERROR, "Unable to compile RegExp for Sample file");
    }

    if (bDirNames && info.strContent.Equals("movies"))
    {
      strMovieName = items.m_strPath;
      CUtil::RemoveSlashAtEnd(strMovieName);
      strMovieName = CUtil::GetFileName(strMovieName);
    }

    if (pDlgProgress)
    {
      if (items.Size() > 1 || items[0]->m_bIsFolder && !bRefresh)
      {
        pDlgProgress->ShowProgressBar(true);
        pDlgProgress->SetPercentage(0);
      }
      else
        pDlgProgress->ShowProgressBar(false);

      pDlgProgress->Progress();
    }

    // for every file found
    CVideoInfoTag showDetails;
    long lTvShowId = -1;
    m_database.Open();
    // needed to ensure the movie count etc is cached
    for (int i=LIBRARY_HAS_VIDEO;i<LIBRARY_HAS_MUSICVIDEOS+1;++i)
      g_infoManager.GetBool(i);
    for (int i = 0; i < (int)items.Size(); ++i)
    {
      IMDB_EPISODELIST episodes;
      IMDB_EPISODELIST files;
      CFileItemPtr pItem = items[i];

      // we do this since we may have a override per dir
      SScraperInfo info2;
      if (pItem->m_bIsFolder)
        m_database.GetScraperForPath(pItem->m_strPath,info2);
      else
        m_database.GetScraperForPath(items.m_strPath,info2);

      if (info2.strContent.Equals("None")) // skip
        continue;

      // we might override scraper
      if (info2.strContent == info.strContent)
        info2.strPath = info.strPath;

      IMDB.SetScraperInfo(info2);

      // Discard all possible sample files defined by regExSample
      CStdString strFileName = CUtil::GetFileName(items[i]->m_strPath);
      strFileName.MakeLower();

      if(!strFileName.IsEmpty())
      {
        CLog::Log(LOGDEBUG, "Checking if file '%s' is a Sample file", strFileName.c_str());
        if (regExSample.RegFind(strFileName) > -1)
        {
          CLog::Log(LOGDEBUG, "File '%s' discarded as Sample file", strFileName.c_str());
          continue;
        }
      }

      if (info2.strContent.Equals("movies") || info2.strContent.Equals("musicvideos"))
      {
        if (m_pObserver)
        {
          m_pObserver->OnSetCurrentProgress(i,items.Size());
          if (!pItem->m_bIsFolder && m_itemCount)
            m_pObserver->OnSetProgress(m_currentItem++,m_itemCount);
        }

      }
      if (info2.strContent.Equals("tvshows"))
      {
        if (pItem->m_bIsFolder)
          lTvShowId = m_database.GetTvShowId(pItem->m_strPath);
        else
        {
          CStdString strPath;
          CUtil::GetDirectory(pItem->m_strPath,strPath);
          lTvShowId = m_database.GetTvShowId(strPath);
        }
        if (lTvShowId > -1 && (!bRefresh || !pItem->m_bIsFolder))
        {
          // fetch episode guide
          m_database.GetTvShowInfo(pItem->m_strPath,showDetails,lTvShowId);
          files.clear();
          EnumerateSeriesFolder(pItem.get(),files);
          if (files.size() == 0) // no update or no files
            continue;

          CScraperUrl url;
          //convert m_strEpisodeGuide in url.m_scrURL
          if (!showDetails.m_strEpisodeGuide.IsEmpty()) // assume local-only series if no episode guide url
          {
            url.ParseEpisodeGuide(showDetails.m_strEpisodeGuide);
            if (pDlgProgress)
            {
              if (pItem->m_bIsFolder)
                pDlgProgress->SetHeading(20353);
              else
                pDlgProgress->SetHeading(20361);
              pDlgProgress->SetLine(0, pItem->GetLabel());
              pDlgProgress->SetLine(1,showDetails.m_strTitle);
              pDlgProgress->SetLine(2,20354);
              pDlgProgress->Progress();
            }
            if (!IMDB.GetEpisodeList(url,episodes))
            {
              if (pDlgProgress)
                pDlgProgress->Close();
              m_database.RollbackTransaction();
              m_database.Close();
              return false;
            }
          }
          if (m_bStop || (pDlgProgress && pDlgProgress->IsCanceled()))
          {
            if (pDlgProgress)
              pDlgProgress->Close();
            m_database.RollbackTransaction();
            m_database.Close();
            return false;
          }
          if (m_pObserver)
            m_pObserver->OnDirectoryChanged(pItem->m_strPath);

          OnProcessSeriesFolder(episodes,files,lTvShowId,IMDB,showDetails.m_strTitle,pDlgProgress);
          continue;
        }
        else
        {
          CUtil::RemoveSlashAtEnd(pItem->m_strPath);
          strMovieName = CUtil::GetFileName(pItem->m_strPath);
          CUtil::AddSlashAtEnd(pItem->m_strPath);
        }
      }

      if (!pItem->m_bIsFolder || info.strContent.Equals("tvshows"))
      {
        if ((pItem->IsVideo() && !pItem->IsNFO() && !pItem->IsPlayList()) || info.strContent.Equals("tvshows") )
        {
          if (!bDirNames && !info.strContent.Equals("tvshows"))
          {
            if (pItem->IsLabelPreformated())
              strMovieName = pItem->GetLabel();
            else
            {
              if (pItem->IsStack())
                strMovieName = pItem->GetLabel();
              else
                strMovieName = CUtil::GetFileName(pItem->m_strPath);
              CUtil::RemoveExtension(strMovieName);
            }
          }

          if (pDlgProgress)
          {
            int iString=198;
            if (info.strContent.Equals("tvshows"))
            {
              if (pItem->m_bIsFolder)
                iString = 20353;
              else
                iString = 20361;
            }
            if (info.strContent.Equals("musicvideos"))
              iString = 20394;
            pDlgProgress->SetHeading(iString);
            pDlgProgress->SetLine(0, pItem->GetLabel());
            pDlgProgress->SetLine(2,"");
            pDlgProgress->Progress();
            if (pDlgProgress->IsCanceled()) 
            {
              pDlgProgress->Close();
              m_database.RollbackTransaction();
              m_database.Close();
              return false;
            }
          }
          if (m_bStop)
          {
            m_database.RollbackTransaction();
            m_database.Close();
            return false;
          }
          if (info.strContent.Equals("movies"))
          {
            if (!m_database.HasMovieInfo(pItem->m_strPath))
            {
              // handle .nfo files
              CScraperUrl scrUrl;
              NFOResult result = CheckForNFOFile(pItem.get(),bDirNames,info2,pDlgProgress,scrUrl);
              if (result == FULL_NFO || result == URL_NFO)
                continue;
            }
            else
              continue;
          }
          else if (info.strContent.Equals("musicvideos"))
          {
            if (!m_database.HasMusicVideoInfo(pItem->m_strPath))
            {
              CScraperUrl scrUrl;
              NFOResult result = CheckForNFOFile(pItem.get(),false,info2,pDlgProgress,scrUrl);
              if (result == FULL_NFO || result == URL_NFO)
                continue;
            }
            else
              continue;
          }
          else if (info.strContent.Equals("tvshows"))
          {
            // handle .nfo files
            CScraperUrl scrUrl;
            NFOResult result = CheckForNFOFile(pItem.get(),false,info2,pDlgProgress,scrUrl);
            if (result == FULL_NFO || result == URL_NFO)
            {
              SScraperInfo info3(info2);
              SScanSettings settings;
              m_database.GetScraperForPath(pItem->m_strPath,info3,settings);
              info3.strPath = info2.strPath;
              m_database.SetScraperForPath(pItem->m_strPath,info3,settings);
              i--;
              continue;
            }
          }

          IMDB_MOVIELIST movielist;
          if (pURL || IMDB.FindMovie(strMovieName, movielist, pDlgProgress))
          {
            CScraperUrl url;
            int iMoviesFound=1;
            if (!pURL)
            {
              iMoviesFound = movielist.size();
              if (iMoviesFound)
                url = movielist[0];
            }
            else
            {
              url = *pURL;
            }
            if (iMoviesFound > 0)
            {
              if (m_pObserver)
                m_pObserver->OnSetTitle(url.strTitle);
              CUtil::ClearCache();
              long lResult=1;
              lResult=GetIMDBDetails(pItem.get(), url, info,bDirNames&&info.strContent.Equals("movies"));
              if (info.strContent.Equals("tvshows"))
              {
                if (!bRefresh)
                {
                  // fetch episode guide
                  CVideoInfoTag details;
                  m_database.GetTvShowInfo(pItem->m_strPath,details,lResult);
                  if (!details.m_strEpisodeGuide.IsEmpty()) // assume local-only series if no episode guide url
                  {
                    CScraperUrl url;
                    url.ParseEpisodeGuide(details.m_strEpisodeGuide);
                    EnumerateSeriesFolder(pItem.get(),files);
                    if (!IMDB.GetEpisodeList(url,episodes))
                      continue;
                  }
                  OnProcessSeriesFolder(episodes,files,lResult,IMDB,details.m_strTitle,pDlgProgress);
                }
                else
                  if (g_guiSettings.GetBool("videolibrary.seasonthumbs"))
                    FetchSeasonThumbs(lResult);
              }
            }
          }
        }
      }
    }
    if(pDlgProgress)
      pDlgProgress->ShowProgressBar(false);

    m_database.Close();
    return true;
  }

  // This function is run by another thread
  void CVideoInfoScanner::Run()
  {
    int count = 0;
    while (!m_bStop && m_pathsToCount.size())
      count+=CountFiles(*m_pathsToCount.begin());
    m_itemCount = count;
  }

  // Recurse through all folders we scan and count files
  int CVideoInfoScanner::CountFiles(const CStdString& strPath)
  {
    int count=0;
    // load subfolder
    CFileItemList items;
    CLog::Log(LOGDEBUG, "%s - processing dir: %s", __FUNCTION__, strPath.c_str());
    CDirectory::GetDirectory(strPath, items, g_stSettings.m_videoExtensions, true);
    if (m_info.strContent.Equals("movies"))
      items.Stack();

    for (int i=0; i<items.Size(); ++i)
    {
      CFileItemPtr pItem=items[i];

      if (m_bStop)
        return 0;

      if (pItem->m_bIsFolder)
        count+=CountFiles(pItem->m_strPath);
      else if (pItem->IsVideo() && !pItem->IsPlayList() && !pItem->IsNFO())
        count++;
    }
    CLog::Log(LOGDEBUG, "%s - finished processing dir: %s", __FUNCTION__, strPath.c_str());
    return count;
  }

  void CVideoInfoScanner::EnumerateSeriesFolder(const CFileItem* item, IMDB_EPISODELIST& episodeList)
  {
    CFileItemList items;
    CRegExp regExSample;
    
    if (!regExSample.RegComp(REGEXSAMPLEFILE))
    {
      CLog::Log(LOGERROR, "Unable to compile RegExp for Sample file");
    }

    if (item->m_bIsFolder)
    {
      CUtil::GetRecursiveListing(item->m_strPath,items,g_stSettings.m_videoExtensions,true);
      CStdString hash, dbHash;
      int numFilesInFolder = GetPathHash(items, hash);

      if (m_database.GetPathHash(item->m_strPath, dbHash) && dbHash == hash)
      {
        m_currentItem += numFilesInFolder;

        // notify our observer of our progress
        if (m_pObserver)
        {
          if (m_itemCount>0)
          {
            m_pObserver->OnSetProgress(m_currentItem, m_itemCount);
            m_pObserver->OnSetCurrentProgress(numFilesInFolder,numFilesInFolder);
          }
          m_pObserver->OnDirectoryScanned(item->m_strPath);
        }
        return;
      }
      m_pathsToClean.push_back(m_database.GetPathId(item->m_strPath));
      m_database.SetPathHash(item->m_strPath,hash);
    }
    else
    {
      CFileItemPtr newItem(new CFileItem(*item));
      items.Add(newItem);
    }

    // enumerate
    CStdStringArray expression = g_advancedSettings.m_tvshowStackRegExps;

    for (int i=0;i<items.Size();++i)
    {
      if (items[i]->m_bIsFolder)
        continue;
      CStdString strPath;
      CUtil::GetDirectory(items[i]->m_strPath,strPath);
      CUtil::RemoveSlashAtEnd(strPath); // want no slash for the test that follows

      if (CUtil::GetFileName(strPath).Equals("sample"))
        continue;

      // Discard all possible sample files defined by regExSample
      CStdString strFileName = CUtil::GetFileName(items[i]->m_strPath);
      strFileName.MakeLower();
      CLog::Log(LOGDEBUG, "Checking if file '%s' is a Sample file", strFileName.c_str());
      if (regExSample.RegFind(strFileName) > -1)
      {
        CLog::Log(LOGDEBUG, "File '%s' discarded as Sample file", strFileName.c_str());
        continue;
      }

      bool bMatched=false;
      for (unsigned int j=0;j<expression.size();++j)
      {
        CRegExp reg;
        if (!reg.RegComp(expression[j]))
          break;
        CStdString strLabel=items[i]->m_strPath;
        strLabel.MakeLower();
        CLog::Log(LOGDEBUG,"running expression %s on label %s",expression[j].c_str(),strLabel.c_str());
        int regexppos, regexp2pos;

        if ((regexppos = reg.RegFind(strLabel.c_str())) > -1)
        {
          char* season = reg.GetReplaceString("\\1");
          char* episode = reg.GetReplaceString("\\2");

          if (season && episode)
          {
            CLog::Log(LOGDEBUG,"found match %s %s %s",strLabel.c_str(),season,episode);
            int iSeason = atoi(season);
            int iEpisode = atoi(episode);
            std::pair<int,int> key(iSeason,iEpisode);
            CScraperUrl url(items[i]->m_strPath);
            episodeList.insert(std::make_pair<std::pair<int,int>,CScraperUrl>(key,url));
            bMatched = true;

            // check the remainder of the string for any further episodes.
            CRegExp reg2;
            if (!reg2.RegComp(g_advancedSettings.m_tvshowMultiPartStackRegExp))
              break;

            char *remainder = reg.GetReplaceString("\\3");
            int offset = 0;
            
            // we want "long circuit" OR below so that both offsets are evaluated
            while (((regexp2pos = reg2.RegFind(remainder + offset)) > -1) | ((regexppos = reg.RegFind(remainder + offset)) > -1)) 
            {
              if (((regexppos <= regexp2pos) && regexppos != -1) ||
                 (regexppos >= 0 && regexp2pos == -1))
              {
                season = reg.GetReplaceString("\\1");
                episode = reg.GetReplaceString("\\2");
                key.first = atoi(season);
                key.second = atoi(episode);
                free(season);
                free(episode);
                CLog::Log(LOGDEBUG, "adding new season %u, multipart episode %u", key.first, key.second);
                episodeList.insert(std::make_pair<std::pair<int,int>,CScraperUrl>(key,url));
                remainder = reg.GetReplaceString("\\3");
                offset = 0;
              } 
              else if (((regexp2pos < regexppos) && regexp2pos != -1) ||
                       (regexp2pos >= 0 && regexppos == -1))
              {
                episode = reg2.GetReplaceString("\\1");
                key.second = atoi(episode);
                free(episode);
                CLog::Log(LOGDEBUG, "adding multipart episode %u", key.second);
                episodeList.insert(std::make_pair<std::pair<int,int>,CScraperUrl>(key,url));
                offset += regexp2pos + reg2.GetFindLen();
              }
            }
          }
        }
      }
      if (!bMatched)
        CLog::Log(LOGDEBUG,"could not enumerate file %s",items[i]->m_strPath.c_str());
    }
  }

  long CVideoInfoScanner::AddMovieAndGetThumb(CFileItem *pItem, const CStdString &content, const CVideoInfoTag &movieDetails, long idShow, bool bApplyToDir, CGUIDialogProgress* pDialog /* == NULL */)
  {
    long lResult=-1;
    // add to all movies in the stacked set
    if (content.Equals("movies"))
    {
      m_database.SetDetailsForMovie(pItem->m_strPath, movieDetails);
    }
    else if (content.Equals("tvshows"))
    {
      if (pItem->m_bIsFolder)
      {
        lResult=m_database.SetDetailsForTvShow(pItem->m_strPath, movieDetails);
      }
      else
      {
        // we add episode then set details, as otherwise set details will delete the
        // episode then add, which breaks multi-episode files.
        long idEpisode = m_database.AddEpisode(idShow, pItem->m_strPath);
        lResult = m_database.SetDetailsForEpisode(pItem->m_strPath, movieDetails, idShow, idEpisode);
      }
    }
    else if (content.Equals("musicvideos"))
    {
      m_database.SetDetailsForMusicVideo(pItem->m_strPath, movieDetails);
    }
    pItem->CacheFanart();
    // get & save fanart image
    if (!CFile::Exists(pItem->GetCachedFanart()))
    {
      if (!movieDetails.m_fanart.m_xml.IsEmpty() && !movieDetails.m_fanart.DownloadImage(pItem->GetCachedFanart()))
        CLog::Log(LOGERROR, "Failed to download fanart %s to %s", movieDetails.m_fanart.GetImageURL().c_str(), pItem->GetCachedFanart().c_str());
    }

    // get & save thumbnail
    CStdString strThumb = "";
    CStdString strImage = movieDetails.m_strPictureURL.GetFirstThumb().m_url;
    if (strImage.size() > 0)
    {
      // check for a cached thumb or user thumb
      strThumb = pItem->GetCachedVideoThumb();

      CHTTP http;
      if (pDialog)
      {
        pDialog->SetLine(2, 415);
        pDialog->Progress();
      }

      string image;
      if (pItem->GetUserVideoThumb().IsEmpty() && http.Get(strImage, image))
      {
        try
        {
          CPicture picture;
          picture.CreateThumbnailFromMemory((const BYTE *)image.c_str(), image.size(), CUtil::GetExtension(strThumb), strThumb);
        }
        catch (...)
        {
          CLog::Log(LOGERROR,"Could not make imdb thumb from %s", strImage.c_str());
          ::DeleteFile(strThumb.c_str());
        }
      }
    }

    if (bApplyToDir)
    {
      CStdString strCheck=pItem->m_strPath;
      CStdString strDirectory;
      if (pItem->IsStack())
        strCheck = CStackDirectory::GetFirstStackedFile(pItem->m_strPath);

      CUtil::GetDirectory(strCheck,strDirectory);
      if (CUtil::IsInRAR(strCheck))
      {
        CStdString strPath=strDirectory;
        CUtil::GetParentPath(strPath,strDirectory);
      }
      if (pItem->IsStack())
      {
        strCheck = strDirectory;
        CUtil::RemoveSlashAtEnd(strCheck);
        if (CUtil::GetFileName(strCheck).size() == 3 && CUtil::GetFileName(strCheck).Left(2).Equals("cd"))
          CUtil::GetDirectory(strCheck,strDirectory);
      }
      ApplyIMDBThumbToFolder(strDirectory,strThumb);
    }

    if (g_guiSettings.GetBool("videolibrary.actorthumbs"))
      FetchActorThumbs(movieDetails.m_cast);
    return lResult;
  }

  void CVideoInfoScanner::OnProcessSeriesFolder(IMDB_EPISODELIST& episodes, IMDB_EPISODELIST& files, long lShowId, CIMDB& IMDB, const CStdString& strShowTitle, CGUIDialogProgress* pDlgProgress /* = NULL */)
  {
    if (pDlgProgress)
    {
      pDlgProgress->SetLine(2, 20361);
      pDlgProgress->SetPercentage(0);
      pDlgProgress->ShowProgressBar(true);
      pDlgProgress->Progress();
    }

    int iMax = files.size();
    int iCurr = 1;
    m_database.Open();
    for (IMDB_EPISODELIST::iterator iter = files.begin();iter != files.end();++iter)
    {
      if (pDlgProgress)
      {
        pDlgProgress->SetLine(2, 20361);
        pDlgProgress->SetPercentage((int)((float)(iCurr++)/iMax*100));
        pDlgProgress->Progress();
      }
      if (m_pObserver)
      {
        if (m_itemCount > 0)
          m_pObserver->OnSetProgress(m_currentItem++,m_itemCount);
        m_pObserver->OnSetCurrentProgress(iCurr++,iMax);
      }
      if (pDlgProgress && pDlgProgress->IsCanceled() || m_bStop)
      {
        if (pDlgProgress)
          pDlgProgress->Close();
        m_database.RollbackTransaction();
        m_database.Close();
        return;
      }

      CVideoInfoTag episodeDetails;
      if (m_database.GetEpisodeId(iter->second.m_url[0].m_url,iter->first.second,iter->first.first) > -1)
      {
        if (m_pObserver)
          m_pObserver->OnSetTitle(g_localizeStrings.Get(20415));
        continue;
      }

      CFileItem item;
      item.m_strPath = iter->second.m_url[0].m_url;

      // handle .nfo files
      CStdString strNfoFile = GetnfoFile(&item,false);
      if (!strNfoFile.IsEmpty())
      {
        CLog::Log(LOGDEBUG,"Found matching nfo file: %s", strNfoFile.c_str());
        CNfoFile nfoReader("tvshows");
        if (nfoReader.Create(strNfoFile) == S_OK)
        {
          if (nfoReader.m_strScraper == "NFO")
          {
            CLog::Log(LOGDEBUG, "%s Got details from nfo", __FUNCTION__);
            nfoReader.GetDetails(episodeDetails);
            AddMovieAndGetThumb(&item,"tvshows",episodeDetails,lShowId);
            continue;
          }
        }
      }

      IMDB_EPISODELIST::iterator iter2 = episodes.find(iter->first);
      if (iter2 != episodes.end())
      {
        if (!IMDB.GetEpisodeDetails(iter2->second,episodeDetails,pDlgProgress))
          break;
        episodeDetails.m_iSeason = iter2->first.first;
        episodeDetails.m_iEpisode = iter2->first.second;
        if (m_pObserver)
        {
          CStdString strTitle;
          strTitle.Format("%s - %ix%i - %s",strShowTitle.c_str(),episodeDetails.m_iSeason,episodeDetails.m_iEpisode,episodeDetails.m_strTitle.c_str());
          m_pObserver->OnSetTitle(strTitle);
        }
        CFileItem item;
        item.m_strPath = iter->second.m_url[0].m_url;
        AddMovieAndGetThumb(&item,"tvshows",episodeDetails,lShowId);
      }
    }
    if (g_guiSettings.GetBool("videolibrary.seasonthumbs"))
      FetchSeasonThumbs(lShowId);
    m_database.Close();
  }

  CStdString CVideoInfoScanner::GetnfoFile(CFileItem *item, bool bGrabAny)
  {
    CStdString nfoFile;
    // Find a matching .nfo file
    if (!item->m_bIsFolder)
    {
      // file
      CStdString strExtension;
      CUtil::GetExtension(item->m_strPath, strExtension);

      if (CUtil::IsInRAR(item->m_strPath)) // we have a rarred item - we want to check outside the rars
      {
        CFileItem item2(*item);
        CURL url(item->m_strPath);
        CStdString strPath;
        CUtil::GetDirectory(url.GetHostName(),strPath);
        CUtil::AddFileToFolder(strPath,CUtil::GetFileName(item->m_strPath),item2.m_strPath);
        return GetnfoFile(&item2,bGrabAny);
      }

      // already an .nfo file?
      if ( strcmpi(strExtension.c_str(), ".nfo") == 0 )
        nfoFile = item->m_strPath;
      // no, create .nfo file
      else
        CUtil::ReplaceExtension(item->m_strPath, ".nfo", nfoFile);

      // test file existence
      if (!nfoFile.IsEmpty() && !CFile::Exists(nfoFile))
        nfoFile.Empty();

      // try looking for .nfo file for a stacked item
      if (item->IsStack())
      {
        // first try .nfo file matching first file in stack
        CStackDirectory dir;
        CStdString firstFile = dir.GetFirstStackedFile(item->m_strPath);
        CFileItem item2;
        item2.m_strPath = firstFile;
        nfoFile = GetnfoFile(&item2,bGrabAny);
        // else try .nfo file matching stacked title
        if (nfoFile.IsEmpty())
        {
          CStdString stackedTitlePath = dir.GetStackedTitlePath(item->m_strPath);
          item2.m_strPath = stackedTitlePath;
          nfoFile = GetnfoFile(&item2,bGrabAny);
        }
      }

      if (nfoFile.IsEmpty()) // final attempt - strip off any cd1 folders
      {
        CStdString strPath;
        CUtil::GetDirectory(item->m_strPath,strPath);
        CUtil::RemoveSlashAtEnd(strPath); // need no slash for the check that follows
        CFileItem item2;
        if (strPath.Mid(strPath.size()-3).Equals("cd1"))
        {
          strPath = strPath.Mid(0,strPath.size()-3);
          CUtil::AddFileToFolder(strPath,CUtil::GetFileName(item->m_strPath),item2.m_strPath);
          return GetnfoFile(&item2,bGrabAny);
        }
      }
    }
    if (item->m_bIsFolder || bGrabAny && nfoFile.IsEmpty())
    {
      // see if there is a unique nfo file in this folder, and if so, use that
      CFileItemList items;
      CDirectory dir;
      CStdString strPath = item->m_strPath;
      if (!item->m_bIsFolder)
        CUtil::GetDirectory(item->m_strPath,strPath);
      if (dir.GetDirectory(strPath, items, ".nfo") && items.Size())
      {
        int numNFO = -1;
        for (int i = 0; i < items.Size(); i++)
        {
          if (items[i]->IsNFO())
          {
            if (numNFO == -1)
              numNFO = i;
            else
            {
              numNFO = -1;
              break;
            }
          }
        }
        if (numNFO > -1)
          return items[numNFO]->m_strPath;
      }
    }
    
    return nfoFile;
  }

  long CVideoInfoScanner::GetIMDBDetails(CFileItem *pItem, CScraperUrl &url, const SScraperInfo& info, bool bUseDirNames, CGUIDialogProgress* pDialog /* = NULL */)
  {
    CIMDB IMDB;
    CVideoInfoTag movieDetails;
    IMDB.SetScraperInfo(info);
    movieDetails.m_strFileNameAndPath = pItem->m_strPath;

    if ( IMDB.GetDetails(url, movieDetails, pDialog) )
    {
      if (info.strContent.Equals("movies"))
      {
        CStdString strFile = pItem->m_strPath;
        if (pItem->IsStack())
        {
          CStdString strPath;
          CUtil::GetParentPath(pItem->m_strPath,strPath);
          CStackDirectory dir;
          CStdString strPath2;
          strPath2 = dir.GetStackedTitlePath(strFile);
          CUtil::AddFileToFolder(strPath,CUtil::GetFileName(strPath2),strFile);
        }
        if (CUtil::IsInRAR(strFile) || CUtil::IsInZIP(strFile))
        {
          CStdString strPath, strParent;
          CUtil::GetDirectory(strFile,strPath);
          CUtil::GetParentPath(strPath,strParent);
          CUtil::AddFileToFolder(strParent,CUtil::GetFileName(pItem->m_strPath),strFile);
        }
        CUtil::RemoveExtension(strFile);
        strFile += "-trailer";
        std::vector<CStdString> exts;
        StringUtils::SplitString(g_stSettings.m_videoExtensions,"|",exts);
        for (unsigned int i=0;i<exts.size();++i)
        {
          if (CFile::Exists(strFile+exts[i]))
          {
            movieDetails.m_strTrailer = strFile+exts[i];
            break;
          }
        }
      }
      return AddMovieAndGetThumb(pItem, info.strContent, movieDetails, -1, bUseDirNames);
    }
    return -1;
  }

  void CVideoInfoScanner::ApplyIMDBThumbToFolder(const CStdString &folder, const CStdString &imdbThumb)
  {
    // copy icon to folder also;
    if (CFile::Exists(imdbThumb))
    {
      CFileItem folderItem(folder, true);
      CStdString strThumb(folderItem.GetCachedVideoThumb());
      CFile::Cache(imdbThumb.c_str(), strThumb.c_str(), NULL, NULL);
    }
  }

  int CVideoInfoScanner::GetPathHash(const CFileItemList &items, CStdString &hash)
  {
    // Create a hash based on the filenames, filesize and filedate.  Also count the number of files
    if (0 == items.Size()) return 0;
    MD5_CTX md5state;
    unsigned char md5hash[16];
    char md5HexString[33];
    MD5Init(&md5state);
    int count = 0;
    for (int i = 0; i < items.Size(); ++i)
    {
      const CFileItemPtr pItem = items[i];
      MD5Update(&md5state, (unsigned char *)pItem->m_strPath.c_str(), (int)pItem->m_strPath.size());
      MD5Update(&md5state, (unsigned char *)&pItem->m_dwSize, sizeof(pItem->m_dwSize));
      FILETIME time = pItem->m_dateTime;
      MD5Update(&md5state, (unsigned char *)&time, sizeof(FILETIME));
      if (pItem->IsVideo() && !pItem->IsPlayList() && !pItem->IsNFO())
        count++;
    }
    MD5Final(md5hash, &md5state);
    XKGeneral::BytesToHexStr(md5hash, 16, md5HexString);
    hash = md5HexString;
    return count;
  }

  void CVideoInfoScanner::FetchSeasonThumbs(long lTvShowId)
  {
    CVideoInfoTag movie;
    m_database.GetTvShowInfo("",movie,lTvShowId);
    CFileItemList items;
    CStdString strPath;
    strPath.Format("videodb://2/2/%i/",lTvShowId);
    m_database.GetSeasonsNav(strPath,items,-1,-1,-1,-1,lTvShowId);
    // used for checking for a season[ ._-](number).tbn
    CFileItemList tbnItems;
    CDirectory::GetDirectory(movie.m_strPath,tbnItems,".tbn");
    for (int i=0;i<items.Size();++i)
    {
      items[i]->SetVideoThumb();
      if (!items[i]->HasThumbnail())
      {
        CStdString strExpression;
        strExpression.Format("season[ ._-]?(0?%i)\\.tbn",items[i]->GetVideoInfoTag()->m_iSeason);
        bool bDownload=true;
        CRegExp reg;
        if (reg.RegComp(strExpression.c_str()))
        {
          for (int j=0;j<tbnItems.Size();++j)
          {
            CStdString strCheck = CUtil::GetFileName(tbnItems[j]->m_strPath);
            strCheck.ToLower();
            if (reg.RegFind(strCheck.c_str()) > -1)
            {
              CPicture picture;
              picture.DoCreateThumbnail(tbnItems[j]->m_strPath,items[i]->GetCachedSeasonThumb());
              bDownload=false;
              break;
            }
          }
        }
        if (bDownload)
          CScraperUrl::DownloadThumbnail(items[i]->GetCachedSeasonThumb(),movie.m_strPictureURL.GetSeasonThumb(items[i]->GetVideoInfoTag()->m_iSeason));
      }
    }
  }

  void CVideoInfoScanner::FetchActorThumbs(const vector<SActorInfo>& actors)
  {
    for (unsigned int i=0;i<actors.size();++i)
    {
      CFileItem item;
      item.SetLabel(actors[i].strName);
      CStdString strThumb = item.GetCachedActorThumb();
      if (!CFile::Exists(strThumb) && !actors[i].thumbUrl.GetFirstThumb().m_url.IsEmpty())
        CScraperUrl::DownloadThumbnail(strThumb,actors[i].thumbUrl.GetFirstThumb());
    }
  }

  CVideoInfoScanner::NFOResult CVideoInfoScanner::CheckForNFOFile(CFileItem* pItem, bool bGrabAny, SScraperInfo& info, CGUIDialogProgress* pDlgProgress, CScraperUrl& scrUrl)
  {
    CStdString strNfoFile;
    if (info.strContent.Equals("movies") || info.strContent.Equals("musicvideos") || (info.strContent.Equals("tvshows") && !pItem->m_bIsFolder))
      strNfoFile = GetnfoFile(pItem,bGrabAny);
    if (info.strContent.Equals("tvshows") && pItem->m_bIsFolder)
      CUtil::AddFileToFolder(pItem->m_strPath,"tvshow.nfo",strNfoFile);

    if (!strNfoFile.IsEmpty() && CFile::Exists(strNfoFile))
    {
      CLog::Log(LOGDEBUG,"Found matching nfo file: %s", strNfoFile.c_str());
      CNfoFile nfoReader(info.strContent);
      if (nfoReader.Create(strNfoFile) == S_OK)
      {
        if (nfoReader.m_strScraper == "NFO")
        {
          CLog::Log(LOGDEBUG, "%s Got details from nfo", __FUNCTION__);
          CVideoInfoTag movieDetails;
          nfoReader.GetDetails(movieDetails);
          if (m_pObserver)
            m_pObserver->OnSetTitle(movieDetails.m_strTitle);
          
          AddMovieAndGetThumb(pItem, info.strContent, movieDetails, -1, bGrabAny, pDlgProgress);
          if (info.strContent.Equals("tvshows"))
          {
            if (nfoReader.m_strScraper == "NFO") // see CNFOFile - used to pass assigned scraper
              info.strPath = nfoReader.m_strImDbNr;
            else
              info.strPath = nfoReader.m_strScraper;
          }

          return FULL_NFO;
        }
        else
        {
          CScraperUrl url(nfoReader.m_strImDbUrl);
          scrUrl = url;
          CLog::Log(LOGDEBUG,"-- nfo-scraper: %s", nfoReader.m_strScraper.c_str());
          CLog::Log(LOGDEBUG,"-- nfo url: %s", scrUrl.m_url[0].m_url.c_str());
          scrUrl.strId  = nfoReader.m_strImDbNr;
          info.strPath = nfoReader.m_strScraper;
          if (m_pObserver)
          {
            CStdString strPath = pItem->m_strPath;
            if (pItem->IsStack())
            {
              CStackDirectory dir;
              strPath = dir.GetStackedTitlePath(pItem->m_strPath);
            }

            m_pObserver->OnSetTitle(CUtil::GetFileName(strPath));
          }
          GetIMDBDetails(pItem, scrUrl, info, bGrabAny, pDlgProgress);

          return URL_NFO;
        }
      }
    }

    return NO_NFO;
  }
}

