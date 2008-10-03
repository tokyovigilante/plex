/*
* XBMC Media Center
* Shader Classes
* Copyright (c) 2007 d4rk
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include "stdafx.h"
#include "include.h"
#include "../RenderFlags.h"
#include "YUV2RGBShader.h"
#include "Settings.h"
#include <string>
#include <sstream>

#ifdef HAS_SDL_OPENGL

using namespace Shaders;
using namespace std;

//
// Transformation matrixes for different colorspaces.
//
static float yuv_coef_bt601[4][4] = 
{
    { 1.0f,      1.0f,     1.0f,     0.0f },
    { 0.0f,     -0.344f,   1.773f,   0.0f },
    { 1.403f,   -0.714f,   0.0f,     0.0f },
    { 0.0f,      0.0f,     0.0f,     0.0f } 
};

static float yuv_coef_bt709[4][4] =
{
    { 1.0f,      1.0f,     1.0f,     0.0f },
    { 0.0f,     -0.1870f,  1.8556f,  0.0f },
    { 1.5701f,  -0.4664f,  0.0f,     0.0f },
    { 0.0f,      0.0f,     0.0f,     0.0f }
};

static float yuv_coef_ebu[4][4] = 
{
    { 1.0f,      1.0f,     1.0f,     0.0f },
    { 0.0f,     -0.3960f,  2.029f,   0.0f },
    { 1.140f,   -0.581f,   0.0f,     0.0f },
    { 0.0f,      0.0f,     0.0f,     0.0f }
};

static float yuv_coef_smtp240m[4][4] =
{
    { 1.0f,      1.0f,     1.0f,     0.0f },
    { 0.0f,     -0.2253f,  1.8270f,  0.0f },
    { 1.5756f,  -0.5000f,  0.0f,     0.0f },
    { 0.0f,      0.0f,     0.0f,     0.0f }
};

static float** PickYUVConversionMatrix(unsigned flags)
{
  // Pick the matrix.
   float (*matrix)[4];
   switch(CONF_FLAGS_YUVCOEF_MASK(flags))
   {
     case CONF_FLAGS_YUVCOEF_240M:
       matrix = yuv_coef_smtp240m; break;
     case CONF_FLAGS_YUVCOEF_BT709:
       matrix = yuv_coef_bt709; break;
     case CONF_FLAGS_YUVCOEF_BT601:    
       matrix = yuv_coef_bt601; break;
     case CONF_FLAGS_YUVCOEF_EBU:
       matrix = yuv_coef_ebu; break;
     default:
       matrix = yuv_coef_bt601; break;
   }
   
   return (float**)matrix;
}

//////////////////////////////////////////////////////////////////////
// BaseYUV2RGBGLSLShader - base class for GLSL YUV2RGB shaders
//////////////////////////////////////////////////////////////////////

BaseYUV2RGBGLSLShader::BaseYUV2RGBGLSLShader(unsigned flags)
{
  m_width      = 1;
  m_height     = 1;
  m_stepX      = 0;
  m_stepY      = 0;
  m_field      = 0;
  m_yTexUnit   = 0;
  m_uTexUnit   = 0;
  m_vTexUnit   = 0;
  m_flags      = flags;

  // shader attribute handles
  m_hYTex  = -1;
  m_hUTex  = -1;
  m_hVTex  = -1;
  m_hStepX = -1;
  m_hStepY = -1;
  m_hField = -1;

  // default passthrough vertex shader
  string shaderv = 
    "void main()"
    "{"
    "gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;"
    "gl_TexCoord[1] = gl_TextureMatrix[1] * gl_MultiTexCoord1;"
    "gl_TexCoord[2] = gl_TextureMatrix[2] * gl_MultiTexCoord2;"
    "gl_TexCoord[3] = gl_TextureMatrix[3] * gl_MultiTexCoord3;"
    "gl_Position = ftransform();"
    "}";
  SetVertexShaderSource(shaderv);
}

string BaseYUV2RGBGLSLShader::BuildYUVMatrix()
{
  // Pick the matrix.
  float (*matrix)[4] = (float (*)[4])PickYUVConversionMatrix(m_flags);
  
  // Convert to GLSL language.
  stringstream strStream;
  strStream << "mat4( \n";
  for (int x=0; x<4; x++)
  {
    strStream << "vec4(";
    
    for (int y=0; y<4; y++)
    {
      strStream << matrix[x][y];
      if (y != 3)
        strStream << ", ";
    }
    
    strStream << ")";
    if (x != 3)
      strStream << ", ";
    strStream << " \n";
  }
  
  strStream << "); \n";
  return strStream.str();
}


//////////////////////////////////////////////////////////////////////
// BaseYUV2RGBGLSLShader - base class for GLSL YUV2RGB shaders
//////////////////////////////////////////////////////////////////////

BaseYUV2RGBARBShader::BaseYUV2RGBARBShader(unsigned flags)
{
  m_width         = 1;
  m_height        = 1;
  m_stepX         = 0;
  m_stepY         = 0;
  m_field         = 0;
  m_yTexUnit      = 0;
  m_uTexUnit      = 0;
  m_vTexUnit      = 0;
  m_flags         = flags;

  // shader attribute handles
  m_hYTex  = -1;
  m_hUTex  = -1;
  m_hVTex  = -1;
  m_hStepX = -1;
  m_hStepY = -1;
  m_hField = -1;
}

//////////////////////////////////////////////////////////////////////
// YUV2RGBProgressiveShader - YUV2RGB with no deinterlacing
// Use for weave deinterlacing / progressive
//////////////////////////////////////////////////////////////////////

YUV2RGBProgressiveShader::YUV2RGBProgressiveShader(bool rect, unsigned flags)
  : BaseYUV2RGBGLSLShader(flags)
{
  string shaderf;
  
  if (rect) 
  {
    shaderf += "#extension GL_ARB_texture_rectangle : enable\n"
               "#define texture2D texture2DRect\n"
               "#define sampler2D sampler2DRect\n";
  }

  // ati breaks down with sampler2DRect, but it seem to work without it
  if(rect && g_advancedSettings.m_GLRectangleHack)
  {
    shaderf += "#define idY 2\n";
    shaderf += "#define idU 0\n";
    shaderf += "#define idV 1\n";
  }
  else
  {
    shaderf += "#define idY 0\n";
    shaderf += "#define idU 1\n";
    shaderf += "#define idV 2\n";
  }

  shaderf += 
    "uniform sampler2D ytex;\n"
    "uniform sampler2D utex;\n"
    "uniform sampler2D vtex;\n"
    "void main()\n"
    "{\n"
    "vec4 yuv, rgb;\n"
    "mat4 yuvmat = " + BuildYUVMatrix();

  if (!(flags & CONF_FLAGS_YUV_FULLRANGE))
  {
    shaderf +=
      "yuv.rgba = vec4(((texture2D(ytex, gl_TexCoord[idY].xy).r - 16.0/256.0) * 1.164383562),\n"
      "                ((texture2D(utex, gl_TexCoord[idU].xy).r - 16.0/256.0) * 1.138392857 - 0.5),\n"
      "                ((texture2D(vtex, gl_TexCoord[idV].xy).r - 16.0/256.0) * 1.138392857 - 0.5),\n"
      "                0.0);\n";
  }
  else
  {
    shaderf +=
      "yuv.rgba = vec4((texture2D(ytex, gl_TexCoord[idY].xy).r),\n"
      "                (texture2D(utex, gl_TexCoord[idU].xy).r - 0.5),\n"
      "                (texture2D(vtex, gl_TexCoord[idV].xy).r - 0.5),\n"
      "                0.0);\n";
  }
  shaderf +=
    "rgb = yuvmat * yuv;\n"
    "rgb.a = 1.0;\n"
    "gl_FragColor = rgb;\n"
    "}";
  SetPixelShaderSource(shaderf);
}

void YUV2RGBProgressiveShader::OnCompiledAndLinked()
{
  // obtain shader attribute handles on successfull compilation
  m_hYTex = glGetUniformLocation(ProgramHandle(), "ytex");
  m_hUTex = glGetUniformLocation(ProgramHandle(), "utex");
  m_hVTex = glGetUniformLocation(ProgramHandle(), "vtex");
  VerifyGLState();
}

bool YUV2RGBProgressiveShader::OnEnabled()
{
  // set shader attributes once enabled
  glUniform1i(m_hYTex, m_yTexUnit);
  glUniform1i(m_hUTex, m_uTexUnit);
  glUniform1i(m_hVTex, m_vTexUnit);
  VerifyGLState();
  return true;
}


//////////////////////////////////////////////////////////////////////
// YUV2RGBBobShader - YUV2RGB with Bob deinterlacing
//////////////////////////////////////////////////////////////////////

YUV2RGBBobShader::YUV2RGBBobShader(bool rect, unsigned flags)
  : BaseYUV2RGBGLSLShader(flags)
{
  string shaderf;
  if (rect) 
  {
    shaderf += "#extension GL_ARB_texture_rectangle : enable\n"
               "#define texture2D texture2DRect\n"
               "#define sampler2D sampler2DRect\n";
  }

  // ati breaks down with sampler2DRect, but it seem to work without it
  if(rect && g_advancedSettings.m_GLRectangleHack)
  {
    shaderf += "#define idY 2\n";
    shaderf += "#define idU 0\n";
    shaderf += "#define idV 1\n";
  }
  else
  {
    shaderf += "#define idY 0\n";
    shaderf += "#define idU 1\n";
    shaderf += "#define idV 2\n";
  }


  shaderf += 
    "uniform sampler2D ytex;"
    "uniform sampler2D utex;"
    "uniform sampler2D vtex;"
    "uniform float stepX, stepY;"
    "uniform int field;"
    "void main()"
    "{"
    "vec4 yuv, rgb;"
    "vec2 offsetY, offsetUV;"
    "float temp1 = mod(gl_TexCoord[idY].y, 2*stepY);"

    "offsetY  = gl_TexCoord[idY].xy ;"
    "offsetUV = gl_TexCoord[idU].xy ;"
    "offsetY.y  -= (temp1 - stepY/2 + float(field)*stepY);"
    "offsetUV.y -= (temp1 - stepY/2 + float(field)*stepY)/2;"
    "mat4 yuvmat = " + BuildYUVMatrix() +
    "yuv = vec4(texture2D(ytex, offsetY).r,"
    "           texture2D(utex, offsetUV).r,"
    "           texture2D(vtex, offsetUV).r,"
    "           0.0);";
    if (!(flags & CONF_FLAGS_YUV_FULLRANGE))
    {
      shaderf +=
        "yuv.rgba = vec4( (yuv.r - 16.0/256.0) * 1.164383562"
        "               , (yuv.g - 16.0/256.0) * 1.138392857 - 0.5"
        "               , (yuv.b - 16.0/256.0) * 1.138392857 - 0.5"
        "               , 0);";
    }
    else
    {
      shaderf +=
        "yuv.rgba = vec4( yuv.r"
        "               , yuv.g - 0.5"
        "               , yuv.b - 0.5"
        "               , 0);";
    }
    shaderf +=
      "rgb = yuvmat * yuv;"
      "rgb.a = 1.0;"
      "gl_FragColor = rgb;"
      "}";
  SetPixelShaderSource(shaderf);  
}

void YUV2RGBBobShader::OnCompiledAndLinked()
{
  // obtain shader attribute handles on successfull compilation
  m_hYTex = glGetUniformLocation(ProgramHandle(), "ytex");
  m_hUTex = glGetUniformLocation(ProgramHandle(), "utex");
  m_hVTex = glGetUniformLocation(ProgramHandle(), "vtex");
  m_hStepX = glGetUniformLocation(ProgramHandle(), "stepX");
  m_hStepY = glGetUniformLocation(ProgramHandle(), "stepY");
  m_hField = glGetUniformLocation(ProgramHandle(), "field");
  VerifyGLState();
}

bool YUV2RGBBobShader::OnEnabled()
{
  // set shader attributes once enabled
  glUniform1i(m_hYTex, m_yTexUnit);
  glUniform1i(m_hUTex, m_uTexUnit);
  glUniform1i(m_hVTex, m_vTexUnit);
  glUniform1i(m_hField, m_field);
  glUniform1f(m_hStepX, 1.0f / (float)m_width);
  glUniform1f(m_hStepY, 1.0f / (float)m_height);
  VerifyGLState();
  return true;
}

string BaseYUV2RGBARBShader::BuildYUVMatrix()
{
  // Pick the matrix.
  float (*matrix)[4] = (float (*)[4])PickYUVConversionMatrix(m_flags);
  
  // Convert to ARB matrix. The forth vector is needed because the generated code
  // uses negation on a vector, and also negation on an element of the vector, so
  // I needed to add another "pre-negated" vector in.
  //
  stringstream strStream;
  strStream << "{ ";
  strStream << "  {     1.0,                   -0.0625,             1.1643835,               1.1383928        },\n";
  strStream << "  {    -0.5,                    0.0, "          <<  matrix[1][1] << ", " <<  matrix[1][2] << "},\n";
  strStream << "  {" << matrix[2][0] << ", " << matrix[2][1] << ",  0.0,                     0.0              },\n"; 
  strStream << "  {    -0.5,                    0.0, "          << -matrix[1][1] << ", " << -matrix[1][2] << "}\n";
  strStream << "};\n";

  return strStream.str();
}


//////////////////////////////////////////////////////////////////////
// YUV2RGBProgressiveShaderARB - YUV2RGB with no deinterlacing
//////////////////////////////////////////////////////////////////////

YUV2RGBProgressiveShaderARB::YUV2RGBProgressiveShaderARB(bool rect, unsigned flags)
  : BaseYUV2RGBARBShader(flags)
{
  string source = "";
  string target = "2D";
  if (rect)
  {
    target = "RECT";
  }
  
  // N.B. If you're changing this code, bear in mind that the GMA X3100 
  // (at least with OS X drivers in 10.5.2), doesn't allow for negation 
  // of constants, like "-c[0].y".
  //
  // Thanks to Aras Pranckevicius for bringing this bug to light.
  // Quoting him, in case the link dies:
  // "In my experience, the X3100 drivers seem to ignore negate modifiers on
  // constant registers in fragment (and possibly vertex) programs. It just
  // seems someone forgot to implement that. (radar: 5632811)"
  // - http://lists.apple.com/archives/mac-opengl/2008/Feb/msg00191.html

  if (flags & CONF_FLAGS_YUV_FULLRANGE)
  {
    source ="!!ARBfp1.0\n"
      "PARAM c[2] = { { 0,           -0.1720674,  0.88599992, -1 },\n"
      "		             { 0.70099545,  -0.35706902, 0,           2 } };\n"
      "TEMP R0;\n"
      "TEMP R1;\n"
      "TEX R1.x, fragment.texcoord[2], texture[2], "+target+";\n"
      "TEX R0.x, fragment.texcoord[1], texture[1], "+target+";\n"
      "MUL R0.z, R0.x, c[1].w;\n"
      "MUL R0.y, R1.x, c[1].w;\n"
      "TEX R0.x, fragment.texcoord[0], texture[0], "+target+";\n"
      "ADD R0.z, R0, c[0].w;\n"
      "MAD R1.xyz, R0.z, c[0], R0.x;\n"
      "ADD R0.x, R0.y, c[0].w;\n"
      "MAD result.color.xyz, R0.x, c[1], R1;\n"
      "MOV result.color.w, c[0];\n"
      "END\n";
  }
  else
  {
    source = 
      "!!ARBfp1.0\n"
      "PARAM c[4] = \n" + BuildYUVMatrix() +
      "TEMP R0;\n"
      "TEMP R1;\n"
      "TEX R1.x, fragment.texcoord[1], texture[1], "+target+"\n;"
      "ADD R0.z, R1.x, c[0].y;\n"
      "TEX R0.x, fragment.texcoord[2], texture[2], "+target+"\n;"
      "ADD R0.x, R0, c[0].y;\n"
      "MUL R0.y, R0.x, c[0].w;\n"
      "TEX R0.x, fragment.texcoord[0], texture[0], "+target+"\n;"
      "ADD R0.x, R0, c[0].y;\n"
      "MUL R0.z, R0, c[0].w;\n"
      "MUL R0.x, R0, c[0].z;\n"
      "ADD R0.z, R0, c[1].x;\n"
      "MAD R1.xyz, R0.z, c[1].yzww, R0.x;\n"
      "ADD R0.x, R0.y, c[3];\n"
      "MAD result.color.xyz, R0.x, c[2], R1;\n"
      "MOV result.color.w, c[0].x;\n"
      "END\n";
  }
  SetPixelShaderSource(source);
}

void YUV2RGBProgressiveShaderARB::OnCompiledAndLinked()
{

}

bool YUV2RGBProgressiveShaderARB::OnEnabled()
{
  return true;
}


#endif // HAS_SDL_OPENGL
