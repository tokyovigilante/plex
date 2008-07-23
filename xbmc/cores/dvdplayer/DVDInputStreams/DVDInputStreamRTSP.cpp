/*
 *  DVDInputStreamRTSP.cpp
 *  Plex
 *
 *  Created by Ryan Walklin on 7/11/08.
 *
 *  Based on MediaPortal patch for XBMC by prashantv
 *  Copyright 2008 Ryan Walklin. All rights reserved.
 *
 */

#include "stdafx.h"
#include "Util.h"
#include "DVDInputStreamRTSP.h"
#include "Socket.h"
//#include 

#define NETWORK_ERROR -1 
#define NETWORK_OK 0

bool CDVDInputStreamRTSP::Open(const char* strFile, const std::string &content)
{
	Close();
	
	CStdString path(strFile);
	this->RTSPFileUrl = path;

	//this->RTSPServerSocket = SocketConnect(path, 8554);
	//if ((this->RTSPServerSocket) < 0)
	{
	//	return false;
	}
	return true;
}

const CStdString CDVDInputStreamRTSP::GetRTSPStream()
{
	return this->RTSPFileUrl;
}


void CDVDInputStreamRTSP::Close()
{
	
}

CDVDInputStreamRTSP::CDVDInputStreamRTSP() : CDVDInputStream(DVDSTREAM_TYPE_RTSP)
{
	
}

CDVDInputStreamRTSP::~CDVDInputStreamRTSP()
{
	Close();
}

int CDVDInputStreamRTSP::Read(BYTE* buf, int buf_size)
{
	unsigned int ret = 0;
	if (RTSPFileUrl) ret = read(this->RTSPServerSocket, buf, buf_size);

	else ret = -1;
	
	return ret;
}

__int64 CDVDInputStreamRTSP::Seek(__int64 offset, int whence)
{
	return -1;
}

bool CDVDInputStreamRTSP::IsEOF()
{
	return false;
}

__int64 CDVDInputStreamRTSP::GetLength()
{
	return 0;
}

#pragma Stream Access

int CDVDInputStreamRTSP::CDVDInputStreamRTSP::InitSocket()
{
  assert(this->RTSPServerSocket > 0);
  return 0;
}

int CDVDInputStreamRTSP::SocketConnect(const char *host, int port)
{
	struct hostent *servhost;
	struct sockaddr_in server;
	int sock;
	
   
	//get IP adddres from host
	servhost = gethostbyname(host);
	bzero(&server, sizeof(server)); 
   
	server.sin_family = AF_INET;
	bcopy(servhost->h_addr, &server.sin_addr, servhost->h_length);
	server.sin_port = htons(port);
   
	//make socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   
   if(sock <= 0)
   {
      return NETWORK_ERROR;
   }
   
   errno = 0;
   
	//make connection
	int nret = connect(sock, (struct sockaddr *)&server, sizeof(server));
   
   if(nret < 0)
   {
      return NETWORK_ERROR;
   }
	return sock;
}

void CDVDInputStreamRTSP::Disconnect()
{
	//SendCommand("CloseConnection", NULL);
	// Send/receive, then cleanup:
#warning fix
}
	
