////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_water.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef CWATER_H
#define CWATER_H 

class COcean : public Cry3DEngineBase
{
  PodArray<struct_VERTEX_FORMAT_P3F> Verts_DWQ;
  PodArray<ushort> Indices_DWQ;
  IRenderMesh *m_pRenderMeshWaters[MAX_RECURSION_LEVELS];

  // water bottom geometry
  PodArray<struct_VERTEX_FORMAT_P3F> OceanBottom_Verts_DWQ;
  PodArray<ushort> OceanBottom_Indices_DWQ;
  IRenderMesh *m_pOceanBottomRenderMeshWaters;

  IMaterial * m_pTerrainWaterMat;
  IMaterial * m_pOceanBottomMat;
  class CREOcclusionQuery * m_pREOcclusionQueries[MAX_RECURSION_LEVELS];
  IShader * m_pShaderOcclusionQuery;
  int m_nLastVisibleFrameId;
  float m_fLastVisibleFrameTime;
  static uint32 m_nVisiblePixelsCount;
  int m_nBottomTexId;
  float m_fLastFov;
  CCamera m_Camera;

  float m_fRECustomData[8]; // used for passing data to renderer
  float m_fREOceanBottomCustomData[8]; // used for passing data to renderer

  // ocean fog related members
  CREWaterVolume::SParams m_wvParams;
  CREWaterVolume::SOceanParams m_wvoParams;
  _smart_ptr< IMaterial > m_pWaterBodyIntoMat;
  _smart_ptr< IMaterial > m_pWaterBodyOutofMat;
  _smart_ptr< IMaterial > m_pWaterBodyIntoMatLowSpec;
  _smart_ptr< IMaterial > m_pWaterBodyOutofMatLowSpec;
  CREWaterVolume* m_pWVRE;
  std::vector<struct_VERTEX_FORMAT_P3F_TEX2F> m_wvVertices;
  std::vector<uint16> m_wvIndices;

  static ITimer *m_pTimer;

  static CREWaterOcean *m_pOceanRE;

  void RenderWaterVolume();
  void RenderWaterBottom(const int nRecursionLevel);

public:
  COcean(IMaterial * pMat);
  ~COcean();

  void SetLastFov(float fLastFov) {m_fLastFov = fLastFov;}
  void Render(const int nRecursionLevel);
  int GetMemoryUsage();
  bool IsWaterVisible();


  static void SetTimer(ITimer *pTimer);
  static float GetWave( const Vec3 &pPosition );
  static uint32 GetVisiblePixelsCount();
};

#endif // CWATER_H
