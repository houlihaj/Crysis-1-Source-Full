/*=============================================================================
	CFlare.cpp : implementation of light coronas and flares RE.
	Copyright (c) 2001 Crytek Studios. All Rights Reserved.

	Revision history:
		* Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"

void CopyLightStyle(int dest, int src);

//===============================================================

float CREFlare::mfDistanceToCameraSquared(Matrix34& matInst)
{
  Vec3 vCenter = matInst.GetTranslation();
  Vec3 delta = gRenDev->GetRCamera().Orig - vCenter;
  return (delta).GetLengthSquared();
}

// Parsing

bool CREFlare::mfCompile(CParserBin& Parser, SParserFrame& Frame)
{
  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(RGBStyle)
    FX_TOKEN(Scale)
    FX_TOKEN(Blind)
    FX_TOKEN(SizeBlindScale)
    FX_TOKEN(SizeBlindBias)
    FX_TOKEN(IntensBlindScale)
    FX_TOKEN(IntensBlindBias)
    FX_TOKEN(MinLight)
    FX_TOKEN(DistFactor)
    FX_TOKEN(DistIntensityFactor)
    FX_TOKEN(FadeTime)
    FX_TOKEN(Color)
    FX_TOKEN(Layer)
    FX_TOKEN(Importance)
    FX_TOKEN(VisAreaScale)
  FX_END_TOKENS

  bool bRes = true;

  while (Parser.ParseObject(sCommands))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
    case eT_RGBStyle:
      {
        if (Parser.m_Data.IsEmpty())
        {
          Warning("missing RGBStyle argument for Flare Effect");
          break;
        }
        uint32 nTok = Parser.GetToken(Parser.m_Data);
        if (nTok == eT_Poly)
          m_eLightRGB = eLIGHT_Poly;
        else
        if (nTok == eT_Identity)
          m_eLightRGB = eLIGHT_Identity;
        else
        if (nTok == eT_FromObj || nTok == eT_FromLight)
          m_eLightRGB = eLIGHT_Object;
        else
        if (nTok == eT_LightStyle)
        {
          m_eLightRGB = eLIGHT_Style;
          m_Color = ColorF(1.0f);
          if (Parser.m_Assign.IsEmpty())
          {
            Warning("missing RgbStyle LightStyle value (use 0)\n");
            m_LightStyle = 0;
          }
          else
            m_LightStyle = Parser.GetInt(Parser.GetToken(Parser.m_Assign));
        }
        else
        if (nTok == eT_Fixed)
        {
          m_eLightRGB = eLIGHT_Fixed;
          if (Parser.m_Assign.IsEmpty())
          {
            Warning("missing RgbStyle Fixed value (use 1.0)\n");
            m_Color = ColorF(1.0f);
          }
          else
          {
            const char *szStr = Parser.GetString(Parser.m_Assign);
            shGetColor(szStr, m_Color);
          }
        }
        else
          m_eLightRGB = eLIGHT_Identity;
      }
      break;
    case eT_Color:
      {
        if (Parser.m_Data.IsEmpty())
        {
          Warning("missing Color argument for Flare Effect");
          break;
        }
        const char *szStr = Parser.GetString(Parser.m_Data);
        shGetColor(szStr, m_fColor);
      }
      break;
    case eT_Scale:
      m_fScaleCorona = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_Blind:
      m_bBlind = true;
      break;

    case eT_Importance:
      m_Importance = Parser.GetInt(Parser.GetToken(Parser.m_Data));
      break;

    case eT_FadeTime:
      m_fFadeTime = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_SizeBlindScale:
      m_fSizeBlindScale = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_SizeBlindBias:
      m_fSizeBlindBias = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_IntensBlindScale:
      m_fIntensBlindScale = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_IntensBlindBias:
      m_fIntensBlindBias = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_MinLight:
      m_fMinLight = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_DistFactor:
      m_fDistSizeFactor = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_VisAreaScale:
      m_fVisAreaScale = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_DistIntensityFactor:
      m_fDistIntensityFactor = Parser.GetFloat(Parser.m_Data);
      break;

    case eT_Layer:
      {
        assert(0);
      }
      break;
    }
  }

  Parser.EndFrame(OldFrame);

  return bRes;
}

void CREFlare::mfPrepare(void)
{
  gRenDev->EF_CheckOverflow(0, 0, this);
  
  gRenDev->m_RP.m_pRE = this;
  gRenDev->m_RP.m_RendNumIndices = 0;
  gRenDev->m_RP.m_RendNumVerts = 0;

  switch(m_eLightRGB)
  {
    case eLIGHT_Identity:
      m_Color = ColorF(1.0f);
      break;
    case eLIGHT_Style:
      {
        if (m_LightStyle>=0 && m_LightStyle<CLightStyle::m_LStyles.Num() && CLightStyle::m_LStyles[m_LightStyle])
        {
          CLightStyle *ls = CLightStyle::m_LStyles[m_LightStyle];
          ls->mfUpdate(gRenDev->m_RP.m_RealTime);
          m_Color = m_fColor * ls->m_fIntensity;
        }
      }
      break;
    case eLIGHT_Object:
      if (gRenDev->m_RP.m_pCurObject)
        m_Color = gRenDev->m_RP.m_pCurObject->m_II.m_AmbColor;
      else
        m_Color = ColorF(1.0f);
      break;
  }
}
