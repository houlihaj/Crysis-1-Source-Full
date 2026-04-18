/*=============================================================================
  ShadowUtils.cpp : 
  Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Tiago Sousa
=============================================================================*/
#include "StdAfx.h"
#include <complex>

const int g_nGridSize = 64;
const int g_nGridLogSize = 6;

// todo. refactor me - should be more general
class CWaterSim
{
  typedef std::complex<float> complexF;

public: 

  CWaterSim():m_pFourierAmps(m_nGridSize*m_nGridSize), 
    m_pHeightField(m_nGridSize*m_nGridSize),
    m_pDisplaceFieldX(m_nGridSize*m_nGridSize),
    m_pDisplaceFieldY(m_nGridSize*m_nGridSize),
    m_pDisplaceGrid(m_nGridSize*m_nGridSize),
    m_fA(1.0f), m_fWind(0.0f), m_fWindScale(1.0f), m_fWorldSizeX(1.0f), m_fWorldSizeY(1.0f),
    m_fMaxWaveSize( 200.0f ), m_fChoppyWaveScale( 400.0f ), m_nFrameID(0)
  {
  }

  ~CWaterSim()
  {
    Release();
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void Create(float fA, float fWind, float fWindScale, float fWorldSizeX, float fWorldSizeY)   // Create/Initialize water simulation  
  {
    m_fA=fA;
    m_fWind=fWind;
    m_fWindScale = fWindScale;
    m_fWorldSizeX=fWorldSizeX;
    m_fWorldSizeY=fWorldSizeY;
    InitFourierAmps();
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void Release()
  {

  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void SaveToDisk(const char *pszFileName)
  {
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  float frand()
  {
    return ((float) rand())/((float) RAND_MAX);
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  float sfrand()
  {
    return (rand() * 2.0f / (float) RAND_MAX) - 1.0f;
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  // Returns (-1)^n
  int powNeg1(int n)
  {
    static int pow[2] = { 1, -1 };
    return pow[n&1];
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  // gaussian random number, using box muller technique
  float frand_gaussian(float m=0.0f, float s=1.0f)  // m- mean, s - standard deviation
  {
    float x1, x2, w, y1;
    static float y2;
    static int use_last(0);

    if(use_last)  // use value from previous call
    {
      y1=y2;
      use_last=0;
    }
    else
    {
      do 
      {
        x1=2.0f*frand() - 1.0f;
        x2=2.0f*frand() - 1.0f;

        w= x1*x1 + x2*x2;
      }
      while(w>=1.0f);

      w=cry_sqrtf((-2.0f*logf(w)) / w);
      y1= x1*w;
      y2= x2*w;

      use_last=1;
    }

    return (m + y1*s);
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  int GetGridOffset(const int &x, const int &y) const
  {
    return (y*m_nGridSize+x);
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  int GetMirrowedGridOffset(const int &x, const int &y) const
  {
    return GetGridOffset((m_nGridSize-x)&(m_nGridSize-1), (m_nGridSize-y)&(m_nGridSize-1));
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  int GetGridOffsetWrapped(const int &x, const int &y) const
  {
    return GetGridOffset(x&(m_nGridSize-1), y&(m_nGridSize-1));
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  void ComputeFFT1D(int nDir, float *pReal, float *pImag)
  {
    // reference: "2 Dimensional FFT" - by Paul Bourke
    long nn, i, i1, j, k, i2, l, l1, l2;
    float c1, c2, fReal, fImag, t1, t2, u1, u2, z;

    nn= m_nGridSize;
    float fRecipNN = 1.0f / (float) nn;

    // Do the bit reversal
    i2= nn >> 1;
    j= 0;
    for(i= 0; i<nn - 1; ++i)
    {
      if(i<j)
      {
        fReal= pReal[i];
        fImag= pImag[i];
        pReal[i]= pReal[j];
        pImag[i]= pImag[j];
        pReal[j]= fReal;
        pImag[j]= fImag;
      }

      k= i2;
      while(k<=j)
      {
        j-= k;
        k>>= 1;
      }

      j+= k;
    }

    // FFT computation
    c1= -1.0f;
    c2= 0.0f;
    l2= 1;
    for(l= 0; l<m_nLogGridSize; ++l)
    {
      l1= l2;
      l2<<= 1;
      u1= 1.0;
      u2= 0.0;
      for(j= 0; j<l1; ++j)
      {
        for(i= j; i<nn; i+= l2)
        {
          i1= i + l1;
          t1= u1 * pReal[i1] - u2 * pImag[i1];
          t2= u1 * pImag[i1] + u2 * pReal[i1];
          pReal[i1]= pReal[i] - t1;
          pImag[i1]= pImag[i] - t2;
          pReal[i]+= t1;
          pImag[i]+= t2;
        }

        z=  u1 * c1 - u2 * c2;
        u2= u1 * c2 + u2 * c1;
        u1= z;
      }

      c2= cry_sqrtf((1.0f - c1) * 0.5f );

      if(nDir==1)
      {
        c2= -c2;
      }

      c1= cry_sqrtf(( 1.0f + c1 ) * 0.5f );
    }

    // Scaling for forward transform
    if(nDir==1)
    {
      for(i = 0; i < nn; ++i)
      {
        pReal[i] *= fRecipNN;
        pImag[i] *= fRecipNN;
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  void ComputeFFT2D(int nDir, std::vector<complexF> &c)
  {
    // reference: "2 Dimensional FFT" - by Paul Bourke
    static float pReal[m_nGridSize];
    static float pImag[m_nGridSize];

    // Transform the rows
    for(int y(0); y<m_nGridSize; ++y)
    {
      for(int x(0); x<m_nGridSize; ++x)
      {
        pReal[x]= c[GetGridOffset(x, y)].real();
        pImag[x]= c[GetGridOffset(x, y)].imag();
      }

      ComputeFFT1D(nDir, pReal, pImag );

      for(int x(0); x<m_nGridSize; ++x)
      {
        c[GetGridOffset(x, y)] = complexF(pReal[x], pImag[x] );
        //c[GetGridOffset(x, y)].[1] = pImag[x];
      }
    }

    // Transform the columns
    for(int x(0); x<m_nGridSize; ++x)
    {
      for(int y(0); y<m_nGridSize; ++y)
      {
        pReal[y]= c[GetGridOffset(x, y)].real();
        pImag[y]= c[GetGridOffset(x, y)].imag();
      }

      ComputeFFT1D(nDir, pReal, pImag);

      for(int y(0); y<m_nGridSize; ++y)
      {
        c[GetGridOffset(x, y)] = complexF(pReal[y], pImag[y]);
        //c[GetGridOffset(x, y)].[1] = pImag[y];
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  float GetIndexToWorldX(int x)
  {
    static const float PI2ByWorldSizeX=(2.0f*PI)/m_fWorldSizeX;
    return (float(x)-((float)m_nGridSize/2.0f))*PI2ByWorldSizeX;
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  float GetIndexToWorldY(int y)
  {
    static const float PI2ByWorldSizeY=(2.0f*PI)/m_fWorldSizeY;
    return (float(y)-((float)m_nGridSize/2.0f))*PI2ByWorldSizeY;
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  float GetTermAngularFreq(float k)
  {
    // reference: "Simulating Ocean Water" - by Jerry Tessendorf (3.2)
    return cry_sqrtf(k*m_fG);
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  // a - constant, pK - wave dir (2d), pW - wind dir (2d)
  float ComputePhillipsSpec(const Vec2 &pK, const Vec2 &pW) const
  {
    float fK2= pK.GetLength2();    
    if(fK2==0)
    {
      return 0.0f;
    }

    float fW2= pW.GetLength2();
    float fL= fW2/m_fG;
    float fL2= sqr(fL);

    // reference: "Simulating Ocean Water" - by Jerry Tessendorf (3.3)
    float fPhillips=m_fA* (expf(-1.0f/(fK2*fL2))/sqr(fK2)) * (sqr(pK.Dot(pW))/fK2*fW2);

    assert(fPhillips>=0);

    // Return Phillips spectrum
    return fPhillips;
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  // Initialize Fourier amplitudes table
  void InitFourierAmps()
  {
    static const float recipSqrt2= 1.0f/cry_sqrtf(2.0f);

    Vec2 pW(cosf(m_fWind), sinf(m_fWind));
    pW = -pW; // meh - match with regular water animation
    //pW *= m_fWindScale;

    for(int y(0); y<m_nGridSize; ++y)
    {
      float fKy=GetIndexToWorldY(y);
      for(int x(0); x<m_nGridSize; ++x)
      {
        complexF e(frand_gaussian(), frand_gaussian());
        float fKx=GetIndexToWorldX(x);
        Vec2 pK(fKx, fKy);

        // reference: "Simulating Ocean Water" - by Jerry Tessendorf (3.4)
        e*= recipSqrt2*cry_sqrtf(ComputePhillipsSpec(pK, pW));
        m_pFourierAmps[GetGridOffset(x, y)]=e;
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  // Update simulation
  void Update(float fTime, bool bOnlyHeight)
  {
    if( m_nFrameID == gRenDev->GetFrameID() )
      return;

    m_nFrameID = gRenDev->GetFrameID();

    static int nUpdateFFT = 0;

//    if( nUpdateFFT == 0)
    {
      // Optimization: only half grid is required to update, since we can use conjugate to the other half
      float fHalfY=(m_nGridSize>>1)+1;
      for(int y(0); y<fHalfY; ++y)
      {
        float fKy = GetIndexToWorldY(y);
        for(int x(0); x<m_nGridSize; ++x)
        {
          float fKx = GetIndexToWorldX(x);
          Vec2 pK(fKx, fKy);

          int offset = GetGridOffset(x, y);
          int offsetMirrowed = GetMirrowedGridOffset(x, y);

          float fKLen = pK.GetLength();
          float fAngularFreq = GetTermAngularFreq(fKLen)*fTime;

          float fAngularFreqSin = 0, fAngularFreqCos = 0;
          sincos_tpl( (f32)fAngularFreq, (f32*) &fAngularFreqSin, (f32*)&fAngularFreqCos);

          complexF ep( fAngularFreqCos, fAngularFreqSin );
          complexF em = conj(ep);

          // reference: "Simulating Ocean Water" - by Jerry Tessendorf (3.4) 
          complexF currWaveField= m_pFourierAmps[offset]*ep + conj(m_pFourierAmps[offsetMirrowed])*em;

          m_pHeightField[offset]= currWaveField;
          if( !bOnlyHeight && fKLen)
          {
  //            m_pDisplaceFieldX[offset] = currWaveField * complexF(0, (-pK.x - pK.y)/fKLen );
            m_pDisplaceFieldX[offset] = currWaveField * ( complexF(0, -pK.x/fKLen) );

              m_pDisplaceFieldY[offset]= currWaveField*( (fKLen==0)?0:complexF(0, -pK.y/fKLen) );
          }
          else
          {
            m_pDisplaceFieldX[offset] = 0;
            m_pDisplaceFieldY[offset] = 0;
          }

          // Set upper half using conjugate
          if (y != fHalfY-1)
          {
            m_pHeightField[offsetMirrowed]= conj(m_pHeightField[offset]);
            if( !bOnlyHeight )
            {
              m_pDisplaceFieldX[offsetMirrowed]= conj(m_pDisplaceFieldX[offset]);                  
              m_pDisplaceFieldY[offsetMirrowed]= conj(m_pDisplaceFieldY[offset]);
            }
          }
        }
      }
      
      ComputeFFT2D(-1, m_pHeightField);
      if( !bOnlyHeight )
      {
        ComputeFFT2D(-1, m_pDisplaceFieldX);
        ComputeFFT2D(-1, m_pDisplaceFieldY);
      }
      
      for(int y(0); y<m_nGridSize; ++y)
      {
        for(int x(0); x<m_nGridSize; ++x)
        {
          int offset=GetGridOffset(x, y);
          int currPowNeg1=powNeg1(x+y);

          m_pHeightField[offset]*= currPowNeg1*m_fMaxWaveSize;
          if( !bOnlyHeight )
          {
              m_pDisplaceFieldX[offset]*= currPowNeg1*m_fChoppyWaveScale;
              m_pDisplaceFieldY[offset]*= currPowNeg1*m_fChoppyWaveScale;
          }

          m_pDisplaceGrid[offset] = Vec4(m_pDisplaceFieldX[offset].real(), 
                                         m_pDisplaceFieldY[offset].real(), 
                                         //m_pDisplaceFieldX[offset].real(), //imag(),
                                         -m_pHeightField[offset].real(), 
                                         0.0f); // store normal ?
        }
      }
    }

    //nUpdateFFT++;
    //if( nUpdateFFT > 1 )
    //  nUpdateFFT = 0;
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  Vec3 GetPositionAt(int x, int y) const
  {
    Vec4 pPos = m_pDisplaceGrid[ GetGridOffsetWrapped(x, y) ];
    return Vec3(pPos.x, pPos.y, pPos.z);
      
      //Vec3(m_pDisplaceFieldX[offset].real(),
        //        m_pDisplaceFieldY[offset].real(),
          //     -m_pHeightField[offset].real());
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  float GetHeightAt(int x, int y) const
  {
    return m_pDisplaceGrid[ GetGridOffsetWrapped(x, y) ].z; ////;-m_pHeightField[GetGridOffsetWrapped(x, y)].real();
  }

  Vec4 *GetDisplaceGrid() 
  {
    if( !m_pDisplaceGrid.empty() )
      return &m_pDisplaceGrid[0];

    return 0;
  }

  static CWaterSim *GetInstance()
  {
    static CWaterSim pInstance;
    return &pInstance;
  }

protected:

  // Simulation constants
  static const int m_nGridSize= g_nGridSize;
  static const int m_nLogGridSize= g_nGridLogSize; // log2(64)
  static const float m_fG;

  std::vector<complexF> m_pFourierAmps; // Fourier amplitudes at time 0 (aka. H0)

  std::vector<complexF> m_pHeightField; // Current Fourier amplitudes height field
  std::vector<complexF> m_pDisplaceFieldX; // Current Fourier amplitudes displace field 
  std::vector<complexF> m_pDisplaceFieldY; // Current Fourier amplitudes displace field 

  std::vector<Vec4> m_pDisplaceGrid; // Current displace grid

  float m_fA; // constant value
  float m_fWind; // window direction angle
  float m_fWindScale; // wind scale
  float m_fWorldSizeX; // world dimensions
  float m_fWorldSizeY; // world dimensions

  float m_fMaxWaveSize;
  float m_fChoppyWaveScale;

  int m_nFrameID;
};

const float CWaterSim::m_fG(9.81f);

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWater::Create(float fA, float fWind, float fWindScale, float fWorldSizeX, float fWorldSizeY )
{
  Release();
  m_pWaterSim = new CWaterSim; //::GetInstance();
  m_pWaterSim->Create(fA, fWind, fWindScale, fWorldSizeX, fWorldSizeY);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWater::Release()
{
  SAFE_DELETE(m_pWaterSim);
  //m_pWaterSim = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWater::SaveToDisk(const char *pszFileName)
{
  assert( m_pWaterSim );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWater::Update(float fTime, bool bOnlyHeight)
{
  assert( m_pWaterSim );
  m_pWaterSim->Update( fTime, bOnlyHeight );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

Vec3 CWater::GetPositionAt(int x, int y) const
{
  assert( m_pWaterSim );
  return m_pWaterSim->GetPositionAt(x, y);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

float CWater::GetHeightAt(int x, int y) const
{
  assert( m_pWaterSim );
  return m_pWaterSim->GetHeightAt(x, y);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

Vec4 *CWater::GetDisplaceGrid() const
{
  assert( m_pWaterSim );
  return m_pWaterSim->GetDisplaceGrid();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

const int CWater::GetGridSize() const
{
  return g_nGridSize;
}

