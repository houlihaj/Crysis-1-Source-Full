//////////////////////////////////////////////////////////////////////
//
//  Game Source Code
//
//  File: XArea.cpp
//  Description: 2D area class. Area is in XY plane.
//	2D areas manager
//
//  History:
//  - Feb 24, 2002: Created by Kirill Bulatsev
//
//////////////////////////////////////////////////////////////////////
 
#include "stdafx.h"
#include "Area.h"
#include "AreaManager.h"
#include "ISound.h"

//////////////////////////////////////////////////////////////////////////
//*************************************************************************************************************
//
//			AREAs MANEGER
//
//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////
CAreaManager::CAreaManager( CEntitySystem *pEntitySystem )// : CSoundAreaManager(pEntitySystem)
{
	m_pEntitySystem = pEntitySystem;
	m_pCurrArea=m_pPrevArea=NULL;
	m_nCurStep = 1;
	m_nCurSoundStep = 11;
	m_nCurTailStep = 22;
	m_bAreasDirty = true;
}

//////////////////////////////////////////////////////////////////////////
CAreaManager::~CAreaManager()
{
	assert( m_vpAreas.size() == 0 );
}

//////////////////////////////////////////////////////////////////////////
CArea* CAreaManager::CreateArea()
{
	CArea *pArea = new CArea(this);

	m_vpAreas.push_back( pArea );
	return pArea;
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::Unregister( CArea *pArea )
{
	//invalidating cache and grid
	m_areaCache.clear();
	m_bAreasDirty = true;

	// removing the area reference
	for(unsigned int i = 0; i < m_vpAreas.size(); i++)
	{
		if (m_vpAreas[i] == pArea)
		{
			m_vpAreas.erase( m_vpAreas.begin() + i );
			break;
		}
	}
}

//	gets area by point
//////////////////////////////////////////////////////////////////////////
CArea*	CAreaManager::GetArea( const Vec3& point )
{
	float		dist;
	float		closeDist=-1;
	CArea*	closeArea=NULL;

	for(unsigned int aIdx=0; aIdx<m_vpAreas.size(); aIdx++)
	{
		if( m_vpAreas[aIdx]->GetAreaType() == ENTITY_AREA_TYPE_SHAPE)
		{
			dist = m_vpAreas[aIdx]->IsPointWithinDist( point );
			if(dist>0)
			{
				if( !closeArea || closeDist>dist )
				{
					closeDist = dist;
					closeArea = m_vpAreas[aIdx];
				}
			}
		}
	}
	return closeArea;
}

int CAreaManager::GetLinkedAreas(EntityId linkedId, int areaId, std::vector<CArea *> &areas)
{
	static std::vector<EntityId> ids;
	for(unsigned int aIdx=0; aIdx<m_vpAreas.size(); aIdx++)
	{
		if (CArea *pArea=m_vpAreas[aIdx])
		{
			ids.resize(0);
			pArea->GetEntites(ids);

			if (!ids.empty())
			{
				for (unsigned int eIdx=0; eIdx<ids.size(); eIdx++)
				{
					if (ids[eIdx]==linkedId)
					{
						if (areaId==-1 || areaId==pArea->GetID())
							areas.push_back(pArea);
					}
				}
			}
		}
	}

	return areas.size();
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::MarkPlayerForUpdate( EntityId id )
{
	// we could get the same entity for update in one frame -> push_back_unique
	stl::push_back_unique(m_playerEntitiesToUpdate, id);
}

//////////////////////////////////////////////////////////////////////////
void  CAreaManager::Update()
{
	for (int i = 0; i < m_playerEntitiesToUpdate.size(); i++)
	{
		IEntity *pPlayer = g_pIEntitySystem->GetEntity(m_playerEntitiesToUpdate[i]);
		if (pPlayer)
		{
			Vec3 playerPos = pPlayer->GetWorldPos() + Vec3(0,0,0.01f);
			UpdatePlayer( playerPos ,pPlayer );
		}
	}
	m_playerEntitiesToUpdate.clear();
}

//	check pPlayer's position for all the areas. Do onEnter, onLeave, updateFade
//////////////////////////////////////////////////////////////////////////
void	CAreaManager::UpdatePlayer( const Vec3 &vPos,IEntity *pPlayer )
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_ENTITY );

	SAreasCache *pAreaCache = GetAreaCache(pPlayer->GetId());
	if (pAreaCache)
	{
		if (vPos == pAreaCache->vLastUpdatePos)
			return;
		pAreaCache->vLastUpdatePos = vPos;
	}

	// increase the step
	m_nCurStep++;

	unsigned int nAreaIndex;

	if (pAreaCache)
	{
		// check all the areas pPlayer is in already
		for (int nCacheIndex = 0; nCacheIndex < pAreaCache->Areas.size(); nCacheIndex++)
		{
			nAreaIndex = pAreaCache->Areas[nCacheIndex].nAreaIndex;

			//safecheck for editor
			if( nAreaIndex >= m_vpAreas.size() )
				return;

			CArea* pArea = m_vpAreas[nAreaIndex];

			// set flag that this area was processed
			if (pArea->GetStepId() == m_nCurStep)
				continue;

			//bool bWasUpdatedAlready = (pArea->GetStepId() == 0);

			// check if Area is hidden
			IEntity *pAreaEntity = m_pEntitySystem->GetEntity(pArea->GetEntityID());
			if (pAreaEntity && pAreaEntity->IsHidden())
			{
				// Is Hidden, so this Area doesnt belong here
				ProceedExclusiveLeave( pPlayer, nAreaIndex );
				pAreaCache->Areas.erase(  pAreaCache->Areas.begin() + nCacheIndex );
				nCacheIndex--;

				SEntityEvent event(pPlayer->GetId(), 0, 0, 0, 0.0f, 0.0f);
				event.event = ENTITY_EVENT_LEAVENEARAREA;
				pArea->SetCachedEvent(&event);
				pArea->SetStepId(m_nCurStep);
				continue;
			}

			//Vec3 Closest3d;
//			float fEffectRadius = pArea->GetMaximumEffectRadius();

			ProcessArea(pArea, nAreaIndex, nCacheIndex, pAreaCache, vPos, pPlayer);
			pArea->SetStepId(m_nCurStep);
		}
	}

	// Update the area grid data structure.
	UpdateDirtyAreas();

	int nSize = m_vpAreas.size();

	// check all the rest areas (pPlayer was outside/far of them)
	for (CAreaGrid::iterator itArea = m_areaGrid.BeginAreas(vPos), itAreaEnd = m_areaGrid.EndAreas(vPos); itArea != itAreaEnd; ++itArea)
	{
		nAreaIndex = *itArea;
		CArea* pArea = m_vpAreas[nAreaIndex];

		// skip those areas being processed already in the cache
		if ( pArea->GetStepId() != m_nCurStep )
		{
			bool bWasUpdatedAlready = (pArea->GetStepId() == 0);

			// check if Area is hidden
			IEntity *pAreaEntity = m_pEntitySystem->GetEntity(pArea->GetEntityID());
			if (pAreaEntity && pAreaEntity->IsHidden())
			{
				// Is Hidden, so this Area is marked as "left"
				SEntityEvent event(pPlayer->GetId(), 0, 0, 0, 0.0f, 0.0f);
				event.event = ENTITY_EVENT_LEAVENEARAREA;
				pArea->SetCachedEvent(&event);

				if (pAreaCache)
				{
					// if that area is in the cache, remove it from there
					for (unsigned int inIdx = 0; inIdx < pAreaCache->Areas.size(); inIdx++)
					{
						unsigned int TempaIdx = pAreaCache->Areas[inIdx].nAreaIndex;
						if (TempaIdx == nAreaIndex)
						{
							pAreaCache->Areas.erase(  pAreaCache->Areas.begin() + inIdx );
							break;
						}
					}
				}
				continue;
			}

			float fMaxRadius = pArea->GetMaximumEffectRadius();
			float fDistanceSq = pArea->PointNearDistSq(vPos);
			bool bIsPointWithin = pArea->IsPointWithin(vPos);

			if (fDistanceSq < fMaxRadius*fMaxRadius || bIsPointWithin)
			{
				// was far away, now enter near or even inside
				if (!pAreaCache)
					pAreaCache = MakeAreaCache(pPlayer->GetId());

				int nCacheIndex = pAreaCache->IsInCache(nAreaIndex);

				if (nCacheIndex == -1)
				{
					// EnterArea is send if bInside is false, that means area was not in cache already
					pAreaCache->Areas.push_back(SAreaCacheEntry(nAreaIndex, false));
					nCacheIndex = pAreaCache->IsInCache(nAreaIndex);

					// send EnterNearArea event if entity did not jump into the area
					if (!bIsPointWithin)
					{
						SEntityEvent event(pPlayer->GetId(), 0, 0, 0, 0.0f, fDistanceSq);
						event.event = ENTITY_EVENT_ENTERNEARAREA;
						pArea->SetCachedEvent(&event);
						pArea->SetStepId(m_nCurStep);
					}
				}

				
				ProcessArea(pArea, nAreaIndex, nCacheIndex, pAreaCache, vPos, pPlayer);
			}
			else
			{
				// for some reason jumped from inside or near to far away

				if (pAreaCache)
				{
					for (unsigned int inIdx = 0; inIdx < pAreaCache->Areas.size(); inIdx++)
					{
						unsigned int TempaIdx = pAreaCache->Areas[inIdx].nAreaIndex;
						if (TempaIdx == nAreaIndex)
						{
							pAreaCache->Areas.erase(  pAreaCache->Areas.begin() + inIdx );
							break;
						}
					}
				}
			}
		}
	}

	if (pAreaCache)
	{
		//
		//update fade. For all hosted areas
		uint32 nCacheSize = pAreaCache->Areas.size();
		for(int nCacheIndex = 0; nCacheIndex < nCacheSize; nCacheIndex++)
		{
			nAreaIndex = pAreaCache->Areas[nCacheIndex].nAreaIndex;
			
			//safecheck for editor
			if( nAreaIndex>=m_vpAreas.size() )
				return;

			CArea* pArea = m_vpAreas[nAreaIndex];

			// this one already updated
			if( pArea->GetStepId() == 0 )
				continue;
			
			// this area is not active - overwritten by same groupId area with higher id (exclusive areas)
			if( !pArea->IsActive() )
				continue;

			ProceedExclusiveUpdate( pPlayer, nAreaIndex);
			//		m_vpAreas[aIdx]->UpdateArea(pPlayer);
		}

		if (pAreaCache->Areas.empty())
		{
			DeleteAreaCache( pPlayer->GetId() );
		}
	}

	// Go through all Areas again and send the events now

	uint32 nAreaSize = m_vpAreas.size();
	for(nAreaIndex = 0; nAreaIndex < nAreaSize; nAreaIndex++)
	{
		CArea* pArea = m_vpAreas[nAreaIndex];
		pArea->SendCachedEvent();
		pArea->SetStepId( m_nCurStep );
	}

	//SoundUpdatePlayer(vPos, pPlayer);
}

void CAreaManager::ProcessArea(CArea* pArea, unsigned int nAreaIndex, int &nCacheIndex, SAreasCache *pAreaCache, const Vec3& vPos, IEntity *pPlayer)
{
	Vec3 Closest3d;
	bool bWasUpdatedAlready = (pArea->GetStepId() == m_nCurStep);
	float fEffectRadius = pArea->GetMaximumEffectRadius();

	if (pArea->IsPointWithin(vPos))
	{
		// was inside/near, now is is inside

		float fDistance = pArea->IsPointWithinDist(vPos);
		float fFade = 1.0f;

		// if there is no cached event waiting, Fade can be overwritten
		if (!bWasUpdatedAlready)
			pArea->SetFade(fFade);

		// fade for nested areas
		fFade = fEffectRadius>0.f ? max(0.f, (fEffectRadius-fDistance)/fEffectRadius) : 0.f; 
		bool bPosInLowerArea = false;
		bool bPosInHigherArea = false;

		int nLowerArea = FindLowerHostedArea(pAreaCache->Areas, nAreaIndex);
		int nHigherArea = FindHighestHostedArea( pAreaCache->Areas, nAreaIndex );

		if ( nLowerArea != -1)
		{
			CArea* pLowerArea = m_vpAreas[nLowerArea];
			bPosInLowerArea = pLowerArea->IsPointWithin(vPos);

			if (bPosInLowerArea)
				ProceedExclusiveSoundUpdate( pPlayer, nAreaIndex, fFade, pArea);
		}

		if ( nHigherArea != -1)
		{
			CArea* pHigherArea = m_vpAreas[nHigherArea];
			bPosInHigherArea = pHigherArea->IsPointWithin(vPos);

			//if (bPosInHigherArea)
			//ProceedExclusiveSoundUpdate( pPlayer, nAreaIndex, fFade, pArea);
		}

		if (!bPosInHigherArea)
		{
			SEntityEvent event(pPlayer->GetId(), 0, 0, 0, 0.0f, 0.0f);
			if (pAreaCache->Areas[nCacheIndex].bInside)
				event.event = ENTITY_EVENT_MOVEINSIDEAREA;
			else
				event.event = ENTITY_EVENT_ENTERAREA;

			pArea->SetCachedEvent(&event);
			pArea->SendCachedEvent(); // send it out immediately so higher areas overwrite these settings.
		}

		pAreaCache->Areas[nCacheIndex].bInside = true;

	}
	else
	{
		// was inside/near, now is not inside anymore
		float fDistanceSq = pArea->PointNearDistSq(vPos, Closest3d);

		if (fDistanceSq < fEffectRadius*fEffectRadius)
		{
			// is near now


			// if there is no cached event waiting, Fade can be overwritten
			if (!bWasUpdatedAlready)
			{
				pArea->SetFade(1.0f);

				SEntityEvent event(pPlayer->GetId(), 0, 0, 0, 0.0f, fDistanceSq);
				if (!pAreaCache->Areas[nCacheIndex].bInside)
					event.event = ENTITY_EVENT_MOVENEARAREA;
				else
					event.event = ENTITY_EVENT_LEAVEAREA;

				pArea->SetCachedEvent(&event);			
			}
			pAreaCache->Areas[nCacheIndex].bInside = false;

			//m_vpAreas[aIdx]->UpdateAreaNear(pPlayer, fDistanceSq);
		}
		else
		{
			// now is far - do LeaveNearArea
			ProceedExclusiveLeave( pPlayer, nAreaIndex );
			pAreaCache->Areas.erase(  pAreaCache->Areas.begin() + nCacheIndex );
			nCacheIndex--;

			if (fEffectRadius > 0)
			{
				SEntityEvent event(pPlayer->GetId(), 0, 0, 0, 0.0f, 0.0f);
				event.event = ENTITY_EVENT_LEAVENEARAREA;
				pArea->SetCachedEvent(&event);
				//m_vpAreas[aIdx]->LeaveNearArea(pPlayer);
			}
		}
	}

}


void	CAreaManager::UpdateTailEntity( const Vec3 &vPos,IEntity *pTailEntity )
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_ENTITY );

	SAreasCache *pAreaCache = GetAreaCache(pTailEntity->GetId());	
	if (pAreaCache)
	{
		if (vPos == pAreaCache->vLastUpdatePos)
			return;
		pAreaCache->vLastUpdatePos = vPos;
	}

	// increase the step
	m_nCurTailStep++;

	unsigned int aIdx;

	if (pAreaCache)
	{
		// check all the areas pPlayer is in already
		for (unsigned int inIdx = 0; inIdx < pAreaCache->Areas.size(); inIdx++)
		{
			aIdx = pAreaCache->Areas[inIdx].nAreaIndex;

			//safecheck for editor
			if( aIdx>=m_vpAreas.size() )
				return;

			CArea* pArea = m_vpAreas[aIdx];

			pArea->SetStepId( m_nCurTailStep );
			if(!pArea->IsPointWithin(vPos))
			{
				// was inside, now is out - do OnLeaveArea
				pAreaCache->Areas.erase(  pAreaCache->Areas.begin() + inIdx );
				inIdx--;
			}
		}
	}

	// check all the rest areas (pPlayer is outside of them)
	for(aIdx=0; aIdx<m_vpAreas.size(); aIdx++)
	{
					
		CArea* pArea = m_vpAreas[aIdx];

		if ( pArea->GetStepId() != m_nCurTailStep )
		{
			if (pArea->HasSoundAttached()) 
			{
				if (pArea->IsPointWithin(vPos))
				{
					// was outside, now inside - do enter area
					//m_vpAreas[aIdx]->EnterArea(pTailEntity);

					if (!pAreaCache)
						pAreaCache = MakeAreaCache(pTailEntity->GetId());
					pAreaCache->Areas.push_back( SAreaCacheEntry(aIdx, true) );
				}
			}
		}
	}

	// skip proceed fade for weapons

	if (pAreaCache)
	{
		if (pAreaCache->Areas.empty())
		{
			DeleteAreaCache( pTailEntity->GetId() );
		}
	}
}

//	gets area by entity
//////////////////////////////////////////////////////////////////////////
CArea*	CAreaManager::GetTailArea( const IEntity *pTailEntity )
{
	CArea* pArea = NULL;

	return pArea; // deactivate for now

	SAreasCache *pAreaCache = GetAreaCache(pTailEntity->GetId());	

	if (pAreaCache)
	{

		int maxArea = FindHighestHostedArea( pAreaCache->Areas, pAreaCache->Areas[0].nAreaIndex );

		if (maxArea == -1)
			pArea = m_vpAreas[pAreaCache->Areas[0].nAreaIndex];
		else					
			pArea=m_vpAreas[maxArea];

		std::vector<EntityId> vEntites;
		pArea->GetEntites( vEntites );

		IEntity* pAreaAttachedEntity = NULL;

		for( unsigned int eIdx=0; eIdx<vEntites.size(); eIdx++ )
		{
			pAreaAttachedEntity = GetEntitySystem()->GetEntity(vEntites[eIdx]);
			//		ASSERT(pEntity);
			if (pAreaAttachedEntity)
			{
				IEntityClass* pClass = pAreaAttachedEntity->GetClass();
				if (pClass)
				{
					if (strcmp(pClass->GetName(),"ReverbVolume") == 0)
					{
						//IEntitySoundProxy *pSoundProxy = (IEntitySoundProxy*)pAreaAttachedEntity->GetProxy(ENTITY_PROXY_SOUND);

						//if (pSoundProxy)
							//pSoundProxy->GetTailName();

						return pArea;

					}
				}
			}
		}
		


		// check all the areas pWeapon is in already
		//for (unsigned int inIdx = 0; inIdx < pAreaCache->Areas.size(); inIdx++)
		//{
		//	aIdx = pAreaCache->Areas[inIdx].nAreaIndex;
		//			
		//	CArea* pArea = m_vpAreas[aIdx];

		//	//TODO returns always the first Area in the cache
		//	closeArea = m_vpAreas[aIdx];
		//}
	}

	return pArea;
}

//	checks for areas in the same group, if entered area is lower priority (areaID) - return false 
//	(don't do enreArea for it)
//////////////////////////////////////////////////////////////////////////
void	CAreaManager::ProceedExclusiveUpdate(  IEntity *pPlayer , unsigned int curIdx )
{
	int	maxArea = -1;
	SAreasCache *pAreaCache = GetAreaCache(pPlayer->GetId());
	if (pAreaCache)
		maxArea = FindHighestHostedArea( pAreaCache->Areas, curIdx );

	CArea *firstArea=m_vpAreas[curIdx];

	// not grouped or the only one in a group
	if (maxArea < 0)
	{
		firstArea->UpdateArea( pPlayer->GetWorldPos(),pPlayer );
		return;
	}

	CArea *secondArea = m_vpAreas[maxArea];

	if( firstArea->GetID()<secondArea->GetID() )
	{
		firstArea = secondArea;
		int	secondMaxArea = FindHighestHostedArea( pAreaCache->Areas, maxArea );
		if (secondMaxArea >= 0)
		{
			secondArea=m_vpAreas[secondMaxArea];
		}
	}

	Vec3 pos = pPlayer->GetWorldPos();
	float fadeBase = firstArea->CalculateFade(pos);

	if (fadeBase < 1.0f)
	{
		if( !secondArea->IsActive() )
			secondArea->EnterArea(pPlayer);

		float fadeSecond = secondArea->CalculateFade(pos);
		fadeSecond *= (1.0f-fadeBase);
		firstArea->ProceedFade(pPlayer, fadeBase);
		secondArea->ProceedFade(pPlayer, fadeSecond);
	}
	else
	{
		if( secondArea->IsActive() )
			secondArea->LeaveArea(pPlayer);
	}

	// mark all the areas of this group as updated
	int curGroup = m_vpAreas[curIdx]->GetGroup();
	for(unsigned int inIdx=0; inIdx < pAreaCache->Areas.size(); inIdx++)
	{
		unsigned int aIdx = pAreaCache->Areas[inIdx].nAreaIndex;

		//safecheck for editor
		if( aIdx>=m_vpAreas.size() )
			continue;

		if(m_vpAreas[curIdx]->GetGroup() != m_vpAreas[aIdx]->GetGroup())
			continue;

		m_vpAreas[aIdx]->SetStepId(0);
	}
}

//	checks for areas in the same group, if entered area is lower priority (areaID) 
//	change fade value the deeper player is inside
//////////////////////////////////////////////////////////////////////////
void	CAreaManager::ProceedExclusiveSoundUpdate(  IEntity *pPlayer , unsigned int curIdx, const float fFade, const CArea* pArea)
{
	int	maxArea = -1;
	SAreasCache *pAreaCache = GetAreaCache(pPlayer->GetId());

	if (!pAreaCache)
		return;

	int size = pAreaCache->Areas.size();

	int nLowerArea = curIdx;
	do 
	{
		nLowerArea = FindLowerHostedArea(pAreaCache->Areas, nLowerArea);
		if (nLowerArea != -1)
		{
			// there is a area with lower priority, so this one controls it
			CArea *lowerArea = m_vpAreas[nLowerArea];
			lowerArea->SetFade( fFade*lowerArea->GetFade() );			
			lowerArea->ExclusiveUpdateAreaInside(pPlayer, pArea->GetEntityID());
			lowerArea->SetStepId(m_nCurStep);

		} 
	} while (nLowerArea != -1);

	int nHigherArea = curIdx;
	do 
	{
		nHigherArea = FindHighestHostedArea(pAreaCache->Areas, nHigherArea);
		if (nHigherArea != -1)
		{
			// there is a area with higher priority, so this one is later processed by it
			//CArea *higherArea = m_vpAreas[nHigherArea];
			//higherArea->SetFade(fFade);
			//higherArea->SetStepId(m_nCurStep);
		} 
	} while (nHigherArea != -1);


	//Vec3 pos = pPlayer->GetWorldPos();
	//float fadeBase = firstArea->CalcDistToPoint()

	//if( fadeBase<1.0f )
	//{
	//	if( !secondArea->IsActive() )
	//		secondArea->EnterArea(pPlayer);
	//	float fadeSecond = secondArea->CalculateFade(pos);
	//	fadeSecond *= (1.0f-fadeBase);
	//	firstArea->ProceedFade(pPlayer, fadeBase);
	//	secondArea->ProceedFade(pPlayer, fadeSecond);
	//}
	//else
	//{
	//	if( secondArea->IsActive() )
	//		secondArea->LeaveArea(pPlayer);
	//}

	// mark all the areas of this group as updated
	int curGroup = m_vpAreas[curIdx]->GetGroup();
	for(unsigned int inIdx=0; inIdx < pAreaCache->Areas.size(); inIdx++)
	{
		unsigned int aIdx = pAreaCache->Areas[inIdx].nAreaIndex;
		
		//safecheck for editor
		if( aIdx>=m_vpAreas.size() )
			continue;
		
		if(m_vpAreas[curIdx]->GetGroup() != m_vpAreas[aIdx]->GetGroup())
			continue;
		
		//m_vpAreas[aIdx]->SetStepId(0);
	}
}



//	checks for areas in the same group, if entered area is lower priority (areaID) - return false 
//	(don't do enreArea for it)
//////////////////////////////////////////////////////////////////////////
bool	CAreaManager::ProceedExclusiveEnter(  IEntity *pPlayer , unsigned int curIdx )
{
	int	maxArea = -1;
	SAreasCache *pAreaCache = GetAreaCache(pPlayer->GetId());
	if (pAreaCache)
		maxArea = FindHighestHostedArea( pAreaCache->Areas, curIdx );

	if(maxArea<0)
		return true;

	int maxID = m_vpAreas[maxArea]->GetID();
	if(m_vpAreas[curIdx]->GetID()<maxID)
		return false;

	//	m_vpAreas[aIdx]->LeaveArea( pPlayer );			// new area will override this one
	return true;
}

//	checks for areas in the same group, if entered area is lower priority (areaID) - return false 
//	(don't do enreArea for it)
//////////////////////////////////////////////////////////////////////////
bool	CAreaManager::ProceedExclusiveLeave(  IEntity *pPlayer , unsigned int curIdx )
{
	int	maxArea = -1;
	SAreasCache *pAreaCache = GetAreaCache(pPlayer->GetId());
	if (pAreaCache)
		maxArea = FindHighestHostedArea( pAreaCache->Areas, curIdx );

	if( maxArea<0 )
	{
		//m_vpAreas[curIdx]->LeaveArea( pPlayer );
		SEntityEvent event( pPlayer->GetId(), 0, 0, 0, 0.0f, 0.0f );
		event.event = ENTITY_EVENT_LEAVEAREA;
		m_vpAreas[curIdx]->SetCachedEvent( &event );
		return true;
	}

	int maxID = m_vpAreas[maxArea]->GetID();
	if( m_vpAreas[curIdx]->GetID()<maxID )
		return false;

	m_vpAreas[curIdx]->LeaveArea( pPlayer );

	if(!m_vpAreas[maxArea]->IsActive())
	{
		SEntityEvent event(pPlayer->GetId(), 0, 0, 0, 0.0f, 0.0f);
		event.event = ENTITY_EVENT_ENTERAREA;
		m_vpAreas[maxArea]->SetCachedEvent(&event);
		//m_vpAreas[maxArea]->EnterArea( pPlayer );			// new area will owerride this one
	}

	return true;
}

//	do onexit for all areas pPlayer is in - do it before kill pPlayer
//////////////////////////////////////////////////////////////////////////
void	CAreaManager::ExitAllAreas( IEntity *pPlayer )
{
	int	maxArea = -1;
	SAreasCache *pAreaCache = GetAreaCache(pPlayer->GetId());
	if (!pAreaCache)
		return;

	// check all the areas pPlayer is in already
	for(unsigned int inIdx=0; inIdx < pAreaCache->Areas.size(); inIdx++)
	{
		unsigned int aIdx;
		aIdx = pAreaCache->Areas[inIdx].nAreaIndex;
		// was inside, now is out - do OnLeaveArea
		if(aIdx<m_vpAreas.size())
		{
			m_vpAreas[aIdx]->LeaveArea( pPlayer );
			pAreaCache->Areas.erase( pAreaCache->Areas.begin() + inIdx );
			inIdx--;
		}
	}
	DeleteAreaCache( pPlayer->GetId() );
}


/*
// [marco] functions common for game and editor mode
// to be able to retrigger areas etc.
//////////////////////////////////////////////////////////////////////////
void CAreaManager::CheckSoundVisAreas()
{
	if (gEnv->pSoundSystem)
	{
		m_pPrevArea = gEnv->pSoundSystem->GetListenerArea();		
		if ((!m_pCurrArea && m_pPrevArea) || (m_pCurrArea && !m_pPrevArea))
		{
			// if we switched between outdoor and indoor, let's
			// retrigger the area to account for random ambient sounds
			// indoor / outdoor only
			//		ReTriggerArea(m_pSystem->GetISoundSystem()->GetListenerPos(),false);

			//
			//if( GetMyPlayer() )
			//ReTriggerArea(GetMyPlayer(), m_pSystem->GetISoundSystem()->GetListenerPos(),false);
		}
		m_pPrevArea = m_pCurrArea;
		m_pCurrArea = gEnv->pSoundSystem->GetListenerArea();
	}
}
*/

//	find hosted area with highest priority withing the group of curIdx area
//////////////////////////////////////////////////////////////////////////
int	CAreaManager::FindHighestHostedArea( TAreaCacheVector &hostedAreas, unsigned int curIdx )
{
	unsigned int aIdx;
	int	minPriority = m_vpAreas[curIdx]->GetPriority();
	int	higherArea=-1;
	int curGroup = m_vpAreas[curIdx]->GetGroup();

	if(curGroup<=0)
	{
		return higherArea;
	}
	for(unsigned int inIdx=0; inIdx<hostedAreas.size(); inIdx++)
	{
		aIdx = hostedAreas[inIdx].nAreaIndex;
		if( aIdx == curIdx )
			continue;

		//safecheck for editor
		if( aIdx>=m_vpAreas.size() )
			continue;

		CArea *pArea = m_vpAreas[aIdx];

		if(curGroup != pArea->GetGroup())	// different groups
			continue;

		if(minPriority < pArea->GetPriority())
		{
			minPriority = pArea->GetPriority();
			higherArea	= aIdx;
		}
	}
	return higherArea;
}


//	find hosted area with lower priority withing the group of curIdx area
//////////////////////////////////////////////////////////////////////////
int	CAreaManager::FindLowerHostedArea( TAreaCacheVector &hostedAreas, unsigned int curIdx )
{
	unsigned int aIdx;
	int	maxPriority = -1;
	int	lowerArea=-1;
	int curGroup = m_vpAreas[curIdx]->GetGroup();

	if(curGroup<=0)
	{
		return lowerArea;
	}
	for(unsigned int inIdx=0; inIdx<hostedAreas.size(); inIdx++)
	{
		aIdx = hostedAreas[inIdx].nAreaIndex;
		if( aIdx == curIdx )
			continue;

		//safecheck for editor
		if( aIdx>=m_vpAreas.size() )
			continue;

		CArea *pArea = m_vpAreas[aIdx];

		if(curGroup != pArea->GetGroup())	// different groups
			continue;

		if( (pArea->GetPriority() > maxPriority) && (pArea->GetPriority() < m_vpAreas[curIdx]->GetPriority() ) )
		{
			maxPriority = pArea->GetPriority();
			lowerArea	= aIdx;
		}
	}
	return lowerArea;
}

//////////////////////////////////////////////////////////////////////////
void	CAreaManager::DrawAreas(const ISystem * const pSystem) 
{
	for(unsigned int aIdx=0; aIdx<m_vpAreas.size(); aIdx++)
		m_vpAreas[aIdx]->Draw( aIdx );
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::DrawGrid()
{
	m_areaGrid.Draw();
}

//////////////////////////////////////////////////////////////////////////
unsigned CAreaManager::MemStat()
{
	unsigned memSize = sizeof *this;

	for(unsigned int aIdx=0; aIdx<m_vpAreas.size(); aIdx++)
		memSize += m_vpAreas[aIdx]->MemStat();

	return memSize;
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::ReTriggerArea(IEntity* pEntity, const Vec3 &vPos,bool bIndoor)
{
	TAreaCacheVector	hostedAreasIdx;
	std::vector<int>	updatedID;
	unsigned int aIdx;


	hostedAreasIdx.clear();
	updatedID.clear();
	// check all the rest areas (pPlayer is outside of them)
	for(aIdx=0; aIdx<m_vpAreas.size(); aIdx++)
		if(m_vpAreas[aIdx]->IsPointWithin(vPos))
			hostedAreasIdx.push_back( SAreaCacheEntry(aIdx, true) );

	for(int idxIdx=0; idxIdx<(int)(hostedAreasIdx.size()); idxIdx++  )
	{
		if(std::find(updatedID.begin(), updatedID.end(), m_vpAreas[hostedAreasIdx[idxIdx].nAreaIndex]->GetGroup())!=updatedID.end())
			continue;
		int exclIdx = FindHighestHostedArea( hostedAreasIdx, hostedAreasIdx[idxIdx].nAreaIndex );
		if(exclIdx<0)
		{
			m_vpAreas[hostedAreasIdx[idxIdx].nAreaIndex]->EnterArea(pEntity);
			continue;
		}
		m_vpAreas[exclIdx]->EnterArea(pEntity);
		updatedID.push_back( m_vpAreas[hostedAreasIdx[idxIdx].nAreaIndex]->GetGroup() );
	}


/*
unsigned int aIdx;
int	sector = 0;		//(int)pPlayer->GetGame()->GetSystem()->GetI3DEngine()->GetVisAreaFromPos(pos);
int	building = 0;	 
	// check all the rest areas (pPlayer is outside of them)
	for(aIdx=0; aIdx<m_vpAreas.size(); aIdx++)
		if(m_vpAreas[aIdx]->IsPointWithin(vPos, building, sector))
		{
			m_vpAreas[aIdx]->EnterArea(pEntity, m_pSystem);
		}
*/
}

void CAreaManager::ResetAreas()
{
	m_areaCache.clear();
	m_playerEntitiesToUpdate.clear();

	// invalidate cached event
	for(unsigned int i = 0; i < m_vpAreas.size(); i++)
	{
		m_vpAreas[i]->SetCachedEvent(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::SetAreasDirty()
{
	m_bAreasDirty = true;
}

//////////////////////////////////////////////////////////////////////////
void CAreaManager::UpdateDirtyAreas()
{
	if (!m_bAreasDirty)
		return;

	m_areaGrid.Compile(m_pEntitySystem, m_vpAreas);

	m_bAreasDirty = false;
}

bool CAreaManager::IsInDiagArea( const Vec3 &vPos )
{
	CArea *pArea = GetArea( vPos );
	if (pArea)
	{
		if (pArea->GetAreaType() == ENTITY_AREA_TYPE_SHAPE)
		{
			int nEntityID = pArea->GetEntityID();
			IEntity *pEntity = m_pEntitySystem->GetEntity( nEntityID );
			if (pEntity && !stricmp(pEntity->GetName(), "diagarea") )
			{
				return true;
			}
		}
	}
	return false;
}
