#include "StdAfx.h"
#include "DecalRenderNode.h"
#include "VisAreas.h"
#include "ObjMan.h"
#include "terrain.h"


CDecalRenderNode::CDecalRenderNode()
: m_pos( 0, 0, 0 )
, m_localBounds( Vec3( -1, -1, -1 ), Vec3( 1, 1, 1 ) )
, m_pMaterial( NULL )
, m_updateRequested( false )
, m_decalProperties()
, m_decals()
{	
}


CDecalRenderNode::~CDecalRenderNode()
{
	Get3DEngine()->UnRegisterEntity( this );
	DeleteDecals();	
  Get3DEngine()->FreeRenderNodeState(this);
}


const SDecalProperties* CDecalRenderNode::GetDecalProperties() const
{
	return &m_decalProperties;
}

void CDecalRenderNode::DeleteDecals()
{
	for( size_t i( 0 ); i < m_decals.size(); ++i )
		delete m_decals[ i ];

	m_decals.resize( 0 );
}


void CDecalRenderNode::CreatePlanarDecal( IMaterial* pMaterial )
{
	CryEngineDecalInfo decalInfo;

	// necessary params
	decalInfo.vPos = m_decalProperties.m_pos;
	decalInfo.vNormal = m_decalProperties.m_normal;
	decalInfo.fSize = m_decalProperties.m_radius;
	decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
	strcpy( decalInfo.szMaterialName, pMaterial->GetName() );
	decalInfo.sortPrio = m_decalProperties.m_sortPrio;

	// default for all other
	decalInfo.pIStatObj = NULL;
	decalInfo.ownerInfo.pRenderNode = NULL;
	decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
	decalInfo.szTextureName[0] = '\0';
	decalInfo.vHitDirection = Vec3( 0, 0, 0 );
	decalInfo.fGrowTime = 0;
	decalInfo.bAdjustPos = false;
	decalInfo.preventDecalOnGround = true;
	decalInfo.fAngle = 0;

	CDecal* pDecal( new CDecal );
	if( m_p3DEngine->CreateDecalOnStatObj( decalInfo, pDecal ) )
		m_decals.push_back( pDecal );
	else
		delete pDecal;
}


void CDecalRenderNode::CreateDecalOnStaticObjects( IMaterial* pMaterial )
{
	CTerrain* pTerrain( GetTerrain() );
	CVisAreaManager* pVisAreaManager( GetVisAreaManager() );
	PodArray< SRNInfo > decalReceivers;

	if( pTerrain && m_pOcNode && !m_pOcNode->m_pVisArea )
		pTerrain->GetObjectsAround( m_decalProperties.m_pos, m_decalProperties.m_radius, &decalReceivers, true );
	else if( pVisAreaManager && m_pOcNode && m_pOcNode->m_pVisArea)
		pVisAreaManager->GetObjectsAround( m_decalProperties.m_pos, m_decalProperties.m_radius, &decalReceivers, true);

  // delete vegetations
  for( size_t i( 0 ); i < decalReceivers.Count(); ++i )
  {
    if( decalReceivers[i].pNode->GetRenderNodeType() == eERType_Vegetation )
    {
      decalReceivers.DeleteFastUnsorted(i);
      i--;
    }
  }

	for( size_t i( 0 ); i < decalReceivers.Count(); ++i )
	{
		CryEngineDecalInfo decalInfo;

		// necessary params
		decalInfo.vPos = m_decalProperties.m_pos;
		decalInfo.vNormal = m_decalProperties.m_normal;
		decalInfo.vHitDirection = -m_decalProperties.m_normal;
		decalInfo.fSize = m_decalProperties.m_radius;
		decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
		strcpy( decalInfo.szMaterialName, pMaterial->GetName() );
		decalInfo.sortPrio = m_decalProperties.m_sortPrio;

		if(GetCVars()->e_decals_merge)
			decalInfo.ownerInfo.pDecalReceivers = &decalReceivers;
		else
			decalInfo.ownerInfo.pRenderNode = decalReceivers[ i ].pNode;

    decalInfo.ownerInfo.nRenderNodeSlotId = 0;
    decalInfo.ownerInfo.nRenderNodeSlotSubObjectId = -1;

		// default for all other
		decalInfo.pIStatObj = NULL;
		decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
		decalInfo.szTextureName[0] = '\0';
		decalInfo.fGrowTime = 0;
		decalInfo.bAdjustPos = false;
		decalInfo.preventDecalOnGround = false;
		decalInfo.fAngle = 0;


		CDecal* pDecal( new CDecal );
    if( m_p3DEngine->CreateDecalOnStatObj( decalInfo, pDecal ) )
    {
			m_decals.push_back( pDecal );
			assert(!GetCVars()->e_decals_merge || pDecal->m_eDecalType == eDecalType_WS_Merged);
		}
		else
			delete pDecal;

		if(GetCVars()->e_decals_merge)
			break;
	}
}


void CDecalRenderNode::CreateDecalOnTerrain( IMaterial* pMaterial )
{
	float terrainHeight( GetTerrain()->GetZApr( m_decalProperties.m_pos.x, m_decalProperties.m_pos.y ) );
	float terrainDelta( m_decalProperties.m_pos.z - terrainHeight );
	if( terrainDelta < m_decalProperties.m_radius && terrainDelta > -0.5f )
	{
		CryEngineDecalInfo decalInfo;

		// necessary params
		decalInfo.vPos = Vec3( m_decalProperties.m_pos.x, m_decalProperties.m_pos.y, terrainHeight );
		decalInfo.vNormal = Vec3( 0, 0, 1 );
		decalInfo.vHitDirection = Vec3( 0, 0, -1 );
		decalInfo.fSize = m_decalProperties.m_radius - terrainDelta;
		decalInfo.pExplicitRightUpFront = &m_decalProperties.m_explicitRightUpFront;
		strcpy( decalInfo.szMaterialName, pMaterial->GetName() );
		decalInfo.sortPrio = m_decalProperties.m_sortPrio;

		// default for all other
		decalInfo.pIStatObj = NULL;
		decalInfo.ownerInfo.pRenderNode = 0;
		decalInfo.fLifeTime = 1.0f; // default life time for rendering, decal won't grow older as we don't update it
		decalInfo.szTextureName[0] = '\0';
		decalInfo.fGrowTime = 0;
		decalInfo.bAdjustPos = false;
		decalInfo.preventDecalOnGround = false;
		decalInfo.fAngle = 0;

		CDecal* pDecal( new CDecal );
		if( m_p3DEngine->CreateDecalOnStatObj( decalInfo, pDecal ) )
			m_decals.push_back( pDecal );
		else
			delete pDecal;
	}
}


void CDecalRenderNode::CreateDecals()
{	
	DeleteDecals();	

	_smart_ptr<IMaterial> pMaterial( GetMaterial() );

	assert( 0 != pMaterial && "CDecalRenderNode::CreateDecals() -- Invalid Material!" );
	if( !pMaterial )
		return;

	switch( m_decalProperties.m_projectionType )
	{
	case SDecalProperties::ePlanar:
		{
			CreatePlanarDecal( pMaterial );
			break;
		}
	case SDecalProperties::eProjectOnStaticObjects:
		{
			CreateDecalOnStaticObjects( pMaterial );
			break;
		}
	case SDecalProperties::eProjectOnTerrain:
		{
			CreateDecalOnTerrain( pMaterial );
			break;
		}
	case SDecalProperties::eProjectOnTerrainAndStaticObjects:
		{
			CreateDecalOnStaticObjects( pMaterial );
			CreateDecalOnTerrain( pMaterial );
			break;
		}
	default:
		{
			assert( !"CDecalRenderNode::CreateDecals() : Unsupported decal projection type!" );
			break;
		}		
	}
}


void CDecalRenderNode::ProcessUpdateRequest()
{
	if( !m_updateRequested || m_nRenderStackLevel>0 )
		return;

	CreateDecals();
	m_updateRequested = false;
}

void CDecalRenderNode::UpdateAABBFromRenderMeshes()
{
  if( m_decalProperties.m_projectionType == SDecalProperties::eProjectOnStaticObjects ||
    m_decalProperties.m_projectionType == SDecalProperties::eProjectOnTerrain ||
    m_decalProperties.m_projectionType == SDecalProperties::eProjectOnTerrainAndStaticObjects )
  {
    AABB WSBBox; 
    WSBBox.Reset();
    for( size_t i( 0 ); i < m_decals.size(); ++i )
    {
      CDecal* pDecal( m_decals[ i ] );
      if( pDecal && pDecal->m_pRenderMesh )
      {
        AABB aabb;
        pDecal->m_pRenderMesh->GetBBox(aabb.min,aabb.max);
        WSBBox.Add(aabb);
      }
    }

    if(!WSBBox.IsReset())
      m_WSBBox = WSBBox;
  }
}

void CDecalRenderNode::SetDecalProperties( const SDecalProperties& properties )
{
	// update bounds
	m_localBounds = AABB( -properties.m_radius * Vec3( 1, 1, 1 ), properties.m_radius * Vec3( 1, 1, 1 ) );

	// register material
	m_pMaterial = GetMatMan()->LoadMaterial( properties.m_pMaterialName, false );

	// copy decal properties 
	m_decalProperties = properties;
	m_decalProperties.m_pMaterialName = 0; // reset this as it's assumed to be a temporary pointer only, refer to m_materialID to get material	

	// request update
	m_updateRequested = true;
}

void CDecalRenderNode::SetMatrix( const Matrix34& mat )
{
	Get3DEngine()->UnRegisterEntity( this );

	m_pos = mat.GetTranslation();

  if( m_decalProperties.m_projectionType == SDecalProperties::ePlanar )
    m_WSBBox.SetTransformedAABB( mat, AABB( -Vec3( 1, 1, 0.1f ), Vec3( 1, 1, 0.1f ) ) );
  else
	  m_WSBBox.SetTransformedAABB( mat, AABB( -Vec3( 1, 1, 1 ), Vec3( 1, 1, 1 ) ) );

	Get3DEngine()->RegisterEntity( this );
}


EERType CDecalRenderNode::GetRenderNodeType() 
{ 
	return eERType_Decal; 
}


const char* CDecalRenderNode::GetEntityClassName() const
{ 
	return "Decal";
}


const char* CDecalRenderNode::GetName() const 
{ 
	return "Decal";
}


Vec3 CDecalRenderNode::GetPos( bool bWorldOnly ) const 
{ 
	return m_pos;
}


bool CDecalRenderNode::Render( const SRendParams& rParam )
{
	if( !GetCVars()->e_decals )
		return false;

  bool bUpdateAABB = m_updateRequested;

	ProcessUpdateRequest();

	float waterLevel( m_p3DEngine->GetWaterLevel() );
	for( size_t i( 0 ); i < m_decals.size(); ++i )
	{
		CDecal* pDecal( m_decals[ i ] );
		if( pDecal && 0 != pDecal->m_pMaterial )
		{
			pDecal->m_vAmbient.x = rParam.AmbientColor.r;
			pDecal->m_vAmbient.y = rParam.AmbientColor.g;
			pDecal->m_vAmbient.z = rParam.AmbientColor.b;
      bool bAfterWater = CObjManager::IsAfterWater(pDecal->m_vWSPos, GetCamera().GetPosition(), waterLevel);
      float fDistFading = SATURATE((1.f - rParam.fDistance/m_fWSMaxViewDist)*DIST_FADING_FACTOR);			
      pDecal->Render( 0, bAfterWater, rParam.nDLightMask, fDistFading );
		}
	}

  // terrain decal meshes are created only during rendering so only after that bbox can be computed
  if(bUpdateAABB) 
    UpdateAABBFromRenderMeshes();

	return true;
}


IPhysicalEntity* CDecalRenderNode::GetPhysics() const 
{ 
	return 0;
}


void CDecalRenderNode::SetPhysics( IPhysicalEntity* ) 
{
}


void CDecalRenderNode::SetMaterial( IMaterial* pMat ) 
{ 
	for( size_t i( 0 ); i < m_decals.size(); ++i )
	{
		CDecal* pDecal( m_decals[ i ] );
		if( pDecal )
			pDecal->m_pMaterial = pMat;
	}
	
	m_pMaterial = pMat;
}


IMaterial* CDecalRenderNode::GetMaterial( Vec3 * pHitPos ) 
{ 
	return m_pMaterial;
}


float CDecalRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_view_dist_min, m_decalProperties.m_radius * GetCVars()->e_view_dist_ratio_detail*GetViewDistRatioNormilized());

	return( max( GetCVars()->e_view_dist_min, m_decalProperties.m_radius * GetCVars()->e_view_dist_ratio * GetViewDistRatioNormilized() ) );
}


void CDecalRenderNode::Precache()
{
	ProcessUpdateRequest();
}


void CDecalRenderNode::GetMemoryUsage( ICrySizer* pSizer )
{
	SIZER_COMPONENT_NAME( pSizer, "DecalNode" );
	pSizer->AddObject( this, sizeof( *this ) + m_decals.capacity() * sizeof( CDecal* ) );
}
