////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrain_water_quad.cpp
//  Version:     v1.00
//  Created:     28/8/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Create and draw terrain water geometry (screen space grid, cycle buffers)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain_water.h"
#include "CullBuffer.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include "VisAreas.h"

ITimer *COcean::m_pTimer = 0;
CREWaterOcean *COcean::m_pOceanRE = 0;
uint32 COcean::m_nVisiblePixelsCount = 0;

COcean::COcean(IMaterial * pMat)
{ 
  memset(m_pRenderMeshWaters,0,sizeof(m_pRenderMeshWaters));
  m_pOceanBottomRenderMeshWaters = 0;

  m_pTerrainWaterMat = pMat;
  m_fLastFov=0;
  m_fLastVisibleFrameTime = 0.0f;

  //  m_pTimer = 0;

  m_pShaderOcclusionQuery = 0;
  if( GetRenderer()->GetFeatures() & RFT_OCCLUSIONTEST )
    m_pShaderOcclusionQuery = GetRenderer()->EF_LoadShader("OcclusionTest", EF_SYSTEM);

  //m_pOceanBottomShader = GetRenderer()->EF_LoadShader("OceanBottom", EF_SYSTEM);

  memset(m_pREOcclusionQueries, 0, sizeof(m_pREOcclusionQueries));

  m_nLastVisibleFrameId = 0;

  m_pOceanBottomMat = GetMatMan()->LoadMaterial( "Materials/Ocean/ocean_bottom", false );
  m_pWaterBodyIntoMat = GetMatMan()->LoadMaterial( "Materials/Fog/OceanInto", false );
  m_pWaterBodyOutofMat = GetMatMan()->LoadMaterial( "Materials/Fog/OceanOutof", false );
  m_pWaterBodyIntoMatLowSpec = GetMatMan()->LoadMaterial( "Materials/Fog/OceanIntoLowSpec", false );
  m_pWaterBodyOutofMatLowSpec = GetMatMan()->LoadMaterial( "Materials/Fog/OceanOutofLowSpec", false );

  m_pWVRE = static_cast<CREWaterVolume*>( GetRenderer()->EF_CreateRE( eDATA_WaterVolume ) );
  if( m_pWVRE )
  {
    m_pWVRE->m_drawWaterSurface = false;
    m_pWVRE->m_pParams = &m_wvParams;
    m_pWVRE->m_pOceanParams = &m_wvoParams;
  }

  m_pOceanRE = static_cast<CREWaterOcean*>( GetRenderer()->EF_CreateRE( eDATA_WaterOcean ) );
}


COcean::~COcean()
{
  for( int x=0; x < MAX_RECURSION_LEVELS; ++x )
  {
    GetRenderer()->DeleteRenderMesh(m_pRenderMeshWaters[x]);
    SAFE_RELEASE( m_pREOcclusionQueries[x] );
  }

  if( m_pOceanBottomRenderMeshWaters )
    GetRenderer()->DeleteRenderMesh(m_pOceanBottomRenderMeshWaters);
}

int COcean::GetMemoryUsage() 
{
  int nSize=0;

  nSize += sizeofVector(Indices_DWQ);
  nSize += sizeofVector(Verts_DWQ);
  nSize += sizeofVector(OceanBottom_Verts_DWQ);
  nSize += sizeofVector(OceanBottom_Indices_DWQ);

  return nSize;
}

void COcean::Render(const int nRecursionLevel)
{
  C3DEngine * p3DEngine = (C3DEngine*)Get3DEngine();  
  IRenderer* pRenderer( GetRenderer() );
  if(nRecursionLevel>0 ||  GetCVars()->e_water_ocean == 0 || !m_pTerrainWaterMat)
    return;

  int nBufID = 0;//(GetFrameID() & 1);

  Vec3 vCamPos = GetCamera().GetPosition();
  float fWaterLevel = p3DEngine->GetWaterLevel();
  int nTesselationAmount = GetCVars()->e_water_tesselation_amount;
  nTesselationAmount = max( nTesselationAmount, 1);

  // No hardware FFT support
  bool bOceanFFT = false;
  if( ( pRenderer->GetFeatures() & (RFT_HW_VERTEXTEXTURES) && GetCVars()->e_water_ocean_fft) && pRenderer->EF_GetShaderQuality( eST_Water ) >= eSQ_High )
    bOceanFFT = true;

  if(vCamPos.z < fWaterLevel)  
  {  
    // if camera is in indoors and lower than ocean level 
    // and exit portals are higher than ocean level - skip ocean rendering
    CVisArea * pVisArea = (CVisArea *)p3DEngine->GetVisAreaFromPos(vCamPos);
    if(pVisArea && !pVisArea->IsPortal())
    {
      for(int i=0; i<pVisArea->m_lstConnections.Count(); i++)
      {
        if(pVisArea->m_lstConnections[i]->IsConnectedToOutdoor() && pVisArea->m_lstConnections[i]->m_boxArea.min.z<fWaterLevel)
          break; // there is portal making ocean visible

        if(i==pVisArea->m_lstConnections.Count())
          return; // ocean surface is not visible 
      }
    }
  }

  CRenderObject * pObject = GetRenderer()->EF_GetObject(true);
  if (!pObject)
    return;
  pObject->m_II.m_Matrix.SetIdentity();
  pObject->m_pID = this;

  bool bWaterVisible = IsWaterVisible();
  float fWaterPlaneSize = GetCamera().GetFarPlane();

  // Check if water surface occluded
  if( fabs(m_fLastFov - GetCamera().GetFov())<0.01f && GetCVars()->e_hw_occlusion_culling_water && (GetRenderer()->GetFeatures() & RFT_OCCLUSIONTEST) && (nRecursionLevel==0))
  {
    AABB boxOcean( Vec3( vCamPos.x-fWaterPlaneSize, vCamPos.y-fWaterPlaneSize,fWaterLevel ),
      Vec3( vCamPos.x+fWaterPlaneSize, vCamPos.y+fWaterPlaneSize,fWaterLevel ) );    

    if((!GetVisAreaManager()->IsOceanVisible() && GetCamera().IsAABBVisible_EM(boxOcean)) ||
      ( GetVisAreaManager()->IsOceanVisible() && GetCamera().IsAABBVisible_E(boxOcean)))
    {
      // make element if not ready
      if(!m_pREOcclusionQueries[nRecursionLevel])
      {
        m_pREOcclusionQueries[nRecursionLevel] = (CREOcclusionQuery *)GetRenderer()->EF_CreateRE( eDATA_OcclusionQuery );
        m_pREOcclusionQueries[nRecursionLevel]->SetRenderMeshBox((CRenderMesh*)GetObjManager()->m_pRMBox);
      }

      // get last test result
      //if((m_pREOcclusionQueries[nRecursionLevel]->m_nCheckFrame - GetFrameID())<2)
      {
        COcean::m_nVisiblePixelsCount = m_pREOcclusionQueries[nRecursionLevel]->GetVisSamplesCount();
        if(COcean::m_nVisiblePixelsCount>16)
        { 
          m_nLastVisibleFrameId = GetFrameID();
          bWaterVisible = true;
        }
      }

      // request new test    
      m_pREOcclusionQueries[nRecursionLevel]->SetBBoxMin( Vec3(boxOcean.min.x, boxOcean.min.y, boxOcean.min.z - 1.0f) );
      m_pREOcclusionQueries[nRecursionLevel]->SetBBoxMax( Vec3(boxOcean.max.x, boxOcean.max.y, boxOcean.max.z) );

      if(!m_pREOcclusionQueries[nRecursionLevel]->GetDrawnFrame() || m_pREOcclusionQueries[nRecursionLevel]->mfReadResult_Try())	
      {
        SShaderItem pOccQuerySI(m_pShaderOcclusionQuery);
        CRenderObject *pObj = GetIdentityCRenderObject();
        if (!pObj)
          return;
        GetRenderer()->EF_AddEf(m_pREOcclusionQueries[nRecursionLevel], pOccQuerySI, pObj, EFSLIST_WATER_VOLUMES, 0); 
      }
    }
  }
  else
  {
    m_nLastVisibleFrameId = GetFrameID();
    bWaterVisible = true;
  }

  if( bWaterVisible || vCamPos.z  < fWaterLevel )
  {
    m_p3DEngine->SetOceanRenderFlags( OCR_OCEANVOLUME_VISIBLE );

    m_fLastFov = GetCamera().GetFov(); 

    if( bWaterVisible ) // cull water surface
    {    
      // Calculate water geometry and update vertex buffers
      int nScrGridSize = 20 * nTesselationAmount; 

      // Store permanently the swath width
      static int swathWidth = 0;
      static bool bUsingFFT = false;

      // Generate screen space grid
      if( (bOceanFFT && bUsingFFT != bOceanFFT) || swathWidth != GetCVars()->e_water_tesselation_swath_width || !Verts_DWQ.Count() || !Indices_DWQ.Count() ||  nScrGridSize * nScrGridSize != Verts_DWQ.Count() )
      {    
        Verts_DWQ.Clear();
        Indices_DWQ.Clear();

        bUsingFFT = bOceanFFT;

        // Render ocean with screen space tessellation

        int nScreenY = GetRenderer()->GetHeight();
        int nScreenX = GetRenderer()->GetWidth();

        if(!nScreenY || !nScreenX)
        {
          return;
        }

        float fRcpScrGridSize = 1.0f / (float) nScrGridSize;

        struct_VERTEX_FORMAT_P3F tmp;
        tmp.xyz.z = 0;

        // Grid vertex generation
        for( int y(0); y< nScrGridSize; ++y )
        {      
          tmp.xyz.y = (float) y * fRcpScrGridSize + fRcpScrGridSize;

          for( int x(0); x< nScrGridSize; ++x )
          {
            // vert 1
            tmp.xyz.x = (float) x * fRcpScrGridSize + fRcpScrGridSize;

            // store in z edges information
            float fx = fabs((tmp.xyz.x)*2.0f - 1.0f);
            float fy = fabs((tmp.xyz.y)*2.0f - 1.0f);
            //float fEdgeDisplace = cry_sqrtf(fx*fx + fy * fy);//max(fx, fy);
            float fEdgeDisplace = max(fx,fy);
            //cry_sqrtf(fx*fx + fy * fy);
            tmp.xyz.z = fEdgeDisplace; //!((y==0 ||y == nScrGridSize-1) || (x==0 || x == nScrGridSize-1));

            int n = Verts_DWQ.Count();
            Verts_DWQ.Add( tmp );        
          }
        }

        // Grid index generation
        // Update the swath width
        swathWidth = GetCVars()->e_water_tesselation_swath_width;

        if( swathWidth <= 0){
          // Normal approach
          int nIndex = 0;
          for( int y(0); y< nScrGridSize - 1; ++y )
          {      
            for( int x(0); x< nScrGridSize; ++x, ++nIndex )
            {        
              Indices_DWQ.Add( nIndex );
              Indices_DWQ.Add( nIndex + nScrGridSize); 
            }

            if( nScrGridSize - 2 > y)
            {
              Indices_DWQ.Add( nIndex + nScrGridSize -1); 
              Indices_DWQ.Add( nIndex ); 
            }
          }
        }else{
          // Boustrophedonic walk
          //
          //  0  1  2  3  4 
          //  5  6  7  8  9
          // 10 11 12 13 14
          // 15 16 17 18 19
          //
          // Should generate the follwing indices
          // 0 5 1 6 2 7 3 8 4 9 9 14 14 9 13 8 12 7 11 6 10 5 5 10 10 15 11 16 12 17 13 18 14 19 
          //

          int nIndex = 0;
          int startX = 0, endX = swathWidth-1;

          do{

            for( int y(0); y < nScrGridSize - 1; y += 2 )
            {
              // Forward
              for( int x(startX); x <= endX; ++x )
              {
                Indices_DWQ.Add( y * nScrGridSize + x );
                Indices_DWQ.Add( (y + 1) * nScrGridSize + x ); 
              }

              // Can we go backwards?
              if( y + 2 < nScrGridSize ) {
                // Restart strip by duplicating last and first of next strip
                Indices_DWQ.Add( (y + 1) * nScrGridSize + endX ); 
                Indices_DWQ.Add( (y + 2) * nScrGridSize + endX ); 

                //Backward
                for( int x(endX); x >= startX; --x )
                {
                  Indices_DWQ.Add( (y + 2) * nScrGridSize + x );
                  Indices_DWQ.Add( (y + 1) * nScrGridSize + x ); 
                }

                // Restart strip
                if(y + 2 == nScrGridSize - 1 && endX < nScrGridSize-1){
                  if(endX < nScrGridSize-1){
                    // Need to restart at the top of the next column
                    Indices_DWQ.Add( (nScrGridSize - 1) * nScrGridSize + startX ); 
                    Indices_DWQ.Add( endX );
                  }
                }else{
                  Indices_DWQ.Add( (y + 1) * nScrGridSize + startX ); 
                  Indices_DWQ.Add( (y + 2) * nScrGridSize + startX ); 
                }
              }else{
                // We can restart to next column
                if(endX < nScrGridSize-1){ 
                  // Restart strip for next swath
                  Indices_DWQ.Add( (nScrGridSize - 1) * nScrGridSize + endX ); 
                  Indices_DWQ.Add( endX );
                }
              }
            }	

            startX = endX;
            endX   = startX + swathWidth-1;

            if(endX >= nScrGridSize) endX = nScrGridSize-1;

          }while(startX < nScrGridSize-1);

        }

        IRenderMesh *pCurrRM = m_pRenderMeshWaters[nRecursionLevel];

        if(m_pRenderMeshWaters[nRecursionLevel])
        {
          GetRenderer()->DeleteRenderMesh(m_pRenderMeshWaters[nRecursionLevel]);
        }

        m_pRenderMeshWaters[nRecursionLevel] = GetRenderer()->CreateRenderMeshInitialized( 
          Verts_DWQ.GetElements(), 
          Verts_DWQ.Count(), 
          VERTEX_FORMAT_P3F, 
          Indices_DWQ.GetElements(), 
          Indices_DWQ.Count(), 
          R_PRIMV_TRIANGLE_STRIP,
          "OutdoorWaterGrid","OutdoorWaterGrid",
          eBT_Static);

        m_pRenderMeshWaters[nRecursionLevel]->SetChunk(m_pTerrainWaterMat, 0, Verts_DWQ.Count(), 0, Indices_DWQ.Count());  

        if( bOceanFFT )
          m_pOceanRE->Create(Verts_DWQ.Count(), Verts_DWQ.GetElements(), Indices_DWQ.Count(), Indices_DWQ.GetElements());
      }

      // make distance to water level near to zero
      m_pRenderMeshWaters[nRecursionLevel]->SetBBox(vCamPos, vCamPos);

      // test for multiple lights and shadows support

      PodArray<CDLight> * pSources = m_p3DEngine->GetDynamicLightSources();
      if( pSources->Count() && (pSources->GetAt(0).m_Flags & DLF_SUN) )
      {
        pObject->m_DynLMMask = 1;
        pObject->m_ObjFlags |= FOB_INSHADOW;
        if( GetCVars()->e_shadows && GetCVars()->e_shadows_water )
        {
          AABB oceanBox;
          float fWaterPlaneSize = GetCamera().GetFarPlane();
          oceanBox.min = Vec3( vCamPos.x-fWaterPlaneSize, vCamPos.y-fWaterPlaneSize,fWaterLevel-0.1f);
          oceanBox.max = Vec3( vCamPos.x+fWaterPlaneSize, vCamPos.y+fWaterPlaneSize,fWaterLevel+0.1f);
          pObject->m_pShadowCasters = Get3DEngine()->GetObjManager()->GetShadowFrustumsList(NULL, oceanBox, 0, pObject->m_DynLMMask, true);
        }
      }

      m_Camera = GetCamera();
      pObject->m_pCustomCamera = &m_Camera;
      pObject->m_fAlpha = 1.f;//m_fWaterTranspRatio;

      m_fRECustomData[0] = p3DEngine->m_oceanWindDirection;
      m_fRECustomData[1] = p3DEngine->m_oceanWindSpeed;
      m_fRECustomData[2] = p3DEngine->m_oceanWavesSpeed;
      m_fRECustomData[3] = p3DEngine->m_oceanWavesAmount;
      m_fRECustomData[4] = p3DEngine->m_oceanWavesSize;

      cry_sincosf(p3DEngine->m_oceanWindDirection, &m_fRECustomData[6], &m_fRECustomData[5]);
      m_fRECustomData[7] = fWaterLevel;

      pObject->m_CustomData = &m_fRECustomData;

      if( !GetCVars()->e_water_ocean_fft || !bOceanFFT  )
        m_pRenderMeshWaters[nRecursionLevel]->AddRenderElements(pObject, EFSLIST_WATER, 0, m_pTerrainWaterMat);
      else
      {
        SShaderItem shaderItem( m_pTerrainWaterMat->GetShaderItem( 0 ) );   
        pRenderer->EF_AddEf( m_pOceanRE, shaderItem, pObject, EFSLIST_WATER, 0);
      }
    }
    if( GetCVars()->e_water_ocean_bottom )
      RenderWaterBottom(nRecursionLevel);    

    RenderWaterVolume(); 
  }   
}

void COcean::RenderWaterBottom(const int nRecursionLevel)
{
  C3DEngine * p3DEngine = (C3DEngine*)Get3DEngine();  

  Vec3 vCamPos = GetCamera().GetPosition();
  float fWaterLevel = p3DEngine->GetWaterLevel();


  // Render ocean with screen space tessellation

  int nScreenY = GetRenderer()->GetHeight();
  int nScreenX = GetRenderer()->GetWidth();

  if(!nScreenY || !nScreenX)
    return;

  // Calculate water geometry and update vertex buffers
  int nScrGridSize = 5;  
  float fRcpScrGridSize = 1.0f / (float) nScrGridSize;

  if( !OceanBottom_Verts_DWQ.Count() || !OceanBottom_Indices_DWQ.Count() ||  nScrGridSize * nScrGridSize != OceanBottom_Verts_DWQ.Count() )
  {

    OceanBottom_Verts_DWQ.Clear();
    OceanBottom_Indices_DWQ.Clear();

    struct_VERTEX_FORMAT_P3F tmp;
    tmp.xyz.z = 0;

    // Grid vertex generation
    for( int y(0); y< nScrGridSize; ++y )
    {
      tmp.xyz.y = (float) y * fRcpScrGridSize + fRcpScrGridSize;
      for( int x(0); x< nScrGridSize; ++x )
      {
        tmp.xyz.x = (float) x * fRcpScrGridSize + fRcpScrGridSize;
        OceanBottom_Verts_DWQ.Add( tmp );        
      }
    }

    // Normal approach
    int nIndex = 0;
    for( int y(0); y< nScrGridSize - 1; ++y )
    {      
      for( int x(0); x< nScrGridSize; ++x, ++nIndex )
      {        
        OceanBottom_Indices_DWQ.Add( nIndex );
        OceanBottom_Indices_DWQ.Add( nIndex + nScrGridSize); 
      }

      if( nScrGridSize - 2 > y)
      {
        OceanBottom_Indices_DWQ.Add( nIndex + nScrGridSize -1); 
        OceanBottom_Indices_DWQ.Add( nIndex ); 
      }
    }

    IRenderMesh *pCurrRM = m_pOceanBottomRenderMeshWaters;
    if(m_pOceanBottomRenderMeshWaters)
      GetRenderer()->DeleteRenderMesh(m_pOceanBottomRenderMeshWaters);

    m_pOceanBottomRenderMeshWaters = GetRenderer()->CreateRenderMeshInitialized( 
      OceanBottom_Verts_DWQ.GetElements(), 
      OceanBottom_Verts_DWQ.Count(), 
      VERTEX_FORMAT_P3F, 
      OceanBottom_Indices_DWQ.GetElements(), 
      OceanBottom_Indices_DWQ.Count(), 
      R_PRIMV_TRIANGLE_STRIP,
      "OceanBottomGrid","OceanBottomGrid",
      eBT_Static);

    m_pOceanBottomRenderMeshWaters->SetChunk(m_pOceanBottomMat, 0, OceanBottom_Verts_DWQ.Count(), 0, OceanBottom_Indices_DWQ.Count());  
  }

  CRenderObject * pObject = GetRenderer()->EF_GetObject(true);
  if (!pObject)
    return;
  pObject->m_II.m_Matrix.SetIdentity();
  pObject->m_pID = this;

  // make distance to water level near to zero
  m_pOceanBottomRenderMeshWaters->SetBBox(vCamPos, vCamPos);

  m_Camera = GetCamera();
  pObject->m_pCustomCamera = &m_Camera;
  pObject->m_fAlpha = 1.f;

  m_pOceanBottomRenderMeshWaters->AddRenderElements(pObject, EFSLIST_GENERAL, 0, m_pOceanBottomMat); 
}

void COcean::RenderWaterVolume()
{
  if( !GetCVars()->e_fog || !GetCVars()->e_fogvolumes )
    return;

  IRenderer* pRenderer( GetRenderer() );
  C3DEngine* p3DEngine( Get3DEngine() );

  CRenderObject* pROVol( pRenderer->EF_GetObject( true ) );
  if (!pROVol)
    return;

  //bool isLowSpec(gEnv->pSystem->GetConfigSpec() == CONFIG_LOW_SPEC);
  bool isLowSpec(GetCVars()->e_obj_quality == CONFIG_LOW_SPEC);
  if (pROVol && m_pWVRE && ((!isLowSpec && m_pWaterBodyIntoMat && m_pWaterBodyOutofMat) || 
    (isLowSpec && m_pWaterBodyIntoMatLowSpec && m_pWaterBodyOutofMatLowSpec)))
  {

    Vec3 camPos( GetCamera().GetPosition() );
    float waterLevel( p3DEngine->GetOceanWaterLevel( camPos ) );
    Vec3 planeOrigin( camPos.x, camPos.y, waterLevel );

    // fill water volume param structure
    m_wvParams.m_center = planeOrigin;
    m_wvParams.m_fogPlane.Set( Vec3( 0, 0, 1 ), -waterLevel );


    if (isLowSpec)
    {
      m_wvParams.m_fogColor = 10.0f * m_p3DEngine->m_oceanFogColor * m_p3DEngine->m_oceanFogColorMultiplier;
      m_wvParams.m_fogDensity = m_p3DEngine->m_oceanFogDensity;

      m_wvoParams.m_fogColor = Vec3( 0, 0, 0 ); // not needed for low spec
      m_wvoParams.m_fogColorShallow = Vec3( 0, 0, 0 ); // not needed for low spec
      m_wvoParams.m_fogDensity = 0; // not needed for low spec

      m_pWVRE->m_pOceanParams = 0;
    }
    else
    {
      m_wvParams.m_fogColor = Vec3( 0, 0, 0 ); // not needed, we set ocean specific params below
      m_wvParams.m_fogDensity = 0; // not needed, we set ocean specific params below

      m_wvoParams.m_fogColor = m_p3DEngine->m_oceanFogColor * m_p3DEngine->m_oceanFogColorMultiplier;
      m_wvoParams.m_fogColorShallow = m_p3DEngine->m_oceanFogColorShallow * m_p3DEngine->m_oceanFogColorMultiplier;
      m_wvoParams.m_fogDensity = m_p3DEngine->m_oceanFogDensity;

      m_pWVRE->m_pOceanParams = &m_wvoParams;
    }




    float fHeight = camPos.z + m_wvParams.m_fogPlane.d;

    float distCamToFogPlane( fHeight)  ;
    m_wvParams.m_viewerCloseToWaterPlane = ( distCamToFogPlane ) < 0.5f;
    m_wvParams.m_viewerInsideVolume = distCamToFogPlane < 0.00f;

    // tessellate plane
    float planeSize( 2.0f * GetCamera().GetFarPlane() ); 
    size_t subDivSize( min(64, 1 + (int) (planeSize / 512.0f)) );
    size_t numSubDivVerts( (subDivSize + 1) * (subDivSize + 1) );

    if( m_wvVertices.size() != numSubDivVerts )
    {
      m_wvVertices.resize( numSubDivVerts );
      m_wvParams.m_pVertices = &m_wvVertices[0];
      m_wvParams.m_numVertices = m_wvVertices.size();

      m_wvIndices.resize( subDivSize * subDivSize * 6 );
      m_wvParams.m_pIndices = &m_wvIndices[0];
      m_wvParams.m_numIndices = m_wvIndices.size();

      size_t ind(0);
      for( int y(0); y < subDivSize; ++y )
      {
        for( int x(0); x < subDivSize; ++x, ind += 6 )
        {
          m_wvIndices[ind+0] = (y  ) * (subDivSize+1) + (x  );
          m_wvIndices[ind+1] = (y  ) * (subDivSize+1) + (x+1);
          m_wvIndices[ind+2] = (y+1) * (subDivSize+1) + (x+1);

          m_wvIndices[ind+3] = (y  ) * (subDivSize+1) + (x  );
          m_wvIndices[ind+4] = (y+1) * (subDivSize+1) + (x+1);
          m_wvIndices[ind+5] = (y+1) * (subDivSize+1) + (x  );
        }
      }
    }
    {				
      float xyDelta(2.0f * planeSize / (float) subDivSize);
      float zDelta(waterLevel - camPos.z);

      size_t ind(0);
      float yd(-planeSize);
      for (int y(0); y <= subDivSize; ++y, yd += xyDelta)
      {
        float xd(-planeSize);
        for (int x(0); x <= subDivSize; ++x, xd += xyDelta, ++ind)
        {
          m_wvVertices[ind].xyz = Vec3(xd, yd, zDelta);
          m_wvVertices[ind].st[0] = 0; 
          m_wvVertices[ind].st[1] = 0;					
        }
      }
    }

    // fill in data for render object
    pROVol->m_II.m_Matrix.SetIdentity();
    pROVol->m_fSort = 0;

    // get shader item
    SShaderItem shaderItem(m_wvParams.m_viewerInsideVolume ? 
      (isLowSpec ? m_pWaterBodyOutofMatLowSpec->GetShaderItem(0) : m_pWaterBodyOutofMat->GetShaderItem(0)) : 
      (isLowSpec ? m_pWaterBodyIntoMatLowSpec->GetShaderItem(0) : m_pWaterBodyIntoMat->GetShaderItem(0)));

    // add to renderer
    pRenderer->EF_AddEf(m_pWVRE, shaderItem, pROVol, EFSLIST_WATER_VOLUMES, distCamToFogPlane < -0.1f);
  }
}

bool COcean::IsWaterVisible()
{

  if( abs(m_nLastVisibleFrameId - GetFrameID()) <= 2 )
    m_fLastVisibleFrameTime = 0.0f;

  ITimer* pTimer(gEnv->pTimer);
  m_fLastVisibleFrameTime += gEnv->pTimer->GetFrameTime();

  if( m_fLastVisibleFrameTime > 2.0f ) // at least 2 seconds
    return (abs(m_nLastVisibleFrameId - GetFrameID())<64); // and at least 64 frames

  return true; // keep water visible for a couple frames - or at least 1 second - minimizes popping during fast camera movement
}

void COcean::SetTimer(ITimer *pTimer)
{
  assert(pTimer);
  m_pTimer = pTimer;
}

float COcean::GetWave( const Vec3 &pPos )
{
  // todo: optimize...

  IRenderer* pRenderer( GetRenderer() );
  if( !pRenderer  )
    return 0.0f;

  EShaderQuality nShaderQuality = pRenderer->EF_GetShaderQuality( eST_Water );

  if( !m_pTimer || nShaderQuality == eSQ_Low )
    return 0.0f;

  // Return height - matching computation on GPU

  C3DEngine* p3DEngine( Get3DEngine() );

  bool bOceanFFT = false;
  if( ( pRenderer->GetFeatures() & (RFT_HW_VERTEXTEXTURES) && GetCVars()->e_water_ocean_fft) && nShaderQuality >= eSQ_High)
    bOceanFFT = true;

  if( bOceanFFT) 
  {
    Vec4 pDispPos = Vec4(0,0,0, 0);

    if( m_pOceanRE ) 
    {
      // Get height from FFT grid

      Vec4 *pGridFFT = m_pOceanRE->GetDisplaceGrid();
      if( !pGridFFT )
        return 0.0f; 

      // match scales used in shader
      float fScaleX = pPos.x* 0.0125f * p3DEngine->m_oceanWavesAmount* 1.25f;
      float fScaleY = pPos.y* 0.0125f * p3DEngine->m_oceanWavesAmount* 1.25f;

      float fu = fScaleX * 64.0f;
      float fv = fScaleY * 64.0f;
      int u1 = ((int)fu) & 63;
      int v1 = ((int)fv) & 63;
      int u2 = (u1 + 1) & 63;
      int v2 = (v1 + 1) & 63;

      // Fractional parts
      float fracu = fu - floorf( fu );
      float fracv = fv - floorf( fv );

      // Get weights
      float w1 = (1 - fracu) * (1 - fracv); 
      float w2 = fracu * (1 - fracv);
      float w3 = (1 - fracu) * fracv;
      float w4 = fracu *  fracv;

      Vec4 h1 = pGridFFT[u1 + v1 * 64];
      Vec4 h2 = pGridFFT[u2 + v1 * 64];
      Vec4 h3 = pGridFFT[u1 + v2 * 64];
      Vec4 h4 = pGridFFT[u2 + v2 * 64];

      // scale and sum the four heights
      pDispPos  = h1 * w1 + h2 * w2 + h3 * w3 + h4 * w4;
    }

    // match scales used in shader
    return pDispPos.z * 0.06f * p3DEngine->m_oceanWavesSize;
  }

  // constant to scale down values a bit
  const float fAnimAmplitudeScale = 1.0f / 5.0f;

  static int nFrameID = 0; 
  static Vec3 vFlowDir = Vec3(0, 0, 0);  
  static Vec4 vFrequencies = Vec4(0, 0, 0, 0);
  static Vec4 vPhases = Vec4(0, 0, 0, 0);
  static Vec4 vAmplitudes = Vec4(0, 0, 0, 0);

  // Update per-frame data
  if( nFrameID != GetFrameID() )
  {
    cry_sincosf(p3DEngine->m_oceanWindDirection, &vFlowDir.y, &vFlowDir.x);
    vFrequencies = Vec4( 0.233f, 0.455f, 0.6135f, -0.1467f ) * p3DEngine->m_oceanWavesSpeed * 5.0f; 
    vPhases = Vec4(0.1f, 0.159f, 0.557f, 0.2199f) * p3DEngine->m_oceanWavesAmount;
    vAmplitudes = Vec4(1.0f, 0.5f, 0.25f, 0.5f) * p3DEngine->m_oceanWavesSize;

    nFrameID = GetFrameID();
  }

  float fPhase = cry_sqrtf(pPos.x * pPos.x + pPos.y * pPos.y);
  Vec4 vCosPhase = vPhases * (fPhase + pPos.x);

  Vec4 vWaveFreq = vFrequencies * m_pTimer->GetCurrTime();

  Vec4 vCosWave = Vec4( cry_cosf( vWaveFreq.x * vFlowDir.x + vCosPhase.x ),
    cry_cosf( vWaveFreq.y * vFlowDir.x + vCosPhase.y ),
    cry_cosf( vWaveFreq.z * vFlowDir.x + vCosPhase.z ),
    cry_cosf( vWaveFreq.w * vFlowDir.x + vCosPhase.w ) );


  Vec4 vSinPhase = vPhases * (fPhase + pPos.y);
  Vec4 vSinWave = Vec4( cry_sinf( vWaveFreq.x * vFlowDir.y + vSinPhase.x ),
    cry_sinf( vWaveFreq.y * vFlowDir.y + vSinPhase.y ),
    cry_sinf( vWaveFreq.z * vFlowDir.y + vSinPhase.z ),
    cry_sinf( vWaveFreq.w * vFlowDir.y + vSinPhase.w ) );

  return  ( vCosWave.Dot( vAmplitudes ) + vSinWave.Dot( vAmplitudes ) ) * fAnimAmplitudeScale; 
}

uint32 COcean::GetVisiblePixelsCount()
{
  return m_nVisiblePixelsCount;
}
