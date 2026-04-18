#include "StdAfx.h"
#include "WaterVolumeRenderNode.h"
#include "VisAreas.h"
#include <Cry_Geo.h>


//////////////////////////////////////////////////////////////////////////
// private triangulation code

namespace
{
	template< typename T > 
	size_t PosOffset();

	template<>
	size_t PosOffset<Vec3>()
	{
		return 0;
	}

	template<>
	size_t PosOffset<struct_VERTEX_FORMAT_P3F_TEX2F>()
	{
		return (size_t) &((struct_VERTEX_FORMAT_P3F_TEX2F*) 0)->xyz;
	}


	template< typename T >
	struct VertexAccess
	{
		VertexAccess( const T* pVertices, size_t numVertices )
		: m_pVertices( pVertices )
		, m_numVertices( numVertices )
		{
		}

		const Vec3& operator[] ( size_t idx ) const
		{
			assert( idx < m_numVertices );
			const T* pVertex = &m_pVertices[ idx ];
			return *(Vec3*) ((size_t) pVertex + PosOffset<T>());
		}

		const size_t GetNumVertices() const
		{
			return m_numVertices;
		}

	private:
		const T* m_pVertices;
		size_t m_numVertices;
	};


	template< typename T >
	float Area( const VertexAccess<T>& contour )
	{
		int n = contour.GetNumVertices();
		float area = 0.0f;

		for( int p = n-1, q = 0; q < n; p = q++ ) 
			area += contour[p].x * contour[q].y - contour[q].x * contour[p].y;

		return area * 0.5f;
	}


	bool InsideTriangle( float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py )
	{
		float ax = Cx - Bx;  float ay = Cy - By;
		float bx = Ax - Cx;  float by = Ay - Cy;
		float cx = Bx - Ax;  float cy = By - Ay;
		float apx= Px - Ax;  float apy= Py - Ay;
		float bpx= Px - Bx;  float bpy= Py - By;
		float cpx= Px - Cx;  float cpy= Py - Cy;

		float aCROSSbp = ax*bpy - ay*bpx;
		float cCROSSap = cx*apy - cy*apx;
		float bCROSScp = bx*cpy - by*cpx;

		return (aCROSSbp >= 0.0f) && (bCROSScp >= 0.0f) && (cCROSSap >= 0.0f);
	};


	template< typename T, typename S >
	bool Snip( const VertexAccess<T>& contour, int u, int v, int w, int n, const S* V )
	{
		float Ax = contour[V[u]].x;
		float Ay = contour[V[u]].y;

		float Bx = contour[V[v]].x;
		float By = contour[V[v]].y;

		float Cx = contour[V[w]].x;
		float Cy = contour[V[w]].y;

		if( 1e-9 > (((Bx-Ax)*(Cy-Ay)) - ((By-Ay)*(Cx-Ax))) ) 
			return false;

		for( int p = 0; p < n; p++ )
		{
			if( (p == u) || (p == v) || (p == w) ) 
				continue;

			float Px = contour[V[p]].x;
			float Py = contour[V[p]].y;

			if( InsideTriangle( Ax, Ay, Bx, By, Cx, Cy, Px, Py ) ) 
				return false;
		}

		return true;
	}


	template< typename T, typename S >
	bool Triangulate( const VertexAccess<T>& contour, std::vector<S>& result )
	{
		// reset result
		result.resize( 0 );

		// allocate and initialize list of vertices in polygon
		int n = contour.GetNumVertices();
		if ( n < 3 ) 
			return false;

		S* V = (S*) alloca( n * sizeof( S ) );

		// we want a counter-clockwise polygon in V
		if( 0.0f < Area( contour ) )
			for( int v=0; v<n; v++ ) 
				V[v] = v;
		else
			for( int v=0; v<n; v++ ) 
				V[v] = (n-1)-v;

		int nv = n;

		//  remove nv-2 vertices, creating 1 triangle every time
		int count = 2 * nv;   // error detection

		for( int m=0, v=nv-1; nv>2; )
		{
			// if we loop, it is probably a non-simple polygon
			if( 0 >= (count--) )			
				return false; // ERROR - probably bad polygon!

			// three consecutive vertices in current polygon, <u,v,w>
			int u = v; if( nv <= u ) u = 0;   // previous
			v = u+1; if( nv <= v ) v = 0;     // new v
			int w = v+1; if( nv <= w ) w = 0; // next

			if( Snip( contour, u, v, w, nv, V ) )
			{
				// true names of the vertices
				S a = V[u]; 
				S b = V[v]; 
				S c = V[w];

				// output triangle
				result.push_back( a );
				result.push_back( b );
				result.push_back( c );

				m++;

				// remove v from remaining polygon
				for( int s=v, t=v+1; t<nv; s++, t++ ) 
					V[s] = V[t]; 
				
				nv--;

				// reset error detection counter
				count = 2 * nv;
			}
		}

		return true;
	}
}


//////////////////////////////////////////////////////////////////////////
// helpers

inline static Vec3 MapVertexToFogPlane( const Vec3& v, const Plane& p )
{
	const Vec3 projDir( 0, 0, 1 );
	float perpdist = p | v;
	float cosine = p.n | projDir;
	assert( fabs( cosine ) > 1e-4 );
	float pd_c = -perpdist / cosine;
	return v + projDir * pd_c;
}


//////////////////////////////////////////////////////////////////////////
// CWaterVolumeRenderNode implementation

CWaterVolumeRenderNode::CWaterVolumeRenderNode()
: m_volumeID( ~0 )
, m_volumeType( IWaterVolumeRenderNode::eWVT_Unknown )
, m_volumeDepth( 0 )
, m_streamSpeed( 0 )
, m_wvParams()
, m_pMaterial( 0 )
, m_pWaterBodyIntoMat( 0 )
, m_pWaterBodyOutofMat( 0 )
, m_pVolumeRE( 0 )
, m_pSurfaceRE( 0 )
, m_pSerParams( 0 )
, m_pPhysAreaInput( 0 )
, m_pPhysArea( 0 )
, m_waterSurfaceVertices()
, m_waterSurfaceIndices()
{	
	m_pWaterBodyIntoMat = GetMatMan()->LoadMaterial( "Materials/Fog/WaterVolumeInto", false );
	m_pWaterBodyOutofMat = GetMatMan()->LoadMaterial( "Materials/Fog/WaterVolumeOutof", false );
	m_pVolumeRE = static_cast<CREWaterVolume*>( GetRenderer()->EF_CreateRE( eDATA_WaterVolume ) );
	if( m_pVolumeRE )
	{
		m_pVolumeRE->m_drawWaterSurface = false;
		m_pVolumeRE->m_pParams = &m_wvParams;
	}
	m_pSurfaceRE = static_cast<CREWaterVolume*>( GetRenderer()->EF_CreateRE( eDATA_WaterVolume ) );
	if( m_pSurfaceRE )
	{
		m_pSurfaceRE->m_drawWaterSurface = true;
		m_pSurfaceRE->m_pParams = &m_wvParams;
	}
}


CWaterVolumeRenderNode::~CWaterVolumeRenderNode()
{
	Dephysicalize();

	SAFE_RELEASE( m_pVolumeRE );
	SAFE_RELEASE( m_pSurfaceRE );
	SAFE_DELETE( m_pSerParams );
	SAFE_DELETE( m_pPhysAreaInput );

	Get3DEngine()->UnRegisterEntity( this );

  Get3DEngine()->FreeRenderNodeState(this);
}


void CWaterVolumeRenderNode::SetFogDensity( float fogDensity )
{
	m_wvParams.m_fogDensity = fogDensity;
}


float CWaterVolumeRenderNode::GetFogDensity() const
{
	return m_wvParams.m_fogDensity;
}


void CWaterVolumeRenderNode::SetFogColor( const Vec3& fogColor )
{
	m_wvParams.m_fogColor = fogColor;
}


void CWaterVolumeRenderNode::CreateOcean( uint64 volumeID, /* TBD */ bool keepSerializationParams )
{
}


void CWaterVolumeRenderNode::CreateArea( uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams )
{
	assert( fabs( fogPlane.n.GetLengthSquared() - 1.0 ) < 1e-4 && "CWaterVolumeRenderNode::CreateArea(...) -- Fog plane normal doesn't have unit length!" );
	assert( fogPlane.n.Dot( Vec3( 0, 0, 1 ) ) > 1e-4f && "CWaterVolumeRenderNode::CreateArea(...) -- Invalid fog plane specified!" );
	if( fogPlane.n.Dot( Vec3( 0, 0, 1 ) ) <= 1e-4f )
		return;

	assert( numVertices >= 3 );
	if( numVertices < 3 )
		return;

	m_volumeID = volumeID;
	m_wvParams.m_fogPlane = fogPlane;
	m_volumeType = IWaterVolumeRenderNode::eWVT_Area;

	// copy volatile creation params to be able to serialize water volume if needed (only in editor)
	if( keepSerializationParams )
		CopyVolatileAreaSerParams( pVertices, numVertices, surfUVScale );

	// remove form 3d engine
	Get3DEngine()->UnRegisterEntity( this );

	// generate vertices
	m_waterSurfaceVertices.resize( numVertices );
	for( size_t i( 0 ); i < numVertices; ++i )
	{
		// project input vertex onto fog plane
		m_waterSurfaceVertices[i].xyz = MapVertexToFogPlane( pVertices[i], fogPlane );

		// generate texture coordinates
		m_waterSurfaceVertices[i].st[0] = surfUVScale.x * ( pVertices[i].x - pVertices[0].x );
		m_waterSurfaceVertices[i].st[1] = surfUVScale.y * ( pVertices[i].y - pVertices[0].y );
	}

	// generate indices
	Triangulate( VertexAccess<struct_VERTEX_FORMAT_P3F_TEX2F>( &m_waterSurfaceVertices[0], m_waterSurfaceVertices.size() ), m_waterSurfaceIndices );

	// update bounding info
	UpdateBoundingBox();

	// update reference to vertex and index buffer
	m_wvParams.m_pVertices = &m_waterSurfaceVertices[0];
	m_wvParams.m_numVertices = m_waterSurfaceVertices.size();
	m_wvParams.m_pIndices = &m_waterSurfaceIndices[0];
	m_wvParams.m_numIndices = m_waterSurfaceIndices.size();

	// add to 3d engine
	Get3DEngine()->RegisterEntity( this );
}


void CWaterVolumeRenderNode::CreateRiver( uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams )
{
	assert( fabs( fogPlane.n.GetLengthSquared() - 1.0 ) < 1e-4 && "CWaterVolumeRenderNode::CreateRiver(...) -- Fog plane normal doesn't have unit length!" );
	assert( fogPlane.n.Dot( Vec3( 0, 0, 1 ) ) > 1e-4f && "CWaterVolumeRenderNode::CreateRiver(...) -- Invalid fog plane specified!" );
	if( fogPlane.n.Dot( Vec3( 0, 0, 1 ) ) <= 1e-4f )
		return;

	assert( numVertices == 4 );
	if( numVertices != 4 || !_finite( pVertices[0].x ) || !_finite( pVertices[1].x ) || !_finite( pVertices[2].x ) || !_finite( pVertices[3].x ) )
		return;

	m_volumeID = volumeID;
	m_wvParams.m_fogPlane = fogPlane;
	m_volumeType = IWaterVolumeRenderNode::eWVT_River;

	// copy volatile creation params to be able to serialize water volume if needed (only in editor)
	if( keepSerializationParams )
		CopyVolatileRiverSerParams( pVertices, numVertices, uTexCoordBegin, uTexCoordEnd, surfUVScale );

	// remove form 3d engine
	Get3DEngine()->UnRegisterEntity( this );

	// texgen planes
	Plane planes[4];
	planes[0].SetPlane( pVertices[0], pVertices[1], pVertices[1] + Vec3( 0, 0,  1 ) );
	planes[1].SetPlane( pVertices[2], pVertices[3], pVertices[3] + Vec3( 0, 0, -1 ) );
	planes[2].SetPlane( pVertices[0], pVertices[2], pVertices[2] + Vec3( 0, 0, -1 ) );
	planes[3].SetPlane( pVertices[1], pVertices[3], pVertices[3] + Vec3( 0, 0,  1 ) );

	// generate vertices
	m_waterSurfaceVertices.resize( 5 );
	for( int i( 0 ); i < 4; ++i )
		m_waterSurfaceVertices[i].xyz = pVertices[i];
	m_waterSurfaceVertices[4].xyz = 0.25f * ( pVertices[0] + pVertices[1] + pVertices[2] + pVertices[3] );

	for( int i( 0 ); i < m_waterSurfaceVertices.size(); ++i )
	{
		// map input vertex onto fog plane
		m_waterSurfaceVertices[i].xyz =	MapVertexToFogPlane( m_waterSurfaceVertices[i].xyz, fogPlane );

		// generate texture coordinates
		float d0( planes[0].DistFromPlane( m_waterSurfaceVertices[i].xyz ) );
		float d1( planes[1].DistFromPlane( m_waterSurfaceVertices[i].xyz ) );
		float d2( planes[2].DistFromPlane( m_waterSurfaceVertices[i].xyz ) );
		float d3( planes[3].DistFromPlane( m_waterSurfaceVertices[i].xyz ) );
		float t( d0 / ( d0 + d1 ) );

		m_waterSurfaceVertices[i].st[0] = ( 1 - t ) * fabsf( uTexCoordBegin ) + t * fabsf( uTexCoordEnd );
		m_waterSurfaceVertices[i].st[1] = d2 / ( d2 + d3 );

		m_waterSurfaceVertices[i].st[0] *= surfUVScale.x;
		m_waterSurfaceVertices[i].st[1] *= surfUVScale.y;
	}

	// generate indices
	m_waterSurfaceIndices.resize( 12 );
	m_waterSurfaceIndices[ 0] = 0;
	m_waterSurfaceIndices[ 1] = 1;
	m_waterSurfaceIndices[ 2] = 4;

	m_waterSurfaceIndices[ 3] = 1;
	m_waterSurfaceIndices[ 4] = 3;
	m_waterSurfaceIndices[ 5] = 4;

	m_waterSurfaceIndices[ 6] = 3;
	m_waterSurfaceIndices[ 7] = 2;
	m_waterSurfaceIndices[ 8] = 4;

	m_waterSurfaceIndices[ 9] = 0;
	m_waterSurfaceIndices[10] = 4;
	m_waterSurfaceIndices[11] = 2;

	// update bounding info
	UpdateBoundingBox();

	// update reference to vertex and index buffer
	m_wvParams.m_pVertices = &m_waterSurfaceVertices[0];
	m_wvParams.m_numVertices = m_waterSurfaceVertices.size();
	m_wvParams.m_pIndices = &m_waterSurfaceIndices[0];
	m_wvParams.m_numIndices = m_waterSurfaceIndices.size();

	// add to 3d engine
	Get3DEngine()->RegisterEntity( this );
}


void CWaterVolumeRenderNode::SetAreaPhysicalArea( const Vec3* pVertices, unsigned int numVertices, float volumeDepth, float streamSpeed, bool keepSerializationParams )
{
	assert( pVertices && numVertices > 3 && m_volumeType == IWaterVolumeRenderNode::eWVT_Area );
	if( !pVertices || numVertices <= 3 || m_volumeType != IWaterVolumeRenderNode::eWVT_Area )
		return;

	if( !m_pPhysAreaInput )
		m_pPhysAreaInput = new SPhysAreaInput;

	// set new properties
	m_volumeDepth = volumeDepth;
	m_streamSpeed = streamSpeed;

	const Plane& fogPlane( m_wvParams.m_fogPlane );

	// generate contour vertices
	m_pPhysAreaInput->m_contour.resize( numVertices );

	// map input vertices onto fog plane
	if( Area( VertexAccess<Vec3>( pVertices, numVertices ) ) > 0.0f )
	{
		for( unsigned int i( 0 ); i < numVertices; ++i )
			m_pPhysAreaInput->m_contour[i] = MapVertexToFogPlane( pVertices[i], fogPlane ); // flip vertex order as physics expects them CCW
	}
	else
	{
		for( unsigned int i( 0 ); i < numVertices; ++i )
			m_pPhysAreaInput->m_contour[i] = MapVertexToFogPlane( pVertices[numVertices-1 - i], fogPlane );
	}

	// triangulate contour
	Triangulate( VertexAccess<Vec3>( &m_pPhysAreaInput->m_contour[0], m_pPhysAreaInput->m_contour.size() ), m_pPhysAreaInput->m_indices );

	// reset flow
	m_pPhysAreaInput->m_flowContour.resize( 0 );

	if( keepSerializationParams )
		CopyVolatilePhysicsAreaContourSerParams( pVertices, numVertices );
}


void CWaterVolumeRenderNode::SetRiverPhysicsArea( const Vec3* pVertices, unsigned int numVertices, float volumeDepth, float streamSpeed, bool keepSerializationParams )
{
	assert( pVertices && numVertices > 3 && !(numVertices & 1) && m_volumeType == IWaterVolumeRenderNode::eWVT_River );
	if( !pVertices || numVertices <= 3 || (numVertices & 1) || m_volumeType != IWaterVolumeRenderNode::eWVT_River )
		return;

	if( !m_pPhysAreaInput )
		m_pPhysAreaInput = new SPhysAreaInput;

	// set new properties
	m_volumeDepth = volumeDepth;
	m_streamSpeed = streamSpeed;
		
	const Plane& fogPlane( m_wvParams.m_fogPlane );

	// generate contour vertices
	m_pPhysAreaInput->m_contour.resize( numVertices );

	// map input vertices onto fog plane
	if( Area( VertexAccess<Vec3>( pVertices, numVertices ) ) > 0.0f )
	{
		for( unsigned int i( 0 ); i < numVertices; ++i )
			m_pPhysAreaInput->m_contour[i] = MapVertexToFogPlane( pVertices[i], fogPlane ); // flip vertex order as physics expects them CCW
	}
	else
	{
		for( unsigned int i( 0 ); i < numVertices; ++i )
			m_pPhysAreaInput->m_contour[i] = MapVertexToFogPlane( pVertices[numVertices-1 - i], fogPlane );
	}

	// generate flow along contour		
	unsigned int h( numVertices / 2 );
	unsigned int h2( numVertices );		
	m_pPhysAreaInput->m_flowContour.resize( numVertices );
	for( unsigned int i( 0 ); i < h; ++i )
	{
		if( !i )
			m_pPhysAreaInput->m_flowContour[i] = ( m_pPhysAreaInput->m_contour[i+1] - m_pPhysAreaInput->m_contour[i] ).GetNormalizedSafe() * m_streamSpeed;
		else if( i == h - 1 )
			m_pPhysAreaInput->m_flowContour[i] = ( m_pPhysAreaInput->m_contour[i] - m_pPhysAreaInput->m_contour[i-1] ).GetNormalizedSafe() * m_streamSpeed;
		else
			m_pPhysAreaInput->m_flowContour[i] = ( m_pPhysAreaInput->m_contour[i+1] - m_pPhysAreaInput->m_contour[i-1] ).GetNormalizedSafe() * m_streamSpeed;
	}

	for( unsigned int i( 0 ); i < h; ++i )
	{
		if( !i )
			m_pPhysAreaInput->m_flowContour[h2-1 - i] = ( m_pPhysAreaInput->m_contour[h2-1 - i-1] - m_pPhysAreaInput->m_contour[h2-1 - i] ).GetNormalizedSafe() * m_streamSpeed;
		else if( i == h - 1 )
			m_pPhysAreaInput->m_flowContour[h2-1 - i] = ( m_pPhysAreaInput->m_contour[h2-1 - i] - m_pPhysAreaInput->m_contour[h2-1 - i+1] ).GetNormalizedSafe() * m_streamSpeed;
		else
			m_pPhysAreaInput->m_flowContour[h2-1 - i] = ( m_pPhysAreaInput->m_contour[h2-1 - i-1] - m_pPhysAreaInput->m_contour[h2-1 - i+1] ).GetNormalizedSafe() * m_streamSpeed;
	}

	// triangulate contour
	m_pPhysAreaInput->m_indices.resize( 3 * 2 * ( numVertices / 2 - 1 ) );		
	for( unsigned int i( 0 ); i < h - 1; ++i )
	{
		m_pPhysAreaInput->m_indices[6*i+0] = i;
		m_pPhysAreaInput->m_indices[6*i+1] = i+1;
		m_pPhysAreaInput->m_indices[6*i+2] = h2-1 - i-1;

		m_pPhysAreaInput->m_indices[6*i+3] = h2-1 - i-1;
		m_pPhysAreaInput->m_indices[6*i+4] = h2-1 - i;
		m_pPhysAreaInput->m_indices[6*i+5] = i;
	}

	if( keepSerializationParams )
		CopyVolatilePhysicsAreaContourSerParams( pVertices, numVertices );
}


EERType CWaterVolumeRenderNode::GetRenderNodeType() 
{ 
	return eERType_WaterVolume; 
}


const char* CWaterVolumeRenderNode::GetEntityClassName() const
{ 
	return "WaterVolume";
}


const char* CWaterVolumeRenderNode::GetName() const		
{ 
	return "WaterVolume";
}


Vec3 CWaterVolumeRenderNode::GetPos( bool bWorldOnly ) const 
{ 
	return m_wvParams.m_center;
}


bool CWaterVolumeRenderNode::Render( const SRendParams& rParam )
{
  // hack: special case for when inside amphibious vehicle
  if( Get3DEngine()->GetOceanRenderFlags()&OCR_NO_DRAW )
    return false;

	// anything to render?
	if( m_nRenderStackLevel>0 || !m_pMaterial || !m_pWaterBodyIntoMat || !m_pWaterBodyOutofMat || !m_pVolumeRE || !m_pSurfaceRE || GetCVars()->e_water_volumes == 0 ||
			m_waterSurfaceVertices.empty() || m_waterSurfaceIndices.empty() )
		return false;

	IRenderer* pRenderer( GetRenderer() );

	// get render objects
	CRenderObject* pROVol( pRenderer->EF_GetObject( true ) );
	CRenderObject* pROSurf( pRenderer->EF_GetObject( true ) );
	if( !pROVol || !pROSurf )
		return false;

	float distToWaterVolumeSurface( GetCameraDistToWaterVolumeSurface() );
	bool aboveWaterVolumeSurface( distToWaterVolumeSurface > 0.0f );
	bool insideWaterVolumeSurface2D( IsCameraInsideWaterVolumeSurface2D() );
	bool insideWaterVolume( insideWaterVolumeSurface2D && !aboveWaterVolumeSurface );

	// fill parameters to render elements 
	m_wvParams.m_viewerInsideVolume = insideWaterVolume;
	m_wvParams.m_viewerCloseToWaterPlane = /*insideWaterVolumeSurface2D && */fabsf( distToWaterVolumeSurface ) < 0.5f;

	// submit volume 
  if( GetCVars()->e_fog )
  {
	  if( insideWaterVolume || aboveWaterVolumeSurface )
	  {
		  // fill in data for render object
		  pROVol->m_II.m_Matrix.SetIdentity();
		  pROVol->m_fSort = 0;

		  // get shader item
		  SShaderItem shaderItem( m_wvParams.m_viewerInsideVolume ? m_pWaterBodyOutofMat->GetShaderItem( 0 ) : m_pWaterBodyIntoMat->GetShaderItem( 0 ) );

		  // add to renderer
		  GetRenderer()->EF_AddEf( m_pVolumeRE, shaderItem, pROVol, EFSLIST_WATER_VOLUMES, aboveWaterVolumeSurface ? 0 : 1 );
	  }
  }

	// submit surface
	{
		// fill in data for render object
		pROSurf->m_II.m_Matrix.SetIdentity();
		pROSurf->m_fSort = 0;

		// get shader item
		SShaderItem shaderItem( m_pMaterial->GetShaderItem( 0 ) );

		// add to renderer
		GetRenderer()->EF_AddEf( m_pSurfaceRE, shaderItem, pROSurf, EFSLIST_WATER, 1 ); 
	}

	return true;
} 


void CWaterVolumeRenderNode::SetMaterial( IMaterial* pMat ) 
{ 
	m_pMaterial = pMat;	
}


IMaterial* CWaterVolumeRenderNode::GetMaterial( Vec3* pHitPos ) 
{ 
	return m_pMaterial;
}


float CWaterVolumeRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio_detail*GetViewDistRatioNormilized());

	return max( GetCVars()->e_view_dist_min,GetBBox().GetRadius()*GetCVars()->e_view_dist_ratio * GetViewDistRatioNormilized() );
}


void CWaterVolumeRenderNode::GetMemoryUsage( ICrySizer* pSizer )
{
	SIZER_COMPONENT_NAME( pSizer, "WaterVolumeNode" );	
	size_t dynSize( sizeof( *this ) );	
	if( m_pSerParams )
		dynSize += m_pSerParams->GetMemoryUsage();	
	if( m_pPhysAreaInput )
		dynSize += m_pPhysAreaInput->GetMemoryUsage();	
	dynSize += m_waterSurfaceVertices.capacity() * sizeof( WaterSurfaceVertices::value_type );
	dynSize += m_waterSurfaceIndices.capacity() * sizeof( WaterSurfaceIndices::value_type );	
	pSizer->AddObject( this, dynSize );
}


void CWaterVolumeRenderNode::Precache()
{
}


IPhysicalEntity* CWaterVolumeRenderNode::GetPhysics() const 
{ 
	return m_pPhysArea;
}


void CWaterVolumeRenderNode::SetPhysics( IPhysicalEntity* pPhysArea ) 
{
	m_pPhysArea = pPhysArea;
}


void CWaterVolumeRenderNode::CheckPhysicalized()
{
	if( !GetPhysics() )
		Physicalize();
}


void CWaterVolumeRenderNode::Physicalize( bool bInstant )
{
	Dephysicalize();

	if( !m_pPhysAreaInput )
		return;

	Vec3* pFlow( !m_pPhysAreaInput->m_flowContour.empty() ? &m_pPhysAreaInput->m_flowContour[0] : 0 );
	//assert( m_pPhysAreaInput->m_contour.size() >= 3 && ( !pFlow || m_pPhysAreaInput->m_contour.size() == m_pPhysAreaInput->m_flowContour.size() ) && m_pPhysAreaInput->m_indices.size() >= 3 && m_pPhysAreaInput->m_indices.size() % 3 == 0 );
	if( m_pPhysAreaInput->m_contour.size() < 3 || ( pFlow && m_pPhysAreaInput->m_contour.size() != m_pPhysAreaInput->m_flowContour.size() ) || m_pPhysAreaInput->m_indices.size() < 3  || m_pPhysAreaInput->m_indices.size() % 3 != 0 )
		return;

	// setup physical area
	m_pPhysArea = GetPhysicalWorld()->AddArea( &m_pPhysAreaInput->m_contour[0], m_pPhysAreaInput->m_contour.size(), min( 0.0f, -m_volumeDepth ), 10.0f, 
		Vec3(ZERO), Quat(IDENTITY), 1.0f, Vec3(ZERO), &m_pPhysAreaInput->m_indices[0], m_pPhysAreaInput->m_indices.size() / 3, pFlow );
	if( m_pPhysArea )
	{
		pe_status_pos sp;
		m_pPhysArea->GetStatus( &sp );		
		
		pe_params_buoyancy pb;
		pb.waterPlane.n = sp.q * Vec3( 0, 0, 1 );
		pb.waterPlane.origin = m_pPhysAreaInput->m_contour[0];
		//pb.waterFlow = sp.q * Vec3( m_streamSpeed, 0, 0 );
		m_pPhysArea->SetParams( &pb );

		pe_params_foreign_data pfd;
		pfd.pForeignData = this;
		pfd.iForeignData = PHYS_FOREIGN_ID_WATERVOLUME;
		pfd.iForeignFlags = 0;
		m_pPhysArea->SetParams(&pfd);
	}
}


void CWaterVolumeRenderNode::Dephysicalize(bool bKeepIfReferenced)
{
	if( m_pPhysArea )
	{
		GetPhysicalWorld()->RemoveArea( m_pPhysArea );
		m_pPhysArea = 0;
	}
}


float CWaterVolumeRenderNode::GetCameraDistToWaterVolumeSurface() const
{
	const CCamera& cam( GetCamera() );	
	Vec3 camPos( cam.GetPosition() );
	return m_wvParams.m_fogPlane.DistFromPlane( camPos );
}


bool CWaterVolumeRenderNode::IsCameraInsideWaterVolumeSurface2D() const
{
	const CCamera& cam( GetCamera() );	
	Vec3 camPos( cam.GetPosition() );

	VertexAccess<struct_VERTEX_FORMAT_P3F_TEX2F> ca( &m_waterSurfaceVertices[0], m_waterSurfaceVertices.size() );
	for( size_t i( 0 ); i < m_waterSurfaceIndices.size(); i += 3 )
	{
		const Vec3& v0( ca[ m_waterSurfaceIndices[i] ] );
		const Vec3& v1( ca[ m_waterSurfaceIndices[i+1] ] );
		const Vec3& v2( ca[ m_waterSurfaceIndices[i+2] ] );

		if( InsideTriangle( v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, camPos.x, camPos.y ) )
			return true;
	}

	return false;
}


void CWaterVolumeRenderNode::UpdateBoundingBox()
{
	m_WSBBox.Reset();
	for( size_t i( 0 ); i < m_waterSurfaceVertices.size(); ++i )
		m_WSBBox.Add( m_waterSurfaceVertices[i].xyz );

	if(IVisArea * pArea = Get3DEngine()->GetVisAreaFromPos(m_WSBBox.GetCenter()))
	{
		if( m_WSBBox.min.z > pArea->GetAABBox()->min.z )
			m_WSBBox.min.z = pArea->GetAABBox()->min.z;
		return;
	}

	int unitSize( GetTerrain()->GetHeightMapUnitSize() );

	int minX( ( (int) m_WSBBox.min.x / unitSize ) * unitSize  );
	int minY( ( (int) m_WSBBox.min.y / unitSize ) * unitSize  );
	int maxX( ( (int) m_WSBBox.max.x / unitSize ) * unitSize + unitSize );
	int maxY( ( (int) m_WSBBox.max.y / unitSize ) * unitSize + unitSize );
	
	//////////////////////////////////////////////////////////////////////////

	//float minZ( GetTerrain()->GetZApr( minX, minY ) );
	//for( int x( minX ); x <= maxX; x += unitSize )
	//{
	//	for( int y( minY ); y <= maxY; y += unitSize )
	//	{
	//		float z( GetTerrain()->GetZApr( x, y ) );
	//		minZ = z < minZ ? z : minZ;
	//	}
	//}

	//////////////////////////////////////////////////////////////////////////

	minX = clamp_tpl( minX, 0, CTerrain::GetTerrainSize() - unitSize );
	minY = clamp_tpl( minY, 0, CTerrain::GetTerrainSize() - unitSize );
	maxX = clamp_tpl( maxX, 0, CTerrain::GetTerrainSize() - unitSize );
	maxY = clamp_tpl( maxY, 0, CTerrain::GetTerrainSize() - unitSize );

	float minZ( GetTerrain()->GetZ( minX, minY ) );
	for( int x( minX ); x <= maxX; x += unitSize )
	{
		for( int y( minY ); y <= maxY; y += unitSize )
		{
			float z( GetTerrain()->GetZ( x, y ) );
			minZ = z < minZ ? z : minZ;
		}
	}

	//////////////////////////////////////////////////////////////////////////

	minZ -= 0.5f;
	if( m_WSBBox.min.z > (float) minZ )
		m_WSBBox.min.z = minZ;

	m_wvParams.m_center = m_WSBBox.GetCenter();
}


const SWaterVolumeSerialize* CWaterVolumeRenderNode::GetSerializationParams()
{ 
	if( !m_pSerParams )
		return 0;

	// before returning, copy non-volatile serialization params 
	m_pSerParams->m_volumeType = m_volumeType;
	m_pSerParams->m_volumeID = m_volumeID;

	m_pSerParams->m_pMaterial = m_pMaterial;

	m_pSerParams->m_fogDensity = m_wvParams.m_fogDensity;
	m_pSerParams->m_fogColor = m_wvParams.m_fogColor;
	m_pSerParams->m_fogPlane = m_wvParams.m_fogPlane;

	m_pSerParams->m_volumeDepth = m_volumeDepth;
	m_pSerParams->m_streamSpeed = m_streamSpeed;

	return m_pSerParams;
}


void CWaterVolumeRenderNode::CopyVolatilePhysicsAreaContourSerParams( const Vec3* pVertices, unsigned int numVertices )
{
	if( !m_pSerParams )
		m_pSerParams = new SWaterVolumeSerialize;

	m_pSerParams->m_physicsAreaContour.resize( numVertices );
	for( unsigned int i( 0 ); i < numVertices; ++i )
		m_pSerParams->m_physicsAreaContour[i] = pVertices[i];	
}


void CWaterVolumeRenderNode::CopyVolatileRiverSerParams( const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale )
{
	if( !m_pSerParams )
		m_pSerParams = new SWaterVolumeSerialize;

	m_pSerParams->m_uTexCoordBegin = uTexCoordBegin;
	m_pSerParams->m_uTexCoordEnd = uTexCoordEnd;

	m_pSerParams->m_surfUScale = surfUVScale.x;
	m_pSerParams->m_surfVScale = surfUVScale.y;

	m_pSerParams->m_vertices.resize( numVertices );
	for( int i( 0 ); i < numVertices; ++i )
		m_pSerParams->m_vertices[i] = pVertices[i];
}


void CWaterVolumeRenderNode::CopyVolatileAreaSerParams( const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale )
{
	if( !m_pSerParams )
		m_pSerParams = new SWaterVolumeSerialize;

	m_pSerParams->m_uTexCoordBegin = 1.0f;
	m_pSerParams->m_uTexCoordEnd = 1.0f;

	m_pSerParams->m_surfUScale = surfUVScale.x;
	m_pSerParams->m_surfVScale = surfUVScale.y;

	m_pSerParams->m_vertices.resize( numVertices );
	for( int i( 0 ); i < numVertices; ++i )
		m_pSerParams->m_vertices[i] = pVertices[i];
}
