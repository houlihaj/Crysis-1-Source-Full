/*=============================================================================
  Parser.cpp : Script parser implementations.
  Copyright (c) 2001-2007 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"

const char *kWhiteSpace = " ,";
char *pCurCommand;

void SkipCharacters (char** buf, const char *toSkip)
{
  char theChar;
  char *skip;

  while ((theChar = **buf) != 0)
  {
    if (theChar >= 0x20) 
    {
      skip = (char *) toSkip;
      while (*skip)
      {
        if (theChar == *skip)
          break;
        ++skip;
      }
      if (*skip == 0)           
        return;                 
    }
    ++*buf;                     
  }
}

void RemoveCR(char *pbuf)
{
  while (*pbuf)
  {
    if (*pbuf == 0xd)
      *pbuf = 0x20;
    pbuf++;
  }
}

FXMacro sStaticMacros;
byte sSkipChars[256];
std::vector<string> g_EnvVars;

// Determine is this preprocessor directive belongs to first pass or second one
bool fxIsFirstPass(char *buf)
{
  char com[1024];
  char tok[256];
  fxFillCR(&buf, com);
  char *s = com;
  while (*s != 0)
  {
    char *pStart = fxFillPr(&s, tok);
    if (tok[0] == '%' && tok[1] == '_')
      return false;
  }
  return true;
}

static void fxAddMacro(char *Name, char *Macro, FXMacro& Macros)
{
  SMacroFX pr;

  if (Name[0] == '%')
  {
    pr.m_nMask = shGetHex(&Macro[0]);
#ifdef _DEBUG
    FXMacroItor it = Macros.find(Name);
    if (it != Macros.end())
      assert(0);
#endif
  }
  else
    pr.m_nMask = 0;
  pr.m_szMacro = Macro ? Macro : "";
  FXMacroItor it = Macros.find(CONST_TEMP_STRING(Name));
  if (it != Macros.end())
    Macros.erase(Name);
  Macros.insert(FXMacroItor::value_type(Name, pr));

  /*it = Macros.find(Name);
  if (it != Macros.end())
  {
    int nnn = 0;
  }*/
}

void fxParserInit(void)
{
#if defined (DIRECT3D8) || defined (DIRECT3D9) || defined (DIRECT3D10)
  fxAddMacro("D3D", NULL, sStaticMacros);
  fxAddMacro("DIRECT3D", NULL, sStaticMacros);
 #if defined (DIRECT3D8)
  fxAddMacro("DIRECT3D8", NULL, sStaticMacros);
  fxAddMacro("D3D8", NULL, sStaticMacros);
 #elif defined (DIRECT3D9) || defined(PS3)
  fxAddMacro("DIRECT3D9", NULL, sStaticMacros);
  fxAddMacro("D3D9", NULL, sStaticMacros);
 #elif defined (DIRECT3D10)
  fxAddMacro("DIRECT3D10", NULL, sStaticMacros);
  fxAddMacro("D3D10", NULL, sStaticMacros);
 #endif
#endif
#if defined (OPENGL)
  fxAddMacro("OGL", NULL, sStaticMacros);
  fxAddMacro("OPENGL", NULL, sStaticMacros);
#endif
#if defined (XBOX)
  fxAddMacro("XBOX", NULL, sStaticMacros);
#endif
#if defined (XENON)
  fxAddMacro("XENON", NULL, sStaticMacros);
#endif
#if defined (PS3)
  fxAddMacro("PS3", NULL, sStaticMacros);
#endif
#if defined (LINUX)
	fxAddMacro("LINUX", NULL, sStaticMacros);
#endif
  if (gRenDev->GetFeatures() & RFT_DEPTHMAPS)
    fxAddMacro("DEPTHMAP", NULL, sStaticMacros);
  if (gRenDev->GetFeatures() & RFT_HW_ENVBUMPPROJECTED)
    fxAddMacro("PROJECTEDENVBUMP", NULL, sStaticMacros);
  int nGPU = gRenDev->GetFeatures() & RFT_HW_MASK;
  switch(nGPU)
  {
    case RFT_HW_GF2:
      fxAddMacro("NV1X", NULL, sStaticMacros);
    	break;
    case RFT_HW_GF3:
      fxAddMacro("NV2X", NULL, sStaticMacros);
  	  break;
    case RFT_HW_GFFX:
      fxAddMacro("NV3X", NULL, sStaticMacros);
  	  break;
    case RFT_HW_NV4X:
      fxAddMacro("NV4X", NULL, sStaticMacros);
  	  break;
    case RFT_HW_ATI:
      fxAddMacro("R3XX", NULL, sStaticMacros);
      fxAddMacro("ATI", NULL, sStaticMacros);
      fxAddMacro("RADEON", NULL, sStaticMacros);
  	  break;
    default:
      assert(0);
  }
  if (gRenDev->GetFeatures()&RFT_HW_HDR)
    fxAddMacro("HDR", NULL, sStaticMacros);

  bool bPS20 = ((gRenDev->GetFeatures() & (RFT_HW_PS2X | RFT_HW_PS30)) == 0) || (gRenDev->m_RP.m_nMaxLightsPerPass < 4);
  if (bPS20)
    fxAddMacro("PS20Only", NULL, sStaticMacros);
  if (gRenDev->GetFeatures()&RFT_HW_PS20 && gRenDev->GetFeatures()&RFT_HW_PS2X && !(gRenDev->GetFeatures()&RFT_HW_PS30))
    fxAddMacro("PS2X", NULL, sStaticMacros);
  if (gRenDev->GetFeatures() & RFT_HW_PS30)
    fxAddMacro("PS30", NULL, sStaticMacros);  
  char *VPSup = gRenDev->GetVertexProfile(true);
  char *PPSup = gRenDev->GetPixelProfile(true);
  fxAddMacro(VPSup, NULL, sStaticMacros);
  fxAddMacro(PPSup, NULL, sStaticMacros);

  int i;
  for (i=0; i<256; i++)
  {
    if (i <= 0x20
     || i == '('
     || i == ')'
     || i == '{'
     || i == '}'
     || i == '!'
     || i == '*'
     || i == '-'
     || i == '+'
     || i == '<'
     || i == '>'
     || i == '['
     || i == ']'
     || i == '|'
     || i == '&'
     || i == '='
     || i == ';'
     || i == ':'
     || i == '\''
     || i == '.'
     || i == '?'
     || i == '\"'
     || i == '/'
     || i == ',')
      sSkipChars[i] = 1;
    else
      sSkipChars[i] = 0;
  }
}

void fxRegisterEnv(const char *szStr)
{
  g_EnvVars.push_back(szStr);
}

char *fxFillPr (char **buf, char *dst)
{
  int n = 0;
  char ch;
  while ((ch=**buf) != 0)
  {
    if (sSkipChars[ch] == 0)
      break;
    ++*buf;
  }
  char *pStart = *buf;
  while ((ch=**buf) != 0)
  {
    if (sSkipChars[ch] == 1)
      break;
    dst[n++] = ch;
    ++*buf;
  }
  dst[n] = 0;
  return pStart;
}

char *fxFillPrC (char **buf, char *dst)
{
  int n = 0;
  char ch;
  while ((ch=**buf) != 0)
  {
    if (sSkipChars[ch] == 0)
      break;
    ++*buf;
  }
  char *pStart = *buf;
  while ((ch=**buf) != 0)
  {
    if (ch != ',' && sSkipChars[ch] == 1)
      break;
    dst[n++] = ch;
    ++*buf;
  }
  dst[n] = 0;
  return pStart;
}

char *fxFillParam (char **buf, char *dst)
{
  int n = 0;
  char ch;
  while ((ch=**buf) != 0)
  {
    if (sSkipChars[ch] == 0)
      break;
    ++*buf;
  }
  char *pStart = *buf;
  while ((ch=**buf) != 0)
  {
    if (ch != '.' && sSkipChars[ch] == 1)
      break;
    dst[n++] = ch;
    ++*buf;
  }
  dst[n] = 0;
  return pStart;
}

int shFill(char **buf, char *dst, int nSize)
{
  int n = 0;
  SkipCharacters (buf, kWhiteSpace);

  while (**buf > 0x20)
  {
    dst[n++] = **buf;
    ++*buf;

    if(nSize > 0 && n == nSize)
    {
      break;
    }
  }

  dst[n] = 0;
  return n;
}
int fxFill(char **buf, char *dst, int nSize)
{
  int n = 0;
  SkipCharacters (buf, kWhiteSpace);

  while (**buf != ';')
  {
    if (**buf == 0)
      break;
    dst[n++] = **buf;
    ++*buf;

    if(nSize > 0 && n == nSize)
    {
      dst[n-1] = 0;
      return 1;
    }
  }

  dst[n] = 0;
  if (**buf == ';')
    ++*buf;
  return n;
}

int fxFillCR(char **buf, char *dst)
{
  int n = 0;
  SkipCharacters (buf, kWhiteSpace);
  while (**buf != 0xa)
  {
    if (**buf == 0)
      break;
    dst[n++] = **buf;
    ++*buf;
  }
  dst[n] = 0;
  return n;
}

//================================================================================

bool shGetBool(char *buf)
{
  if (!buf)
    return false;

  if ( !strnicmp(buf, "yes", 3) )
    return true;

  if ( !strnicmp(buf, "true", 4) )
    return true;

  if ( !strnicmp(buf, "on", 2) )
    return true;

  if ( !strncmp(buf, "1", 1) )
    return true;

  return false;
}

float shGetFloat(const char *buf)
{
  if (!buf)
    return 0;
  float f = 0;

  sscanf(buf, "%f", &f);

  return f;
}

void shGetFloat(const char *buf, float *v1, float *v2)
{
  if (!buf)
    return;
  float f=0, f1=0;

  int n = sscanf(buf, "%f %f", &f, &f1);
  if (n == 1)
  {
    *v1 = f;
    *v2 = f;
  }
  else
  {
    *v1 = f;
    *v2 = f1;
  }
}

int shGetInt(const char *buf)
{
  if (!buf)
    return 0;
  int i = 0;

  if (buf[0] == '0' && buf[1] == 'x')
    sscanf(&buf[2], "%x", &i);
  else
    sscanf(buf, "%i", &i);

  return i;
}

int shGetHex(const char *buf)
{
  if (!buf)
    return 0;
  int i = 0;

  sscanf(buf, "%x", &i);

  return i;
}

uint64 shGetHex64(const char *buf)
{
  if (!buf)
    return 0;
#if defined(__GNUC__)
  unsigned long long i = 0;
  sscanf(buf, "%llx", &i);
	return (uint64)i;
#else
  uint64 i = 0;
  sscanf(buf, "%I64x", &i);
  return i;
#endif
}

void shGetVector(char *buf, Vec3& v)
{
  if (!buf)
    return;
  sscanf(buf, "%f %f %f", &v[0], &v[1], &v[2]);
}

void shGetVector(char *buf, float v[3])
{
  if (!buf)
    return;
  sscanf(buf, "%f %f %f", &v[0], &v[1], &v[2]);
}

void shGetVector4(char *buf, Vec4& v)
{
  if (!buf)
    return;
  sscanf(buf, "%f %f %f %f", &v.x, &v.y, &v.z, &v.w);
}

static struct SColAsc
{
  char *nam;
  ColorF col;

  SColAsc(char *name, const ColorF& c)
  {
    nam = name;
    col = c;
  }
} sCols[] =
{
  SColAsc("Aquamarine", Col_Aquamarine),
  SColAsc("Black", Col_Black),
  SColAsc("Blue", Col_Blue),
  SColAsc("BlueViolet", Col_BlueViolet),
  SColAsc("Brown", Col_Brown),
  SColAsc("CadetBlue", Col_CadetBlue),
  SColAsc("Coral", Col_Coral),
  SColAsc("CornflowerBlue", Col_CornflowerBlue),
  SColAsc("Cyan", Col_Cyan),
  SColAsc("DarkGreen", Col_DarkGreen),
  SColAsc("DarkOliveGreen", Col_DarkOliveGreen),
  SColAsc("DarkOrchild", Col_DarkOrchild),
  SColAsc("DarkSlateBlue", Col_DarkSlateBlue),
  SColAsc("DarkSlateGray", Col_DarkSlateGray),
  SColAsc("DarkSlateGrey", Col_DarkSlateGrey),
  SColAsc("DarkTurquoise", Col_DarkTurquoise),
  SColAsc("DarkWood", Col_DarkWood),
  SColAsc("DimGray", Col_DimGray),
  SColAsc("DimGrey", Col_DimGrey),
  SColAsc("FireBrick", Col_FireBrick),
  SColAsc("ForestGreen", Col_ForestGreen),
  SColAsc("Gold", Col_Gold),
  SColAsc("Goldenrod", Col_Goldenrod),
  SColAsc("Gray", Col_Gray),
  SColAsc("Green", Col_Green),
  SColAsc("GreenYellow", Col_GreenYellow),
  SColAsc("Grey", Col_Grey),
  SColAsc("IndianRed", Col_IndianRed),
  SColAsc("Khaki", Col_Khaki),
  SColAsc("LightBlue", Col_LightBlue),
  SColAsc("LightGray", Col_LightGray),
  SColAsc("LightGrey", Col_LightGrey),
  SColAsc("LightSteelBlue", Col_LightSteelBlue),
  SColAsc("LightWood", Col_LightWood),
  SColAsc("LimeGreen", Col_LimeGreen),
  SColAsc("Magenta", Col_Magenta),
  SColAsc("Maroon", Col_Maroon),
  SColAsc("MedianWood", Col_MedianWood),
  SColAsc("MediumAquamarine", Col_MediumAquamarine),
  SColAsc("MediumBlue", Col_MediumBlue),
  SColAsc("MediumForestGreen", Col_MediumForestGreen),
  SColAsc("MediumGoldenrod", Col_MediumGoldenrod),
  SColAsc("MediumOrchid", Col_MediumOrchid),
  SColAsc("MediumSeaGreen", Col_MediumSeaGreen),
  SColAsc("MediumSlateBlue", Col_MediumSlateBlue),
  SColAsc("MediumSpringGreen", Col_MediumSpringGreen),
  SColAsc("MediumTurquoise", Col_MediumTurquoise),
  SColAsc("MediumVioletRed", Col_MediumVioletRed),
  SColAsc("MidnightBlue", Col_MidnightBlue),
  SColAsc("Navy", Col_Navy),
  SColAsc("NavyBlue", Col_NavyBlue),
  SColAsc("Orange", Col_Orange),
  SColAsc("OrangeRed", Col_OrangeRed),
  SColAsc("Orchid", Col_Orchid),
  SColAsc("PaleGreen", Col_PaleGreen),
  SColAsc("Pink", Col_Pink),
  SColAsc("Plum", Col_Plum),
  SColAsc("Red", Col_Red),
  SColAsc("Salmon", Col_Salmon),
  SColAsc("SeaGreen", Col_SeaGreen),
  SColAsc("Sienna", Col_Sienna),
  SColAsc("SkyBlue", Col_SkyBlue),
  SColAsc("SlateBlue", Col_SlateBlue),
  SColAsc("SpringGreen", Col_SpringGreen),
  SColAsc("SteelBlue", Col_SteelBlue),
  SColAsc("Tan", Col_Tan),
  SColAsc("Thistle", Col_Thistle),
  SColAsc("Turquoise", Col_Turquoise),
  SColAsc("Violet", Col_Violet),
  SColAsc("VioletRed", Col_VioletRed),
  SColAsc("Wheat", Col_Wheat),
  SColAsc("White", Col_White),
  SColAsc("Yellow", Col_Yellow),
  SColAsc("YellowGreen", Col_YellowGreen),

  SColAsc(NULL, ColorF(1.0f,1.0f,1.0f))
};

#include <ctype.h>

void shGetColor(const char *buf, ColorF& v)
{
  char name[64];
  int n;

  if (!buf)
  {
    v = Col_White;
    return;
  }
  if (buf[0] == '{')
    buf++;
  if (isalpha(buf[0]))
  {
    n = 0;
    float scal = 1;
    strcpy(name, buf);
    char nm[64];
    if (strchr(buf, '*'))
    {
      while (buf[n] != '*')
      {
        if (buf[n] == 0x20)
          break;
        nm[n] = buf[n];
        n++;
      }
      nm[n] = 0;
      if (buf[n] == 0x20)
      {
        while (buf[n] != '*')
          n++;
      }
      n++;
      while (buf[n] == 0x20)
        n++;
      scal = shGetFloat(&buf[n]);
      strcpy(name, nm);
    }
    n = 0;
    while (sCols[n].nam)
    {
      if (!stricmp(sCols[n].nam, name))
      {
        v = sCols[n].col;
        if (scal != 1)
          v.ScaleCol(scal);
        return;
      }
      n++;
    }
  }
  n = 0;
  while(true)
  {
    if (n == 4)
      break;
    char par[64];
    par[0] = 0;
    fxFillPr((char **)&buf, par);
    if (!par[0])
      break;
    v[n++] = atof(par);
  }

  //v.Clamp();
}

void shGetColor(char *buf, float v[4])
{
  char name[64];

  if (!buf)
  {
    v[0] = 1.0f;
    v[1] = 1.0f;
    v[2] = 1.0f;
    v[3] = 1.0f;
    return;
  }
  if (isalpha(buf[0]))
  {
    int n = 0;
    float scal = 1;
    strcpy(name, buf);
    char nm[64];
    if (strchr(buf, '*'))
    {
      while (buf[n] != '*')
      {
        if (buf[n] == 0x20)
          break;
        nm[n] = buf[n];
        n++;
      }
      nm[n] = 0;
      if (buf[n] == 0x20)
      {
        while (buf[n] != '*')
          n++;
      }
      n++;
      while (buf[n] == 0x20)
        n++;
      scal = shGetFloat(&buf[n]);
      strcpy(name, nm);
    }
    n = 0;
    while (sCols[n].nam)
    {
      if (!stricmp(sCols[n].nam, name))
      {
        v[0] = sCols[n].col[0];
        v[1] = sCols[n].col[1];
        v[2] = sCols[n].col[2];
        v[3] = sCols[n].col[3];
        if (scal != 1)
        {
          v[0] *= scal;
          v[1] *= scal;
          v[2] *= scal;
        }
        return;
      }
      n++;
    }
  }
  int n = sscanf(buf, "%f %f %f %f", &v[0], &v[1], &v[2], &v[3]);
  switch (n)
  {
    case 0:
      v[0] = v[1] = v[2] = v[3] = 1.0f;
      break;

    case 1:
      v[1] = v[2] = v[3] = 1.0f;
      break;

    case 2:
      v[2] = v[3] = 1.0f;
      break;

    case 3:
      v[3] = 1.0f;
      break;
  }
  //v.Clamp();
}

//=========================================================================================

char *GetAssignmentText (char **buf)
{
  SkipCharacters (buf, kWhiteSpace);
  char *result = *buf;

  char theChar;

  while ((theChar = **buf) != 0)
  {
    if (theChar == '[')
    {
      while ((theChar = **buf) != ']')
      {
        if (theChar == 0 || theChar == ';')  
          break;
        ++*buf;
      }
      continue;
    }
    if (theChar <= 0x20 || theChar == ';')  
      break;
    ++*buf;
  }
  **buf = 0;            
  if (theChar)          
    ++*buf;
  return result;
}

char *GetSubText (char **buf, char open, char close)
{
  if (**buf == 0 || **buf != open)
    return 0;
  ++*buf;                       
  char *result = *buf;

  char theChar;
  long skip = 1;

  if (open == close)            
    open = 0;
  while ((theChar = **buf) != 0)
  {
    if (theChar == open)
      ++skip;
    if (theChar == close)
    {
      if (--skip == 0)
      {
        **buf = 0; 
        ++*buf;    
        break;
      }
    }
    ++*buf;       
  }
  return result;
}

_inline static int IsComment(char **buf)
{
  if (!(*buf))
    return 0;

  if ((*buf)[0] == '/' && (*buf)[1] == '/')
    return 2;

  if ((*buf)[0] == '/' && (*buf)[1] == '*')
    return 3;

  return 0;
}

void SkipComments(char **buf, bool bSkipWhiteSpace)
{
  int n;
  static int m;

  while (n=IsComment(buf))
  {
    switch (n)
    {
    case 2:
      // skip comment lines.
      *buf = strchr (*buf, '\n');
      if (*buf && bSkipWhiteSpace)
        SkipCharacters (buf, kWhiteSpace);
      break;

    case 3:
      // skip comment blocks.
      m = 0;
      do
      {
        *buf = strchr (*buf, '*');
        if (!(*buf))
          break;
        if ((*buf)[-1] == '/')
        {
          *buf += 1;
          m++;
        }
        else
          if ((*buf)[1] == '/')
          {
            *buf += 2;
            m--;
          }
          else
            *buf += 1;
      } while (m);
      if (!(*buf))
      {
        iLog->Log ("Warning: Comment lines don't closed\n");
        break;
      }
      if (bSkipWhiteSpace)
        SkipCharacters (buf, kWhiteSpace);
      break;
    }
  }
}

int shGetObject (char **buf, STokenDesc *tokens, char **name, char **data)
{
  if (!*buf)
    return 0;

  SkipCharacters (buf, kWhiteSpace);
  SkipComments(buf, true);

  if (!(*buf))
    return -2;

  if (!**buf)       
    return -2;

  STokenDesc *ptokens = tokens;

  while (tokens->id != 0)
  {
    if (!strnicmp (tokens->token, *buf, strlen(tokens->token)))
    {
      pCurCommand = *buf;
      break;
    }
    ++tokens;
  }
  if (tokens->id == 0)
  {
    char *p = strchr (*buf, '\n');

    char pp[1024];
    if (p)
    {

      strncpy(pp, *buf, p - *buf);
      pp[p - *buf] = 0;

      *buf = p;
    }
    else
      strcpy(pp, *buf);

    iLog->Log ("Warning: Found token '%s' which was not one of the list (Skipping).\n", pp);
    while (ptokens->id != 0)
    {
      iLog->Log ("    %s\n", ptokens->token);
      ptokens++;
    }
    return 0;       
  }
  *buf += strlen (tokens->token);
  SkipCharacters (buf, kWhiteSpace);

  *name = GetSubText (buf, 0x27, 0x27);
  SkipCharacters (buf, kWhiteSpace);

  if (**buf == '=')     
  {
    ++*buf;             
    *data = GetAssignmentText (buf);
  }
  else
  {
    *data = GetSubText (buf, '(', ')');
    if (!*data)
      *data = GetSubText (buf, '{', '}');
  }


  return tokens->id;
}
