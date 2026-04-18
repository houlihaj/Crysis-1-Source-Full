#ifndef _DynamicVB_H_
#define _DynamicVB_H_

template <class VertexType> class DynamicVB 
{
public:
  D3DVertexBuffer *m_pVB;

private :
    uint m_BytesCount;
    uint m_nBytesOffs;
    bool m_bLocked;
    VertexType* m_pLockedData;

  public :

    uint GetBytesCount() const 
    { 
      return m_BytesCount; 
    }
    int GetBytesOffset() const 
    { 
      return m_nBytesOffs; 
    }
    void Reset()
    {
      m_nBytesOffs = m_BytesCount;
    }

#if defined (DIRECT3D9) || defined(OPENGL)
    DynamicVB(LPDIRECT3DDEVICE9 pD3D, const DWORD& theFVF, const unsigned int& theVertsCount )
#elif defined (DIRECT3D10)
    DynamicVB(ID3D10Device *pD3D, const DWORD& theFVF, const unsigned int& theVertsCount )
#endif
    {
      m_pVB = 0;

      m_bLocked = false;
      m_pLockedData = NULL;
      m_nBytesOffs = 0;

      m_BytesCount = theVertsCount * sizeof(VertexType);
    
#if defined (DIRECT3D9) || defined(OPENGL)
      HRESULT hr = pD3D->CreateVertexBuffer(m_BytesCount, D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, theFVF, D3DPOOL_DEFAULT, &m_pVB, NULL);
#elif defined (DIRECT3D10)
      D3D10_BUFFER_DESC BufDesc;
      ZeroStruct(BufDesc);
      BufDesc.ByteWidth = m_BytesCount;
      BufDesc.Usage = D3D10_USAGE_DYNAMIC;
      BufDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
      BufDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
      BufDesc.MiscFlags = 0;
      HRESULT hr = pD3D->CreateBuffer(&BufDesc, NULL, &m_pVB);
#endif
      assert((hr == D3D_OK) && (m_pVB));
    }

    VertexType* Lock(const unsigned int& theLockBytesCount, uint &nOffs)
    {
      if (theLockBytesCount > m_BytesCount)
      {
        assert(false);
        return NULL;
      }

      if (m_bLocked)
        return m_pLockedData;

      HRESULT hr = S_OK;
#if defined (XENON)
      if (m_pVB == gcpRendD3D->m_RP.m_VertexStreams[0].pStream)
      {
        hr = gcpRendD3D->m_pd3dDevice->SetStreamSource(0, NULL, 0, 0);
        gcpRendD3D->m_RP.m_VertexStreams[0].pStream = NULL;
      }
#endif
      if (m_pVB)
      {
#if defined (DIRECT3D9) || defined(OPENGL)
        if (theLockBytesCount+m_nBytesOffs > m_BytesCount)
        {
          gcpRendD3D->m_RP.m_VertexStreams[3].pStream = NULL;
#if !defined(XENON) && !defined(PS3)
          hr = m_pVB->Lock(0, theLockBytesCount, (void **) &m_pLockedData, D3DLOCK_DISCARD);
#else
          hr = m_pVB->Lock(0, 0, (void **) &m_pLockedData, D3DLOCK_DISCARD);
#endif
          nOffs = 0;
          m_nBytesOffs = theLockBytesCount;
        }
        else
        {
#if !defined(XENON) && !defined(PS3)
          hr = m_pVB->Lock(m_nBytesOffs, theLockBytesCount, (void **) &m_pLockedData, D3DLOCK_NOOVERWRITE);
#else
          byte *pD;
          hr = m_pVB->Lock(0, 0, (void **) &pD, D3DLOCK_NOOVERWRITE);
          m_pLockedData = (VertexType *)(pD + m_nBytesOffs);
#endif
          nOffs = m_nBytesOffs;
          m_nBytesOffs += theLockBytesCount;
        }
#elif defined (DIRECT3D10)
        if (theLockBytesCount+m_nBytesOffs > m_BytesCount)
        {
          hr = m_pVB->Map(D3D10_MAP_WRITE_DISCARD, 0, (void **) &m_pLockedData);
          nOffs = 0;
          m_nBytesOffs = theLockBytesCount;
        }
        else
        {
          byte *pD;
          hr = m_pVB->Map(D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void **) &pD);
          m_pLockedData = (VertexType *)(pD + m_nBytesOffs);
          nOffs = m_nBytesOffs;
          m_nBytesOffs += theLockBytesCount;
        }
#endif
        assert(m_pLockedData != NULL);
        m_bLocked = true;
      }

      return m_pLockedData;
    }

    void Unlock()
    {
      if ((m_bLocked) && (m_pVB))
      {
#if defined (DIRECT3D9) || defined(OPENGL)
        HRESULT hr = m_pVB->Unlock();        
#elif defined (DIRECT3D10)
        HRESULT hr = S_OK;
        m_pVB->Unmap();        
#endif
        assert(hr == D3D_OK);
        m_bLocked = false;
      }
    }

    HRESULT Bind(uint StreamNumber, int nBytesOffset, int Stride, uint nFreq=1)
    {
      HRESULT h = gcpRendD3D->FX_SetVStream(StreamNumber, m_pVB, nBytesOffset, Stride, nFreq);
      return h;
    }
    ~DynamicVB()
    {
      Unlock();
      SAFE_RELEASE(m_pVB);
    }
  
};

#endif  _DynamicVB_H_
