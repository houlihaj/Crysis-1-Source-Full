#ifndef _StaticVB_H_
#define _StaticVB_H_

template <class VertexType> class StaticVB 
{
  private :

    LPDIRECT3DVERTEXBUFFER9 m_pVB;
    uint mVertexCount;
    bool    mbLocked;
    VertexType* m_pLockedData;

  public :

    uint GetVertexCount() const 
    { 
      return mVertexCount; 
    }

    StaticVB( const LPDIRECT3DDEVICE9 pD3D, const DWORD& theFVF, const unsigned int& theVertexCount )
    {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
      m_pVB = 0;

      mbLocked = false;
      m_pLockedData = NULL;

      mVertexCount = theVertexCount;
    
      HRESULT hr = pD3D->CreateVertexBuffer( mVertexCount * sizeof( VertexType ), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, theFVF, D3DPOOL_DEFAULT, &m_pVB, NULL);
      assert( ( hr == D3D_OK ) && ( m_pVB ) );
#endif
    }

    LPDIRECT3DVERTEXBUFFER9 GetInterface() const { return m_pVB; }

    VertexType* Lock( const unsigned int& theLockCount, unsigned int& theStartVertex )
    {
      theStartVertex = 0;

      // Ensure there is enough space in the VB for this data
      assert ( theLockCount <= mVertexCount );

      if (mbLocked)
        return m_pLockedData;

      if ( m_pVB )
      {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
        DWORD dwFlags = D3DLOCK_DISCARD;
        DWORD dwSize = 0;

        HRESULT hr = m_pVB->Lock( 0, 0, reinterpret_cast<void**>( &m_pLockedData ), dwFlags );

        assert( hr == D3D_OK );
        assert( m_pLockedData != 0 );
        mbLocked = true;
#endif
      }

      return m_pLockedData;
    }

    void Unlock()
    {
      if ( ( mbLocked ) && ( m_pVB ) )
      {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
        HRESULT hr = m_pVB->Unlock();        
        assert( hr == D3D_OK );
        mbLocked = false;
#endif
      }
    }

    ~StaticVB()
    {
      Unlock();
      if ( m_pVB )
      {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
        m_pVB->Release();
#endif
      }
    }
  
};

#endif  _StaticVB_H_
