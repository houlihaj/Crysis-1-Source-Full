//////////////////////////////////////////////////////////////////////////
// A simple profiler useful for collecting multiple call times per frame
// and displaying their different average statistics.
// For usage, see the bottom of the file
//////////////////////////////////////////////////////////////////////////


#ifndef _SIMPLE_FRAME_PROFILER_
#define _SIMPLE_FRAME_PROFILER_

// set #if 0 here if you don't want profiling to be compiled in the code
#if ENABLE_FRAME_PROFILER
#define PROFILE_FRAME(id) FRAME_PROFILER_FAST( "Renderer:" #id,iSystem,PROFILE_RENDERER,g_bProfilerEnabled )
#define PROFILE_SHADER_START \
  bool bProfile;             \
  float time0 = 0;           \
  int nNumDips = 0;          \
  int nNumPolys = 0;         \
  if (CRenderer::CV_r_profileshaders==1 || (CRenderer::CV_r_profileshaders==2 && gcpRendD3D->m_RP.m_pCurObject && (gcpRendD3D->m_RP.m_pCurObject->m_ObjFlags & FOB_SELECTED))) \
  {                                   \
    bProfile = true;                  \
    gcpRendD3D->FX_Flush();           \
    time0 = iTimer->GetAsyncCurTime(); \
    gcpRendD3D->m_RP.m_fProfileTime = time0; \
    nNumPolys = gcpRendD3D->m_RP.m_PS.m_nPolygons[gcpRendD3D->m_RP.m_nPassGroupDIP]; \
    nNumDips = gcpRendD3D->m_RP.m_PS.m_nDIPs[gcpRendD3D->m_RP.m_nPassGroupDIP]; \
  }                                   \
  else                                \
    bProfile = false;                 \

#define PROFILE_SHADER_END            \
  float fTime;                        \
  if (bProfile)                       \
  {                                   \
    gcpRendD3D->FX_Flush();           \
    float time1 = iTimer->GetAsyncCurTime(); \
    fTime = time1 - time0;            \
  }                                   \
                                      \
  if (CRenderer::CV_r_profileshaders==1 || (CRenderer::CV_r_profileshaders==2 && gcpRendD3D->m_RP.m_pCurObject && (gcpRendD3D->m_RP.m_pCurObject->m_ObjFlags & FOB_SELECTED))) \
  {                                   \
    if (time0 == gcpRendD3D->m_RP.m_fProfileTime) \
    {                                          \
      SProfInfo pi;                     \
      pi.Time = fTime;                  \
      pi.NumPolys = gcpRendD3D->m_RP.m_PS.m_nPolygons[gcpRendD3D->m_RP.m_nPassGroupDIP] - nNumPolys;  \
      pi.NumDips = gcpRendD3D->m_RP.m_PS.m_nDIPs[gcpRendD3D->m_RP.m_nPassGroupDIP] - nNumDips;        \
      assert(gcpRendD3D->m_RP.m_pShader);   \
      assert(gcpRendD3D->m_RP.m_pCurTechnique);   \
      pi.pShader = gcpRendD3D->m_RP.m_pShader;      \
      pi.pTechnique = gcpRendD3D->m_RP.m_pCurTechnique; \
      pi.m_nItems = 0;                  \
      gcpRendD3D->m_RP.m_Profile.AddElem(pi);  \
    }                                   \
  }

#else
#define PROFILE_FRAME(id)
#define PROFILE_SHADER_START
#define PROFILE_SHADER_END
#endif


#endif