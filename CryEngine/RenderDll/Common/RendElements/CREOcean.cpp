#include "StdAfx.h"
#include "RendElement.h"
#include "CREOcean.h"
#include "I3DEngine.h"

#if !defined(XENON) && !defined(PS3)

SREOceanStats CREOcean::m_RS;
CREOcean *CREOcean::m_pStaticOcean = NULL;

DEFINE_ALIGNED_DATA( float, CREOcean::m_HX[OCEANGRID][OCEANGRID], 16 ); 
DEFINE_ALIGNED_DATA( float, CREOcean::m_HY[OCEANGRID][OCEANGRID], 16 ); 

DEFINE_ALIGNED_DATA( float, CREOcean::m_NX[OCEANGRID][OCEANGRID], 16 ); 
DEFINE_ALIGNED_DATA( float, CREOcean::m_NY[OCEANGRID][OCEANGRID], 16 ); 

DEFINE_ALIGNED_DATA( float, CREOcean::m_DX[OCEANGRID][OCEANGRID], 16 ); 
DEFINE_ALIGNED_DATA( float, CREOcean::m_DY[OCEANGRID][OCEANGRID], 16 ); 

CREOcean::~CREOcean()
{
  m_pStaticOcean = NULL;
  if (m_pBuffer)
  {
    gRenDev->ReleaseBuffer(m_pBuffer, m_nNumVertsInPool);
    m_pBuffer = NULL;
  }
  SAFE_DELETE_ARRAY(m_HMap);
  for (int i=0; i<NUM_LODS; i++)
  {
    m_pIndices[i].Free();
  }
}

float CREOcean::GetWaterZElevation(float fX, float fY)
{
  if (!m_HMap)
    return 0;
  CRenderer *r = gRenDev;
  I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
  float fSize = (float)CRenderer::CV_r_oceansectorsize;
  float fHeightScale = (float)CRenderer::CV_r_oceanheightscale;
  float fWaterLevel = eng->GetWaterLevel();
  float fHScale = fabsf(gRenDev->GetRCamera().Orig.z-fWaterLevel);
  float fMaxDist = eng->GetMaxViewDistance() / 1.0f;
  float fGrid = (float)OCEANGRID;

  float fZH = GetHMap(fX, fY);
  if (fZH >= fWaterLevel)
    return fWaterLevel;

  float fXPart = fX / fSize;
  float fYPart = fY / fSize;
  float fXLerp = (fXPart - (int)fXPart) * fGrid;
  float fYLerp = (fYPart - (int)fYPart) * fGrid;
  int nXMin = (int)fXLerp;
  int nXMax = nXMin+1;
  int nYMin = (int)fYLerp;
  int nYMax = nYMin+1;
  fXLerp = fXLerp - (int)fXLerp;
  fYLerp = fYLerp - (int)fYLerp;

  if (fHScale < 5.0f)
    fHScale = LERP(0.1f, 0.5f, fHScale/5.0f);
  else
  if (fHScale < 20)
    fHScale = LERP(0.5f, 1.0f, (fHScale-5.0f)/15.0f);
  else
    fHScale = 1.0f;

  float fZ00 = -m_HX[nYMin][nXMin] * fHScale;
  float fZ01 = -m_HX[nYMin][nXMax] * fHScale;
  float fZ10 = -m_HX[nYMax][nXMin] * fHScale;
  float fZ11 = -m_HX[nYMax][nXMax] * fHScale;
  float fZ0 = LERP(fZ00, fZ01, fXLerp);
  float fZ1 = LERP(fZ10, fZ11, fXLerp);
  float fZ = LERP(fZ0, fZ1, fYLerp);
  //fZ *= fHeightScale;

  float fHeightDelta = fWaterLevel - fZH;
  fHeightScale *= CLAMP(fHeightDelta * 0.066f, 0, 1);

  return fZ * fHeightScale + fWaterLevel;
}

void CREOcean::PrepareHMap()
{
  int nDim=0;

  I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
  m_nHMUnitSize = eng->GetHeightMapUnitSize();
  int nExp = 8;
  m_fExpandHMap = (float)nExp;
  m_fExpandHM = (float)m_nHMUnitSize * (float)nExp;
  m_fInvHMUnitSize = 1.0f / (float)m_nHMUnitSize;
  m_fTerrainSize = (float)eng->GetTerrainSize();

	assert(!"UnderWaterSmoothHMap removed from 3dengine die to refactoring and clean up");
	//  eng->MakeUnderWaterSmoothHMap(m_nHMUnitSize);
  ushort *shm = 0;//eng->GetUnderWaterSmoothHMap(nDim);
	return;

	if(!nDim)
		return; // terrain not present

  float fWaterLevel = eng->GetWaterLevel();

  if (m_HMap)
    delete [] m_HMap;
  int nDimO = nDim+nExp*2;
  m_nHMapSize = nDimO;
  m_HMap = new float [nDimO*nDimO];
  float f;
  for (int y=-nExp; y<nDim+nExp; y++)
  {
    for (int x=-nExp; x<nDim+nExp; x++)
    {
      if (x>0 && x<nDim-1 && y>0 && y<nDim-1)
        m_HMap[(y+nExp)*nDimO+x+nExp] = (float)shm[y*nDim+x] / 256.0f;
      else
      {
        int nX = x;
        int nY = y;
        float fLerpX = -1.0f;
        float fLerpY = -1.0f;
        
        if (nX<=0)
          nX = 1;
        else
        if (nX>=nDim-1)
          nX = nDim-2;

        if (nY<=0)
          nY = 1;
        else
        if (nY>=nDim-1)
          nY = nDim-2;

        f = (float)shm[nY*nDim+nX] / 256.0f;

        if (nX != x)
          fLerpX = 1.0f - fabsf((float)(nX-x)) / (float)(nExp+1);
        if (nY != y)
          fLerpY = 1.0f - fabsf((float)(nY-y)) / (float)(nExp+1);

        float fX, fY;

        if (fLerpX >= 0)
          fX = LERP(fWaterLevel/2.0f, f, fLerpX);
        if (fLerpY >= 0)
          fY = LERP(fWaterLevel/2.0f, f, fLerpY);
        if (fLerpX >= 0 && fLerpY >= 0)
          m_HMap[(y+nExp)*nDimO+x+nExp] = (fX + fY) / 2.0f;
        else
        if (fLerpX >= 0)
          m_HMap[(y+nExp)*nDimO+x+nExp] = fX;
        else
        if (fLerpY >= 0)
          m_HMap[(y+nExp)*nDimO+x+nExp] = fY;
        else
          m_HMap[(y+nExp)*nDimO+x+nExp] = f;
      }
    }
  }
}

bool sGetPublic(const CCryName& n, float *v, int nID);

void CREOcean::mfPrepare()
{
  CRenderer *rd = gRenDev;

  if (m_nFrameUpdate != rd->GetFrameID(false))
  {
    m_nFrameUpdate = rd->GetFrameID(false);
    bool bRebuild = false;
    float v[4];
    bool bFound = sGetPublic("ParamDepth", v, 0);
    if (bFound && m_fDepth != v[0])
    {
      m_fDepth = v[0];
      bRebuild = true;
    }
    bFound = sGetPublic("ParamGravity", v, 0);
    if (bFound && m_fGravity != v[0])
    {
      m_fGravity = v[0];
      bRebuild = true;
    }
    bFound = sGetPublic("ParamSpeed", v, 0);
    if (bFound && m_fSpeed != v[0])
    {
      m_fSpeed = v[0];
      bRebuild = true;
    }
    bFound = sGetPublic("ParamWindDirection", v, 0);
    if (bFound && m_fWindDirection != v[0])
    {
      m_fWindDirection = v[0];
      bRebuild = true;
    }
    bFound = sGetPublic("ParamWaveHeight", v, 0);
    if (bFound && m_fWaveHeight != v[0])
    {
      m_fWaveHeight = v[0];
      bRebuild = true;
    }
    bFound = sGetPublic("ParamWindSpeed", v, 0);
    if (bFound && m_fWindSpeed != v[0])
    {
      m_fWindSpeed = v[0];
      bRebuild = true;
    }
    bFound = sGetPublic("ParamDirectionalDependence", v, 0);
    if (bFound && m_fDirectionalDependence != v[0])
    {
      m_fDirectionalDependence = v[0];
      bRebuild = true;
    }
    bFound = sGetPublic("ParamChoppyWaveFactor", v, 0);
    if (bFound && m_fChoppyWaveFactor!= v[0])
    {
      m_fChoppyWaveFactor = v[0];
      bRebuild = true;
    }
    bFound = sGetPublic("ParamSuppressSmallWavesFactor", v, 0);
    if (bFound && m_fSuppressSmallWavesFactor!= v[0])
    {
      m_fSuppressSmallWavesFactor = v[0];
      bRebuild = true;
    }
    if (bRebuild)
      PostLoad(4357, m_fWindDirection, m_fWindSpeed, m_fWaveHeight, m_fDirectionalDependence, m_fChoppyWaveFactor, m_fSuppressSmallWavesFactor);

    if (m_nFrameLoad != rd->m_nFrameLoad)
    {
      m_nFrameLoad = rd->m_nFrameLoad;
      //PrepareHMap();
    }

    rd->EF_CheckOverflow(0, 0, this);

    Update(rd->m_RP.m_RealTime * m_fSpeed);

    float time0 = iTimer->GetAsyncCurTime();

    I3DEngine *eng = (I3DEngine *)iSystem->GetI3DEngine();
    float fWaterLevel = eng->GetWaterLevel();
    float fHScale = fabsf(gRenDev->GetRCamera().Orig.z-fWaterLevel);
    if (fHScale < 5.0f)
      fHScale = LERP(0.1f, 0.5f, fHScale/5.0f);
    else
    if (fHScale < 20)
      fHScale = LERP(0.5f, 1.0f, (fHScale-5.0f)/15.0f);
    else
      fHScale = 1.0f;
    m_MinBound.Set(9999.0f, 9999.0f, 9999.0f);
    m_MaxBound.Set(-9999.0f, -9999.0f, -9999.0f);

    if (!m_pBuffer)
      m_pBuffer = gRenDev->CreateBuffer((OCEANGRID+1)*(OCEANGRID+1), VERTEX_FORMAT_P3F, "Ocean", true);

    static const float fScale = 1.0f / (float)OCEANGRID;

    struct_VERTEX_FORMAT_P3F *pVertices = new struct_VERTEX_FORMAT_P3F[(OCEANGRID+1)*(OCEANGRID+1)];
    int n = 0;
    for(int vy=0; vy<OCEANGRID+1; vy++)
    {
      int y = vy & (OCEANGRID-1);
      for(int vx=0; vx<(OCEANGRID+1); vx++)
      {
        int x = vx & (OCEANGRID-1);
        m_Pos[vy][vx][0] = vx * fScale + m_DX[y][x] * m_fChoppyWaveFactor;
        m_Pos[vy][vx][1] = vy * fScale + m_DY[y][x] * m_fChoppyWaveFactor;
        pVertices[n].xyz.x = m_Pos[vy][vx][0];
        pVertices[n].xyz.y = m_Pos[vy][vx][1];
        float fZ = -m_HX[y][x] * fHScale;
        pVertices[n].xyz.z = fZ;
        m_MinBound.x = min(m_Pos[vy][vx][0], m_MinBound.x);
        m_MinBound.y = min(m_Pos[vy][vx][1], m_MinBound.y);
        m_MinBound.z = min(fZ, m_MinBound.z);

        m_MaxBound.x = max(m_Pos[vy][vx][0], m_MaxBound.x);
        m_MaxBound.y = max(m_Pos[vy][vx][1], m_MaxBound.y);
        m_MaxBound.z = max(fZ, m_MaxBound.z);

        m_Normals[vy][vx] = Vec3(m_NX[y][x], m_NY[y][x], 64.0f);
        m_Normals[vy][vx].NormalizeFast();

        n++;
      }       
    }
    rd->UpdateBuffer(m_pBuffer, pVertices, (OCEANGRID+1)*(OCEANGRID+1), true);
    delete [] pVertices;

    m_RS.m_StatsTimeUpdateVerts = iTimer->GetAsyncCurTime()-time0;

    UpdateTexture();
  }

  rd->m_RP.m_pRE = this;
  rd->m_RP.m_RendNumIndices = 0;
  rd->m_RP.m_RendNumVerts = (OCEANGRID+1)*(OCEANGRID+1);
  rd->m_RP.m_FirstVertex = 0;
}

int CREOcean::GetLOD(Vec3 camera, Vec3 pos)
{
  float dist = (pos-camera).GetLength();
  dist /= CRenderer::CV_r_oceanloddist;

  return (int)CLAMP(dist, 0.0f, (float)(NUM_LODS-1));
}

void *CREOcean::mfGetPointer(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags)
{

  switch(ePT) 
  {
    case eSrcPointer_Vert:
      {
        struct_VERTEX_FORMAT_P3F *pVertices = (struct_VERTEX_FORMAT_P3F *)m_pBuffer->m_VS[VSF_GENERAL].m_VData;
        *Stride = sizeof(struct_VERTEX_FORMAT_P3F);
        return &pVertices->xyz.x;
      }
  }
  return NULL;
}


void CREOcean::GenerateGeometry()
{
  m_pBuffer = gRenDev->CreateBuffer((OCEANGRID+1)*(OCEANGRID+1), VERTEX_FORMAT_P3F, "Ocean", true);

  for (int lod=0; lod<NUM_LODS; lod++)
  {
    int nl = 1<<lod;
    // set indices
    int iIndex = 0;
    int yStep = (OCEANGRID+1) * nl;
    for(int a=0; a<(OCEANGRID+1)-1; a+=nl )
    {
      for(int i=0; i<(OCEANGRID+1); i+=nl, iIndex+=nl )
      {
        m_pIndices[lod].AddElem(iIndex);
        m_pIndices[lod].AddElem(iIndex + yStep);
      }

      int iNextIndex = (a+nl) * (OCEANGRID+1);

      if(a < (OCEANGRID+1)-1-nl)
      {
        m_pIndices[lod].AddElem(iIndex + yStep - nl);
        m_pIndices[lod].AddElem(iNextIndex);
      }
      iIndex = iNextIndex;
    }
    m_pIndices[lod].Shrink();
  }
}

void CREOcean::SmoothLods_r(SOceanSector *os, float fSize, int minLod)
{
  if (os->m_Frame != gRenDev->m_cEF.m_Frame)
    return;
  if (os->nLod > minLod+1)
  {
    os->nLod = minLod+1;
    SOceanSector *osLeft = GetSectorByPos(os->x-fSize, os->y, false);
    SOceanSector *osRight = GetSectorByPos(os->x+fSize, os->y, false);
    SOceanSector *osTop = GetSectorByPos(os->x, os->y-fSize, false);
    SOceanSector *osBottom = GetSectorByPos(os->x, os->y+fSize, false);

    if (osLeft)
      SmoothLods_r(osLeft, fSize, os->nLod);
    if (osRight)
      SmoothLods_r(osRight, fSize, os->nLod);
    if (osTop)
      SmoothLods_r(osTop, fSize, os->nLod);
    if (osBottom)
      SmoothLods_r(osBottom, fSize, os->nLod);
  }
}

void CREOcean::LinkVisSectors(float fSize)
{
  for (uint i=0; i<m_VisOceanSectors.Num(); i++)
  {
    SOceanSector *os = m_VisOceanSectors[i];
    SOceanSector *osLeft = GetSectorByPos(os->x-fSize, os->y, false);
    SOceanSector *osRight = GetSectorByPos(os->x+fSize, os->y, false);
    SOceanSector *osTop = GetSectorByPos(os->x, os->y-fSize, false);
    SOceanSector *osBottom = GetSectorByPos(os->x, os->y+fSize, false);

    if (osLeft)
      SmoothLods_r(osLeft, fSize, os->nLod);
    if (osRight)
      SmoothLods_r(osRight, fSize, os->nLod);
    if (osTop)
      SmoothLods_r(osTop, fSize, os->nLod);
    if (osBottom)
      SmoothLods_r(osBottom, fSize, os->nLod);
  }
}

void CREOcean::PostLoad(unsigned long ulSeed, float fWindDirection, float fWindSpeed, float fWaveHeight, float fDirectionalDependence, float fChoppyWavesFactor, float fSuppressSmallWavesFactor)
{
  m_fWindX                    = cry_cosf( fWindDirection );    
  m_fWindY                    = cry_sinf( fWindDirection );
  m_fWindSpeed                = fWindSpeed;
  m_fWaveHeight               = fWaveHeight;
  m_fDirectionalDependence    = fDirectionalDependence;
  m_fChoppyWaveFactor         = fChoppyWavesFactor;
  m_fSuppressSmallWavesFactor = fSuppressSmallWavesFactor;

  m_fLargestPossibleWave =  m_fWindSpeed * m_fWindSpeed / m_fGravity;   
  m_fSuppressSmallWaves  = m_fLargestPossibleWave * m_fSuppressSmallWavesFactor;

  // init H0
  CRERandom kRnd( ulSeed );
  float fOneBySqrtTwo = 1.0f / cry_sqrtf(2.0f);
  int i, j;
  for(j=-OCEANGRID/2; j<=OCEANGRID/2; j++)
  {
    int y = j + OCEANGRID/2;
    for(i=-OCEANGRID/2; i<=OCEANGRID/2; i++)
    {
      int x = i + OCEANGRID/2;
      float rndX = (float)kRnd.GetGauss();
      float rndY = (float)kRnd.GetGauss();
      rndX *= fOneBySqrtTwo * cry_sqrtf(PhillipsSpectrum(2.0f*PI*i, 2.0f*PI*j));
      rndY *= fOneBySqrtTwo * cry_sqrtf(PhillipsSpectrum(2.0f*PI*i, 2.0f*PI*j));

      m_H0X[y][x] = rndX;
      m_H0Y[y][x] = rndY;
    }
  } 

  // precalc length of K
  for(j=-OCEANGRID/2; j<OCEANGRID/2; j++)
  {
    int y = j + OCEANGRID/2;
    for(i=-OCEANGRID/2; i<OCEANGRID/2; i++)
    {
      int x = i + OCEANGRID/2;
      float fKLength = cry_sqrtf(sqrf(2.0f*PI*i) + sqrf(2.0f*PI*j));
      if( fKLength < 1e-8f )
        fKLength = 1e-8f;
      m_aKScale[y][x] = 1.0f / fKLength;
    }
  }

  // init angular frequencies
  for(j=-OCEANGRID/2; j<OCEANGRID/2; j++)
  {
    int y = j + OCEANGRID/2;
    for(i=-OCEANGRID/2; i<OCEANGRID/2; i++)
    {
      int x = i + OCEANGRID/2;
      float fKLength = cry_sqrtf(sqrf(2.0f*PI*i) + sqrf(2.0f*PI*j));
      m_aAngularFreq[y][x] = cry_sqrtf(m_fGravity * fKLength) * cry_tanhf(fKLength * m_fDepth);
    }
  }
}

void CREOcean::FFT( int iDir, float* real, float* imag )
{
  long nn,i,i1,j,k,i2,l,l1,l2;
  float c1,c2,treal,timag,t1,t2,u1,u2,z;
  
  nn = OCEANGRID;
  
  // Do the bit reversal
  i2 = nn >> 1;
  j = 0;
  for( i = 0; i < nn - 1; ++i )
  {
    if( i < j )
    {
      treal = real[ i ];
      timag = imag[ i ];
      real[ i ] = real[ j ];
      imag[ i ] = imag[ j ];
      real[ j ] = treal;
      imag[ j ] = timag;
    }

    k = i2;
    while( k <= j )
    {
      j -= k;
      k >>= 1;
    }

    j += k;
  }
  
  // Compute the FFT
  c1 = -1.0f;
  c2 = 0.0f;
  l2 = 1;
  for(l=0; l<LOG_OCEANGRID; l++)
  {
    l1 = l2;
    l2 <<= 1;
    u1 = 1.0;
    u2 = 0.0;
    for( j = 0; j < l1; ++j )
    {
      for( i = j; i < nn; i += l2 )
      {
        i1 = i + l1;
        t1 = u1 * real[i1] - u2 * imag[i1];
        t2 = u1 * imag[i1] + u2 * real[i1];
        real[i1] = real[i] - t1;
        imag[i1] = imag[i] - t2;
        real[i] += t1;
        imag[i] += t2;
      }

      z =  u1 * c1 - u2 * c2;
      u2 = u1 * c2 + u2 * c1;
      u1 = z;
    }

    c2 = sqrt_fast_tpl(( 1.0f - c1 ) * 0.5f);
    
    if( 1 == iDir )
    {
      c2 = -c2;
    }

    c1 = sqrt_fast_tpl(( 1.0f + c1 ) * 0.5f);
  }
  
  // Scaling for forward transform
  if( 1 == iDir )
  {
    for(i=0; i<nn; ++i)
    {
      real[i] /= (float) nn;
      imag[i] /= (float) nn;
    }
  }
}

void FFTSSE_64(float* ar, float* ai);

void CREOcean::FFT2D(int iDir, float cmpX[OCEANGRID][OCEANGRID], float cmpY[OCEANGRID][OCEANGRID])
{
  float real[OCEANGRID];
  float imag[OCEANGRID];


#if !defined(_XBOX) && !defined(WIN64) && !defined(LINUX) && !defined(PS3)
	// NOTE: AMD64 port: implement
  if ((g_CpuFlags & CPUF_SSE) && !(((int)&cmpX[0][0]) & 0xf) && !(((int)&cmpY[0][0]) & 0xf) && OCEANGRID == 64)
  {
    FFTSSE_64(&cmpY[0][0], &cmpX[0][0]);
    return;
  }
#endif
  
  int i, j;

  // Transform the rows
  for(j=0; j<OCEANGRID; j++)
  {
    for(i=0; i<OCEANGRID; i++)
    {
      real[i] = cmpX[j][i];
      imag[i] = cmpY[j][i];
    }

    FFT( iDir, real, imag );
    
    for(i=0; i<OCEANGRID; i++)
    {
      cmpX[j][i] = real[i];
      cmpY[j][i] = imag[i];
    }
  }
  
  // Transform the columns
  for(i=0; i<OCEANGRID; i++)
  {
    for(j=0; j<OCEANGRID; j++)
    {
      real[j] = cmpX[j][i];
      imag[j] = cmpY[j][i];
    }

    FFT( iDir, real, imag );

    for(j=0; j<OCEANGRID; j++)
    {
      cmpX[j][i] = real[j];
      cmpY[j][i] = imag[j];
    }
  }
}

void CREOcean::Update( float fTime )
{
  float time0 = iTimer->GetAsyncCurTime();

  float fK = 2.0f * PI;
  for(int j=-OCEANGRID/2; j<OCEANGRID/2; j++)
  {
    int jn = j & (OCEANGRID-1);
    for(int i=-OCEANGRID/2; i<OCEANGRID/2; i++)
    {
      int x = i + OCEANGRID/2;
      int y = j + OCEANGRID/2;
      unsigned int val = (int)(m_aAngularFreq[y][x] * 1024.0f / (PI*2) * fTime);
      float fSin = gRenDev->m_RP.m_tSinTable[val&0x3ff];
      float fCos = gRenDev->m_RP.m_tSinTable[(val+512)&0x3ff];

      int x1 = -i + OCEANGRID/2;
      int y1 = -j + OCEANGRID/2;

      float hx = (m_H0X[y][x] + m_H0X[y1][x1]) * fCos -
                 (m_H0Y[y][x] + m_H0Y[y1][x1]) * fSin;

      float hy = (m_H0X[y][x] - m_H0X[y1][x1]) * fSin + 
                 (m_H0Y[y][x] - m_H0Y[y1][x1]) * fCos;

      int in = i & (OCEANGRID-1);

      // update height
      m_HX[jn][in] = hx;
      m_HY[jn][in] = hy;

      // update normal
      float fKx = fK * i;
      float fKy = fK * j;
      m_NX[jn][in] = (-hy * fKx - hx * fKy);
      m_NY[jn][in] = ( hx * fKx - hy * fKy);

      // update displacement vector for choppy waves 
      fKx *= m_aKScale[y][x];
      fKy *= m_aKScale[y][x];
      m_DX[jn][in] = ( hy * fKx + hx * fKy);
      m_DY[jn][in] = (-hx * fKx + hy * fKy);
    }
  }
  m_RS.m_StatsTimeFFTTable = iTimer->GetAsyncCurTime()-time0;

  time0 = iTimer->GetAsyncCurTime();
  FFT2D(-1, m_HX, m_HY);    
  FFT2D(-1, m_NX, m_NY);    
  FFT2D(-1, m_DX, m_DY);    
  m_RS.m_StatsTimeFFT = iTimer->GetAsyncCurTime() - time0;
}

bool CREOcean::mfCompile(CParserBin& Parser, SParserFrame& Frame)
{
  SParserFrame OldFrame = Parser.BeginFrame(Frame);

  FX_BEGIN_TOKENS
    FX_TOKEN(Speed)
    FX_TOKEN(Gravity)
    FX_TOKEN(Depth)
    FX_TOKEN(WindDirection)
    FX_TOKEN(WindSpeed)
    FX_TOKEN(WaveHeight)
    FX_TOKEN(DirectionalDependence)
    FX_TOKEN(ChoppyWaveFactor)
    FX_TOKEN(SuppressSmallWavesFactor)
  FX_END_TOKENS

  bool bRes = true;

  m_fWindDirection = 90.0f;
  m_fSpeed = 1.0f;
  m_fGravity = 9.81f;
  m_fDepth = 10;
  const char *data;

  while (Parser.ParseObject(sCommands))
  {
    EToken eT = Parser.GetToken();
    if (!Parser.m_Data.IsEmpty())
      data = Parser.GetString(Parser.m_Data);
    else
      data = NULL;
    switch (eT)
    {
      case eT_Gravity:
        if (!data || !data[0])
        {
          Warning("missing Gravity argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fGravity = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fGravity, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for Gravity", data);
        }
        break;

      case eT_Depth: 
        if (!data || !data[0])
        {
          Warning("missing Depth argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fDepth = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fDepth, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for Depth", data);
        }
        break;

      case eT_Speed: 
        if (!data || !data[0])
        {
          Warning("missing Speed argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fSpeed = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fSpeed, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for Speed", data);
        }
        break;

      case eT_WindDirection: 
        if (!data || !data[0])
        {
          Warning("missing WindDirection argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fWindDirection = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fWindDirection, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for WindDirection", data);
        }
        break;

      case eT_WindSpeed: 
        if (!data || !data[0])
        {
          Warning("missing WindSpeed argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fWindSpeed = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fWindSpeed, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for WindSpeed", data);
        }
        break;

      case eT_WaveHeight: 
        if (!data || !data[0])
        {
          Warning("missing WaveHeight argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fWaveHeight = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fWaveHeight, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for WaveHeight", data);
        }
        break;

      case eT_DirectionalDependence: 
        if (!data || !data[0])
        {
          Warning("missing DirectionalDependence argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fDirectionalDependence = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fDirectionalDependence, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for DirectionalDependence", data);
        }
        break;

      case eT_ChoppyWaveFactor: 
        if (!data || !data[0])
        {
          Warning("missing ChoppyWaveFactor argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fChoppyWaveFactor = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fChoppyWaveFactor, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for ChoppyWaveFactor", data);
        }
        break;

      case eT_SuppressSmallWavesFactor: 
        if (!data || !data[0])
        {
          Warning("missing SuppressSmallWavesFactor argument for Ocean Effect");
          break;
        }
        if (isdigit(data[0]) || data[0] == '.')
          m_fSuppressSmallWavesFactor = shGetFloat(data);
        else
        {
          bool bFound = SShaderParam::GetValue(data, &Parser.m_pCurShader->m_PublicParams, &m_fSuppressSmallWavesFactor, 0);
          if (!bFound)
            Warning("CREOcean::mfCompile: Couldn't find shader public parameter '%s' for SuppressSmallWavesFactor", data);
        }
        break;
    }
  }

  PostLoad(4357, m_fWindDirection, m_fWindSpeed, m_fWaveHeight, m_fDirectionalDependence, m_fChoppyWaveFactor, m_fSuppressSmallWavesFactor);
  m_CustomTexBind[0] = 0;

  Parser.EndFrame(OldFrame);

  return bRes;
}

#endif
