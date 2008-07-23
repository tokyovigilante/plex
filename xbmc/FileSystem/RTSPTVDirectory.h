/*
 *  RTSPTVDirectory.h
 *  XBMC
 *
 *  Created by Ryan Walklin on 7/11/08.
 *  Copyright 2008 Ryan Walklin. All rights reserved.
 *
 */

#pragma once

//#include "lib/tvserver/TVServerConnection.h"
#include "IDirectory.h"

namespace DIRECTORY
{

class RTSPTVDirectory
  : public IDirectory
{
public:
 RTSPTVDirectory();
  ~RTSPTVDirectory();
#warning show stream info for direcrtoy
  virtual bool GetDirectory(const CStdString& strPath, CFileItemList &items);

private:
  void Release();
  bool GetChannels(const CStdString& base, CFileItemList &items, CStdString filePath);
  bool GetGroups  (const CStdString& base, CFileItemList &items);

};


}
