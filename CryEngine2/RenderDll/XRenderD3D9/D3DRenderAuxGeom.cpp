#include "StdAfx.h"
#include "DriverD3D.h"
#include "D3DRenderAuxGeom.h"


const float c_clipThres( 0.1f );

enum EAuxGeomBufferSizes
{
	e_auxGeomVBSize = 32768,
	e_auxGeomIBSize = e_auxGeomVBSize * 2
	//e_auxGeomVBSize = 17,
	//e_auxGeomIBSize = 41
};



struct SAuxObjVertex
{
	SAuxObjVertex()
	{
	}

	SAuxObjVertex( const Vec3& pos, const Vec3& normal )
		: m_pos( pos )
		, m_normal( normal )
	{
	}

	Vec3 m_pos;
	Vec3 m_normal;
};


typedef std::vector< SAuxObjVertex > AuxObjVertexBuffer;
typedef std::vector< uint16 > AuxObjIndexBuffer;


CD3DRenderAuxGeom::CD3DRenderAuxGeom( CD3D9Renderer& renderer )
: m_renderer( renderer )
, m_pAuxGeomVB( 0 )
, m_pAuxGeomIB( 0 )
, m_pCurVB( 0 )
, m_pCurIB( 0 )
, m_auxGeomSBM()
, m_wndXRes( 0 )
, m_wndYRes( 0 )
, m_aspect( 1.0f )
, m_aspectInv( 1.0f )
, m_matrices()
, m_curPrimType( e_PrimTypeInvalid )
, m_curPointSize( 1 )
, m_transMatrixIdx( -1 )
, m_pAuxGeomShader( 0 )
, m_curDrawInFrontMode( e_DrawInFrontOff )
, m_colOrthoMatrices()
{
}


CD3DRenderAuxGeom::~CD3DRenderAuxGeom()
{
	// m_pAuxGeomShader released by CShaderMan ???
	//delete m_pAuxGeomShader;
	//m_pAuxGeomShader = 0;
}


void 
CD3DRenderAuxGeom::GetMemoryUsage( ICrySizer* pSizer )
{
	SIZER_COMPONENT_NAME( pSizer, "Renderer (Aux Geometries)" );
	pSizer->Add( *this );
	CRenderAuxGeom::GetMemoryUsage( pSizer );
}


void
CD3DRenderAuxGeom::ReleaseDeviceObjects()
{
	SAFE_RELEASE( m_pAuxGeomVB );
	SAFE_RELEASE( m_pAuxGeomIB );

	for( uint32 i( 0 ); i < e_auxObjNumLOD; ++i )
	{
		m_sphereObj[ i ].Release();
		m_coneObj[ i ].Release();
		m_cylinderObj[ i ].Release();
	}	
}


// function to generate a sphere mesh
static void
CreateSphere( AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib, float radius, unsigned int rings, unsigned int sections )
{
	// calc required number of vertices/indices/triangles to build a sphere for the given parameters
	uint32 numVertices( ( rings - 1 ) * ( sections + 1 ) + 2 );
	uint32 numTriangles( ( rings - 2 ) * sections * 2  + 2 * sections );
	uint32 numIndices( numTriangles * 3 );

	// setup buffers
	vb.clear();
	vb.reserve( numVertices );

	ib.clear();
	ib.reserve( numIndices );

	// 1st pole vertex
	vb.push_back( SAuxObjVertex( Vec3( 0.0f, 0.0f, radius ), Vec3( 0.0f, 0.0f, 1.0f ) ) );

	// calculate "inner" vertices
	float sectionSlice( DEG2RAD( 360.0f / (float) sections ) );
	float ringSlice( DEG2RAD( 180.0f / (float) rings ) );

	for( uint32 a( 1 ); a < rings; ++a )
	{
		float w( sinf( a * ringSlice ) );
		for( uint32 i( 0 ); i <= sections; ++i )
		{
			Vec3 v;
			v.x = radius * cosf( i * sectionSlice ) * w;
			v.y = radius * sinf( i * sectionSlice ) * w;
			v.z = radius * cosf( a * ringSlice );
			vb.push_back( SAuxObjVertex( v, v.GetNormalized() ) );
		}
	}
	
	// 2nd vertex of pole (for end cap)
	vb.push_back( SAuxObjVertex( Vec3( 0.0f, 0.0f, -radius ), Vec3( 0.0f, 0.0f, 1.0f ) ) );

	// build "inner" faces
	for( uint32 a( 0 ); a < rings - 2; ++a )
	{
		for( uint32 i( 0 ); i < sections; ++i )
		{
			ib.push_back( (uint16) ( 1 + a * ( sections + 1 ) + i + 1 ) );
			ib.push_back( (uint16) ( 1 + a * ( sections + 1 ) + i ) );
			ib.push_back( (uint16) ( 1 + ( a + 1 ) * ( sections + 1 ) + i + 1 ) );

			ib.push_back( (uint16) ( 1 + ( a + 1 ) * ( sections + 1 ) + i ) );
			ib.push_back( (uint16) ( 1 + ( a + 1 ) * ( sections + 1 ) + i + 1 ) );
			ib.push_back( (uint16) ( 1 + a * ( sections + 1 ) + i ) );
		}
	}

	// build faces for end caps (to connect "inner" vertices with poles)
	for( uint32 i( 0 ); i < sections; ++i )
	{
		ib.push_back( (uint16) ( 1 + ( 0 ) * ( sections + 1 ) + i ) );
		ib.push_back( (uint16) ( 1 + ( 0 ) * ( sections + 1 ) + i + 1 ) );
		ib.push_back( (uint16) 0 );
	}

	for( uint32 i( 0 ); i < sections; ++i )
	{
		ib.push_back( (uint16) ( 1 + ( rings - 2 ) * ( sections + 1 ) + i + 1 ) );
		ib.push_back( (uint16) ( 1 + ( rings - 2 ) * ( sections + 1 ) + i ) );
		ib.push_back( (uint16) ( ( rings - 1 ) * ( sections + 1 ) + 1 ) );
	}
}



// function to generate a cone mesh
static void
CreateCone( AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib, float radius, float height, unsigned int sections )
{
	// calc required number of vertices/indices/triangles to build a cone for the given parameters
	uint32 numVertices( 2 * ( sections + 1 ) + 2 );
	uint32 numTriangles( 2 * sections );
	uint32 numIndices( numTriangles * 3 );

	// setup buffers
	vb.clear();
	vb.reserve( numVertices );

	ib.clear();
	ib.reserve( numIndices );

	// center vertex
	vb.push_back( SAuxObjVertex( Vec3( 0.0f, 0.0f, 0.0f ), Vec3( 0.0f, -1.0f, 0.0f ) ) );

	// create circle around it
	float sectionSlice( DEG2RAD( 360.0f / (float) sections ) );
	for( uint32 i( 0 ); i <= sections; ++i )
	{
		Vec3 v;
		v.x = radius * cosf( i * sectionSlice );
		v.y = 0.0f;
		v.z = radius * sinf( i * sectionSlice );
		vb.push_back( SAuxObjVertex( v, Vec3( 0.0f, -1.0f, 0.0f ) ) );
	}

	// build faces for end cap 
	for( uint16 i( 0 ); i < sections; ++i )
	{
		ib.push_back( 0 );
		ib.push_back( 1 + i );
		ib.push_back( 1 + i + 1 );
	}

	// top
	vb.push_back( SAuxObjVertex( Vec3( 0.0f, height, 0.0f ), Vec3( 0.0f, 1.0f, 0.0f ) ) );

	for( uint32 i( 0 ); i <= sections; ++i )
	{
		Vec3 v;
		v.x = radius * cosf( i * sectionSlice );
		v.y = 0.0f;
		v.z = radius * sinf( i * sectionSlice );

		Vec3 v1;
		v1.x = radius * cosf( i * sectionSlice + 0.01f );
		v1.y = 0.0f;
		v1.z = radius * sinf( i * sectionSlice + 0.01f );

		Vec3 d( v1 - v );
		Vec3 d1( Vec3( 0.0, height, 0.0f ) - v );

		Vec3 n( ( d1.Cross( d ) ).normalized() );
		vb.push_back( SAuxObjVertex( v, n ) );
	}

	// build faces
	for( uint16 i( 0 ); i < sections; ++i )
	{
		ib.push_back( sections + 2 );
		ib.push_back( sections + 3 + i + 1 );
		ib.push_back( sections + 3 + i );
	}
}


// function to generate a cylinder mesh
static void
CreateCylinder( AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib, float radius, float height, unsigned int sections )
{
	// calc required number of vertices/indices/triangles to build a cylinder for the given parameters
	uint32 numVertices( 4 * ( sections + 1 ) + 2 );
	uint32 numTriangles( 4 * sections );
	uint32 numIndices( numTriangles * 3 );

	// setup buffers
	vb.clear();
	vb.reserve( numVertices );

	ib.clear();
	ib.reserve( numIndices );

	float sectionSlice( DEG2RAD( 360.0f / (float) sections ) );

	// bottom cap
	{
		// center bottom vertex
		vb.push_back( SAuxObjVertex( Vec3( 0.0f, -0.5f * height, 0.0f ), Vec3( 0.0f, -1.0f, 0.0f ) ) );

		// create circle around it
		for( uint32 i( 0 ); i <= sections; ++i )
		{
			Vec3 v;
			v.x = radius * cosf( i * sectionSlice );
			v.y = -0.5f * height;
			v.z = radius * sinf( i * sectionSlice );
			vb.push_back( SAuxObjVertex( v, Vec3( 0.0f, -1.0f, 0.0f ) ) );
		}

		// build faces
		for( uint16 i( 0 ); i < sections; ++i )
		{
			ib.push_back( 0 );
			ib.push_back( 1 + i );
			ib.push_back( 1 + i + 1 );
		}
	}

	// side
	{
		int vIdx( vb.size() );

		for( uint32 i( 0 ); i <= sections; ++i )
		{
			Vec3 v;
			v.x = radius * cosf( i * sectionSlice );
			v.y = -0.5f * height;
			v.z = radius * sinf( i * sectionSlice );

			Vec3 n( v.normalized() );
			vb.push_back( SAuxObjVertex( v, n ) );
			vb.push_back( SAuxObjVertex( Vec3( v.x, -v.y, v.z ), n ) );
		}

		// build faces
		for( uint16 i( 0 ); i < sections; ++i, vIdx += 2 )
		{
			ib.push_back( vIdx );
			ib.push_back( vIdx + 1 );
			ib.push_back( vIdx + 2 );

			ib.push_back( vIdx + 1 );
			ib.push_back( vIdx + 3 );
			ib.push_back( vIdx + 2 );

		}
	}

	// top cap
	{
		int vIdx( vb.size() );

		// center top vertex
		vb.push_back( SAuxObjVertex( Vec3( 0.0f, 0.5f * height, 0.0f ), Vec3( 0.0f, 1.0f, 0.0f ) ) );

		// create circle around it
		for( uint32 i( 0 ); i <= sections; ++i )
		{
			Vec3 v;
			v.x = radius * cosf( i * sectionSlice );
			v.y = 0.5f * height;
			v.z = radius * sinf( i * sectionSlice );
			vb.push_back( SAuxObjVertex( v, Vec3( 0.0f, 1.0f, 0.0f ) ) );
		}

		// build faces
		for( uint16 i( 0 ); i < sections; ++i )
		{
			ib.push_back( vIdx );
			ib.push_back( vIdx + 1 + i + 1 );
			ib.push_back( vIdx + 1 + i );
		}
	}
}


// Functor to generate a sphere mesh. To be used with generalized CreateMesh function.
struct SSphereMeshCreateFunc
{
	SSphereMeshCreateFunc( float radius, unsigned int rings, unsigned int sections )
		: m_radius( radius )
		, m_rings( rings )
		, m_sections( sections )
	{
	}

	void CreateMesh( AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib )
	{
		CreateSphere( vb, ib, m_radius, m_rings, m_sections );
	}

	float m_radius;
	unsigned int m_rings;
	unsigned int m_sections;
};


// Functor to generate a cone mesh. To be used with generalized CreateMesh function.
struct SConeMeshCreateFunc
{
	SConeMeshCreateFunc( float radius, float height, unsigned int sections )
		: m_radius( radius )
		, m_height( height )
		, m_sections( sections )
	{
	}

	void CreateMesh( AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib )
	{
		CreateCone( vb, ib, m_radius, m_height, m_sections );
	}

	float m_radius;
	float m_height;
	unsigned int m_sections;
};


// Functor to generate a cylinder mesh. To be used with generalized CreateMesh function.
struct SCylinderMeshCreateFunc
{
	SCylinderMeshCreateFunc( float radius, float height, unsigned int sections )
		: m_radius( radius )
		, m_height( height )
		, m_sections( sections )
	{
	}

	void CreateMesh( AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib )
	{
		CreateCylinder( vb, ib, m_radius, m_height, m_sections );
	}

	float m_radius;
	float m_height;
	unsigned int m_sections;
};


// Generalized CreateMesh function to create any mesh via passed-in functor.
// The functor needs to provide a CreateMesh function which accepts an 
// AuxObjVertexBuffer and AuxObjIndexBuffer to stored the resulting mesh.
template< typename TMeshFunc >
HRESULT 
CD3DRenderAuxGeom::CreateMesh( SDrawObjMesh& mesh, TMeshFunc meshFunc )
{
	//LPDIRECT3DDEVICE9 pd3dDevice( gcpRendD3D->m_pd3dDevice );

	// create mesh
	std::vector< SAuxObjVertex > vb;
	std::vector< uint16 > ib;
	meshFunc.CreateMesh( vb, ib );

	// create vertex buffer and copy data
	HRESULT hr( S_OK );
#if defined (DIRECT3D9) || defined(OPENGL)
	if( FAILED( hr = gcpRendD3D->m_pd3dDevice->CreateVertexBuffer( vb.size() * sizeof( SAuxObjVertex ), 0, 0, D3DPOOL_MANAGED, &mesh.m_pVB, 0 ) ) )
	{
		return( hr );
	}
	SAuxObjVertex* pVertices( 0 );
	if( FAILED( hr = mesh.m_pVB->Lock( 0, 0, (void **) &pVertices, 0 ) ) )
	{
		return( hr );
	}

	memcpy( pVertices, &vb[ 0 ], vb.size() * sizeof( SAuxObjVertex ) );
	hr = mesh.m_pVB->Unlock();
#elif defined (DIRECT3D10)
  D3D10_BUFFER_DESC BufDescV;
  ZeroStruct(BufDescV);
  BufDescV.ByteWidth = vb.size() * sizeof( SAuxObjVertex );
  BufDescV.Usage = D3D10_USAGE_DEFAULT;
  BufDescV.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  BufDescV.CPUAccessFlags = 0;
  BufDescV.MiscFlags = 0;

  D3D10_SUBRESOURCE_DATA InitData;
  InitData.pSysMem = &vb[0];
  InitData.SysMemPitch = 0;
  InitData.SysMemSlicePitch = 0;
  if( FAILED( hr = gcpRendD3D->m_pd3dDevice->CreateBuffer( &BufDescV, &InitData, &mesh.m_pVB ) ) )
  {
    assert(SUCCEEDED(hr));
    return( hr );
  }
#endif

#if defined (DIRECT3D9) || defined(OPENGL)
	// create index buffer and copy data
	if( FAILED( hr = gcpRendD3D->m_pd3dDevice->CreateIndexBuffer( ib.size() * sizeof( uint16 ), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &mesh.m_pIB, 0 ) ) )
	{
		return( hr );
	}
	uint16* pIndices( 0 );
	if( FAILED( hr = mesh.m_pIB->Lock( 0, 0, (void **) &pIndices, 0 ) ) )
	{
		return( hr );
	}

	memcpy( pIndices, &ib[ 0 ], ib.size() * sizeof( uint16 ) );
	hr = mesh.m_pIB->Unlock();
#elif defined (DIRECT3D10)
  D3D10_BUFFER_DESC BufDescI;
  ZeroStruct(BufDescI);
  BufDescI.ByteWidth = ib.size() * sizeof( uint16 );
  BufDescI.Usage = D3D10_USAGE_DEFAULT;
  BufDescI.BindFlags = D3D10_BIND_INDEX_BUFFER;
  BufDescI.CPUAccessFlags = 0;
  BufDescI.MiscFlags = 0;

  InitData.pSysMem = &ib[0];
  InitData.SysMemPitch = 0;
  InitData.SysMemSlicePitch = 0;
  if( FAILED( hr = gcpRendD3D->m_pd3dDevice->CreateBuffer(&BufDescI, &InitData, &mesh.m_pIB ) ) )
  {
    assert(SUCCEEDED(hr));
    return( hr );
  }
#endif

	// write mesh info
	mesh.m_numVertices = vb.size();
	mesh.m_numFaces = ib.size() / 3;

	return( hr );
}


HRESULT
CD3DRenderAuxGeom::RestoreDeviceObjects()
{
	HRESULT hr( S_OK );
#if defined (DIRECT3D9) || defined(OPENGL)
	LPDIRECT3DDEVICE9 pd3dDevice( gcpRendD3D->m_pd3dDevice );

	// recreate vertex buffer 
	SAFE_RELEASE( m_pAuxGeomVB );
	if( FAILED( hr = pd3dDevice->CreateVertexBuffer( e_auxGeomVBSize * sizeof( SAuxVertex ), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 
		0, D3DPOOL_DEFAULT, &m_pAuxGeomVB, NULL ) ) )
	{
		return( hr );
	}

	// recreate index buffer
	SAFE_RELEASE( m_pAuxGeomIB );
	if( FAILED( hr = pd3dDevice->CreateIndexBuffer( e_auxGeomIBSize * sizeof( uint16 ), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 
		D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_pAuxGeomIB, NULL ) ) )
	{
		return( hr );
	}
#elif defined (DIRECT3D10)
  ID3D10Device* pd3dDevice( gcpRendD3D->m_pd3dDevice );

  // recreate vertex buffer 
  SAFE_RELEASE( m_pAuxGeomVB );

  D3D10_BUFFER_DESC BufDescV;
  ZeroStruct(BufDescV);
  BufDescV.ByteWidth = e_auxGeomVBSize * sizeof( SAuxVertex );
  BufDescV.Usage = D3D10_USAGE_DYNAMIC;
  BufDescV.BindFlags = D3D10_BIND_VERTEX_BUFFER;
  BufDescV.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  BufDescV.MiscFlags = 0;

  if( FAILED( hr = pd3dDevice->CreateBuffer( &BufDescV , 0, &m_pAuxGeomVB ) ) )
  {
		assert(0);
    return( hr );
  }

  // recreate index buffer
  SAFE_RELEASE( m_pAuxGeomIB );
  D3D10_BUFFER_DESC BufDescI;
  ZeroStruct(BufDescI);
  BufDescI.ByteWidth =  e_auxGeomIBSize * sizeof( uint16 );
  BufDescI.Usage = D3D10_USAGE_DYNAMIC;
  BufDescI.BindFlags = D3D10_BIND_INDEX_BUFFER;
  BufDescI.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
  BufDescI.MiscFlags = 0;
  if( FAILED( hr = pd3dDevice->CreateBuffer( &BufDescI, 0, &m_pAuxGeomIB ) ) )
  {
    assert(0);
    return( hr );
  }
#endif

	// recreate aux objects
	for( uint32 i( 0 ); i < e_auxObjNumLOD; ++i )
	{
		m_sphereObj[ i ].Release();
		if( FAILED( hr = CreateMesh( m_sphereObj[ i ], SSphereMeshCreateFunc( 1.0f, 9 + 4 * i, 9 + 4 * i ) ) ) )
		{
			return( hr );
		}

		m_coneObj[ i ].Release();
		if( FAILED( hr = CreateMesh( m_coneObj[ i ], SConeMeshCreateFunc( 1.0f, 1.0f, 10 + i * 6 ) ) ) )
		{
			return( hr );
		}

		m_cylinderObj[ i ].Release();
		if( FAILED( hr = CreateMesh( m_cylinderObj[ i ], SCylinderMeshCreateFunc( 1.0f, 1.0f, 10 + i * 6 ) ) ) )
		{
			return( hr );
		}
	}	
	return( hr );
}


void
CD3DRenderAuxGeom::DrawAuxPrimitives( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd, const EPrimType& primType )
{
	assert( e_PtList == primType || e_LineList == primType || e_TriList == primType );

	HRESULT hr( S_OK );
	//LPDIRECT3DDEVICE9 pd3dDevice( gcpRendD3D->m_pd3dDevice );

	// bind vertex and index streams and set vertex declaration
	BindStreams( VERTEX_FORMAT_P3F_COL4UB, m_pAuxGeomVB, m_pAuxGeomIB );

	// get aux vertex buffer
	const AuxVertexBuffer& auxVertexBuffer( GetAuxVertexBuffer() );

	// determine flags for prim type
	uint32 d3dNumPrimDivider;
#if defined (DIRECT3D9) || defined(OPENGL)
	D3DPRIMITIVETYPE d3dPrim;
#elif defined (DIRECT3D10)
  D3D10_PRIMITIVE_TOPOLOGY d3dPrim;
#endif
	DetermineAuxPrimitveFlags( d3dNumPrimDivider, d3dPrim, primType );

	// helpers for DP call
	uint32 initialVBLockOffset( m_auxGeomSBM.m_curVBIndex );
	uint32 numVerticesWrittenToVB( 0 );

  gcpRendD3D->EF_Commit();

	// process each entry
	for( AuxSortedPushBuffer::const_iterator it( itBegin ); it != itEnd; ++it )
	{
		// get current push buffer entry
		const SAuxPushBufferEntry* curPBEntry( *it );

		// number of vertices to copy
		uint32 verticesToCopy( curPBEntry->m_numVertices );
		uint32 verticesCopied( 0 );

		// stream vertex data
		while( verticesToCopy > 0 )
		{
			// number of vertices which fit into current vb
			uint32 maxVerticesInThisBatch( e_auxGeomVBSize - m_auxGeomSBM.m_curVBIndex );

			// round down to previous multiple of "d3dNumPrimDivider"
			maxVerticesInThisBatch -= maxVerticesInThisBatch % d3dNumPrimDivider;

			// still enough space to feed data in the current vb
			if( maxVerticesInThisBatch > 0 )
			{
				// compute amount of vertices to move in this batch
				uint32 toCopy( verticesToCopy > maxVerticesInThisBatch ? maxVerticesInThisBatch : verticesToCopy );

				// get pointer to vertex buffer
				SAuxVertex* pVertices( 0 );
#if defined (DIRECT3D9) || defined(OPENGL)
        // determine lock flags
        uint32 lockFlags( D3DLOCK_NOOVERWRITE );
        if( false != m_auxGeomSBM.m_discardVB )
        {
          m_auxGeomSBM.m_discardVB = false;
          lockFlags = D3DLOCK_DISCARD;
        }				

				if( FAILED( hr = m_pAuxGeomVB->Lock( m_auxGeomSBM.m_curVBIndex * sizeof( SAuxVertex ), toCopy * sizeof( SAuxVertex ), (void**) &pVertices, lockFlags ) ) )
				{
					assert( 0 );
					iLog->Log( "ERROR: CD3DRenderAuxGeom::DrawAuxPrimitives() - Vertex buffer could not be locked!" );
					return;
				}				
#elif defined (DIRECT3D10)
        // determine lock flags
        D3D10_MAP mapFlags( D3D10_MAP_WRITE_NO_OVERWRITE );
        if( false != m_auxGeomSBM.m_discardVB )
        {
          m_auxGeomSBM.m_discardVB = false;
          mapFlags = D3D10_MAP_WRITE_DISCARD;
        }				

        if( FAILED( hr = m_pAuxGeomVB->Map( mapFlags, 0, (void**) &pVertices ) ) )
        {
          assert( 0 );
          iLog->Log( "ERROR: CD3DRenderAuxGeom::DrawAuxPrimitives() - Vertex buffer could not be locked!" );
          return;
        }				
        pVertices += m_auxGeomSBM.m_curVBIndex;
#endif

				// move vertex data
				memcpy( pVertices, &auxVertexBuffer[ curPBEntry->m_vertexOffs + verticesCopied ], toCopy * sizeof( SAuxVertex ) );

				// unlock vb
#if defined (DIRECT3D9) || defined(OPENGL)
				hr = m_pAuxGeomVB->Unlock();
#elif defined (DIRECT3D10)
        m_pAuxGeomVB->Unmap();
#endif
				// update accumulators and buffer indices
				verticesCopied += toCopy;
				verticesToCopy -= toCopy;

				m_auxGeomSBM.m_curVBIndex += toCopy;
				numVerticesWrittenToVB += toCopy;
			}
			else
			{
				// not enough space in vb for (remainings of) current push buffer entry
				if( numVerticesWrittenToVB > 0 )
				{
					// commit batch 
					assert( 0 == numVerticesWrittenToVB % d3dNumPrimDivider );
#if defined (DIRECT3D9) || defined(OPENGL)
					hr = gcpRendD3D->m_pd3dDevice->DrawPrimitive( d3dPrim, initialVBLockOffset, numVerticesWrittenToVB / d3dNumPrimDivider );
#elif defined (DIRECT3D10)
          gcpRendD3D->SetPrimitiveTopology(d3dPrim);
          gcpRendD3D->m_pd3dDevice->Draw(numVerticesWrittenToVB, initialVBLockOffset);
#endif
				}

				// request a DISCARD lock of vb in the next run
				m_auxGeomSBM.DiscardVB();
				initialVBLockOffset = m_auxGeomSBM.m_curVBIndex;
				numVerticesWrittenToVB = 0;				
			}
		}
	}

	if( numVerticesWrittenToVB > 0 )
	{
		// commit batch
		assert( 0 == numVerticesWrittenToVB % d3dNumPrimDivider );
#if defined (DIRECT3D9) || defined(OPENGL)
		hr = gcpRendD3D->m_pd3dDevice->DrawPrimitive( d3dPrim, initialVBLockOffset, numVerticesWrittenToVB / d3dNumPrimDivider );
#elif defined (DIRECT3D10)
    gcpRendD3D->SetPrimitiveTopology(d3dPrim);
    gcpRendD3D->m_pd3dDevice->Draw(numVerticesWrittenToVB, initialVBLockOffset);
#endif
	}
}


void
CD3DRenderAuxGeom::DrawAuxIndexedPrimitives( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd, const EPrimType& primType )
{
	assert( e_LineListInd == primType || e_TriListInd == primType );

	HRESULT hr( S_OK );
	//LPDIRECT3DDEVICE9 pd3dDevice( gcpRendD3D->m_pd3dDevice );

	// bind vertex and index streams and set vertex declaration
	BindStreams( VERTEX_FORMAT_P3F_COL4UB, m_pAuxGeomVB, m_pAuxGeomIB );

	// get aux vertex and index buffer
	const AuxVertexBuffer& auxVertexBuffer( GetAuxVertexBuffer() );
	const AuxIndexBuffer& auxIndexBuffer( GetAuxIndexBuffer() );

	// determine flags for prim type
	uint32 d3dNumPrimDivider;
#if defined (DIRECT3D9) || defined(OPENGL)
	D3DPRIMITIVETYPE d3dPrim;
#elif defined (DIRECT3D10)
  D3D10_PRIMITIVE_TOPOLOGY d3dPrim;
#endif
	DetermineAuxPrimitveFlags( d3dNumPrimDivider, d3dPrim, primType );

	// helpers for DP call
	uint32 initialVBLockOffset( m_auxGeomSBM.m_curVBIndex );
	uint32 numVerticesWrittenToVB( 0 );
	uint32 initialIBLockOffset( m_auxGeomSBM.m_curIBIndex );
	uint32 numIndicesWrittenToIB( 0 );

  gcpRendD3D->EF_Commit();

	// process each entry
	for( AuxSortedPushBuffer::const_iterator it( itBegin ); it != itEnd; )
	{
		// get current push buffer entry
		const SAuxPushBufferEntry* curPBEntry( *it );

		// process a push buffer entry if it can fit at all (otherwise silently skip it)
		if( e_auxGeomVBSize >= curPBEntry->m_numVertices && e_auxGeomIBSize >= curPBEntry->m_numIndices )
		{
			// check if push buffer still fits into current buffer
			if( e_auxGeomVBSize >= m_auxGeomSBM.m_curVBIndex + curPBEntry->m_numVertices && e_auxGeomIBSize >= m_auxGeomSBM.m_curIBIndex + curPBEntry->m_numIndices )
			{
				// determine lock vb flags

				// get pointer to vertex buffer
				SAuxVertex* pVertices( 0 );
#if defined (DIRECT3D9) || defined(OPENGL)
        uint32 lockFlags( D3DLOCK_NOOVERWRITE );
        if( false != m_auxGeomSBM.m_discardVB )
        {
          m_auxGeomSBM.m_discardVB = false;
          lockFlags = D3DLOCK_DISCARD;
        }				
				if( FAILED( hr = m_pAuxGeomVB->Lock( m_auxGeomSBM.m_curVBIndex * sizeof( SAuxVertex ), curPBEntry->m_numVertices * sizeof( SAuxVertex ), (void**) &pVertices, lockFlags ) ) )
				{
					assert( 0 );
					iLog->Log( "ERROR: CD3DRenderAuxGeom::DrawAuxIndexedPrimitives() - Vertex buffer could not be locked!" );
					return;
				}				
#elif defined (DIRECT3D10)
        D3D10_MAP mp (D3D10_MAP_WRITE_NO_OVERWRITE);
        if( false != m_auxGeomSBM.m_discardVB )
        {
          m_auxGeomSBM.m_discardVB = false;
          mp = D3D10_MAP_WRITE_DISCARD;
        }				
        if( FAILED( hr = m_pAuxGeomVB->Map(mp, 0, (void**) &pVertices) ) )
        {
          assert( 0 );
          iLog->Log( "ERROR: CD3DRenderAuxGeom::DrawAuxIndexedPrimitives() - Vertex buffer could not be locked!" );
          return;
        }				
        pVertices += m_auxGeomSBM.m_curVBIndex;
#endif

				// move vertex data of this entry
				memcpy( pVertices, &auxVertexBuffer[ curPBEntry->m_vertexOffs ], curPBEntry->m_numVertices * sizeof( SAuxVertex ) );

				// unlock vb
#if defined (DIRECT3D9) || defined(OPENGL)
				hr = m_pAuxGeomVB->Unlock();
#elif defined (DIRECT3D10)
        m_pAuxGeomVB->Unmap();
#endif

				// get pointer to index buffer
				uint16* pIndices( 0 );
#if defined (DIRECT3D9) || defined(OPENGL)
        // determine lock ib flags
        lockFlags = D3DLOCK_NOOVERWRITE;
        if( false != m_auxGeomSBM.m_discardIB )
        {
          m_auxGeomSBM.m_discardIB = false;
          lockFlags = D3DLOCK_DISCARD;
        }				
				if( FAILED( hr = m_pAuxGeomIB->Lock( m_auxGeomSBM.m_curIBIndex * sizeof( uint16 ), curPBEntry->m_numIndices * sizeof( uint16 ), (void**) &pIndices, lockFlags ) ) )
				{
					assert( 0 );
					iLog->Log( "ERROR: CD3DRenderAuxGeom::DrawAuxIndexedPrimitives() - Index buffer could not be locked!" );

					m_pAuxGeomVB->Unlock();
					return;
				}				
#elif defined (DIRECT3D10)
        mp = D3D10_MAP_WRITE_NO_OVERWRITE;
        if( false != m_auxGeomSBM.m_discardIB )
        {
          m_auxGeomSBM.m_discardIB = false;
          mp = D3D10_MAP_WRITE_DISCARD;
        }				
        if( FAILED( hr = m_pAuxGeomIB->Map(mp, 0, (void**) &pIndices) ) )
        {
          assert( 0 );
          iLog->Log( "ERROR: CD3DRenderAuxGeom::DrawAuxIndexedPrimitives() - Index buffer could not be locked!" );

          m_pAuxGeomVB->Unmap();
          return;
        }				
        pIndices += m_auxGeomSBM.m_curIBIndex;
#endif

				// move index data of this entry (modify indices to match VB insert location)
				for( uint32 i( 0 ); i < curPBEntry->m_numIndices; ++i )
				{
					pIndices[ i ] = numVerticesWrittenToVB + auxIndexBuffer[ curPBEntry->m_indexOffs + i ] ;
				}

				// unlock ib
#if defined (DIRECT3D9) || defined(OPENGL)
				hr = m_pAuxGeomIB->Unlock();
#elif defined (DIRECT3D10)
        m_pAuxGeomIB->Unmap();
#endif

				// update buffer indices
				m_auxGeomSBM.m_curVBIndex += curPBEntry->m_numVertices;
				m_auxGeomSBM.m_curIBIndex += curPBEntry->m_numIndices;

				numVerticesWrittenToVB += curPBEntry->m_numVertices;
				numIndicesWrittenToIB += curPBEntry->m_numIndices;

				// advance to next push puffer entry
				++it;
			}
			else
			{
				// push buffer entry currently doesn't fit, will be processed in the next iteration when buffers got flushed
				if( numVerticesWrittenToVB > 0 && numIndicesWrittenToIB > 0 )
				{
					// commit batch 
					assert( 0 == numIndicesWrittenToIB % d3dNumPrimDivider );
#if defined (DIRECT3D9) || defined(OPENGL)
					hr = gcpRendD3D->m_pd3dDevice->DrawIndexedPrimitive( d3dPrim, initialVBLockOffset, 0, numVerticesWrittenToVB, initialIBLockOffset, numIndicesWrittenToIB / d3dNumPrimDivider );
#elif defined (DIRECT3D10)
          gcpRendD3D->SetPrimitiveTopology(d3dPrim);
          gcpRendD3D->m_pd3dDevice->DrawIndexed(numIndicesWrittenToIB, initialIBLockOffset, initialVBLockOffset);
#endif
				}

				// request a DISCARD lock / don't advance iterator!
				m_auxGeomSBM.DiscardVB();
				initialVBLockOffset = m_auxGeomSBM.m_curVBIndex;
				numVerticesWrittenToVB = 0;				

				m_auxGeomSBM.DiscardIB();
				initialIBLockOffset = m_auxGeomSBM.m_curIBIndex;
				numIndicesWrittenToIB = 0;				
			}
		}
		else
		{
			// push buffer entry too big for dedicated vb/ib buffer
			// advance to next push puffer entry
			assert( 0 );
			iLog->Log( "ERROR: CD3DRenderAuxGeom::DrawAuxIndexedPrimitives() - Auxiliary geometry too big to render!" );
			++it;
		}
	}

	if( numVerticesWrittenToVB > 0 && numIndicesWrittenToIB > 0 )
	{
		// commit batch 
		assert( 0 == numIndicesWrittenToIB % d3dNumPrimDivider );
#if defined (DIRECT3D9) || defined(OPENGL)
		hr = gcpRendD3D->m_pd3dDevice->DrawIndexedPrimitive( d3dPrim, initialVBLockOffset, 0, numVerticesWrittenToVB, initialIBLockOffset, numIndicesWrittenToIB / d3dNumPrimDivider );
#elif defined (DIRECT3D10)
    gcpRendD3D->SetPrimitiveTopology(d3dPrim);
    gcpRendD3D->m_pd3dDevice->DrawIndexed(numIndicesWrittenToIB, initialIBLockOffset, initialVBLockOffset);
#endif
	}
}


void 
CD3DRenderAuxGeom::DrawAuxObjects( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd )
{
	EAuxDrawObjType objType( GetAuxObjType( (*itBegin)->m_renderFlags ) );

	HRESULT hr( S_OK );
	//LPDIRECT3DDEVICE9 pd3dDevice( gcpRendD3D->m_pd3dDevice );

	// get draw params buffer
	const AuxDrawObjParamBuffer& auxDrawObjParamBuffer( GetAuxDrawObjParamBuffer() );

	// process each entry
	for( AuxSortedPushBuffer::const_iterator it( itBegin ); it != itEnd; ++it )
	{
		// get current push buffer entry
		const SAuxPushBufferEntry* curPBEntry( *it );

		// assert than all objects in this batch are of same type
		assert( GetAuxObjType( curPBEntry->m_renderFlags ) == objType );

		uint32 drawParamOffs( 0 );
		if( curPBEntry->GetDrawParamOffs( drawParamOffs ) )
		{
			// get draw params
			const SAuxDrawObjParams& drawParams( auxDrawObjParamBuffer[ drawParamOffs ] );

			// Prepare d3d world space matrix in draw param structure 
			// Attention: in d3d terms matWorld is actually matWorld^T
			D3DXMATRIX matWorld;
			D3DXMatrixIdentity( &matWorld );
			memcpy( &matWorld, &drawParams.m_matWorld, sizeof( drawParams.m_matWorld ) );

			// set transformation matrices
			static CCryName matWorldViewProjName("matWorldViewProj");
			if( m_curDrawInFrontMode == e_DrawInFrontOn )
			{
				D3DXMATRIX matScale;
				mathMatrixScaling( &matScale, 0.999f, 0.999f, 0.999f );

				D3DXMATRIX matWorldViewScaleProjT;
				mathMatrixMultiplyF( &matWorldViewScaleProjT, &matScale, &GetCurrentView(), g_CpuFlags	);
				mathMatrixMultiplyF( &matWorldViewScaleProjT, &GetCurrentProj(), &matWorldViewScaleProjT, g_CpuFlags	 );

				mathMatrixTransposeF( &matWorldViewScaleProjT, &matWorldViewScaleProjT );
				mathMatrixMultiplyF( &matWorldViewScaleProjT, &matWorld,	&matWorldViewScaleProjT, g_CpuFlags );

				m_pAuxGeomShader->FXSetVSFloat( matWorldViewProjName, (Vec4*) &matWorldViewScaleProjT._11, 4 );
			}
			else
			{
				D3DXMATRIX matWorldViewProjT;
				mathMatrixTransposeF( &matWorldViewProjT, m_matrices.m_pCurTransMat );
				mathMatrixMultiplyF( &matWorldViewProjT, &matWorld, &matWorldViewProjT, g_CpuFlags );
				m_pAuxGeomShader->FXSetVSFloat( matWorldViewProjName, (Vec4*) &matWorldViewProjT._11, 4 );
			}

			// set color
			ColorF col(drawParams.m_color);
			Vec4 colVec( col.b, col.g, col.r, col.a ); // need to flip r/b as drawParams.m_color was originally argb
			static CCryName auxGeomObjColorName("auxGeomObjColor");
			m_pAuxGeomShader->FXSetVSFloat( auxGeomObjColorName, &colVec, 1 );

			// set shading flag			
			Vec4 shadingVec( drawParams.m_shaded ? 0.4f : 0, drawParams.m_shaded ? 0.6f : 1, 0, 0 );
			static CCryName auxGeomObjShadingName("auxGeomObjShading");
			m_pAuxGeomShader->FXSetVSFloat( auxGeomObjShadingName, &shadingVec, 1 );

			// set light vector (rotate back into local space)
			Matrix33 matWorldInv( drawParams.m_matWorld.GetInverted() );
			Vec3 lightLocalSpace( matWorldInv * Vec3( 0.5773f, 0.5773f, 0.5773f ) );
			
			// normalize light vector (matWorld could contain non-uniform scaling)
			lightLocalSpace.Normalize();
			Vec4 lightVec( lightLocalSpace.x, lightLocalSpace.y, lightLocalSpace.z, 0.0f );
			static CCryName globalLightLocalName("globalLightLocal");
			m_pAuxGeomShader->FXSetVSFloat( globalLightLocalName, &lightVec, 1 );
			
			// LOD calculation
			D3DXMATRIX matWorldT;
			mathMatrixTransposeF( &matWorldT, &matWorld );

			D3DXVECTOR4 objCenterWorld;
			D3DXVECTOR3 nullVec( 0.0f, 0.0f, 0.0f );
			mathVec3TransformF( &objCenterWorld, &nullVec, &matWorldT );			
			D3DXVECTOR4 objOuterRightWorld( objCenterWorld + (D3DXVECTOR4( GetCurrentView()._11, GetCurrentView()._21, GetCurrentView()._31, 0.0f ) * drawParams.m_size) );

			D3DXVECTOR4 v0, v1;

			D3DXVECTOR3 objCenterWorldVec( objCenterWorld.x, objCenterWorld.y, objCenterWorld.z );
			D3DXVECTOR3 objOuterRightWorldVec( objOuterRightWorld.x, objOuterRightWorld.y, objOuterRightWorld.z );
			mathVec3TransformF( &v0, &objCenterWorldVec, m_matrices.m_pCurTransMat );
			mathVec3TransformF( &v1, &objOuterRightWorldVec, m_matrices.m_pCurTransMat );			

			float scale;
			assert( fabs(v0.w - v0.w ) < 1e-4 );
			if( fabs( v0.w ) < 1e-2 ) 
			{
				scale = 0.5f;
			}
			else
			{
				scale = ( ( v1.x - v0.x ) / v0.w ) * (float) max( m_wndXRes, m_wndYRes ) / 500.0f;
			}			

			// map scale to detail level
			uint32 lodLevel( (uint32) ( ( scale / 0.5f ) * ( e_auxObjNumLOD - 1 ) ) );
			if( lodLevel >= e_auxObjNumLOD )
			{
				lodLevel = e_auxObjNumLOD - 1;
			}

			// get appropriate mesh
			assert( lodLevel >= 0 && lodLevel < e_auxObjNumLOD );
			SDrawObjMesh* pMesh( 0 );
			switch( objType )
			{
			case eDOT_Sphere:
			default:
				{
					pMesh = &m_sphereObj[ lodLevel ];
					break;
				}
			case eDOT_Cone:
				{
					pMesh = &m_coneObj[ lodLevel ];
					break;
				}
			case eDOT_Cylinder:
				{
					pMesh = &m_cylinderObj[ lodLevel ];
					break;
				}
			}		
			assert( 0 != pMesh );

			// bind vertex and index streams and set vertex declaration
			BindStreams( VERTEX_FORMAT_P3F_TEX3F, pMesh->m_pVB, pMesh->m_pIB );

      gcpRendD3D->EF_Commit();

			// draw mesh
#if defined (DIRECT3D9) || defined(OPENGL)
			hr = gcpRendD3D->m_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, pMesh->m_numVertices, 0, pMesh->m_numFaces );
#elif defined (DIRECT3D10)
			gcpRendD3D->SetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      gcpRendD3D->m_pd3dDevice->DrawIndexed(pMesh->m_numFaces * 3, 0, 0);
#endif

			//// debug code to visualize the LOD scale value
			//// to use it remove the multiplications of matWorld in the "set transformation matrices" code above
			// 
			//SAuxObjVertex line[ 2 ] =
			//{
			//	SAuxObjVertex( Vec3( objCenterWorld.x, objCenterWorld.y, objCenterWorld.z ), Vec3( 0, 0, 0 ) ),
			//	SAuxObjVertex( Vec3( objOuterRightWorld.x, objOuterRightWorld.y, objOuterRightWorld.z ), Vec3( 0, 0, 0 ) )
			//};
			//hr = pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 1, &line, sizeof( SAuxObjVertex ) );
		}
		else
		{
			assert( 0 ); // GetDrawParamOffs( ... ) failed -- corrupt data in push buffer?
		}
	}
}


static inline D3DXVECTOR3
IntersectLinePlane( const D3DXVECTOR3& o, const D3DXVECTOR3& d, const D3DXPLANE& p, float& t )
{	
	t = -( D3DXPlaneDotNormal( &p, &o ) + ( p.d + c_clipThres ) ) / D3DXPlaneDotNormal( &p, &d );
	return( o + d * t);
}


static inline DWORD
ClipColor( const DWORD& c0, const DWORD& c1, float t )
{
	D3DXCOLOR v0( c0 ), v1( c1 );
	D3DXCOLOR vRes( v0 + ( v1 - v0 ) * t );
	return( D3DCOLOR_COLORVALUE( vRes.r, vRes.g, vRes.b, vRes.a ) );
}


static bool
ClipLine( D3DXVECTOR3* v, DWORD* c )
{
	// get near plane to perform clipping	
	const Plane& np( *gRenDev->GetCamera().GetFrustumPlane( FR_PLANE_NEAR ) );
	D3DXPLANE nearPlane( np.n.x, np.n.y, np.n.z, np.d );

	// get clipping flags
	bool bV0Behind( -( D3DXPlaneDotNormal( &nearPlane, &v[ 0 ] ) + nearPlane.d ) < c_clipThres );
	bool bV1Behind( -( D3DXPlaneDotNormal( &nearPlane, &v[ 1 ] ) + nearPlane.d ) < c_clipThres );

	// proceed only if both are not behind near clipping plane
	if( false == bV0Behind || false == bV1Behind )
	{		
		if( false == bV0Behind && false == bV1Behind )
		{
			// no clipping needed
			return( true );
		}

		// define line to be clipped
		D3DXVECTOR3 p( v[ 0 ] );
		D3DXVECTOR3 d( v[ 1 ] - v[ 0 ] );

		// get clipped position
		float t;
		v[ 0 ] = ( false == bV0Behind ) ? v[ 0 ] : IntersectLinePlane( p, d, nearPlane, t );
		v[ 1 ] = ( false == bV1Behind ) ? v[ 1 ] : IntersectLinePlane( p, d, nearPlane, t );

		// get clipped colors
		c[ 0 ] = ( false == bV0Behind ) ? c[ 0 ] : ClipColor( c[ 0 ], c[ 1 ], t );
		c[ 1 ] = ( false == bV1Behind ) ? c[ 1 ] : ClipColor( c[ 0 ], c[ 1 ], t );

		return( true );
	}	
	else
	{
		return( false );
	}
}


static float 
ComputeConstantScale( const D3DXVECTOR3& v, const D3DXMATRIX& matView, const D3DXMATRIX& matProj, const uint32 wndXRes )
{
	D3DXVECTOR4 vCam0; 
	mathVec3TransformF( &vCam0, &v, &matView );

	D3DXVECTOR4 vCam1( vCam0 );
	vCam1.x += 1.0f;

	float c0( ( vCam0.x * matProj.m[ 0 ][ 0 ] + 
		vCam0.y * matProj.m[ 1 ][ 0 ] + 
		vCam0.z * matProj.m[ 2 ][ 0 ] + 
		matProj.m[ 3 ][ 0 ] ) / ( vCam0.x * matProj.m[ 0 ][ 3 ] + 
		vCam0.y * matProj.m[ 1 ][ 3 ] + 
		vCam0.z * matProj.m[ 2 ][ 3 ] + 
		matProj.m[ 3 ][ 3 ] ) );

	float c1( ( vCam1.x * matProj.m[ 0 ][ 0 ] + 
		vCam1.y * matProj.m[ 1 ][ 0 ] + 
		vCam1.z * matProj.m[ 2 ][ 0 ] + 
		matProj.m[ 3 ][ 0 ] ) / ( vCam1.x * matProj.m[ 0 ][ 3 ] + 
		vCam1.y * matProj.m[ 1 ][ 3 ] + 
		vCam1.z * matProj.m[ 2 ][ 3 ] + 
		matProj.m[ 3 ][ 3 ] ) );

	return( 1.0f / ( (float) wndXRes * ( c1 - c0 ) ) );
}


void 
CD3DRenderAuxGeom::PrepareThickLines3D( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd )
{
	const AuxVertexBuffer& auxVertexBuffer( GetAuxVertexBuffer() );

	// process each entry
	for( AuxSortedPushBuffer::const_iterator it( itBegin ); it != itEnd; ++it )
	{
		// get current push buffer entry
		const SAuxPushBufferEntry* curPBEntry( *it );

		uint32 offset( curPBEntry->m_vertexOffs );
		for( uint32 i( 0 ); i < curPBEntry->m_numVertices / 6; ++i, offset += 6 )
		{
			// get line vertices and thickness parameter
			D3DXVECTOR3 v[ 2 ] = 
			{
				D3DXVECTOR3( (const float*) &(auxVertexBuffer[ offset + 0 ].xyz.x) ),
					D3DXVECTOR3( (const float*) &(auxVertexBuffer[ offset + 1 ].xyz.x) )
			};
			DWORD col[ 2 ] =
			{
				auxVertexBuffer[ offset + 0 ].color.dcolor,
					auxVertexBuffer[ offset + 1 ].color.dcolor
			};
			float thickness( auxVertexBuffer[ offset + 2 ].xyz.x );

			// compute depth corrected thickness of line end points
			float thicknessV0( 0.5f * thickness * ComputeConstantScale( v[ 0 ], GetCurrentView(), GetCurrentProj(), m_wndXRes ) );
			float thicknessV1( 0.5f * thickness * ComputeConstantScale( v[ 1 ], GetCurrentView(), GetCurrentProj(), m_wndXRes ) );

			bool skipLine( false );
			D3DXVECTOR4 vf[ 4 ];

			if( false == IsOrthoMode() ) // regular, 3d projected geometry
			{
				skipLine = !ClipLine( v, col );
				if( false == skipLine )
				{
					// compute camera space line delta
					D3DXVECTOR4 vt[ 2 ];
					mathVec3TransformF( &vt[ 0 ], &v[ 0 ], &GetCurrentView() );
					mathVec3TransformF( &vt[ 1 ], &v[ 1 ], &GetCurrentView() );
					D3DXVECTOR2 delta( vt[ 1 ] / vt[ 1 ].z - vt[ 0 ] / vt[ 0 ].z );

					// create screen space normal of line delta
					D3DXVECTOR2 normal( -delta.y , delta.x );
					mathVec2NormalizeF( &normal, &normal );

					D3DXVECTOR2 n[ 2 ];
					n[ 0 ] = normal * thicknessV0;
					n[ 1 ] = normal * thicknessV1;

					// compute final world space vertices of thick line
					D3DXVECTOR4 vertices[4] = 
						{	
							D3DXVECTOR4( vt[ 0 ].x + n[ 0 ].x, vt[ 0 ].y + n[ 0 ].y, vt[ 0 ].z, vt[ 0 ].w ),
							D3DXVECTOR4( vt[ 1 ].x + n[ 1 ].x, vt[ 1 ].y + n[ 1 ].y, vt[ 1 ].z, vt[ 1 ].w ),
							D3DXVECTOR4( vt[ 1 ].x - n[ 1 ].x, vt[ 1 ].y - n[ 1 ].y, vt[ 1 ].z, vt[ 1 ].w ),
							D3DXVECTOR4( vt[ 0 ].x - n[ 0 ].x, vt[ 0 ].y - n[ 0 ].y, vt[ 0 ].z, vt[ 0 ].w )
						};
					mathVec4TransformF( &vf[ 0 ], &vertices[0], &GetCurrentViewInv() );
					mathVec4TransformF( &vf[ 1 ], &vertices[1], &GetCurrentViewInv() );
					mathVec4TransformF( &vf[ 2 ], &vertices[2], &GetCurrentViewInv() );
					mathVec4TransformF( &vf[ 3 ], &vertices[3], &GetCurrentViewInv() );
				}
			}
			else // orthogonal projected geometry
			{
				// compute line delta
				D3DXVECTOR2 delta( v[ 1 ] - v[ 0 ] );

				// create normal of line delta
				D3DXVECTOR2 normal( -delta.y , delta.x );
				mathVec2NormalizeF( &normal, &normal );

				D3DXVECTOR2 n[ 2 ];
				n[ 0 ] = normal * thicknessV0 * 2.0f;
				n[ 1 ] = normal * thicknessV1 * 2.0f;

				// compute final world space vertices of thick line				
				vf[ 0 ] = D3DXVECTOR4( v[ 0 ].x + n[ 0 ].x, v[ 0 ].y + n[ 0 ].y, v[ 0 ].z, 1.0f );
				vf[ 1 ] = D3DXVECTOR4( v[ 1 ].x + n[ 1 ].x, v[ 1 ].y + n[ 1 ].y, v[ 1 ].z, 1.0f );
				vf[ 2 ] = D3DXVECTOR4( v[ 1 ].x - n[ 1 ].x, v[ 1 ].y - n[ 1 ].y, v[ 1 ].z, 1.0f );
				vf[ 3 ] = D3DXVECTOR4( v[ 0 ].x - n[ 0 ].x, v[ 0 ].y - n[ 0 ].y, v[ 0 ].z, 1.0f );
			}

			SAuxVertex* pVertices( const_cast<SAuxVertex*>( &auxVertexBuffer[ offset ] ) );
			if( false == skipLine )
			{
				// copy data to vertex buffer
				pVertices[ 0 ].xyz.Set( vf[ 0 ].x, vf[ 0 ].y, vf[ 0 ].z );
				pVertices[ 0 ].color.dcolor = col[ 0 ];
				pVertices[ 1 ].xyz.Set( vf[ 1 ].x, vf[ 1 ].y, vf[ 1 ].z );
				pVertices[ 1 ].color.dcolor = col[ 1 ];
				pVertices[ 2 ].xyz.Set( vf[ 2 ].x, vf[ 2 ].y, vf[ 2 ].z );
				pVertices[ 2 ].color.dcolor = col[ 1 ];
				pVertices[ 3 ].xyz.Set( vf[ 0 ].x, vf[ 0 ].y, vf[ 0 ].z );
				pVertices[ 3 ].color.dcolor = col[ 0 ];
				pVertices[ 4 ].xyz.Set( vf[ 2 ].x, vf[ 2 ].y, vf[ 2 ].z );
				pVertices[ 4 ].color.dcolor = col[ 1 ];
				pVertices[ 5 ].xyz.Set( vf[ 3 ].x, vf[ 3 ].y, vf[ 3 ].z );
				pVertices[ 5 ].color.dcolor = col[ 0 ];
			}
			else
			{
				// invalidate parameter data of thick line stored in vertex buffer 
				// (generates two black degenerated triangles at (0,0,0))
				memset( pVertices, 0, sizeof( SAuxVertex ) * 6 );
			}
		}
	}
}


void 
CD3DRenderAuxGeom::PrepareThickLines2D( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd )
{
	const AuxVertexBuffer& auxVertexBuffer( GetAuxVertexBuffer() );

	// process each entry
	for( AuxSortedPushBuffer::const_iterator it( itBegin ); it != itEnd; ++it )
	{
		// get current push buffer entry
		const SAuxPushBufferEntry* curPBEntry( *it );

		uint32 offset( curPBEntry->m_vertexOffs );
		for( uint32 i( 0 ); i < curPBEntry->m_numVertices / 6; ++i, offset += 6 )
		{
			// get line vertices and thickness parameter
			const D3DXVECTOR3 v[ 2 ] = 
			{
				D3DXVECTOR3( (const float*) &(auxVertexBuffer[ offset + 0 ].xyz.x) ),
					D3DXVECTOR3( (const float*) &(auxVertexBuffer[ offset + 1 ].xyz.x) )
			};
			const DWORD col[ 2 ] =
			{
				auxVertexBuffer[ offset + 0 ].color.dcolor,
					auxVertexBuffer[ offset + 1 ].color.dcolor
			};
			float thickness( auxVertexBuffer[ offset + 2 ].xyz.x );

			// get line delta and aspect ratio corrected normal
			D3DXVECTOR3 delta( v[ 1 ] - v[ 0 ] );		
			D3DXVECTOR3 normal( -delta.y * m_aspectInv , delta.x * m_aspect, 0.0f );

			// normalize and scale to line thickness
			mathVec3NormalizeF( &normal, &normal );
			normal *= thickness * 0.001f;

			// compute final 2D vertices of thick line in normalized device space
			D3DXVECTOR3 vf[ 4 ];
			vf[ 0 ] = v[ 0 ] + normal;
			vf[ 1 ] = v[ 1 ] + normal;
			vf[ 2 ] = v[ 1 ] - normal;
			vf[ 3 ] = v[ 0 ] - normal;

			// copy data to vertex buffer
			SAuxVertex* pVertices( const_cast<SAuxVertex*>( &auxVertexBuffer[ offset ] ) );
			pVertices[ 0 ].xyz.Set( vf[ 0 ].x, vf[ 0 ].y, vf[ 0 ].z );
			pVertices[ 0 ].color.dcolor = col[ 0 ];
			pVertices[ 1 ].xyz.Set( vf[ 1 ].x, vf[ 1 ].y, vf[ 1 ].z );
			pVertices[ 1 ].color.dcolor = col[ 1 ];
			pVertices[ 2 ].xyz.Set( vf[ 2 ].x, vf[ 2 ].y, vf[ 2 ].z );
			pVertices[ 2 ].color.dcolor = col[ 1 ];
			pVertices[ 3 ].xyz.Set( vf[ 0 ].x, vf[ 0 ].y, vf[ 0 ].z );
			pVertices[ 3 ].color.dcolor = col[ 0 ];
			pVertices[ 4 ].xyz.Set( vf[ 2 ].x, vf[ 2 ].y, vf[ 2 ].z );
			pVertices[ 4 ].color.dcolor = col[ 1 ];
			pVertices[ 5 ].xyz.Set( vf[ 3 ].x, vf[ 3 ].y, vf[ 3 ].z );
			pVertices[ 5 ].color.dcolor = col[ 0 ];
		}
	}
}


void
CD3DRenderAuxGeom::PrepareRendering()
{
	// update transformation matrices
	m_matrices.UpdateMatrices();

	// get current window resultion and update aspect ratios
	m_wndXRes = m_renderer.GetWidth();
	m_wndYRes = m_renderer.GetHeight();

	m_aspect = 1.0f;
	m_aspectInv = 1.0f;
	if( m_wndXRes > 0 && m_wndYRes > 0 )
	{
		m_aspect = (float) m_wndXRes / (float) m_wndYRes;
		m_aspectInv = 1.0f / m_aspect;
	}			

	// reset DrawInFront mode
	m_curDrawInFrontMode = e_DrawInFrontOff;

	// reset stream buffer manager
	m_auxGeomSBM.Reset();

	// reset current VB/IB
	m_pCurVB = 0;
	m_pCurIB = 0;

	// reset current prim type
	m_curPrimType = e_PrimTypeInvalid;
}


#if defined (DIRECT3D9) || defined(OPENGL)
void CD3DRenderAuxGeom::BindStreams( eVertexFormat newVertexFormat, IDirect3DVertexBuffer9* pNewVB, IDirect3DIndexBuffer9* pNewIB )
#elif defined (DIRECT3D10)
void CD3DRenderAuxGeom::BindStreams( eVertexFormat newVertexFormat, ID3D10Buffer* pNewVB, ID3D10Buffer* pNewIB )
#endif
{
	HRESULT hr( S_OK );
	//LPDIRECT3DDEVICE9 pd3dDevice( gcpRendD3D->m_pd3dDevice );

	// set vertex declaration
	m_renderer.EF_SetVertexDeclaration( 0, newVertexFormat );

	// bind streams
	if( m_pCurVB != pNewVB )
	{
    hr = gcpRendD3D->FX_SetVStream(0, pNewVB, 0, m_VertexSize[newVertexFormat]);
		m_pCurVB = pNewVB;
	}
	if( m_pCurIB != pNewIB )
	{
    hr = gcpRendD3D->FX_SetIStream(pNewIB);
		m_pCurIB = pNewIB;
	}		
}


void
CD3DRenderAuxGeom::SetShader( const SAuxGeomRenderFlags& renderFlags )
{
	if( 0 == m_pAuxGeomShader )
	{	
		m_pAuxGeomShader = m_renderer.m_cEF.mfForName( "AuxGeom", EF_SYSTEM );
		assert( 0 != m_pAuxGeomShader );
	}

	if( 0 != m_pAuxGeomShader )
	{
		bool dirty( 0 != ( m_renderer.m_RP.m_PersFlags & ( RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY ) ) );
		if( false != dirty )
		{
			// NOTE: dirty flags are either set when marking matrices as dirty or setting EF_ColorOp in AdjustRenderStates
			m_renderer.m_RP.m_PersFlags &= ~RBPF_FP_DIRTY | RBPF_FP_MATRIXDIRTY;
			m_renderer.m_RP.m_ObjFlags &= ~FOB_TRANS_MASK;
			m_renderer.m_RP.m_pCurObject = m_renderer.m_RP.m_TempObjects[ 0 ];
      m_renderer.m_RP.m_pCurInstanceInfo = &m_renderer.m_RP.m_pCurObject->m_II;
			m_renderer.m_RP.m_FlagsShader_LT = m_renderer.m_eCurColorOp[ 0 ] | ( m_renderer.m_eCurAlphaOp[ 0 ] << 8 ) | 
				( m_renderer.m_eCurColorArg[ 0 ] << 16 ) | ( m_renderer.m_eCurAlphaArg[ 0 ] << 24 );
		}

		EAuxGeomPublicRenderflags_DrawInFrontMode newDrawInFrontMode( renderFlags.GetDrawInFrontMode() );
		EPrimType newPrimType( GetPrimType( renderFlags ) );

		if( false != dirty || m_pAuxGeomShader != m_renderer.m_RP.m_pShader || m_curDrawInFrontMode != newDrawInFrontMode || m_curPrimType != newPrimType )
		{			
			if( e_Obj != newPrimType )
			{
        static CCryName techName("AuxGeometry");
				m_pAuxGeomShader->FXSetTechnique( techName );
				m_pAuxGeomShader->FXBegin( &m_renderer.m_RP.m_nNumRendPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES );
				m_pAuxGeomShader->FXBeginPass( 0 );

				static CCryName matViewProjName("matViewProj");
				if( e_DrawInFrontOn == renderFlags.GetDrawInFrontMode() && e_Mode3D == renderFlags.GetMode2D3DFlag() )
				{
					D3DXMATRIX matScale;
					mathMatrixScaling( &matScale, 0.999f, 0.999f, 0.999f );

					D3DXMATRIX matViewScaleProjT;
					mathMatrixMultiplyF( &matViewScaleProjT, &matScale, &GetCurrentView(), g_CpuFlags );
					mathMatrixMultiplyF( &matViewScaleProjT, &GetCurrentProj(), &matViewScaleProjT,  g_CpuFlags );
					mathMatrixTransposeF( &matViewScaleProjT, &matViewScaleProjT );

					m_pAuxGeomShader->FXSetVSFloat( matViewProjName, (Vec4*) &matViewScaleProjT._11, 4 );
					m_curDrawInFrontMode = e_DrawInFrontOn;
				}
				else
				{
					D3DXMATRIX matViewProjT;
					mathMatrixTransposeF( &matViewProjT, m_matrices.m_pCurTransMat );
					m_pAuxGeomShader->FXSetVSFloat( matViewProjName, (Vec4*) &matViewProjT._11, 4 );
					m_curDrawInFrontMode = e_DrawInFrontOff;
				}			
			}
			else
			{
        static CCryName techName("AuxGeometryObj");
				m_pAuxGeomShader->FXSetTechnique( techName );
				m_pAuxGeomShader->FXBegin( &m_renderer.m_RP.m_nNumRendPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES );
				m_pAuxGeomShader->FXBeginPass( 0 );

				if( e_DrawInFrontOn == renderFlags.GetDrawInFrontMode() && e_Mode3D == renderFlags.GetMode2D3DFlag() )
				{
					m_curDrawInFrontMode = e_DrawInFrontOn;
				}
				else
				{
					m_curDrawInFrontMode = e_DrawInFrontOff;
				}			
			}
			m_curPrimType = newPrimType;
		}
	}
	else
	{
		m_renderer.FX_SetFPMode();
	}
}


void
CD3DRenderAuxGeom::AdjustRenderStates( const SAuxGeomRenderFlags& renderFlags )
{
	HRESULT hr( S_OK );
	//LPDIRECT3DDEVICE9 pd3dDevice( gcpRendD3D->m_pd3dDevice );

	// init current render states mask
	uint32 curRenderStates( 0 ) ;

	// mode 2D/3D -- set new transformation matrix
	const D3DXMATRIX* pNewTransMat( &GetCurrentTrans3D() );
	if( e_Mode2D == renderFlags.GetMode2D3DFlag() )
	{
		pNewTransMat = &GetCurrentTrans2D();
	}
	if( m_matrices.m_pCurTransMat != pNewTransMat )
	{
		m_matrices.m_pCurTransMat = pNewTransMat;

		m_renderer.m_matView->LoadIdentity();
		m_renderer.m_matProj->LoadMatrix( (Matrix44*)pNewTransMat );		
		m_renderer.EF_DirtyMatrix();
	}	

	// set alpha blending mode
	switch( renderFlags.GetAlphaBlendMode() )
	{
	case e_AlphaAdditive:
		{				
			curRenderStates |= GS_BLSRC_ONE | GS_BLDST_ONE;
			break;
		}
	case e_AlphaBlended:
		{
			curRenderStates |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
			break;
		}
	case e_AlphaNone:
	default:
		{
			break;
		}
	}

	// set fill mode
	switch( renderFlags.GetFillMode() )
	{
	case e_FillModeWireframe:
		{
			curRenderStates |= GS_WIREFRAME;
			break;
		}
	case e_FillModeSolid:
	default:
		{							
			break;
		}
	}

	// set cull mode
	switch( renderFlags.GetCullMode() )
	{
	case e_CullModeNone:
		{
			m_renderer.SetCullMode( R_CULL_NONE );
			break;
		}
	case e_CullModeFront:
		{
			m_renderer.SetCullMode( R_CULL_FRONT );
			break;
		}
	case e_CullModeBack:
	default:
		{				
			m_renderer.SetCullMode( R_CULL_BACK );
			break;
		}
	}

	// set depth write mode
	switch( renderFlags.GetDepthWriteFlag() )
	{
	case e_DepthWriteOff:
		{
			break;
		}
	case e_DepthWriteOn:
	default:
		{
			curRenderStates |= GS_DEPTHWRITE;
			break;
		}
	}

	// set depth test mode
	switch( renderFlags.GetDepthTestFlag() )
	{
	case e_DepthTestOff:
		{
			curRenderStates |= GS_NODEPTHTEST;
			break;
		}
	case e_DepthTestOn:
	default:
		{
			break;
		}
	}

	// set point size
	uint8 newPointSize( m_curPointSize );
	if( e_PtList == GetPrimType( renderFlags ) )
	{
		newPointSize = GetPointSize( renderFlags );
	}
	else
	{
		newPointSize = 1;
	}

	if( newPointSize != m_curPointSize )
	{
		assert( newPointSize > 0 );
		float pointSize( (float) newPointSize );
#if defined (DIRECT3D9) || defined(OPENGL)
		gcpRendD3D->m_pd3dDevice->SetRenderState( D3DRS_POINTSIZE, *( (uint32*) &pointSize ) );
#elif defined (DIRECT3D10)
    assert(0);
#endif
		m_curPointSize = newPointSize;
	}			

	// apply states 
	m_renderer.EF_SetState( curRenderStates );

	// set color operations
	m_renderer.EF_SetColorOp( eCO_REPLACE, eCO_REPLACE, ( eCA_Diffuse | ( eCA_Diffuse << 3 ) ), ( eCA_Diffuse | ( eCA_Diffuse << 3 ) ) );
}


void
CD3DRenderAuxGeom::Flush( bool discardGeometry )
{
	HRESULT hr( S_OK );
#ifdef XENON
  return;
#endif

	m_renderer.m_matProj->Push();
	m_renderer.m_matView->Push();

	int backupTransMatrixIdx( m_transMatrixIdx );

	if( false == m_renderer.IsDeviceLost() )
	{
		// prepare rendering
		PrepareRendering();

		// get push buffer to process all submitted auxiliary geometries
		const AuxSortedPushBuffer& auxSortedPushBuffer( GetSortedPushBuffer() );

		// process push buffer
		for( AuxSortedPushBuffer::const_iterator it( auxSortedPushBuffer.begin() ), itEnd( auxSortedPushBuffer.end() ); it != itEnd; )
		{
			// mark current push buffer position
			AuxSortedPushBuffer::const_iterator itCur( it );

			// get current render flags
			const SAuxGeomRenderFlags& curRenderFlags( (*itCur)->m_renderFlags );			
			m_transMatrixIdx = (*itCur)->m_transMatrixIdx;

			// get prim type
			EPrimType primType( GetPrimType( curRenderFlags ) );

			// find all entries sharing the same render flags			
			while( true )
			{
				++it;
				if( ( it == itEnd ) || ( (*it)->m_renderFlags != curRenderFlags ) || ( (*it)->m_transMatrixIdx != m_transMatrixIdx ) )
				{
					break;
				}
			}

			// adjust render states based on current render flags
			AdjustRenderStates( curRenderFlags );

			// prepare thick lines
			if( e_TriList == primType && false != IsThickLine( curRenderFlags ) )
			{
				if( e_Mode3D == curRenderFlags.GetMode2D3DFlag() )
				{
					PrepareThickLines3D( itCur, it );
				}
				else
				{
					PrepareThickLines2D( itCur, it );
				}			
			}

			// set appropriate shader
			SetShader( curRenderFlags );

			// draw push buffer entries
			switch( primType )
			{
			case e_PtList:
			case e_LineList: 
			case e_TriList:
				{
					DrawAuxPrimitives( itCur, it, primType );
					break;
				}
			case e_LineListInd: 
			case e_TriListInd:
				{
					DrawAuxIndexedPrimitives( itCur, it, primType );
					break;
				}
			case e_Obj:
			default:
				{
					DrawAuxObjects( itCur, it );
					break;
				}
			}
		}
	}

	m_renderer.m_matProj->Pop();
	m_renderer.m_matView->Pop();
	m_renderer.EF_DirtyMatrix();

	m_transMatrixIdx = backupTransMatrixIdx;

	if( false != discardGeometry )
	{
		// discard geometry
		DiscardGeometry();
	}
}


void CD3DRenderAuxGeom::DiscardTransMatrices()
{
	// discard all orthogonal transformation matrices
	m_colOrthoMatrices.resize( 0 );
	m_transMatrixIdx = -1;	
}


void CD3DRenderAuxGeom::Discard()
{
  m_auxGeomSBM.Reset();

  // get pointer to vertex buffer
  SAuxVertex* pVertices( 0 );
  int toCopy = 1;
#if defined (DIRECT3D9) || defined(OPENGL)
#if !defined(XENON) && !defined(PS3)
  HRESULT hr = m_pAuxGeomVB->Lock(m_auxGeomSBM.m_curVBIndex*sizeof(SAuxVertex), toCopy*sizeof(SAuxVertex), (void**)&pVertices, D3DLOCK_DISCARD);
  m_auxGeomSBM.m_curVBIndex = toCopy;
  hr = m_pAuxGeomIB->Lock(m_auxGeomSBM.m_curIBIndex*sizeof(short), toCopy*sizeof(short), (void**)&pVertices, D3DLOCK_DISCARD);
  m_auxGeomSBM.m_curIBIndex = toCopy;
#else
  HRESULT hr = m_pAuxGeomVB->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD);
  m_auxGeomSBM.m_curVBIndex = 0;
  hr = m_pAuxGeomIB->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD);
  m_auxGeomSBM.m_curIBIndex = 0;
#endif
  hr = m_pAuxGeomVB->Unlock();
  hr = m_pAuxGeomIB->Unlock();
#elif defined (DIRECT3D10)
  assert(0);
  m_pAuxGeomVB->Unmap();
  m_pAuxGeomIB->Unmap();
#endif
  m_auxGeomSBM.m_discardVB = false;
  m_auxGeomSBM.m_discardIB = false;
}


void
CD3DRenderAuxGeom::SetOrthoMode( bool enable, D3DXMATRIX* pMatrix )
{
	if( false != enable )
	{
		assert( 0 != pMatrix );
		m_transMatrixIdx = m_colOrthoMatrices.size();
		m_colOrthoMatrices.push_back( *pMatrix );
	}
	else
	{
		m_transMatrixIdx = -1;
	}
}


int 
CD3DRenderAuxGeom::GetTransMatrixIndex()
{
	return( m_transMatrixIdx );
}


const D3DXMATRIX& 
CD3DRenderAuxGeom::GetCurrentView() const
{	
	if( false == IsOrthoMode() )
	{
		return( m_matrices.m_matView );
	}

	static D3DXMATRIX matIdent( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 );
	return( matIdent );
}


const D3DXMATRIX& 
CD3DRenderAuxGeom::GetCurrentViewInv() const
{
	if( false == IsOrthoMode() )
	{
		return( m_matrices.m_matViewInv );
	}

	static D3DXMATRIX matIdent( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 );
	return( matIdent );
}


const D3DXMATRIX& 
CD3DRenderAuxGeom::GetCurrentProj() const
{
	if( false == IsOrthoMode() )
	{
		return( m_matrices.m_matProj );
	}

	return( m_colOrthoMatrices[ m_transMatrixIdx ] );
}


const D3DXMATRIX& 
CD3DRenderAuxGeom::GetCurrentTrans3D() const
{
	if( false == IsOrthoMode() )
	{
		return( m_matrices.m_matTrans3D );
	}

	return( m_colOrthoMatrices[ m_transMatrixIdx ] );
}


const D3DXMATRIX& 
CD3DRenderAuxGeom::GetCurrentTrans2D() const
{
	return( m_matrices.m_matTrans2D );
}


bool 
CD3DRenderAuxGeom::IsOrthoMode() const
{
	return( -1 != m_transMatrixIdx );
}


void 
CD3DRenderAuxGeom::SMatrices::UpdateMatrices()
{
	gcpRendD3D->GetModelViewMatrix( &m_matView._11 );
	gcpRendD3D->GetProjectionMatrix( &m_matProj._11 );

	mathMatrixInverse( (f32*)&m_matViewInv, (f32*)&m_matView, g_CpuFlags );
	mathMatrixMultiplyF( &m_matTrans3D, &m_matProj, &m_matView, g_CpuFlags );

	m_pCurTransMat = 0;
}
