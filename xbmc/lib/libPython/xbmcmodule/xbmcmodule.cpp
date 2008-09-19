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
#ifndef _LINUX
#include "lib/libPython/python/Python.h"
#else
#include <python2.4/Python.h>
#include "../XBPythonDll.h"
#endif
#include "player.h"
#include "pyplaylist.h"
#include "keyboard.h"
#include "xbox/IoSupport.h"
#ifndef _LINUX
#include <ConIo.h>
#endif
#include "infotagvideo.h"
#include "infotagmusic.h"
#include "lib/libGoAhead/XBMChttp.h"
#include "utils/GUIInfoManager.h"
#include "GUIWindowManager.h"
#include "GUIAudioManager.h"
#include "Application.h"
#include "Crc32.h"
#include "Util.h"
#include "FileSystem/File.h"
#include "Settings.h"
#include "TextureManager.h"

// include for constants
#include "pyutil.h"
#include "PlayListPlayer.h"

using namespace std;
using namespace XFILE;

#ifndef __GNUC__
#pragma code_seg("PY_TEXT")
#pragma data_seg("PY_DATA")
#pragma bss_seg("PY_BSS")
#pragma const_seg("PY_RDATA")
#endif

#ifdef __cplusplus
extern "C" {
#endif

namespace PYXBMC
{
/*****************************************************************
 * start of xbmc methods
 *****************************************************************/

  // output() method
  PyDoc_STRVAR(output__doc__,
    "output(text) -- Write a string to XBMC's log file and the debug window.\n"
    "\n"
    "text           : string - text to output.\n"
    "\n"
    "*Note, Text is written to the log only when <loglevel>1</loglevel> (DEBUG)\n"
    "       or higher is set in AdvancedSettings.xml.\n"
    "\n"
    "example:\n"
    "  - xbmc.output('This is a test string.')\n");

  PyObject* XBMC_Output(PyObject *self, PyObject *args)
  {
    char *s_line = NULL;
    if (!PyArg_ParseTuple(args, "s:xb_output", &s_line)) return NULL;

    CLog::Log(LOGINFO, "%s", s_line);
    ThreadMessage tMsg = {TMSG_WRITE_SCRIPT_OUTPUT};
    tMsg.strParam = s_line;
    g_application.getApplicationMessenger().SendMessage(tMsg);

    Py_INCREF(Py_None);
    return Py_None;
  }

  // log() method
  PyDoc_STRVAR(log__doc__,
    "log(text) -- Write a string to XBMC's log file.\n"
    "\n"
    "text           : string - text to write to log.\n"
    "\n"
    "example:\n"
    "  - xbmc.log('This is a test string.')\n");

  PyObject* XBMC_Log(PyObject *self, PyObject *args)
  {
    char *s_line = NULL;
    if (!PyArg_ParseTuple(args, "s", &s_line)) return NULL;

    CLog::Log(LOGFATAL, "%s", s_line);

    Py_INCREF(Py_None);
    return Py_None;
  }

  // shutdown() method
  PyDoc_STRVAR(shutdown__doc__,
    "shutdown() -- Shutdown the xbox.\n"
    "\n"
    "example:\n"
    "  - xbmc.shutdown()\n");

  PyObject* XBMC_Shutdown(PyObject *self, PyObject *args)
  {
    ThreadMessage tMsg = {TMSG_SHUTDOWN};
    g_application.getApplicationMessenger().SendMessage(tMsg);

    Py_INCREF(Py_None);
    return Py_None;
  }

  // shutdown() method
  PyDoc_STRVAR(sleepSystem__doc__,
	"sleepSystem() -- Send the xbox to standby mode.\n"
	"\n"
	"example:\n"
	"  - xbmc.sleepSystem()\n");
	
  PyObject* XBMC_SleepSystem(PyObject *self, PyObject *args)
  {
	ThreadMessage tMsg = {TMSG_SLEEPSYSTEM};
	g_application.getApplicationMessenger().SendMessage(tMsg);
	  
	Py_INCREF(Py_None);
	return Py_None;
  }
	
  // dashboard() method
  PyDoc_STRVAR(dashboard__doc__,
    "dashboard() -- Boot to dashboard as set in My Pograms/General.\n"
    "\n"
    "example:\n"
    "  - xbmc.dashboard()\n");

  PyObject* XBMC_Dashboard(PyObject *self, PyObject *args)
  {
    ThreadMessage tMsg = {TMSG_DASHBOARD};
    g_application.getApplicationMessenger().SendMessage(tMsg);

    Py_INCREF(Py_None);
    return Py_None;
  }

  // restart() method
  PyDoc_STRVAR(restart__doc__,
    "restart() -- Restart the xbox.\n"
    "\n"
    "example:\n"
    "  - xbmc.restart()\n");

  PyObject* XBMC_Restart(PyObject *self, PyObject *args)
  {
    ThreadMessage tMsg = {TMSG_RESTART};
    g_application.getApplicationMessenger().SendMessage(tMsg);

    Py_INCREF(Py_None);
    return Py_None;
  }

  // executescript() method
  PyDoc_STRVAR(executeScript__doc__,
    "executescript(script) -- Execute a python script.\n"
    "\n"
    "script         : string - script filename to execute.\n"
    "\n"
    "example:\n"
    "  - xbmc.executescript('q:\\\\scripts\\\\update.py')\n");

  PyObject* XBMC_ExecuteScript(PyObject *self, PyObject *args)
  {
    char *cLine = NULL;
    if (!PyArg_ParseTuple(args, "s", &cLine)) return NULL;

    ThreadMessage tMsg = {TMSG_EXECUTE_SCRIPT};
    tMsg.strParam = cLine;
    g_application.getApplicationMessenger().SendMessage(tMsg);

    Py_INCREF(Py_None);
    return Py_None;
  }

  // executebuiltin() method
  PyDoc_STRVAR(executeBuiltIn__doc__,
    "executebuiltin(function) -- Execute a built in XBMC function.\n"
    "\n"
    "function       : string - builtin function to execute.\n"
    "\n"
    "List of functions - http://xbmc.org/wiki/?title=List_of_Built_In_Functions \n"
    "\n"
    "example:\n"
    "  - xbmc.executebuiltin('XBMC.RunXBE(c:\\\\avalaunch.xbe)')\n");

  PyObject* XBMC_ExecuteBuiltIn(PyObject *self, PyObject *args)
  {
    char *cLine = NULL;
    if (!PyArg_ParseTuple(args, "s", &cLine)) return NULL;

    g_application.getApplicationMessenger().ExecBuiltIn(cLine);

    Py_INCREF(Py_None);
    return Py_None;
  }

  // executehttpapi() method
  PyDoc_STRVAR(executeHttpApi__doc__,
    "executehttpapi(httpcommand) -- Execute an HTTP API command.\n"
    "\n"
    "httpcommand    : string - http command to execute.\n"
    "\n"
    "List of commands - http://xbmc.org/wiki/?title=WebServerHTTP-API#The_Commands \n"
    "\n"
    "example:\n"
    "  - response = xbmc.executehttpapi('TakeScreenShot(q:\\\\test.jpg,0,false,200,-1,90)')\n");

   PyObject* XBMC_ExecuteHttpApi(PyObject *self, PyObject *args)
  {
    char *cLine = NULL;
    CStdString ret;
    if (!PyArg_ParseTuple(args, (char*)"s", &cLine)) return NULL;
    if (!m_pXbmcHttp)
    {
      CSectionLoader::Load("LIBHTTP");
      m_pXbmcHttp = new CXbmcHttp();
    }
    if (!pXbmcHttpShim)
    {
      pXbmcHttpShim = new CXbmcHttpShim();
      if (!pXbmcHttpShim)
        return NULL;
    }
    ret=pXbmcHttpShim->xbmcExternalCall(cLine);

    return PyString_FromString(ret.c_str());
  }

  // sleep() method
  PyDoc_STRVAR(sleep__doc__,
    "sleep(time) -- Sleeps for 'time' msec.\n"
    "\n"
    "time           : integer - number of msec to sleep.\n"
    "\n"
    "*Note, This is useful if you have for example a Player class that is waiting\n"
    "       for onPlayBackEnded() calls.\n"
    "\n"
    "Throws: PyExc_TypeError, if time is not an integer.\n"
    "\n"
    "example:\n"
    "  - xbmc.sleep(2000) # sleeps for 2 seconds\n");

  PyObject* XBMC_Sleep(PyObject *self, PyObject *args)
  {
    PyObject *pObject;
    if (!PyArg_ParseTuple(args, "O", &pObject)) return NULL;
    if (!PyInt_Check(pObject))
    {
      PyErr_Format(PyExc_TypeError, "argument must be a bool(integer) value");
      return NULL;
    }

    long i = PyInt_AsLong(pObject);
    //while(i != 0)
    //{
      Py_BEGIN_ALLOW_THREADS
      Sleep(i);//(500);
      Py_END_ALLOW_THREADS

      Py_MakePendingCalls();
      //i = PyInt_AsLong(pObject);
    //}

    Py_INCREF(Py_None);
    return Py_None;
  }

  // getLocalizedString() method
  PyDoc_STRVAR(getLocalizedString__doc__,
    "getLocalizedString(id) -- Returns a localized 'unicode string'.\n"
    "\n"
    "id             : integer - id# for string you want to localize.\n"
    "\n"
    "*Note, See strings.xml in \\language\\{yourlanguage}\\ for which id\n"
    "       you need for a string.\n"
    "\n"
    "example:\n"
    "  - locstr = xbmc.getLocalizedString(6)\n");

  PyObject* XBMC_GetLocalizedString(PyObject *self, PyObject *args)
  {
    int iString;
    if (!PyArg_ParseTuple(args, "i", &iString)) return NULL;

    CStdStringW unicodeLabel;
    if (iString >= 30000 && iString <= 30999)
      g_charsetConverter.utf8ToW(g_localizeStringsTemp.Get(iString), unicodeLabel);
    else
      g_charsetConverter.utf8ToW(g_localizeStrings.Get(iString), unicodeLabel);

    return Py_BuildValue("u", unicodeLabel.c_str());
  }

  // getSkinDir() method
  PyDoc_STRVAR(getSkinDir__doc__,
    "getSkinDir() -- Returns the active skin directory as a string.\n"
    "\n"
    "*Note, This is not the full path like 'q:\\skins\\MediaCenter', but only 'MediaCenter'.\n"
    "\n"
    "example:\n"
    "  - skindir = xbmc.getSkinDir()\n");

  PyObject* XBMC_GetSkinDir(PyObject *self, PyObject *args)
  {
    return PyString_FromString(g_guiSettings.GetString("lookandfeel.skin"));
  }

  // getLanguage() method
  PyDoc_STRVAR(getLanguage__doc__,
    "getLanguage() -- Returns the active language as a string.\n"
    "\n"
    "example:\n"
    "  - language = xbmc.getLanguage()\n");

  PyObject* XBMC_GetLanguage(PyObject *self, PyObject *args)
  {
    return PyString_FromString(g_guiSettings.GetString("locale.language"));
  }

  // getIPAddress() method
  PyDoc_STRVAR(getIPAddress__doc__,
    "getIPAddress() -- Returns the current ip address as a string.\n"
    "\n"
    "example:\n"
    "  - ip = xbmc.getIPAddress()\n");

  PyObject* XBMC_GetIPAddress(PyObject *self, PyObject *args)
  {
    char cTitleIP[32];
#ifdef HAS_XBOX_NETWORK
    XNADDR xna;
    XNetGetTitleXnAddr(&xna);
    XNetInAddrToString(xna.ina, cTitleIP, 32);
#else
    sprintf(cTitleIP, "127.0.0.1");
#endif
    return PyString_FromString(cTitleIP);
  }

  // getDVDState() method
  PyDoc_STRVAR(getDVDState__doc__,
    "getDVDState() -- Returns the dvd state as an integer.\n"
    "\n"
    "return values are:\n"
    "   1 : xbmc.DRIVE_NOT_READY\n"
    "  16 : xbmc.TRAY_OPEN\n"
    "  64 : xbmc.TRAY_CLOSED_NO_MEDIA\n"
    "  96 : xbmc.TRAY_CLOSED_MEDIA_PRESENT\n"
    "\n"
    "example:\n"
    "  - dvdstate = xbmc.getDVDState()\n");

  PyObject* XBMC_GetDVDState(PyObject *self, PyObject *args)
  {
    return PyInt_FromLong(CIoSupport::GetTrayState());
  }

  // getFreeMem() method
  PyDoc_STRVAR(getFreeMem__doc__,
    "getFreeMem() -- Returns the amount of free memory in MB as an integer.\n"
    "\n"
    "example:\n"
    "  - freemem = xbmc.getFreeMem()\n");

  PyObject* XBMC_GetFreeMem(PyObject *self, PyObject *args)
  {
    MEMORYSTATUS stat;
    GlobalMemoryStatus(&stat);
    return PyInt_FromLong( stat.dwAvailPhys  / ( 1024 * 1024 ) );
  }

  // getCpuTemp() method
  // ## Doesn't work right, use getInfoLabel('System.CPUTemperature') instead.
  /*PyDoc_STRVAR(getCpuTemp__doc__,
    "getCpuTemp() -- Returns the current cpu temperature as an integer.\n"
    "\n"
    "example:\n"
    "  - cputemp = xbmc.getCpuTemp()\n");

  PyObject* XBMC_GetCpuTemp(PyObject *self, PyObject *args)
  {
    unsigned short cputemp;
    unsigned short cpudec;

    _outp(0xc004, (0x4c<<1)|0x01);
    _outp(0xc008, 0x01);
    _outpw(0xc000, _inpw(0xc000));
    _outp(0xc002, (0) ? 0x0b : 0x0a);
    while ((_inp(0xc000) & 8));
    cputemp = _inpw(0xc006);

    _outp(0xc004, (0x4c<<1)|0x01);
    _outp(0xc008, 0x10);
    _outpw(0xc000, _inpw(0xc000));
    _outp(0xc002, (0) ? 0x0b : 0x0a);
    while ((_inp(0xc000) & 8));
    cpudec = _inpw(0xc006);

    if (cpudec<10) cpudec = cpudec * 100;
    if (cpudec<100) cpudec = cpudec *10;

    return PyInt_FromLong((long)(cputemp + cpudec / 1000.0f));
  }*/

  // getInfolabel() method
  PyDoc_STRVAR(getInfoLabel__doc__,
    "getInfoLabel(infotag) -- Returns an InfoLabel as a string.\n"
    "\n"
    "infotag        : string - infoTag for value you want returned.\n"
    "\n"
    "List of InfoTags - http://xbmc.org/wiki/?title=InfoLabels \n"
    "\n"
    "example:\n"
    "  - label = xbmc.getInfoLabel('Weather.Conditions')\n");

  PyObject* XBMC_GetInfoLabel(PyObject *self, PyObject *args)
  {
    char *cLine = NULL;
    if (!PyArg_ParseTuple(args, "s", &cLine)) return NULL;

    int ret = g_infoManager.TranslateString(cLine);
    return Py_BuildValue("s", g_infoManager.GetLabel(ret).c_str());
  }

  // getInfoImage() method
  PyDoc_STRVAR(getInfoImage__doc__,
    "getInfoImage(infotag) -- Returns a filename including path to the InfoImage's\n"
    "                         thumbnail as a string.\n"
    "\n"
    "infotag        : string - infotag for value you want returned.\n"
    "\n"
    "List of InfoTags - http://xbmc.org/wiki/?title=InfoLabels \n"
    "\n"
    "example:\n"
    "  - filename = xbmc.getInfoImage('Weather.Conditions')\n");

  PyObject* XBMC_GetInfoImage(PyObject *self, PyObject *args)
  {
    char *cLine = NULL;
    if (!PyArg_ParseTuple(args, "s", &cLine)) return NULL;

    int ret = g_infoManager.TranslateString(cLine);
    return Py_BuildValue("s", g_infoManager.GetImage(ret, WINDOW_INVALID).c_str());
  }

  // playSFX() method
  PyDoc_STRVAR(playSFX__doc__,
    "playSFX(filename) -- Plays a wav file by filename\n"
    "\n"
    "filename       : string - filename of the wav file to play.\n"
    "\n"
    "example:\n"
    "  - xbmc.playSFX('Q:\\\\scripts\\\\dingdong.wav')\n");

  PyObject* XBMC_PlaySFX(PyObject *self, PyObject *args)
  {
    const char *cFile = NULL;

    if (!PyArg_ParseTuple(args, "s", &cFile)) return NULL;

    if (CFile::Exists(cFile))
    {
      g_audioManager.PlayPythonSound(cFile);
    }

    Py_INCREF(Py_None);
    return Py_None;
  }

  // enableNavSounds() method
  PyDoc_STRVAR(enableNavSounds__doc__,
    "enableNavSounds(yesNo) -- Enables/Disables nav sounds\n"
    "\n"
    "yesNo          : integer - enable (True) or disable (False) nav sounds\n"
    "\n"
    "example:\n"
    "  - xbmc.enableNavSounds(True)\n");

  PyObject* XBMC_EnableNavSounds(PyObject *self, PyObject *args)
  {
    int yesNo = 1;

    if (!PyArg_ParseTuple(args, "i", &yesNo)) return NULL;

    g_audioManager.Enable(yesNo==1);

    Py_INCREF(Py_None);
    return Py_None;
  }

  // getCondVisibility() method
  PyDoc_STRVAR(getCondVisibility__doc__,
    "getCondVisibility(condition) -- Returns True (1) or False (0) as a bool.\n"
    "\n"
    "condition      : string - condition to check.\n"
    "\n"
    "List of Conditions - http://xbmc.org/wiki/?title=List_of_Boolean_Conditions \n"
    "\n"
    "*Note, You can combine two (or more) of the above settings by using \"+\" as an AND operator,\n"
    "\"|\" as an OR operator, \"!\" as a NOT operator, and \"[\" and \"]\" to bracket expressions.\n"
    "\n"
    "example:\n"
    "  - visible = xbmc.getCondVisibility('[System.KaiEnabled + !Skin.String(KAI)]')\n");

  PyObject* XBMC_GetCondVisibility(PyObject *self, PyObject *args)
  {
    char *cLine = NULL;
    if (!PyArg_ParseTuple(args, "s", &cLine)) return NULL;

    PyGUILock();
    DWORD dwId = m_gWindowManager.GetTopMostModalDialogID();
    if (dwId == WINDOW_INVALID) dwId = m_gWindowManager.GetActiveWindow();
    PyGUIUnlock();

    int ret = g_infoManager.TranslateString(cLine);
    return Py_BuildValue("b", g_infoManager.GetBool(ret,dwId));
  }

  // getGlobalIdleTime() method
  PyDoc_STRVAR(getGlobalIdleTime__doc__,
    "getGlobalIdleTime() -- Returns the elapsed idle time in seconds as an integer.\n"
    "\n"
    "example:\n"
    "  - t = xbmc.getGlobalIdleTime()");

  PyObject* XBMC_GetGlobalIdleTime(PyObject *self)
  {
    return Py_BuildValue("i", g_application.GlobalIdleTime());
  }

  // getCacheThumbName function
  PyDoc_STRVAR(getCacheThumbName__doc__,
    "getCacheThumbName(path) -- Returns a thumb cache filename.\n"
    "\n"
    "path           : string or unicode - path to file\n"
    "\n"
    "example:\n"
    "  - thumb = xbmc.getCacheThumbName('f:\\\\videos\\\\movie.avi')\n");

  PyObject* XBMC_GetCacheThumbName(PyObject *self, PyObject *args)
  {
    PyObject *pObjectText;
    if (!PyArg_ParseTuple(args, "O", &pObjectText)) return NULL;
 
    string strText;
    if (!PyGetUnicodeString(strText, pObjectText, 1)) return NULL;

    Crc32 crc;
    CStdString strPath;
    crc.ComputeFromLowerCase(strText);
    strPath.Format("%08x.tbn", (unsigned __int32)crc);
    return Py_BuildValue("s", strPath.c_str());
  }

  // makeLegalFilename function
  PyDoc_STRVAR(makeLegalFilename__doc__,
    "makeLegalFilename(filename[, fatX]) -- Returns a legal filename or path as a string.\n"
    "\n"
    "filename       : string - filename/path to make legal\n"
    "fatX           : [opt] bool - True=Xbox file system(Default)\n"
    "\n"
    "*Note, If fatX is true you should pass a full path. If fatX is false only pass\n"
    "       the basename of the path.\n"
    "\n"
    "example:\n"
    "  - filename = xbmc.makeLegalFilename('F:\\Trailers\\Ice Age: The Meltdown.avi')\n");

  PyObject* XBMC_MakeLegalFilename(PyObject *self, PyObject *args)
  {
    char *cFilename = NULL;
    bool bIsFatX = true;
    if (!PyArg_ParseTuple(args, "s|b", &cFilename, &bIsFatX)) return NULL;

    CStdString strFilename;
    strFilename = CUtil::MakeLegalFileName(cFilename,bIsFatX);
    return Py_BuildValue("s", strFilename.c_str());
  }

  // translatePath function
  PyDoc_STRVAR(translatePath__doc__,
    "translatePath(path) -- Returns the translated path.\n"
    "\n"
    "path           : string or unicode - Path to format\n"
    "\n"
    "*Note, Only useful if you are coding for both Linux and the Xbox.\n"
    "       e.g. Converts 'T:\\script_data' -> '/home/user/XBMC/UserData/script_data'\n"
    "       on Linux. Would return 'T:\\script_data' on the Xbox.\n"
    "\n"
    "example:\n"
    "  - fpath = xbmc.translatePath('T:\\script_data')\n");

  PyObject* XBMC_TranslatePath(PyObject *self, PyObject *args)
  {
    PyObject *pObjectText;
    if (!PyArg_ParseTuple(args, "O", &pObjectText)) return NULL;

    CStdString strText;
    if (!PyGetUnicodeString(strText, pObjectText, 1)) return NULL;

    CStdString strPath;
    if (strText.Left(3).Equals("P:\\"))
      CUtil::AddFileToFolder(g_settings.GetProfileUserDataFolder(),strText.Mid(3),strText);
    strPath = _P(strText);

    return Py_BuildValue("s", strPath.c_str());
  }

  // getRegion function
  PyDoc_STRVAR(getRegion__doc__,
    "getRegion(id) -- Returns your regions setting as a string for the specified id.\n"
    "\n"
    "id             : string - id of setting to return\n"
    "\n"
    "*Note, choices are (dateshort, datelong, time, meridiem, tempunit, speedunit)\n"
    "\n"
    "example:\n"
    "  - date_long_format = xbmc.getRegion('datelong')\n");

  PyObject* XBMC_GetRegion(PyObject *self, PyObject *args, PyObject *kwds)
  {
    static char *keywords[] = { "id", NULL };
    char *id = NULL;
    // parse arguments to constructor
    if (!PyArg_ParseTupleAndKeywords(
      args,
      kwds,
      "s",
      keywords,
      &id
      ))
    {
      return NULL;
    };

    CStdString result;

    if (strcmpi(id, "datelong") == 0)
      result = g_langInfo.GetDateFormat(true);
    else if (strcmpi(id, "dateshort") == 0)
      result = g_langInfo.GetDateFormat(false);
    else if (strcmpi(id, "tempunit") == 0)
      result = g_langInfo.GetTempUnitString();
    else if (strcmpi(id, "speedunit") == 0)
      result = g_langInfo.GetSpeedUnitString();
    else if (strcmpi(id, "time") == 0)
      result = g_langInfo.GetTimeFormat();
    else if (strcmpi(id, "meridiem") == 0)
      result.Format("%s/%s", g_langInfo.GetMeridiemSymbol(CLangInfo::MERIDIEM_SYMBOL_AM), g_langInfo.GetMeridiemSymbol(CLangInfo::MERIDIEM_SYMBOL_PM));

    return Py_BuildValue("s", result.c_str());
  }

  // getSupportedMedia function
  PyDoc_STRVAR(getSupportedMedia__doc__,
    "getSupportedMedia(media) -- Returns the supported file types for the specific media as a string.\n"
    "\n"
    "media          : string - media type\n"
    "\n"
    "*Note, media type can be (video, music, picture).\n"
    "\n"
    "       The return value is a pipe separated string of filetypes (eg. '.mov|.avi').\n"
    "\n"
    "       You can use the above as keywords for arguments and skip certain optional arguments.\n"
    "       Once you use a keyword, all following arguments require the keyword.\n"
    "\n"
    "example:\n"
    "  - mTypes = xbmc.getSupportedMedia('video')\n");

  PyObject* XBMC_GetSupportedMedia(PyObject *self, PyObject *args, PyObject *kwds)
  {
    static char *keywords[] = { "media", NULL };
    char *media = NULL;
    // parse arguments to constructor
    if (!PyArg_ParseTupleAndKeywords(
      args,
      kwds,
      "s",
      keywords,
      &media
      ))
    {
      return NULL;
    };

    CStdString result;
    if (strcmpi(media, "video") == 0)
      result = g_stSettings.m_videoExtensions;
    else if (strcmpi(media, "music") == 0)
      result = g_stSettings.m_musicExtensions;
    else if (strcmpi(media, "picture") == 0)
      result = g_stSettings.m_pictureExtensions;
    else
    {
      PyErr_SetString(PyExc_ValueError, "media = (video, music, picture)");
      return NULL;
    }

    return Py_BuildValue((char*)"s", result.c_str());
  }

  // skinHasImage function
  PyDoc_STRVAR(skinHasImage__doc__,
    "skinHasImage(image) -- Returns True if the image file exists in the skin.\n"
    "\n"
    "image          : string - image filename\n"
    "\n"
    "*Note, If the media resides in a subfolder include it. (eg. home-myfiles\\\\home-myfiles2.png)\n"
    "\n"
    "       You can use the above as keywords for arguments and skip certain optional arguments.\n"
    "       Once you use a keyword, all following arguments require the keyword.\n"
    "\n"
    "example:\n"
    "  - exists = xbmc.skinHasImage('ButtonFocusedTexture.png')\n");

  PyObject* XBMC_SkinHasImage(PyObject *self, PyObject *args, PyObject *kwds)
  {
    static const char *keywords[] = { "image", NULL };
    char *image = NULL;
    // parse arguments to constructor
    if (!PyArg_ParseTupleAndKeywords(
      args,
      kwds,
      (char*)"s",
      (char**)keywords,
      &image
      ))
    {
      return NULL;
    };

    bool exists = g_TextureManager.HasTexture(image);

    return Py_BuildValue((char*)"b", exists);
  }

  // define c functions to be used in python here
  PyMethodDef xbmcMethods[] = {
    {"output", (PyCFunction)XBMC_Output, METH_VARARGS, output__doc__},
    {"log", (PyCFunction)XBMC_Log, METH_VARARGS, log__doc__},
    {"executescript", (PyCFunction)XBMC_ExecuteScript, METH_VARARGS, executeScript__doc__},
    {"executebuiltin", (PyCFunction)XBMC_ExecuteBuiltIn, METH_VARARGS, executeBuiltIn__doc__},

    {"sleep", (PyCFunction)XBMC_Sleep, METH_VARARGS, sleep__doc__},
    {"shutdown", (PyCFunction)XBMC_Shutdown, METH_VARARGS, shutdown__doc__},
    {"sleepSystem", (PyCFunction)XBMC_SleepSystem, METH_VARARGS, sleepSystem__doc__},
    {"dashboard", (PyCFunction)XBMC_Dashboard, METH_VARARGS, dashboard__doc__},
    {"restart", (PyCFunction)XBMC_Restart, METH_VARARGS, restart__doc__},
    {"getSkinDir", (PyCFunction)XBMC_GetSkinDir, METH_VARARGS, getSkinDir__doc__},
    {"getLocalizedString", (PyCFunction)XBMC_GetLocalizedString, METH_VARARGS, getLocalizedString__doc__},

    {"getLanguage", (PyCFunction)XBMC_GetLanguage, METH_VARARGS, getLanguage__doc__},
    {"getIPAddress", (PyCFunction)XBMC_GetIPAddress, METH_VARARGS, getIPAddress__doc__},
    {"getDVDState", (PyCFunction)XBMC_GetDVDState, METH_VARARGS, getDVDState__doc__},
    {"getFreeMem", (PyCFunction)XBMC_GetFreeMem, METH_VARARGS, getFreeMem__doc__},
    //{"getCpuTemp", (PyCFunction)XBMC_GetCpuTemp, METH_VARARGS, getCpuTemp__doc__},

    {"executehttpapi", (PyCFunction)XBMC_ExecuteHttpApi, METH_VARARGS, executeHttpApi__doc__},
    {"getInfoLabel", (PyCFunction)XBMC_GetInfoLabel, METH_VARARGS, getInfoLabel__doc__},
    {"getInfoImage", (PyCFunction)XBMC_GetInfoImage, METH_VARARGS, getInfoImage__doc__},
    {"getCondVisibility", (PyCFunction)XBMC_GetCondVisibility, METH_VARARGS, getCondVisibility__doc__},
    {"getGlobalIdleTime", (PyCFunction)XBMC_GetGlobalIdleTime, METH_VARARGS, getGlobalIdleTime__doc__},

    {"playSFX", (PyCFunction)XBMC_PlaySFX, METH_VARARGS, playSFX__doc__},
    {"enableNavSounds", (PyCFunction)XBMC_EnableNavSounds, METH_VARARGS, enableNavSounds__doc__},

    {"getCacheThumbName", (PyCFunction)XBMC_GetCacheThumbName, METH_VARARGS, getCacheThumbName__doc__},

    {"makeLegalFilename", (PyCFunction)XBMC_MakeLegalFilename, METH_VARARGS, makeLegalFilename__doc__},
    {"translatePath", (PyCFunction)XBMC_TranslatePath, METH_VARARGS, translatePath__doc__},

    {"getRegion", (PyCFunction)XBMC_GetRegion, METH_VARARGS|METH_KEYWORDS, getRegion__doc__},
    {"getSupportedMedia", (PyCFunction)XBMC_GetSupportedMedia, METH_VARARGS|METH_KEYWORDS, getSupportedMedia__doc__},
    {NULL, NULL, 0, NULL}
  };

/*****************************************************************
 * end of methods and python objects
 * initxbmc(void);
 *****************************************************************/
  PyMODINIT_FUNC
  InitXBMCTypes(bool bInitTypes)
  {
    initKeyboard_Type();
    initPlayer_Type();
    initPlayList_Type();
    initPlayListItem_Type();
    initInfoTagMusic_Type();
    initInfoTagVideo_Type();

    if (PyType_Ready(&Keyboard_Type) < 0 ||
        PyType_Ready(&Player_Type) < 0 ||
        PyType_Ready(&PlayList_Type) < 0 ||
        PyType_Ready(&PlayListItem_Type) < 0 ||
        PyType_Ready(&InfoTagMusic_Type) < 0 ||
        PyType_Ready(&InfoTagVideo_Type) < 0) return;
  }

  PyMODINIT_FUNC
  DeinitXBMCModule()
  {
    Py_DECREF(&Keyboard_Type);
    Py_DECREF(&Player_Type);
    Py_DECREF(&PlayList_Type);
    Py_DECREF(&PlayListItem_Type);
    Py_DECREF(&InfoTagMusic_Type);
    Py_DECREF(&InfoTagVideo_Type);

  }

  PyMODINIT_FUNC
  InitXBMCModule()
  {
    // init general xbmc modules
    PyObject* pXbmcModule;
    
    Py_INCREF(&Keyboard_Type);
    Py_INCREF(&Player_Type);
    Py_INCREF(&PlayList_Type);
    Py_INCREF(&PlayListItem_Type);
    Py_INCREF(&InfoTagMusic_Type);
    Py_INCREF(&InfoTagVideo_Type);

    pXbmcModule = Py_InitModule("xbmc", xbmcMethods);
    if (pXbmcModule == NULL) return;

    PyModule_AddObject(pXbmcModule, "Keyboard", (PyObject*)&Keyboard_Type);
    PyModule_AddObject(pXbmcModule, "Player", (PyObject*)&Player_Type);
    PyModule_AddObject(pXbmcModule, "PlayList", (PyObject*)&PlayList_Type);
    PyModule_AddObject(pXbmcModule, "PlayListItem", (PyObject*)&PlayListItem_Type);
    PyModule_AddObject(pXbmcModule, "InfoTagMusic", (PyObject*)&InfoTagMusic_Type);
    PyModule_AddObject(pXbmcModule, "InfoTagVideo", (PyObject*)&InfoTagVideo_Type);

    // constants
    PyModule_AddStringConstant(pXbmcModule, "__author__", PY_XBMC_AUTHOR);
    PyModule_AddStringConstant(pXbmcModule, "__date__", "15 November 2005");
    PyModule_AddStringConstant(pXbmcModule, "__version__", "1.3");
    PyModule_AddStringConstant(pXbmcModule, "__credits__", PY_XBMC_CREDITS);
    PyModule_AddStringConstant(pXbmcModule, "__platform__", PY_XBMC_PLATFORM);

    // playlist constants
    PyModule_AddIntConstant(pXbmcModule, "PLAYLIST_MUSIC", PLAYLIST_MUSIC);
    //PyModule_AddIntConstant(pXbmcModule, "PLAYLIST_MUSIC_TEMP", PLAYLIST_MUSIC_TEMP);
    PyModule_AddIntConstant(pXbmcModule, "PLAYLIST_VIDEO", PLAYLIST_VIDEO);
    //PyModule_AddIntConstant(pXbmcModule, "PLAYLIST_VIDEO_TEMP", PLAYLIST_VIDEO_TEMP);

    // player constants
    PyModule_AddIntConstant(pXbmcModule, "PLAYER_CORE_AUTO", EPC_NONE);
    PyModule_AddIntConstant(pXbmcModule, "PLAYER_CORE_DVDPLAYER", EPC_DVDPLAYER);
    PyModule_AddIntConstant(pXbmcModule, "PLAYER_CORE_MPLAYER", EPC_MPLAYER);
    PyModule_AddIntConstant(pXbmcModule, "PLAYER_CORE_PAPLAYER", EPC_PAPLAYER);
    PyModule_AddIntConstant(pXbmcModule, "PLAYER_CORE_MODPLAYER", EPC_MODPLAYER);

    // dvd state constants
    PyModule_AddIntConstant(pXbmcModule, "TRAY_OPEN", TRAY_OPEN);
    PyModule_AddIntConstant(pXbmcModule, "DRIVE_NOT_READY", DRIVE_NOT_READY);
    PyModule_AddIntConstant(pXbmcModule, "TRAY_CLOSED_NO_MEDIA", TRAY_CLOSED_NO_MEDIA);
    PyModule_AddIntConstant(pXbmcModule, "TRAY_CLOSED_MEDIA_PRESENT", TRAY_CLOSED_MEDIA_PRESENT);
  }
}

#ifdef __cplusplus
}
#endif
