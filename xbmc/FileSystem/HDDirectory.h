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


#include "IDirectory.h"
struct WIN32_FILE_DATA;
class CFileItem;

namespace DIRECTORY
{

class CHDDirectory : public IDirectory
{
public:
  CHDDirectory(void);
  virtual ~CHDDirectory(void);
  virtual bool GetDirectory(const CStdString& strPath, CFileItemList &items);
  virtual bool Create(const char* strPath);
  virtual bool Exists(const char* strPath);
  virtual bool Remove(const char* strPath);
protected:
  bool GetNormalFolder(const CStdString& strPath, CFileItemList &items);
  bool GetSmartFolder(const CStdString& strPath, CFileItemList &items);
  static void HandleSearchResult(void* thisPtr, void* itemListPtr, const char* strFilePath);

  bool       IsAllowed(CFileItem* pItem, WIN32_FIND_DATA& wfd);
  CFileItem* BuildFileItem(const CStdString& strRoot, WIN32_FIND_DATA& wfd);
  CFileItem* BuildResolvedFileItem(const CStdString& strRoot, WIN32_FIND_DATA& wfd);
};
}
