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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <assert.h>
#include <mach-o/dyld.h>
#include <errno.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <string>

using namespace std;

#include "XBMCHelper.h"

#include "CocoaUtils.h"
#include "PlatformDefs.h"
#include "log.h"
#include "system.h"
#include "Settings.h"
#include "Util.h"
#include "XFileUtils.h"

XBMCHelper g_xbmcHelper;

#define PLEX_HELPER_PROGRAM "PlexHelper"
#define SOFA_CONTROL_PROGRAM "Sofa Control"
#define XBMC_LAUNCH_PLIST "com.plexapp.helper.plist"
#define RESOURCES_DIR "/Library/Application Support/Plex"

static int GetBSDProcessList(kinfo_proc **procList, size_t *procCount);

/////////////////////////////////////////////////////////////////////////////
XBMCHelper::XBMCHelper()
  : m_errorStarting(false)
  , m_mode(APPLE_REMOTE_DISABLED)
  , m_alwaysOn(false)
  , m_sequenceDelay(0)
{
  CStdString homePath;
  CUtil::GetHomePath(homePath);

  // Compute the helper filename.
  m_helperFile = homePath + "/" PLEX_HELPER_PROGRAM;

  // Compute the local (pristine) launch agent filename.
  m_launchAgentLocalFile = homePath + "/" XBMC_LAUNCH_PLIST;

  // Compute the install path for the launch agent.
  m_launchAgentInstallFile = getenv("HOME");
  m_launchAgentInstallFile += "/Library/LaunchAgents/" XBMC_LAUNCH_PLIST;

  // Compute the configuration file name.
  m_configFile = getenv("HOME");
  m_configFile += RESOURCES_DIR "/" PLEX_HELPER_PROGRAM ".conf";

  // This is where we install the helper.
  m_helperInstalledFile = getenv("HOME");
  m_helperInstalledFile += RESOURCES_DIR "/" PLEX_HELPER_PROGRAM;

  // This is where we store the installed version of helper.
  m_helperInstalledVersionFile = getenv("HOME");
  m_helperInstalledVersionFile += RESOURCES_DIR "/" PLEX_HELPER_PROGRAM ".version";
  
  ////// ONLY ONCE - uninstall and delete old helper, stop.
  CStdString oldHelper = getenv("HOME");
  oldHelper += "/Library/LaunchAgents/";
  oldHelper += "org.xbmc.helper.plist";
  
  if (::access(oldHelper.c_str(), R_OK) == 0)
  {
    string cmd = "/bin/launchctl unload ";
    cmd += oldHelper;
    system(cmd.c_str());
    DeleteFile(oldHelper.c_str());
  
    int pid = GetProcessPid("XBMCHelper");
    if (pid != -1)
      kill(pid, SIGKILL);
  }
  //////
}

/////////////////////////////////////////////////////////////////////////////
bool XBMCHelper::EnsureLatestHelperInstalled()
{
  std::string appVersion = Cocoa_GetAppVersion();
  std::string installedVersion = GetInstalledHelperVersion();

  if (/* appVersion.length() > 0 && */ appVersion != installedVersion ||
      ::access(m_helperInstalledFile.c_str(), R_OK) != 0)
  {
    CLog::Log(LOGNOTICE, "Detected change in helper version, it was '%s,' and application version was '%s'.\n", installedVersion.c_str(), appVersion.c_str());

    // Whack old helper.
    DeleteFile(m_helperInstalledFile.c_str());

    // Copy helper and ensure executable.
    CopyFile(m_helperFile.c_str(), m_helperInstalledFile.c_str(), FALSE);
    chmod(m_helperInstalledFile.c_str(), S_IRWXU | S_IRGRP | S_IROTH);

    // Write version file.
    WriteFile(m_helperInstalledVersionFile.c_str(), appVersion);
    return true;
  }

  return false;
}

/////////////////////////////////////////////////////////////////////////////
void XBMCHelper::Start()
{
  if (GetProcessPid(PLEX_HELPER_PROGRAM) == -1)
  {
    CLog::Log(LOGNOTICE, "Asking PlexHelper to start.");

    string cmd = "\"" + m_helperInstalledFile + "\" -x &";
    system(cmd.c_str());
  }
}

/////////////////////////////////////////////////////////////////////////////
void XBMCHelper::Stop(bool hup)
{
  // Kill the process.
  int pid = GetProcessPid(PLEX_HELPER_PROGRAM);
  if (pid != -1)
  {
    CLog::Log(LOGNOTICE, "Asking PlexHelper to %s.", hup ? "reconfigure" : "stop");
    kill(pid, (hup ? SIGHUP : SIGKILL));
  }
  else
  {
    CLog::Log(LOGNOTICE, "PlexHelper is not running");
  }
}

/////////////////////////////////////////////////////////////////////////////
void XBMCHelper::Configure()
{
  int oldMode = m_mode;
  int oldDelay = m_sequenceDelay;
  int oldAlwaysOn = m_alwaysOn;
  int oldSecureInput = m_secureInput;

  // Read the new configuration.
  m_errorStarting = false;
  m_mode = g_guiSettings.GetInt("appleremote.mode");
  m_sequenceDelay = g_guiSettings.GetInt("appleremote.sequencetime");
  m_alwaysOn = g_guiSettings.GetBool("appleremote.alwayson");
  m_secureInput = g_guiSettings.GetBool("appleremote.secureinput");

  // Don't let it enable if sofa control or remote buddy is around.
  if (/* IsRemoteBuddyInstalled() || */ IsSofaControlRunning())
  {
    // If we were starting then remember error.
    if (oldMode == APPLE_REMOTE_DISABLED && m_mode != APPLE_REMOTE_DISABLED)
      m_errorStarting = true;

    m_mode = APPLE_REMOTE_DISABLED;
    g_guiSettings.SetInt("appleremote.mode", APPLE_REMOTE_DISABLED);
  }

  // New configuration.
  if (oldMode != m_mode || oldDelay != m_sequenceDelay || oldSecureInput != m_secureInput)
  {
    // Build a new config string.
    std::string strConfig;
    if (m_mode == APPLE_REMOTE_UNIVERSAL)
      strConfig = "--universal ";

    // Delay.
    char strDelay[64];
    sprintf(strDelay, "--timeout %d ", m_sequenceDelay);
    strConfig += strDelay;

    // Secure input.
    char strSecure[64];
    sprintf(strSecure, "--secureInput %d ", m_secureInput ? 1 : 0);
    strConfig += strSecure;
    
    // Find out where we're running from.
    char     given_path[2*MAXPATHLEN];
    uint32_t path_size = 2*MAXPATHLEN;

    int result = _NSGetExecutablePath(given_path, &path_size);
    if (result == 0)
    {
      char real_path[2*MAXPATHLEN];
      if (realpath(given_path, real_path) != NULL)
      {
        // Move backwards out to the application.
        for (int x=0; x<4; x++)
        {
          for (int n=strlen(real_path)-1; real_path[n] != '/'; n--)
            real_path[n] = '\0';
        
          real_path[strlen(real_path)-1] = '\0';
        }
      }
      
      strConfig += "--appLocation \"";
      strConfig += real_path;
      strConfig += "\"";
    }
    
    // Write the new configuration.
    WriteFile(m_configFile.c_str(), strConfig + "\n");

    // If process is running, kill -HUP to have it reload settings.
    Stop(TRUE);
  }

  // Make sure latest helper is installed.
  if (EnsureLatestHelperInstalled() == true)
  {
    CLog::Log(LOGNOTICE, "Version of helper changed, uninstalling and stopping.");

    // Things changed. Uninstall and stop.
    Uninstall();
    Stop();

    // Make sure we reinstall/start if needed.
    oldMode = APPLE_REMOTE_DISABLED;
    oldAlwaysOn = false;
  }

  // Turning off?
  if (oldMode != APPLE_REMOTE_DISABLED && m_mode == APPLE_REMOTE_DISABLED)
  {
    Stop();
    Uninstall();
  }

  // Turning on.
  if (oldMode == APPLE_REMOTE_DISABLED && m_mode != APPLE_REMOTE_DISABLED)
    Start();

  // Installation/uninstallation.
  if (oldAlwaysOn == false && m_alwaysOn == true)
    Install();
  if (oldAlwaysOn == true && m_alwaysOn == false)
    Uninstall();
}

/////////////////////////////////////////////////////////////////////////////
std::string XBMCHelper::GetInstalledHelperVersion()
{
  if (::access(m_helperInstalledVersionFile.c_str(), R_OK) == 0)
    return ReadFile(m_helperInstalledVersionFile.c_str());
  else
    return "???";
}

/////////////////////////////////////////////////////////////////////////////
void XBMCHelper::Install()
{
  if (::access(m_launchAgentLocalFile.c_str(), R_OK) == 0 && // Launch agent template exists
      ::access(m_launchAgentInstallFile.c_str(), R_OK) != 0) // Not already installed.
  {
    // Make sure directory exists.
    string strDir = getenv("HOME");
    strDir += "/Library/LaunchAgents";
    CreateDirectory(strDir.c_str(), NULL);

    // Load template.
    string plistData = ReadFile(m_launchAgentLocalFile.c_str());

    if (plistData != "")
    {
      CLog::Log(LOGNOTICE, "Installing PlexHelper at %s", m_launchAgentInstallFile.c_str());

      // Replace it in the file.
      int start = plistData.find("${PATH}");
      plistData.replace(start, 7, m_helperInstalledFile.c_str(), m_helperInstalledFile.length());

      // Install it.
      WriteFile(m_launchAgentInstallFile.c_str(), plistData);

      // Load it.
      string cmd = "/bin/launchctl load ";
      cmd += m_launchAgentInstallFile;
      system(cmd.c_str());
    }
    else
    {
      CLog::Log(LOGERROR, "Unable to install \"%s\". Launch agent template (%s) is corrupted.",
                PLEX_HELPER_PROGRAM, m_launchAgentLocalFile.c_str());
    }
  }
  else
  {
    if (::access(m_launchAgentLocalFile.c_str(), R_OK) != 0)
      CLog::Log(LOGERROR, "Unable to install \"%s\". Unable to find launch agent template (%s).",
                 PLEX_HELPER_PROGRAM, m_launchAgentLocalFile.c_str());
  }
}

/////////////////////////////////////////////////////////////////////////////
void XBMCHelper::Uninstall()
{
  if (::access(m_launchAgentInstallFile.c_str(), R_OK) == 0)
  {
    CLog::Log(LOGNOTICE, "Uninstalling PlexHelper from %s.", m_launchAgentInstallFile.c_str());

    // Call the unloader.
    string cmd = "/bin/launchctl unload ";
    cmd += m_launchAgentInstallFile;
    system(cmd.c_str());

    // Remove the plist file.
    DeleteFile(m_launchAgentInstallFile.c_str());
  }
  else
  {
    CLog::Log(LOGNOTICE, "Asked to uninstalling PlexHelper, but it wasn't installed.");
  }
}

/////////////////////////////////////////////////////////////////////////////
bool XBMCHelper::IsRemoteBuddyInstalled()
{
  // Check for existence of kext file.
  return ::access("/System/Library/Extensions/RBIOKitHelper.kext", R_OK) != -1;
}

/////////////////////////////////////////////////////////////////////////////
bool XBMCHelper::IsSofaControlRunning()
{
  // Check for a "Sofa Control" process running.
  return GetProcessPid(SOFA_CONTROL_PROGRAM) != -1;
}

/////////////////////////////////////////////////////////////////////////////
std::string XBMCHelper::ReadFile(const char* fileName)
{
  ifstream is;
  is.open(fileName);
  if (!is.is_open())
    return "";

  // Get length of file:
  is.seekg (0, ios::end);
  int length = is.tellg();
  is.seekg (0, ios::beg);

  // Allocate memory:
  char* buffer = new char [length+1];

  // Read data as a block:
  is.read(buffer,length);
  is.close();
  buffer[length] = '\0';

  std::string ret = buffer;
  delete[] buffer;
  return ret;
}

/////////////////////////////////////////////////////////////////////////////
void XBMCHelper::WriteFile(const char* fileName, const std::string& data)
{
  ofstream out(fileName);
  if (!out)
  {
    CLog::Log(LOGERROR, "PlexHelper: Unable to open file '%s'", fileName);
  }
  else
  {
    // Write new configuration.
    out << data;
    out.flush();
    out.close();
  }
}

/////////////////////////////////////////////////////////////////////////////
extern "C" int GetProcessPid(const char* processName)
{
  return XBMCHelper::GetProcessPid(processName);
}

/////////////////////////////////////////////////////////////////////////////
int XBMCHelper::GetProcessPid(const char* strProgram)
{
  kinfo_proc* mylist;
  size_t mycount = 0;
  int ret = -1;

  GetBSDProcessList(&mylist, &mycount);
  for (size_t k = 0; k < mycount && ret == -1; k++)
  {
    kinfo_proc *proc = NULL;
    proc = &mylist[k];
    
    if (strcmp(proc->kp_proc.p_comm, strProgram) == 0)
    {
      //if (ignorePid == 0 || ignorePid != proc->kp_proc.p_pid)
      ret = proc->kp_proc.p_pid;
    }

  }

  free (mylist);

  return ret;
}

typedef struct kinfo_proc kinfo_proc;

// Returns a list of all BSD processes on the system.  This routine
// allocates the list and puts it in *procList and a count of the
// number of entries in *procCount.  You are responsible for freeing
// this list (use "free" from System framework).
// On success, the function returns 0.
// On error, the function returns a BSD errno value.
//
static int GetBSDProcessList(kinfo_proc **procList, size_t *procCount)
{
  int err;
  kinfo_proc * result;
  bool done;
  static const int name[] =
  { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };

  // Declaring name as const requires us to cast it when passing it to
  // sysctl because the prototype doesn't include the const modifier.
  size_t length;

  assert(procList != NULL);
  assert(procCount != NULL);

  *procCount = 0;

  // We start by calling sysctl with result == NULL and length == 0.
  // That will succeed, and set length to the appropriate length.
  // We then allocate a buffer of that size and call sysctl again
  // with that buffer.  If that succeeds, we're done.  If that fails
  // with ENOMEM, we have to throw away our buffer and loop.  Note
  // that the loop causes use to call sysctl with NULL again; this
  // is necessary because the ENOMEM failure case sets length to
  // the amount of data returned, not the amount of data that
  // could have been returned.
  //
  result = NULL;
  done = false;
  do
  {
    assert(result == NULL);

    // Call sysctl with a NULL buffer.
    length = 0;
    err = sysctl((int *) name, (sizeof(name) / sizeof(*name)) - 1, NULL,
        &length, NULL, 0);
    if (err == -1)
      err = errno;

    // Allocate an appropriately sized buffer based on the results from the previous call.
    if (err == 0)
    {
      result = (kinfo_proc*) malloc(length);
      if (result == NULL)
        err = ENOMEM;
    }

    // Call sysctl again with the new buffer.  If we get an ENOMEM
    // error, toss away our buffer and start again.
    //
    if (err == 0)
    {
      err = sysctl((int *) name, (sizeof(name) / sizeof(*name)) - 1, result,
          &length, NULL, 0);

      if (err == -1)
        err = errno;
      else if (err == 0)
        done = true;
      else if (err == ENOMEM)
      {
        assert(result != NULL);
        free(result);
        result = NULL;
        err = 0;
      }
    }
  } while (err == 0 && !done);

  // Clean up and establish post conditions.
  if (err != 0 && result != NULL)
  {
    free(result);
    result = NULL;
  }

  *procList = result;
  if (err == 0)
    *procCount = length / sizeof(kinfo_proc);

  assert( (err == 0) == (*procList != NULL) );
  return err;
}
