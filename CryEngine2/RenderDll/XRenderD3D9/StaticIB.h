#ifndef _STATICIB_H_
#define _STATICIB_H_

/////////////////////////////
// D. Sim Dietrich Jr.
// sim.dietrich@nvidia.com
//////////////////////

template < class IndexType > class StaticIB
{
  private :

    LPDIRECT3DINDEXBUFFER9 mpIB;

    uint mIndexCount;
    bool    mbLocked;
    IndexType* m_pLockedData;

  public :

    unsigned int GetIndexCount() const 
    { 
      return mIndexCount; 
    }

    StaticIB( const LPDIRECT3DDEVICE9 pD3D, const unsigned int& theIndexCount )
    {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
      mpIB = 0;
      mbLocked = false;

      mIndexCount = theIndexCount;
    
      HRESULT hr = pD3D->CreateIndexBuffer( mIndexCount * sizeof( IndexType ), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &mpIB, NULL);

      assert( ( hr == D3D_OK ) && ( mpIB ) );
#endif
    }

    LPDIRECT3DINDEXBUFFER9 GetInterface() const { return mpIB; }

    IndexType* Lock( const unsigned int& theLockCount, unsigned int& theStartIndex )
    {
      // Ensure there is enough space in the IB for this data
      assert ( theLockCount <= mIndexCount );

      if  (mbLocked)
        return m_pLockedData;

      if ( mpIB )
      {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
        DWORD dwFlags = D3DLOCK_DISCARD;
        DWORD dwSize = 0;

        HRESULT hr = mpIB->Lock( 0, 0, reinterpret_cast<void**>( &m_pLockedData ), dwFlags );

        assert( hr == D3D_OK );
        assert( m_pLockedData != 0 );
        mbLocked = true;
        theStartIndex = 0;
#endif
      }

      return m_pLockedData;
    }

    void Unlock()
    {
      if ( ( mbLocked ) && ( mpIB ) )
      {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
        HRESULT hr = mpIB->Unlock();        
        assert( hr == D3D_OK );
        mbLocked = false;
#endif
      }
    }

    ~StaticIB()
    {
      Unlock();
      if ( mpIB )
      {
#if defined(PS3)
			assert("D3D9 call during PS3/D3D10 rendering");
#else
        mpIB->Release();
#endif
      }
    }
  
};

typedef StaticIB< unsigned short > StaticIB16;
typedef StaticIB< unsigned int > StaticIB32;

#endif  _STATICIB_H_
