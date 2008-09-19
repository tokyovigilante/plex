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
#include "FileCurl.h"
#include "Util.h"
#include "URL.h"
#include "Settings.h"

#include <sys/stat.h>

#ifdef _LINUX
#include <errno.h>
#include <inttypes.h>
#include "../linux/XFileUtils.h"
#endif

#include "DllLibCurl.h"
#include "FileShoutcast.h"

using namespace XFILE;
using namespace XCURL;

#define XMIN(a,b) ((a)<(b)?(a):(b))

#if defined(__APPLE__)
extern "C" int __stdcall dllselect(int ntfs, fd_set *readfds, fd_set *writefds, fd_set *errorfds, const timeval *timeout);
#define dllselect select
#else
#define dllselect select
#endif

// curl calls this routine to debug
extern "C" int debug_callback(CURL_HANDLE *handle, curl_infotype info, char *output, size_t size, void *data)
{
  if (info == CURLINFO_DATA_IN || info == CURLINFO_DATA_OUT)
    return 0;
  char *pOut = new char[size + 1];
  strncpy(pOut, output, size);
  pOut[size] = '\0';
  CStdString strOut = pOut;
  delete[] pOut;
  strOut.TrimRight("\r\n");
  CLog::Log(LOGDEBUG, "Curl:: Debug %s", strOut.c_str());
  return 0;
}

/* curl calls this routine to get more data */
extern "C" size_t write_callback(char *buffer,
               size_t size,
               size_t nitems,
               void *userp)
{
  if(userp == NULL) return 0;
 
  CFileCurl::CReadState *state = (CFileCurl::CReadState *)userp;
  return state->WriteCallback(buffer, size, nitems);
}

extern "C" size_t header_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
  CFileCurl::CReadState *state = (CFileCurl::CReadState *)stream;
  return state->HeaderCallback(ptr, size, nmemb);
}

/* fix for silly behavior of realloc */
static inline void* realloc_simple(void *ptr, size_t size)
{
  void *ptr2 = realloc(ptr, size);
  if(ptr && !ptr2 && size > 0)
  {
    free(ptr);
    return NULL;
  }
  else
    return ptr2;
}

size_t CFileCurl::CReadState::HeaderCallback(void *ptr, size_t size, size_t nmemb)
{
  // libcurl doc says that this info is not always \0 terminated
  char* strData = (char*)ptr;
  int iSize = size * nmemb;
  
  if (strData[iSize] != 0)
  {
    strData = (char*)malloc(iSize + 1);
    strncpy(strData, (char*)ptr, iSize);
    strData[iSize] = 0;
  }
  else strData = strdup((char*)ptr);
  
  m_httpheader.Parse(strData);

  free(strData);
  
  return iSize;
}

size_t CFileCurl::CReadState::WriteCallback(char *buffer, size_t size, size_t nitems)
{
  unsigned int amount = size * nitems;
//  CLog::Log(LOGDEBUG, "CFileCurl::WriteCallback (%p) with %i bytes, readsize = %i, writesize = %i", this, amount, m_buffer.GetMaxReadSize(), m_buffer.GetMaxWriteSize() - m_overflowSize);
  if (m_overflowSize)
  {
    // we have our overflow buffer - first get rid of as much as we can
    unsigned int maxWriteable = XMIN((unsigned int)m_buffer.GetMaxWriteSize(), m_overflowSize);
    if (maxWriteable)
    {
      if (!m_buffer.WriteBinary(m_overflowBuffer, maxWriteable))
        CLog::Log(LOGERROR, "Unable to write to buffer - what's up?");
      if (m_overflowSize > maxWriteable)
      { // still have some more - copy it down
        memmove(m_overflowBuffer, m_overflowBuffer + maxWriteable, m_overflowSize - maxWriteable);
      }
      m_overflowSize -= maxWriteable;
    }
  }
  // ok, now copy the data into our ring buffer
  unsigned int maxWriteable = XMIN((unsigned int)m_buffer.GetMaxWriteSize(), amount);
  if (maxWriteable)
  {
    if (!m_buffer.WriteBinary(buffer, maxWriteable))
      CLog::Log(LOGERROR, "Unable to write to buffer - what's up?");
    amount -= maxWriteable;
    buffer += maxWriteable;
  }
  if (amount)
  {
    CLog::Log(LOGDEBUG, "CFileCurl::WriteCallback(%p) not enough free space for %i bytes", (void*)this,  amount); 
    
    m_overflowBuffer = (char*)realloc_simple(m_overflowBuffer, amount + m_overflowSize);
    if(m_overflowBuffer == NULL)
    {
      CLog::Log(LOGDEBUG, "%s - Failed to grow overflow buffer", __FUNCTION__);
      return 0;
    }
    memcpy(m_overflowBuffer + m_overflowSize, buffer, amount);
    m_overflowSize += amount;
  }
  return size * nitems;
}

CFileCurl::CReadState::CReadState()
{
  m_easyHandle = NULL;
  m_multiHandle = NULL;
  m_overflowBuffer = NULL;
  m_overflowSize = 0;
	m_filePos = 0;
	m_fileSize = 0;
  m_bufferSize = 0;
}

CFileCurl::CReadState::~CReadState()
{
  Disconnect();

  if(m_easyHandle)
    g_curlInterface.easy_release(&m_easyHandle, &m_multiHandle);
}

bool CFileCurl::CReadState::Seek(__int64 pos)
{
  if(pos == m_filePos)
    return true;

  if(m_buffer.SkipBytes((int)(pos - m_filePos)))
  {
    m_filePos = pos;
    return true;
  }

  if(pos > m_filePos && pos < m_filePos + m_bufferSize)
  {
    int len = m_buffer.GetMaxReadSize();
    m_filePos += len;
    m_buffer.SkipBytes(len);
    if(!FillBuffer(m_bufferSize))
    {
      if(!m_buffer.SkipBytes(-len))
        CLog::Log(LOGERROR, "%s - Failed to restore position after failed fill", __FUNCTION__);
      else
        m_filePos -= len;
      return false;
    }

    if(!m_buffer.SkipBytes((int)(pos - m_filePos)))
    {
      CLog::Log(LOGERROR, "%s - Failed to skip to position after having filled buffer", __FUNCTION__);
      if(!m_buffer.SkipBytes(-len))
        CLog::Log(LOGERROR, "%s - Failed to restore position after failed seek", __FUNCTION__);
      else
        m_filePos -= len;
      return false;
    }
    m_filePos = pos;
    return true;
  }
  return false;
}

long CFileCurl::CReadState::Connect(unsigned int size)
{
  g_curlInterface.easy_setopt(m_easyHandle, CURLOPT_RESUME_FROM_LARGE, m_filePos);
  g_curlInterface.multi_add_handle(m_multiHandle, m_easyHandle);

  m_bufferSize = size;
  m_buffer.Destroy();
  m_buffer.Create(size * 3);

  // read some data in to try and obtain the length
  // maybe there's a better way to get this info??
  m_stillRunning = 1;
  if (!FillBuffer(1))
  {
    CLog::Log(LOGERROR, "CFileCurl::CReadState::Open, didn't get any data from stream.");    
    return -1;
  }

  double length;
  if (CURLE_OK == g_curlInterface.easy_getinfo(m_easyHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &length))
    m_fileSize = m_filePos + (__int64)length;

  long response;
  if (CURLE_OK == g_curlInterface.easy_getinfo(m_easyHandle, CURLINFO_RESPONSE_CODE, &response))
    return response;

  return -1;
}

void CFileCurl::CReadState::Disconnect()
{
  if(m_multiHandle && m_easyHandle)
    g_curlInterface.multi_remove_handle(m_multiHandle, m_easyHandle);

  m_buffer.Clear();
  if (m_overflowBuffer)
    free(m_overflowBuffer);
  m_overflowBuffer = NULL;
  m_overflowSize = 0;
}

CFileCurl::~CFileCurl()
{ 
  Close();
  delete m_state;
  g_curlInterface.Unload();
}

CFileCurl::CFileCurl()
{
  g_curlInterface.Load(); // loads the curl dll and resolves exports etc.
  m_curlAliasList = NULL;
  m_curlHeaderList = NULL;
  m_opened = false;
  m_multisession  = true;
  m_seekable = true;
  m_useOldHttpVersion = false;
  m_timeout = 0;
  m_ftpauth = "";
  m_ftpport = "";
  m_ftppasvip = false;
  m_bufferSize = 32768;
  m_binary = true;
  m_state = new CReadState();
}

//Has to be called before Open()
void CFileCurl::SetBufferSize(unsigned int size)
{
  m_bufferSize = size;
}

void CFileCurl::Close()
{
  CLog::Log(LOGDEBUG, "FileCurl::Close(%p) %s", (void*)this, m_url.c_str());
  m_opened = false;
  m_state->Disconnect();

  m_url.Empty();
  
  /* cleanup */
  if( m_curlAliasList )
    g_curlInterface.slist_free_all(m_curlAliasList);
  if( m_curlHeaderList )
    g_curlInterface.slist_free_all(m_curlHeaderList);
  
  m_curlAliasList = NULL;
  m_curlHeaderList = NULL;
}

void CFileCurl::SetCommonOptions(CReadState* state)
{
  CURL_HANDLE* h = state->m_easyHandle;

  g_curlInterface.easy_reset(h);

  g_curlInterface.easy_setopt(h, CURLOPT_DEBUGFUNCTION, debug_callback);

  if( g_advancedSettings.m_logLevel >= LOG_LEVEL_DEBUG )
    g_curlInterface.easy_setopt(h, CURLOPT_VERBOSE, TRUE);
  else
    g_curlInterface.easy_setopt(h, CURLOPT_VERBOSE, FALSE);
  
  g_curlInterface.easy_setopt(h, CURLOPT_WRITEDATA, state);
  g_curlInterface.easy_setopt(h, CURLOPT_WRITEFUNCTION, write_callback);
  
  // make sure headers are seperated from the data stream
  g_curlInterface.easy_setopt(h, CURLOPT_WRITEHEADER, state);
  g_curlInterface.easy_setopt(h, CURLOPT_HEADERFUNCTION, header_callback);
  g_curlInterface.easy_setopt(h, CURLOPT_HEADER, FALSE);
  
  g_curlInterface.easy_setopt(h, CURLOPT_FTP_USE_EPSV, 0); // turn off epsv
  
  // Allow us to follow two redirects
  g_curlInterface.easy_setopt(h, CURLOPT_FOLLOWLOCATION, TRUE);
  g_curlInterface.easy_setopt(h, CURLOPT_MAXREDIRS, 5);

  // When using multiple threads you should set the CURLOPT_NOSIGNAL option to
  // TRUE for all handles. Everything will work fine except that timeouts are not
  // honored during the DNS lookup - which you can work around by building libcurl
  // with c-ares support. c-ares is a library that provides asynchronous name
  // resolves. Unfortunately, c-ares does not yet support IPv6.
  g_curlInterface.easy_setopt(h, CURLOPT_NOSIGNAL, TRUE);

  // not interested in failed requests
  g_curlInterface.easy_setopt(h, CURLOPT_FAILONERROR, 1);

  // enable support for icecast / shoutcast streams
  m_curlAliasList = g_curlInterface.slist_append(m_curlAliasList, "ICY 200 OK"); 
  g_curlInterface.easy_setopt(h, CURLOPT_HTTP200ALIASES, m_curlAliasList); 

  // never verify peer, we don't have any certificates to do this
  g_curlInterface.easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0);
  g_curlInterface.easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0);

  g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_URL, m_url.c_str());
  g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_TRANSFERTEXT, m_binary ? FALSE : TRUE);

  // setup any requested authentication
  if( m_ftpauth.length() > 0 )
  {
    g_curlInterface.easy_setopt(h, CURLOPT_FTP_SSL, CURLFTPSSL_TRY);
    if( m_ftpauth.Equals("any") )
      g_curlInterface.easy_setopt(h, CURLOPT_FTPSSLAUTH, CURLFTPAUTH_DEFAULT);
    else if( m_ftpauth.Equals("ssl") )
      g_curlInterface.easy_setopt(h, CURLOPT_FTPSSLAUTH, CURLFTPAUTH_SSL);
    else if( m_ftpauth.Equals("tls") )
      g_curlInterface.easy_setopt(h, CURLOPT_FTPSSLAUTH, CURLFTPAUTH_TLS);
  }

  // allow passive mode for ftp
  if( m_ftpport.length() > 0 )
    g_curlInterface.easy_setopt(h, CURLOPT_FTPPORT, m_ftpport.c_str());
  else
    g_curlInterface.easy_setopt(h, CURLOPT_FTPPORT, NULL);

  // allow curl to not use the ip address in the returned pasv response
  if( m_ftppasvip )
    g_curlInterface.easy_setopt(h, CURLOPT_FTP_SKIP_PASV_IP, 0);
  else
    g_curlInterface.easy_setopt(h, CURLOPT_FTP_SKIP_PASV_IP, 1);

  // always allow gzip compression
  if( m_contentencoding.length() > 0 )
    g_curlInterface.easy_setopt(h, CURLOPT_ENCODING, m_contentencoding.c_str());
  
  if (m_userAgent.length() > 0)
    g_curlInterface.easy_setopt(h, CURLOPT_USERAGENT, m_userAgent.c_str());
  else /* set some default agent as shoutcast doesn't return proper stuff otherwise */
    g_curlInterface.easy_setopt(h, CURLOPT_USERAGENT, "XBMC/pre-2.1 (compatible; MSIE 6.0; Windows NT 5.1; WinampMPEG/5.09)");
  
  if (m_useOldHttpVersion)
    g_curlInterface.easy_setopt(h, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
  else
    SetRequestHeader("Connection", "keep-alive");

  if (m_proxy.length() > 0)
    g_curlInterface.easy_setopt(h, CURLOPT_PROXY, m_proxy.c_str());

  if (m_customrequest.length() > 0)
    g_curlInterface.easy_setopt(h, CURLOPT_CUSTOMREQUEST, m_customrequest.c_str());

  if(m_timeout == 0)
    m_timeout = g_advancedSettings.m_curlclienttimeout;

  // set our timeouts, we abort connection after m_timeout, and reads after no data for m_timeout seconds
  g_curlInterface.easy_setopt(h, CURLOPT_CONNECTTIMEOUT, m_timeout);
  g_curlInterface.easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1);
  g_curlInterface.easy_setopt(h, CURLOPT_LOW_SPEED_TIME, m_timeout);
}

void CFileCurl::SetRequestHeaders(CReadState* state)
{
  if(m_curlHeaderList) 
  {
    g_curlInterface.slist_free_all(m_curlHeaderList);
    m_curlHeaderList = NULL;
  }

  MAPHTTPHEADERS::iterator it;
  for(it = m_requestheaders.begin(); it != m_requestheaders.end(); it++)
  {
    CStdString buffer = it->first + ": " + it->second;
    m_curlHeaderList = g_curlInterface.slist_append(m_curlHeaderList, buffer.c_str()); 
  }

  // add user defined headers
  if (m_curlHeaderList && state->m_easyHandle)
    g_curlInterface.easy_setopt(state->m_easyHandle, CURLOPT_HTTPHEADER, m_curlHeaderList); 

}

void CFileCurl::SetCorrectHeaders(CReadState* state)
{
  CHttpHeader& h = state->m_httpheader;
  /* workaround for shoutcast server wich doesn't set content type on standard mp3 */
  if( h.GetContentType().IsEmpty() )
  {
    if( !h.GetValue("icy-notice1").IsEmpty()
    || !h.GetValue("icy-name").IsEmpty()
    || !h.GetValue("icy-br").IsEmpty() )
    h.Parse("Content-Type: audio/mpeg\r\n");
  }

  /* hack for google video */
  if ( h.GetContentType().Equals("text/html") 
  &&  !h.GetValue("Content-Disposition").IsEmpty() )
  {
    CStdString strValue = h.GetValue("Content-Disposition");
    if (strValue.Find("filename=") > -1 && strValue.Find(".flv") > -1)
      h.Parse("Content-Type: video/flv\r\n");
  }
}

void CFileCurl::ParseAndCorrectUrl(CURL &url2)
{
  if( url2.GetProtocol().Equals("ftpx") )
    url2.SetProtocol("ftp");
  else if( url2.GetProtocol().Equals("shout") 
       ||  url2.GetProtocol().Equals("daap") 
       ||  url2.GetProtocol().Equals("upnp") 
       ||  url2.GetProtocol().Equals("tuxbox") 
       ||  url2.GetProtocol().Equals("lastfm")
       ||  url2.GetProtocol().Equals("mms"))
    url2.SetProtocol("http");    

  if( url2.GetProtocol().Equals("ftp")
  ||  url2.GetProtocol().Equals("ftps") )
  {
    /* this is uggly, depending on from where   */
    /* we get the link it may or may not be     */
    /* url encoded. if handed from ftpdirectory */
    /* it won't be so let's handle that case    */
    
    CStdString partial, filename(url2.GetFileName());
    CStdStringArray array;

    /* our current client doesn't support utf8 */
    g_charsetConverter.utf8ToStringCharset(filename);

    /* TODO: create a tokenizer that doesn't skip empty's */
    CUtil::Tokenize(filename, array, "/");
    filename.Empty();
    for(CStdStringArray::iterator it = array.begin(); it != array.end(); it++)
    {
      if(it != array.begin())
        filename += "/";

      partial = *it;      
      CUtil::URLEncode(partial);      
      filename += partial;
    }

    /* make sure we keep slashes */    
    if(url2.GetFileName().Right(1) == "/")
      filename += "/";

    url2.SetFileName(filename);

    CStdString options = url2.GetOptions().Mid(1);
    options.TrimRight('/'); // hack for trailing slashes being added from source

    m_ftpauth = "";
    m_ftpport = "";
    m_ftppasvip = false;

    /* parse options given */
    CUtil::Tokenize(options, array, "&");
    for(CStdStringArray::iterator it = array.begin(); it != array.end(); it++)
    {
      CStdString name, value;
      int pos = it->Find('=');
      if(pos >= 0)
      {
        name = it->Left(pos);
        value = it->Mid(pos+1, it->size());
      }
      else
      {
        name = (*it);
        value = "";
      }      

      if(name.Equals("auth"))
      {
        m_ftpauth = value;
        if(m_ftpauth.IsEmpty())
          m_ftpauth = "any";
      }
      else if(name.Equals("active"))
      {
        m_ftpport = value;
        if(value.IsEmpty())
          m_ftpport = "-";
      }
      else if(name.Equals("pasvip"))
      {        
        if(value == "0")
          m_ftppasvip = false;
        else
          m_ftppasvip = true;
      }
    }

    /* ftp has no options */
    url2.SetOptions("");
  }
  else if( url2.GetProtocol().Equals("http")
       ||  url2.GetProtocol().Equals("https"))
  {
    if (g_guiSettings.GetBool("network.usehttpproxy") && m_proxy.IsEmpty())
    {
      m_proxy = "http://" + g_guiSettings.GetString("network.httpproxyserver");
      m_proxy += ":" + g_guiSettings.GetString("network.httpproxyport");
      CLog::Log(LOGDEBUG, "Using proxy %s", m_proxy.c_str());
    }
  }
}

bool CFileCurl::Open(const CURL& url, bool bBinary)
{

  CURL url2(url);
  ParseAndCorrectUrl(url2);

  url2.GetURL(m_url);
  m_binary = bBinary;

  CLog::Log(LOGDEBUG, "FileCurl::Open(%p) %s", (void*)this, m_url.c_str());  

  ASSERT(!(!m_state->m_easyHandle ^ !m_state->m_multiHandle));
  if( m_state->m_easyHandle == NULL )
    g_curlInterface.easy_aquire(url2.GetProtocol(), url2.GetHostName(), &m_state->m_easyHandle, &m_state->m_multiHandle );

  // setup common curl options
  SetCommonOptions(m_state);
  SetRequestHeaders(m_state);

  m_opened = true;

  long response = m_state->Connect(m_bufferSize);
  if( response < 0 )
    return false;
  
  SetCorrectHeaders(m_state);

  // check if this stream is a shoutcast stream. sometimes checking the protocol line is not enough so examine other headers as well.
  // shoutcast streams should be handled by FileShoutcast.
  if (m_state->m_httpheader.GetProtoLine().Left(3) == "ICY" || !m_state->m_httpheader.GetValue("icy-notice1").IsEmpty()
     || !m_state->m_httpheader.GetValue("icy-name").IsEmpty()
     || !m_state->m_httpheader.GetValue("icy-br").IsEmpty() )
  {
    CLog::Log(LOGDEBUG,"FileCurl - file <%s> is a shoutcast stream. re-opening", m_url.c_str());
    throw new CRedirectException(new CFileShoutcast); 
  }

  m_multisession = false;
  if(m_url.Left(5).Equals("http:") || m_url.Left(6).Equals("https:"))
  {
    m_multisession = true;
    if(m_state->m_httpheader.GetValue("Server").Find("Portable SDK for UPnP devices") >= 0)
    {
      CLog::Log(LOGWARNING, "FileCurl - disabling multi session due to broken libupnp server");
      m_multisession = false;
    }
  }

  m_seekable = false;
  if(m_state->m_fileSize > 0)
    m_seekable = true;

  return true;
}

bool CFileCurl::CReadState::ReadString(char *szLine, int iLineLength)
{
  unsigned int want = (unsigned int)iLineLength;

  if((m_fileSize == 0 || m_filePos < m_fileSize) && !FillBuffer(want))
    return false;

  // ensure only available data is considered 
  want = XMIN((unsigned int)m_buffer.GetMaxReadSize(), want);

  /* check if we finished prematurely */
  if (!m_stillRunning && (m_fileSize == 0 || m_filePos != m_fileSize) && !want)
  {
    CLog::Log(LOGWARNING, "%s - Transfer ended before entire file was retreived pos %" PRId64 ", size %" PRId64, __FUNCTION__, m_filePos, m_fileSize);
    return false;
  }

  char* pLine = szLine;
  do
  {
    if (!m_buffer.ReadBinary(pLine, 1))
      break;

    pLine++;
  } while (((pLine - 1)[0] != '\n') && ((unsigned int)(pLine - szLine) < want));
  pLine[0] = 0;
  m_filePos += (pLine - szLine);
  return (bool)((pLine - szLine) > 0);
}

bool CFileCurl::Exists(const CURL& url)
{
  return Stat(url, NULL) == 0;
}


__int64 CFileCurl::Seek(__int64 iFilePosition, int iWhence)
{
  __int64 nextPos = m_state->m_filePos;
	switch(iWhence) 
	{
		case SEEK_SET:
			nextPos = iFilePosition;
			break;
		case SEEK_CUR:
			nextPos += iFilePosition;
			break;
		case SEEK_END:
			if (m_state->m_fileSize)
        nextPos = m_state->m_fileSize + iFilePosition;
      else
        return -1;
			break;
    case SEEK_POSSIBLE:
      return m_seekable ? 1 : 0;
    default:
      return -1;
	}

  if(m_state->Seek(nextPos))
    return nextPos;

  if(!m_seekable)
    return -1;

  CReadState* oldstate = NULL;
  if(!(m_url.Find(":31339") >= 0) && m_multisession)
  {
    CURL url(m_url);
    oldstate = m_state;
    m_state = new CReadState();

    g_curlInterface.easy_aquire(url.GetProtocol(), url.GetHostName(), &m_state->m_easyHandle, &m_state->m_multiHandle );

    // setup common curl options
    SetCommonOptions(m_state);
  }
  else
    m_state->Disconnect();

  /* caller might have changed some headers (needed for daap)*/
  SetRequestHeaders(m_state);

  m_state->m_filePos = nextPos;
  long response = m_state->Connect(m_bufferSize);
  if(response < 0)
  {
    m_seekable = false;
    if(oldstate)
    {
      delete m_state;
      m_state = oldstate;
    }
    return -1;
  }

  SetCorrectHeaders(m_state);

  if(oldstate)
    delete oldstate;

  return m_state->m_filePos;
}

__int64 CFileCurl::GetLength()
{
	if (!m_opened) return 0;
	return m_state->m_fileSize;
}

__int64 CFileCurl::GetPosition()
{
	if (!m_opened) return 0;
	return m_state->m_filePos;
}

int CFileCurl::Stat(const CURL& url, struct __stat64* buffer)
{ 
  // if file is already running, get infor from it
  if( m_opened )
  {
    CLog::Log(LOGWARNING, "%s - Stat called on open file", __FUNCTION__);
    buffer->st_size = GetLength();
		buffer->st_mode = _S_IFREG;
    return 0;
  }

  CURL url2(url);
  ParseAndCorrectUrl(url2);

  url2.GetURL(m_url);

  ASSERT(m_state->m_easyHandle == NULL);
  g_curlInterface.easy_aquire(url2.GetProtocol(), url2.GetHostName(), &m_state->m_easyHandle, NULL);

  SetCommonOptions(m_state); 
  SetRequestHeaders(m_state);
  g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_TIMEOUT, 5);
  g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_NOBODY, 1);
  g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_WRITEDATA, NULL); /* will cause write failure*/

  CURLcode result = g_curlInterface.easy_perform(m_state->m_easyHandle);

  
  if (result == CURLE_GOT_NOTHING)
  {
    /* some http servers and shoutcast servers don't give us any data on a head request */
    /* request normal and just fail out, it's their loss */
    /* somehow curl doesn't reset CURLOPT_NOBODY properly so reset everything */    
    SetCommonOptions(m_state);
    SetRequestHeaders(m_state);
    g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_TIMEOUT, 5);
    g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_RANGE, "0-0");
    g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_WRITEDATA, NULL); /* will cause write failure*/    
    result = g_curlInterface.easy_perform(m_state->m_easyHandle);
  }

  if(result == CURLE_HTTP_RANGE_ERROR )
  {
    /* crap can't use the range option, disable it and try again */
    g_curlInterface.easy_setopt(m_state->m_easyHandle, CURLOPT_RANGE, NULL);
    result = g_curlInterface.easy_perform(m_state->m_easyHandle);
  }

  if( result != CURLE_WRITE_ERROR && result != CURLE_OK )
  {
    g_curlInterface.easy_release(&m_state->m_easyHandle, NULL);
    errno = ENOENT;
    return -1;
  }

  SetCorrectHeaders(m_state);

  if(buffer)
  {

    double length;
    char content[255];
    if (CURLE_OK == g_curlInterface.easy_getinfo(m_state->m_easyHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &length)
     && CURLE_OK == g_curlInterface.easy_getinfo(m_state->m_easyHandle, CURLINFO_CONTENT_TYPE, content))
    {
		  buffer->st_size = (__int64)length;
      if(strstr(content, "text/html")) //consider html files directories
        buffer->st_mode = _S_IFDIR;
      else
        buffer->st_mode = _S_IFREG;      
    }
  }
  g_curlInterface.easy_release(&m_state->m_easyHandle, NULL);
	return 0;
}

unsigned int CFileCurl::CReadState::Read(void* lpBuf, __int64 uiBufSize)
{
  /* only request 1 byte, for truncated reads (only if not eof) */
  if((m_fileSize == 0 || m_filePos < m_fileSize) && !FillBuffer(1))
    return 0;

  /* ensure only available data is considered */
  unsigned int want = (unsigned int)XMIN(m_buffer.GetMaxReadSize(), uiBufSize);

  /* xfer data to caller */
  if (m_buffer.ReadBinary((char *)lpBuf, want))
  {
    m_filePos += want;
    return want;
  }  

  /* check if we finished prematurely */
  if (!m_stillRunning && (m_fileSize == 0 || m_filePos != m_fileSize))
  {
    CLog::Log(LOGWARNING, "%s - Transfer ended before entire file was retreived pos %"PRId64", size %"PRId64, __FUNCTION__, m_filePos, m_fileSize);
    return 0;
  }

  return 0;
}

/* use to attempt to fill the read buffer up to requested number of bytes */
bool CFileCurl::CReadState::FillBuffer(unsigned int want)
{  
  int maxfd;  
  fd_set fdread;
  fd_set fdwrite;
  fd_set fdexcep;

  // only attempt to fill buffer if transactions still running and buffer
  // doesnt exceed required size already
  while ((unsigned int)m_buffer.GetMaxReadSize() < want && m_buffer.GetMaxWriteSize() > 0 )
  {
    /* if there is data in overflow buffer, try to use that first */
    if(m_overflowSize)
    {
      unsigned amount = XMIN((unsigned int)m_buffer.GetMaxWriteSize(), m_overflowSize);
      m_buffer.WriteBinary(m_overflowBuffer, amount);

      if(amount < m_overflowSize)
        memcpy(m_overflowBuffer, m_overflowBuffer+amount,m_overflowSize-amount);

      m_overflowSize -= amount;
      m_overflowBuffer = (char*)realloc_simple(m_overflowBuffer, m_overflowSize);
      continue;
    }

    CURLMcode result = g_curlInterface.multi_perform(m_multiHandle, &m_stillRunning);
    if( !m_stillRunning )
    {
      if( result == CURLM_OK )
      {
        /* if we still have stuff in buffer, we are fine */
        if( m_buffer.GetMaxReadSize() )
          return true;

        /* verify that we are actually okey */
        int msgs;
        CURLMsg* msg;
        while((msg = g_curlInterface.multi_info_read(m_multiHandle, &msgs)))
        {
          if (msg->msg == CURLMSG_DONE)
            return (msg->data.result == CURLE_OK);
        }
      }
      return false;
    }
    switch(result)
    {
      case CURLM_OK:
      {
        // hack for broken curl, that thinks there is data all the time
        // happens especially on ftp during initial connection
#ifndef _LINUX
        SwitchToThread();
#elif __APPLE__
        sched_yield();
#else
	    pthread_yield();
#endif

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);

        // get file descriptors from the transfers
        if( CURLM_OK != g_curlInterface.multi_fdset(m_multiHandle, &fdread, &fdwrite, &fdexcep, &maxfd) || maxfd == -1 )
          return false;

        long timeout = 0;
        if(CURLM_OK != g_curlInterface.multi_timeout(m_multiHandle, &timeout) || timeout == -1)
          timeout = 200;

        if( maxfd >= 0  )
        {
          struct timeval t = { timeout / 1000, (timeout % 1000) * 1000 };          
          
          // wait until data is avialable or a timeout occours
          if( SOCKET_ERROR == dllselect(maxfd + 1, &fdread, &fdwrite, &fdexcep, &t) )
            return false;
        }
        else
          SleepEx(timeout, true);

      }
      break;
      case CURLM_CALL_MULTI_PERFORM:
      {
        // we don't keep calling here as that can easily overwrite our buffer wich we want to avoid
        // docs says we should call it soon after, but aslong as we are reading data somewhere
        // this aught to be soon enough. should stay in socket otherwise
        continue;
      }
      break;
      default:
      {
        CLog::Log(LOGERROR, "%s - curl multi perform failed with code %d, aborting", __FUNCTION__, result);
        return false;
      }
      break;
    }
  }
  return true;
}

void CFileCurl::ClearRequestHeaders()
{
  m_requestheaders.clear();
}

void CFileCurl::SetRequestHeader(CStdString header, CStdString value)
{
  m_requestheaders[header] = value;
}

void CFileCurl::SetRequestHeader(CStdString header, long value)
{
  CStdString buffer;
  buffer.Format("%ld", value);
  m_requestheaders[header] = buffer;
}

/* STATIC FUNCTIONS */
bool CFileCurl::GetHttpHeader(const CURL &url, CHttpHeader &headers)
{
  try
  {
    CFileCurl file;
    if(file.Stat(url, NULL) == 0)
    {
      headers = file.GetHttpHeader();
      return true;
    }
    return false;
  }
  catch(...)
  {
    CStdString path;
    url.GetURL(path);
    CLog::Log(LOGERROR, "%s - Exception thrown while trying to retrieve header url: %s", __FUNCTION__, path.c_str());
    return false;
  }
}

bool CFileCurl::GetContent(const CURL &url, CStdString &content)
{
   CFileCurl file;
   if( file.Stat(url, NULL) == 0 )
   {
     content = file.GetContent();
     return true;
   }
   
   content = "";
   return false;
}
