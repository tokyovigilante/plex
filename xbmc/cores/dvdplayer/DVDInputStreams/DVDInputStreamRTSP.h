/*
 *  DVDInputStreamRTSP.h
 *  Plex
 *
 *  Created by Ryan Walklin on 7/11/08.
 *
 *  Based on MediaPortal patch for XBMC by prashantv
 *  Copyright 2008 Ryan Walklin. All rights reserved.
 *
 */
#pragma once

/*#include "FileSystem/IFile.h"
#include "FileSystem/ILiveTV.h"
#include "lib/tvserver/TVServerConnection.h"*/
#include "DVDInputStream.h"
#include "DVDInputStreamFFmpeg.h"

class DllLibCMyth;

class DVDInputStreamRTSP:
 public  CDVDInputStream//,
 //XFILE::ILiveTVInterface,
 //XFILE::IRecordable
{
public:
	DVDInputStreamRTSP();
	virtual ~DVDInputStreamRTSP();
	virtual bool    Open(const char* strFile, const std::string &content);
	virtual void    Close();
	virtual int     Read(BYTE* buf, int buf_size);
	virtual __int64 Seek(__int64 offset, int whence);
	
	virtual const CStdString GetRTSPStream();
	
protected:
	
private:
	CStdString RTSPFileUrl;
	int RTSPServerSocket;
	int InitSocket();
	int SocketConnect(const char *host, int port);
	void Disconnect();
	
};

