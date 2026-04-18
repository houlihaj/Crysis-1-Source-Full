#include "StdAfx.h"
#include "FogVolumeRenderNode.h"
#include "VisAreas.h"
#include "CREFogVolume.h"
#include "Cry_Geo.h"
#include "ObjMan.h"

#include <limits>


AABB CFogVolumeRenderNode::s_tracableFogVolumeArea( Vec3( 0, 0, 0 ), Vec3( 0, 0, 0 ) );
CFogVolumeRenderNode::CachedFogVolumes CFogVolumeRenderNode::s_cachedFogVolumes;
CFogVolumeRenderNode::GlobalFogVolumeMap CFogVolumeRenderNode::s_globalFogVolumeMap;
bool CFogVolumeRenderNode::s_forceTraceableAreaUpdate( false );


void CFogVolumeRenderNode::ForceTraceableAreaUpdate()
{
	s_forceTraceableAreaUpdate = true;
}


void CFogVolumeRenderNode::SetTraceableArea( const AABB& traceableArea )
{
	// do we bother?
	if( !GetCVars()->e_fog || !GetCVars()->e_fogvolumes )
		return;

	// is update of traceable areas necessary
	if( !s_forceTraceableAreaUpdate )
		if( ( s_tracableFogVolumeArea.GetCenter() - traceableArea.GetCenter() ).GetLengthSquared() < 1e-4f && ( s_tracableFogVolumeArea.GetSize() - traceableArea.GetSize() ).GetLengthSquared() < 1e-4f )
				return;

	// set new area and reset list of traceable fog volumes
	s_tracableFogVolumeArea = traceableArea;
	s_cachedFogVolumes.resize( 0 );

	// collect all candidates
	Vec3 traceableAreaCenter( s_tracableFogVolumeArea.GetCenter() );
	IVisArea* pVisAreaOfCenter( GetVisAreaManager() ? GetVisAreaManager()->GetVisAreaFromPos( traceableAreaCenter ) : NULL );

	GlobalFogVolumeMap::const_iterator itEnd( s_globalFogVolumeMap.end() );
	for( GlobalFogVolumeMap::const_iterator it( s_globalFogVolumeMap.begin() ); it != itEnd; ++it )
	{
		const CFogVolumeRenderNode* pFogVolume( *it );
		if( pVisAreaOfCenter || ( !pVisAreaOfCenter && !pFogVolume->GetEntityVisArea() ) ) // if outside only add fog volumes which are outside as well
			if( Overlap::AABB_AABB( s_tracableFogVolumeArea, pFogVolume->m_WSBBox ) ) // bb of fog volume overlaps with traceable area
				s_cachedFogVolumes.push_back( SCachedFogVolume( pFogVolume, Vec3( pFogVolume->m_pos - traceableAreaCenter ).GetLengthSquared() ) );
	}

	// sort by distance
	std::sort( s_cachedFogVolumes.begin(), s_cachedFogVolumes.end() );

	// reset force-update flags
	s_forceTraceableAreaUpdate = false;
}


inline static float lerp( float a, float b, float l )
{
	return a + l * ( b - a );
}


void CFogVolumeRenderNode::TraceFogVolumes( const Vec3& worldPos, ColorF& fogColor )
{
	FUNCTION_PROFILER_3DENGINE;

	// init default result
	fogColor = ColorF( 0, 0, 0, 0 );

	if( GetCVars()->e_fog && GetCVars()->e_fogvolumes )
	{
		// init view ray
		Vec3 camPos( s_tracableFogVolumeArea.GetCenter() );
		Ray viewRay( camPos, worldPos - camPos );

#ifdef _DEBUG
		const SCachedFogVolume* prev( 0 );
#endif

		// loop over all traceable fog volumes
		CachedFogVolumes::const_iterator itEnd( s_cachedFogVolumes.end() );
		for( CachedFogVolumes::const_iterator it( s_cachedFogVolumes.begin() ); it != itEnd; ++it )
		{
			// get current fog volume
			const CFogVolumeRenderNode* pFogVol( (*it).m_pFogVol );

			// only trace visible fog volumes
			if( !( pFogVol->GetRndFlags() & ERF_HIDDEN ) )
			{
				// check if view ray intersects with bounding box of current fog volume
				Vec3 intersection;
				if( Intersect::Ray_AABB( viewRay, pFogVol->m_WSBBox, intersection ) )
				{
					// compute contribution of current fog volume
					ColorF color;
					if( 0 == pFogVol->m_volumeType )
						color = pFogVol->GetVolumetricFogColorEllipsoid( worldPos );
					else
						color = pFogVol->GetVolumetricFogColorBox( worldPos );

					color.a = 1.0f - color.a;		// 0 = transparent, 1 = opaque

					// blend fog colors
					fogColor.r = lerp( fogColor.r, color.r, color.a );
					fogColor.g = lerp( fogColor.g, color.g, color.a );
					fogColor.b = lerp( fogColor.b, color.b, color.a );
					fogColor.a = lerp( fogColor.a, 1.0, color.a );
				}
			}

#ifdef _DEBUG
			if( prev )
			{
				assert( prev->m_distToCenterSq >= (*it).m_distToCenterSq );
				prev = &(*it);
			}
#endif

		}

		if( fogColor.a > 0.01f )
		{
			fogColor.r /= fogColor.a;
			fogColor.g /= fogColor.a;
			fogColor.b /= fogColor.a;
		}
		else
		{
			fogColor.r = 0;
			fogColor.g = 0;
			fogColor.b = 0;
		}
	}

	fogColor.a = 1.0f - fogColor.a;
}


void CFogVolumeRenderNode::RegisterFogVolume( const CFogVolumeRenderNode* pFogVolume )
{
	GlobalFogVolumeMap::const_iterator it( s_globalFogVolumeMap.find( pFogVolume ) );
	assert( it == s_globalFogVolumeMap.end() && 
		"CFogVolumeRenderNode::RegisterFogVolume() -- Fog volume already registered!" );
	if( it == s_globalFogVolumeMap.end() )
	{
		s_globalFogVolumeMap.insert( pFogVolume );
		ForceTraceableAreaUpdate();
	}
}


void CFogVolumeRenderNode::UnregisterFogVolume( const CFogVolumeRenderNode* pFogVolume )
{
	GlobalFogVolumeMap::iterator it( s_globalFogVolumeMap.find( pFogVolume ) );
	assert( it != s_globalFogVolumeMap.end() && 
		"CFogVolumeRenderNode::UnRegisterFogVolume() -- Fog volume previously not registered!" );
	if( it != s_globalFogVolumeMap.end() )
	{
		s_globalFogVolumeMap.erase( it );
		ForceTraceableAreaUpdate();
	}
}


CFogVolumeRenderNode::CFogVolumeRenderNode()
:	m_matNodeWS()
, m_matWS()
, m_matWSInv()
, m_volumeType( 0 )
, m_pos( 0, 0, 0 )
, m_x( 1, 0, 0 )
, m_y( 0, 1, 0 )
, m_z( 0, 0, 1 )
, m_scale( 1, 1, 1 )
, m_globalDensity( 1 )
, m_fHDRDynamic( 0 )
, m_softEdges( 1 )
, m_color( 1, 1, 1, 1 )
, m_useGlobalFogColor( false )
, m_heightFallOffDir( 0, 0, 1 )
, m_heightFallOffDirScaled( 0, 0, 1 )
, m_heightFallOffShift( 0, 0, 0 )
, m_heightFallOffBasePoint( 0, 0, 0 )
, m_localBounds( Vec3( -1, -1, -1 ), Vec3( 1, 1, 1 ) )
, m_globalDensityFader()
, m_pMatFogVolEllipsoid( 0 )
, m_pMatFogVolBox( 0 )
, m_pFogVolumeRenderElement( 0 )
, m_pRenderObject( 0 )
, m_WSBBox()
, m_cachedSoftEdgesLerp(1, 0)
, m_cachedFogColor(1, 1, 1, 1)
{
	m_matNodeWS.SetIdentity();
	
	m_matWS.SetIdentity();
	m_matWSInv.SetIdentity();

	m_pFogVolumeRenderElement = (CREFogVolume*) GetRenderer()->EF_CreateRE( eDATA_FogVolume );
	m_pRenderObject = GetRenderer()->EF_GetObject( false );
	
	m_pMatFogVolEllipsoid = GetMatMan()->LoadMaterial( "Materials/Fog/FogVolumeEllipsoid", false );
	m_pMatFogVolBox = GetMatMan()->LoadMaterial( "Materials/Fog/FogVolumeBox", false );

	//Get3DEngine()->RegisterEntity( this );
	RegisterFogVolume( this );
}


CFogVolumeRenderNode::~CFogVolumeRenderNode()
{
	if( m_pFogVolumeRenderElement )
		m_pFogVolumeRenderElement->Release();
	if( m_pRenderObject )
		m_pRenderObject->RemovePermanent();

	UnregisterFogVolume( this );
	Get3DEngine()->UnRegisterEntity( this );
  Get3DEngine()->FreeRenderNodeState(this);
}


void CFogVolumeRenderNode::UpdateFogVolumeMatrices()
{
	// update matrices used for ray tracing, distance sorting, etc.
	m_matWS = Matrix34::CreateFromVectors( m_scale.x * m_x, m_scale.y * m_y, m_scale.z * m_z, m_pos );
	m_matWSInv = m_matWS.GetInverted();
}


void CFogVolumeRenderNode::UpdateWorldSpaceBBox()
{
	// update bounding box in world space used for culling
	m_WSBBox.SetTransformedAABB( m_matNodeWS, m_localBounds );
}


void CFogVolumeRenderNode::UpdateHeightFallOffBasePoint()
{
	m_heightFallOffBasePoint = m_pos + m_heightFallOffShift; 
}


void CFogVolumeRenderNode::SetFogVolumeProperties( const SFogVolumeProperties& properties )
{
	m_globalDensityFader.SetInvalid();

	assert( properties.m_size.x > 0 && properties.m_size.z > 0 && properties.m_size.z > 0 );
	Vec3 scale( properties.m_size * 0.5f );
	if( ( m_scale - scale ).GetLengthSquared() > 1e-4 )
	{
		m_scale = properties.m_size * 0.5f;
		m_localBounds.min = Vec3( -1, -1, -1 ).CompMul( m_scale );
		m_localBounds.max = -m_localBounds.min;
		UpdateWorldSpaceBBox();
	}

	m_volumeType = properties.m_volumeType;
	assert( m_volumeType >= 0 && m_volumeType <= 1 );
	m_color = properties.m_color;
	assert( properties.m_globalDensity >= 0 );
	m_useGlobalFogColor = properties.m_useGlobalFogColor;
	m_globalDensity = properties.m_globalDensity;
	m_fHDRDynamic = properties.m_fHDRDynamic;
	assert( properties.m_softEdges >= 0 && properties.m_softEdges <= 1 );
	m_softEdges = properties.m_softEdges;

	float latiArc( DEG2RAD( 90.0f - properties.m_heightFallOffDirLati ) );
	float longArc( DEG2RAD( properties.m_heightFallOffDirLong ) );
	float sinLati( sinf( latiArc ) ); float cosLati( cosf( latiArc ) );
	float sinLong( sinf( longArc ) ); float cosLong( cosf( longArc ) );
	m_heightFallOffDir = Vec3( sinLati * cosLong, sinLati * sinLong, cosLati );
	m_heightFallOffShift = m_heightFallOffDir * properties.m_heightFallOffShift;
	m_heightFallOffDirScaled = m_heightFallOffDir * properties.m_heightFallOffScale;

	UpdateHeightFallOffBasePoint();
}


const Matrix34& CFogVolumeRenderNode::GetMatrix() const
{
	return m_matNodeWS;
}


void CFogVolumeRenderNode::GetLocalBounds( AABB& bbox )
{ 
	bbox = m_localBounds; 
};


void CFogVolumeRenderNode::SetMatrix( const Matrix34& mat )
{
	//UnregisterFogVolume( this );
	Get3DEngine()->UnRegisterEntity( this );

	m_matNodeWS = mat;
	
	// get translation and rotational part of fog volume from entity matrix
	// scale is specified explicitly as fog volumes can be non-uniformly scaled
	m_pos = m_matNodeWS.GetTranslation();
	m_x = m_matNodeWS.GetColumn( 0 );
	m_y = m_matNodeWS.GetColumn( 1 );
	m_z = m_matNodeWS.GetColumn( 2 );

	UpdateFogVolumeMatrices();
	UpdateWorldSpaceBBox();
	UpdateHeightFallOffBasePoint();

	Get3DEngine()->RegisterEntity( this );
	//RegisterFogVolume( this );
	ForceTraceableAreaUpdate();
}

float CFogVolumeRenderNode::GetDistanceToCameraSquared() const
{
	Vec3 boundingBox[ 8 ] =
	{
		m_matWS * Vec3( -1.0, -1.0, -1.0 ), 
		m_matWS * Vec3( -1.0,  1.0, -1.0 ), 
		m_matWS * Vec3(  1.0,  1.0, -1.0 ), 
		m_matWS * Vec3(  1.0, -1.0, -1.0 ), 
		m_matWS * Vec3( -1.0, -1.0,  1.0 ), 
		m_matWS * Vec3( -1.0,  1.0,  1.0 ), 
		m_matWS * Vec3(  1.0,  1.0,  1.0 ), 
		m_matWS * Vec3(  1.0, -1.0,  1.0 )
	};

	const CCamera& cam( Get3DEngine()->GetCamera() );	
	const Plane* pNearPlane( cam.GetFrustumPlane( FR_PLANE_NEAR ) );
	const Vec3 camPos( cam.GetPosition() );

	f32 distSq( 0.0f );
	for( int i( 0 ); i < 8; ++i )
	{
		float tmpDistSq( ( boundingBox[ i ] - camPos ).GetLengthSquared() );
		if( tmpDistSq > distSq && pNearPlane->DistFromPlane( boundingBox[ i ] ) < 0.0f )
			distSq = tmpDistSq;
	}

	return distSq;
}


void CFogVolumeRenderNode::FadeGlobalDensity(float fadeTime, float newGlobalDensity)
{
	if (fadeTime > 0 && newGlobalDensity >= 0)
	{
		float curFrameTime(gEnv->pTimer->GetCurrTime());
		m_globalDensityFader.Set(curFrameTime, curFrameTime + fadeTime, m_globalDensity, newGlobalDensity);
	}
}


EERType CFogVolumeRenderNode::GetRenderNodeType() 
{ 
	return eERType_FogVolume; 
}


const char* CFogVolumeRenderNode::GetEntityClassName() const
{ 
	return "FogVolume";
}


const char* CFogVolumeRenderNode::GetName() const 
{ 
	return "FogVolume";
}


Vec3 CFogVolumeRenderNode::GetPos( bool bWorldOnly ) const 
{ 
	return m_pos;
}


ColorF CFogVolumeRenderNode::GetFogColor() const
{
	//FUNCTION_PROFILER_3DENGINE
	Vec3 fogColor(m_useGlobalFogColor ? Get3DEngine()->GetFogColor() : Vec3(m_color.r, m_color.g, m_color.b));
	if (GetRenderer()->EF_Query(EFQ_HDRModeEnabled))
		fogColor *= powf(Get3DEngine()->GetHDRDynamicMultiplier(), m_fHDRDynamic);	
	return fogColor;
}


Vec2 CFogVolumeRenderNode::GetSoftEdgeLerp(const Vec3& viewerPosOS) const
{
	//FUNCTION_PROFILER_3DENGINE
	// ramp down soft edge factor as soon as camera enters the ellipsoid	
	float softEdge(m_softEdges * clamp_tpl((viewerPosOS.GetLength() - 0.95f) * 20.0f, 0.0f, 1.0f)); 
	return Vec2(softEdge, 1.0f - softEdge);
}


inline static float expf_s(float arg)
{
	assert(fabs(arg) < 80.0f);
	return expf(clamp_tpl(arg, -80.0f, 80.0f));
}


ColorF CFogVolumeRenderNode::GetVolumetricFogColorEllipsoid( const Vec3& worldPos ) const
{
	const CCamera& cam( GetCamera() );
	Vec3 camPos( cam.GetPosition() );
	Vec3 camDir( cam.GetViewdir() );

	Vec3 cameraLookDir(worldPos - camPos);
	if (cameraLookDir.GetLengthSquared() > 1e-4f)
	{
		// setup ray tracing in OS
		Vec3 cameraPosInOSx2( m_matWSInv.TransformPoint( camPos ) * 2.0f );
		Vec3 cameraLookDirInOS( m_matWSInv.TransformVector( cameraLookDir ) );

		float tI( sqrtf( cameraLookDirInOS.Dot( cameraLookDirInOS ) ) );
		float invOfScaledCamDirLength( 1.0f / tI );
		cameraLookDirInOS *= invOfScaledCamDirLength;

		// calc coefficients for ellipsoid parametrization (just a simple unit-sphere in its own space)
		float B( cameraPosInOSx2.Dot( cameraLookDirInOS ) );
		float Bsq( B * B );
		float C( cameraPosInOSx2.Dot( cameraPosInOSx2 ) - 4.0f );

		// solve quadratic equation
		float discr( Bsq - C );
		if( discr >= 0.0 ) 
		{
			float discrSqrt = sqrtf( discr );

			// ray hit
			Vec3 cameraPosInWS( camPos );
			Vec3 cameraLookDirInWS( ( worldPos - camPos ) * invOfScaledCamDirLength );

			//////////////////////////////////////////////////////////////////////////

			float tS( max( 0.5f * ( -B - discrSqrt ), 0.0f ) ); // clamp to zero so front ray-ellipsoid intersection is NOT behind camera
			float tE( max( 0.5f * ( -B + discrSqrt ), 0.0f ) ); // clamp to zero so back ray-ellipsoid intersection is NOT behind camera
			//float tI( ( worldPos - camPos ).Dot( camDir ) / cameraLookDirInWS.Dot( camDir ) );
			tI = max( tS, min( tI, tE ) ); // clamp to range [tS, tE]

			Vec3 front( tS * cameraLookDirInWS + cameraPosInWS );
			Vec3 dist( ( tI - tS ) * cameraLookDirInWS );
			float distLength( dist.GetLength() );
			float fogInt( distLength * expf_s( -( front - m_heightFallOffBasePoint).Dot( m_heightFallOffDirScaled ) ) );

			//////////////////////////////////////////////////////////////////////////

			float heightDiff( dist.Dot( m_heightFallOffDirScaled ) );
			if( fabsf( heightDiff ) > 0.001f )
				fogInt *= ( 1.0f - expf_s( -heightDiff ) ) / heightDiff;

			float softArg(clamp_tpl(discr * m_cachedSoftEdgesLerp.x + m_cachedSoftEdgesLerp.y, 0.0f, 1.0f));
			fogInt *= softArg * ( 2.0f - softArg );

			float fog( expf_s( -m_globalDensity * fogInt ) );

			return ColorF(m_cachedFogColor.r, m_cachedFogColor.g, m_cachedFogColor.b, min(fog, 1.0f));
		}
		else
			return ColorF( 1, 1, 1, 1 );
	}
	else
		return ColorF( 1, 1, 1, 1 );
}


ColorF CFogVolumeRenderNode::GetVolumetricFogColorBox( const Vec3& worldPos ) const
{
	const CCamera& cam( GetCamera() );
	Vec3 camPos( cam.GetPosition() );
	Vec3 camDir( cam.GetViewdir() );
	
	Vec3 cameraLookDir(worldPos - camPos);
	if (cameraLookDir.GetLengthSquared() > 1e-4f)
	{
		// setup ray tracing in OS
		Vec3 cameraPosInOS( m_matWSInv.TransformPoint( camPos ) );
		Vec3 cameraLookDirInOS( m_matWSInv.TransformVector( cameraLookDir ) );

		float tI( sqrtf( cameraLookDirInOS.Dot( cameraLookDirInOS ) ) );
		float invOfScaledCamDirLength( 1.0f / tI );
		cameraLookDirInOS *= invOfScaledCamDirLength;

		float tS(0), tE(std::numeric_limits<float>::max());

		if (fabsf(cameraLookDirInOS.x) > 0.0f)
		{
			float invCameraDirInOSx(1.0f / cameraLookDirInOS.x);

			float tPosPlane((1 - cameraPosInOS.x) * invCameraDirInOSx);
			float tNegPlane((-1 - cameraPosInOS.x) * invCameraDirInOSx);

			float tFrontFace(cameraLookDirInOS.x > 0 ? tNegPlane : tPosPlane);
			float tBackFace(cameraLookDirInOS.x > 0 ? tPosPlane : tNegPlane);

			tS = max(tS, tFrontFace);
			tE = min(tE, tBackFace);
		}

		if (fabsf(cameraLookDirInOS.y) > 0.0f)
		{
			float invCameraDirInOSy(1.0f / cameraLookDirInOS.y);

			float tPosPlane((1 - cameraPosInOS.y) * invCameraDirInOSy);
			float tNegPlane((-1 - cameraPosInOS.y) * invCameraDirInOSy);

			float tFrontFace(cameraLookDirInOS.y > 0 ? tNegPlane : tPosPlane);
			float tBackFace(cameraLookDirInOS.y > 0 ? tPosPlane : tNegPlane);

			tS = max(tS, tFrontFace);
			tE = min(tE, tBackFace);
		}

		if (fabsf(cameraLookDirInOS.z) > 0.0f)
		{
			float invCameraDirInOSz(1.0f / cameraLookDirInOS.z);

			float tPosPlane((1 - cameraPosInOS.z) * invCameraDirInOSz);
			float tNegPlane((-1 - cameraPosInOS.z) * invCameraDirInOSz);

			float tFrontFace(cameraLookDirInOS.z > 0 ? tNegPlane : tPosPlane);
			float tBackFace(cameraLookDirInOS.z > 0 ? tPosPlane : tNegPlane);

			tS = max(tS, tFrontFace);
			tE = min(tE, tBackFace);
		}

		tE = max(tE, 0.0f);

#if 0
		Vec3 dummy(0, 0, 0);
		bool intesects(Intersect::Ray_AABB(Ray(cameraPosInOS, cameraLookDirInOS), AABB(Vec3(-1), Vec3(1)), dummy) != 0);
		assert(tS <= tE && intesects);
		Vec3 pS(cameraPosInOS + cameraLookDirInOS * tS); // pS.xyz should be in range [-1, 1]
		Vec3 pE(cameraPosInOS + cameraLookDirInOS * tE); // pE.xyz should be in range [-1, 1]
#else
		if (tS > tE)
			return ColorF(1, 1, 1, 1);
#endif

		Vec3 cameraPosInWS( camPos );
		Vec3 cameraLookDirInWS( ( worldPos - camPos ) * invOfScaledCamDirLength );

		//////////////////////////////////////////////////////////////////////////

		//float tI( ( worldPos - camPos ).Dot( camDir ) / cameraLookDirInWS.Dot( camDir ) );
		tI = max( tS, min( tI, tE ) ); // clamp to range [tS, tE]

		Vec3 front( tS * cameraLookDirInWS + cameraPosInWS );
		Vec3 dist( ( tI - tS ) * cameraLookDirInWS );
		float distLength( dist.GetLength() );
		float fogInt( distLength * expf_s( -( front - m_heightFallOffBasePoint).Dot( m_heightFallOffDirScaled ) ) );

		//////////////////////////////////////////////////////////////////////////

		float heightDiff( dist.Dot( m_heightFallOffDirScaled ) );
		if( fabsf( heightDiff ) > 0.001f )
			fogInt *= ( 1.0f - expf_s( -heightDiff ) ) / heightDiff;

		float fog( expf_s( -m_globalDensity * fogInt ) );

		return ColorF(m_cachedFogColor.r, m_cachedFogColor.g, m_cachedFogColor.b, min(fog, 1.0f));
	}
	else
		return ColorF( 1, 1, 1, 1 );
}


bool CFogVolumeRenderNode::IsViewerInsideVolume() const
{
	const CCamera& cam( GetCamera() );

	// check if fog volumes bounding box intersects the near clipping plane
	const Plane* pNearPlane( cam.GetFrustumPlane( FR_PLANE_NEAR ) );
	Vec3 pntOnNearPlane( cam.GetPosition() - pNearPlane->DistFromPlane( cam.GetPosition() ) * pNearPlane->n );
	Vec3 pntOnNearPlaneOS( m_matWSInv.TransformPoint( pntOnNearPlane ) );

	Vec3 nearPlaneOS_n( m_matWSInv.TransformVector( pNearPlane->n )/*.GetNormalized()*/ );
	f32 nearPlaneOS_d( -nearPlaneOS_n.Dot( pntOnNearPlaneOS ) );

	// get extreme lengths
	float t( fabsf( nearPlaneOS_n.x ) + fabsf( nearPlaneOS_n.y ) + fabsf( nearPlaneOS_n.z ) );
	//float t( 0.0f );
	//if( nearPlaneOS_n.x >= 0 ) t += -nearPlaneOS_n.x; else t += nearPlaneOS_n.x;
	//if( nearPlaneOS_n.y >= 0 ) t += -nearPlaneOS_n.y; else t += nearPlaneOS_n.y;
	//if( nearPlaneOS_n.z >= 0 ) t += -nearPlaneOS_n.z; else t += nearPlaneOS_n.z;

	float t0 = t + nearPlaneOS_d;
	float t1 = -t + nearPlaneOS_d;
	
	return t0 * t1 < 0.0f;
}


bool CFogVolumeRenderNode::Render( const SRendParams& rParam )
{
	// anything to render?
  if( Cry3DEngineBase::m_nRenderStackLevel != 0)
    return false;

	if( !m_pRenderObject || !m_pFogVolumeRenderElement || !m_pMatFogVolBox || !m_pMatFogVolEllipsoid || GetCVars()->e_fog == 0 || GetCVars()->e_fogvolumes == 0 )
		return false;

	if (m_globalDensityFader.IsValid())
	{
		float curFrameTime(gEnv->pTimer->GetCurrTime());
		m_globalDensity = m_globalDensityFader.GetValue(curFrameTime);
		if (!m_globalDensityFader.IsTimeInRange(curFrameTime))
			m_globalDensityFader.SetInvalid();			
	}

	// set basic render object properties
	m_pRenderObject->m_II.m_Matrix = m_matNodeWS;
	m_pRenderObject->m_ObjFlags |= FOB_TRANS_MASK;
	m_pRenderObject->m_fSort = 0;

  int nAfterWater = GetObjManager()->IsAfterWater(m_pos, GetCamera().GetPosition(), Get3DEngine()->GetWaterLevel()) ? 1 : 0;

	// TODO: add constant factor to sortID to make fog volumes render before all other alpha transparent geometry (or have separate render list?)
	m_pRenderObject->m_fSort = WATER_LEVEL_SORTID_OFFSET * 0.5f;

	//m_pRenderObject->m_TempVars[0] = m_fScale;
	//m_pRenderObject->m_AmbColor = rParams.AmbientColor;
	//m_pRenderObject->m_Color.r = rParams.vColor.x;
	//m_pRenderObject->m_Color.g = rParams.vColor.y;
	//m_pRenderObject->m_Color.b = rParams.vColor.z;
	//m_pRenderObject->m_Color.a = rParams.fAlpha;

	// transform camera into fog volumes object space (where fog volume is a unit-sphere at (0,0,0))
	const CCamera& cam( GetCamera() );	
	Vec3 viewerPosWS( cam.GetPosition() );
	Vec3 viewerPosOS( m_matWSInv * viewerPosWS );

	m_cachedFogColor = GetFogColor();
	m_cachedSoftEdgesLerp = GetSoftEdgeLerp(viewerPosOS);

	// set render element attributes
	m_pFogVolumeRenderElement->m_pFogVolume = this;
	m_pFogVolumeRenderElement->m_center = m_pos;	
	m_pFogVolumeRenderElement->m_viewerInsideVolume = IsViewerInsideVolume();
	m_pFogVolumeRenderElement->m_localAABB = m_localBounds;
	m_pFogVolumeRenderElement->m_matWSInv = m_matWSInv;
	m_pFogVolumeRenderElement->m_fogColor = m_cachedFogColor;	
	m_pFogVolumeRenderElement->m_globalDensity = m_globalDensity;	
	m_pFogVolumeRenderElement->m_softEdgesLerp = m_cachedSoftEdgesLerp;
	m_pFogVolumeRenderElement->m_heightFallOffDirScaled = m_heightFallOffDirScaled;
	m_pFogVolumeRenderElement->m_heightFallOffBasePoint = m_heightFallOffBasePoint;	
	m_pFogVolumeRenderElement->m_eyePosInWS = viewerPosWS;
	m_pFogVolumeRenderElement->m_eyePosInOS = viewerPosOS;

	// get shader item
	SShaderItem shaderItem( 0 != rParam.pMaterial ? rParam.pMaterial->GetShaderItem( 0 ) : 
		1 == m_volumeType ? m_pMatFogVolBox->GetShaderItem( 0 ) : m_pMatFogVolEllipsoid->GetShaderItem( 0 ) );

	// add to renderer
	CRenderObject *pObj = GetRenderer()->EF_GetObject( false, m_pRenderObject->m_Id );
  if (!pObj)
    return false;
  GetRenderer()->EF_AddEf(m_pFogVolumeRenderElement, shaderItem, m_pRenderObject, EFSLIST_TRANSP, nAfterWater);
	
	return true;
}


IPhysicalEntity* CFogVolumeRenderNode::GetPhysics() const 
{ 
	return 0;
}


void CFogVolumeRenderNode::SetPhysics( IPhysicalEntity* ) 
{
}


void CFogVolumeRenderNode::SetMaterial( IMaterial* pMat ) 
{ 
	//m_pMaterial = pMat;
}


IMaterial* CFogVolumeRenderNode::GetMaterial( Vec3* pHitPos ) 
{ 
	//return( m_pMaterial );
	return 0;
}


float CFogVolumeRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min, GetBBox().GetRadius() * GetCVars()->e_view_dist_ratio_detail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_view_dist_min, GetBBox().GetRadius() * GetCVars()->e_view_dist_ratio * GetViewDistRatioNormilized());
}


void CFogVolumeRenderNode::GetMemoryUsage( ICrySizer* pSizer )
{
	SIZER_COMPONENT_NAME( pSizer, "FogVolumeNode" );
	pSizer->AddObject( this, sizeof( *this ) );
}