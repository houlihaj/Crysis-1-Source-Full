/*=============================================================================
D3DRendPipeline.cpp : Direct3D rendering pipeline.
Copyright (c) 2001-2004 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <I3DEngine.h>
#include <CryHeaders.h>
#include "../Common/PostProcess/PostProcessUtils.h"

//============================================================================================
// Shaders rendering
//============================================================================================

//============================================================================================
// Init Shaders rendering

void CD3D9Renderer::EF_InitRandTables()
{ 
  int i;
  float f;

  for (i=0; i<256; i++)
  {
    f = (float)rand() / 32767.0f;
    m_RP.m_tRandFloats[i] = f + f - 1.0f;

    m_RP.m_tRandBytes[i] = (byte)((float)rand() / 32767.0f * 255.0f);
  }
}

void CD3D9Renderer::EF_InitWaveTables()
{
  int i;

  //Init wave Tables
  for (i=0; i<1024; i++)
  {
    float f = (float)i;

    m_RP.m_tSinTable[i] = sin_tpl(f * (360.0f/1023.0f) * (float)M_PI / 180.0f);
    m_RP.m_tHalfSinTable[i] = sin_tpl(f * (360.0f/1023.0f) * (float)M_PI / 180.0f);
    if (m_RP.m_tHalfSinTable[i] < 0)
      m_RP.m_tHalfSinTable[i] = 0;
    m_RP.m_tCosTable[i] = cos_tpl(f * (360.0f/1023.0f) * (float)M_PI / 180.0f);
    m_RP.m_tHillTable[i] = sin_tpl(f * (180.0f/1023.0f) * (float)M_PI / 180.0f);

    if (i < 512)
      m_RP.m_tSquareTable[i] = 1.0f;
    else
      m_RP.m_tSquareTable[i] = -1.0f;

    m_RP.m_tSawtoothTable[i] = f / 1024.0f;
    m_RP.m_tInvSawtoothTable[i] = 1.0f - m_RP.m_tSawtoothTable[i];

    if (i < 512)
    {
      if (i < 256)
        m_RP.m_tTriTable[i] = f / 256.0f;
      else
        m_RP.m_tTriTable[i] = 1.0f - m_RP.m_tTriTable[i-256];
    }
    else
      m_RP.m_tTriTable[i] = 1.0f - m_RP.m_tTriTable[i-512];
  }
}

// Init vertex declarations (for programmable pipeline)
#if defined (DIRECT3D9) || defined (OPENGL)
void CD3D9Renderer::EF_CreateVertexDeclarations(int nStreamMask, D3DVERTEXELEMENT9 *pElements[])
#elif defined (DIRECT3D10)
void CD3D9Renderer::EF_CreateVertexDeclarations(int nStreamMask, D3D10_INPUT_ELEMENT_DESC *pElements[], int nNumElements[])
#endif
{
  int i;
  uint j;

#if defined (DIRECT3D9) || defined (OPENGL)
  for (i=0; i<VERTEX_FORMAT_NUMS; i++)
  {
    if (!m_RP.m_D3DVertexDeclaration[0][i].m_Declaration.Num())
      continue;

    for (j=0; j<m_RP.m_D3DVertexDeclaration[0][i].m_Declaration.Num()-1; j++)
    {
      m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.AddElem(m_RP.m_D3DVertexDeclaration[0][i].m_Declaration[j]);
    }
    for (j=1; j<VSF_NUM; j++)
    {
      if (!(nStreamMask & (1<<(j-1))))
        continue;
      int n = 0;
      while (pElements[j][n].Stream != 0xff)
      {
        m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.Add(pElements[j][n]); 
        n++;
      }
    }
    D3DVERTEXELEMENT9 elemEnd = D3DDECL_END();         // terminate
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.Add(elemEnd);
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.Shrink();
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pDeclaration = NULL;

    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration = new SD3DVertexDeclaration;
    for (j=0; j<m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.Num(); j++)
    {
      D3DVERTEXELEMENT9 *pEl = &m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration[j];
      if (pEl->Stream == 0xff)
        break;
      m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.AddElem(*pEl);
    }
    for (j=0; j<m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.Num(); j++)
    {
      D3DVERTEXELEMENT9 *pEl = &m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration[j];
      if (pEl->Stream != 0xff)
      {
        D3DVERTEXELEMENT9 El = *pEl;
        El.Stream += VSF_MORPHBUDDY;
        El.UsageIndex += 8;
        m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.AddElem(El);
      }
      else
      {
        D3DVERTEXELEMENT9 El = {VSF_MORPHBUDDY_WEIGHTS, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 1};	// BlendWeight
        m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.AddElem(El);
        m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.AddElem(*pEl);
      }
    }
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.Shrink();
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_pDeclaration = NULL;
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_pMorphDeclaration = NULL;
  }
#elif defined (DIRECT3D10)
  int n;
  for (i=0; i<VERTEX_FORMAT_NUMS; i++)
  {
    if (!m_RP.m_D3DVertexDeclaration[0][i].m_Declaration.Num())
      continue;

    for (j=0; j<m_RP.m_D3DVertexDeclaration[0][i].m_Declaration.Num(); j++)
    {
      m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.AddElem(m_RP.m_D3DVertexDeclaration[0][i].m_Declaration[j]);
    }
    for (j=1; j<VSF_NUM; j++)
    {
      if (!(nStreamMask & (1<<(j-1))))
        continue;
      for (n=0; n<nNumElements[j]; n++)
      {
        m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.AddElem(pElements[j][n]); 
      }
    }
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.Shrink();
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pDeclaration = NULL;

    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration = new SD3DVertexDeclaration;
    for (j=0; j<m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.Num(); j++)
    {
      D3D10_INPUT_ELEMENT_DESC *pEl = &m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration[j];
      m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.AddElem(*pEl);
    }
    for (j=0; j<m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration.Num(); j++)
    {
      D3D10_INPUT_ELEMENT_DESC *pEl = &m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_Declaration[j];
      D3D10_INPUT_ELEMENT_DESC El = *pEl;
      El.InputSlot += VSF_MORPHBUDDY;
      El.SemanticIndex += 8;
      m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.AddElem(El);
    }
    D3D10_INPUT_ELEMENT_DESC El = {"BLENDWEIGHT", 1, DXGI_FORMAT_R32G32_FLOAT, VSF_MORPHBUDDY_WEIGHTS, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}; // BlendWeight
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.AddElem(El);

    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_Declaration.Shrink();
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_pDeclaration = NULL;
    m_RP.m_D3DVertexDeclaration[nStreamMask][i].m_pMorphDeclaration->m_pMorphDeclaration = NULL;
  }
#endif
}

void CD3D9Renderer::EF_InitD3DVertexDeclarations()
{
  if (m_RP.m_D3DVertexDeclaration[0][0].m_Declaration.Num())
    return;

  int i;

#if defined (DIRECT3D9) || defined (OPENGL)
  SBufInfoTable *pOffs;
  int j;
  int n = 0;

  //========================================================================================
  // base stream declarations (stream 0)
  D3DVERTEXELEMENT9 elemPos = {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0};
  D3DVERTEXELEMENT9 elemPosS = {0, 0, D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0};

#ifdef XENON
  D3DVERTEXELEMENT9 elemPosTR = {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0};  // position
#else
  D3DVERTEXELEMENT9 elemPosTR = {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0};  // position
#endif

  D3DVERTEXELEMENT9 elemNormalB = {0, 0, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0};
  D3DVERTEXELEMENT9 elemPS = {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_PSIZE, 0};      // psize
  D3DVERTEXELEMENT9 elemColor = {0, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0};    // diffuse
  D3DVERTEXELEMENT9 elemTC = {0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0};      // texture
  D3DVERTEXELEMENT9 elemTC3 = {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0};      // texture
  D3DVERTEXELEMENT9 elemEnd = D3DDECL_END();                                                                   // terminate
  for (n=0; n<VERTEX_FORMAT_NUMS; n++)
  {
    pOffs = &gBufInfoTable[n];
    if (n == VERTEX_FORMAT_TRP3F_COL4UB_TEX2F || n == VERTEX_FORMAT_TRP3F_TEX2F_TEX3F)
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemPosTR);
    else
    if (n == VERTEX_FORMAT_P4S_COL4UB_TEX2F || n == VERTEX_FORMAT_P4S_TEX2F)
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemPosS);
    else
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemPos);
    if (n == VERTEX_FORMAT_P3F_N4B_COL4UB)
    {
      elemNormalB.Offset = sizeof(Vec3);
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemNormalB);
    }
    if (pOffs->OffsColor)
    {
      elemColor.Offset = pOffs->OffsColor;
      elemColor.UsageIndex = 0;
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemColor);
    }
    if (n == VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F)
    {
      elemColor.Offset = pOffs->OffsColor+4;
      elemColor.UsageIndex = 1;
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemColor);
    }
    if (pOffs->OffsTC)
    {
      elemTC.Offset = pOffs->OffsTC;
      elemTC.UsageIndex = 0;
      if (n == VERTEX_FORMAT_P3F_TEX3F)
      {
        elemTC3.Offset = pOffs->OffsTC;
        elemTC3.UsageIndex = 0;
        m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemTC3);
      }
      else
        m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemTC);
      if (n == VERTEX_FORMAT_TRP3F_TEX2F_TEX3F || n == VERTEX_FORMAT_P3F_TEX2F_TEX3F )
      {
        elemTC3.Offset = pOffs->OffsTC+8;
        elemTC3.UsageIndex = 1;
        m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemTC3);
      }
    }
    if (n == VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F)
    {
      elemPS.Offset = (int)(UINT_PTR)&(((struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F *)0)->xaxis);
      elemPS.UsageIndex = 0;
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemPS);
    }
    m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemEnd);
    m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.Shrink();

    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration = new SD3DVertexDeclaration;
    for (j=0; j<m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.Num(); j++)
    {
      D3DVERTEXELEMENT9 *pEl = &m_RP.m_D3DVertexDeclaration[0][n].m_Declaration[j];
      if (pEl->Stream == 0xff)
        break;
      m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.AddElem(*pEl);
    }
    for (j=0; j<m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.Num(); j++)
    {
      D3DVERTEXELEMENT9 *pEl = &m_RP.m_D3DVertexDeclaration[0][n].m_Declaration[j];
      if (pEl->Stream != 0xff)
      {
        D3DVERTEXELEMENT9 El = *pEl;
        El.Stream += VSF_MORPHBUDDY;
        El.UsageIndex += 8;
        m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.AddElem(El);
      }
      else
      {
        D3DVERTEXELEMENT9 El = {VSF_MORPHBUDDY_WEIGHTS, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,	1};	// BlendWeight
        m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.AddElem(El);
        m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.AddElem(*pEl);
      }
    }
    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.Shrink();
    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_pDeclaration = NULL;
    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_pMorphDeclaration = NULL;
  }

  //=============================================================================
  // Additional streams declarations:

  // Tangents stream
  D3DVERTEXELEMENT9 VElemTangents[] =
  {
#ifdef TANG_FLOATS
    {VSF_TANGENTS, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0},  // tangent
    {VSF_TANGENTS, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0},  // binormal
#else
    {VSF_TANGENTS, 0, D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0},  // tangent
    {VSF_TANGENTS, 8, D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0},  // binormal
#endif
    D3DDECL_END()
  };
  // LMTC stream
  D3DVERTEXELEMENT9 VElemLMTC[] =
  {
    {VSF_LMTC, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},  // LM texcoord
    D3DDECL_END()
  };

  //HW Skin stream
  D3DVERTEXELEMENT9 VElemHWSkin[] =
  {
    {VSF_HWSKIN_INFO, 0, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,	0	},	// BlendWeight
    {VSF_HWSKIN_INFO, 4, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0	},	// BlendIndices
    {VSF_HWSKIN_INFO, 8, D3DDECLTYPE_FLOAT3,		D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,			4	},	// BoneSpace
    D3DDECL_END()
  };

  D3DVERTEXELEMENT9 VElemHWSkin_ShapeDeformation[] =
  {
    {VSF_HWSKIN_SHAPEDEFORM_INFO, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1}, // thin vertex pos
    {VSF_HWSKIN_SHAPEDEFORM_INFO, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2}, // fat vertex pos
    {VSF_HWSKIN_SHAPEDEFORM_INFO, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 4}, // index
    D3DDECL_END()
  };

  D3DVERTEXELEMENT9 VElemHWSkin_MorphTargets[] =
  {
    {VSF_HWSKIN_MORPHTARGET_INFO, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 3}, // morph target
    D3DDECL_END()
  };

  //SH coef. stream
  D3DVERTEXELEMENT9 VElemSH[] =
  {
    {VSF_SH_INFO, 0, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 2}, // SH coefs0
    {VSF_SH_INFO, 4, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 3}, // SH coefs1
    D3DDECL_END()
  };

	D3DVERTEXELEMENT9 VElemScatter[] =
	{
		{VSF_SCATTER, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},  // Scattering depth
		D3DDECL_END()
	};

  // stream 1 (Tangent basis vectors)
  // stream 2 (LM tc)
  // stream 3 (HW skin info)
  // stream 4 (SH coefs)
  // stream 5 (Shape deform)
  // stream 6 (Morph target)
	// stream 7 (Scattering depth)
  D3DVERTEXELEMENT9 *pStreamVElements[VSF_NUM];
  for (i=0; i<VSF_NUM; i++)
  {
    switch (i)
    {
    case VSF_GENERAL:
      pStreamVElements[i] = NULL;
      break;
    case VSF_TANGENTS:
      pStreamVElements[i] = VElemTangents;
      break;
    case VSF_LMTC:
      pStreamVElements[i] = VElemLMTC;
      break;
    case VSF_HWSKIN_INFO:
      pStreamVElements[i] = VElemHWSkin;
      break;
    case VSF_SH_INFO:
      pStreamVElements[i] = VElemSH;
      break;
    case VSF_HWSKIN_SHAPEDEFORM_INFO:
      pStreamVElements[i] = VElemHWSkin_ShapeDeformation;
      break;
    case VSF_HWSKIN_MORPHTARGET_INFO:
      pStreamVElements[i] = VElemHWSkin_MorphTargets;
      break;
		case VSF_SCATTER:
			pStreamVElements[i] = VElemScatter;
			break;
    default:
      assert(0);
    }
  }

  // Vertex declarations for mixed streams
  for (i=1; i<(1<<VSF_NUM); i++)
  {
    EF_CreateVertexDeclarations(i, pStreamVElements);
  }

#elif defined (DIRECT3D10)
  SBufInfoTable *pOffs;
  int j;
  int n = 0;

  //========================================================================================
  // base stream declarations (stream 0)
  D3D10_INPUT_ELEMENT_DESC elemPos = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0};

  D3D10_INPUT_ELEMENT_DESC elemPosTR = {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0};  // position

  D3D10_INPUT_ELEMENT_DESC elemNormalB = {"NORMAL", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0};
  D3D10_INPUT_ELEMENT_DESC elemPS = {"PSIZE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0};   // psize
  D3D10_INPUT_ELEMENT_DESC elemColor = {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0};        // diffuse
  D3D10_INPUT_ELEMENT_DESC elemTC0 = {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0};      // texture
  D3D10_INPUT_ELEMENT_DESC elemTC1 = {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0};      // texture
  D3D10_INPUT_ELEMENT_DESC elemTC1_3 = {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0};      // texture
  for (n=0; n<VERTEX_FORMAT_NUMS; n++)
  {
    pOffs = &gBufInfoTable[n];
    if (n == VERTEX_FORMAT_TRP3F_COL4UB_TEX2F || n == VERTEX_FORMAT_TRP3F_TEX2F_TEX3F)
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemPosTR);
    else
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemPos);
    if (n == VERTEX_FORMAT_P3F_N4B_COL4UB)
    {
      elemNormalB.AlignedByteOffset = sizeof(Vec3);
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemNormalB);
    }
    if (pOffs->OffsColor)
    {
      elemColor.AlignedByteOffset = pOffs->OffsColor;
      elemColor.SemanticIndex = 0;
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemColor);
    }
    if (n == VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F)
    {
      elemColor.AlignedByteOffset = pOffs->OffsColor+sizeof(UCol);
      elemColor.SemanticIndex = 1;
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemColor);
    }
    if (pOffs->OffsTC)
    {
      elemTC0.AlignedByteOffset = pOffs->OffsTC;
      elemTC0.SemanticIndex = 0;
      if (n == VERTEX_FORMAT_P3F_TEX3F)
      {
        elemTC1_3.AlignedByteOffset = pOffs->OffsTC;
        elemTC1_3.SemanticIndex = 0;
        m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemTC1_3);
      }
      else
        m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemTC0);
      if (n == VERTEX_FORMAT_TRP3F_TEX2F_TEX3F || n == VERTEX_FORMAT_P3F_TEX2F_TEX3F )
      {
        elemTC1_3.AlignedByteOffset = pOffs->OffsTC+8;
        elemTC1_3.SemanticIndex = 1;
        m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemTC1_3);
      }
    }
    if (n == VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F)
    {
      elemPS.AlignedByteOffset = (int)(UINT_PTR)&(((struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F *)0)->xaxis);
      elemPS.SemanticIndex = 0;
      m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.AddElem(elemPS);
    }
    m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.Shrink();

    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration = new SD3DVertexDeclaration;
    for (j=0; j<m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.Num(); j++)
    {
      D3D10_INPUT_ELEMENT_DESC *pEl = &m_RP.m_D3DVertexDeclaration[0][n].m_Declaration[j];
      m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.AddElem(*pEl);
    }
    for (j=0; j<m_RP.m_D3DVertexDeclaration[0][n].m_Declaration.Num(); j++)
    {
      D3D10_INPUT_ELEMENT_DESC *pEl = &m_RP.m_D3DVertexDeclaration[0][n].m_Declaration[j];
      D3D10_INPUT_ELEMENT_DESC El = *pEl;
      El.InputSlot += VSF_MORPHBUDDY;
      El.SemanticIndex += 8;
      m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.AddElem(El);
    }
    D3D10_INPUT_ELEMENT_DESC El = {"BLENDWEIGHT", 1, DXGI_FORMAT_R32G32_FLOAT, VSF_MORPHBUDDY_WEIGHTS, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}; // BlendWeight
    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.AddElem(El);

    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_Declaration.Shrink();
    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_pDeclaration = NULL;
    m_RP.m_D3DVertexDeclaration[0][n].m_pMorphDeclaration->m_pMorphDeclaration = NULL;
  }

  //=============================================================================
  // Additional streams declarations:

  // Tangents stream
  D3D10_INPUT_ELEMENT_DESC VElemTangents[] =
  {
#ifdef TANG_FLOATS
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, VSF_TANGENTS, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},   // Binormal
    {"BINORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, VSF_TANGENTS, 16, D3D10_INPUT_PER_VERTEX_DATA, 0}, // Tangent
#else
    {"TANGENT", 0, DXGI_FORMAT_R16G16B16A16_SNORM, VSF_TANGENTS, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},   // Binormal
    {"BINORMAL", 0, DXGI_FORMAT_R16G16B16A16_SNORM, VSF_TANGENTS, 8, D3D10_INPUT_PER_VERTEX_DATA, 0},  // Tangent
#endif
  };
  // LMTC stream
  D3D10_INPUT_ELEMENT_DESC VElemLMTC[] =
  {
    {"TEXCOORD1", 0, DXGI_FORMAT_R32G32_FLOAT, VSF_LMTC, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}, // LM texcoord
  };

  //HW Skin stream
  D3D10_INPUT_ELEMENT_DESC VElemHWSkin[] =
  {
    {"BLENDWEIGHT", 0, DXGI_FORMAT_R8G8B8A8_UNORM, VSF_HWSKIN_INFO, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}, // BlendWeight
    {"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UNORM, VSF_HWSKIN_INFO, 4, D3D10_INPUT_PER_VERTEX_DATA, 0}, // BlendIndices
    {"POSITION", 4, DXGI_FORMAT_R32G32B32_FLOAT, VSF_HWSKIN_INFO, 8, D3D10_INPUT_PER_VERTEX_DATA, 0}, // BoneSpace
  };

  D3D10_INPUT_ELEMENT_DESC VElemHWSkin_ShapeDeformation[] =
  {
    {"POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, VSF_HWSKIN_SHAPEDEFORM_INFO, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}, // thin vertex pos
    {"POSITION", 2, DXGI_FORMAT_R32G32B32_FLOAT, VSF_HWSKIN_SHAPEDEFORM_INFO, 12, D3D10_INPUT_PER_VERTEX_DATA, 0}, // fat vertex pos
    {"COLOR", 4, DXGI_FORMAT_R8G8B8A8_UNORM, VSF_HWSKIN_SHAPEDEFORM_INFO, 24, D3D10_INPUT_PER_VERTEX_DATA, 0}, // index
  };

  D3D10_INPUT_ELEMENT_DESC VElemHWSkin_MorphTargets[] =
  {
    {"POSITION", 3, DXGI_FORMAT_R32G32B32_FLOAT, VSF_HWSKIN_MORPHTARGET_INFO, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}, // morph target
  };

  //SH coef. stream
  D3D10_INPUT_ELEMENT_DESC VElemSH[] =
  {
    {"COLOR", 2, DXGI_FORMAT_R8G8B8A8_UINT, VSF_SH_INFO, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}, // SH coefs0
    {"COLOR", 3, DXGI_FORMAT_R8G8B8A8_UINT, VSF_SH_INFO, 4, D3D10_INPUT_PER_VERTEX_DATA, 0}, // SH coefs1
  };

	D3D10_INPUT_ELEMENT_DESC VElemScatter[] =
	{
		{"TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, VSF_SCATTER, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},  // Scattering depth
	};

  // stream 1 (Tangent basis vectors)
  // stream 2 (LM tc)
  // stream 3 (HW skin info)
  // stream 4 (SH coefs)
  // stream 5 (Shape deform)
  // stream 6 (Morph target)
	// stream 7 (Scattering depth)
  D3D10_INPUT_ELEMENT_DESC *pStreamVElements[VSF_NUM];
  int nNumElements[VSF_NUM];
  for (i=0; i<VSF_NUM; i++)
  {
    switch (i)
    {
    case VSF_GENERAL:
      pStreamVElements[i] = NULL;
      nNumElements[i] = 0;
      break;
    case VSF_TANGENTS:
      pStreamVElements[i] = VElemTangents;
      nNumElements[i] = sizeof(VElemTangents) / sizeof(D3D10_INPUT_ELEMENT_DESC);
      break;
    case VSF_LMTC:
      pStreamVElements[i] = VElemLMTC;
      nNumElements[i] = sizeof(VElemLMTC) / sizeof(D3D10_INPUT_ELEMENT_DESC);
      break;
    case VSF_HWSKIN_INFO:
      pStreamVElements[i] = VElemHWSkin;
      nNumElements[i] = sizeof(VElemHWSkin) / sizeof(D3D10_INPUT_ELEMENT_DESC);
      break;
    case VSF_SH_INFO:
      pStreamVElements[i] = VElemSH;
      nNumElements[i] = sizeof(VElemSH) / sizeof(D3D10_INPUT_ELEMENT_DESC);
      break;
    case VSF_HWSKIN_SHAPEDEFORM_INFO:
      pStreamVElements[i] = VElemHWSkin_ShapeDeformation;
      nNumElements[i] = sizeof(VElemHWSkin_ShapeDeformation) / sizeof(D3D10_INPUT_ELEMENT_DESC);
      break;
    case VSF_HWSKIN_MORPHTARGET_INFO:
      pStreamVElements[i] = VElemHWSkin_MorphTargets;
      nNumElements[i] = sizeof(VElemHWSkin_MorphTargets) / sizeof(D3D10_INPUT_ELEMENT_DESC);
      break;
    case VSF_SCATTER:
      pStreamVElements[i] = VElemScatter;
      nNumElements[i] = sizeof(VElemScatter) / sizeof(D3D10_INPUT_ELEMENT_DESC);
      break;
    default:
      assert(0);
    }
  }

  // Vertex declarations for mixed streams
  for (i=1; i<(1<<VSF_NUM); i++)
  {
    EF_CreateVertexDeclarations(i, pStreamVElements, nNumElements);
  }

#endif

  m_MaxVertBufferSize = CLAMP((int)(CV_d3d9_pip_buff_size), 5, 200)*1024*1024;
  m_CurVertBufferSize = 0;

  for (i=0; i<MAX_STREAMS; i++)
  {
    m_RP.m_ReqStreamFrequence[i] = 1;
    m_RP.m_VertexStreams[i].nFreq = 1;
  }
}

_inline static void *sAlign0x20(byte *vrts)
{
  return (void*)(((INT_PTR)vrts + 0x1f) & ~0x1f);
}

// Init the vertex format merge map.
void CD3D9Renderer::EF_InitVFMergeMap()
{
  for (int i=0; i<VERTEX_FORMAT_NUMS; i++)
  {
    for (int j=0; j<VERTEX_FORMAT_NUMS; j++)
    {
      SVertBufComps Cps[2];
      GetVertBufComps(&Cps[0], i);
      GetVertBufComps(&Cps[1], j);

      bool bNeedTC = Cps[1].m_bHasTC | Cps[0].m_bHasTC;
      bool bNeedCol = Cps[1].m_bHasColors | Cps[0].m_bHasColors;
      bool bShortPos = Cps[1].m_bShortPos | Cps[0].m_bShortPos;
      if (bShortPos)
      {
        if (!bNeedCol)
          m_RP.m_VFormatsMerge[i][j] = VERTEX_FORMAT_P4S_TEX2F;
        else
          m_RP.m_VFormatsMerge[i][j] = VERTEX_FORMAT_P4S_COL4UB_TEX2F;
      }
      else
      if (i == VERTEX_FORMAT_P3F_N4B_COL4UB || j == VERTEX_FORMAT_P3F_N4B_COL4UB)
        m_RP.m_VFormatsMerge[i][j] = VERTEX_FORMAT_P3F_N4B_COL4UB;
      else
      if (i == VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F || j == VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F)
        m_RP.m_VFormatsMerge[i][j] = VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F;
      else
        m_RP.m_VFormatsMerge[i][j] = VertFormatForComponents(bNeedCol, bNeedTC, false);
    }
  }
}

// Init shaders pipeline
void CD3D9Renderer::EF_Init()
{
  bool nv = 0;

  if (CV_r_logTexStreaming && !m_LogFileStr)
  {
    m_LogFileStr = fxopen ("Direct3DLogStreaming.txt", "w");
    if (m_LogFileStr)
    {      
      iLog->Log("Direct3D texture streaming log file '%s' opened\n", "Direct3DLogStreaming.txt");
      char time[128];
      char date[128];

      _strtime( time );
      _strdate( date );

      fprintf(m_LogFileStr, "\n==========================================\n");
      fprintf(m_LogFileStr, "Direct3D Textures streaming Log file opened: %s (%s)\n", date, time);
      fprintf(m_LogFileStr, "==========================================\n");
    }
  }

  m_RP.m_MaxVerts = CV_d3d9_rb_verts;
  m_RP.m_MaxTris = CV_d3d9_rb_tris;

	iLog->Log("");
  iLog->Log("Allocate render buffer for particles (%d verts, %d tris)...", m_RP.m_MaxVerts, m_RP.m_MaxTris);

  int n = 0;

  int nSizeV = 0;
  /*for (int i=0; i<VERTEX_FORMAT_NUMS; i++)
  {
    nSizeV = max(nSizeV, m_VertexSize[i]);
  }*/
  nSizeV = sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F);

  n += nSizeV * m_RP.m_MaxVerts + 32;

  n += sizeof(SPipTangents) * m_RP.m_MaxVerts + 32;

  //m_RP.mRendIndices;
  n += sizeof(ushort)*3*m_RP.m_MaxTris+32;

  byte *buf = new byte [n];
  m_RP.m_SizeSysArray = n;
  m_RP.m_SysArray = buf;
  if (!buf)
    iConsole->Exit("Can't allocate buffers for RB");

  memset(buf, 0, n);

  m_RP.m_Ptr.Ptr = sAlign0x20(buf);
  buf += sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F) * m_RP.m_MaxVerts + 32;

  m_RP.m_PtrTang.Ptr = sAlign0x20(buf);
  buf += sizeof(SPipTangents) * m_RP.m_MaxVerts + 32;

  m_RP.m_RendIndices = (ushort *)sAlign0x20(buf);
  m_RP.m_SysRendIndices = m_RP.m_RendIndices;
  buf += sizeof(ushort)*3*m_RP.m_MaxTris+32;

  EF_Restore();

  EF_InitWaveTables();
  EF_InitRandTables();
  EF_InitD3DVertexDeclarations();
  EF_InitLightInfotable_DB();

  //==================================================

  CRenderObject::m_Waves.Create(32);
  m_RP.m_VisObjects = new CRenderObject *[MAX_REND_OBJECTS];

  if (!m_RP.m_TempObjects.Num())
    m_RP.m_TempObjects.Reserve(MAX_REND_OBJECTS);
  if (!m_RP.m_Objects.Num())
  {
    m_RP.m_Objects.Reserve(MAX_REND_OBJECTS);
    m_RP.m_Objects.SetUse(1);
    SAFE_DELETE_ARRAY(m_RP.m_ObjectsPool);
    m_RP.m_nNumObjectsInPool = 512;
    m_RP.m_ObjectsPool = new CRenderObject[m_RP.m_nNumObjectsInPool];
    for (uint i=0; i<m_RP.m_nNumObjectsInPool; i++)
    {
      m_RP.m_TempObjects[i] = &m_RP.m_ObjectsPool[i];
      m_RP.m_TempObjects[i]->Init();
      m_RP.m_TempObjects[i]->m_II.m_AmbColor = Col_White;
      m_RP.m_TempObjects[i]->m_ObjFlags = 0;
      m_RP.m_TempObjects[i]->m_II.m_Matrix.SetIdentity();
      m_RP.m_TempObjects[i]->m_RState = 0;
    }
    m_RP.m_VisObjects[0] = &m_RP.m_ObjectsPool[0];
  }
  m_RP.m_NumVisObjects = 1;

  // create hdr element  
  m_RP.m_pREHDR = (CREHDRProcess *)EF_CreateRE(eDATA_HDRProcess);

  //SDynTexture::CreateShadowPool();

//  m_RP.m_fLastWaterFOVUpdate = 0;
//  m_RP.m_LastWaterViewdirUpdate = Vec3(0, 0, 0);
//  m_RP.m_LastWaterUpdirUpdate = Vec3(0, 0, 0);
//  m_RP.m_LastWaterPosUpdate = Vec3(0, 0, 0);
  m_RP.m_fLastWaterUpdate = 0;
  m_RP.m_nLastWaterFrameID = 0;
  m_RP.m_bLastWaterAnisotropicRefl = false;
  memset(m_RP.m_bFrameUpdated, 0, sizeof(m_RP.m_bFrameUpdated));
  m_RP.m_nCurrUpdateType = 0;
  m_RP.m_fCurrUpdateFactor = 0.0f;
  m_RP.m_fCurrUpdateDistance = 0.0f;
}

// Invalidate shaders pipeline
void CD3D9Renderer::EF_Invalidate()
{
  int j;
  for (j=0; j<MAX_DYNVBS; j++)
  {
    SAFE_DELETE (m_RP.m_VBs[j].VBPtr_0);
  }
  SAFE_DELETE(m_RP.m_IndexBuf);
  SAFE_DELETE(m_RP.m_VB_Inst);
}

// Restore shaders pipeline
void CD3D9Renderer::EF_Restore()
{
  int j;

  if (!m_RP.m_MaxTris)
    return;

  EF_Invalidate();

  m_RP.m_IndexBuf = new DynamicIB<ushort>(m_pd3dDevice, m_RP.m_MaxTris*3);
  m_RP.m_VB_Inst = new DynamicVB <vec4_t>(m_pd3dDevice, 0, MAX_HWINST_PARAMS);

  for (j=0; j<MAX_DYNVBS; j++)
  {
    m_RP.m_VBs[j].VBPtr_5 = new DynamicVB <struct_VERTEX_FORMAT_P3F_COL4UB_RES4UB_TEX2F_PS4F>(m_pd3dDevice, 0, m_RP.m_MaxVerts);
  }
}

// Shutdown shaders pipeline
void CD3D9Renderer::EF_PipelineShutdown()
{
  uint i, j;

  EF_Invalidate();

  CRenderObject::m_Waves.Free();
  SAFE_DELETE_ARRAY(m_RP.m_VisObjects);
  SAFE_DELETE_ARRAY(m_RP.m_SysArray);  
  m_RP.m_SysVertexPool.Free();  
  m_RP.m_SysIndexPool.Free();  

  for (i=0; i<1<<VSF_NUM; i++)
  {
    for (j=0; j<VERTEX_FORMAT_NUMS; j++)
    {
      SAFE_DELETE(m_RP.m_D3DVertexDeclaration[i][j].m_pMorphDeclaration);
      SAFE_RELEASE(m_RP.m_D3DVertexDeclaration[i][j].m_pDeclaration);
      m_RP.m_D3DVertexDeclaration[i][j].m_Declaration.Free();
    }
  }

  for (i=0; i<CREClientPoly2D::m_PolysStorage.Num(); i++)
  {
    SAFE_RELEASE(CREClientPoly2D::m_PolysStorage[i]);
  }
  CREClientPoly2D::m_PolysStorage.Free();

  for (j=0; j<4; j++)
  {
    for (i=0; i<CREClientPoly::m_PolysStorage[j].Num(); i++)
    {
      SAFE_RELEASE(CREClientPoly::m_PolysStorage[j][i]);
    }
    CREClientPoly::m_PolysStorage[j].Free();
  }

  SafeReleaseParticleREs();

  CHWShader_D3D::ShutDown();
}

void DrawFullScreenQuad(float fLeftU, float fTopV, float fRightU, float fBottomV);


//==========================================================================
// Calculate current scene node matrices
void CD3D9Renderer::EF_SetCameraInfo()
{
  GetModelViewMatrix(&m_ViewMatrix(0,0));
  m_CameraMatrix = m_ViewMatrix;

  GetProjectionMatrix(&m_ProjMatrix(0,0));  


  if( m_RP.m_PersFlags & RBPF_OBLIQUE_FRUSTUM_CLIPPING)
  {
    Matrix44 mObliqueProjMatrix;
    mObliqueProjMatrix.SetIdentity();

    mObliqueProjMatrix.m02 = m_RP.m_pObliqueClipPlane.n[0]; 
    mObliqueProjMatrix.m12 = m_RP.m_pObliqueClipPlane.n[1];
    mObliqueProjMatrix.m22 = m_RP.m_pObliqueClipPlane.n[2];
    mObliqueProjMatrix.m32 = m_RP.m_pObliqueClipPlane.d; 

    mathMatrixMultiplyF((D3DXMATRIXA16 *)m_ProjMatrix.GetData(), (D3DXMATRIXA16 *)mObliqueProjMatrix.GetData(), (D3DXMATRIXA16 *)m_ProjMatrix.GetData(),g_CpuFlags); 
  }

  mathMatrixMultiplyF((D3DXMATRIXA16 *)m_CameraProjMatrix.GetData(), (D3DXMATRIXA16 *)m_ProjMatrix.GetData(), (D3DXMATRIXA16 *)m_CameraMatrix.GetData(), g_CpuFlags); 
  mathMatrixMultiplyF((D3DXMATRIXA16 *)m_CameraProjZeroMatrix.GetData(), (D3DXMATRIXA16 *)m_ProjMatrix.GetData(), (D3DXMATRIXA16 *)m_CameraZeroMatrix.GetData(), g_CpuFlags);
  mathMatrixInverse(m_InvCameraProjMatrix.GetData(), m_CameraProjMatrix.GetData(), g_CpuFlags);
  
  m_RP.m_PersFlags &= ~RBPF_WASWORLDSPACE;
  m_RP.m_PersFlags |= RBPF_FP_DIRTY;
  m_RP.m_ObjFlags = FOB_TRANS_MASK;
  m_RP.m_TransformFrame++;
  m_RP.m_FrameObject++;

  CHWShader_D3D::mfSetCameraParams();
}

DEFINE_ALIGNED_DATA( Matrix44, sIdentityMatrix(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1), 16 );

// Set object transform for fixed pipeline shader
void CD3D9Renderer::EF_SetObjectTransform(CRenderObject *obj, CShader *pSH, int nTransFlags)
{
  if (nTransFlags & FOB_TRANS_MASK)
    mathMatrixMultiply(m_ViewMatrix.GetData(), m_CameraMatrix.GetData(), GetTransposed44(Matrix44(obj->m_II.m_Matrix)).GetData(), g_CpuFlags);
  else
    m_ViewMatrix = m_CameraMatrix;
  m_matView->LoadMatrix(&m_ViewMatrix);
  m_RP.m_PersFlags |= RBPF_FP_MATRIXDIRTY;
}

// Set clip plane for the current scene
// on NVidia NV2X GPUs we use fake clip planes using texkill PS instruction : m_RP.m_ClipPlaneEnabled = 1
// on ATI hardware and NV30 and better we use native ClipPlanes : m_RP.m_ClipPlaneEnabled = 2
void CD3D9Renderer::EF_SetClipPlane (bool bEnable, float *pPlane, bool bRefract)
{
  if (!CV_d3d9_clipplanes)
    return;

  if (bEnable)
  {
#ifdef DO_RENDERLOG
    if (CV_r_log)
      Logv(SRendItem::m_RecurseLevel, "Set clip-plane\n");
#endif
    if (m_RP.m_ClipPlaneEnabled)
      return;
    Plane p;
    p.n[0] = pPlane[0];
    p.n[1] = pPlane[1];
    p.n[2] = pPlane[2];
    p.d = pPlane[3];
    //if (bRefract)
    //p[3] += 0.1f;
    m_RP.m_bClipPlaneRefract = bRefract;
    m_RP.m_CurClipPlane.m_Normal.x = p.n[0];
    m_RP.m_CurClipPlane.m_Normal.y = p.n[1];
    m_RP.m_CurClipPlane.m_Normal.z = p.n[2];
    m_RP.m_CurClipPlane.m_Dist = p.d;
    m_RP.m_CurClipPlane.Init();

    m_RP.m_CurClipPlaneCull = m_RP.m_CurClipPlane;
    m_RP.m_CurClipPlaneCull.m_Dist = -m_RP.m_CurClipPlaneCull.m_Dist;
    int nGPU = m_Features & RFT_HW_MASK;

    m_RP.m_ClipPlaneEnabled = 2;

    // Since we use programmable pipeline only
    // always keep clip-plane in clip space
    Plane pTr = TransformPlane2(m_InvCameraProjMatrix, p);
        
#if defined (DIRECT3D9) || defined(OPENGL)
    m_pd3dDevice->SetClipPlane(0, &pTr.n[0]);
    m_pd3dDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 1);
#elif defined (DIRECT3D10)
    assert(0);
#endif
  }
  else
  {
#ifdef DO_RENDERLOG
    if (CV_r_log)
      Logv(SRendItem::m_RecurseLevel, "Reset clip-plane\n");
#endif
    m_RP.m_ClipPlaneEnabled = 0;

#if defined (DIRECT3D9) || defined(OPENGL)
    m_pd3dDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
#elif defined (DIRECT3D10)
    assert(0);
#endif
  }
}



//==============================================================================
// Shader Pipeline
//=======================================================================

void CD3D9Renderer::EF_SetFogColor(ColorF Color)
{
  const SSkyLightRenderParams* pSkyLightRenderParams(GetSkyLightRenderParams());
  if (pSkyLightRenderParams)
  {
    if(IsHDRModeEnabled())
    {
      Color.r += min( pSkyLightRenderParams->m_hazeColor.x, 64.0f );
      Color.g += min( pSkyLightRenderParams->m_hazeColor.y, 64.0f );
      Color.b += min( pSkyLightRenderParams->m_hazeColor.z, 64.0f );
    }
    else
    {			
      Color.r += 1.0f - expf( -1 * pSkyLightRenderParams->m_hazeColor.x );
      Color.g += 1.0f - expf( -1 * pSkyLightRenderParams->m_hazeColor.y );
      Color.b += 1.0f - expf( -1 * pSkyLightRenderParams->m_hazeColor.z );
    }
  }

  m_FS.m_CurColor = Color;
}

// Set current texture color op modes (used in fixed pipeline shaders)
void CD3D9Renderer::SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa)
{
  EF_SetColorOp(eCo, eAo, eCa, eAa);
}

void CD3D9Renderer::EF_SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa)
{
  int stage = CTexture::m_CurStage;
  //assert(stage == 0);
  if (eCo != 255 && m_eCurColorOp[stage] != eCo)
  {
    m_eCurColorOp[stage] = eCo;
    m_RP.m_PersFlags |= RBPF_FP_DIRTY;
  }
  if (eAo != 255 && m_eCurAlphaOp[stage] != eAo)
  {
    m_eCurAlphaOp[stage] = eAo;
    m_RP.m_PersFlags |= RBPF_FP_DIRTY;
  }
  if (eCa != 255 && m_eCurColorArg[stage] != eCa)
  {
    m_eCurColorArg[stage] = eCa;
    m_RP.m_PersFlags |= RBPF_FP_DIRTY;
  }
  if (eAa != 255 && m_eCurAlphaArg[stage] != eAa)
  {
    m_eCurAlphaArg[stage] = eAa;
    m_RP.m_PersFlags |= RBPF_FP_DIRTY;
  }
}

void CD3D9Renderer::FX_ScreenStretchRect( CTexture *pDst )
{
  if( pDst && pDst->GetDeviceTexture() )
  {
    int iTempX, iTempY, iWidth, iHeight;
    gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

    // in hdr we render to HDRScene target - can just render to scene target instead of using resolve
    if( CRenderer::CV_r_hdrrendering && CTexture::m_Text_SceneTarget && CTexture::m_Text_HDRTarget && !(CTexture::m_Text_HDRTarget->GetFlags() & FT_USAGE_FSAA && gRenDev->m_RP.m_FSAAData.Type))
    {
      using namespace NPostEffects;
      
      gcpRendD3D->Set2DMode(true, 1, 1);      
      CTexture::BindNULLFrom(0); // workaround for dx10 skipping setting correct samplers

      SD3DSurface *pCurrDepthBuffer = ( pDst->GetFlags() & FT_USAGE_FSAA && gRenDev->m_RP.m_FSAAData.Type)?&gcpRendD3D->m_DepthBufferOrigFSAA : &gcpRendD3D->m_DepthBufferOrig;

      gcpRendD3D->FX_PushRenderTarget(0, pDst, pCurrDepthBuffer); 
      gRenDev->SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());        

      static CCryName pTechName("TextureToTexture");                 
      SPostEffectsUtils::ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      gRenDev->EF_SetState(GS_NODEPTHTEST);   

      SPostEffectsUtils::SetTexture(CTexture::m_Text_HDRTarget, 0, FILTER_LINEAR);    
      SPostEffectsUtils::DrawFullScreenQuad(pDst->GetWidth(), pDst->GetHeight());
      SPostEffectsUtils::ShEndPass();

      // Restore previous viewport
      gcpRendD3D->FX_PopRenderTarget(0);
      gRenDev->SetViewport(iTempX, iTempY, iWidth, iHeight);        
      gcpRendD3D->Set2DMode(false, 1, 1);     
    }
    else
    {
      // update scene target before using it for water rendering
#if defined (DIRECT3D9) || defined(OPENGL)
      LPDIRECT3DSURFACE9 pBackSurface= gcpRendD3D->GetBackSurface();

      LPDIRECT3DTEXTURE9 pTexture = (LPDIRECT3DTEXTURE9)pDst->GetDeviceTexture();
      LPDIRECT3DSURFACE9 pTexSurf;
      pTexture->GetSurfaceLevel(0, &pTexSurf);

      RECT pSrcRect={0, 0, iWidth, iHeight };
      RECT pDstRect={0, 0, pDst->GetWidth(), pDst->GetHeight() };

      CD3D9Renderer::SRTStack *pCurRT = m_pNewTarget[0];
      
      if( pDst->GetFlags() & FT_USAGE_FSAA ) 
        gcpRendD3D->m_RP.m_PS.m_RTCopiedFSAA++;
      else
        gcpRendD3D->m_RP.m_PS.m_RTCopied++;

      gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += pDst->GetDeviceDataSize();
      gcpRendD3D->m_pd3dDevice->StretchRect(pCurRT->m_pTarget, &pSrcRect, pTexSurf, &pDstRect, D3DTEXF_NONE); 

      SAFE_RELEASE(pTexSurf);
#elif defined (DIRECT3D10)
      ID3D10Texture2D *pDstResource = (ID3D10Texture2D *) pDst->GetDeviceTexture();
      ID3D10RenderTargetView* pOrigRT = m_pNewTarget[0]->m_pTarget;
      ID3D10Resource *pSrcResource;
      D3D10_RENDER_TARGET_VIEW_DESC Desc;
      if (pOrigRT)
      {
        pOrigRT->GetResource(&pSrcResource);
        pOrigRT->GetDesc(&Desc);
        if (Desc.ViewDimension == D3D10_RTV_DIMENSION_TEXTURE2DMS)
        {
          gcpRendD3D->m_RP.m_PS.m_RTCopiedFSAA++;
          gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += pDst->GetDeviceDataSize();
          m_pd3dDevice->ResolveSubresource(pDstResource, 0, pSrcResource, 0, Desc.Format);
        }
        else
        {
          D3D10_BOX box;
          ZeroStruct(box);
          box.right = pDst->GetWidth();
          box.bottom = pDst->GetHeight();
          box.back = 1;

          gcpRendD3D->m_RP.m_PS.m_RTCopied++;
          gcpRendD3D->m_RP.m_PS.m_RTCopiedSize += pDst->GetDeviceDataSize();
          m_pd3dDevice->CopySubresourceRegion(pDstResource, 0, 0, 0, 0, pSrcResource, 0, &box);
        }
        SAFE_RELEASE(pSrcResource);
      }
#endif

    }

  }
}


bool CD3D9Renderer::FX_ScatterScene(bool bEnable, bool ScatterPass)
{  
  if(bEnable)
  {
    //GetUtils().Log(" +++ Begin Scattering layer +++ \n"); 

    if (ScatterPass)
    {
      Logv(SRendItem::m_RecurseLevel, "\n   +++ Scattering layers - depth accumulation +++ \n"); 
      gcpRendD3D->m_RP.m_PersFlags2 |= RBPF2_SCATTERPASS;
      EF_SetState(m_RP.m_CurState | (GS_BLSRC_ONE | GS_BLDST_ONE)) ;

#if defined(DIRECT3D10)
      SStateBlend bl = m_StatesBL[m_nCurStateBL];
      //bl.Desc.BlendOp = D3D10_BLEND_OP_REV_SUBTRACT;
      bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_REV_SUBTRACT;
      SetBlendState(&bl);
#else
      m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
#endif

    }
    else 
    {
      Logv(SRendItem::m_RecurseLevel, "\n   +++ Start Scattering depth layers +++ \n"); 
      gcpRendD3D->m_RP.m_PersFlags2 &= ~RBPF2_SCATTERPASS;

      EF_SetState(m_RP.m_CurState | (GS_BLSRC_ONE | GS_BLDST_ONE)) ;
#if defined(DIRECT3D10)
      SStateBlend bl = m_StatesBL[m_nCurStateBL];
      //bl.Desc.BlendOp = D3D10_BLEND_OP_ADD;
      bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
      SetBlendState(&bl);
#else
      m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
#endif

      //CTexture::m_Text_ScatterLayer->Invalidate(GetWidth()/2, GetHeight()/2,eTF_A16B16G16R16F);  
      //int nWidth = CTexture::m_Text_ScatterLayer->m_nWidth;
      //int nHeight = CTexture::m_Text_ScatterLayer->m_nHeight;
      //SD3DSurface* pSepDepthSurf = FX_GetDepthSurface(nWidth, nHeight, false);

      //gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_ScatterLayer, pSepDepthSurf);
      //gcpRendD3D->SetViewport(0, 0, nWidth, nHeight);
      //ColorF clearColor(0.0f, 0.0f, 0.0f, 0.0f);
      //gcpRendD3D->EF_ClearBuffers(FRT_CLEAR, &clearColor);
    }

  }
  else
  { 
    gcpRendD3D->m_RP.m_PersFlags2 &= ~RBPF2_SCATTERPASS;

      EF_SetState(m_RP.m_CurState | (GS_BLSRC_ONE | GS_BLDST_ONE)) ;
    //EF_SetState(m_RP.m_CurState & (~GS_BLEND_MASK)) ;
#if defined(DIRECT3D10)
    SStateBlend bl = m_StatesBL[m_nCurStateBL];
    //bl.Desc.BlendOp = D3D10_BLEND_OP_ADD;
    bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
    SetBlendState(&bl);
#else
    m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
#endif

    Logv(SRendItem::m_RecurseLevel, "\n   +++ End Scattering depth layers +++ \n"); 

    //gcpRendD3D->Set2DMode(true, 1, 1);

    //GetUtils().m_pCurDepthSurface = &gcpRendD3D->m_DepthBufferOrig;//FX_GetScreenDepthSurface();
    // Investigate: There's some issue with viewports, if copyScreenToTexture/copyTextureToTexture orders swap around, it doens't copy correctly
    //GetUtils().StretchRect(CTexture::m_Text_BackBuffer, CTexture::m_Text_ScatterLayer);
    //GetUtils().TexBlurGaussian(CTexture::m_Text_ScatterLayer, 1, 1.25f, 8.0f, true);  

    //gcpRendD3D->Set2DMode(false, 1, 1);     
    //gcpRendD3D->SetViewport(GetUtils().m_pScreenRect.left, GetUtils().m_pScreenRect.top, GetUtils().m_pScreenRect.right, GetUtils().m_pScreenRect.bottom); 
    //gcpRendD3D->SetViewport( 0, 0, GetWidth(), GetHeight() );
  }

  return true;
}


bool CD3D9Renderer::FX_ZScene(bool bEnable, bool bUseHDR, bool bClearZBuffer)
{
  if (bEnable)
  {
    if (m_LogFile)
      Logv(SRendItem::m_RecurseLevel, " +++ Start Z scene +++ \n");
    int nWidth = gcpRendD3D->GetWidth(); //m_d3dsdBackBuffer.Width;
    int nHeight = gcpRendD3D->GetHeight(); //m_d3dsdBackBuffer.Height;
    if (!CTexture::m_Text_ZTarget
      || CTexture::m_Text_ZTarget->IsFSAAChanged()
      || CTexture::m_Text_ZTarget->GetDstFormat() != CTexture::m_eTFZ
      || CTexture::m_Text_ZTarget->GetWidth() != nWidth
      || CTexture::m_Text_ZTarget->GetHeight() != nHeight)
      CTexture::GenerateZMaps();
    EF_SetState(GS_DEPTHWRITE);
    EF_ClearBuffers(FRT_CLEAR_COLOR, NULL);
    // Set float render target for Z frame buffer
    FX_PushRenderTarget(0, CTexture::m_Text_ZTarget, &m_DepthBufferOrigFSAA);
    SetViewport(0, 0, GetWidth(), GetHeight());

    if (bClearZBuffer)
    {
      const ColorF cDepthClear(0.0f, m_RP.m_FSAAData.Type? 1.0f :  0.0f, 0.0f);
      EF_ClearBuffers(FRT_CLEAR, &cDepthClear);
    }
    m_RP.m_PersFlags |= RBPF_ZPASS;
    if (CTexture::m_eTFZ == eTF_R32F || CTexture::m_eTFZ == eTF_G16R16F || CTexture::m_eTFZ == eTF_A16B16G16R16F)
      m_RP.m_PersFlags2 |= RBPF2_NOALPHABLEND | RBPF2_NOALPHATEST;
  }
  else
  if (m_RP.m_PersFlags & RBPF_ZPASS)
  {
    if (m_LogFile)
      Logv(SRendItem::m_RecurseLevel, " +++ End Z scene +++ \n");
    FX_PopRenderTarget(0);
    CTexture::m_Text_ZTarget->Resolve();
    SetViewport(0, 0, GetWidth(), GetHeight());
    m_RP.m_PersFlags &= ~RBPF_ZPASS;
    if (CTexture::m_eTFZ == eTF_G16R16F || CTexture::m_eTFZ == eTF_R32F || CTexture::m_eTFZ == eTF_A16B16G16R16F)
      m_RP.m_PersFlags2 &= ~(RBPF2_NOALPHABLEND | RBPF2_NOALPHATEST);
    if (m_RP.m_FSAAData.Type)
    {
      EF_Commit();
    #if defined (DIRECT3D9) || defined(OPENGL)
      m_pd3dDevice->EndScene();
      HRESULT hr = m_pd3dDevice->SetDepthStencilSurface(NULL);
      LPDIRECT3DSURFACE9 pDestSurf = (LPDIRECT3DSURFACE9)m_DepthBufferOrig.pSurf;
      LPDIRECT3DSURFACE9 pSrcSurf = (LPDIRECT3DSURFACE9)m_DepthBufferOrigFSAA.pSurf;
      int nWidth = m_d3dsdBackBuffer.Width;
      int nHeight = m_d3dsdBackBuffer.Height;

      RECT pSrcRect = {0, 0, nWidth, nHeight};
      RECT pDstRect = {0, 0, nWidth, nHeight};

      m_RP.m_PS.m_RTCopied++;
      m_RP.m_PS.m_RTCopiedSize += CTexture::m_Text_ZTarget->GetDeviceDataSize();
      hr = m_pd3dDevice->StretchRect(pSrcSurf, &pSrcRect, pDestSurf, &pDstRect, D3DTEXF_NONE); 
      assert(hr == S_OK);
      m_pd3dDevice->BeginScene();
      hr = m_pd3dDevice->SetDepthStencilSurface(m_pNewTarget[0]->m_pDepth);
    #elif defined (DIRECT3D10)
      m_pd3dDevice->OMSetRenderTargets(1, &m_pNewTarget[0]->m_pTarget, NULL);
      ID3D10Texture2D *pDestSurf = (ID3D10Texture2D *)m_DepthBufferOrig.pTex;
      ID3D10Texture2D *pSrcSurf = (ID3D10Texture2D *)m_DepthBufferOrigFSAA.pTex;

      //m_pd3dDevice->ResolveSubresource(pDestSurf, 0, pSrcSurf, 0, DXUTGetCurrentDeviceSettings()->d3d10.AutoDepthStencilFormat); 

      FX_ResolveDepthTarget(CTexture::m_Text_ZTarget, &m_DepthBufferOrig);

      //m_pd3dDevice->OMSetRenderTargets(1, &m_pNewTarget[0]->m_pTarget, m_pNewTarget[0]->m_pDepth);
    #endif

    }
  }
  else
  if (!CV_r_usezpass)
    CTexture::DestroyZMaps();

  return true;
}

bool CD3D9Renderer::FX_FogScene()
{
  EF_ResetPipe();
  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, " +++ Fog scene +++ \n");
  m_RP.m_PersFlags2 &= ~(RBPF2_SCENEPASS | RBPF2_NOSHADERFOG);

  if (m_FS.m_bEnable && CV_r_usezpass)
  {
    PROFILE_SHADER_START

    m_pNewTarget[0]->m_ClearFlags = 0;
    SetViewport(0, 0, GetWidth(), GetHeight());

   if (m_RP.m_PersFlags2 & RBPF2_HDR_FP16)
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HDR_MODE];

    if (m_RP.m_FSAAData.Type)
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_FSAA];
    if (GetSkyLightRenderParams())
      m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SKYLIGHT_BASED_FOG];

    CShader *pSH = CShaderMan::m_ShaderPostProcess;

    Vec3 vFarPlaneVerts[4];
    UnProjectFromScreen( m_width, m_height, 1, &vFarPlaneVerts[0].x, &vFarPlaneVerts[0].y, &vFarPlaneVerts[0].z);
    UnProjectFromScreen( 0,				m_height, 1, &vFarPlaneVerts[1].x, &vFarPlaneVerts[1].y, &vFarPlaneVerts[1].z);
    UnProjectFromScreen( 0,							 0, 1, &vFarPlaneVerts[2].x, &vFarPlaneVerts[2].y, &vFarPlaneVerts[2].z);
    UnProjectFromScreen( m_width,				 0, 1, &vFarPlaneVerts[3].x, &vFarPlaneVerts[3].y, &vFarPlaneVerts[3].z);

    Vec3 vRT = vFarPlaneVerts[0] - GetCamera().GetPosition();
    Vec3 vLT = vFarPlaneVerts[1] - GetCamera().GetPosition();
    Vec3 vLB = vFarPlaneVerts[2] - GetCamera().GetPosition();
    Vec3 vRB = vFarPlaneVerts[3] - GetCamera().GetPosition();
#if defined (DIRECT3D10) || defined (XENON)
    std::swap(vRT, vRB);
    std::swap(vLT, vLB);
#endif

    static CCryName TechName("FogPass");
    pSH->FXSetTechnique(TechName);

    uint nPasses;
    pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
    pSH->FXBeginPass(0);

    STexState TexStatePoint = STexState(FILTER_POINT, true);
    CTexture::m_Text_ZTarget->Apply(0, CTexture::GetTexState(TexStatePoint));

    float fWidth5 = (float)CTexture::m_Text_ZTarget->GetWidth()-0.5f;
    float fHeight5 = (float)CTexture::m_Text_ZTarget->GetHeight()-0.5f;

    int nOffs;
    struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F *Verts = (struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F *)GetVBPtr(4, nOffs, POOL_TRP3F_TEX2F_TEX3F);
    if (Verts)
    {
#ifdef XENON
      struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F SysVB[4];
      struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F *pDst = Verts;
      Verts = SysVB;
#endif

#if defined (DIRECT3D10) || defined (XENON)
      float t = 1;
#else
			float t = 0;
#endif
      Verts[0].p = Vec4(-0.5f, -0.5f, 0.0f, 1.0f);
      Verts[0].st0 = Vec2(0, t);
      Verts[0].st1 = vLT;

      Verts[1].p = Vec4(fWidth5, -0.5f, 0.0f, 1.0f);
      Verts[1].st0 = Vec2(1, t);
			Verts[1].st1 = vRT;

      Verts[2].p = Vec4(-0.5f, fHeight5, 0.0f, 1.0f);
      Verts[2].st0 = Vec2(0, 1-t);
			Verts[2].st1 = vLB;

			Verts[3].p = Vec4(fWidth5, fHeight5, 0.0f, 1.0f);
			Verts[3].st0 = Vec2(1, 1-t);
			Verts[3].st1 = vRB;

#ifdef XENON
      memcpy(pDst, SysVB, sizeof(SysVB));
#endif
    }
    UnlockVB(POOL_TRP3F_TEX2F_TEX3F);

    EF_Commit();

    // Draw a fullscreen quad to sample the RT
    EF_SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

    if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_TRP3F_TEX2F_TEX3F)))
    {
      FX_SetVStream(0, m_pVB[POOL_TRP3F_TEX2F_TEX3F], 0, sizeof(struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F));
  #if defined (DIRECT3D9) || defined(OPENGL)
      m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
  #elif defined (DIRECT3D10)
      SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      m_pd3dDevice->Draw(4, nOffs);
  #endif
    }
    pSH->FXEndPass();

		//////////////////////////////////////////////////////////////////////////

		Vec3 lCol;
		iSystem->GetI3DEngine()->GetGlobalParameter( E3DPARAM_SKY_HIGHLIGHT_COLOR, lCol );

		bool useFogPassWithLightning(lCol.x > 1e-4f || lCol.y > 1e-4f || lCol.z > 1e-4f);
		if (useFogPassWithLightning)
		{
			static CCryName TechName("FogPassWithLightning");
			if (pSH->FXSetTechnique(TechName))
			{
				uint nPasses;
				pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
				pSH->FXBeginPass(0);

				Vec3 lPos;
				iSystem->GetI3DEngine()->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_POS, lPos);
				Vec4 lightningPosition(lPos.x, lPos.y, lPos.z, 0.0f);
				static CCryName Param1Name("LightningPos");
				pSH->FXSetPSFloat(Param1Name, &lightningPosition, 1);

				Vec3 lSize;
				iSystem->GetI3DEngine()->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_SIZE, lSize);
				Vec4 lightningColorSize(lCol.x, lCol.y, lCol.z, lSize.x * 0.01f);
				static CCryName Param2Name("LightningColSize");
				pSH->FXSetPSFloat(Param2Name, &lightningColorSize, 1);

				EF_Commit();

				EF_SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);

#if defined (DIRECT3D9) || defined(OPENGL)
				m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nOffs, 2);
#elif defined (DIRECT3D10)
				SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				m_pd3dDevice->Draw(4, nOffs);
#endif
				pSH->FXEndPass();
			}
		}

		//////////////////////////////////////////////////////////////////////////

    EF_SelectTMU(0);

    PROFILE_SHADER_END
  }

  // make sure to disable
  if (GetSkyLightRenderParams())
    m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SKYLIGHT_BASED_FOG];

  return true;
}

//================================================================================

void CD3D9Renderer::EF_InitLightInfotable_DB()
{
  int i, j;
  for (i=0; i<256; i++)
  {
    m_RP.m_LightInfo[i][0] = -1.0f;
    m_RP.m_LightInfo[i][1] = -1.0f;
    m_RP.m_LightInfo[i][3] = 1.0f / 64.0f;
    m_RP.m_LightInfo[i][2] = m_RP.m_LightInfo[i][3] * 4.0f;

    int nID[4];
    for (j=0; j<4; j++)
    {
      nID[j] = (i>>(2*j) & 3);
    }
    if (!nID[1] && !nID[2] && !nID[3])
    {
      m_RP.m_LightInfo[i][0] = nID[0] * m_RP.m_LightInfo[i][2];
      continue;
    }
    if (!nID[2] && !nID[3])
    {
      if (nID[1] == nID[0]+1)
        m_RP.m_LightInfo[i][0] = nID[0] * m_RP.m_LightInfo[i][2];
      else
      if (nID[1] == nID[0]+2)
      {
        m_RP.m_LightInfo[i][0] = nID[0] * m_RP.m_LightInfo[i][2];
        m_RP.m_LightInfo[i][2] *= 2;
      }
      else
      if (nID[1] == nID[0]+3)
      {
        m_RP.m_LightInfo[i][0] = nID[0] * m_RP.m_LightInfo[i][2];
        m_RP.m_LightInfo[i][2] *= 3;
      }
    }
    else
    if (!nID[3])
    {
      if (nID[1]==nID[0]+1 && nID[2]==nID[1]+1)
        m_RP.m_LightInfo[i][0] = nID[0] * m_RP.m_LightInfo[i][2];
      else
      if (nID[1]==nID[0]+2 && nID[2]==nID[1]+1)
        m_RP.m_LightInfo[i][0] = m_RP.m_LightInfo[i][2] * 4;
      else
      if (nID[1]==nID[0]+1 && nID[2]==nID[1]+2)
        m_RP.m_LightInfo[i][0] = m_RP.m_LightInfo[i][2] * 8;
    }
    else
    if (nID[1]==nID[0]+1 && nID[2]==nID[1]+1 && nID[3]==nID[2]+1)
      m_RP.m_LightInfo[i][0] = 0;
  }
}


bool CD3D9Renderer::FX_PrepareLightInfoTexture(bool bEnable)
{
  if (bEnable)
  {
    int nr = SRendItem::m_RecurseLevel;
    if (!m_RP.m_DLights[nr].Num())
      return true;
    if (CV_r_shadersdynamicbranching == 4)
      return true;
    if (m_LogFile)
      Logv(SRendItem::m_RecurseLevel, " +++ Prepare LightInfo texture +++ \n");
    ETEX_Format eTF = eTF_A32B32G32R32F;
    // 4 sets by 4 lights, 4 vectors per light
    int nWidth = 4*4*4;
    // 8 light groups (32 lights - maximum)
    int nHeight = 8;
    bool bGenerate = false;
    if (!CTexture::m_Text_LightInfo[nr-1]->GetDeviceTexture())
      bGenerate = true;
    if (CV_r_shadersdynamicbranching == 2 && !(CTexture::m_Text_LightInfo[nr-1]->GetFlags() & FT_USAGE_DYNAMIC))
      bGenerate = true;
    else
    if (!(CTexture::m_Text_LightInfo[nr-1]->GetFlags() & FT_USAGE_RENDERTARGET))
      bGenerate = true;
    if (bGenerate)
      CTexture::GenerateLightInfo(eTF, nWidth, nHeight);

    int i, j, n;
    static byte index[4][4] = 
    {
      {0,1,2,3}, {0,2,3,1}, {0,1,3,2}, {0,0,0,0}
    };

    Vec3 CameraPos = m_rcam.Orig;
    if (CV_r_shadersdynamicbranching == 2)
    {
      // Update light info on CPU
      int nPitch;
      Vec4 *pData = (Vec4 *)CTexture::m_Text_LightInfo[nr-1]->LockData(nPitch);
      assert(nPitch == nWidth*sizeof(Vec4));
      for (i=0; i<8; i++)
      {
        for (j=0; j<4; j++)
        {
          for (n=0; n<4; n++)
          {
            int nIdInGroup = index[j][n];
            int nL = i*4+nIdInGroup;
            CDLight *dl = NULL;
            if (nL < m_RP.m_DLights[nr].Num() && (dl=m_RP.m_DLights[nr][nL]) != NULL)
            {
              Vec3 vPos = dl->m_Origin - CameraPos;
              pData[0] = Vec4(vPos.x, vPos.y, vPos.z, 1.0f/dl->m_fRadius);
              pData[1] = dl->m_Color.toVec4();
              pData[2] = Vec4(0,0,0,0);
              float fType = 0;
              if (dl->m_Flags & DLF_POINT)
                fType = 1;
              else
              if (dl->m_Flags & DLF_PROJECT)
                fType = 2;
              pData[2].w = fType;
              pData[3] = Vec4(0,0,0,0);
              pData[3][nIdInGroup] = 1;
            }
            else
            {
#ifdef _DEBUG
              // Non-used light
              pData[0] = Vec4(0,0,0,0);
              pData[1] = Vec4(0,0,0,0);
              pData[2] = Vec4(0,0,0,0);
              pData[3] = Vec4(0,0,0,0);
#endif
            }
            pData += 4;
          }
        }
      }
      CTexture::m_Text_LightInfo[nr-1]->UnlockData();
    }
    else
    {
      // Update light info on GPU
      int iTempX, iTempY, iWidth, iHeight;
      GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
      SetViewport(0, 0, nWidth, nHeight);

      // Update LightInfo
      EF_ResetPipe();

      FX_PushRenderTarget(0, CTexture::m_Text_LightInfo[nr-1], &m_DepthBufferOrig);
      ColorF Black(Col_Black);
      EF_ClearBuffers(FRT_CLEAR_COLOR, &Black);

      m_RP.m_PersFlags2 &= ~(RBPF2_SCENEPASS | RBPF2_NOSHADERFOG);
      CShader *pSH = CShaderMan::m_ShaderPostProcess;
      pSH->FXSetTechnique("LightInfo");
      uint nPasses;
      pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
      pSH->FXBeginPass(0);
      SShaderPass *pPass = m_RP.m_pCurPass;
      assert (pPass->m_VShader && pPass->m_PShader);
      if (pPass->m_VShader && pPass->m_PShader)
      {
        CHWShader_D3D *curVS = (CHWShader_D3D *)pPass->m_VShader;
        int nGroups = (m_RP.m_DLights[nr].Num()-1) / 4 + 1;
        int nPoints = nGroups*4*4*4;
        int nOffs;
        struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F *Verts = (struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F *)GetVBPtr(nPoints, nOffs, POOL_TRP3F_TEX2F_TEX3F);
        struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F *OrigVerts = Verts;

        Vec4 vData;
        if (Verts)
        {
          for (i=0; i<8; i++)
          {
            if (i >= nGroups)
              break;
            for (j=0; j<4; j++)
            {
              for (n=0; n<4; n++)
              {
                int nIdInGroup = index[j][n];
                int nL = i*4+nIdInGroup;
                CDLight *dl = NULL;
                Vec4 v = Vec4((float)((j*4+n)*4),(float)i,1,1);
                if (nL < m_RP.m_DLights[nr].Num() && (dl=m_RP.m_DLights[nr][nL]) != NULL)
                {
                  Verts[0].p = v; v.x += 1;
                  Vec3 vPos;
                  if (dl->m_Flags & DLF_DIRECTIONAL)
                    vPos = iSystem->GetI3DEngine()->GetSunDirNormalized();
                  else
                    vPos = dl->m_Origin - CameraPos;
                  Verts[0].st0 = Vec2(vPos.x, vPos.y); Verts[0].st1 = Vec3(vPos.z, 1.0f/dl->m_fRadius, 0);

                  Verts[1].p = v; v.x += 1;
                  vData = dl->m_Color.toVec4();
                  Verts[1].st0 = Vec2(vData.x, vData.y); Verts[1].st1 = Vec3(vData.z, vData.w, 0);
                  vData = Vec4(0,0,0,0);
                  float fType = 0;
                  if (dl->m_Flags & DLF_POINT)
                    fType = 1;
                  else
                  if (dl->m_Flags & DLF_PROJECT)
                    fType = 2;
                  vData.w = fType;

                  Verts[2].p = v; v.x += 1;
                  Verts[2].st0 = Vec2(vData.x, vData.y); Verts[2].st1 = Vec3(vData.z, vData.w, 0);

                  Verts[3].p = v; v.x += 1;
                  vData = Vec4(0, 0, 0, 0);
                  vData[nIdInGroup] = 1;
                  Verts[3].st0 = Vec2(vData.x, vData.y); Verts[3].st1 = Vec3(vData.z, vData.w, 0);
                }
                else
                {
                  // Non-used light
                  Verts[0].p = v; v.x += 1;
                  Verts[1].p = v; v.x += 1;
                  Verts[2].p = v; v.x += 1;
                  Verts[3].p = v; v.x += 1;
                }
                Verts += 4;
              }
            }
          }

          UnlockVB(POOL_TRP3F_TEX2F_TEX3F);

          EF_Commit();

          // Draw a fullscreen quad to sample the RT
          EF_SetState(GS_NODEPTHTEST);

          if (!FAILED(EF_SetVertexDeclaration(0, VERTEX_FORMAT_TRP3F_TEX2F_TEX3F)))
          {
            FX_SetVStream(0, m_pVB[POOL_TRP3F_TEX2F_TEX3F], 0, sizeof(struct_VERTEX_FORMAT_TRP3F_TEX2F_TEX3F));
  #if defined (DIRECT3D9) || defined(OPENGL)
            HRESULT hr = m_pd3dDevice->DrawPrimitive(D3DPT_POINTLIST, nOffs, nPoints);
  #elif defined (DIRECT3D10)
            assert(0);
  #endif
          }
          pSH->FXEndPass();

          EF_SelectTMU(0);
        }
        pSH->FXEnd();
      }
      FX_PopRenderTarget(0);
      SetViewport(iTempX, iTempY, iWidth, iHeight);
    }
  }
  else
    CTexture::DestroyLightInfo();
  return true;
}

bool CD3D9Renderer::FX_HDRScene(bool bEnableHDR)
{
#ifdef USE_HDR
  if (m_nHDRType != CV_r_hdrtype)
  {
    if (CV_r_hdrtype <= 0)
      CV_r_hdrtype = 1;
    else
      if (CV_r_hdrtype > 3)
        CV_r_hdrtype = 3;
    if (m_nHDRType != CV_r_hdrtype)
    {
      m_nHDRType = CV_r_hdrtype;
      SAFE_RELEASE(CTexture::m_Text_HDRTarget);
    }
  }
  if (bEnableHDR)
  {
    if (m_LogFile)
      Logv(SRendItem::m_RecurseLevel, " +++ Start HDR scene +++ \n");
    SShaderItem shItem(CShaderMan::m_ShaderPostProcess);
    CRenderObject *pObj = EF_GetObject(true);
    if (pObj)
    {
      pObj->m_II.m_Matrix.SetIdentity();
      EF_AddEf(m_RP.m_pREHDR, shItem, pObj, EFSLIST_HDRPOSTPROCESS, 0);

      bool bSceneTargetFSAA = gcpRendD3D->m_RP.m_FSAAData.Type && CRenderer::CV_r_debug_extra_scenetarget_fsaa;
      if (!CTexture::m_Text_HDRTarget || CTexture::m_Text_HDRTarget->IsFSAAChanged() || CTexture::m_Text_HDRTarget->GetWidth() != GetWidth() || CTexture::m_Text_HDRTarget->GetHeight() != GetHeight() || 
        bSceneTargetFSAA && (CTexture::m_Text_SceneTargetFSAA->IsFSAAChanged() || CTexture::m_Text_SceneTargetFSAA->GetWidth() != GetWidth() || CTexture::m_Text_SceneTargetFSAA->GetHeight() != GetHeight()))
        CTexture::GenerateHDRMaps();

      // Set float render target for HDR frame buffer
      CTexture *tp = CTexture::m_Text_HDRTarget;
      FX_PushRenderTarget(0, CTexture::m_Text_HDRTarget, &m_DepthBufferOrigFSAA);
      SetViewport(0, 0, GetWidth(), GetHeight());
      EF_ClearBuffers(FRT_CLEAR, NULL);
      m_RP.m_PersFlags |= RBPF_HDR;
    }
  }
  else
    if (!CV_r_hdrrendering && CTexture::m_Text_HDRTarget)
    {
      if (m_LogFile)
        Logv(SRendItem::m_RecurseLevel, " +++ End HDR scene +++ \n");
      CTexture::DestroyHDRMaps();
    }
    return true;
#endif
    return false;
}

// Draw overlay geometry in wireframe mode
void CD3D9Renderer::EF_DrawWire()
{
  if (CV_r_showlines == 1)
    gcpRendD3D->EF_SetState(GS_WIREFRAME | GS_NODEPTHTEST);
  else
    if (CV_r_showlines == 2)
      gcpRendD3D->EF_SetState(GS_WIREFRAME);
  gcpRendD3D->SetMaterialColor(1,1,1,1);
  CTexture::m_Text_White->Apply();
  gcpRendD3D->EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, (eCA_Texture|(eCA_Constant<<3)), (eCA_Texture|(eCA_Constant<<3)));
  gcpRendD3D->EF_SetObjectTransform(gcpRendD3D->m_RP.m_pCurObject, NULL, gcpRendD3D->m_RP.m_pCurObject->m_ObjFlags);
  CRenderObject *pObj = gcpRendD3D->m_RP.m_pCurObject;
  gcpRendD3D->FX_SetFPMode();
  gcpRendD3D->m_RP.m_pCurObject = pObj;
  gcpRendD3D->m_RP.m_pCurInstanceInfo = &gcpRendD3D->m_RP.m_pCurObject->m_II;
  if (gcpRendD3D->m_RP.m_pRE)
    gcpRendD3D->m_RP.m_pRE->mfCheckUpdate(gcpRendD3D->m_RP.m_pShader->m_VertexFormatId, 0);

  gcpRendD3D->EF_DrawBatch(gcpRendD3D->m_RP.m_pShader, NULL);
}

// Draw geometry normal vectors
void CD3D9Renderer::EF_DrawNormals()
{
  HRESULT h = S_OK;

  float len = CRenderer::CV_r_normalslength;
  int StrVrt, StrNrm;
  //if (gcpRendD3D->m_RP.m_pRE)
  //  gcpRendD3D->m_RP.m_pRE->mfCheckUpdate(gcpRendD3D->m_RP.m_pShader->m_VertexFormatId, SHPF_NORMALS);
  byte *verts = (byte *)gcpRendD3D->EF_GetPointer(eSrcPointer_Vert, &StrVrt, eType_FLOAT, eSrcPointer_Vert, FGP_SRC | FGP_REAL);
  byte *norms = (byte *)gcpRendD3D->EF_GetPointer(eSrcPointer_Normal, &StrNrm, eType_FLOAT, eSrcPointer_Normal, FGP_SRC | FGP_REAL);
  if ((INT_PTR)norms > 256 && (INT_PTR)verts > 256)
  {
    gcpRendD3D->EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB);
    gcpRendD3D->EF_SetObjectTransform(gcpRendD3D->m_RP.m_pCurObject, NULL, gcpRendD3D->m_RP.m_pCurObject->m_ObjFlags);
    gcpRendD3D->EF_SetColorOp(eCO_REPLACE, eCO_REPLACE, (eCA_Diffuse|(eCA_Diffuse<<3)), (eCA_Diffuse|(eCA_Diffuse<<3)));
    gcpRendD3D->FX_SetFPMode();
    //gcpRendD3D->m_pd3dDevice->SetVertexShader(NULL);
#if defined (DIRECT3D9) || defined(OPENGL)
    gcpRendD3D->m_pd3dDevice->SetTransform(D3DTS_VIEW, (D3DXMATRIX*)gcpRendD3D->m_matView->GetTop());
#elif defined (DIRECT3D10)
    assert(0);
#endif

    int numVerts = gcpRendD3D->m_RP.m_RendNumVerts;

    gcpRendD3D->EF_SetState(0);
    struct_VERTEX_FORMAT_P3F_COL4UB *Verts = new struct_VERTEX_FORMAT_P3F_COL4UB[numVerts*2];

    uint col0 = 0x000000ff;
    uint col1 = 0x00ffffff;

    for (int v=0; v<numVerts*2; v+=2,verts+=StrVrt,norms+=StrNrm)
    {
      float *fverts = (float *)verts;
      Vec3 vNorm = Vec3((norms[0]-128.0f)/127.5f, (norms[1]-128.0f)/127.5f, (norms[2]-128.0f)/127.5f);

      Verts[v].xyz.x = fverts[0];
      Verts[v].xyz.y = fverts[1];
      Verts[v].xyz.z = fverts[2];
      Verts[v].color.dcolor = col0;

      Verts[v+1].xyz.x = fverts[0] + vNorm[0]*len;
      Verts[v+1].xyz.y = fverts[1] + vNorm[1]*len;
      Verts[v+1].xyz.z = fverts[2] + vNorm[2]*len;
      Verts[v+1].color.dcolor = col1;
    }
#if defined (DIRECT3D9) || defined(OPENGL)
    h = gcpRendD3D->m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST, numVerts, Verts, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB));
#elif defined (DIRECT3D10)
    assert(0);
#endif

    delete [] Verts;
    gcpRendD3D->m_RP.m_VertexStreams[0].pStream = NULL;
  }
}

// Draw geometry tangent vectors
void CD3D9Renderer::EF_DrawTangents()
{
  HRESULT h = S_OK;

  float len = CRenderer::CV_r_normalslength;
  //if (gcpRendD3D->m_RP.m_pRE)
  //  gcpRendD3D->m_RP.m_pRE->mfCheckUpdate(gcpRendD3D->m_RP.m_pShader->m_VertexFormatId, SHPF_TANGENTS);
  int StrVrt, StrTang, StrBinorm;
  byte *verts = NULL;
  byte *tangs = NULL;
  byte *binorm = NULL;
  int flags = 0;
  if (CRenderer::CV_r_showtangents == 1)
    flags = FGP_SRC | FGP_REAL;
  else
    flags = FGP_REAL;
  verts = (byte *)gcpRendD3D->EF_GetPointer(eSrcPointer_Vert, &StrVrt, eType_FLOAT, eSrcPointer_Vert, flags);
  tangs = (byte *)gcpRendD3D->EF_GetPointer(eSrcPointer_Tangent, &StrTang, eType_BYTE, eSrcPointer_Tangent, flags);
  binorm = (byte *)gcpRendD3D->EF_GetPointer(eSrcPointer_Binormal, &StrBinorm, eType_BYTE, eSrcPointer_Binormal, flags);
  if ((INT_PTR)tangs>256 && (INT_PTR)binorm>256)
  {
    gcpRendD3D->EF_SetObjectTransform(gcpRendD3D->m_RP.m_pCurObject, NULL, gcpRendD3D->m_RP.m_pCurObject->m_ObjFlags);

    int numVerts = gcpRendD3D->m_RP.m_RendNumVerts;

    CTexture::m_Text_White->Apply();
    gcpRendD3D->EF_SetColorOp(eCO_REPLACE, eCO_REPLACE, (eCA_Diffuse|(eCA_Diffuse<<3)), (eCA_Diffuse|(eCA_Diffuse<<3)));
    if (gcpRendD3D->m_polygon_mode == R_SOLID_MODE)
      gcpRendD3D->EF_SetState(GS_DEPTHWRITE);
    else
      gcpRendD3D->EF_SetState(0);
    gcpRendD3D->FX_SetFPMode();

#if defined (DIRECT3D9) || defined(OPENGL)
    gcpRendD3D->m_pd3dDevice->SetTransform(D3DTS_VIEW, (D3DXMATRIX*)gcpRendD3D->m_matView->GetTop());
#elif defined (DIRECT3D10)
    assert(0);
#endif

    struct_VERTEX_FORMAT_P3F_COL4UB *Verts = new struct_VERTEX_FORMAT_P3F_COL4UB[numVerts*6];

    for (int v=0; v<numVerts; v++,verts+=StrVrt,tangs+=StrTang, binorm+=StrBinorm)
    {
      uint col0 = 0xffff0000;
      uint col1 = 0xffffffff;
      float *fverts = (float *)verts;
      int16f *fv = (int16f *)tangs;
      Vec3 vTang = Vec3(tPackB2F(fv[0]), tPackB2F(fv[1]), tPackB2F(fv[2]));
      Verts[v*6+0].xyz.x = fverts[0];
      Verts[v*6+0].xyz.y = fverts[1];
      Verts[v*6+0].xyz.z = fverts[2];
      Verts[v*6+0].color.dcolor = col0;

      Verts[v*6+1].xyz.x = fverts[0] + vTang[0]*len;
      Verts[v*6+1].xyz.y = fverts[1] + vTang[1]*len;
      Verts[v*6+1].xyz.z = fverts[2] + vTang[2]*len;
      Verts[v*6+1].color.dcolor = col1;

      col0 = 0x0000ff00;
      col1 = 0x00ffffff;
      fverts = (float *)verts;
      int16f *fv1 = (int16f *)binorm;
      Vec3 vBinorm = Vec3(tPackB2F(fv1[0]), tPackB2F(fv1[1]), tPackB2F(fv1[2]));

      Verts[v*6+2].xyz.x = fverts[0];
      Verts[v*6+2].xyz.y = fverts[1];
      Verts[v*6+2].xyz.z = fverts[2];
      Verts[v*6+2].color.dcolor = col0;

      Verts[v*6+3].xyz.x = fverts[0] + vBinorm[0]*len;
      Verts[v*6+3].xyz.y = fverts[1] + vBinorm[1]*len;
      Verts[v*6+3].xyz.z = fverts[2] + vBinorm[2]*len;
      Verts[v*6+3].color.dcolor = col1;

      col0 = 0x000000ff;
      col1 = 0x00ffffff;
      fverts = (float *)verts;
      Vec3 vTNorm = (vTang ^ vBinorm) * tPackB2F(fv[3]);

      Verts[v*6+4].xyz.x = fverts[0];
      Verts[v*6+4].xyz.y = fverts[1];
      Verts[v*6+4].xyz.z = fverts[2];
      Verts[v*6+4].color.dcolor = col0;

      Verts[v*6+5].xyz.x = fverts[0] + vTNorm[0]*len;
      Verts[v*6+5].xyz.y = fverts[1] + vTNorm[1]*len;
      Verts[v*6+5].xyz.z = fverts[2] + vTNorm[2]*len;
      Verts[v*6+5].color.dcolor = col1;
    }
    gcpRendD3D->EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB);
#if defined (DIRECT3D9) || defined(OPENGL)
    h = gcpRendD3D->m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST, numVerts*3, Verts, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB));
#elif defined (DIRECT3D10)
    assert(0);
#endif
    delete [] Verts;
    gcpRendD3D->m_RP.m_VertexStreams[0].pStream = NULL;
  }
}

// Draw light sources in debug mode
void CD3D9Renderer::EF_DrawDebugLights()
{
  static int sFrame = 0;
  ResetToDefault();
  GetIRenderAuxGeom()->SetRenderFlags(e_DepthTestOff | e_DepthWriteOff | e_CullModeNone | e_FillModeSolid | e_AlphaBlended);
  if (m_nFrameUpdateID != sFrame)
  {
    uint i;
    sFrame = m_nFrameID;

    CTexture::m_Text_White->Apply();

    for (i=0; i<m_RP.m_DLights[SRendItem::m_RecurseLevel].Num(); i++)
    {
      CDLight *dl = m_RP.m_DLights[SRendItem::m_RecurseLevel][i];
      if (!dl)
        continue;
      ColorF colF = dl->m_Color;
      colF.NormalizeCol(colF);
      ColorB col = ColorB(colF);
      col.a = 0x3f;
      SetMaterialColor(dl->m_Color[0], dl->m_Color[1], dl->m_Color[2], dl->m_Color[3]);
      if (dl->m_Flags & DLF_DIRECTIONAL)
        GetIRenderAuxGeom()->DrawSphere(dl->m_Origin, 0.02f, col);
      else
      if (dl->m_Flags & DLF_POINT)
        GetIRenderAuxGeom()->DrawSphere(dl->m_Origin, 0.05f, col);
      if (dl->m_Flags & DLF_PROJECT)
      {
        Vec3 dir, rgt, org;
        EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
        EF_SetVertexDeclaration(0, VERTEX_FORMAT_P3F_COL4UB);

        //dir = dl->m_Orientation.m_vForward;
        //rgt = dl->m_Orientation.m_vRight;
        float ang = dl->m_fLightFrustumAngle;          
        if (ang == 0)
          ang = 45.0f;
        org = dl->m_Origin;

        dir *= 0.3f;

        ColorF Col = dl->m_Color;

        Matrix44 m;
        Vec3 vertex = dir;

        vertex = Matrix33::CreateRotationAA(DEG2RAD(ang),rgt.GetNormalized()) * vertex; //NOTE: angle need to be in radians
        Matrix44 mat = Matrix33::CreateRotationAA(DEG2RAD(60),dir.GetNormalized()); //NOTE: angle need to be in radians
        Vec3 tmpvertex;
        int ctr;

        //fill the inside of the light
        EnableTMU(false);
        struct_VERTEX_FORMAT_P3F_COL4UB Verts[32];
        memset(Verts, 0, sizeof(Verts));
        ColorF cl = Col*0.3f;
        int n = 0;
        Verts[n].xyz = org;
        Verts[n].color.dcolor = D3DRGBA(cl[0], cl[1], cl[2], 1.0f);
        n++;
        tmpvertex = org + vertex;
        Verts[n].xyz = tmpvertex;
        Verts[n].color.dcolor = D3DRGBA(Col[0], Col[1], Col[2], 1.0f);
        n++;
        for (ctr=0; ctr<6; ctr++)
        {
          vertex = mat.TransformVector(vertex);
          Vec3 tmpvertex = org + vertex;
          Verts[n].xyz = tmpvertex;
          Verts[n].color.dcolor = D3DRGBA(Col[0], Col[1], Col[2], 1.0f);
          n++;
        }
#if defined (DIRECT3D9) || defined(OPENGL)
        m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, n-2, Verts, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB));
        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
#elif defined (DIRECT3D10)
        assert(0);
#endif

        //draw the inside of the light with lines and the outside filled
        SetCullMode(R_CULL_NONE);
        n = 0;
        Verts[n].xyz = org;
        Verts[n].color.dcolor = D3DRGBA(0.3f, 0.3f, 0.3f, 1.0f);
        n++;
        tmpvertex = org + vertex;
        Verts[n].xyz = tmpvertex;
        Verts[n].color.dcolor = D3DRGBA(1.0f, 1.0f, 1.0f, 1.0f);
        n++;
        for (ctr=0; ctr<6; ctr++)
        {
          vertex = mat.TransformVector(vertex);
          Vec3 tmpvertex = org + vertex;
          Verts[n].xyz = tmpvertex;
          Verts[n].color.dcolor = D3DRGBA(1.0f, 1.0f, 1.0f, 1.0f);
          n++;
        }
#if defined (DIRECT3D9) || defined(OPENGL)
        m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, n-2, Verts, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB));
        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
#elif defined (DIRECT3D10)
        assert(0);
#endif
        SetCullMode(R_CULL_FRONT);

        //set the color to the color of the light
        Verts[0].xyz = org;
        Verts[0].color.dcolor = D3DRGBA(Col[0], Col[1], Col[2], 1.0f);

        //draw a point at the origin of the light
#if defined (DIRECT3D9) || defined(OPENGL)
        m_pd3dDevice->DrawPrimitiveUP(D3DPT_POINTLIST, 1, Verts, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB));
#elif defined (DIRECT3D10)
        assert(0);
#endif
        //draw a line in the center of the light
        Verts[0].xyz = org;
        Verts[0].color.dcolor = D3DRGBA(Col[0], Col[1], Col[2], 1.0f);

        tmpvertex = org + dir;
        Verts[1].xyz = tmpvertex;
        Verts[1].color.dcolor = D3DRGBA(Col[0], Col[1], Col[2], 1.0f);
#if defined (DIRECT3D9) || defined(OPENGL)
        m_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, Verts, sizeof(struct_VERTEX_FORMAT_P3F_COL4UB));
#elif defined (DIRECT3D10)
        assert(0);
#endif
        EnableTMU(true);
      }
      if (CV_r_debuglights >= 2 && !(dl->m_Flags & DLF_DIRECTIONAL))
      {
        EF_SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
        SetCullMode(R_CULL_NONE);
        SetMaterialColor(dl->m_Color[0], dl->m_Color[1], dl->m_Color[2], 0.25f);
        GetIRenderAuxGeom()->DrawSphere(dl->m_Origin, dl->m_fRadius, col);

				if(CV_r_debuglights >= 3)
          DrawLabel(dl->m_Origin, 1.5f, "(%.2f %.2f %.2f)\n(%.2f %.2f %.2f)\nHDRDyn:%.2f S:%.2f F:%x Sty:%d\nName:%s DistRatio:%d",
					dl->m_BaseColor[0], dl->m_BaseColor[1], dl->m_BaseColor[2],
					dl->m_Color[0], dl->m_Color[1], dl->m_Color[2],
					dl->m_fHDRDynamic,dl->m_SpecMult,dl->m_Flags,dl->m_nLightStyle,
          dl->m_sName ? dl->m_sName : "Unknown",
          dl->m_pOwner ? dl->m_pOwner->GetViewDistRatio() : -1);
      }
    }
  }
  gcpRendD3D->m_RP.m_VertexStreams[0].pStream = NULL;
}

// Draw debug geometry/info
void CD3D9Renderer::EF_DrawDebugTools()
{
  if (CV_r_showlines)
    FX_ProcessAllRenderLists(EF_DrawWire, 0);

  if (CV_r_shownormals)
    FX_ProcessAllRenderLists(EF_DrawNormals, 0);

  if (CV_r_showtangents)
    FX_ProcessAllRenderLists(EF_DrawTangents, 0);

  if (SRendItem::m_RecurseLevel==1 && CV_r_debuglights)
    EF_DrawDebugLights();
}


static int __cdecl TimeProfCallback( const VOID* arg1, const VOID* arg2 )
{
  SProfInfo *pi1 = (SProfInfo *)arg1;
  SProfInfo *pi2 = (SProfInfo *)arg2;
  if (pi1->pTechnique->m_fProfileTime > pi2->pTechnique->m_fProfileTime)
    return -1;
  if (pi1->pTechnique->m_fProfileTime < pi2->pTechnique->m_fProfileTime)
    return 1;
  return 0;
}

static int __cdecl NameCallback( const VOID* arg1, const VOID* arg2 )
{
  SProfInfo *pi1 = (SProfInfo *)arg1;
  SProfInfo *pi2 = (SProfInfo *)arg2;
  if (pi1->pTechnique > pi2->pTechnique)
    return -1;
  if (pi1->pTechnique < pi2->pTechnique)
    return 1;
  return 0;
}

//#include "FMallocWindows.h"

/*struct vertex
{
  Vec3 position;
  Vec3 normal;
  Vec4 color;
};
vertex mesh1[1000];
vertex mesh2[1000];

__declspec(noinline) 
void rotate_mesh(const vertex *from, vertex *to, int size, Matrix33 m, Vec3 offset)
{
  for (int i=0; i<size; ++i)
  {
    Vec3 pos = from[i].position;
    to[i].position = pos.x * m.GetRow(0) + pos.y * m.GetRow(1) + pos.z * m.GetRow(2) + offset;			
    Vec3 normal = from[i].normal;
    to[i].position = normal.x * m.GetRow(0) + normal.y * m.GetRow(1) + normal.z * m.GetRow(2);
    to[i].color = from[i].color;
  }
}*/


// Print shaders profile info on the screen
void CD3D9Renderer::EF_PrintProfileInfo()
{
  double fTime = 0;
  uint i;
  if (CV_r_profileshaders)
  { // group by name
    if(!m_RP.m_Profile.empty())
      qsort(&m_RP.m_Profile[0], m_RP.m_Profile.Num(), sizeof(SProfInfo), NameCallback );

    for(i=0; i<m_RP.m_Profile.Num(); i++)
    {
      // if next is the same
      if( i<(m_RP.m_Profile.Num()-1) && m_RP.m_Profile[i].pTechnique == m_RP.m_Profile[i+1].pTechnique )
      {
        m_RP.m_Profile[i].Time += m_RP.m_Profile[i+1].Time;
        m_RP.m_Profile[i].m_nItems++;
        m_RP.m_Profile[i].NumPolys += m_RP.m_Profile[i+1].NumPolys;
        m_RP.m_Profile[i].NumDips += m_RP.m_Profile[i+1].NumDips;
        m_RP.m_Profile.DelElem(i+1);
        i--;
      }
    }
  }
  for (i=0; i<m_RP.m_Profile.Num(); i++)
  {
    m_RP.m_Profile[i].pTechnique->m_fProfileTime = 
			(float)(m_RP.m_Profile[i].Time + m_RP.m_Profile[i].pTechnique->m_fProfileTime*(float)CV_r_profileshaderssmooth)/((float)CV_r_profileshaderssmooth+1);
  }

  TextToScreenColor(1,14, 0,2,0,1, "Instances: %d, Batches: %d, DrawCalls: %d, Text: %d, Stat: %d, PShad: %d, VShad: %d",  m_RP.m_PS.m_NumRendInstances, m_RP.m_PS.m_NumRendBatches, GetCurrentNumberOfDrawCalls(), m_RP.m_PS.m_NumTextChanges, m_RP.m_PS.m_NumStateChanges, m_RP.m_PS.m_NumPShadChanges, m_RP.m_PS.m_NumVShadChanges);
  TextToScreenColor(1,17, 0,2,0,1, "VShad: %d, PShad: %d, Text: %d",  m_RP.m_PS.m_NumVShaders, m_RP.m_PS.m_NumPShaders, m_RP.m_PS.m_NumTextures);
  TextToScreenColor(1,20, 0,2,0,1, "Preprocess: %8.02f ms, Occl. queries: %8.02f ms",  m_RP.m_PS.m_fPreprocessTime, m_RP.m_PS.m_fOcclusionTime);
  TextToScreenColor(1,23, 0,2,0,1, "Skinning:   %8.02f ms (Skinned Objects: %d)",  m_RP.m_PS.m_fSkinningTime, m_RP.m_PS.m_NumRendSkinnedObjects);

  uint nLine;
  { // sort by interpolated time and print
    if(!m_RP.m_Profile.empty())
      qsort(&m_RP.m_Profile[0], m_RP.m_Profile.Num(), sizeof(SProfInfo), TimeProfCallback );

    for(nLine=0; nLine<m_RP.m_Profile.Num(); nLine++)
    {
      fTime += m_RP.m_Profile[nLine].Time;
      if (nLine >= 18)
        continue;
      TextToScreenColor(4,(27+(nLine*3)), 1,0,0,1, "%8.02f ms, %5d polys, %4d DIPs, '%s.%s(0x%x)', %d item(s)", 
        m_RP.m_Profile[nLine].pTechnique->m_fProfileTime*1000.f, 
        m_RP.m_Profile[nLine].NumPolys,
        m_RP.m_Profile[nLine].NumDips,
        m_RP.m_Profile[nLine].pShader->GetName(),
        m_RP.m_Profile[nLine].pTechnique->m_Name.c_str(),
        m_RP.m_Profile[nLine].pShader->m_nMaskGenFX,
        m_RP.m_Profile[nLine].m_nItems+1);
    }
  }
  int nShaders = nLine;
  TextToScreenColor(2,(28+(nLine*3)), 0,2,0,1, "Total unique shaders:   %d",  nShaders);
  TextToScreenColor(2,(31+(nLine*3)), 0,2,0,1, "Total flush time:   %8.02f ms",  fTime);
  TextToScreenColor(2,(34+(nLine*3)), 0,2,0,1, "Total shaders processing time:   %8.02f ms", m_RP.m_PS.m_fFlushTime);


  //std::vector<Matrix33> vv;
  //vv.resize(10);

    /*int N = 1000;
    memset(mesh1, 0, sizeof(mesh1));
    memset(mesh2, 0, sizeof(mesh2));
    Matrix33 rotation;
    memset(&rotation, 0, sizeof(rotation));
    Vec3 offset(0, 0, 0);
    for (int j = 0; j<10000; ++j)
    {
      rotate_mesh(mesh1, mesh2, N, rotation, offset);
    }*/




  /*{
  int i = 0;
  double timeC = 0;
  double timeSSE = 0;

  CCamera cam = GetCamera();
  Vec3d camPos = cam.GetPos();
  AABB aabb;
  aabb.min = camPos+Vec3d(-10, -16,-16);
  aabb.max = camPos+Vec3d(16,32,16);
  Vec3d Origin = (aabb.min+aabb.max)*0.5f;
  Vec3d Extent = aabb.max - Origin;

  ticks(timeC);
  for (i=0; i<100000; i++)
  {
  cam.IsAABBVisible_exact(aabb, NULL);
  }
  unticks(timeC);

  ticks(timeSSE);
  for (i=0; i<100000; i++)
  {
  cam.IsAABBVisible_exact_SSE(Origin, Extent, NULL);
  }
  unticks(timeSSE);

  TextToScreenColor(8,(36+(nLine*3)), 0,2,0,1, "TimeC %8.02f ms, TimeSSE %8.02f ms", (float)(timeC*1000.0*m_RP.m_SecondsPerCycle), (float)(timeSSE*1000.0*m_RP.m_SecondsPerCycle));
  }*/
  /*{
  int i = 0;
  double timeC = 0;
  double timeC33 = 0;
  double time3DN = 0;
  double timeSSE = 0;
  _declspec(align(16)) Matrix m, mOut;
  m = m_cam.GetMatrix();
  m_RP.m_pCurObject = m_RP.m_VisObjects[0];
  Matrix33 m31;
  Matrix33 m32;
  m31.SetIdentity33();

  ticks(timeC33);
  for (i=0; i<100000; i++)
  {
  m31.Invert33();
  }
  unticks(timeC33);

  ticks(timeC);
  for (i=0; i<100000; i++)
  {
  QQinvertMatrixf(mOut.GetData(), m.GetData());
  }
  unticks(timeC);

  if (m_Cpu->mCpu[0].mFeatures & CFI_3DNOW)
  {
  ticks(time3DN);
  for (i=0; i<100000; i++)
  {
  invertMatrixf_3DNow(mOut.GetData(), m.GetData());
  }
  unticks(time3DN);
  }

  ticks(timeSSE);
  for (i=0; i<100000; i++)
  {
  invertMatrixf_SSE(mOut.GetData(), m.GetData());
  }
  unticks(timeSSE);

  TextToScreenColor(8,(36+(nLine*3)), 0,2,0,1, "TimeC: %8.02f ms, TimeC33: %8.02f ms,, Time3DN: %8.02f ms, TimeSSE: %8.02f ms", (float)(timeC*1000.0*m_RP.m_SecondsPerCycle), (float)(timeC33*1000.0*m_RP.m_SecondsPerCycle), (float)(time3DN*1000.0*m_RP.m_SecondsPerCycle), (float)(timeSSE*1000.0*m_RP.m_SecondsPerCycle));
  }*/
  /*{
  int i = 0;
  double timeC = 0;
  double timeSSE = 0;
  double timeSSENew = 0;
  byte *dataSrc = new byte[1024*1024*10];
  byte *dataDst = new byte[1024*1024*10];

  ticks(timeC);
  for (i=0; i<10; i++)
  {
  memcpy(dataDst, dataSrc, 1024*1024*10-128);
  }
  unticks(timeC);

  byte *Dst = (byte *)((int)(dataDst+15)&0xfffffff0);
  byte *Src = (byte *)((int)(dataSrc+15)&0xfffffff0);
  ticks(timeSSE);
  for (i=0; i<10; i++)
  {
  cryMemcpy(Dst, Src, 1024*1024*10-128);
  }
  unticks(timeSSE);

  ticks(timeSSENew);
  for (i=0; i<10; i++)
  {
  cryMemcpy(Dst, Src, 1024*1024*10-128, 0);
  }
  unticks(timeSSENew);

  delete [] dataSrc;
  delete [] dataDst;

  TextToScreenColor(8,(36+(nLine*3)), 0,2,0,1, "TimeC: %8.02f ms, TimeSSE: %8.02f ms", (float)(timeC*1000.0*g_SecondsPerCycle), (float)(timeSSE*1000.0*g_SecondsPerCycle));
  }*/
  /*{
  int i = 0;
  double timeCM10 = 0;
  double timeCF10 = 0;
  double timeCM60 = 0;
  double timeCF60 = 0;
  double timeCM110 = 0;
  double timeCF110 = 0;
  double timeCM510 = 0;
  double timeCF510 = 0;
  double timeCM1010 = 0;
  double timeCF1010 = 0;
  double timeCM5010 = 0;
  double timeCF5010 = 0;
  double timeCM10010 = 0;
  double timeCF10010 = 0;
  double timeCM100010 = 0;
  double timeCF100010 = 0;

  double timeUM10 = 0;
  double timeUF10 = 0;
  double timeUM60 = 0;
  double timeUF60 = 0;
  double timeUM110 = 0;
  double timeUF110 = 0;
  double timeUM510 = 0;
  double timeUF510 = 0;
  double timeUM1010 = 0;
  double timeUF1010 = 0;
  double timeUM5010 = 0;
  double timeUF5010 = 0;
  double timeUM10010 = 0;
  double timeUF10010 = 0;
  double timeUM100010 = 0;
  double timeUF100010 = 0;

  static FMallocWindows *pM;
  if (!pM)
  {
  pM = new FMallocWindows;
  pM->Init();
  }
  void *pPtr[1000];

  ticks(timeCM10);
  for (i=0; i<1000; i++)
  pPtr[i] = malloc(10);
  unticks(timeCM10);
  ticks(timeCF10);
  for (i=0; i<1000; i++)
  free(pPtr[i]);
  unticks(timeCF10);

  ticks(timeUM10);
  for (i=0; i<1000; i++)
  pPtr[i] = pM->Malloc(10, "Test");
  unticks(timeUM10);
  ticks(timeUF10);
  for (i=0; i<1000; i++)
  pM->Free(pPtr[i]);
  unticks(timeUF10);

  ticks(timeCM60);
  for (i=0; i<1000; i++)
  pPtr[i] = malloc(60);
  unticks(timeCM60);
  ticks(timeCF60);
  for (i=0; i<1000; i++)
  free(pPtr[i]);
  unticks(timeCF60);

  ticks(timeUM60);
  for (i=0; i<1000; i++)
  pPtr[i] = pM->Malloc(60, "Test");
  unticks(timeUM60);
  ticks(timeUF60);
  for (i=0; i<1000; i++)
  pM->Free(pPtr[i]);
  unticks(timeUF60);

  ticks(timeCM110);
  for (i=0; i<1000; i++)
  pPtr[i] = malloc(110);
  unticks(timeCM110);
  ticks(timeCF110);
  for (i=0; i<1000; i++)
  free(pPtr[i]);
  unticks(timeCF110);

  ticks(timeUM110);
  for (i=0; i<1000; i++)
  pPtr[i] = pM->Malloc(110, "Test");
  unticks(timeUM110);
  ticks(timeUF110);
  for (i=0; i<1000; i++)
  pM->Free(pPtr[i]);
  unticks(timeUF110);

  ticks(timeCM510);
  for (i=0; i<1000; i++)
  pPtr[i] = malloc(510);
  unticks(timeCM510);
  ticks(timeCF510);
  for (i=0; i<1000; i++)
  free(pPtr[i]);
  unticks(timeCF510);

  ticks(timeUM510);
  for (i=0; i<1000; i++)
  pPtr[i] = pM->Malloc(510, "Test");
  unticks(timeUM510);
  ticks(timeUF510);
  for (i=0; i<1000; i++)
  pM->Free(pPtr[i]);
  unticks(timeUF510);

  ticks(timeCM1010);
  for (i=0; i<1000; i++)
  pPtr[i] = malloc(1010);
  unticks(timeCM1010);
  ticks(timeCF1010);
  for (i=0; i<1000; i++)
  free(pPtr[i]);
  unticks(timeCF1010);

  ticks(timeUM1010);
  for (i=0; i<1000; i++)
  pPtr[i] = pM->Malloc(1010, "Test");
  unticks(timeUM1010);
  ticks(timeUF1010);
  for (i=0; i<1000; i++)
  pM->Free(pPtr[i]);
  unticks(timeUF1010);

  ticks(timeCM5010);
  for (i=0; i<1000; i++)
  pPtr[i] = malloc(5010);
  unticks(timeCM5010);
  ticks(timeCF5010);
  for (i=0; i<1000; i++)
  free(pPtr[i]);
  unticks(timeCF5010);

  ticks(timeUM5010);
  for (i=0; i<1000; i++)
  pPtr[i] = pM->Malloc(5010, "Test");
  unticks(timeUM5010);
  ticks(timeUF5010);
  for (i=0; i<1000; i++)
  pM->Free(pPtr[i]);
  unticks(timeUF5010);

  ticks(timeCM10010);
  for (i=0; i<1000; i++)
  pPtr[i] = malloc(10010);
  unticks(timeCM10010);
  ticks(timeCF10010);
  for (i=0; i<1000; i++)
  free(pPtr[i]);
  unticks(timeCF10010);

  ticks(timeUM10010);
  for (i=0; i<1000; i++)
  pPtr[i] = pM->Malloc(10010, "Test");
  unticks(timeUM10010);
  ticks(timeUF10010);
  for (i=0; i<1000; i++)
  pM->Free(pPtr[i]);
  unticks(timeUF10010);

  ticks(timeCM100010);
  for (i=0; i<1000; i++)
  pPtr[i] = malloc(100010);
  unticks(timeCM100010);
  ticks(timeCF100010);
  for (i=0; i<1000; i++)
  free(pPtr[i]);
  unticks(timeCF100010);

  ticks(timeUM100010);
  for (i=0; i<1000; i++)
  pPtr[i] = pM->Malloc(100010, "Test");
  unticks(timeUM100010);
  ticks(timeUF100010);
  for (i=0; i<1000; i++)
  pM->Free(pPtr[i]);
  unticks(timeUF100010);

  TextToScreenColor(1,(36+(nLine*3)), 0,2,0,1, "CM_10: %3.02f, CM_60: %3.02f, CM_110: %3.02f, CM_510: %3.02f, CM_1010: %3.02f, CM_5010: %3.02f, CM_10010: %3.02f, CM_100010: %3.02f", (float)(timeCM10*1000.0*g_SecondsPerCycle), (float)(timeCM60*1000.0*g_SecondsPerCycle), (float)(timeCM110*1000.0*g_SecondsPerCycle), (float)(timeCM510*1000.0*g_SecondsPerCycle), (float)(timeCM1010*1000.0*g_SecondsPerCycle), (float)(timeCM5010*1000.0*g_SecondsPerCycle), (float)(timeCM10010*1000.0*g_SecondsPerCycle), (float)(timeCM100010*1000.0*g_SecondsPerCycle));
  TextToScreenColor(1,(40+(nLine*3)), 0,2,0,1, "UM_10: %3.02f, UM_60: %3.02f, UM_110: %3.02f, UM_510: %3.02f, UM_1010: %3.02f, UM_5010: %3.02f, UM_10010: %3.02f, UM_100010: %3.02f", (float)(timeUM10*1000.0*g_SecondsPerCycle), (float)(timeUM60*1000.0*g_SecondsPerCycle), (float)(timeUM110*1000.0*g_SecondsPerCycle), (float)(timeUM510*1000.0*g_SecondsPerCycle), (float)(timeUM1010*1000.0*g_SecondsPerCycle), (float)(timeUM5010*1000.0*g_SecondsPerCycle), (float)(timeUM10010*1000.0*g_SecondsPerCycle), (float)(timeUM100010*1000.0*g_SecondsPerCycle));
  TextToScreenColor(1,(44+(nLine*3)), 0,2,0,1, "CF_10: %3.02f, CF_60: %3.02f, CF_110: %3.02f, CF_510: %3.02f, CF_1010: %3.02f, CF_5010: %3.02f, CF_10010: %3.02f, CF_100010: %3.02f", (float)(timeCF10*1000.0*g_SecondsPerCycle), (float)(timeCF60*1000.0*g_SecondsPerCycle), (float)(timeCF110*1000.0*g_SecondsPerCycle), (float)(timeCF510*1000.0*g_SecondsPerCycle), (float)(timeCF1010*1000.0*g_SecondsPerCycle), (float)(timeCF5010*1000.0*g_SecondsPerCycle), (float)(timeCF10010*1000.0*g_SecondsPerCycle), (float)(timeCF100010*1000.0*g_SecondsPerCycle));
  TextToScreenColor(1,(48+(nLine*3)), 0,2,0,1, "UF_10: %3.02f, UF_60: %3.02f, UF_110: %3.02f, UF_510: %3.02f, UF_1010: %3.02f, UF_5010: %3.02f, UF_10010: %3.02f, UF_100010: %3.02f", (float)(timeUF10*1000.0*g_SecondsPerCycle), (float)(timeUF60*1000.0*g_SecondsPerCycle), (float)(timeUF110*1000.0*g_SecondsPerCycle), (float)(timeUF510*1000.0*g_SecondsPerCycle), (float)(timeUF1010*1000.0*g_SecondsPerCycle), (float)(timeUF5010*1000.0*g_SecondsPerCycle), (float)(timeUF10010*1000.0*g_SecondsPerCycle), (float)(timeUF100010*1000.0*g_SecondsPerCycle));
  }*/
}

struct SPreprocess
{
  int m_nPreprocess;
  int m_Num;
  CRenderObject *m_pObject;
  int m_nTech;
  CShader *m_Shader;
  SRenderShaderResources *m_pRes;
  CRendElement *m_RE;
};

static inline bool Compare2(const SPreprocess &a, const SPreprocess &b)
{
  if (a.m_nPreprocess != b.m_nPreprocess)
    return a.m_nPreprocess < b.m_nPreprocess;
  return false;
}

// Current scene preprocess operations (Rendering to RT, screen effects initializing, ...)
int CD3D9Renderer::FX_Preprocess(SRendItem *ri, uint nums, uint nume)
{
  uint i, j;
  CShader *Shader;
  SRenderShaderResources *Res;
  CRenderObject *pObject;
  int nTech;

  SPreprocess Procs[512];
  uint nProcs = 0;

  float time0 = iTimer->GetAsyncCurTime();

  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, "*** Start preprocess frame ***\n");

  int DLDFlags = 0;
  int nReturn = 0;

  for (i=nums; i<nume; i++)
  {
    if (nProcs >= 512)
      break;
    SRendItem::mfGet(ri[i].SortVal, nTech, Shader, Res);
    pObject = ri[i].pObj;
    if (!(ri[i].nBatchFlags & 0xffff0000))
      break;
    nReturn++;
    if (nTech < 0)
      nTech = 0;
    if (nTech < (int)Shader->m_HWTechniques.Num())
    {
      SShaderTechnique *pTech = Shader->m_HWTechniques[nTech];
      for (j=SPRID_FIRST; j<32; j++)
      {
        uint nMask = 1<<j;
        if (nMask >= FSPR_MAX || nMask > (ri[i].nBatchFlags & 0xffff0000))
          break;
        if (nMask & ri[i].nBatchFlags)
        {
          Procs[nProcs].m_nPreprocess = j;
          Procs[nProcs].m_Num = i;
          Procs[nProcs].m_Shader = Shader;
          Procs[nProcs].m_pRes = Res;
          Procs[nProcs].m_RE = ri[i].Item;
          Procs[nProcs].m_pObject = pObject;
          Procs[nProcs].m_nTech = nTech;
          nProcs++;
        }
      }
    }
  }
  if (!nProcs)
    return 0;
  std::sort(&Procs[0], &Procs[nProcs], Compare2);

  if (m_RP.m_pRenderFunc != EF_Flush)
    return nReturn;

  bool bRes = true;
  for (i=0; i<nProcs; i++)
  {
    SPreprocess *pr = &Procs[i];
    if (!pr->m_Shader)
      continue;
    m_RP.m_pRE = pr->m_RE;
    m_RP.m_pCurObject = pr->m_pObject;
    m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
    switch (pr->m_nPreprocess)
    {
    case SPRID_CORONA:
      break;

    case SPRID_GENSPRITES:
      iSystem->GetI3DEngine()->GenerateFarTrees();
      break;

    case SPRID_SCANTEX:
    case SPRID_SCANCM:
    case SPRID_SCANLCM:
    case SPRID_SCANTEXWATER:
      if (!(m_RP.m_PersFlags & RBPF_DRAWTOTEXTURE))
      {
        CRenderObject *pObj = pr->m_pObject;
        int nTech = pr->m_nTech;
        if (nTech < 0)
          nTech = 0;
        SShaderTechnique *pTech = pr->m_Shader->m_HWTechniques[nTech];
        SRenderShaderResources *pRes = pr->m_pRes;
        for (j=0; j<pTech->m_RTargets.Num(); j++)
        {
          SHRenderTarget *pTarg = pTech->m_RTargets[j];
          if (pTarg->m_eOrder == eRO_PreProcess)
            bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE);
        }
        if (pRes)
        {
          for (j=0; j<pRes->m_RTargets.Num(); j++)
          {
            SHRenderTarget *pTarg = pRes->m_RTargets[j];
            if (pTarg->m_eOrder == eRO_PreProcess)
              bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE);
          }
        }
      }
      break;

    case SPRID_CUSTOMTEXTURE:
      if (!(m_RP.m_PersFlags & RBPF_DRAWTOTEXTURE))
      {
        CRenderObject *pObj = pr->m_pObject;
        int nTech = pr->m_nTech;
        if (nTech < 0)
          nTech = 0;
        SShaderTechnique *pTech = pr->m_Shader->m_HWTechniques[nTech];
        SRenderShaderResources *pRes = pr->m_pRes;
        for (j=0; j<pRes->m_RTargets.Num(); j++)
        {
          SHRenderTarget *pTarg = pRes->m_RTargets[j];
          if (pTarg->m_eOrder == eRO_PreProcess)
            bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE);
        }
      }
      break;
    case SPRID_PANORAMA:
      {
        assert (pr->m_RE->mfGetType() == eDATA_PanoramaCluster);
        if (pr->m_RE->mfGetType() == eDATA_PanoramaCluster)
        {
          CREPanoramaCluster *pRE = (CREPanoramaCluster *)pr->m_RE;
          CRenderObject *pObj = pr->m_pObject;

          pRE->UpdateImposter();
        }
      }
      break;

    default:
      assert(0);
    }
  }

  if (m_LogFile)
    Logv(SRendItem::m_RecurseLevel, "*** End preprocess frame ***\n");

  m_RP.m_PS.m_fPreprocessTime += iTimer->GetAsyncCurTime()-time0;

  return nReturn;
}

void CD3D9Renderer::EF_EndEf2D(bool bSort)
{
}

#ifndef EXCLUDE_SCALEFORM_SDK

//////////////////////////////////////////////////////////////////////////
struct SSF_ResourcesD3D
{	
	CCryName m_shTech_SolidColor;
	CCryName m_shTech_Glyph;
	CCryName m_shTech_CxformMultiplyTexture;
	CCryName m_shTech_CxformTexture;
	CCryName m_shTech_CxformGouraudMultiplyNoAddAlpha;
	CCryName m_shTech_CxformGouraudNoAddAlpha;
	CCryName m_shTech_CxformGouraudMultiply;
	CCryName m_shTech_CxformGouraud;
	CCryName m_shTech_CxformGouraudMultiplyTexture;
	CCryName m_shTech_CxformGouraudTexture;
	CCryName m_shTech_CxformMultiply2Texture;
	CCryName m_shTech_Cxform2Texture;

	CShader* m_pShader;

#if !defined(DIRECT3D10)
	IDirect3DVertexDeclaration9* m_pVertexDeclXY16i;
	IDirect3DVertexDeclaration9* m_pVertexDeclXY16iC32;
	IDirect3DVertexDeclaration9* m_pVertexDeclXY16iCF32;
	IDirect3DVertexDeclaration9* m_pVertexDeclGlyph;
	IDirect3DQuery9* m_pQuery;
#else
	ID3D10InputLayout* m_pVertexDeclXY16i;
	ID3D10InputLayout* m_pVertexDeclXY16iC32;
	ID3D10InputLayout* m_pVertexDeclXY16iCF32;
	ID3D10InputLayout* m_pVertexDeclGlyph;
	ID3D10Query* m_pQuery;
#endif

	SSF_ResourcesD3D(CD3D9Renderer* pRenderer)
	: m_shTech_SolidColor("SolidColor")
	, m_shTech_Glyph("Glyph")
	, m_shTech_CxformMultiplyTexture("CxformMultiplyTexture")
	, m_shTech_CxformTexture("CxformTexture")
	, m_shTech_CxformGouraudMultiplyNoAddAlpha("CxformGouraudMultiplyNoAddAlpha")
	, m_shTech_CxformGouraudNoAddAlpha("CxformGouraudNoAddAlpha")
	, m_shTech_CxformGouraudMultiply("CxformGouraudMultiply")
	, m_shTech_CxformGouraud("CxformGouraud")
	, m_shTech_CxformGouraudMultiplyTexture("CxformGouraudMultiplyTexture")
	, m_shTech_CxformGouraudTexture("CxformGouraudTexture")
	, m_shTech_CxformMultiply2Texture("CxformMultiply2Texture")
	, m_shTech_Cxform2Texture("Cxform2Texture")
	, m_pShader(0)
	, m_pVertexDeclXY16i(0)
	, m_pVertexDeclXY16iC32(0)
	, m_pVertexDeclXY16iCF32(0)
	, m_pVertexDeclGlyph(0)
	, m_pQuery(0)
	{
		m_pShader = pRenderer->m_cEF.mfForName("Scaleform", EF_SYSTEM);
	}
};


//////////////////////////////////////////////////////////////////////////
SSF_ResourcesD3D& CD3D9Renderer::SF_GetResources()
{
	static SSF_ResourcesD3D s_sfRes(this);		
	return s_sfRes;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_ResetResources()
{
	SAFE_RELEASE(SF_GetResources().m_pQuery);
}

//////////////////////////////////////////////////////////////////////////
bool CD3D9Renderer::SF_SetVertexDeclaration(SSF_GlobalDrawParams::EVertexFmt vertexFmt)
{
#if !defined(DIRECT3D10)
	const D3DVERTEXELEMENT9 VertexDeclXY16i[] =
	{
		{0, 0, D3DDECLTYPE_SHORT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		D3DDECL_END()
	};
	const D3DVERTEXELEMENT9 VertexDeclXY16iC32[] =
	{
		{0, 0, D3DDECLTYPE_SHORT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 4, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		D3DDECL_END()
	};
	const D3DVERTEXELEMENT9 VertexDeclXY16iCF32[] =
	{
		{0, 0, D3DDECLTYPE_SHORT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 4, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		{0, 8, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1},
		D3DDECL_END()
	};
	const D3DVERTEXELEMENT9 VertexDeclGlyph[] =
	{
		{0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		{0, 16, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		D3DDECL_END()
	};

	IDirect3DVertexDeclaration9* pVD(0);

#	define SF_CREATE_VERTEX_DECL(inputElements, pDecl) m_pd3dDevice->CreateVertexDeclaration(inputElements, &pDecl);

#else // #if !defined(DIRECT3D10)
	const D3D10_INPUT_ELEMENT_DESC VertexDeclXY16i[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R16G16_SINT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0}
	};
	const D3D10_INPUT_ELEMENT_DESC VertexDeclXY16iC32[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R16G16_SINT,	0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM,	0, 4, D3D10_INPUT_PER_VERTEX_DATA, 0}
	};
	const D3D10_INPUT_ELEMENT_DESC VertexDeclXY16iCF32[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R16G16_SINT,	0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM,	0, 4, D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM,	0, 8, D3D10_INPUT_PER_VERTEX_DATA, 0}
	};
	const D3D10_INPUT_ELEMENT_DESC VertexDeclGlyph[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,	0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D10_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0}
	};

	ID3D10InputLayout* pVD(0);

#	define SF_CREATE_VERTEX_DECL(inputElements, pDecl) \
	m_pd3dDevice->CreateInputLayout(inputElements, sizeof(inputElements)/sizeof(inputElements[0]), CHWShader_D3D::m_pCurInstVS->m_pShaderData, \
		CHWShader_D3D::m_pCurInstVS->m_nShaderByteCodeSize, &pDecl);

	assert(CHWShader_D3D::m_pCurInstVS && CHWShader_D3D::m_pCurInstVS->m_pShaderData);
#endif // #if !defined(DIRECT3D10)

	SSF_ResourcesD3D& sfRes(SF_GetResources());

	HRESULT hr(S_OK);
	switch (vertexFmt)
	{
	case SSF_GlobalDrawParams::Vertex_XY16i:
		{
			if (!sfRes.m_pVertexDeclXY16i)
				hr = SF_CREATE_VERTEX_DECL(VertexDeclXY16i, sfRes.m_pVertexDeclXY16i);
			pVD = sfRes.m_pVertexDeclXY16i;
			break;
		}
	case SSF_GlobalDrawParams::Vertex_XY16iC32:
		{
			if (!sfRes.m_pVertexDeclXY16iC32)
				hr = SF_CREATE_VERTEX_DECL(VertexDeclXY16iC32, sfRes.m_pVertexDeclXY16iC32);
			pVD = sfRes.m_pVertexDeclXY16iC32;
			break;
		}
	case SSF_GlobalDrawParams::Vertex_XY16iCF32:
		{
			if (!sfRes.m_pVertexDeclXY16iCF32)
				hr = SF_CREATE_VERTEX_DECL(VertexDeclXY16iCF32, sfRes.m_pVertexDeclXY16iCF32);
			pVD = sfRes.m_pVertexDeclXY16iCF32;
			break;
		}
	case SSF_GlobalDrawParams::Vertex_Glyph:
		{
			if (!sfRes.m_pVertexDeclGlyph)
				hr = SF_CREATE_VERTEX_DECL(VertexDeclGlyph, sfRes.m_pVertexDeclGlyph);
			pVD = sfRes.m_pVertexDeclGlyph;
			break;
		}
	default:
		{
			assert(0);
			break;
		}
	}

	assert(SUCCEEDED(hr) && pVD);
	if (FAILED(hr) || !pVD)
		return false;

	if (m_pLastVDeclaration != pVD)
	{
		m_pLastVDeclaration = pVD;
#if !defined(DIRECT3D10)
		hr = m_pd3dDevice->SetVertexDeclaration(pVD);
#else
		m_pd3dDevice->IASetInputLayout(pVD);
		hr = S_OK;
#endif
	}

	return SUCCEEDED(hr);
}

//////////////////////////////////////////////////////////////////////////
CShader* CD3D9Renderer::SF_SetTechnique(const CCryName& techName)
{
	CShader* pShader(SF_GetResources().m_pShader);
	if (!pShader)
		return 0;
	
	SShaderTechnique* pTech(0);
	int i(0);
	for (; i<pShader->m_HWTechniques.Num(); ++i)
	{
		pTech = pShader->m_HWTechniques[i];
		if (techName == pTech->m_Name)
			break;
	}

	if (i == pShader->m_HWTechniques.Num())
		return 0;

	CRenderer* rd(gRenDev);
	rd->m_RP.m_pShader = pShader;
	rd->m_RP.m_pCurTechnique = pTech;

	return pShader;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_SetBlendOp(SSF_GlobalDrawParams::EAlphaBlendOp blendOp, bool reset)
{
	if (!reset)
	{
		if (blendOp != SSF_GlobalDrawParams::Add)
		{
			switch(blendOp)
			{
			case SSF_GlobalDrawParams::Substract:
				{
#if defined(DIRECT3D10)
					SStateBlend bl = m_StatesBL[m_nCurStateBL];
					bl.Desc.BlendOp = D3D10_BLEND_OP_SUBTRACT;
					bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_SUBTRACT;
					SetBlendState(&bl);
#else
					m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_SUBTRACT);
#endif
				}
				break;
			case SSF_GlobalDrawParams::RevSubstract:
				{
#if defined(DIRECT3D10)
					SStateBlend bl = m_StatesBL[m_nCurStateBL];
					bl.Desc.BlendOp = D3D10_BLEND_OP_REV_SUBTRACT;
					bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_REV_SUBTRACT;
					SetBlendState(&bl);
#else
					m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
#endif
				}
				break;
			case SSF_GlobalDrawParams::Min:
				{
#if defined(DIRECT3D10)
					SStateBlend bl = m_StatesBL[m_nCurStateBL];
					bl.Desc.BlendOp = D3D10_BLEND_OP_MIN;
					bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_MIN;
					SetBlendState(&bl);
#else
					m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_MIN);
#endif
				}
				break;
			case SSF_GlobalDrawParams::Max:
				{
#if defined(DIRECT3D10)
					SStateBlend bl = m_StatesBL[m_nCurStateBL];
					bl.Desc.BlendOp = D3D10_BLEND_OP_MAX;
					bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_MAX;
					SetBlendState(&bl);
#else
					m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_MAX);
#endif
				}
				break;
			default:
				assert(0);
				break;
			}
		}
	}
	else
	{
		if (blendOp != SSF_GlobalDrawParams::Add)
		{
#if defined(DIRECT3D10)
			SStateBlend bl = m_StatesBL[m_nCurStateBL];
			bl.Desc.BlendOp = D3D10_BLEND_OP_ADD;
			bl.Desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
			SetBlendState(&bl);
#else
			m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
#endif
		}
	}
}


uint32 CD3D9Renderer::SF_AdjustBlendStateForMeasureOverdraw(uint32 blendModeStates)
{
	if (CV_r_measureoverdraw)
	{
		blendModeStates = (blendModeStates & ~GS_BLEND_MASK) | (GS_BLSRC_ONE | GS_BLDST_ONE);
		blendModeStates &= ~GS_ALPHATEST_MASK;
	}
	return blendModeStates;
}


//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_DrawIndexedTriList( int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount, const SSF_GlobalDrawParams& params )
{
  FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

  if (IsDeviceLost())
    return;

	assert(params.vertexFmt != SSF_GlobalDrawParams::Vertex_Glyph && params.vertexFmt != SSF_GlobalDrawParams::Vertex_None);
  assert(params.pIndexPtr);
  assert(params.indexFmt ==  SSF_GlobalDrawParams::Index_16);

	const SSF_ResourcesD3D& sfRes(SF_GetResources());
  CShader* pSFShader(0);
	{
		//FRAME_PROFILER_FAST("SF_DITL::SetShader", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set appropriate shader
		SSF_GlobalDrawParams::EFillType fillType(params.fillType);
		if (fillType >= SSF_GlobalDrawParams::GColor && params.texture[0].texID <= 0)
			fillType = SSF_GlobalDrawParams::GColor;

		switch(fillType)
		{
		case SSF_GlobalDrawParams::SolidColor:
			pSFShader = SF_SetTechnique(sfRes.m_shTech_SolidColor);
			break;
		case SSF_GlobalDrawParams::Texture:
			pSFShader = SF_SetTechnique(params.isMultiplyDarkBlendMode ? sfRes.m_shTech_CxformMultiplyTexture : sfRes.m_shTech_CxformTexture);
			break;
		case SSF_GlobalDrawParams::GColor:
			if (params.vertexFmt == SSF_GlobalDrawParams::Vertex_XY16iC32)
				pSFShader = SF_SetTechnique(params.isMultiplyDarkBlendMode ? sfRes.m_shTech_CxformGouraudMultiplyNoAddAlpha : sfRes.m_shTech_CxformGouraudNoAddAlpha);
			else
				pSFShader = SF_SetTechnique(params.isMultiplyDarkBlendMode ? sfRes.m_shTech_CxformGouraudMultiply : sfRes.m_shTech_CxformGouraud);
			break;
		case SSF_GlobalDrawParams::G1Texture:
		case SSF_GlobalDrawParams::G1TextureColor:
			pSFShader = SF_SetTechnique(params.isMultiplyDarkBlendMode ? sfRes.m_shTech_CxformGouraudMultiplyTexture : sfRes.m_shTech_CxformGouraudTexture);
			break;
		case SSF_GlobalDrawParams::G2Texture:
			pSFShader = SF_SetTechnique(params.isMultiplyDarkBlendMode ? sfRes.m_shTech_CxformMultiply2Texture : sfRes.m_shTech_Cxform2Texture);
			break;
		default:
			assert(0);
			break;
		}
	}

	if (!pSFShader)
		return;

	{
		//FRAME_PROFILER_FAST("SF_DITL::FxBegin", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		m_pSFDrawParams = &params;
		uint numPasses(0);
		pSFShader->FXBegin(&numPasses, /*FEF_DONTSETTEXTURES |*/ FEF_DONTSETSTATES);
		if (!numPasses)
		{
			m_pSFDrawParams = 0;
			return;	
		}
		pSFShader->FXBeginPass(0);
	}
	{
		//FRAME_PROFILER_FAST("SF_DITL::SetState", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set states
		EF_SetState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | /*GS_NODEPTHTEST | */params.renderMaskedStates);
		D3DSetCull(eCULL_None);
	}
	{
		//FRAME_PROFILER_FAST("SF_DITL::EF_Commit", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Commit all render changes
		EF_Commit();
	}
	{
		//FRAME_PROFILER_FAST("SF_DITL::SetVertexDecl", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set vertex declaration
		if (!SF_SetVertexDeclaration(params.vertexFmt))
		{
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			m_pSFDrawParams = 0;
			return;
		}
	}

  // Copy vertex data...
	uint32 finalStartIndex(-1);
	{
		//FRAME_PROFILER_FAST("SF_DITL::DynBufUpdate", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);
		{
			if (params.pVertexPtr)
			{
				//FRAME_PROFILER_FAST("SF_DITL::CopyVerts", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

				size_t vertexSize(-1);
				switch(params.vertexFmt)
				{
				case SSF_GlobalDrawParams::Vertex_XY16i:
					vertexSize = 4;
					break;
				case SSF_GlobalDrawParams::Vertex_XY16iC32:
					vertexSize = 8;
					break;
				case SSF_GlobalDrawParams::Vertex_XY16iCF32:
					vertexSize = 12;
					break;
				default:
					assert(0);
					break;
				}

				uint reqBufferSize(vertexSize * params.numVertices);
				uint bufferOffset(-1);
				void* pVB(FX_LockVB(reqBufferSize, bufferOffset));
				memcpy(pVB, params.pVertexPtr, reqBufferSize);
				FX_UnlockVB();
				m_RP.m_VBs[m_RP.m_CurVB].VBPtr_0->Bind(0, bufferOffset, vertexSize);
			}
		}
		{
			//FRAME_PROFILER_FAST("SF_DITL::CopyInds", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

			assert(params.pIndexPtr);			
			void* pIB(m_RP.m_IndexBuf->Lock(params.numIndices, finalStartIndex));
			memcpy(pIB, params.pIndexPtr, params.numIndices * sizeof(uint16));
			m_RP.m_IndexBuf->Unlock();
			m_RP.m_IndexBuf->Bind();
			finalStartIndex += startIndex;
		}		
	}
	{
		//FRAME_PROFILER_FAST("SF_DITL::BlendStateAndDraw", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Override blend op if necessary
		if (!CV_r_measureoverdraw)
			SF_SetBlendOp(params.blendOp);

		// Submit draw call
#if !defined(DIRECT3D10)
		m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseVertexIndex, minVertexIndex, numVertices, finalStartIndex, triangleCount);
#else
		SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pd3dDevice->DrawIndexed(triangleCount * 3, finalStartIndex, baseVertexIndex);
#endif

		m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += triangleCount;
		m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;

		// Reset overridden blend op if necessary
		if (!CV_r_measureoverdraw)
			SF_SetBlendOp(params.blendOp, true);
	}
	{
		//FRAME_PROFILER_FAST("SF_DITL::FXEnd", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// End shader pass
		pSFShader->FXEndPass();
		pSFShader->FXEnd();
	}

	m_pSFDrawParams = 0;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_DrawLineStrip( int baseVertexIndex, int lineCount, const SSF_GlobalDrawParams& params )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

  if (IsDeviceLost())
    return;

	assert(params.vertexFmt == SSF_GlobalDrawParams::Vertex_XY16i);

	const SSF_ResourcesD3D& sfRes(SF_GetResources());
	CShader* pSFShader(0);
	{
		//FRAME_PROFILER_FAST("SF_DLS::SetShader", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set shader
		switch(params.fillType)
		{
		case SSF_GlobalDrawParams::SolidColor:
			pSFShader = SF_SetTechnique(sfRes.m_shTech_SolidColor);
			break;
		default:
			assert(0);
			break;
		}		
	}

	if (!pSFShader)
		return;

	{
		//FRAME_PROFILER_FAST("SF_DLS::FxBegin", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		m_pSFDrawParams = &params;
		uint numPasses(0);
		pSFShader->FXBegin(&numPasses, /*FEF_DONTSETTEXTURES |*/ FEF_DONTSETSTATES);
		if (!numPasses)
		{
			m_pSFDrawParams = 0;
			return;	
		}
		pSFShader->FXBeginPass(0);
	}
	{
		//FRAME_PROFILER_FAST("SF_DLS::SetState", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set states
		EF_SetState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | /*GS_NODEPTHTEST | */params.renderMaskedStates);
		D3DSetCull(eCULL_None);
	}
	{
		//FRAME_PROFILER_FAST("SF_DLS::EF_Commit", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Commit all render changes
		EF_Commit();
	}
	{
		//FRAME_PROFILER_FAST("SF_DLS::SetVertexDecl", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set vertex declaration
		if (!SF_SetVertexDeclaration(params.vertexFmt))
		{
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			m_pSFDrawParams = 0;
			return;
		}
	}

	// Copy vertex data...
	{
		//FRAME_PROFILER_FAST("SF_DLS::DynVBUpdate", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		if (params.pVertexPtr)
		{
			//FRAME_PROFILER_FAST("SF_DLS::CopyVerts", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

			size_t vertexSize(-1);
			switch(params.vertexFmt)
			{
			case SSF_GlobalDrawParams::Vertex_XY16i:
				vertexSize = 4;
				break;
			default:
				assert(0);
				break;
			}

			uint reqBufferSize(vertexSize * params.numVertices);
			uint bufferOffset(-1);
			void* pVB(FX_LockVB(reqBufferSize, bufferOffset));
			memcpy(pVB, params.pVertexPtr, reqBufferSize);
			FX_UnlockVB();
			m_RP.m_VBs[m_RP.m_CurVB].VBPtr_0->Bind(0, bufferOffset, vertexSize);
		}
	}
	{
		//FRAME_PROFILER_FAST("SF_DLS::BlendStateAndDraw", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Override blend op if necessary
		if (!CV_r_measureoverdraw)
			SF_SetBlendOp(params.blendOp);

		// Submit draw call
#if !defined(DIRECT3D10)
		m_pd3dDevice->DrawPrimitive(D3DPT_LINESTRIP, baseVertexIndex, lineCount);
#else
		SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
		m_pd3dDevice->Draw(params.numVertices, baseVertexIndex);
#endif

		m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += lineCount;
		m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;

		// Reset overridden blend op if necessary
		if (!CV_r_measureoverdraw)
			SF_SetBlendOp(params.blendOp, true);
	}
	{
		//FRAME_PROFILER_FAST("SF_DLS::FXEnd", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// End shader pass
		pSFShader->FXEndPass();
		pSFShader->FXEnd();
	}

	m_pSFDrawParams = 0;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_DrawGlyphClear( const SSF_GlobalDrawParams& params )
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

  if (IsDeviceLost())
    return;

	assert(params.vertexFmt == SSF_GlobalDrawParams::Vertex_Glyph || params.vertexFmt == SSF_GlobalDrawParams::Vertex_XY16i);
	assert(params.pVertexPtr);

	const SSF_ResourcesD3D& sfRes(SF_GetResources());
	CShader* pSFShader(0);
	{
		//FRAME_PROFILER_FAST("SF_DG::SetShader", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set shader
		switch(params.fillType)
		{
		case SSF_GlobalDrawParams::Glyph:
			pSFShader = SF_SetTechnique(sfRes.m_shTech_Glyph);
			break;
		case SSF_GlobalDrawParams::SolidColor:
			pSFShader = SF_SetTechnique(sfRes.m_shTech_SolidColor);
			break;
		default:
			assert(0);
			break;
		}		
	}

	if (!pSFShader)
		return;

	{
		//FRAME_PROFILER_FAST("SF_DG::FxBegin", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		m_pSFDrawParams = &params;
		uint numPasses(0);
		pSFShader->FXBegin(&numPasses, /*FEF_DONTSETTEXTURES |*/ FEF_DONTSETSTATES);
		if (!numPasses)
		{
			m_pSFDrawParams = 0;
			return;	
		}
		pSFShader->FXBeginPass(0);
	}
	{
		//FRAME_PROFILER_FAST("SF_DG::SetState", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set states
		EF_SetState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | /*GS_NODEPTHTEST | */params.renderMaskedStates);
		D3DSetCull(eCULL_None);
	}
	{
		//FRAME_PROFILER_FAST("SF_DG::EF_Commit", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Commit all render changes
		EF_Commit();
	}
	{
		//FRAME_PROFILER_FAST("SF_DG::SetVertexDecl", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Set vertex declaration
		if (!SF_SetVertexDeclaration(params.vertexFmt))
		{
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			m_pSFDrawParams = 0;
			return;
		}
	}

	// Copy vertex data...
	{
		//FRAME_PROFILER_FAST("SF_DG::DynVBUpdate", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		size_t vertexSize(-1);
		switch(params.vertexFmt)
		{
		case SSF_GlobalDrawParams::Vertex_XY16i:
			vertexSize = 4;
			break;
		case SSF_GlobalDrawParams::Vertex_Glyph:
			vertexSize = 20;
			break;
		default:
			assert(0);
			break;
		}

		uint reqBufferSize(vertexSize * params.numVertices);
		uint bufferOffset(-1);
		void* pVB(FX_LockVB(reqBufferSize, bufferOffset));
		memcpy(pVB, params.pVertexPtr, reqBufferSize);
		FX_UnlockVB();
		m_RP.m_VBs[m_RP.m_CurVB].VBPtr_0->Bind(0, bufferOffset, vertexSize);
	}
	{
		//FRAME_PROFILER_FAST("SF_DG::BlendStateAndDraw", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// Override blend op if necessary
		if (!CV_r_measureoverdraw)
			SF_SetBlendOp(params.blendOp);

		// Submit draw call
#if !defined(DIRECT3D10)
		m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, params.numVertices - 2);
#else
		SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_pd3dDevice->Draw(params.numVertices, 0);
#endif

		m_RP.m_PS.m_nPolygons[m_RP.m_nPassGroupDIP] += params.numVertices - 2;
		m_RP.m_PS.m_nDIPs[m_RP.m_nPassGroupDIP]++;

		// Reset overridden blend op if necessary
		if (!CV_r_measureoverdraw)
			SF_SetBlendOp(params.blendOp, true);
	}
	{
		//FRAME_PROFILER_FAST("SF_DG::FXEnd", gEnv->pSystem, PROFILE_SYSTEM, g_bProfilerEnabled);

		// End shader pass
		pSFShader->FXEndPass();
		pSFShader->FXEnd();
	}

	m_pSFDrawParams = 0;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_Flush()
{
  if (IsDeviceLost())
    return;

	HRESULT hr(S_OK);

#if !defined(DIRECT3D10)
	SSF_ResourcesD3D& sfRes(SF_GetResources());
	if (!sfRes.m_pQuery)
		hr = m_pd3dDevice->CreateQuery(D3DQUERYTYPE_EVENT, &sfRes.m_pQuery);

#else
	SSF_ResourcesD3D& sfRes(SF_GetResources());
	if (!sfRes.m_pQuery)
	{
		D3D10_QUERY_DESC desc;
		desc.Query = D3D10_QUERY_EVENT;
		desc.MiscFlags = 0;
		hr = m_pd3dDevice->CreateQuery(&desc, &sfRes.m_pQuery);
	}
#endif

	if (sfRes.m_pQuery)
	{
#if !defined(DIRECT3D10)
		hr = sfRes.m_pQuery->Issue(D3DISSUE_END);
#else
		sfRes.m_pQuery->End();
#endif
		BOOL data(FALSE);
		while (S_FALSE == (hr = sfRes.m_pQuery->GetData(&data, sizeof(data), D3DGETDATA_FLUSH)));		
	}
}

//////////////////////////////////////////////////////////////////////////
bool CD3D9Renderer::SF_UpdateTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, unsigned char* pData, size_t pitch, ETEX_Format eTF)
{
	FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);

	assert(texId > 0 && numRects > 0 && pRects != 0 && pData != 0 && pitch > 0);

	CTexture* pTexture(CTexture::GetByID(texId));
	assert(pTexture);

	if (pTexture->GetDstFormat() != eTF || pTexture->GetTextureType() != eTT_2D)
	{
		assert(0);
		return false;
	}

#if defined(DIRECT3D9) || defined(OPENGL)
	IDirect3DTexture9* pTex((IDirect3DTexture9*) pTexture->GetDeviceTexture());
	if (!pTex)
		return false;
#	if defined(XENON)
	assert(0); // this code hasn't been tested yet, please let me know when you hit this assert so we can check - CarstenW
	// lock surface of given texture mip level and update all rects
	D3DLOCKED_RECT lr;
	if (SUCCEEDED(pTex->LockRect(mipLevel, &lr, 0, 0)))
	{
		for (int i(0); i<numRects; ++i)
		{
			int sizePixel(CTexture::BitsPerPixel(eTF) >> 3);
			int sizeLine(sizePixel * pRects[i].width);

			POINT dstPnt = {pRects[i].dstX, pRects[i].dstY};
			RECT srcRc = {pRects[i].srcX, pRects[i].srcY, pRects[i].srcX + pRects[i].width, pRects[i].srcY + pRects[i].height};

			XGTileSurface(lr.pBits, pRects[i].width, pRects[i].height, &dstPnt, pData, (unsigned int) pitch, &srcRc, sizePixel);
		}
		pTex->UnlockRect(mipLevel);
		return true;
	}
	else
		return false;
#	else
	// get surface of given texture mip level 
	IDirect3DSurface9* pSurf(0);
	if (FAILED(pTex->GetSurfaceLevel(mipLevel, &pSurf)))
		return false;

	// build union of all rectangle in the destination surfaces
	RECT rc = {pRects[0].dstX, pRects[0].dstY, pRects[0].dstX + pRects[0].width, pRects[0].dstY + pRects[0].height};
	for (int i(1); i<numRects; ++i)
	{
		rc.left = min(rc.left, pRects[i].dstX);
		rc.top = min(rc.top, pRects[i].dstY);
		rc.right = max(rc.right, pRects[i].dstX + pRects[i].width);
		rc.bottom = max(rc.bottom, pRects[i].dstY + pRects[i].height);
	}

	// lock surface and update all rects
	bool successful(false);
	D3DLOCKED_RECT lr;
	if (SUCCEEDED(pSurf->LockRect(&lr, &rc, 0)))
	{
		for (int i(0); i<numRects; ++i)
		{
			int sizePixel(CTexture::BitsPerPixel(eTF) >> 3);
			int sizeLine(sizePixel * pRects[i].width);

			const unsigned char* pSrc(&pData[pRects[i].srcY * pitch + sizePixel * pRects[i].srcX]);
			unsigned char* pDst(&(((unsigned char*) lr.pBits)[(pRects[i].dstY - rc.top) * lr.Pitch + sizePixel * (pRects[i].dstX - rc.left)]));

			for (int y(0); y<pRects[i].height; ++y)
			{
				memcpy(pDst, pSrc, sizeLine);
				pSrc += pitch;
				pDst += lr.Pitch;
			}
		}
		pSurf->UnlockRect();
		successful = true;
	}
	SAFE_RELEASE(pSurf);
	return successful;
#	endif
#elif defined(DIRECT3D10)
	ID3D10Texture2D* pTex((ID3D10Texture2D*) pTexture->GetDeviceTexture());
	if (!pTex)
		return false;

	for (int i(0); i<numRects; ++i)
	{
		int sizePixel(CTexture::BitsPerPixel(eTF) >> 3);
		const unsigned char* pSrc(&pData[pRects[i].srcY * pitch + sizePixel * pRects[i].srcX]);

		D3D10_BOX box = {pRects[i].dstX, pRects[i].dstY, 0, pRects[i].dstX + pRects[i].width, pRects[i].dstY + pRects[i].height, 1};
		m_pd3dDevice->UpdateSubresource(pTex, mipLevel, &box, pSrc, (unsigned int) pitch, 0);
	}
	return true;
#else
	assert(!"CD3D9Renderer::SF_UpdateTexture() - Not implemented for this platform!");
	return false;
#endif	
}

#endif //EXCLUDE_SCALEFORM_SDK

//========================================================================================================

bool CRenderer::EF_TryToMerge(CRenderObject *pObjN, CRenderObject *pObjO, CRendElement *pRE)
{
  if (!m_RP.m_pRE || m_RP.m_pRE != pRE || pRE->mfGetType() != eDATA_Mesh)
    return false;

  // Batching/Instancing case
  if ((pObjN->m_ObjFlags ^ pObjO->m_ObjFlags) & FOB_MASK_AFFECTS_MERGING)
    return false;

  if (pObjN->m_DynLMMask != pObjO->m_DynLMMask)
    return false;

  if (pObjN->m_nMaterialLayers != pObjO->m_nMaterialLayers)
    return false;

  if (*(uint *)(&pObjN->m_nRAEId) != *(uint *)(&pObjO->m_nRAEId))
    return false;

  if ((INT_PTR)pObjN->m_pShadowCasters != (INT_PTR)pObjO->m_pShadowCasters)
    return false;
	if ((INT_PTR)pObjN->m_pCharInstance != (INT_PTR)pObjO->m_pCharInstance)
		return false;

	const SRenderObjData* pObjDataN = pObjN->GetObjData();
	const SRenderObjData* pObjDataO = pObjO->GetObjData();
	if (pObjDataO && pObjDataO->m_pScatterBuffer)
		return false;
	if (pObjDataN && pObjDataN->m_pScatterBuffer)
		return false;
	if (pObjDataO && pObjDataN && memcmp(&pObjDataN->m_VertexScatterTransformZ, &pObjDataO->m_VertexScatterTransformZ, sizeof(pObjDataN->m_VertexScatterTransformZ)))
		return false;

  if (m_RP.m_ClipPlaneEnabled)
  {
    if (pRE->mfCullByClipPlane(pObjN))
      return true;
  }

  if (!m_RP.m_ObjectInstances.Num())
    m_RP.m_ObjectInstances.AddElem(pObjO);
  m_RP.m_ObjectInstances.AddElem(pObjN);
  m_RP.m_ObjFlags |= pObjN->m_ObjFlags & FOB_SELECTED;
  if (m_RP.m_fMinDistance > pObjN->m_fDistance)
    m_RP.m_fMinDistance = pObjN->m_fDistance;
  if (pObjN->GetInstanceInfo())
    m_RP.m_FlagsPerFlush |= RBSI_INSTANCED;
  return true;
}

// Note: If you add any new technique, update this list with it's name, in correct order (used for debug output)
static char *sDescList[] = 
{ 
  "NULL", 
  "Preprocess", 
  "General", 
  "TerrainLayer", 
  "Decal", 
  "WaterVolume", 
  "Transparent", 
  "Water", 
  "PostProcess", 
  "AfterPostprocess",
  "ShadowGen",
  "ShadowPass",
  "RefractPass"
};
static char *sBatchList[] = 
{ 
  "FB_GENERAL", 
  "FB_TRANSPARENT", 
  "FB_DETAIL", 
  "FB_Z", 
  "FB_GLOW", 
  "FB_SCATTER", 
  "FB_PREPROCESS", 
  "FB_MOTIONBLUR", 
  "FB_REFRACTIVE",
  "FB_MULTILAYERS",
  "FB_CAUSTICS",
  "FB_CUSTOM_RENDER",
  "FB_RAIN"
};

// Process render items list [nList] from [nums] to [nume]
// 1. Sorting of the list
// 2. Preprocess shaders handling
// 3. Process sorted ordered list of render items

void CD3D9Renderer::FX_SortRenderList(int nList, int nAW, int nR)
{
  int nStart = SRendItem::m_StartRI[nR][nAW][nList];
  int nEnd   = SRendItem::m_EndRI[nR][nAW][nList];
  int n = nEnd - nStart;
  if (!n)
    return;

  switch (nList)
  {    
  case EFSLIST_PREPROCESS:
    {
      PROFILE_FRAME(State_SortingPre);
      SRendItem::mfSortPreprocess(&SRendItem::m_RendItems[nAW][nList][nStart], n, nList, nAW);
    }
    break;
  case EFSLIST_HDRPOSTPROCESS:    
  case EFSLIST_POSTPROCESS:    
    {
      PROFILE_FRAME(State_SortingPost);
      // Don't sort post-process!!!
      //SRendItem::mfSortPreprocess(&SRendItem::m_RendItems[nAW][nList][nStart], n, nList, nAW);
    }
    break;
  case EFSLIST_WATER_VOLUMES:
  case EFSLIST_TRANSP:
  case EFSLIST_WATER:
  case EFSLIST_REFRACTPASS:
    {
      PROFILE_FRAME(State_SortingDist); 
      SRendItem::mfSortByDist(&SRendItem::m_RendItems[nAW][nList][nStart], n, nList, nAW, false);
    }
    break;
  case EFSLIST_DECAL:
    {
      PROFILE_FRAME(State_SortingDecals); 
      SRendItem::mfSortByDist(&SRendItem::m_RendItems[nAW][nList][nStart], n, nList, nAW, true);
    }
    break;
  case EFSLIST_GENERAL:
  case EFSLIST_AFTER_POSTPROCESS:
  case EFSLIST_AFTER_HDRPOSTPROCESS:
    {
      PROFILE_FRAME(State_SortingLight);
      SRendItem::mfSortByLight(&SRendItem::m_RendItems[nAW][nList][nStart], n, nList, true, true, false, nList == EFSLIST_DECAL, nAW);
    }
    break;
  case EFSLIST_TERRAINLAYER:
    {
      PROFILE_FRAME(State_SortingLight_TerrainLayers);
      SRendItem::mfSortByLight(&SRendItem::m_RendItems[nAW][nList][nStart], n, nList, true, false, true, false, nAW);
    }
    break;

  default:
    assert(0);
  }
}

void CD3D9Renderer::FX_SortRenderLists()
{
  int i, j;
  int nR = SRendItem::m_RecurseLevel-1;
  for (j=0; j<MAX_LIST_ORDER; j++)
  {
    for (i=0; i<EFSLIST_NUM; i++)
    {
      SRendItem::m_EndRI[nR][j][i] = SRendItem::m_RendItems[j][i].Num();
    }
  }

  //invalidate shadow masks
  for (int j=0; j<MAX_REND_LIGHT_GROUPS; j++)
  {
    SRendItem::m_ShadowsValidMask[SRendItem::m_RecurseLevel][j] = 0;
  }

  for (j=0; j<MAX_LIST_ORDER; j++)
  {
    for (i=1; i<EFSLIST_NUM; i++)
    {
      FX_SortRenderList(i, j, nR);
    }
  }
}

// Init states before rendering of the scene
void CD3D9Renderer::EF_PreRender(int Stage)
{
  uint i;
  if (m_bDeviceLost)
    return;

  if (Stage & 1)
  { // Before preprocess
    m_RP.m_pSunLight = NULL;
    m_RP.m_RealTime = iTimer->GetCurrTime();

    m_RP.m_Flags = 0;
    m_RP.m_pPrevObject = NULL;
    m_RP.m_FrameObject++;

    EF_SetCameraInfo();

    for (i=0; i<m_RP.m_DLights[SRendItem::m_RecurseLevel].Num(); i++)
    {
      CDLight *dl = m_RP.m_DLights[SRendItem::m_RecurseLevel][i];
      if (dl->m_Flags & DLF_FAKE)
        continue;

      if (dl->m_Flags & DLF_SUN)
        m_RP.m_pSunLight = dl;
    }
  }

  CHWShader_D3D::mfSetGlobalParams();
  m_RP.m_DynLMask = 0;
  m_RP.m_PrevLMask = -1;
  EF_PushVP();
}

// Restore states after rendering of the scene
void CD3D9Renderer::EF_PostRender()
{
  //FrameProfiler f("CD3D9Renderer:EF_PostRender", iSystem );

  EF_ObjectChange(NULL, NULL, m_RP.m_VisObjects[0], NULL);
  m_RP.m_pRE = NULL;

  EF_ResetPipe();

  if (m_RP.m_PersFlags2 & RBPF2_SETCLIPPLANE)
  {
    EF_SetClipPlane(false, NULL, false);
    m_RP.m_PersFlags2 &= ~RBPF2_SETCLIPPLANE;
  }
  EF_PopVP();

  m_RP.m_FlagsShader_MD = 0;
  m_RP.m_FlagsShader_MDV = 0;
  m_RP.m_FlagsShader_LT = 0;
  m_RP.m_pCurObject = m_RP.m_VisObjects[0];
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_PersFlags |= RBPF_FP_DIRTY;
  m_RP.m_PrevLMask = -1;
}


// Object changing handling (skinning, shadow maps updating, initial states setting, ...)
bool CD3D9Renderer::EF_ObjectChange(CShader *Shader, SRenderShaderResources *Res, CRenderObject *obj, CRendElement *pRE)
{
  if ((obj->m_ObjFlags & FOB_NEAREST) && ((m_RP.m_PersFlags2 & RBPF2_DONTDRAWNEAREST) || CV_r_nodrawnear))
    return false;

  if (Shader)
  {
    if (m_RP.m_pIgnoreObject && m_RP.m_pIgnoreObject->m_pID == obj->m_pID)
      return false;
  }

  if (obj == m_RP.m_pPrevObject)
    return true;

  m_RP.m_FrameObject++;

  m_RP.m_pCurObject = obj;

  int flags = 0;
  if (obj->m_VisId)
  {
    // Skinning6
    if(obj->m_ObjFlags & FOB_NEAREST)
      flags |= RBF_NEAREST;

    if (CV_r_drawnearfov && (flags ^ m_RP.m_Flags) & RBF_NEAREST)
    {
      if (flags & RBF_NEAREST)
      {
        CCamera Cam = GetCamera();
        m_RP.m_PrevCamera = Cam;
        if (m_LogFile)
          Logv(SRendItem::m_RecurseLevel, "*** Prepare nearest Z range ***\n");
        // set nice fov for weapons

        float fFov = Cam.GetFov();

        if (CV_r_drawnearfov>1 && CV_r_drawnearfov<179)
          fFov = DEG2RAD(CV_r_drawnearfov);

        Cam.SetFrustum(Cam.GetViewSurfaceX(), Cam.GetViewSurfaceZ(), fFov, DRAW_NEAREST_MIN, DRAW_NEAREST_MAX);

        SetCamera(Cam);
#if defined (DIRECT3D9) || defined(OPENGL)
        m_NewViewport.MaxZ = 0.1f;
#elif defined (DIRECT3D10)
        m_NewViewport.MaxDepth = 0.1f;
#endif
        m_bViewportDirty = true;
        m_RP.m_Flags |= RBF_NEAREST;
      }
      else
      {
        if (m_LogFile)
          Logv(SRendItem::m_RecurseLevel, "*** Restore Z range ***\n");

        SetCamera(m_RP.m_PrevCamera);
#if defined (DIRECT3D9) || defined(OPENGL)
        m_NewViewport.MaxZ = m_RP.m_PrevCamera.GetZRangeMax();
#elif defined (DIRECT3D10)
        m_NewViewport.MaxDepth = m_RP.m_PrevCamera.GetZRangeMax();
#endif
        m_bViewportDirty = true;
        m_RP.m_Flags &= ~RBF_NEAREST;
      }
			m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF;
    }

    if(obj->m_ObjFlags & FOB_CUSTOM_CAMERA)
      flags |= RBF_CUSTOM_CAMERA;

    if ((flags ^ m_RP.m_Flags) & RBF_CUSTOM_CAMERA)
    {
      if (flags & RBF_CUSTOM_CAMERA)
      {
        CCamera Cam = GetCamera();
        m_RP.m_PrevCamera = Cam;
        if (m_LogFile)
          Logv(SRendItem::m_RecurseLevel, "*** Prepare custom camera ***\n");
        CCamera *pCam = obj->m_pCustomCamera;
        /*if (!pCam)
        {
        // Find camera in shader public params.
        for (unsigned int i = 0; i < obj->m_ShaderParams->size(); i++)
        {
        SShaderParam &sp = (*obj->m_ShaderParams)[i];
        if (sp.m_Type == eType_CAMERA)
        {
        pCam = sp.m_Value.m_pCamera;
        break;
        }
        }
        }*/
        if (pCam)
        {
          SetCamera(*pCam);
#if defined (DIRECT3D9) || defined(OPENGL)
          m_NewViewport.MinZ = pCam->GetZRangeMin();
          m_NewViewport.MaxZ = pCam->GetZRangeMax();
#elif defined (DIRECT3D10)
          m_NewViewport.MinDepth = pCam->GetZRangeMin();
          m_NewViewport.MaxDepth = pCam->GetZRangeMax();
#endif
          m_bViewportDirty = true;
        }
        m_RP.m_Flags |= RBF_CUSTOM_CAMERA;
      }
      else
      {
        if (m_LogFile)
          Logv(SRendItem::m_RecurseLevel, "*** Restore camera ***\n");

        SetCamera(m_RP.m_PrevCamera);
#if defined (DIRECT3D9) || defined(OPENGL)
        if (m_NewViewport.MinZ != m_RP.m_PrevCamera.GetZRangeMin() || m_NewViewport.MaxZ != m_RP.m_PrevCamera.GetZRangeMax())
        {
          m_NewViewport.MinZ = m_RP.m_PrevCamera.GetZRangeMin();
          m_NewViewport.MaxZ = m_RP.m_PrevCamera.GetZRangeMax();
          m_bViewportDirty = true;
        }
#elif defined (DIRECT3D10)
        if (m_NewViewport.MinDepth != m_RP.m_PrevCamera.GetZRangeMin() || m_NewViewport.MaxDepth != m_RP.m_PrevCamera.GetZRangeMax())
        {
          m_NewViewport.MinDepth = m_RP.m_PrevCamera.GetZRangeMin();
          m_NewViewport.MaxDepth = m_RP.m_PrevCamera.GetZRangeMax();
          m_bViewportDirty = true;
        }
#endif
        m_RP.m_Flags &= ~RBF_CUSTOM_CAMERA;
      }
    }
  }
  else
  {
    if (m_RP.m_Flags & (RBF_NEAREST | RBF_CUSTOM_CAMERA))
    {
      if (m_LogFile)
        Logv(SRendItem::m_RecurseLevel, "*** Restore Z range/camera ***\n");
      SetCamera(m_RP.m_PrevCamera);
#if defined (DIRECT3D9) || defined(OPENGL)
      m_NewViewport.MaxZ = 1.0f;
#elif defined (DIRECT3D10)
      m_NewViewport.MaxDepth = 1.0f;
#endif
      m_bViewportDirty = true;
      m_RP.m_Flags &= ~(RBF_NEAREST | RBF_CUSTOM_CAMERA);
    }
    m_ViewMatrix = m_CameraMatrix;
    // Restore transform
    m_matView->LoadMatrix(&m_CameraMatrix);
    m_RP.m_PersFlags |= RBPF_FP_MATRIXDIRTY;
  }
  m_RP.m_fMinDistance = obj->m_fDistance;
  m_RP.m_pPrevObject = obj;

  return true;
}

//=================================================================================
// Check buffer overflow during geometry batching
void CRenderer::EF_CheckOverflow(int nVerts, int nInds, CRendElement *re, int* nNewVerts, int* nNewInds)
{
  if (nNewVerts)
    *nNewVerts = nVerts;
  if (nNewInds)
    *nNewInds = nInds;

  if (m_RP.m_pRE || (m_RP.m_RendNumVerts+nVerts >= m_RP.m_MaxVerts || m_RP.m_RendNumIndices+nInds >= m_RP.m_MaxTris*3))
  {
    m_RP.m_pRenderFunc();
    if (nVerts >= m_RP.m_MaxVerts)
    {
      // iLog->Log("CD3D9Renderer::EF_CheckOverflow: numVerts >= MAX (%d > %d)\n", nVerts, m_RP.m_MaxVerts);
      *nNewVerts = m_RP.m_MaxVerts;
    }
    if (nInds >= m_RP.m_MaxTris*3)
    {
      // iLog->Log("CD3D9Renderer::EF_CheckOverflow: numIndices >= MAX (%d > %d)\n", nInds, m_RP.m_MaxTris*3);
      *nNewInds = m_RP.m_MaxTris*3;
    }
    EF_Start(m_RP.m_pShader, m_RP.m_nShaderTechnique, m_RP.m_pShaderResources, re);
    EF_StartMerging();
  }
}


// Start of the new shader pipeline (3D pipeline version)
void CRenderer::EF_Start(CShader *ef, int nTech, SRenderShaderResources *Res, CRendElement *re)
{
  assert(ef);		

  if (!ef)		// should not be 0, check to prevent crash
    return;

  m_RP.m_nNumRendPasses = 0;
  m_RP.m_FirstIndex = 0;
  m_RP.m_IndexOffset = 0;
  m_RP.m_FirstVertex = 0;
  m_RP.m_RendNumIndices = 0;
  m_RP.m_RendNumVerts = 0;
  m_RP.m_RendNumGroup = -1;
  m_RP.m_pShader = ef;
  m_RP.m_nShaderTechnique = nTech;
  m_RP.m_pShaderResources = Res;
  CTexture::m_pCurEnvTexture = NULL;
  m_RP.m_FlagsPerFlush = 0;

  m_RP.m_FlagsStreams_Decl = 0;
  m_RP.m_FlagsStreams_Stream = 0;
  m_RP.m_FlagsShader_RT = 0;
  m_RP.m_FlagsShader_MD = 0;
  m_RP.m_FlagsShader_MDV = 0;

  EF_ApplyShaderQuality(ef->m_eShaderType);

  if ((m_RP.m_PersFlags2 & RBPF2_HDR_FP16) && !(m_RP.m_nBatchFilter & FB_Z))
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HDR_MODE];
  if (m_RP.m_FSAAData.Type)
    m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_FSAA];

  m_RP.m_fCurOpacity = 1.0f;
  m_RP.m_CurVFormat = ef->m_VertexFormatId;
  m_RP.m_ObjFlags = m_RP.m_pCurObject->m_ObjFlags;
  m_RP.m_DynLMask = m_RP.m_pCurObject->m_DynLMMask;
  if (m_RP.m_pCurObject->GetInstanceInfo())
    m_RP.m_FlagsPerFlush |= RBSI_INSTANCED;
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_ObjectInstances.SetUse(0);

  m_RP.m_pRE = NULL;
  m_RP.m_Frame++;
}

//==============================================================================================

static void sBatchFilter(uint32 nFilter, char *sFilt)
{
  sFilt[0] = 0;
  int n = 0;
  for (int i=0; i<32; i++)
  {
    if (nFilter & (1<<i))
    {
      if (n)
        strcat(sFilt, "|");
      strcat(sFilt, sBatchList[i]);
      n++;
    }
  }
}

void CD3D9Renderer::FX_ProcessBatchesList(int nums, int nume, uint32 nBatchFilter)
{
  PROFILE_FRAME(DrawShader_ProcessBatchesList);

  int i;
  CShader *pShader, *pCurShader;
  SRenderShaderResources *pRes;
  CRenderObject *pObject, *pCurObject;
  int nTech;

  if (nume-nums == 0)
    return;

  m_RP.m_nBatchFilter = nBatchFilter;
  m_RP.m_bNotFirstPass = false;

  uint oldVal = -1;
  pCurObject = NULL;
  pCurShader = NULL;
  bool bIgnore = false;
  bool bChanged;

  int nList = m_RP.m_nPassGroupID; 
  int nAW = m_RP.m_nSortGroupID;

  if (SRendItem::m_RendItems[nAW][nList].Num() < nume)
  {
    assert(!"Invalid batch, not enough renderitems?!");
    return;
  }

#ifdef DO_RENDERLOG
  if (CV_r_log)
  {
    char sFilt[256];
    sBatchFilter(nBatchFilter, sFilt);
    Logv(SRendItem::m_RecurseLevel, "\n*** Start batch list %s (Filter: %s) (%s) ***\n", sDescList[nList], sFilt, nAW ? "After water" : "Before water");
  }
#endif

  bool bUseBatching = (m_RP.m_pRenderFunc == EF_Flush);
  m_RP.m_nCurLightGroup = -1;

  for (i=nums; i<nume; i++)
  {
    SRendItem *ri = &SRendItem::m_RendItems[nAW][nList][i];
    if (!(ri->nBatchFlags & nBatchFilter))
      continue;
    CRendElement *pRE = ri->Item;
    if (oldVal != ri->SortVal)
    {
      SRendItem::mfGet(ri->SortVal, nTech, pShader, pRes);
      bChanged = true;
    }
    else
      bChanged = false;
    pObject = ri->pObj;
    oldVal = ri->SortVal;
    if (pObject != pCurObject)
    {
      if (!bChanged && bUseBatching)
      {
        if (EF_TryToMerge(pObject, pCurObject, pRE))
          continue;
      }
      if (pCurShader)
      {
        m_RP.m_pRenderFunc();

        pCurShader = NULL;
        bChanged = true;
      }
      if (!EF_ObjectChange(pShader, pRes, pObject, pRE))
      {
        bIgnore = true;
        continue;
      }
      bIgnore = false;
      pCurObject = pObject;
    }

    if (bChanged)
    {
      if (pCurShader)
        m_RP.m_pRenderFunc();      
      EF_Start(pShader, nTech, pRes, pRE);
      pCurShader = pShader;
    }

    {
      //PROFILE_FRAME_TOTAL(Mesh_REPrepare);
      pRE->mfPrepare();
    }
  }
  if (pCurShader)
    m_RP.m_pRenderFunc();

#ifdef DO_RENDERLOG
  if (CV_r_log)
    Logv(SRendItem::m_RecurseLevel, "*** End batch list ***\n\n");
#endif
}

void CD3D9Renderer::FX_ProcessRenderList(int nums, int nume, int nList, int nAW, void (*RenderFunc)(), bool bLighting)
{
  if (nume-nums < 1)
    return;

  EF_PushMatrix();
  m_matProj->Push();

  m_RP.m_pRenderFunc = RenderFunc;

  m_RP.m_pCurObject = m_RP.m_VisObjects[0];
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_pPrevObject = m_RP.m_pCurObject;

  if (nList==EFSLIST_PREPROCESS)
  {
    // Perform pre-process operations for the current frame
    if (SRendItem::m_RendItems[nAW][nList][nums].nBatchFlags & 0xffff0000)
    {
      EF_PreRender(1);
      nums += FX_Preprocess(&SRendItem::m_RendItems[nAW][nList][0], nums, nume);
      EF_PostRender();
    }
  }
  EF_PreRender(3);

  int nPrevGroup = m_RP.m_nPassGroupID;
  int nPrevGroup2 = m_RP.m_nPassGroupDIP;
  int nPrevSortGroupID = m_RP.m_nSortGroupID;

  m_RP.m_nPassGroupID = nList; 
  m_RP.m_nPassGroupDIP = nList; 
  m_RP.m_nSortGroupID = nAW;
  if (m_RP.m_PersFlags & RBPF_SHADOWGEN)
    m_RP.m_nPassGroupDIP = EFSLIST_SHADOW_GEN;
  m_RP.m_Flags |= RBF_3D;

  bool bUseBatching = (RenderFunc == EF_Flush);

  if (bLighting && bUseBatching)
  {
    if ((nList != EFSLIST_TRANSP) || CV_r_usealphablend)
      FX_ProcessLightGroups(nums, nume);
  }
  else
  {
    m_RP.m_PersFlags2 &= ~RBPF2_LIGHTSTENCILCULL;

    if (nume>nums)
    {
    FX_ProcessBatchesList(nums, nume, FB_GENERAL);
      assert(m_RP.m_nPassGroupID == nList);
      assert(m_RP.m_nSortGroupID == nAW);
    FX_ProcessPostGroups(nums, nume);
  }
  }

  EF_PostRender();

  EF_PopMatrix();
  m_matProj->Pop();

  m_RP.m_nPassGroupID = nPrevGroup;
  m_RP.m_nPassGroupDIP = nPrevGroup2;
  m_RP.m_nSortGroupID = nPrevSortGroupID;
}

//////////////////////////////////////////////////////////////////////////

bool CD3D9Renderer::FX_ProcessLightsListForLightGroup(int nGroup, SRendLightGroup *pGr, int nOffsRI)
{
  uint j;

  m_RP.m_PersFlags2 |= RBPF2_DRAWLIGHTS;
  m_RP.m_pRE = NULL;
  m_RP.m_pShader = NULL;

  CShader *pShader, *pCurShader;
  SRenderShaderResources *pRes;
  CRenderObject *pObject, *pCurObject;
  int nTech;

  uint oldVal = -1;
  pCurObject = NULL;
  pCurShader = NULL;
  bool bIgnore = false;
  bool bChanged;

  // stencil cull pre-passes
  if (CV_r_optimisedlightsetup == 3 && m_RP.m_nPassGroupID != EFSLIST_TRANSP && m_RP.m_nPassGroupID != EFSLIST_REFRACTPASS && nGroup < 0x4 && SRendItem::m_RecurseLevel == 1)
  {
    EF_ClearBuffers(FRT_CLEAR_STENCIL|FRT_CLEAR_IMMEDIATE, NULL, 1);
    m_RP.m_PersFlags2 |= RBPF2_LIGHTSTENCILCULL;
    FX_StencilCullPassForLightGroup(nGroup);
  }
  else
  {
    m_RP.m_PersFlags2 &= ~RBPF2_LIGHTSTENCILCULL;
  }
  uint32 nBatchFlags = m_RP.m_nBatchFilter;

  int nPrevGroup = m_RP.m_nCurLightGroup;
  m_RP.m_nCurLightGroup = (nGroup == MAX_REND_LIGHT_GROUPS) ? -1 : nGroup;
  assert(pGr!=NULL);
  m_RP.m_pCurrentLightGroup = pGr;

  bool bNotFirstPass;
  const int nAW = m_RP.m_nSortGroupID;
  const int nSize = pGr->RendItemsLights.size();
  for (j=0; j<nSize; j++)
  {
    uint nRI = pGr->RendItemsLights[j];
    bNotFirstPass = (nRI & 0x80000000) ? true : false;
    nRI = (nRI & 0xffff) + nOffsRI;
    SRendItem *ri = &SRendItem::m_RendItems[nAW][m_RP.m_nPassGroupID][nRI];
    if (!(ri->nBatchFlags & nBatchFlags))
      continue;

    CRendElement *pRE = ri->Item;
    if (oldVal != ri->SortVal)
    {
      SRendItem::mfGet(ri->SortVal, nTech, pShader, pRes);
      bChanged = true;
    }
    else
      bChanged = false;
    pObject = ri->pObj;
    oldVal = ri->SortVal;
    if (pObject != pCurObject)
    {
      if (!bChanged)
      {
        if (EF_TryToMerge(pObject, pCurObject, pRE))
          continue;
      }
      if (pCurShader)
      {
        m_RP.m_pRenderFunc();

        pCurShader = NULL;
        bChanged = true;
      }
      if (!EF_ObjectChange(pShader, pRes, pObject, pRE))
      {
        bIgnore = true;
        oldVal = ~0;
        continue;
      }
      bIgnore = false;
      pCurObject = pObject;
    }

    if (bChanged)
    {
      if (pCurShader)
      {
        m_RP.m_pRenderFunc();
      }

      EF_Start(pShader, nTech, pRes, pRE);
      pCurShader = pShader;
      m_RP.m_nMaxPasses = (ri->ObjSort >> 8) & 0xff;
    }

    {
      //PROFILE_FRAME_TOTAL(Mesh_REPrepare);
      pRE->mfPrepare();
      m_RP.m_bNotFirstPass = bNotFirstPass;
    }
    m_RP.m_bNotFirstPass = bNotFirstPass;

  }
  if (pCurShader)
    m_RP.m_pRenderFunc();

  m_RP.m_PersFlags2 &= ~RBPF2_DRAWLIGHTS;
  m_RP.m_nCurLightGroup = nPrevGroup;

  return true;
}

void CD3D9Renderer::FX_ProcessRenderList(int nList, uint32 nBatchFilter)
{
  int nR = SRendItem::m_RecurseLevel;
  EF_PreRender(3);

  int nPrevGroup = m_RP.m_nPassGroupID;
  int nPrevGroup2 = m_RP.m_nPassGroupDIP;
  int nPrevSortGroupID = m_RP.m_nSortGroupID;

  m_RP.m_pRenderFunc = EF_Flush;
  m_RP.m_nPassGroupID = nList;
  m_RP.m_nPassGroupDIP = nList;

  m_RP.m_nSortGroupID = 0;
  FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], nBatchFilter);

  m_RP.m_nSortGroupID = 1;
  FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], nBatchFilter);

  EF_PostRender();

  m_RP.m_nPassGroupID = nPrevGroup;
  m_RP.m_nPassGroupDIP = nPrevGroup2;
  m_RP.m_nSortGroupID = nPrevSortGroupID;
}

void CD3D9Renderer::FX_ProcessZPassRenderLists()
{
  int nR = SRendItem::m_RecurseLevel;
  if (nR > 1)
    return;
  int nList = EFSLIST_GENERAL;

  uint32 nBatchMask = SRendItem::BatchFlags(EFSLIST_GENERAL);
  uint32 nBatchMaskTransp = SRendItem::BatchFlags(EFSLIST_TRANSP);
  uint32 nBatchMaskDecal = SRendItem::BatchFlags(EFSLIST_DECAL);
    if ((nBatchMask|nBatchMaskTransp|nBatchMaskDecal) & FB_Z)
  {
#ifdef DO_RENDERLOG
    if (CV_r_log)
      Logv(SRendItem::m_RecurseLevel, "*** Start z-prepass ***\n");
#endif

    EF_PreRender(3);

    m_RP.m_pRenderFunc = EF_Flush;
    FX_ZScene(true, m_RP.m_bUseHDR, !(m_RP.m_nRendFlags & SHDF_DO_NOT_CLEAR_Z_BUFFER));
    EF_EnableATOC();

    if (nBatchMask & FB_Z)
    {
      m_RP.m_nPassGroupID = nList;
      m_RP.m_nPassGroupDIP = nList;

      m_RP.m_nSortGroupID = 0;

      FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_Z);
      m_RP.m_nSortGroupID = 1;
      FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_Z);
    }

    if (nBatchMaskDecal & FB_Z)
    {
      // Needed in case user explicitly forces zpass for decals
      int nList = EFSLIST_DECAL;

      m_RP.m_nPassGroupID = nList;
      m_RP.m_nPassGroupDIP = nList;

      m_RP.m_nSortGroupID = 0;

      FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_Z);
      m_RP.m_nSortGroupID = 1;
      FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_Z);
    }


    if (nBatchMaskTransp & FB_Z)
    {
      // Needed in case user explicitly forces zpass for alpha blended stuff

      int nList = EFSLIST_TRANSP;

      m_RP.m_nPassGroupID = nList;
      m_RP.m_nPassGroupDIP = nList;

      m_RP.m_nSortGroupID = 0;

      FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_Z);
      m_RP.m_nSortGroupID = 1;
      FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_Z);
    }

    EF_PostRender();

    EF_DisableATOC();
    FX_ZScene(false, m_RP.m_bUseHDR, false);

#ifdef DO_RENDERLOG
    if (CV_r_log)
      Logv(SRendItem::m_RecurseLevel, "*** End z-prepass ***\n");
#endif
  }
}

void CD3D9Renderer::FX_ProcessScatterRenderLists()
{
  int nR = SRendItem::m_RecurseLevel;
  if (nR > 1)
    return;
  int nList = EFSLIST_GENERAL;
  uint32 nBatchMask = SRendItem::BatchFlags(nList);
  if (nBatchMask & FB_SCATTER)
  {
#ifdef DO_RENDERLOG
    if (CV_r_log)
      Logv(SRendItem::m_RecurseLevel, "*** Start scatter-pass ***\n");
#endif

    EF_PreRender(3);

    //////////////////////////////////////////////////////////////////////////
    CTexture *tpScatterLayer = NULL;
    SDynTexture *pDynScatterLayer = NULL;

    //hack to improve half-res quality
    //int nWidth = max (GetWidth(),GetHeight());
    //int nHeight = nWidth;

    //TF: make pow2
    int nWidth = GetWidth()/2;
    int nHeight = GetHeight()/2;


    ETEX_Format etfScatLayer = m_FormatA16B16G16R16.IsValid()?eTF_A16B16G16R16F:eTF_A8R8G8B8;

    /*if (IsHDRModeEnabled())
    {

    }*/

    pDynScatterLayer = new SDynTexture(nWidth, nHeight, etfScatLayer, eTT_2D, FT_DONT_ANISO | FT_STATE_CLAMP, "TempScatterRT", 95);
    pDynScatterLayer->Update(nWidth, nHeight);
    tpScatterLayer = pDynScatterLayer->m_pTexture;

    CTexture::m_Text_ScatterLayer->Invalidate(nWidth, nHeight, etfScatLayer);


    SD3DSurface* pSepDepthSurf = FX_GetDepthSurface(nWidth, nHeight, false);
    gcpRendD3D->FX_PushRenderTarget(0, CTexture::m_Text_ScatterLayer, pSepDepthSurf);
    //gcpRendD3D->SetViewport(0, 0, nWidth, nHeight);
    ColorF clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gcpRendD3D->EF_ClearBuffers(FRT_CLEAR, &clearColor);
    //EF_Commit();

    //////////////////////////////////////////////////////////////////////////

    m_RP.m_pRenderFunc = EF_Flush;
    m_RP.m_nPassGroupID = nList;
    m_RP.m_nPassGroupDIP = nList;

    //first depth prepass
    FX_ScatterScene(true, false);

    m_RP.m_nSortGroupID = 0;
    FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_SCATTER);

    m_RP.m_nSortGroupID = 1;
    FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_SCATTER);

    //EF_PostRender();

    // depth accumulation pass
    FX_ScatterScene(true, true);

    m_RP.m_nSortGroupID = 0;
    FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_SCATTER);

    m_RP.m_nSortGroupID = 1;
    FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_SCATTER);

    FX_ScatterScene(false, false);
  
    FX_PopRenderTarget(0);

    EF_PostRender();
    //////////////////////////////////////////////////////////////////////////
    //remove blur from FX_ScatterScene
    //and put proper blur here

    //gcpRendD3D->EF_ResetPipe(); it's been made in EF_PostRender


    //is it correct parameters
#if !defined (DIRECT3D10)
    FX_ShadowBlur(0.1f, pDynScatterLayer, CTexture::m_Text_ScatterLayer, 1);
    FX_ShadowBlur(0.1f, pDynScatterLayer, CTexture::m_Text_ScatterLayer, 1);
#endif

    //////////////////////////////////////////////////////////////////////////

    SAFE_DELETE(pDynScatterLayer);

  #ifdef DO_RENDERLOG
      if (CV_r_log)
        Logv(SRendItem::m_RecurseLevel, "*** End scatter-pass ***\n");
  #endif
  }
}

void CD3D9Renderer::FX_ProcessRainRenderLists(int nList, int nAW, void (*RenderFunc)())
{
  int nR = SRendItem::m_RecurseLevel;
  if ((m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS) && nR <= 1)
  {
    uint32 nBatchMask = SRendItem::BatchFlags(nList);
    if (nBatchMask & FB_RAIN)
    {
      m_RP.m_PersFlags2 |= RBPF2_RAINPASS;

      EF_PreRender(3);

      short nLightGroupPrev = m_RP.m_nCurLightGroup;
      m_RP.m_nCurLightGroup = 0;

      m_RP.m_nPassGroupID = nList; 
      m_RP.m_nPassGroupDIP = nList; 
      m_RP.m_nSortGroupID = nAW;

      FX_ProcessBatchesList(SRendItem::m_StartRI[nR-1][m_RP.m_nSortGroupID][nList], SRendItem::m_EndRI[nR-1][m_RP.m_nSortGroupID][nList], FB_RAIN);

      m_RP.m_nCurLightGroup = nLightGroupPrev;
      
      EF_PostRender();

      m_RP.m_PersFlags2 &= ~RBPF2_RAINPASS;
    }
  }
}

void CD3D9Renderer::FX_ProcessPostRenderLists(uint32 nBatchFilter)
{
  int nR = SRendItem::m_RecurseLevel;
  if ((m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS) && nR <= 1)
  {
    int nList = EFSLIST_GENERAL;
    uint32 nBatchMask = SRendItem::BatchFlags(EFSLIST_GENERAL) | SRendItem::BatchFlags(EFSLIST_TRANSP) | SRendItem::BatchFlags(EFSLIST_DECAL);
    if (nBatchMask & nBatchFilter)
    {
      if (nBatchFilter == FB_GLOW)
        FX_GlowScene(true);
      else
      if (nBatchFilter == FB_MOTIONBLUR)
      {
        if( FX_MotionBlurScene(true) == false )
          return;
      }
      else
      if (nBatchFilter == FB_CUSTOM_RENDER)
        FX_CustomRenderScene(true);
      else
        assert(0);

      FX_ProcessRenderList(EFSLIST_GENERAL, nBatchFilter);
      FX_ProcessRenderList(EFSLIST_TRANSP, nBatchFilter);
      FX_ProcessRenderList(EFSLIST_DECAL, nBatchFilter);

      if (nBatchFilter == FB_GLOW)
        FX_GlowScene(false);
      else
      if (nBatchFilter == FB_MOTIONBLUR)
        FX_MotionBlurScene(false);
      else
      if (nBatchFilter == FB_CUSTOM_RENDER)
        FX_CustomRenderScene(false);
      else
        assert(0);
    }
  }
}

void CD3D9Renderer::FX_ProcessPostGroups(int nums, int nume)
{
  uint32 nBatchMask = SRendItem::m_GroupProperty[SRendItem::m_RecurseLevel][m_RP.m_nSortGroupID][m_RP.m_nPassGroupID].m_nBatchMask;
  if (nBatchMask & FB_MULTILAYERS && CV_r_usemateriallayers)
    FX_ProcessBatchesList(nums, nume, FB_MULTILAYERS);
  if ((nBatchMask & FB_CAUSTICS) && CV_r_watercaustics && gRenDev->m_RP.m_eQuality>0)
    FX_ProcessBatchesList(nums, nume, FB_CAUSTICS);
  if ((nBatchMask & FB_DETAIL) && CV_r_detailtextures)
    FX_ProcessBatchesList(nums, nume, FB_DETAIL);
}

void CD3D9Renderer::FX_ProcessShadows(int nList, int nAW)
{
  EF_PreRender(3);

  int nums = SRendItem::m_StartRI[SRendItem::m_RecurseLevel-1][nAW][nList];
  int nume = SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][nAW][nList];

  m_RP.m_pRenderFunc = EF_Flush;

  m_RP.m_pCurObject = m_RP.m_VisObjects[0];
  m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
  m_RP.m_pPrevObject = m_RP.m_pCurObject;

  int nPrevGroup = m_RP.m_nPassGroupID;
  int nPrevGroup2 = m_RP.m_nPassGroupDIP;
  int nPrevSortGroupID = m_RP.m_nSortGroupID;

  m_RP.m_nPassGroupID = nList; 
  m_RP.m_nPassGroupDIP = nList; 
  m_RP.m_nSortGroupID = nAW;
  if (m_RP.m_PersFlags & RBPF_SHADOWGEN)
    m_RP.m_nPassGroupDIP = EFSLIST_SHADOW_GEN;
  m_RP.m_Flags |= RBF_3D;

  // update screen space ambient occlusion mask
  if ((m_RP.m_PersFlags & RBPF_ALLOW_AO) && SRendItem::m_RecurseLevel==1 && GetFrameID(false) != m_RP.m_nAOMaskUpdateLastFrameId)
  {
    FX_ProcessAOTarget();
    m_RP.m_FillLights[SRendItem::m_RecurseLevel].SetUse(0);
  }

  if (!(m_RP.m_PersFlags & RBPF_SHADOWGEN))
  {
    for (int i=0; i<MAX_REND_LIGHT_GROUPS; i++)
    {
      if (m_RP.m_DLights[SRendItem::m_RecurseLevel].Num() < i*4)
        break;
      // First process all the shadows for the light group
      FX_ProcessShadowsListsForLightGroup(i, nums);
    }
  }

  m_RP.m_nPassGroupID = nPrevGroup;
  m_RP.m_nPassGroupDIP = nPrevGroup2;
  m_RP.m_nSortGroupID = nPrevSortGroupID;
}

void CD3D9Renderer::FX_ProcessLightGroups(int nums, int nume)
{
  int i, j;
  int nR = SRendItem::m_RecurseLevel;

#if !defined(XENON) && !defined(PS3)
  // update screen space ambient occlusion mask
  if(	m_RP.m_PersFlags & RBPF_ALLOW_AO && SRendItem::m_RecurseLevel==1 && GetFrameID(false) != m_RP.m_nAOMaskUpdateLastFrameId )
  {
    FX_ProcessAOTarget();
    m_RP.m_FillLights[SRendItem::m_RecurseLevel].SetUse(0);
  }

  if (m_RP.m_nPassGroupID != EFSLIST_DECAL && !(m_RP.m_PersFlags & RBPF_SHADOWGEN))
  {
    for (i=0; i<MAX_REND_LIGHT_GROUPS; i++)
    {
      if (m_RP.m_DLights[SRendItem::m_RecurseLevel].Num() < i*4)
        break;
      // First process all the shadows for the light group
      FX_ProcessShadowsListsForLightGroup(i, nums);
    }
  }
#endif

  m_RP.m_PrevLMask = -1;
  m_RP.m_nBatchFilter = FB_GENERAL;

  // Second draw light passes for light groups
  for (j=0; j<SRendItem::m_GroupProperty[nR][m_RP.m_nSortGroupID][m_RP.m_nPassGroupID].m_nSortGroups; j++)
  {    
    for (i=0; i<=MAX_REND_LIGHT_GROUPS; i++)
    {
      SRendLightGroup *pGr = &SRendItem::m_RenderLightGroups[nR][m_RP.m_nPassGroupID][m_RP.m_nSortGroupID][j][i];
      if (pGr->RendItemsLights.size())
      {
        m_RP.m_pShader = NULL;
        m_RP.m_pShaderResources = NULL;
        m_RP.m_pCurObject = m_RP.m_VisObjects[0];
        m_RP.m_pCurInstanceInfo = &m_RP.m_pCurObject->m_II;
        m_RP.m_pPrevObject = NULL;
        FX_ProcessLightsListForLightGroup(i, pGr, nums);        
      }
    }
  }
  FX_ProcessPostGroups(nums, nume);
}


// Process all render item lists
void CD3D9Renderer::FX_ProcessAllRenderLists(void (*RenderFunc)(), int nFlags)
{
  m_RP.m_bUseHDR = false;
  uint32 nSaveRendFlags = m_RP.m_nRendFlags;
  m_RP.m_nRendFlags = nFlags;
  int nPF2 = m_RP.m_PersFlags2;
  int nR = SRendItem::m_RecurseLevel-1;
  if (m_pNewTarget[0])
  {
    if (nFlags & SHDF_DO_NOT_RENDER_TARGET)
      m_pNewTarget[0]->m_bDontDraw = true;
    else
      m_pNewTarget[0]->m_bDontDraw = false;
  }
  if (nFlags & SHDF_CLEAR_SHADOW_MASK)
    m_RP.m_PersFlags2 |= RBPF2_CLEAR_SHADOW_MASK;

  if (CV_r_shadersdynamicbranching)
    FX_PrepareLightInfoTexture(true);
  else
    FX_PrepareLightInfoTexture(false);

#ifdef USE_HDR
  if ((nFlags & SHDF_ALLOWHDR) && IsHDRModeEnabled() && !(m_RP.m_PersFlags & RBPF_MAKESPRITE) && !CV_r_measureoverdraw)
  {
    m_RP.m_bUseHDR = true;
    if (SRendItem::m_RecurseLevel == 1)
      FX_HDRScene(m_RP.m_bUseHDR);
    if (m_RP.m_bUseHDR)
      m_RP.m_PersFlags2 |= RBPF2_HDR_FP16;
  }
  else
  {
    FX_HDRScene(false);
    m_RP.m_PersFlags2 &= ~RBPF2_HDR_FP16;
  }
#endif

  if ((nFlags & SHDF_ALLOWHDR) && !(m_RP.m_PersFlags & RBPF_MAKESPRITE))
  {
    ETEX_Format eTF = (m_RP.m_bUseHDR && m_nHDRType==1) ? eTF_A16B16G16R16F : eTF_A8R8G8B8;
    int nWidth = gcpRendD3D->GetWidth(); //m_d3dsdBackBuffer.Width;
    int nHeight = gcpRendD3D->GetHeight(); //m_d3dsdBackBuffer.Height;

    if (!CTexture::m_Text_SceneTarget || 
      ( (!CRenderer::CV_r_debug_extra_scenetarget_fsaa || (CTexture::m_Text_SceneTarget->GetFlags() & FT_USAGE_FSAA)) && CTexture::m_Text_SceneTarget->IsFSAAChanged()) || 
      (CRenderer::CV_r_debug_extra_scenetarget_fsaa && (CTexture::m_Text_SceneTarget->GetFlags() & FT_USAGE_FSAA) ) || 
      CTexture::m_Text_SceneTarget->GetDstFormat() != eTF || CTexture::m_Text_SceneTarget->GetWidth() != nWidth || CTexture::m_Text_SceneTarget->GetHeight() != nHeight)
      CTexture::GenerateSceneMap(eTF);
  }

  // Prepare post processing
  bool bAllowPostProcess = (nFlags & SHDF_ALLOWPOSTPROCESS) && !nR && (m_RP.m_PersFlags & RBPF_POSTPROCESS) && (CV_r_postprocess_effects) && !CV_r_measureoverdraw && !(m_RP.m_PersFlags & RBPF_MAKESPRITE);  
  FX_PostProcessScene(bAllowPostProcess);

  if(nFlags & SHDF_ALLOW_AO)
    m_RP.m_PersFlags |= RBPF_ALLOW_AO;
  else
    m_RP.m_PersFlags &= ~RBPF_ALLOW_AO;

  FX_SortRenderLists();

  if (!nR)
  {
    if ((nFlags & SHDF_ALLOWPOSTPROCESS) && !(m_RP.m_PersFlags & RBPF_MAKESPRITE))
      FX_ProcessRenderList(EFSLIST_PREPROCESS, 0, RenderFunc, false);

    //TODO: make support for forward shadow map rendering
    if (CV_r_ShadowsForwardPass && !(nFlags & SHDF_ZPASS_ONLY)) //|SHDF_ALLOWPOSTPROCESS
      FX_PrepareAllDepthMaps();
  }

  // XENON: Generate shadow masks before rendering of main scene
#ifdef XENON
  FX_ProcessShadows(EFSLIST_GENERAL, 0);
  FX_ProcessShadows(EFSLIST_GENERAL, 1);
#endif

  if (!(nFlags & SHDF_ZPASS_ONLY)) 
  {
    if ((nFlags & (SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS)) && CV_r_usezpass)
      FX_ProcessZPassRenderLists();

    if (!(m_RP.m_PersFlags & RBPF_MAKESPRITE))
      FX_ProcessScatterRenderLists();

    bool bEmpty = SRendItem::IsListEmpty(EFSLIST_GENERAL);
    if (!nR && !bEmpty && m_FS.m_bEnable && CV_r_usezpass)
      m_RP.m_PersFlags2 |= RBPF2_SCENEPASS | RBPF2_NOSHADERFOG;

    bool bLighting = (m_RP.m_PersFlags & RBPF_SHADOWGEN) == 0;

    FX_ProcessRenderList(EFSLIST_GENERAL, 0, RenderFunc, bLighting);         // Sorted list without preprocess
    FX_ProcessRenderList(EFSLIST_TERRAINLAYER, 0, RenderFunc, bLighting);    // Unsorted list without preprocess
    FX_ProcessRenderList(EFSLIST_GENERAL, 1, RenderFunc, bLighting);         // Sorted list without preprocess
    FX_ProcessRenderList(EFSLIST_TERRAINLAYER, 1, RenderFunc, bLighting);    // Unsorted list without preprocess

    FX_ProcessRenderList(EFSLIST_DECAL, 0, RenderFunc, bLighting);           // Sorted list without preprocess
    FX_ProcessRenderList(EFSLIST_DECAL, 1, RenderFunc, bLighting);           // Sorted list without preprocess

    /* SSIL research by Vlad
    if(	CV_r_SSAO && m_RP.m_PersFlags & RBPF_ALLOW_AO && SRendItem::m_RecurseLevel==1 && GetFrameID(false) != m_RP.m_nAOMaskUpdateLastFrameId )
    {
      FX_ProcessAOTarget();
      m_RP.m_FillLights[SRendItem::m_RecurseLevel].SetUse(0);

      // Modulate screen by colored SSAO buffer

      {
        PROFILE_SHADER_START;
        FX_ScreenStretchRect(CTexture::m_Text_SceneTarget);
        PROFILE_SHADER_END;
      }

      {
        PROFILE_SHADER_START;
        STexState TexStatePoint = STexState(FILTER_POINT, true);
        CTexture::m_Text_AOTarget->Apply(0, CTexture::GetTexState(TexStatePoint));
        SetViewport(0, 0, GetWidth(), GetHeight());
        static CCryName techName("TextureToTexture");
        SetCullMode(R_CULL_NONE);
        DrawFullScreenQuad(CShaderMan::m_ShaderPostProcess, techName, 0, 0, 1, 1, 
//          GS_NODEPTHTEST);
//        GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);
        GS_NODEPTHTEST | GS_BLSRC_ZERO | GS_BLDST_SRCCOL);
        PROFILE_SHADER_END;
      }
    }*/

    if( CV_r_rain )
    {
      FX_ProcessRainRenderLists(EFSLIST_GENERAL, 0, RenderFunc);
      FX_ProcessRainRenderLists(EFSLIST_GENERAL, 1, RenderFunc);
    }

    if ((nFlags & SHDF_ALLOWPOSTPROCESS) && !nR && !(m_RP.m_PersFlags & RBPF_MAKESPRITE) && !bEmpty)
      FX_FogScene();

    if( nFlags & SHDF_ALLOW_WATER )
      FX_ProcessRenderList(EFSLIST_WATER_VOLUMES, 0, RenderFunc, false);    // Sorted list without preprocess

    FX_ProcessRenderList(EFSLIST_TRANSP, 0, RenderFunc, bLighting); // Unsorted list

    if(SRendItem::m_RecurseLevel == 1 && !(m_RP.m_PersFlags & RBPF_MAKESPRITE))
    {
      if (nFlags & SHDF_ALLOWPOSTPROCESS)
      {
        if (CV_r_refraction)
        {
          int nR = SRendItem::m_RecurseLevel-1;
          bool bEmpty = SRendItem::m_EndRI[nR][0][EFSLIST_REFRACTPASS] - SRendItem::m_StartRI[nR][0][EFSLIST_REFRACTPASS] == 0;
          if (!bEmpty && CTexture::m_Text_SceneTarget && CTexture::m_Text_SceneTarget->GetDeviceTexture())
          {
            if (!CRenderer::CV_r_debugrefraction)
              FX_ScreenStretchRect(CTexture::m_Text_SceneTarget);
            else
              CTexture::m_Text_SceneTarget->Fill(ColorF(1, 0, 0, 1));                        

            FX_ProcessRenderList(EFSLIST_REFRACTPASS, 0, RenderFunc, false); // Unsorted list            
          }
        }

        if( CV_r_rain )
          FX_ProcessRainRenderLists(EFSLIST_TRANSP, 0, RenderFunc);
      }

      if( nFlags & SHDF_ALLOW_WATER )
      {
        bool bEmpty = SRendItem::IsListEmpty(EFSLIST_WATER) && SRendItem::IsListEmpty(EFSLIST_WATER_VOLUMES);
        if (!bEmpty && CTexture::m_Text_SceneTarget && CTexture::m_Text_SceneTarget->GetDeviceTexture())
        {
          if (!CRenderer::CV_r_debugrefraction)
            FX_ScreenStretchRect(CTexture::m_Text_SceneTarget);
          else
            CTexture::m_Text_SceneTarget->Fill(ColorF(1, 0, 0, 1));
        }
      }
    }

    if( nFlags & SHDF_ALLOW_WATER )
    {
      FX_ProcessRenderList(EFSLIST_WATER, 0, RenderFunc, false); // Unsorted list
      FX_ProcessRenderList(EFSLIST_WATER, 1, RenderFunc, false); // Unsorted list
      FX_ProcessRenderList(EFSLIST_WATER_VOLUMES, 1, RenderFunc, false);    // Sorted list without preprocess 
    }

    FX_ProcessRenderList(EFSLIST_TRANSP, 1, RenderFunc, true); // Unsorted list

    if (nFlags & SHDF_ALLOWPOSTPROCESS)
    {
      if (SRendItem::m_RecurseLevel==1 && CV_r_refraction && !(m_RP.m_PersFlags & RBPF_MAKESPRITE)) 
      {
        int nR = SRendItem::m_RecurseLevel-1;

        bool bEmpty = SRendItem::m_EndRI[nR][1][EFSLIST_REFRACTPASS] - SRendItem::m_StartRI[nR][1][EFSLIST_REFRACTPASS] == 0;
        if (!bEmpty && CTexture::m_Text_SceneTarget && CTexture::m_Text_SceneTarget->GetDeviceTexture())
        {
          if( !CRenderer::CV_r_debugrefraction )
            FX_ScreenStretchRect(CTexture::m_Text_SceneTarget);
          else
            CTexture::m_Text_SceneTarget->Fill(ColorF(1, 0, 0, 1));                    

          FX_ProcessRenderList(EFSLIST_REFRACTPASS, 1, RenderFunc, false); // Unsorted list
        }
      }

      if( CV_r_rain )
        FX_ProcessRainRenderLists(EFSLIST_TRANSP, 1, RenderFunc);
    }

    if (CV_r_glow && bAllowPostProcess)
      FX_ProcessPostRenderLists(FB_GLOW);
    if (CRenderer::s_MotionBlurMode > 1)
      FX_ProcessPostRenderLists(FB_MOTIONBLUR);

    if (!nR)
    {
      FX_ProcessRenderList(EFSLIST_HDRPOSTPROCESS, 0, RenderFunc, false);       // Sorted list without preprocess of all fog passes and screen shaders
      FX_ProcessRenderList(EFSLIST_HDRPOSTPROCESS, 1, RenderFunc, false);       // Sorted list without preprocess of all fog passes and screen shaders
      FX_ProcessRenderList(EFSLIST_AFTER_HDRPOSTPROCESS, 0, RenderFunc, false); // for specific cases where rendering after tone mapping is needed
      FX_ProcessRenderList(EFSLIST_AFTER_HDRPOSTPROCESS, 1, RenderFunc, false);  
      FX_ProcessRenderList(EFSLIST_POSTPROCESS, 0, RenderFunc, false);       // Sorted list without preprocess of all fog passes and screen shaders
      FX_ProcessRenderList(EFSLIST_POSTPROCESS, 1, RenderFunc, false);       // Sorted list without preprocess of all fog passes and screen shaders
      FX_ProcessRenderList(EFSLIST_AFTER_POSTPROCESS, 0, RenderFunc, false); // for specific cases where rendering after all post effects is needed
      FX_ProcessRenderList(EFSLIST_AFTER_POSTPROCESS, 1, RenderFunc, false);  
    }
  }
  else
  {
    FX_ProcessRenderList(EFSLIST_GENERAL, 0, RenderFunc, true);    // Sorted list without preprocess
    FX_ProcessRenderList(EFSLIST_TERRAINLAYER, 0, RenderFunc, true);    // Unsorted list without preprocess
    FX_ProcessRenderList(EFSLIST_DECAL, 0, RenderFunc, true);    // Sorted list without preprocess
    FX_ProcessRenderList(EFSLIST_WATER_VOLUMES, 0, RenderFunc, false);    // Sorted list without preprocess

    FX_ProcessRenderList(EFSLIST_GENERAL, 1, RenderFunc, true);    // Sorted list without preprocess
    FX_ProcessRenderList(EFSLIST_TERRAINLAYER, 1, RenderFunc, true);    // Unsorted list without preprocess
    FX_ProcessRenderList(EFSLIST_DECAL, 1, RenderFunc, true);    // Sorted list without preprocess
    FX_ProcessRenderList(EFSLIST_WATER_VOLUMES, 1, RenderFunc, false);    // Sorted list without preprocess
  }

  m_RP.m_PersFlags2 = nPF2;
  m_RP.m_nRendFlags = nSaveRendFlags;
}


// Process all render item lists
void CD3D9Renderer::EF_EndEf3D(int nFlags)
{
  if(CV_r_flush == 2 && (nFlags & SHDF_ALLOWPOSTPROCESS) && SRendItem::m_RecurseLevel==1)
    FlushHardware(true);

  float time0 = iTimer->GetAsyncCurTime();

  assert(SRendItem::m_RecurseLevel >= 1);
  if (SRendItem::m_RecurseLevel < 1)
  {
    iLog->Log("Error: CRenderer::EF_EndEf3D without CRenderer::EF_StartEf");
    return;
  }

  CheckDeviceLost();
  if (m_bDeviceLost)
  {
    SRendItem::m_RecurseLevel--;
    return;
  }

  if (CV_r_nodrawshaders)
  {
    SetClearColor(Vec3(0,0,0));
    EF_ClearBuffers(FRT_CLEAR, NULL);
    SRendItem::m_RecurseLevel--;
    EF_PopObjectsList();
    return;
  }

  m_RP.m_RealTime = iTimer->GetCurrTime();
  int nAsyncShaders = CV_r_shadersasynccompiling;
  int nTexStr = CV_r_texturesstreamingsync;
  if (nFlags & SHDF_NOASYNC)
  {
    CV_r_shadersasynccompiling = 0;
    CV_r_texturesstreamingsync = 1;
  }

  if (CV_r_envlightcmdebug)
  {
    SEnvTexture *cm = NULL;
    Vec3 Pos = m_cam.GetPosition();
    cm = CTexture::FindSuitableEnvLCMap(Pos, false, -1, 0);
  }

  if (CV_r_excludeshader->GetString()[0] != '0')
  {
    char nm[256];
    strcpy(nm, CV_r_excludeshader->GetString());
    strlwr(nm);
    m_RP.m_sExcludeShader = nm;
  }
  else
    m_RP.m_sExcludeShader = "";

  if (CV_r_showonlyshader->GetString()[0] != '0')
  {
    char nm[256];
    strcpy(nm, CV_r_showonlyshader->GetString());
    strlwr(nm);
    m_RP.m_sShowOnlyShader = nm;
  }
  else
    m_RP.m_sShowOnlyShader = "";

  if (nFlags & SHDF_ALLOWPOSTPROCESS)
  {
    EF_UpdateSplashes(m_RP.m_RealTime);  
    EF_AddClientPolys();
    EF_AddClientPolys2D();
  }

  FX_ProcessAllRenderLists(EF_Flush, nFlags);

  EF_DrawDebugTools();
  SRendItem::m_RecurseLevel--;
  EF_PopObjectsList();

  m_RP.m_PS.m_fFlushTime += iTimer->GetAsyncCurTime()-time0;
  CV_r_shadersasynccompiling = nAsyncShaders;
  CV_r_texturesstreamingsync = nTexStr;
}

//========================================================================================================

