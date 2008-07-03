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
#include "GUIDialogPluginSettings.h"
#include "FileSystem/PluginDirectory.h"
#include "GUIDialogNumeric.h"
#include "GUIDialogFileBrowser.h"
#include "GUIControlGroupList.h"
#include "Util.h"
#include "MediaManager.h"
#include "GUIRadioButtonControl.h"
#include "GUISpinControlEx.h"
#include "FileSystem/HDDirectory.h"
#include "VideoInfoScanner.h"
#include "ScraperSettings.h"
#include "GUIWindowManager.h"
#include "Application.h"
#include "GUIDialogKeyboard.h"
#include "FileItem.h"

using namespace std;

#define CONTROL_AREA                  2
#define CONTROL_DEFAULT_BUTTON        3
#define CONTROL_DEFAULT_RADIOBUTTON   4
#define CONTROL_DEFAULT_SPIN          5
#define CONTROL_DEFAULT_SEPARATOR     6
#define ID_BUTTON_OK                  10
#define ID_BUTTON_CANCEL              11
#define ID_BUTTON_DEFAULT             12
#define CONTROL_HEADING_LABEL         20
#define CONTROL_START_CONTROL         100

CGUIDialogPluginSettings::CGUIDialogPluginSettings()
   : CGUIDialogBoxBase(WINDOW_DIALOG_PLUGIN_SETTINGS, "DialogPluginSettings.xml")
{}

CGUIDialogPluginSettings::~CGUIDialogPluginSettings(void)
{}

bool CGUIDialogPluginSettings::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_WINDOW_INIT:
    {
      CGUIDialogBoxBase::OnMessage(message);
      FreeControls();
      CreateControls();
      return true;
    }

    case GUI_MSG_CLICKED:
    {
      int iControl = message.GetSenderId();
      bool bCloseDialog = false;

      if (iControl == ID_BUTTON_OK)
        SaveSettings();
      else if (iControl == ID_BUTTON_DEFAULT)
        SetDefaults();
      else
        bCloseDialog = ShowVirtualKeyboard(iControl);

      if (iControl == ID_BUTTON_OK || iControl == ID_BUTTON_CANCEL || bCloseDialog)
      {
        m_bConfirmed = true;
        Close();
        return true;
      }
    }
    break;
  }
  return CGUIDialogBoxBase::OnMessage(message);
}

// \brief Show CGUIDialogOK dialog, then wait for user to dismiss it.
void CGUIDialogPluginSettings::ShowAndGetInput(CURL& url)
{
  m_url = url;

  // Load language strings temporarily
  DIRECTORY::CPluginDirectory::LoadPluginStrings(url);

  // Create the dialog
  CGUIDialogPluginSettings* pDialog = (CGUIDialogPluginSettings*) m_gWindowManager.GetWindow(WINDOW_DIALOG_PLUGIN_SETTINGS);

  pDialog->m_strHeading = m_url.GetFileName();
  CUtil::RemoveSlashAtEnd(pDialog->m_strHeading);
  pDialog->m_strHeading.Format("$LOCALIZE[1045] - %s", pDialog->m_strHeading.c_str());

  CPluginSettings settings;
  settings.Load(m_url);
  pDialog->m_settings = settings;

  pDialog->DoModal();

  settings = pDialog->m_settings;
  settings.Save();

  // Unload temporary language strings
  DIRECTORY::CPluginDirectory::ClearPluginStrings();

  return;
}

// \brief Show CGUIDialogOK dialog, then wait for user to dismiss it.
void CGUIDialogPluginSettings::ShowAndGetInput(SScraperInfo& info)
{
  // Create the dialog
  CGUIDialogPluginSettings* pDialog = (CGUIDialogPluginSettings*) m_gWindowManager.GetWindow(WINDOW_DIALOG_PLUGIN_SETTINGS);

  pDialog->m_settings = info.settings;
  pDialog->m_strHeading.Format("$LOCALIZE[20407] - %s", info.strTitle.c_str());

  pDialog->DoModal();
  info.settings.LoadUserXML(static_cast<CScraperSettings&>(pDialog->m_settings).GetSettings());

  return;
}

bool CGUIDialogPluginSettings::ShowVirtualKeyboard(int iControl)
{
  int controlId = CONTROL_START_CONTROL;
  bool bCloseDialog = false;

  TiXmlElement *setting = m_settings.GetPluginRoot()->FirstChildElement("setting");
  while (setting)
  {
    if (controlId == iControl)
    {
      const CGUIControl* control = GetControl(controlId);
      if (control->GetControlType() == CGUIControl::GUICONTROL_BUTTON)
      {
        const char *type = setting->Attribute("type");
        const char *option = setting->Attribute("option");
        const char *source = setting->Attribute("source");
        CStdString value = ((CGUIButtonControl*) control)->GetLabel2();

        if (strcmp(type, "text") == 0)
        {
          // get any options
          bool bHidden = false;
          if (option)
            bHidden = (strcmp(option, "hidden") == 0);

          if (CGUIDialogKeyboard::ShowAndGetInput(value, ((CGUIButtonControl*) control)->GetLabel(), true, bHidden))
            ((CGUIButtonControl*) control)->SetLabel2(value);
        }
        else if (strcmp(type, "integer") == 0 && CGUIDialogNumeric::ShowAndGetNumber(value, ((CGUIButtonControl*) control)->GetLabel()))
        {
          ((CGUIButtonControl*) control)->SetLabel2(value);
        }
        else if (strcmp(type, "ipaddress") == 0 && CGUIDialogNumeric::ShowAndGetIPAddress(value, ((CGUIButtonControl*) control)->GetLabel()))
        {
          ((CGUIButtonControl*) control)->SetLabel2(value);
        }
        else if (strcmpi(type, "video") == 0 || strcmpi(type, "music") == 0 ||
          strcmpi(type, "pictures") == 0 || strcmpi(type, "programs") == 0 || 
          strcmpi(type, "folder") == 0 || strcmpi(type, "files") == 0)
        {
          // setup the shares
          VECSOURCES *shares = NULL;
          if (!source || strcmpi(source, "") == 0)
            shares = g_settings.GetSourcesFromType(type);
          else
            shares = g_settings.GetSourcesFromType(source);

          if (!shares)
          {
            VECSOURCES localShares, networkShares;
            g_mediaManager.GetLocalDrives(localShares);
            if (!source || strcmpi(source, "local") != 0)
              g_mediaManager.GetNetworkLocations(networkShares);
            localShares.insert(localShares.end(), networkShares.begin(), networkShares.end());
            shares = &localShares;
          }
          
          if (strcmpi(type, "folder") == 0)
          {
            // get any options
            bool bWriteOnly = false;
            if (option)
              bWriteOnly = (strcmpi(option, "writeable") == 0);

            if (CGUIDialogFileBrowser::ShowAndGetDirectory(*shares, ((CGUIButtonControl*) control)->GetLabel(), value, bWriteOnly))
              ((CGUIButtonControl*) control)->SetLabel2(value);
          }
          else if (strcmpi(type, "picture") == 0)
          {
            if (CGUIDialogFileBrowser::ShowAndGetImage(*shares, ((CGUIButtonControl*) control)->GetLabel(), value))
              ((CGUIButtonControl*) control)->SetLabel2(value);
          }
          else
          {
            // set the proper mask
            CStdString strMask;
            if (setting->Attribute("mask"))
              strMask = setting->Attribute("mask");
            else
            {
              if (strcmpi(type, "video") == 0)
                strMask = g_stSettings.m_videoExtensions;
              else if (strcmpi(type, "music") == 0)
                strMask = g_stSettings.m_musicExtensions;
              else if (strcmpi(type, "programs") == 0)
                strMask = ".xbe|.py";
            }

            // get any options
            bool bUseThumbs = false;
            bool bUseFileDirectories = false;
            if (option)
            {
              bUseThumbs = (strcmpi(option, "usethumbs") == 0 || strcmpi(option, "usethumbs|treatasfolder") == 0);
              bUseFileDirectories = (strcmpi(option, "treatasfolder") == 0 || strcmpi(option, "usethumbs|treatasfolder") == 0);
            }

            if (CGUIDialogFileBrowser::ShowAndGetFile(*shares, strMask, ((CGUIButtonControl*) control)->GetLabel(), value))
              ((CGUIButtonControl*) control)->SetLabel2(value);
          }
        }
        else if (strcmpi(type, "action") == 0)
        {
          if (setting->Attribute("default"))
          {
            if (option)
              bCloseDialog = (strcmpi(option, "close") == 0);
            g_application.getApplicationMessenger().ExecBuiltIn(setting->Attribute("default"));
          }
        }
        break;
      }
    }
    setting = setting->NextSiblingElement("setting");
    controlId++;
  }
  EnableControls();
  return bCloseDialog;
}

// Go over all the settings and set their values according to the values of the GUI components
bool CGUIDialogPluginSettings::SaveSettings(void)
{
  // Retrieve all the values from the GUI components and put them in the model
  int controlId = CONTROL_START_CONTROL;
  TiXmlElement *setting = m_settings.GetPluginRoot()->FirstChildElement("setting");
  while (setting)
  {
    CStdString id;
    if (setting->Attribute("id"))
      id = setting->Attribute("id");
    const char *type = setting->Attribute("type");
    const CGUIControl* control = GetControl(controlId);

    CStdString value;
    switch (control->GetControlType())
    {
      case CGUIControl::GUICONTROL_BUTTON:
        value = ((CGUIButtonControl*) control)->GetLabel2();
        break;
      case CGUIControl::GUICONTROL_RADIO:
        value = ((CGUIRadioButtonControl*) control)->IsSelected() ? "true" : "false";
        break;
      case CGUIControl::GUICONTROL_SPINEX:
        if (strcmpi(type, "fileenum") == 0 || strcmpi(type, "labelenum") == 0)
          value = ((CGUISpinControlEx*) control)->GetLabel();
        else
          value.Format("%i", ((CGUISpinControlEx*) control)->GetValue());
        break;
      default:
        break;
    }
    m_settings.Set(id, value);

    setting = setting->NextSiblingElement("setting");
    controlId++;
  }
  return true;
}

void CGUIDialogPluginSettings::FreeControls()
{
  // clear the category group
  CGUIControlGroupList *control = (CGUIControlGroupList *)GetControl(CONTROL_AREA);
  if (control)
  {
    control->FreeResources();
    control->ClearAll();
  }
}

void CGUIDialogPluginSettings::CreateControls()
{
  CGUISpinControlEx *pOriginalSpin = (CGUISpinControlEx*)GetControl(CONTROL_DEFAULT_SPIN);
  CGUIRadioButtonControl *pOriginalRadioButton = (CGUIRadioButtonControl *)GetControl(CONTROL_DEFAULT_RADIOBUTTON);
  CGUIButtonControl *pOriginalButton = (CGUIButtonControl *)GetControl(CONTROL_DEFAULT_BUTTON);
  CGUIImage *pOriginalImage = (CGUIImage *)GetControl(CONTROL_DEFAULT_SEPARATOR);

  if (!pOriginalSpin || !pOriginalRadioButton || !pOriginalButton || !pOriginalImage)
    return;

  pOriginalSpin->SetVisible(false);
  pOriginalRadioButton->SetVisible(false);
  pOriginalButton->SetVisible(false);
  pOriginalImage->SetVisible(false);

  // clear the category group
  CGUIControlGroupList *group = (CGUIControlGroupList *)GetControl(CONTROL_AREA);
  if (!group)
    return;

  // set our dialog heading
  SET_CONTROL_LABEL(CONTROL_HEADING_LABEL, m_strHeading);

  // Create our base path, used for type "fileenum" settings
  CStdString basepath = "Q:\\plugins\\";
  CUtil::AddFileToFolder(basepath, m_url.GetHostName(), basepath);
  CUtil::AddFileToFolder(basepath, m_url.GetFileName(), basepath);
  // Replace the / at end, GetFileName() leaves a / at the end
  basepath.Replace("/", "\\");

  CGUIControl* pControl = NULL;
  int controlId = CONTROL_START_CONTROL;
  TiXmlElement *setting = m_settings.GetPluginRoot()->FirstChildElement("setting");
  while (setting)
  {
    const char *type = setting->Attribute("type");
    const char *id = setting->Attribute("id");
    CStdString values;
    if (setting->Attribute("values"))
      values = setting->Attribute("values");
    CStdString lvalues;
    if (setting->Attribute("lvalues"))
      lvalues = setting->Attribute("lvalues");
    CStdString entries;
    if (setting->Attribute("entries"))
      entries = setting->Attribute("entries");
    CStdString label;
    if (setting->Attribute("label") && atoi(setting->Attribute("label")) > 0)
      label.Format("$LOCALIZE[%s]", setting->Attribute("label"));
    else
      label = setting->Attribute("label");

    if (strcmpi(type, "text") == 0 || strcmpi(type, "ipaddress") == 0 ||
      strcmpi(type, "integer") == 0 || strcmpi(type, "video") == 0 ||
      strcmpi(type, "music") == 0 || strcmpi(type, "pictures") == 0 ||
      strcmpi(type, "folder") == 0 || strcmpi(type, "programs") == 0 ||
      strcmpi(type, "files") == 0 || strcmpi(type, "action") == 0)
    {
      pControl = new CGUIButtonControl(*pOriginalButton);
      if (!pControl) return;
      ((CGUIButtonControl *)pControl)->SettingsCategorySetTextAlign(XBFONT_CENTER_Y);
      ((CGUIButtonControl *)pControl)->SetLabel(label);
      if (id)
        ((CGUIButtonControl *)pControl)->SetLabel2(m_settings.Get(id));
    }
    else if (strcmpi(type, "bool") == 0)
    {
      pControl = new CGUIRadioButtonControl(*pOriginalRadioButton);
      if (!pControl) return;
      ((CGUIRadioButtonControl *)pControl)->SetLabel(label);
      ((CGUIRadioButtonControl *)pControl)->SetSelected(m_settings.Get(id) == "true");
    }
    else if (strcmpi(type, "enum") == 0 || strcmpi(type, "labelenum") == 0)
    {
      vector<CStdString> valuesVec;
      vector<CStdString> entryVec;

      pControl = new CGUISpinControlEx(*pOriginalSpin);
      if (!pControl) return;
      ((CGUISpinControlEx *)pControl)->SetText(label);

      if (!lvalues.IsEmpty())
        CUtil::Tokenize(lvalues, valuesVec, "|");
      else
        CUtil::Tokenize(values, valuesVec, "|");
      if (!entries.IsEmpty())
        CUtil::Tokenize(entries, entryVec, "|");
      for (unsigned int i = 0; i < valuesVec.size(); i++)
      {
        int iAdd = i;
        if (entryVec.size() > i)
          iAdd = atoi(entryVec[i]);
        if (!lvalues.IsEmpty())
        {
          CStdString replace = g_localizeStringsTemp.Get(atoi(valuesVec[i]));
          if (replace.IsEmpty())
            replace = g_localizeStrings.Get(atoi(valuesVec[i]));
          ((CGUISpinControlEx *)pControl)->AddLabel(replace, iAdd);
        }
        else
          ((CGUISpinControlEx *)pControl)->AddLabel(valuesVec[i], iAdd);
      }
      if (strcmpi(type, "labelenum") == 0)
      { // need to run through all our settings and find the one that matches
        ((CGUISpinControlEx*) pControl)->SetValueFromLabel(m_settings.Get(id));
      }
      else
        ((CGUISpinControlEx*) pControl)->SetValue(atoi(m_settings.Get(id)));

    }
    else if (strcmpi(type, "fileenum") == 0)
    {
      pControl = new CGUISpinControlEx(*pOriginalSpin);
      if (!pControl) return;
      ((CGUISpinControlEx *)pControl)->SetText(label);

      //find Folders...
      DIRECTORY::CHDDirectory directory;
      CFileItemList items;
      CStdString enumpath;
      CUtil::AddFileToFolder(basepath, values, enumpath);
      CStdString mask;
      if (setting->Attribute("mask"))
        mask = setting->Attribute("mask");
      if (!mask.IsEmpty())
        directory.SetMask(mask);
      directory.GetDirectory(enumpath, items);

      int iItem = 0;
      for (int i = 0; i < items.Size(); ++i)
      {
        CFileItem* pItem = items[i];
        if ((mask.Equals("/") && pItem->m_bIsFolder) || !pItem->m_bIsFolder)
        {
          ((CGUISpinControlEx *)pControl)->AddLabel(pItem->GetLabel(), iItem);
          if (pItem->GetLabel().Equals(m_settings.Get(id)))
            ((CGUISpinControlEx *)pControl)->SetValue(iItem);
          iItem++;
        }
      }
    }
    else if (strcmpi(type, "sep") == 0 && pOriginalImage)
      pControl = new CGUIImage(*pOriginalImage);

    if (pControl)
    {
      pControl->SetWidth(group->GetWidth());
      pControl->SetVisible(true);
      pControl->SetID(controlId);
      pControl->AllocResources();
      group->AddControl(pControl);
      pControl = NULL;
    }

    setting = setting->NextSiblingElement("setting");
    controlId++;
  }
  EnableControls();
}

// Go over all the settings and set their enabled condition according to the values of the enabled attribute
void CGUIDialogPluginSettings::EnableControls()
{
  int controlId = CONTROL_START_CONTROL;
  TiXmlElement *setting = m_settings.GetPluginRoot()->FirstChildElement("setting");
  while (setting)
  {
    const CGUIControl* control = GetControl(controlId);
    if (control)
    {
      // set enable status
      if (setting->Attribute("enable"))
        ((CGUIControl*) control)->SetEnabled(GetCondition(setting->Attribute("enable"), controlId));
      else
        ((CGUIControl*) control)->SetEnabled(true);
      // set visible status
      if (setting->Attribute("visible"))
        ((CGUIControl*) control)->SetVisible(GetCondition(setting->Attribute("visible"), controlId));
      else
        ((CGUIControl*) control)->SetVisible(true);
    }
    setting = setting->NextSiblingElement("setting");
    controlId++;
  }
}

bool CGUIDialogPluginSettings::GetCondition(const CStdString &condition, const int controlId)
{
  if (condition.IsEmpty()) return true;

  vector<CStdString> conditionVec;
  CUtil::Tokenize(condition, conditionVec, "+");
  
  bool bCondition = true;
  
  for (unsigned int i = 0; i < conditionVec.size(); i++)
  {
    vector<CStdString> condVec;
    if (!TranslateSingleString(conditionVec[i], condVec)) continue;

    const CGUIControl* control2 = GetControl(controlId + atoi(condVec[1]));

    CStdString value;
    switch (control2->GetControlType())
    {
      case CGUIControl::GUICONTROL_BUTTON:
        value = ((CGUIButtonControl*) control2)->GetLabel2();
        break;
      case CGUIControl::GUICONTROL_RADIO:
        value = ((CGUIRadioButtonControl*) control2)->IsSelected() ? "true" : "false";
        break;
      case CGUIControl::GUICONTROL_SPINEX:
        value.Format("%i", ((CGUISpinControlEx*) control2)->GetValue());
        break;
      default:
        break;
    }
    if (condVec[0].Equals("eq"))
      bCondition &= value.Equals(condVec[2]);
    else if (condVec[0].Equals("!eq"))
      bCondition &= !value.Equals(condVec[2]);
    else if (condVec[0].Equals("gt"))
      bCondition &= (atoi(value) > atoi(condVec[2]));
    else if (condVec[0].Equals("lt"))
      bCondition &= (atoi(value) < atoi(condVec[2]));
  }
  return bCondition;
}

bool CGUIDialogPluginSettings::TranslateSingleString(const CStdString &strCondition, vector<CStdString> &condVec)
{
  CStdString strTest = strCondition;
  strTest.ToLower();
  strTest.TrimLeft(" ");
  strTest.TrimRight(" ");

  int pos1 = strTest.Find("(");
  int pos2 = strTest.Find(",");
  int pos3 = strTest.Find(")");
  if (pos1 >= 0 && pos2 > pos1 && pos3 > pos2)
  {
    condVec.push_back(strTest.Left(pos1));
    condVec.push_back(strTest.Mid(pos1 + 1, pos2 - pos1 - 1));
    condVec.push_back(strTest.Mid(pos2 + 1, pos3 - pos2 - 1));
    return true;
  }
  return false;
}

// Go over all the settings and set their default values
void CGUIDialogPluginSettings::SetDefaults()
{
  int controlId = CONTROL_START_CONTROL;
  TiXmlElement *setting = m_settings.GetPluginRoot()->FirstChildElement("setting");
  while (setting)
  {
    const CGUIControl* control = GetControl(controlId);
    if (control)
    {
      CStdString value;
      switch (control->GetControlType())
      {
        case CGUIControl::GUICONTROL_BUTTON:
          if (setting->Attribute("default") && setting->Attribute("id"))
            ((CGUIButtonControl*) control)->SetLabel2(setting->Attribute("default"));
          else
            ((CGUIButtonControl*) control)->SetLabel2("");
          break;
        case CGUIControl::GUICONTROL_RADIO:
          if (setting->Attribute("default"))
            ((CGUIRadioButtonControl*) control)->SetSelected(strcmpi(setting->Attribute("default"), "true") == 0);
          else
            ((CGUIRadioButtonControl*) control)->SetSelected(false);
          break;
        case CGUIControl::GUICONTROL_SPINEX:
          {
            if (setting->Attribute("default"))
            {
              if (strcmpi(setting->Attribute("type"), "fileenum") == 0 || strcmpi(setting->Attribute("type"), "labelenum") == 0)
              { // need to run through all our settings and find the one that matches
                  ((CGUISpinControlEx*) control)->SetValueFromLabel(setting->Attribute("default"));
              }
              else
                ((CGUISpinControlEx*) control)->SetValue(atoi(setting->Attribute("default")));
            }
            else
              ((CGUISpinControlEx*) control)->SetValue(0);
          }
          break;
        default:
          break;
      }
    }
    setting = setting->NextSiblingElement("setting");
    controlId++;
  }
  EnableControls();
}

CURL CGUIDialogPluginSettings::m_url;

