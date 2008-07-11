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

#define NETWORK_ERROR -1 
#define NETWORK_OK 0

bool DVDInputStreamRTSP::Open(const char* strFile, const std::string &content)
{
	Close();
	
	CStdString path(strFile);
	this->RTSPFileUrl = path;

	this->RTSPServerSocket = SocketConnect(path, 8554);
	if ((this->RTSPServerSocket) < 0)
	{
		return false;
	}
	return true;
}

const CStdString DVDInputStreamRTSP::GetRTSPStream()
{
	return this->RTSPFileUrl;
}


void DVDInputStreamRTSP::Close()
{
	
}

DVDInputStreamRTSP::DVDInputStreamRTSP() : CDVDInputStream(DVDSTREAM_TYPE_RTSP)
{
	
}

DVDInputStreamRTSP::~DVDInputStreamRTSP()
{
	Close();
}

int DVDInputStreamRTSP::Read(BYTE* buf, int buf_size)
{
	unsigned int ret = 0;
	if (RTSPFileUrl) ret = read(this->RTSPServerSocket, buf, buf_size);

	else ret = -1;
	
	return ret;
}

__int64 DVDInputStreamRTSP::Seek(__int64 offset, int whence)
{
	return -1;
}

#pragma Stream Access

int DVDInputStreamRTSP::DVDInputStreamRTSP::InitSocket()
{
  assert(this->RTSPServerSocket > 0);
  return 0;
}

int DVDInputStreamRTSP::SocketConnect(const char *host, int port)
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

void DVDInputStreamRTSP::Disconnect()
{
	//SendCommand("CloseConnection", NULL);
	// Send/receive, then cleanup:
#warning fix
}
	
