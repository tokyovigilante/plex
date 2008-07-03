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

#include "include.h"
#include "GUIListItemLayout.h"
#include "GUIListItem.h"
#include "GuiControlFactory.h"
#include "GUIFontManager.h"
#include "XMLUtils.h"
#include "SkinInfo.h"
#include "utils/GUIInfoManager.h"
#include "utils/CharsetConverter.h"
#include "FileItem.h"

using namespace std;

CGUIListItemLayout::CListBase::CListBase()
{
}

CGUIListItemLayout::CListBase::~CListBase()
{
}

CGUIListItemLayout::CListLabel::CListLabel(float posX, float posY, float width, float height, int visibleCondition, const CLabelInfo &label, bool alwaysScroll, const CGUIInfoLabel &content, const vector<CAnimation> &animations)
: CGUIListItemLayout::CListBase(),
  m_label(0, 0, posX, posY, width, height, label, alwaysScroll)
{
  m_type = LIST_LABEL;
  m_label.SetAnimations(animations);
  m_info = content;
  m_label.SetVisibleCondition(visibleCondition, false);
}

CGUIListItemLayout::CListLabel::~CListLabel()
{
}

CGUIListItemLayout::CListSelectLabel::CListSelectLabel(float posX, float posY, float width, float height, int visibleCondition, const CImage &imageFocus, const CImage &imageNoFocus, const CLabelInfo &label, const CGUIInfoLabel &content, const vector<CAnimation> &animations)
: CGUIListItemLayout::CListBase(),
  m_label(0, 0, posX, posY, width, height, imageFocus, imageNoFocus, label, CGUIInfoLabel(""))
{
  m_type = LIST_SELECT_LABEL;
  m_info = content;
  m_label.SetAnimations(animations);
  m_label.SetVisibleCondition(visibleCondition, false);
}

CGUIListItemLayout::CListSelectLabel::~CListSelectLabel()
{
}


CGUIListItemLayout::CListTexture::CListTexture(float posX, float posY, float width, float height, int visibleCondition, const CImage &image, const CImage &borderImage, const FRECT &borderSize, const CGUIImage::CAspectRatio &aspectRatio, const CGUIInfoColor &colorDiffuse, const vector<CAnimation> &animations)
: CGUIListItemLayout::CListBase(),
  m_image(0, 0, posX, posY, width, height, image, borderImage, borderSize)
{
  m_type = LIST_TEXTURE;
  m_image.SetAspectRatio(aspectRatio);
  m_image.SetAnimations(animations);
  m_image.SetColorDiffuse(colorDiffuse);
  m_image.SetVisibleCondition(visibleCondition, false);
}

CGUIListItemLayout::CListTexture::~CListTexture()
{
  m_image.FreeResources();
}

CGUIListItemLayout::CListImage::CListImage(float posX, float posY, float width, float height, int visibleCondition, const CImage &image, const CImage &borderImage, const FRECT &borderSize, const CGUIImage::CAspectRatio &aspectRatio, const CGUIInfoColor &colorDiffuse, const CGUIInfoLabel &content, const vector<CAnimation> &animations)
: CGUIListItemLayout::CListTexture(posX, posY, width, height, visibleCondition, image, borderImage, borderSize, aspectRatio, colorDiffuse, animations)
{
  m_info = content;
  m_type = LIST_IMAGE;
}

CGUIListItemLayout::CListImage::~CListImage()
{
}

CGUIListItemLayout::CGUIListItemLayout()
{
  m_width = 0;
  m_height = 0;
  m_condition = 0;
  m_focused = false;
  m_invalidated = true;
  m_isPlaying = false;
}

CGUIListItemLayout::CGUIListItemLayout(const CGUIListItemLayout &from)
{
  m_width = from.m_width;
  m_height = from.m_height;
  m_focused = from.m_focused;
  m_condition = from.m_condition;
  // copy across our controls
  for (ciControls it = from.m_controls.begin(); it != from.m_controls.end(); ++it)
  {
    CListBase *item = *it;
    if (item->m_type == CListBase::LIST_LABEL)
      m_controls.push_back(new CListLabel(*(CListLabel *)item));
    else if (item->m_type ==  CListBase::LIST_IMAGE)
      m_controls.push_back(new CListImage(*(CListImage *)item));
    else if (item->m_type ==  CListBase::LIST_TEXTURE)
      m_controls.push_back(new CListTexture(*(CListTexture *)item));
    else if (item->m_type == CListBase::LIST_SELECT_LABEL)
      m_controls.push_back(new CListSelectLabel(*(CListSelectLabel *)item));
  }
  m_invalidated = true;
  m_isPlaying = false;
}

CGUIListItemLayout::~CGUIListItemLayout()
{
  for (iControls it = m_controls.begin(); it != m_controls.end(); ++it)
    delete *it;
}

float CGUIListItemLayout::Size(ORIENTATION orientation) const
{
  return (orientation == HORIZONTAL) ? m_width : m_height;
}

bool CGUIListItemLayout::IsAnimating(ANIMATION_TYPE animType)
{
  for (iControls it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = *it;
    if (layoutItem->m_type == CListBase::LIST_LABEL)
    {
      CGUIListLabel &label = ((CListLabel *)layoutItem)->m_label;
      if (label.IsAnimating(animType))
        return true;
    }
    else if (layoutItem->m_type == CListBase::LIST_SELECT_LABEL)
    {
      if (((CListSelectLabel *)layoutItem)->m_label.IsAnimating(animType))
        return true;
    }
    else
    {
      if (((CListTexture *)layoutItem)->m_image.IsAnimating(animType))
        return true;
    }
  }
  return false;
}

void CGUIListItemLayout::ResetAnimation(ANIMATION_TYPE animType)
{
  for (iControls it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = *it;
    if (layoutItem->m_type == CListBase::LIST_LABEL)
    {
      CGUIListLabel &label = ((CListLabel *)layoutItem)->m_label;
      label.ResetAnimation(animType);
    }
    else if (layoutItem->m_type == CListBase::LIST_SELECT_LABEL)
    {
      ((CListSelectLabel *)layoutItem)->m_label.ResetAnimation(animType);
    }
    else
    {
      ((CListTexture *)layoutItem)->m_image.ResetAnimation(animType);
    }
  }
}

void CGUIListItemLayout::Render(CGUIListItem *item, DWORD parentID, DWORD time)
{
  if (m_invalidated)
  { // need to update our item
    // could use a dynamic cast here if RTTI was enabled.  As it's not,
    // let's use a static cast with a virtual base function
    CFileItem *fileItem = item->IsFileItem() ? (CFileItem *)item : new CFileItem(*item);
    Update(fileItem, parentID);
    // delete our temporary fileitem
    if (!item->IsFileItem())
      delete fileItem;
  }

  // and render
  for (iControls it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = *it;
    if (layoutItem->m_type == CListBase::LIST_LABEL)
    {
      CGUIListLabel &label = ((CListLabel *)layoutItem)->m_label;
      label.SetSelected(item->IsSelected() || m_isPlaying);
      label.SetScrolling(m_focused);
      label.UpdateVisibility(item);
      label.DoRender(time);
    }
    else if (layoutItem->m_type == CListBase::LIST_SELECT_LABEL)
    {
      ((CListSelectLabel *)layoutItem)->m_label.UpdateVisibility(item);
      ((CListSelectLabel *)layoutItem)->m_label.DoRender(time);
    }
    else
    {
      ((CListTexture *)layoutItem)->m_image.UpdateVisibility(item);
      ((CListTexture *)layoutItem)->m_image.DoRender(time);
    }
  }
}

void CGUIListItemLayout::Update(CFileItem *item, DWORD parentID)
{
  // check for boolean conditions
  m_isPlaying = g_infoManager.GetBool(LISTITEM_ISPLAYING, parentID, item);
  for (iControls it = m_controls.begin(); it != m_controls.end(); it++)
    UpdateItem(*it, item, parentID);
  // now we have to check our overlapping label pairs
  for (unsigned int i = 0; i < m_controls.size(); i++)
  {
    if (m_controls[i]->m_type == CListBase::LIST_LABEL && ((CListLabel *)m_controls[i])->m_label.IsVisible())
    {
      CGUIListLabel &label1 = ((CListLabel *)m_controls[i])->m_label;
      CRect rect1(label1.GetRenderRect());
      for (unsigned int j = i + 1; j < m_controls.size(); j++)
      {
        if (m_controls[j]->m_type == CListBase::LIST_LABEL && ((CListLabel *)m_controls[j])->m_label.IsVisible())
        { // ok, now check if they overlap
          CGUIListLabel &label2 = ((CListLabel *)m_controls[j])->m_label;
          if (!rect1.Intersect(label2.GetRenderRect()).IsEmpty())
          { // overlap vertically and horizontally - check alignment
            CGUIListLabel &left = label1.GetRenderRect().x1 < label2.GetRenderRect().x1 ? label1 : label2;
            CGUIListLabel &right = label1.GetRenderRect().x1 < label2.GetRenderRect().x1 ? label2 : label1;
            if ((left.GetLabelInfo().align & 3) == 0 && right.GetLabelInfo().align & XBFONT_RIGHT)
            {
              float chopPoint = (left.GetXPosition() + left.GetWidth() + right.GetXPosition() - right.GetWidth()) * 0.5f;
// [1       [2...[2  1].|..........1]         2]
// [1       [2.....[2   |      1]..1]         2]
// [1       [2..........|.[2   1]..1]         2]
              CRect leftRect(left.GetRenderRect());
              CRect rightRect(right.GetRenderRect());
              if (rightRect.x1 > chopPoint)
                chopPoint = rightRect.x1 - 5;
              else if (leftRect.x2 < chopPoint)
                chopPoint = leftRect.x2 + 5;
              leftRect.x2 = chopPoint - 5;
              rightRect.x1 = chopPoint + 5;
              left.SetRenderRect(leftRect);
              right.SetRenderRect(rightRect);
            }
          }
        }
      }
    }
  }
  m_invalidated = false;
}

void CGUIListItemLayout::UpdateItem(CGUIListItemLayout::CListBase *control, CFileItem *item, DWORD parentID)
{
  if (control->m_type == CListBase::LIST_IMAGE && item)
  {
    CListImage *image = (CListImage *)control;
    if (!image->m_info.IsConstant())
      image->m_image.SetFileName(image->m_info.GetItemLabel(item, true));
  }
  else if (control->m_type == CListBase::LIST_LABEL)
  {
    CListLabel *label = (CListLabel *)control;
    label->m_label.SetLabel(label->m_info.GetItemLabel(item));
  }
  else if (control->m_type == CListBase::LIST_SELECT_LABEL)
  {
    CListSelectLabel *label = (CListSelectLabel *)control;
    label->m_label.UpdateText(label->m_info.GetItemLabel(item));
  }
}

void CGUIListItemLayout::ResetScrolling()
{
  for (iControls it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = (*it);
    if (layoutItem->m_type == CListBase::LIST_LABEL)
      ((CListLabel *)layoutItem)->m_label.SetScrolling(false);
  }
}

void CGUIListItemLayout::SetFocus(unsigned int focus)
{
  for (iControls it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = (*it);
    if (layoutItem->m_type == CListBase::LIST_IMAGE ||
        layoutItem->m_type == CListBase::LIST_TEXTURE)
      ((CListTexture *)layoutItem)->m_image.SetFocus(focus > 0);
    else if (layoutItem->m_type == CListBase::LIST_LABEL)
      ((CListLabel *)layoutItem)->m_label.SetFocus(focus > 0);
    else if (layoutItem->m_type == CListBase::LIST_SELECT_LABEL)
      ((CListSelectLabel *)layoutItem)->m_label.SetFocusedItem(focus);
  }
}

unsigned int CGUIListItemLayout::GetFocus() const
{
  for (vector<CListBase*>::const_iterator it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = (*it);
    if (layoutItem->m_type == CListBase::LIST_SELECT_LABEL)
      return ((CListSelectLabel *)layoutItem)->m_label.GetFocusedItem();
  }
  return 0;
}

void CGUIListItemLayout::SelectItemFromPoint(const CPoint &point)
{
  for (vector<CListBase*>::const_iterator it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = (*it);
    if (layoutItem->m_type == CListBase::LIST_SELECT_LABEL)
      return ((CListSelectLabel *)layoutItem)->m_label.SelectItemFromPoint(point);
  }
}

bool CGUIListItemLayout::MoveLeft()
{
  for (vector<CListBase*>::const_iterator it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = (*it);
    if (layoutItem->m_type == CListBase::LIST_SELECT_LABEL)
      return ((CListSelectLabel *)layoutItem)->m_label.MoveLeft();
  }
  return false;
}

bool CGUIListItemLayout::MoveRight()
{
  for (vector<CListBase*>::const_iterator it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = (*it);
    if (layoutItem->m_type == CListBase::LIST_SELECT_LABEL)
      return ((CListSelectLabel *)layoutItem)->m_label.MoveRight();
  }
  return false;
}

CGUIListItemLayout::CListBase *CGUIListItemLayout::CreateItem(TiXmlElement *child)
{
  // resolve any <include> tag's in this control
  g_SkinInfo.ResolveIncludes(child);

  // grab the type...
  CStdString type = CGUIControlFactory::GetType(child);

  // resolve again with strType set so that <default> tags are added
  g_SkinInfo.ResolveIncludes(child, type);

  float posX = 0;
  float posY = 0;
  float width = 10;
  float height = 10;
  CStdString infoString;
  CImage image, borderImage;
  CImage imageFocus, imageNoFocus;
  FRECT borderSize = { 0, 0, 0, 0 };
  CLabelInfo label;
  CGUIControlFactory::GetFloat(child, "posx", posX);
  CGUIControlFactory::GetFloat(child, "posy", posY);
  CGUIControlFactory::GetFloat(child, "width", width);
  CGUIControlFactory::GetFloat(child, "height", height);
  XMLUtils::GetString(child, "info", infoString);
  CGUIControlFactory::GetInfoColor(child, "textcolor", label.textColor);
  CGUIControlFactory::GetInfoColor(child, "selectedcolor", label.selectedColor);
  CGUIControlFactory::GetInfoColor(child, "shadowcolor", label.shadowColor);
  CGUIControlFactory::GetInfoColor(child, "focusedcolor", label.focusedColor);
  CStdString fontName;
  XMLUtils::GetString(child, "font", fontName);
  label.font = g_fontManager.GetFont(fontName);
  CGUIControlFactory::GetTexture(child, "texture", image);
  // reset the info file for this image as we want to handle the updating
  // when items change, rather than every frame
  if (!image.file.IsConstant())
    image.file = CGUIInfoLabel("");
  // and get the <info> tag for this as well if available (GetTexture doesn't handle <info>)
  CGUIInfoLabel infoTexture;
  CGUIControlFactory::GetInfoLabel(child, "texture", infoTexture);
  CGUIControlFactory::GetTexture(child, "texturefocus", imageFocus);
  CGUIControlFactory::GetTexture(child, "texturenofocus", imageNoFocus);
  CGUIControlFactory::GetAlignment(child, "align", label.align);
  FRECT rect = { posX, posY, width, height };
  vector<CAnimation> animations;
  CGUIControlFactory::GetAnimations(child, rect, animations);
  CGUIInfoColor colorDiffuse(0xffffffff);
  CGUIControlFactory::GetInfoColor(child, "colordiffuse", colorDiffuse);
  DWORD alignY = 0;
  if (CGUIControlFactory::GetAlignmentY(child, "aligny", alignY))
    label.align |= alignY;
  CGUIInfoLabel infoLabel;
  CGUIControlFactory::GetInfoLabel(child, "label", infoLabel);
  CGUIImage::CAspectRatio aspectRatio(CGUIImage::CAspectRatio::AR_KEEP);
  CGUIControlFactory::GetAspectRatio(child, "aspectratio", aspectRatio);
  int visibleCondition = 0;
  CGUIControlFactory::GetConditionalVisibility(child, visibleCondition);
  XMLUtils::GetFloat(child, "angle", label.angle); label.angle *= -1;
  bool scroll(false);
  XMLUtils::GetBoolean(child, "scroll", scroll);
  CGUIControlFactory::GetTexture(child, "bordertexture", borderImage);
  CStdString borderStr;
  if (XMLUtils::GetString(child, "bordersize", borderStr))
    CGUIControlFactory::GetRectFromString(borderStr, borderSize);
  if (type == "label")
  { // info label
    return new CListLabel(posX, posY, width, height, visibleCondition, label, scroll, infoLabel, animations);
  }
  else if (type == "multiselect")
  {
    return new CListSelectLabel(posX, posY, width, height, visibleCondition, imageFocus, imageNoFocus, label, infoLabel, animations);
  }
  else if (type == "image")
  {
    if (!infoTexture.IsConstant())
      return new CListImage(posX, posY, width, height, visibleCondition, image, borderImage, borderSize, aspectRatio, colorDiffuse, infoTexture, animations);
    else
    {
      // Due to old behaviour, we force aspectratio to stretch here
      aspectRatio.ratio = CGUIImage::CAspectRatio::AR_STRETCH;
      return new CListTexture(posX, posY, width, height, visibleCondition, image, borderImage, borderSize, aspectRatio, colorDiffuse, animations);
    }
  }
  return NULL;
}

void CGUIListItemLayout::LoadLayout(TiXmlElement *layout, bool focused)
{
  m_focused = focused;
  g_SkinInfo.ResolveIncludes(layout);
  g_SkinInfo.ResolveConstant(layout->Attribute("width"), m_width);
  g_SkinInfo.ResolveConstant(layout->Attribute("height"), m_height);
  const char *condition = layout->Attribute("condition");
  if (condition)
    m_condition = g_infoManager.TranslateString(condition);
  TiXmlElement *child = layout->FirstChildElement("control");
  while (child)
  {
    CListBase *item = CreateItem(child);
    if (item)
      m_controls.push_back(item);
    child = child->NextSiblingElement("control");
  }
}

//#ifdef PRE_SKIN_VERSION_2_1_COMPATIBILITY
void CGUIListItemLayout::CreateListControlLayouts(float width, float height, bool focused, const CLabelInfo &labelInfo, const CLabelInfo &labelInfo2, const CImage &texture, const CImage &textureFocus, float texHeight, float iconWidth, float iconHeight, int nofocusCondition, int focusCondition)
{
  m_width = width;
  m_height = height;
  m_focused = focused;
  vector<CAnimation> blankAnims;
  CImage border;
  FRECT borderRect = { 0, 0, 0, 0 };
  CListTexture *tex = new CListTexture(0, 0, width, texHeight, nofocusCondition, texture, border, borderRect, CGUIImage::CAspectRatio(), 0xffffffff, blankAnims);
  m_controls.push_back(tex);
  if (focused)
  {
    CListTexture *tex = new CListTexture(0, 0, width, texHeight, focusCondition, textureFocus, border, borderRect, CGUIImage::CAspectRatio(), 0xffffffff, blankAnims);
    m_controls.push_back(tex);
  }
  CListImage *image = new CListImage(8, 0, iconWidth, texHeight, 0, CImage(""), border, borderRect, CGUIImage::CAspectRatio(CGUIImage::CAspectRatio::AR_KEEP), 0xffffffff, CGUIInfoLabel("$INFO[ListItem.Icon]"), blankAnims);
  m_controls.push_back(image);
  float x = iconWidth + labelInfo.offsetX + 10;
  CListLabel *label = new CListLabel(x, labelInfo.offsetY, width - x - 18, height, 0, labelInfo, false, CGUIInfoLabel("$INFO[ListItem.Label]"), blankAnims);
  m_controls.push_back(label);
  x = labelInfo2.offsetX ? labelInfo2.offsetX : m_width - 16;
  label = new CListLabel(x, labelInfo2.offsetY, x - iconWidth - 20, height, 0, labelInfo2, false, CGUIInfoLabel("$INFO[ListItem.Label2]"), blankAnims);
  m_controls.push_back(label);
}

void CGUIListItemLayout::CreateThumbnailPanelLayouts(float width, float height, bool focused, const CImage &image, float texWidth, float texHeight, float thumbPosX, float thumbPosY, float thumbWidth, float thumbHeight, DWORD thumbAlign, const CGUIImage::CAspectRatio &thumbAspect, const CLabelInfo &labelInfo, bool hideLabels)
{
  m_width = width;
  m_height = height;
  m_focused = focused;
  float centeredPosX = (m_width - texWidth)*0.5f;
  CImage border;
  FRECT borderRect = { 0, 0, 0, 0 };
  // background texture
  vector<CAnimation> blankAnims;
  CListTexture *tex = new CListTexture(centeredPosX, 0, texWidth, texHeight, 0, image, border, borderRect, CGUIImage::CAspectRatio(), 0xffffffff, blankAnims);
  m_controls.push_back(tex);
  // thumbnail
  float xOff = 0;
  float yOff = 0;
  if (thumbAlign != 0)
  {
    xOff += (texWidth - thumbWidth) * 0.5f;
    yOff += (texHeight - thumbHeight) * 0.5f;
    //if thumbPosX or thumbPosX != 0 the thumb will be bumped off-center
  }
  CListImage *thumb = new CListImage(thumbPosX + centeredPosX + xOff, thumbPosY + yOff, thumbWidth, thumbHeight, 0, CImage(""), border, borderRect, thumbAspect, 0xffffffff, CGUIInfoLabel("$INFO[ListItem.Icon]"), blankAnims);
  m_controls.push_back(thumb);
  // overlay
  CListImage *overlay = new CListImage(thumbPosX + centeredPosX + xOff + thumbWidth - 32, thumbPosY + yOff + thumbHeight - 32, 32, 32, 0, CImage(""), border, borderRect, thumbAspect, 0xffffffff, CGUIInfoLabel("$INFO[ListItem.Overlay]"), blankAnims);
  m_controls.push_back(overlay);
  // label
  if (hideLabels) return;
  CListLabel *label = new CListLabel(width*0.5f, texHeight, width, height, 0, labelInfo, false, CGUIInfoLabel("$INFO[ListItem.Label]"), blankAnims);
  m_controls.push_back(label);
}
//#endif

#ifdef _DEBUG
void CGUIListItemLayout::DumpTextureUse()
{
  for (iControls it = m_controls.begin(); it != m_controls.end(); it++)
  {
    CListBase *layoutItem = (*it);
    if (layoutItem->m_type == CListBase::LIST_IMAGE || layoutItem->m_type == CListBase::LIST_TEXTURE)
      ((CListTexture *)layoutItem)->m_image.DumpTextureUse();
  }
}
#endif
