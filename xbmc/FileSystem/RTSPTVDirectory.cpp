/*
 *  RTSPTVDirectory.cpp
 *  XBMC
 *
 *  Created by Ryan Walklin on 7/11/08.
 *  Copyright 2008 Ryan Walklin. All rights reserved.
 *
 */



#include "stdafx.h"
#include "RTSPTVDirectory.h"
#include "Util.h"
#include "URL.h"
#include "FileItem.h"

using namespace DIRECTORY;
using namespace XFILE;
using namespace std;

RTSPTVDirectory::RTSPTVDirectory()
{
	
}

RTSPTVDirectory::~RTSPTVDirectory()
{
	Release();
}

void RTSPTVDirectory::Release()
{
	
}

bool RTSPTVDirectory::GetChannels(const CStdString& base, CFileItemList &items, CStdString filePath)
{	
/*	CUtil::RemoveSlashAtEnd(filePath);
	CLog::DebugLog("RTSPTVDirectory :: GetChannels for %s", base.c_str());
	
	// filePath = group/[groupName]
	const CStdString& groupName = filePath.Mid(6);
	
	//vector<TvServerChannel> channels = m_tvConnection.GetChannels(groupName);
	
	CURL url(base);
	
	for(unsigned i=0;i<channels.size();i++)
	{
		TvServerChannel channel = channels[i];
		CFileItemPtr item(new CFileItem("", false));
		url.SetFileName("channel/" + channel.id);
		url.GetURL(item->m_strPath);
		item->SetLabel(channel.name + " [" + channel.curShow + " from " + channel.curTime + "]");
		items.Add(item);
	}
	
	m_tvConnection.Disconnect();*/
	return true;
}

bool RTSPTVDirectory::GetGroups (const CStdString& base, CFileItemList &items)
{
	/*CLog::Log(LOGERROR, "RTSPTVDirectory :: GetGroups for %s", base.c_str());
	vector<CStdString> groups = m_tvConnection.GetGroups();
	
	CURL url(base);
	
	for(unsigned i=0;i<groups.size();i++)
	{
		CStdString group = groups[i];
		CFileItemPtr item(new CFileItem("", true));
		url.SetFileName("group/" + group + "/");
		url.GetURL(item->m_strPath);
		item->SetLabel(group);
		items.Add(item);
	}
	
	m_tvConnection.Disconnect();*/
	return true;
}


bool RTSPTVDirectory::GetDirectory(const CStdString& strPath, CFileItemList &items)
{
	CURL url(strPath);
	CStdString base(strPath);
	CUtil::RemoveSlashAtEnd(base);
	
/*	if(m_tvConnection.Connect(url.GetHostName().c_str()))
	{
		CLog::Log(LOGERROR, "RTSPTVDirectory: Cannot connect to host %s", url.GetHostName().c_str());
		// error connecting
		return false;
	}*/
 CFileItem* item(new CFileItem(base, false));
 item->SetLabel("Live Stream");
 item->SetLabelPreformated(true);
 items.Add(item);
	/*if(url.GetFileName().IsEmpty())
	{
		CFileItemPtr item(new CFileItem(base + "/groups/", true));
		item->SetLabel("Channel Groups");
		item->SetLabelPreformated(true);
		items.Add(item);
		
		m_tvConnection.Disconnect();
		return true;
	}
	else if(url.GetFileName() == "groups/")
	{
		return GetGroups(base, items);
	}
	else if(url.GetFileName().Left(6) == "group/")
	{
		return GetChannels(base, items, url.GetFileName());
	}
	
 return false;*/ return true;
}

