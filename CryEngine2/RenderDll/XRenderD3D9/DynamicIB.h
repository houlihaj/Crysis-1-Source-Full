#ifndef _DynamicIB_H_
#define _DynamicIB_H_

template <class Type> class DynamicIB 
{
  private :

#if defined (DIRECT3D9) || defined(OPENGL)
    LPDIRECT3DINDEXBUFFER9 m_pIB;
#elif defined (DIRECT3D10)
    ID3D10Buffer *m_pIB;
#endif
    uint m_Count;
    int m_nOffs;
    bool m_bLocked;
    Type* m_pLockedData;

  public :

    uint GetCount() const 
    { 
      return m_Count; 
    }
    int GetOffset() const 
    { 
      return m_nOffs; 
    }
    void Reset()
    {
      m_nOffs = 0;
    }

#if defined (DIRECT3D9) || defined(OPENGL)
    DynamicIB(LPDIRECT3DDEVICE9 pD3D, const unsigned int& theElementsCount)
#elif defined (DIRECT3D10)
    DynamicIB(ID3D10Device *pD3D, const unsigned int& theElementsCount)
#endif
    {
      m_pIB = 0;

      m_bLocked = false;
      m_pLockedData = NULL;
      m_nOffs = 0;

      m_Count = theElementsCount;
    
#if defined (DIRECT3D9) || defined(OPENGL)
      HRESULT hr = pD3D->CreateIndexBuffer(theElementsCount*sizeof(Type), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, sizeof(Type)==2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &m_pIB, NULL);
      assert((hr == D3D_OK) && (m_pIB));
#elif defined (DIRECT3D10)
      D3D10_BUFFER_DESC BufDesc;
      ZeroStruct(BufDesc);
      BufDesc.ByteWidth = theElementsCount*sizeof(Type);
      BufDesc.Usage = D3D10_USAGE_DYNAMIC;
      BufDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
      BufDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
      BufDesc.MiscFlags = 0;
      HRESULT hr = pD3D->CreateBuffer(&BufDesc, NULL, &m_pIB);
#endif
    }

    Type* Lock(const uint& theLockCount, uint &nOffs)
    {
      if (theLockCount > m_Count)
      {
        assert(false);
        return NULL;
      }

      if (m_bLocked)
        return m_pLockedData;

      HRESULT hr = S_OK;
#if defined (XENON)
      if (m_pIB == gcpRendD3D->m_RP.m_pIndexStream)
      {
        hr = gcpRendD3D->m_pd3dDevice->SetIndices(NULL);
        gcpRendD3D->m_RP.m_pIndexStream = NULL;
      }
#endif
      if ( m_pIB )
      {
#if defined (DIRECT3D9) || defined(OPENGL)
        if (theLockCount+m_nOffs > m_Count)
        {
#if !defined(XENON) && !defined(PS3)
          hr = m_pIB->Lock(0, theLockCount*sizeof(Type), (void **) &m_pLockedData, D3DLOCK_DISCARD);
#else
          hr = m_pIB->Lock(0, 0, (void **) &m_pLockedData, D3DLOCK_DISCARD);
#endif
          nOffs = 0;
          m_nOffs = theLockCount;
        }
        else
        {
#if !defined(XENON) && !defined(PS3)
          hr = m_pIB->Lock(m_nOffs*sizeof(Type), theLockCount*sizeof(Type), (void **) &m_pLockedData, D3DLOCK_NOOVERWRITE);
#else
          byte *pD;
          hr = m_pIB->Lock(0, 0, (void **) &pD, D3DLOCK_NOOVERWRITE);
          m_pLockedData = (Type *)(pD + m_nOffs*sizeof(Type));
#endif
          nOffs = m_nOffs;
          m_nOffs += theLockCount;
        }
#elif defined (DIRECT3D10)
        if (theLockCount+m_nOffs > m_Count)
        {
          hr = m_pIB->Map(D3D10_MAP_WRITE_DISCARD, 0, (void **) &m_pLockedData);
          nOffs = 0;
          m_nOffs = theLockCount;
        }
        else
        {
          byte *pD;
          hr = m_pIB->Map(D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void **) &pD);
          m_pLockedData = (Type *)(pD + m_nOffs*sizeof(Type));
          nOffs = m_nOffs;
          m_nOffs += theLockCount;
        }
#endif
        assert(m_pLockedData != NULL);
        m_bLocked = true;
      }

      return m_pLockedData;
    }

    void Unlock()
    {
      if ((m_bLocked) && (m_pIB))
      {
        HRESULT hr = S_OK;
#if defined (DIRECT3D9) || defined(OPENGL)
        hr = m_pIB->Unlock();        
#elif defined (DIRECT3D10)
        m_pIB->Unmap();        
#endif
        assert(hr == D3D_OK);
        m_bLocked = false;
      }
    }
#if defined (DIRECT3D9) || defined(OPENGL)
    LPDIRECT3DVERTEXBUFFER9 GetInterface() { return m_pIB; }
#elif defined (DIRECT3D10)
    ID3D10Buffer* GetInterface() { return m_pIB; }
#endif

    HRESULT Bind()
    {
      return gcpRendD3D->FX_SetIStream(m_pIB);
    }

    ~DynamicIB()
    {
      Unlock();
      SAFE_RELEASE(m_pIB);
    }
  
};

#endif  _DynamicIB_H_
