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
#include "Util.h"

using namespace std;

#define ICONV_PREPARE(iconv) iconv=(iconv_t)-1
#define ICONV_SAFE_CLOSE(iconv) if (iconv!=(iconv_t)-1) { iconv_close(iconv); iconv=(iconv_t)-1; }

size_t iconv_const (iconv_t cd, const char** inbuf, size_t *inbytesleft, 
		    char* * outbuf, size_t *outbytesleft)
{
#if defined(_LINUX) || defined(__APPLE__)
  return iconv(cd, (char**)inbuf, inbytesleft, outbuf, outbytesleft);
#else
  return iconv(cd, inbuf, inbytesleft, outbuf, outbytesleft);
#endif
}

CCharsetConverter g_charsetConverter;

CCharsetConverter::CCharsetConverter()
{
  m_vecCharsetNames.push_back("ISO-8859-1");
  m_vecCharsetLabels.push_back("Western Europe (ISO)");
  m_vecCharsetNames.push_back("ISO-8859-2");
  m_vecCharsetLabels.push_back("Central Europe (ISO)");
  m_vecCharsetNames.push_back("ISO-8859-3");
  m_vecCharsetLabels.push_back("South Europe (ISO)");
  m_vecCharsetNames.push_back("ISO-8859-4");
  m_vecCharsetLabels.push_back("Baltic (ISO)");
  m_vecCharsetNames.push_back("ISO-8859-5");
  m_vecCharsetLabels.push_back("Cyrillic (ISO)");
  m_vecCharsetNames.push_back("ISO-8859-6");
  m_vecCharsetLabels.push_back("Arabic (ISO)");
  m_vecCharsetNames.push_back("ISO-8859-7");
  m_vecCharsetLabels.push_back("Greek (ISO)");
  m_vecCharsetNames.push_back("ISO-8859-8");
  m_vecCharsetLabels.push_back("Hebrew (ISO)");
  m_vecCharsetNames.push_back("ISO-8859-9");
  m_vecCharsetLabels.push_back("Turkish (ISO)");

  m_vecCharsetNames.push_back("CP1250");
  m_vecCharsetLabels.push_back("Central Europe (Windows)");
  m_vecCharsetNames.push_back("CP1251");
  m_vecCharsetLabels.push_back("Cyrillic (Windows)");
  m_vecCharsetNames.push_back("CP1252");
  m_vecCharsetLabels.push_back("Western Europe (Windows)");
  m_vecCharsetNames.push_back("CP1253");
  m_vecCharsetLabels.push_back("Greek (Windows)");
  m_vecCharsetNames.push_back("CP1254");
  m_vecCharsetLabels.push_back("Turkish (Windows)");
  m_vecCharsetNames.push_back("CP1255");
  m_vecCharsetLabels.push_back("Hebrew (Windows)");
  m_vecCharsetNames.push_back("CP1256");
  m_vecCharsetLabels.push_back("Arabic (Windows)");
  m_vecCharsetNames.push_back("CP1257");
  m_vecCharsetLabels.push_back("Baltic (Windows)");
  m_vecCharsetNames.push_back("CP1258");
  m_vecCharsetLabels.push_back("Vietnamesse (Windows)");
  m_vecCharsetNames.push_back("CP874");
  m_vecCharsetLabels.push_back("Thai (Windows)");

  m_vecCharsetNames.push_back("BIG5");
  m_vecCharsetLabels.push_back("Chinese Traditional (Big5)");
  m_vecCharsetNames.push_back("GBK");
  m_vecCharsetLabels.push_back("Chinese Simplified (GBK)");
  m_vecCharsetNames.push_back("SHIFT_JIS");
  m_vecCharsetLabels.push_back("Japanese (Shift-JIS)");
  m_vecCharsetNames.push_back("CP949");
  m_vecCharsetLabels.push_back("Korean");
  m_vecCharsetNames.push_back("BIG5-HKSCS");
  m_vecCharsetLabels.push_back("Hong Kong (Big5-HKSCS)");

  m_vecBidiCharsetNames.push_back("ISO-8859-6");
  m_vecBidiCharsetNames.push_back("ISO-8859-8");
  m_vecBidiCharsetNames.push_back("CP1255");
  m_vecBidiCharsetNames.push_back("Windows-1255");
  m_vecBidiCharsetNames.push_back("CP1256");
  m_vecBidiCharsetNames.push_back("Windows-1256");
  m_vecBidiCharsets.push_back(FRIBIDI_CHAR_SET_ISO8859_6);
  m_vecBidiCharsets.push_back(FRIBIDI_CHAR_SET_ISO8859_8);
  m_vecBidiCharsets.push_back(FRIBIDI_CHAR_SET_CP1255);
  m_vecBidiCharsets.push_back(FRIBIDI_CHAR_SET_CP1255);
  m_vecBidiCharsets.push_back(FRIBIDI_CHAR_SET_CP1256);
  m_vecBidiCharsets.push_back(FRIBIDI_CHAR_SET_CP1256);

  ICONV_PREPARE(m_iconvStringCharsetToFontCharset);
  ICONV_PREPARE(m_iconvUtf8ToStringCharset);
  ICONV_PREPARE(m_iconvStringCharsetToUtf8);
  ICONV_PREPARE(m_iconvUcs2CharsetToStringCharset);
  ICONV_PREPARE(m_iconvSubtitleCharsetToW);
  ICONV_PREPARE(m_iconvWtoUtf8);
  ICONV_PREPARE(m_iconvUtf16BEtoUtf8);
  ICONV_PREPARE(m_iconvUtf16LEtoUtf8);
  ICONV_PREPARE(m_iconvUtf16LEtoW);
  ICONV_PREPARE(m_iconvUtf32ToStringCharset);
  ICONV_PREPARE(m_iconvUtf8toW);
  ICONV_PREPARE(m_iconvUcs2CharsetToUtf8);
}

void CCharsetConverter::clear()
{
  m_vecBidiCharsetNames.clear();
  m_vecBidiCharsets.clear();
  m_vecCharsetNames.clear();
  m_vecCharsetLabels.clear();
}

vector<CStdString> CCharsetConverter::getCharsetLabels()
{
  return m_vecCharsetLabels;
}

CStdString& CCharsetConverter::getCharsetLabelByName(const CStdString& charsetName)
{
  for (unsigned int i = 0; i < m_vecCharsetNames.size(); i++)
  {
    if (m_vecCharsetNames[i] == charsetName)
    {
      return m_vecCharsetLabels[i];
    }
  }

  return EMPTY;
}

CStdString& CCharsetConverter::getCharsetNameByLabel(const CStdString& charsetLabel)
{
  for (unsigned int i = 0; i < m_vecCharsetLabels.size(); i++)
  {
    if (m_vecCharsetLabels[i] == charsetLabel)
    {
      return m_vecCharsetNames[i];
    }
  }

  return EMPTY;
}

bool CCharsetConverter::isBidiCharset(const CStdString& charset)
{
  for (unsigned int i = 0; i < m_vecBidiCharsetNames.size(); i++)
  {
    if (m_vecBidiCharsetNames[i].Equals(charset))
    {
      return true;
    }
  }

  return false;
}

void CCharsetConverter::reset(void)
{
  ICONV_SAFE_CLOSE(m_iconvStringCharsetToFontCharset);
  ICONV_SAFE_CLOSE(m_iconvUtf8ToStringCharset);
  ICONV_SAFE_CLOSE(m_iconvStringCharsetToUtf8);
  ICONV_SAFE_CLOSE(m_iconvUcs2CharsetToStringCharset);
  ICONV_SAFE_CLOSE(m_iconvSubtitleCharsetToW);
  ICONV_SAFE_CLOSE(m_iconvWtoUtf8);
  ICONV_SAFE_CLOSE(m_iconvUtf16BEtoUtf8);
  ICONV_SAFE_CLOSE(m_iconvUtf16LEtoUtf8);
  ICONV_SAFE_CLOSE(m_iconvUtf32ToStringCharset);
  ICONV_SAFE_CLOSE(m_iconvUtf8toW);
  ICONV_SAFE_CLOSE(m_iconvUcs2CharsetToUtf8);

  m_stringFribidiCharset = FRIBIDI_CHAR_SET_NOT_FOUND;

  CStdString strCharset=g_langInfo.GetGuiCharSet();

  for (unsigned int i = 0; i < m_vecBidiCharsetNames.size(); i++)
  {
    if (m_vecBidiCharsetNames[i] == strCharset)
    {
      m_stringFribidiCharset = m_vecBidiCharsets[i];
    }
  }
}

// The bVisualBiDiFlip forces a flip of characters for hebrew/arabic languages, only set to false if the flipping
// of the string is already made or the string is not displayed in the GUI
void CCharsetConverter::utf8ToW(const CStdStringA& utf8String, CStdStringW &wString, bool bVisualBiDiFlip/*=true*/)
{
  CStdStringA strFlipped;
  const char* src;
  size_t inBytes;
  
  // Try to flip hebrew/arabic characters, if any
  if (bVisualBiDiFlip)
  {
    logicalToVisualBiDi(utf8String, strFlipped, FRIBIDI_CHAR_SET_UTF8);
    src = strFlipped.c_str();
    inBytes = strFlipped.length() + 1;
  }
  else
  {
    src = utf8String.c_str();
    inBytes = utf8String.length() + 1;
  }

  if (m_iconvUtf8toW == (iconv_t) - 1)
    m_iconvUtf8toW = iconv_open(WCHAR_CHARSET, UTF8_SOURCE);

  if (m_iconvUtf8toW != (iconv_t) - 1)
  {
    size_t outBytes = inBytes * sizeof(wchar_t);
    char      * dst = (char*)wString.GetBuffer(outBytes);

    if (iconv_const(m_iconvUtf8toW, &src, &inBytes, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      wString.ReleaseBuffer();
      wString = utf8String;
      return;
    }

    if (iconv(m_iconvUtf8toW, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      wString.ReleaseBuffer();
      wString = utf8String;
      return;
    }
    wString.ReleaseBuffer();
  }
}

void CCharsetConverter::subtitleCharsetToW(const CStdStringA& strSource, CStdStringW& strDest)
{
  CStdStringA strFlipped;

  // No need to flip hebrew/arabic as mplayer does the flipping

  if (m_iconvSubtitleCharsetToW == (iconv_t) - 1)
  {
    CStdString strCharset=g_langInfo.GetSubtitleCharSet();
    m_iconvSubtitleCharsetToW = iconv_open(WCHAR_CHARSET, strCharset.c_str());
  }

  if (m_iconvSubtitleCharsetToW != (iconv_t) - 1)
  {
    size_t inBytes  = (strSource.length() + 1);
    size_t outBytes = (strSource.length() + 1) * sizeof(wchar_t);
    const char *src = strSource.c_str();
    char       *dst = (char*)strDest.GetBuffer(outBytes);

    if (iconv_const(m_iconvSubtitleCharsetToW, &src, &inBytes, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv_const(m_iconvSubtitleCharsetToW, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }
    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::logicalToVisualBiDi(const CStdStringA& strSource, CStdStringA& strDest, CStdStringA& charset, FriBidiCharType base)
{
  FriBidiCharSet fribidiCharset = FRIBIDI_CHAR_SET_UTF8;

  for (unsigned int i = 0; i < m_vecBidiCharsetNames.size(); i++)
  {
    if (m_vecBidiCharsetNames[i].Equals(charset))
    {
      fribidiCharset = m_vecBidiCharsets[i];
      break;
    }
  }

  logicalToVisualBiDi(strSource, strDest, fribidiCharset, base);
}

void CCharsetConverter::logicalToVisualBiDi(const CStdStringA& strSource, CStdStringA& strDest, FriBidiCharSet fribidiCharset, FriBidiCharType base)
{
  vector<CStdString> lines;
  CUtil::Tokenize(strSource, lines, "\n");
  CStdString resultString;

  for (unsigned int i = 0; i < lines.size(); i++)
  {
    int sourceLen = lines[i].length();
    FriBidiChar* logical = (FriBidiChar*) malloc((sourceLen + 1) * sizeof(FriBidiChar));
    FriBidiChar* visual = (FriBidiChar*) malloc((sourceLen + 1) * sizeof(FriBidiChar));
    // Convert from the selected charset to Unicode
    int len = fribidi_charset_to_unicode(fribidiCharset, (char*) lines[i].c_str(), sourceLen, logical);

    if (fribidi_log2vis(logical, len, &base, visual, NULL, NULL, NULL))
    {
      // Removes bidirectional marks
      //len = fribidi_remove_bidi_marks(visual, len, NULL, NULL, NULL);

      // Apperently a string can get longer during this transformation
      // so make sure we allocate the maximum possible character utf8
      // can generate atleast, should cover all bases
      char *result = strDest.GetBuffer(len*4);

      // Convert back from Unicode to the charset
      int len2 = fribidi_unicode_to_charset(fribidiCharset, visual, len, result);
      ASSERT(len2 <= len*4);
      strDest.ReleaseBuffer();

      resultString += strDest;
    }

    free(logical);
    free(visual);
  }

  strDest = resultString;
}

void CCharsetConverter::utf8ToStringCharset(const CStdStringA& strSource, CStdStringA& strDest)
{
  if (m_iconvUtf8ToStringCharset == (iconv_t) - 1)
  {
    CStdString strCharset=g_langInfo.GetGuiCharSet();
    m_iconvUtf8ToStringCharset = iconv_open(strCharset.c_str(), UTF8_SOURCE);
  }

  if (m_iconvUtf8ToStringCharset != (iconv_t) - 1)
  {
    size_t inBytes  = strSource.length() + 1;
    size_t outBytes = strSource.length() + 1;
    const char *src = strSource.c_str();
    char       *dst = strDest.GetBuffer(inBytes);

    if (iconv_const(m_iconvUtf8ToStringCharset, &src, &inBytes, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv_const(m_iconvUtf8ToStringCharset, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::utf8ToStringCharset(CStdStringA& strSourceDest)
{
  CStdString strDest;
  utf8ToStringCharset(strSourceDest, strDest);
  strSourceDest=strDest;
}

void CCharsetConverter::stringCharsetToUtf8(const CStdStringA& strSource, CStdStringA& strDest)
{
  if (m_iconvStringCharsetToUtf8 == (iconv_t) - 1)
  {
    CStdString strCharset=g_langInfo.GetGuiCharSet();
    m_iconvStringCharsetToUtf8 = iconv_open("UTF-8", strCharset.c_str());
  }

  if (m_iconvStringCharsetToUtf8 != (iconv_t) - 1)
  {
    size_t inBytes  = (strSource.length() + 1);
    size_t outBytes = (strSource.length() + 1) * 4;
    const char *src = strSource.c_str();
    char       *dst = strDest.GetBuffer(outBytes);

    if (iconv_const(m_iconvStringCharsetToUtf8, &src, &inBytes, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv_const(m_iconvStringCharsetToUtf8, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::stringCharsetToUtf8(CStdStringA& strSourceDest)
{
  CStdString strSource=strSourceDest;
  stringCharsetToUtf8(strSource, strSourceDest);
}

void CCharsetConverter::stringCharsetToUtf8(const CStdStringA& strSourceCharset, const CStdStringA& strSource, CStdStringA& strDest)
{
  iconv_t iconvString=iconv_open("UTF-8", strSourceCharset.c_str());

  if (iconvString != (iconv_t) - 1)
  {
    size_t inBytes  = (strSource.length() + 1);
    size_t outBytes = (strSource.length() + 1) * 4;
    const char *src = strSource.c_str();
    char       *dst = strDest.GetBuffer(outBytes);

    if (iconv_const(iconvString, &src, &inBytes, &dst, &outBytes) == (size_t) -1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv(iconvString, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    strDest.ReleaseBuffer();

    iconv_close(iconvString);
  }
}

void CCharsetConverter::wToUTF8(const CStdStringW& strSource, CStdStringA &strDest)
{
  if (m_iconvWtoUtf8 == (iconv_t) - 1)
    m_iconvWtoUtf8 = iconv_open("UTF-8", WCHAR_CHARSET);

  if (m_iconvWtoUtf8 != (iconv_t) - 1)
  {
    size_t inBytes  = (strSource.length() + 1) * sizeof(wchar_t);
    size_t outBytes = (strSource.length() + 1) * sizeof(wchar_t);  // some free for UTF-8 (up to 4 bytes/char);
    const char *src = (const char*) strSource.c_str();
    char       *dst = strDest.GetBuffer(outBytes);

    if (iconv_const(m_iconvWtoUtf8, &src, &inBytes, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv(m_iconvWtoUtf8, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::utf16BEtoUTF8(const CStdStringW& strSource, CStdStringA &strDest)
{
  if (m_iconvUtf16BEtoUtf8 == (iconv_t) - 1)
    m_iconvUtf16BEtoUtf8 = iconv_open("UTF-8", "UTF-16BE");

  if (m_iconvUtf16BEtoUtf8 != (iconv_t) - 1)
  {
    size_t inBytes  = (strSource.length() + 1) * sizeof(wchar_t);
    size_t outBytes = (strSource.length() + 1) * 4;
    const char *src = (const char*) strSource.c_str();
    char       *dst = strDest.GetBuffer(outBytes);

    if (iconv_const(m_iconvUtf16BEtoUtf8, &src, &inBytes, &dst, &outBytes))
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv(m_iconvUtf16BEtoUtf8, NULL, NULL, &dst, &outBytes))
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }
    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::utf16LEtoUTF8(const void *strSource,
                                      CStdStringA &strDest)
{
  if (m_iconvUtf16LEtoUtf8 == (iconv_t) - 1)
    m_iconvUtf16LEtoUtf8 = iconv_open("UTF-8", "UTF-16LE");

  if (m_iconvUtf16LEtoUtf8 != (iconv_t) - 1)
  {
    size_t inBytes = 2;
    uint16_t *s = (uint16_t *)strSource;
    while (*s != 0)
    { 
      s++;
      inBytes += 2;
    }
    // UTF-8 is up to 4 bytes/character, or up to twice the length of UTF-16
    size_t outBytes = inBytes * 2;

    const char *src = (const char *)strSource;
    char *dst = strDest.GetBuffer(outBytes);
    if (iconv_const(m_iconvUtf16LEtoUtf8, &src, &inBytes, &dst, &outBytes) ==
        (size_t)-1)
    { // failed :(
      strDest.clear();
      strDest.ReleaseBuffer();
      return;
    }
    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::ucs2ToUTF8(const CStdStringW& strSource, CStdStringA& strDest)
{
  if (m_iconvUcs2CharsetToUtf8 == (iconv_t) - 1)
    m_iconvUcs2CharsetToUtf8 = iconv_open("UTF-8", "UCS-2LE");

  if (m_iconvUcs2CharsetToUtf8 != (iconv_t) - 1)
  {
    const char* src = (const char*) strSource.c_str();
    size_t inBytes = (strSource.length() + 1)*2;
    size_t outBytes = (inBytes + 1)*2;  // some free for UTF-8 (up to 4 bytes/char)
    char *dst = strDest.GetBuffer(outBytes);
    
    if (iconv_const(m_iconvUcs2CharsetToUtf8, &src, &inBytes, &dst, &outBytes) == (size_t) -1)
    { // failed :(
      CLog::Log(LOGERROR, "CCharsetConverter::ucs2ToUTF8 failed for Python with errno=%d", errno);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }
    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::utf16LEtoW(const char* strSource, CStdStringW &strDest)
{
  if (m_iconvUtf16LEtoW == (iconv_t) - 1)
    m_iconvUtf16LEtoW = iconv_open(WCHAR_CHARSET, "UTF-16LE");

  if (m_iconvUtf16LEtoW != (iconv_t) - 1)
  {
    size_t inBytes = 2;
    short* s = (short*) strSource;
    while (*s != 0)
    {
      s++;
      inBytes += 2;
    }
    size_t outBytes = (inBytes + 1)*sizeof(wchar_t);  // UTF-8 is up to 4 bytes/character  
    char *dst = (char*) strDest.GetBuffer(outBytes);

    if (iconv_const(m_iconvUtf16LEtoW, &strSource, &inBytes, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv(m_iconvUtf16LEtoW, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::ucs2CharsetToStringCharset(const CStdStringW& strSource, CStdStringA& strDest, bool swap)
{
  if (m_iconvUcs2CharsetToStringCharset == (iconv_t) - 1)
  {
    CStdString strCharset=g_langInfo.GetGuiCharSet();
    m_iconvUcs2CharsetToStringCharset = iconv_open(strCharset.c_str(), "UTF-16LE");
  }

  if (m_iconvUcs2CharsetToStringCharset != (iconv_t) - 1)
  {
    CStdStringW strCopy = strSource;
    size_t inBytes  = (strCopy.length() + 1) * sizeof(wchar_t);
    size_t outBytes = (strCopy.length() + 1) * 4;
    const char *src = (const char*)strCopy.c_str();
    char       *dst = strDest.GetBuffer(inBytes);

    if (swap)
    {
      char* s = (char*) src;

      while (*s || *(s + 1))
      {
        char c = *s;
        *s = *(s + 1);
        *(s + 1) = c;

        s++;
        s++;
      }
    }

    if (iconv_const(m_iconvUcs2CharsetToStringCharset, &src, &inBytes, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv_const(m_iconvUcs2CharsetToStringCharset, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    strDest.ReleaseBuffer();
  }
}

void CCharsetConverter::utf32ToStringCharset(const unsigned long* strSource, CStdStringA& strDest)
{
  if (m_iconvUtf32ToStringCharset == (iconv_t) - 1)
  {
    CStdString strCharset=g_langInfo.GetGuiCharSet();
    m_iconvUtf32ToStringCharset = iconv_open(strCharset.c_str(), "UTF-32LE");
  }

  if (m_iconvUtf32ToStringCharset != (iconv_t) - 1)
  {
    const unsigned long* ptr=strSource;
    while (*ptr) ptr++;
    const char* src = (const char*) strSource;
    size_t inBytes = (ptr-strSource+1)*4;

    char *dst = strDest.GetBuffer(inBytes);
    size_t outBytes = inBytes;

    if (iconv_const(m_iconvUtf32ToStringCharset, &src, &inBytes, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = (const char *)strSource;
      return;
    }

    if (iconv(m_iconvUtf32ToStringCharset, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = (const char *)strSource;
      return;
    }

    strDest.ReleaseBuffer();
  }
}

// Taken from RFC2640
bool CCharsetConverter::isValidUtf8(const char *buf, unsigned int len)
{
  const unsigned char *endbuf = (unsigned char*)buf + len;
  unsigned char byte2mask=0x00, c;
  int trailing=0; // trailing (continuation) bytes to follow

  while ((unsigned char*)buf != endbuf)
  {
    c = *buf++;
    if (trailing)
      if ((c & 0xc0) == 0x80) // does trailing byte follow UTF-8 format ?
      {
        if (byte2mask) // need to check 2nd byte for proper range
        {
          if (c & byte2mask) // are appropriate bits set ?
            byte2mask = 0x00;
          else
            return false;
        }
        trailing--;
      }
      else
        return 0;
    else
      if ((c & 0x80) == 0x00) continue; // valid 1-byte UTF-8
      else if ((c & 0xe0) == 0xc0)      // valid 2-byte UTF-8
        if (c & 0x1e)                   //is UTF-8 byte in proper range ?
          trailing = 1;
        else
          return false;
      else if ((c & 0xf0) == 0xe0)      // valid 3-byte UTF-8
       {
        if (!(c & 0x0f))                // is UTF-8 byte in proper range ?
          byte2mask = 0x20;             // if not set mask
        trailing = 2;                   // to check next byte
      }
      else if ((c & 0xf8) == 0xf0)      // valid 4-byte UTF-8
      {
        if (!(c & 0x07))                // is UTF-8 byte in proper range ?
          byte2mask = 0x30;             // if not set mask
        trailing = 3;                   // to check next byte
      }
      else if ((c & 0xfc) == 0xf8)      // valid 5-byte UTF-8
      {
        if (!(c & 0x03))                // is UTF-8 byte in proper range ?
          byte2mask = 0x38;             // if not set mask
        trailing = 4;                   // to check next byte
      }
      else if ((c & 0xfe) == 0xfc)      // valid 6-byte UTF-8
      {
        if (!(c & 0x01))                // is UTF-8 byte in proper range ?
          byte2mask = 0x3c;             // if not set mask
        trailing = 5;                   // to check next byte
      }
      else
        return false;
  }
  return trailing == 0;
}

bool CCharsetConverter::isValidUtf8(const CStdString& str)
{
  return isValidUtf8(str.c_str(), str.size());
}


void CCharsetConverter::GuiCharsetTo(const CStdStringA& strDestCharset, CStdStringA& strSourceDest)
{
  CStdString strSource=strSourceDest;
  GuiCharsetTo(strDestCharset, strSource, strSourceDest);
}

void CCharsetConverter::GuiCharsetTo(const CStdStringA& strDestCharset, const CStdStringA& strSource, CStdStringA& strDest)
{

  CStdString strCharset=g_langInfo.GetGuiCharSet();
  if (strCharset == strDestCharset) 
  {
    strDest = strSource;
    return;
  }
  
  iconv_t iconvString=iconv_open(strDestCharset.c_str(), strCharset.c_str());

  if (iconvString != (iconv_t) - 1)
  {
    size_t inBytes  = (strSource.length() + 1);
    size_t outBytes = (strSource.length() + 1) * 4;
    const char *src = strSource.c_str();
    char       *dst = strDest.GetBuffer(outBytes);

    if (iconv_const(iconvString, &src, &inBytes, &dst, &outBytes) == (size_t) -1)
    {
      CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    if (iconv(iconvString, NULL, NULL, &dst, &outBytes) == (size_t)-1)
    {
      CLog::Log(LOGERROR, "%s failed cleanup", __FUNCTION__);
      strDest.ReleaseBuffer();
      strDest = strSource;
      return;
    }

    strDest.ReleaseBuffer();

    iconv_close(iconvString);
  }
}
