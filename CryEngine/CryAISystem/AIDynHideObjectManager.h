/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
File name:   CAIDynHideObjectManager.cpp
$Id$
Description: Provides to query hide points around entities which 
are flagged as AI hideable. The manage also caches the objects.

-------------------------------------------------------------------------
History:
- 2007				: Created by Mikko Mononen

*********************************************************************/

#ifndef _AIDYNHIDEOBJECTMANAGER_H_
#define _AIDYNHIDEOBJECTMANAGER_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include "CAISystem.h"
#include "StlUtils.h"

struct IRenderer;

struct SDynamicObjectHideSpot 
{
	SDynamicObjectHideSpot(const Vec3 &pos = ZERO, const Vec3 &dir = ZERO, EntityId id = 0, unsigned nodeIndex = 0) :
		pos(pos), dir(dir), entityId(id), nodeIndex(nodeIndex) {}
	Vec3 pos, dir;
	EntityId	entityId;
	unsigned nodeIndex;
};

class CAIDynHideObjectManager
{
public:
	CAIDynHideObjectManager();
	~CAIDynHideObjectManager();

	void Reset();

	void GetHidePositionsWithinRange(std::vector<SDynamicObjectHideSpot> &hideSpots, const Vec3 &pos, float radius,
			IAISystem::tNavCapMask navCapMask, float passRadius, unsigned lastNavNodeIndex = 0);

	bool ValidateHideSpotLocation(const Vec3& pos, const SAIBodyInfo& bi, EntityId objectEntId);

	void DebugDraw(IRenderer* pRenderer);

private:

	void ResetCache();
	void FreeCacheItem(int i);
	int GetNewCacheItem();
	unsigned GetPositionHashFromEntity(IEntity* pEntity);
	void InvalidateHideSpotLocation(const Vec3& pos, EntityId objectEntId);

	struct SCachedDynamicObject
	{
		EntityId id;
		unsigned positionHash;
		std::vector<SDynamicObjectHideSpot> spots;
		CTimeValue timeStamp;
	};

	typedef VectorMap<EntityId, unsigned> DynamicOHideObjectMap;

	DynamicOHideObjectMap m_cachedObjects;
	std::vector<SCachedDynamicObject> m_cache;
	std::vector<int> m_cacheFreeList;
};

#endif