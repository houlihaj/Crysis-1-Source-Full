#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "WorldQuery.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include "IViewSystem.h"
#include "IMovementController.h"

CWorldQuery::UpdateQueryFunction CWorldQuery::m_updateQueryFunctions[] = {
	&CWorldQuery::UpdateRaycastQuery,
	&CWorldQuery::UpdateProximityQuery,
	&CWorldQuery::UpdateInFrontOfQuery
};

namespace
{

	class CCompareEntitiesByDistanceFromPoint
	{
	public:
		CCompareEntitiesByDistanceFromPoint( const Vec3& point ) : m_point(point), m_pEntitySystem(gEnv->pEntitySystem) {}

		bool operator()( IEntity * pEnt0, IEntity * pEnt1 ) const
		{
			float distSq0 = (pEnt0->GetPos() - m_point).GetLengthSquared();
			float distSq1 = (pEnt1->GetPos() - m_point).GetLengthSquared();
			return distSq0 < distSq1;
		}
		bool operator()( EntityId ent0, EntityId ent1 ) const
		{
			return this->operator()( m_pEntitySystem->GetEntity(ent0), m_pEntitySystem->GetEntity(ent1) );
		}

	private:
		Vec3 m_point;
		IEntitySystem * m_pEntitySystem;
	};

}

CWorldQuery::CWorldQuery()
{
	m_wpos	=	Vec3(ZERO);
	m_dir		=	Vec3(0,1,0);
	m_pActor = NULL;
	m_lookAtEntityId = 0;
	m_validQueries = 0;
	m_proximityRadius = 5.0f;
	m_pEntitySystem = gEnv->pEntitySystem;
	m_pPhysWorld = gEnv->pPhysicalWorld;
	m_pViewSystem = CCryAction::GetCryAction()->GetIViewSystem();
	m_rayHitAny = false;
  m_renderFrameId = -1;
}

CWorldQuery::~CWorldQuery()
{
}

bool CWorldQuery::Init( IGameObject * pGameObject )
{
	SetGameObject(pGameObject);
	m_pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor( GetEntityId() );
	if (!m_pActor)
	{
		GameWarning("WorldQuery extension only available for actors");
		return false;
	}
	return true;
}

void CWorldQuery::PostInit( IGameObject * pGameObject )
{
	pGameObject->EnableUpdateSlot(this,0);
}

void CWorldQuery::Release()
{
	delete this;
}

void CWorldQuery::FullSerialize( TSerialize ser )
{
	if (ser.GetSerializationTarget() == eST_Network)
		return;

	ser.Value("proximityRadius", m_proximityRadius);
	m_validQueries = 0;

	if(GetISystem()->IsSerializingFile() == 1)
	{
		m_pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor( GetEntityId() );
		assert(m_pActor);
	}
}

void CWorldQuery::Update( SEntityUpdateContext& ctx, int slot )
{
	//m_validQueries = 0;
  //m_renderFrameId = gEnv->pRenderer->GetFrameID();

	if(!m_pActor)
		return;

	IMovementController * pMC = m_pActor->GetMovementController();
	if (pMC)
	{
		SMovementState s;
		pMC->GetMovementState(s);
		m_wpos = s.eyePosition;
		m_dir = s.eyeDirection;
	}
}

void CWorldQuery::UpdateRaycastQuery()
{
	if(!m_pActor)
		return;
	IEntity * pEntity = m_pEntitySystem->GetEntity( m_pActor->GetEntityId() );
	IPhysicalEntity * pPhysEnt = pEntity? pEntity->GetPhysics() : NULL;

	static const int obj_types = ent_all; // ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
	static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
	m_rayHitAny = 0 != m_pPhysWorld->RayWorldIntersection( m_wpos, 300.0f * m_dir, obj_types, flags, &m_rayHit, 1, pPhysEnt );
	if(m_rayHitAny)
	{
		IEntity *pLookAt=m_pEntitySystem->GetEntityFromPhysics(m_rayHit.pCollider);
		if (pLookAt)
		{
			m_lookAtEntityId = pLookAt->GetId();
			//m_rayHit.pCollider=0; // -- commented until a better solution is thought of.
		}
		else
			m_lookAtEntityId = 0;
	}
	else
		m_lookAtEntityId = 0;

//	if (m_rayHitAny)
//		CryLogAlways( "HIT: terrain:%i distance:%f (%f,%f,%f)", m_rayHit.bTerrain, m_rayHit.dist, m_rayHit.pt.x, m_rayHit.pt.y, m_rayHit.pt.z );
}

void CWorldQuery::UpdateProximityQuery()
{
	m_proximity.resize(0);

	SEntityProximityQuery qry;
	m_pEntitySystem->QueryProximity( qry );
	m_proximity.reserve(qry.nCount);
	for (int i=0; i<qry.nCount; i++)
	{
		m_proximity.push_back( qry.pEntities[i]->GetId() );
	}

/*
	IPhysicalEntity ** pList;
	int n = m_pEntitySystem->GetPhysicalEntitiesInBox( m_pos, m_proximityRadius, pList );
	m_proximity.reserve(n);
	for (int i=0; i<n; i++)
		m_proximity.push_back( m_pEntitySystem->GetEntityFromPhysics(pList[i])->GetId() );
*/
}

void CWorldQuery::UpdateInFrontOfQuery()
{
	m_inFrontOf.resize(0);
	Lineseg lineseg(m_wpos, m_wpos + m_proximityRadius * m_dir);

	SEntityProximityQuery qry;
	qry.nEntityFlags=~0;
	qry.pEntityClass=0;
	qry.box=AABB(Vec3(m_wpos.x-m_proximityRadius,m_wpos.y-m_proximityRadius,m_wpos.z-m_proximityRadius),
							 Vec3(m_wpos.x+m_proximityRadius,m_wpos.y+m_proximityRadius,m_wpos.z+m_proximityRadius));
	m_pEntitySystem->QueryProximity( qry );
	int n = qry.nCount;
	m_inFrontOf.reserve(n);

	IEntity * pEntity;

	for (int i=0; i<n; ++i)
	{
		pEntity = qry.pEntities[i];
		
		//skip the user
		if (!pEntity || pEntity->GetId() == m_pActor->GetEntityId())
			continue;

		AABB bbox;
		pEntity->GetLocalBounds(bbox);
		if (!bbox.IsEmpty())
		{
			OBB obb(OBB::CreateOBBfromAABB(Matrix33(pEntity->GetWorldTM()), bbox));
			if (Overlap::Lineseg_OBB(lineseg, pEntity->GetWorldPos(), obb))
				m_inFrontOf.push_back( pEntity->GetId() );
		}
	}

	/*IEntityItPtr pIt = m_pEntitySystem->GetEntityIterator();
	while (IEntity * pEntity = pIt->Next())
	{
		//skip the user
		if (pEntity->GetId() == m_pActorProxy->GetEntityId())
			continue;

		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		if (!bbox.IsEmpty())
			if (Overlap::Lineseg_AABB(lineseg, bbox))
				m_inFrontOf.push_back( pEntity->GetId() );
	}

	std::sort( m_inFrontOf.begin(), m_inFrontOf.end(), CCompareEntitiesByDistanceFromPoint(m_pos) );*/
}

void CWorldQuery::HandleEvent( const SGameObjectEvent& )
{
}

void CWorldQuery::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_proximity);
	s->AddContainer(m_inFrontOf);
}