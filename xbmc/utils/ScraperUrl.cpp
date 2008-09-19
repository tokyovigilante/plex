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
#include "ScraperUrl.h"
#include "Settings.h"
#include "HTMLUtil.h"
#include "CharsetConverter.h"
#include "URL.h"
#include "HTTP.h"
#include "FileSystem/FileZip.h"
#include "Picture.h"
#include "Util.h"

#include <cstring>
#include <sstream>

using namespace std;

CScraperUrl::CScraperUrl(const CStdString& strUrl)
{
  ParseString(strUrl);
}

CScraperUrl::CScraperUrl(const TiXmlElement* element)
{
  ParseElement(element);
}

CScraperUrl::CScraperUrl()
{
}

CScraperUrl::~CScraperUrl()
{
}

void CScraperUrl::Clear()
{
  m_url.clear();
  m_spoof.clear();
  m_xml.clear();
}

bool CScraperUrl::Parse()
{
  CStdString strToParse = m_xml;
  m_xml.Empty();
  return ParseString(strToParse);
}

bool CScraperUrl::ParseElement(const TiXmlElement* element)
{
  if (!element || !element->FirstChild()) return false;

  std::stringstream stream;
  stream << *element;
  m_xml += stream.str();
  bool bHasChilds = false;
  if (element->FirstChildElement("thumb"))
  {
    element = element->FirstChildElement("thumb");
    bHasChilds = true;
  }
  else if (element->FirstChildElement("url"))
  {
    element = element->FirstChildElement("url");
    bHasChilds = true;
  }
  while (element)
  {
    SUrlEntry url;
    url.m_url = element->FirstChild()->Value();
    const char* pSpoof = element->Attribute("spoof");
    if (pSpoof)
      url.m_spoof = pSpoof;
    const char* szPost=element->Attribute("post");
    if (szPost && stricmp(szPost,"yes") == 0)
      url.m_post = true;
    else
      url.m_post = false;
    const char* pCache = element->Attribute("cache");
    if (pCache)
      url.m_cache = pCache;

    const char* szType = element->Attribute("type");
    url.m_type = URL_TYPE_GENERAL;
    if (szType && stricmp(szType,"season") == 0)
    {
      url.m_type = URL_TYPE_SEASON;
      const char* szSeason = element->Attribute("season");
      if (szSeason)
        url.m_season = atoi(szSeason);
      else
        url.m_season = -1;
    }
    else
      url.m_season = -1;

    m_url.push_back(url);
    if (bHasChilds)
    {
      const TiXmlElement* temp = element->NextSiblingElement("thumb");
      if (temp)
        element = temp;
      else
        element = element->NextSiblingElement("url");
    }
    else
      element = NULL;
  }
  return true;
}

bool CScraperUrl::ParseString(CStdString strUrl)
{
  if (strUrl.IsEmpty())
    return false;

  // ok, now parse the xml file
  if (strUrl.Find("encoding=\"utf-8\"") < 0)
    g_charsetConverter.stringCharsetToUtf8(strUrl);

  TiXmlDocument doc;
  doc.Parse(strUrl.c_str(),0,TIXML_ENCODING_UTF8);
  m_xml += strUrl;

  TiXmlElement* pElement = doc.RootElement();
  if (pElement)
    ParseElement(pElement);
  else
  {
    SUrlEntry url;
    url.m_url = strUrl;
    url.m_type = URL_TYPE_GENERAL;
    url.m_season = -1;
    url.m_post = false;
    m_url.push_back(url);
  }
  return true;
}

const CScraperUrl::SUrlEntry CScraperUrl::GetFirstThumb() const
{
  for (std::vector<SUrlEntry>::const_iterator iter=m_url.begin();iter != m_url.end();++iter)
  {
    if (iter->m_type == URL_TYPE_GENERAL)
      return *iter;
  }
  SUrlEntry result;
  result.m_season = -1;
  return result;
}

const CScraperUrl::SUrlEntry CScraperUrl::GetSeasonThumb(int season) const
{
  for (std::vector<SUrlEntry>::const_iterator iter=m_url.begin();iter != m_url.end();++iter)
  {
    if (iter->m_type == URL_TYPE_SEASON && iter->m_season == season)
      return *iter;
  }
  SUrlEntry result;
  result.m_season = -1;
  return result;
}

bool CScraperUrl::Get(const SUrlEntry& scrURL, string& strHTML, CHTTP& http)
{
  CURL url(scrURL.m_url);
  http.SetReferer(scrURL.m_spoof);
  CStdString strCachePath;

  if (!scrURL.m_cache.IsEmpty())
  {
    CUtil::AddFileToFolder(g_advancedSettings.m_cachePath,"scrapers\\"+scrURL.m_cache,strCachePath);
    if (XFILE::CFile::Exists(strCachePath))
    {
      XFILE::CFile file;
      file.Open(strCachePath);
      char* temp = new char[(int)file.GetLength()];
      file.Read(temp,file.GetLength());
      strHTML.append(temp,temp+file.GetLength());
      file.Close();
      delete[] temp;
      return true;
    }
  }

  if (scrURL.m_post)
  {
    CStdString strOptions = url.GetOptions();
    strOptions = strOptions.substr(1);
    url.SetOptions("");
    CStdString strUrl;
    url.GetURL(strUrl);

    if (!http.Post(strUrl, strOptions, strHTML))
      return false;
  }
  else
    if (!http.Get(scrURL.m_url, strHTML))
      return false;

  if (scrURL.m_url.Find(".zip") > -1)
  {
    XFILE::CFileZip file;
    CStdString strBuffer;
    int iSize = file.UnpackFromMemory(strBuffer,strHTML);
    if (iSize)
    {
      strHTML.clear();
      strHTML.append(strBuffer.c_str(),strBuffer.data()+iSize);
    }
  }

  if (!scrURL.m_cache.IsEmpty())
  {
    CStdString strCachePath;
    CUtil::AddFileToFolder(g_advancedSettings.m_cachePath,"scrapers\\"+scrURL.m_cache,strCachePath);
    XFILE::CFile file;
    if (file.OpenForWrite(strCachePath,true,true))
      file.Write(strHTML.data(),strHTML.size());
    file.Close();
  }
  return true;
}

bool CScraperUrl::DownloadThumbnail(const CStdString &thumb, const CScraperUrl::SUrlEntry& entry)
{
  if (entry.m_url.IsEmpty())
    return false;

  CURL url(entry.m_url);
  if (url.GetProtocol() != "http")
  { // do a direct file copy
    try
    {
      CPicture picture;
      return picture.DoCreateThumbnail(entry.m_url, thumb);
    }
    catch (...)
    {
      ::DeleteFile(thumb.c_str());
    }
    return false;
  }

  CHTTP http;
  http.SetReferer(entry.m_spoof);
  string thumbData;
  if (http.Get(entry.m_url, thumbData))
  {
    try
    {
      CPicture picture;
      return picture.CreateThumbnailFromMemory((const BYTE *)thumbData.c_str(), thumbData.size(), CUtil::GetExtension(entry.m_url), thumb);
    }
    catch (...)
    {
      ::DeleteFile(thumb.c_str());
    }
  }
  return false;
}

bool CScraperUrl::ParseEpisodeGuide(CStdString strUrls)
{
  if (strUrls.IsEmpty())
    return false;

  // ok, now parse the xml file
  if (strUrls.Find("encoding=\"utf-8\"") < 0)
    g_charsetConverter.stringCharsetToUtf8(strUrls);

  TiXmlDocument doc;
  doc.Parse(strUrls.c_str(),0,TIXML_ENCODING_UTF8);
  if (doc.RootElement())
  {
    TiXmlHandle docHandle( &doc );
    TiXmlElement *link = docHandle.FirstChild( "episodeguide" ).FirstChild( "url" ).Element();
    while (link)
    {
      ParseElement(link);
      link = link->NextSiblingElement("url");
    }
  }
  else
    return false;

  return true;
}
