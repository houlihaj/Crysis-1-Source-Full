#include "StdAfx.h"
#include "RendElement.h"
#include "I3DEngine.h"

float CREBeam::mfDistanceToCameraSquared(Matrix34& matInst)
{
  Vec3 vObj = matInst.GetTranslation();
  Vec3 Delta = gRenDev->GetRCamera().Orig - vObj;
  return Delta.GetLengthSquared();
}

void CREBeam::mfPrepare(void)
{
  CRenderer *rd = gRenDev;

  rd->EF_CheckOverflow(0, 0, this);

  int Features = rd->GetFeatures();
  CRenderObject *obj = rd->m_RP.m_pCurObject;

  if (CRenderer::CV_r_beams == 0)
  {
    rd->m_RP.m_pRE = NULL;
    rd->m_RP.m_RendNumIndices = 0;
    rd->m_RP.m_RendNumVerts = 0;
  }
  else
  if (CRenderer::CV_r_beams == 2)
  {
    if (!m_pBuffer)
    {
      I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
			//@TODO Memory Leak here on IStatObj
      IStatObj *pObj = eng->LoadStatObj(m_ModelName.c_str());	
      m_pBuffer = (CRenderMesh*)pObj->GetRenderMesh();
      if (!m_pBuffer)
      {
        rd->m_RP.m_pRE = NULL;
        rd->m_RP.m_RendNumIndices = 0;
        rd->m_RP.m_RendNumVerts = 0;
        return;
      }
      Vec3 Mins = m_pBuffer->m_vBoxMin;
      Vec3 Maxs = m_pBuffer->m_vBoxMax;

      m_fLengthScale = Maxs.x;
      m_fWidthScale  = Maxs.z;
    }

    CRenderMesh *pRM = m_pBuffer;
    CRenderChunk *pChunk = &(*pRM->m_pChunks)[0];

	  //@TODO: Timur
	  assert(m_pBuffer->GetMaterial() && "RenderMesh must have material");
	  const SShaderItem &shaderItem = m_pBuffer->GetMaterial()->GetShaderItem(pChunk->m_nMatID);
    rd->m_RP.m_pShader = (CShader*)shaderItem.m_pShader;
	  rd->m_RP.m_pShaderResources = (SRenderShaderResources *)shaderItem.m_pShaderResources;
    rd->m_RP.m_pRE = pChunk->pRE;
    rd->m_RP.m_nShaderTechnique = -1;

    obj->m_pRE = this;
    UParamVal pv;
    pv.m_Float = m_fLengthScale;
    SShaderParam::SetParam("origlength", &rd->m_RP.m_pShader->m_PublicParams, pv);

    pv.m_Float = m_fWidthScale;
    SShaderParam::SetParam("origwidth", &rd->m_RP.m_pShader->m_PublicParams, pv);

    pv.m_Float = m_fLength;
    SShaderParam::SetParam("length", &rd->m_RP.m_pShader->m_PublicParams, pv);

    pv.m_Float = m_fStartRadius;
    SShaderParam::SetParam("startradius", &rd->m_RP.m_pShader->m_PublicParams, pv);

    pv.m_Float = m_fEndRadius;
    SShaderParam::SetParam("endradius", &rd->m_RP.m_pShader->m_PublicParams, pv);

    pv.m_Color[0] = m_StartColor[0];
    pv.m_Color[1] = m_StartColor[1];
    pv.m_Color[2] = m_StartColor[2];
    pv.m_Color[3] = m_StartColor[3];
    if (m_LightStyle == 0)
    {
      pv.m_Color[0] = obj->m_II.m_AmbColor[0];
      pv.m_Color[1] = obj->m_II.m_AmbColor[1];
      pv.m_Color[2] = obj->m_II.m_AmbColor[2];
    }
    SShaderParam::SetParam("startcolor", &rd->m_RP.m_pShader->m_PublicParams, pv);

    pv.m_Color[0] = m_EndColor[0];
    pv.m_Color[1] = m_EndColor[1];
    pv.m_Color[2] = m_EndColor[2];
    pv.m_Color[3] = m_EndColor[3];
    if (m_LightStyle == 0)
    {
      pv.m_Color[0] = obj->m_II.m_AmbColor[0];
      pv.m_Color[1] = obj->m_II.m_AmbColor[1];
      pv.m_Color[2] = obj->m_II.m_AmbColor[2];
    }
    SShaderParam::SetParam("endcolor", &rd->m_RP.m_pShader->m_PublicParams, pv);

    //obj->m_ShaderParams = &rd->m_RP.m_pShader->m_PublicParams;

    rd->m_RP.m_FirstVertex = pChunk->nFirstVertId;
    rd->m_RP.m_RendNumIndices = pChunk->nNumIndices;
    rd->m_RP.m_RendNumVerts = pChunk->nNumVerts;
    rd->m_RP.m_FirstIndex = pChunk->nFirstIndexId;
  }
  else
  {
    if (obj->m_pLight && obj->m_pLight->m_Flags & DLF_PROJECT)
    {
      rd->m_RP.m_pRE = this;
    }
    else
    {
      rd->m_RP.m_pRE = NULL;
      rd->m_RP.m_RendNumIndices = 0;
      rd->m_RP.m_RendNumVerts = 0;
    }
  }
}

bool CREBeam::mfCompile(CParserBin& Parser, SParserFrame& Frame)
{
  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(Model)
    FX_TOKEN(StartRadius)
    FX_TOKEN(EndRadius)
    FX_TOKEN(StartColor)
    FX_TOKEN(EndColor)
    FX_TOKEN(LightStyle)
    FX_TOKEN(Length)
  FX_END_TOKENS

  bool bRes = true;
  int nIndex;

  while (Parser.ParseObject(sCommands, nIndex))
  {
    EToken eT = Parser.GetToken();
    switch (eT)
    {
      case eT_Model: 
        m_ModelName = Parser.GetString(Parser.m_Data);
        break;
      case eT_StartRadius:
        if (Parser.m_Data.IsEmpty())
        {
          Warning("missing StartRadius argument for Beam Effect");
          break;
        }
        m_fStartRadius = Parser.GetFloat(Parser.m_Data);
        break;
      case eT_EndRadius:
        if (Parser.m_Data.IsEmpty())
        {
          Warning("missing EndRadius argument for Beam Effect");
          break;
        }
        m_fEndRadius = Parser.GetFloat(Parser.m_Data);
        break;
      case eT_LightStyle:
        if (Parser.m_Data.IsEmpty())
        {
          Warning("missing lightStyle argument for Beam Effect");
          break;
        }
        m_LightStyle = Parser.GetInt(Parser.GetToken(Parser.m_Data));
        break;
      case eT_StartColor:
        {
          if (Parser.m_Data.IsEmpty())
          {
            Warning("missing StartColor argument for Beam Effect");
            break;
          }
          string szStr = Parser.GetString(Parser.m_Data);
          shGetColor(szStr.c_str(), m_StartColor);
        }
        break;
      case eT_EndColor:
        {
          if (Parser.m_Data.IsEmpty())
          {
            Warning("missing EndColor argument for Beam Effect");
            break;
          }
          string szStr = Parser.GetString(Parser.m_Data);
          shGetColor(szStr.c_str(), m_EndColor);
        }
        break;
    }
  }

  SShaderParam *sp;
  sp = new SShaderParam;
  strcpy(sp->m_Name, "origlength");
  sp->m_Type = eType_FLOAT;
  sp->m_Value.m_Float = 10.0f;
  m_ShaderParams.AddElem(sp);

  sp = new SShaderParam;
  strcpy(sp->m_Name, "origwidth");
  sp->m_Type = eType_FLOAT;
  sp->m_Value.m_Float = 1.0f;
  m_ShaderParams.AddElem(sp);

  sp = new SShaderParam;
  strcpy(sp->m_Name, "startradius");
  sp->m_Type = eType_FLOAT;
  sp->m_Value.m_Float = 0.1f;
  m_ShaderParams.AddElem(sp);

  sp = new SShaderParam;
  strcpy(sp->m_Name, "endradius");
  sp->m_Type = eType_FLOAT;
  sp->m_Value.m_Float = 1.0f;
  m_ShaderParams.AddElem(sp);

  sp = new SShaderParam;
  strcpy(sp->m_Name, "startcolor");
  sp->m_Type = eType_FCOLOR;
  sp->m_Value.m_Color[0] = 1.0f;
  sp->m_Value.m_Color[1] = 1.0f;
  sp->m_Value.m_Color[2] = 1.0f;
  sp->m_Value.m_Color[3] = 1.0f;
  m_ShaderParams.AddElem(sp);

  sp = new SShaderParam;
  strcpy(sp->m_Name, "endcolor");
  sp->m_Type = eType_FCOLOR;
  sp->m_Value.m_Color[0] = 1.0f;
  sp->m_Value.m_Color[1] = 1.0f;
  sp->m_Value.m_Color[2] = 1.0f;
  sp->m_Value.m_Color[3] = 0.1f;
  m_ShaderParams.AddElem(sp);

  Parser.EndFrame(OldFrame);

  return bRes;
}

