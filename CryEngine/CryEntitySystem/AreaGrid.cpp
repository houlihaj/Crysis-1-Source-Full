////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   AreaGrid.h
//  Version:     v1.00
//  Created:     08/03/2007 by Michael Smith.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AreaGrid.h"
#include "Area.h"
#include <IRenderAuxGeom.h>

const float CAreaGrid::GRID_SIZE = 40.0f;
const int CAreaGrid::BUCKET_COUNT = 1291;

CAreaGrid::CAreaGrid(): m_pAreas(0), m_pEntitySystem(0) {}

unsigned PositionTag(const int x, const int y) {return unsigned(x & 0xFFFF) | unsigned((y & 0xFFFF) << 16);}

void CAreaGrid::Compile(CEntitySystem* pEntitySystem, std::vector<CArea*>& areas)
{
	FUNCTION_PROFILER( GetISystem(),PROFILE_ENTITY );

	Reset();

	m_pEntitySystem = pEntitySystem;

	std::vector<unsigned short> hashBuckets(BUCKET_COUNT);
	std::fill_n(hashBuckets.begin(), BUCKET_COUNT, 0);
	std::vector<HashNode> hashNodes(1);
	hashNodes[0].nextIndex = 0;
	hashNodes[0].firstAreaListNodeIndex = 0;
	hashNodes[0].positionTag = ~0;
	std::vector<AreaListNode> areaListNodes(1);
	areaListNodes[0].nextIndex = 0;
	areaListNodes[0].areaIndex = ~0;

	// Loop through all the grid cells that might be covered.
	std::vector<int> areaIndexList; // Declare some vectors out here so we don't keep freeing the memory and reallocating.
	for (int areaToStepThroughIndex = 0, areaToStepThroughCount = areas.size(); areaToStepThroughIndex < areaToStepThroughCount; ++areaToStepThroughIndex)
	{
		CArea* const pAreaWhoseBoundingBoxToStepThrough = areas[areaToStepThroughIndex];

		// Calculate a loose bounding box (ie one that covers all the region we will have to check this
		// shape for, including fade area).
		Vec2 vBBCentre(0, 0);
		Vec2 vBBExtent(0, 0);
		switch (pAreaWhoseBoundingBoxToStepThrough ? pAreaWhoseBoundingBoxToStepThrough->GetAreaType() : EEntityAreaType(-1))
		{
		case ENTITY_AREA_TYPE_SPHERE:
			{
				Vec3 vSphereCentre(ZERO);
				float fSphereRadius(0.0f);
				if (pAreaWhoseBoundingBoxToStepThrough)
					pAreaWhoseBoundingBoxToStepThrough->GetSphere(vSphereCentre, fSphereRadius);
				vBBCentre = Vec2(vSphereCentre.x, vSphereCentre.y);
				float fMaxEffectiveRadius = GetMaximumEffectRadius(pAreaWhoseBoundingBoxToStepThrough);
				vBBExtent = Vec2(fSphereRadius + fMaxEffectiveRadius, fSphereRadius + fMaxEffectiveRadius);
			}
			break;

		case ENTITY_AREA_TYPE_SHAPE:
			{
				Vec2 vShapeMin, vShapeMax;
				pAreaWhoseBoundingBoxToStepThrough->GetBBox(vShapeMin, vShapeMax);
				vBBCentre = (vShapeMax + vShapeMin) * 0.5f;
				float fMaxEffectiveRadius = GetMaximumEffectRadius(pAreaWhoseBoundingBoxToStepThrough);
				vBBExtent = (vShapeMax - vShapeMin) * 0.5f + Vec2(fMaxEffectiveRadius, fMaxEffectiveRadius);
			}
			break;

		case ENTITY_AREA_TYPE_BOX:
			{
				Vec3 vBoxMin, vBoxMax;
				pAreaWhoseBoundingBoxToStepThrough->GetBox(vBoxMin, vBoxMax);
				Matrix34 tm;
				pAreaWhoseBoundingBoxToStepThrough->GetBoxMatrix(tm);
				static const float sqrt2 = 1.42f;
				vBBExtent = Vec2(abs(vBoxMax.x - vBoxMin.x), abs(vBoxMax.y - vBoxMin.y)) * sqrt2 * 0.5f;
				float fMaxEffectiveRadius = GetMaximumEffectRadius(pAreaWhoseBoundingBoxToStepThrough);
				vBBExtent += Vec2(fMaxEffectiveRadius, fMaxEffectiveRadius);
				const Vec3 vBoxMinWorld = tm.TransformPoint(vBoxMin);
				const Vec3 vBoxMaxWorld = tm.TransformPoint(vBoxMax);
				vBBCentre = Vec2(vBoxMinWorld.x + vBoxMaxWorld.x, vBoxMinWorld.y + vBoxMaxWorld.y) * 0.5f;
			}
			break;
		}

		// Loop through all the grid cells that intersect this bounding box.
		const int gridMinX = (int)(floor((vBBCentre.x - vBBExtent.x) / GRID_SIZE));
		const int gridMaxX = (int)(floor((vBBCentre.x + vBBExtent.x) / GRID_SIZE));
		const int gridMinY = (int)(floor((vBBCentre.y - vBBExtent.y) / GRID_SIZE));
		const int gridMaxY = (int)(floor((vBBCentre.y + vBBExtent.y) / GRID_SIZE));
		for (int gridY = gridMinY, gridX = gridMinX; gridY <= gridMaxY; ++gridX, (gridX > gridMaxX ? ++gridY, gridX = gridMinX : 0))
		{
			// Check whether this cell has already been processed.
			const unsigned positionTag = PositionTag(gridX, gridY);
			const unsigned hashNodeIndex = GetHashNodeIndex(hashBuckets, hashNodes, positionTag);
			if (!hashNodeIndex)
			{
				Vec2 gridCentrePoint = Vec2((float(gridX) + 0.5f) * GRID_SIZE, (float(gridY) + 0.5f) * GRID_SIZE);

				// Loop through all the areas.
				areaIndexList.clear();
				for (int areaIndex = 0, areaCount = areas.size(); areaIndex < areaCount; ++areaIndex)
				{
					CArea* const pArea = areas[areaIndex];

					// Check whether this area should be included in this grid cell.
					float fMaxRadius = GetMaximumEffectRadius(pArea) + GRID_SIZE;
					float fDistanceSq = pArea->PointNearDistSq(gridCentrePoint, true);
					bool bIsPointWithin = pArea->IsPointWithin(gridCentrePoint, true);
					if (fDistanceSq < fMaxRadius*fMaxRadius || bIsPointWithin)
						areaIndexList.push_back(areaIndex);
				}

				// Find the longest area list tail that matches our tail - we can use this to share as much as possible of the area node list data.
				int longestTail = 0;
				int longestTailIndex = 0;
				for (int areaListNodeIndex = 1, areaListNodeCount = areaListNodes.size(); areaListNodeIndex < areaListNodeCount; ++areaListNodeIndex)
				{
					AreaListNode& node = areaListNodes[areaListNodeIndex];
					int startPosition = -1;
					for (int i = 0, count = areaIndexList.size(); i < count; ++i)
						startPosition = (areaIndexList[i] == node.areaIndex ? i : startPosition);
					bool valid = startPosition != -1;
					int tail;
					if (valid)
					{
						int nodeIndex = -1, listPosition = -1;
						for (nodeIndex = areaListNodeIndex, listPosition = startPosition; nodeIndex; nodeIndex = areaListNodes[nodeIndex].nextIndex, ++listPosition)
							valid = valid && (listPosition < int(areaIndexList.size())) && (areaListNodes[nodeIndex].areaIndex == areaIndexList[listPosition]);
						valid = valid && (nodeIndex == 0) && (listPosition == areaIndexList.size());
						tail = areaIndexList.size() - startPosition;
					}
					if (valid && tail > longestTail)
					{
						longestTail = tail;
						longestTailIndex = areaListNodeIndex;
					}
				}

				// Create the rest of the list.
				int tailIndex = longestTailIndex;
				for (int listIndex = areaIndexList.size() - longestTail - 1; listIndex >= 0; --listIndex)
				{
					// Create a new node.
					int newAreaListNodeIndex = areaListNodes.size();
					areaListNodes.resize(areaListNodes.size() + 1);
					AreaListNode& newNode = areaListNodes.back();
					newNode.areaIndex = areaIndexList[listIndex];
					newNode.nextIndex = tailIndex;
					tailIndex = newAreaListNodeIndex;
				}

				if (tailIndex)
					SetFirstAreaIndex(hashBuckets, hashNodes, positionTag, tailIndex);
			}
		}
	}

	// Assign the values to the member variables. Assigning the vectors here ensures that they use exactly as much
	// memory as they need, rather than having buffers larger than necessary (due to using push_back() above).
	m_pAreas = &areas;
	m_hashBuckets = hashBuckets;
	m_hashNodes = hashNodes;
	m_areaListNodes = areaListNodes;
}

void CAreaGrid::Reset()
{
	m_pAreas = 0;
	std::vector<unsigned short>().swap(m_hashBuckets);
	std::vector<HashNode>().swap(m_hashNodes);
	std::vector<AreaListNode>().swap(m_areaListNodes);
}

CAreaGrid::iterator CAreaGrid::BeginAreas(const Vec3& position)
{
	if (m_pAreas == 0 || m_hashBuckets.empty())
	{
		EntityWarning("CAreaGrid::BeginAreas() called before being initialized.");
		return EndAreas(position);
	}

	int gridX = (int)(position.x / GRID_SIZE);
	int gridY = (int)(position.y / GRID_SIZE);

	unsigned int positionTag = PositionTag(gridX, gridY);

	unsigned int hashNodeIndex = GetHashNodeIndex(m_hashBuckets, m_hashNodes, positionTag);

	// hashNodeIndex might be 0 here, but that's ok, we set up a special dummy entry at 0
	// for just that purpose.
	unsigned int areaListNodeIndex = m_hashNodes[hashNodeIndex].firstAreaListNodeIndex;

	return iterator(this, areaListNodeIndex);
}

CAreaGrid::iterator CAreaGrid::EndAreas(const Vec3& position)
{
	return iterator(this, 0);
}

float CAreaGrid::GetMaximumEffectRadius(CArea *pArea)
{
	float fMaxRadius = 0.0f;

	std::vector<EntityId> entIDs;
	pArea->GetEntites(entIDs);

	// Check all Entities.
	for (int i = 0; i < entIDs.size(); i++)
	{
		int entityId = entIDs[i];
		IEntity* pEntity = (m_pEntitySystem ? m_pEntitySystem->GetEntity(entityId) : 0);
		if (pEntity)
		{
			IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)pEntity->GetProxy(ENTITY_PROXY_SOUND);
			if (pSoundProxy)
			{
				fMaxRadius = max(fMaxRadius, pSoundProxy->GetEffectRadius());
			}
		}
	}

	return fMaxRadius;
}

unsigned CAreaGrid::GetHashNodeIndex(const std::vector<unsigned short>& hashBuckets, const std::vector<HashNode>& hashNodes, unsigned positionTag)
{
	unsigned int bucketIndex = positionTag % (hashBuckets.size());
	unsigned int hashNodeIndex = hashBuckets[bucketIndex];
	for (; hashNodeIndex; hashNodeIndex = hashNodes[hashNodeIndex].nextIndex)
	{
		if (hashNodes[hashNodeIndex].positionTag == positionTag)
			break;
	}
	return hashNodeIndex;
}

void CAreaGrid::SetFirstAreaIndex(std::vector<unsigned short>& hashBuckets, std::vector<HashNode>& hashNodes, unsigned positionTag, unsigned firstAreaIndex)
{
	unsigned int bucketIndex = positionTag % (hashBuckets.size());
	unsigned int initialhashNodeIndex = hashBuckets[bucketIndex];
	unsigned int hashNodeIndex = initialhashNodeIndex;
	for (; hashNodeIndex; hashNodeIndex = hashNodes[hashNodeIndex].nextIndex)
	{
		if (hashNodes[hashNodeIndex].positionTag == positionTag)
			break;
	}
	if (!hashNodeIndex)
	{
		int newNodeIndex = hashNodes.size();
		hashNodes.resize(hashNodes.size() + 1);
		HashNode& newNode = hashNodes.back();
		newNode.nextIndex = initialhashNodeIndex;
		newNode.positionTag = positionTag;
		hashNodeIndex = newNodeIndex;
		hashBuckets[bucketIndex] = hashNodeIndex;
	}

	HashNode& node = hashNodes[hashNodeIndex];
	node.firstAreaListNodeIndex = firstAreaIndex;
}

void CAreaGrid::Draw()
{
	if (!m_pAreas)
		return;

	I3DEngine* p3DEngine = gEnv->p3DEngine;
	IRenderAuxGeom* pRC = gEnv->pRenderer->GetIRenderAuxGeom();
	pRC->SetRenderFlags(e_Def3DPublicRenderflags);

	ColorF colorsArray[] = {
		ColorF(1.0f, 0.0f, 0.0f, 1.0f),
		ColorF(0.0f, 1.0f, 0.0f, 1.0f),
		ColorF(0.0f, 0.0f, 1.0f, 1.0f),
		ColorF(1.0f, 1.0f, 0.0f, 1.0f),
		ColorF(1.0f, 0.0f, 1.0f, 1.0f),
		ColorF(0.0f, 1.0f, 1.0f, 1.0f),
		ColorF(1.0f, 1.0f, 1.0f, 1.0f),
	};

	for (size_t hashNodeIndex = 1; hashNodeIndex < m_hashNodes.size(); ++hashNodeIndex)
	{
		const HashNode& node = m_hashNodes[hashNodeIndex];
		int gridX = short(node.positionTag & 0xFFFF);
		int gridY = short(node.positionTag >> 16);
		Vec3 vGridCentre = Vec3((float(gridX) + 0.5f) * GRID_SIZE, (float(gridY) + 0.5f) * GRID_SIZE, 0.0f);

		ColorF colour(0, 0, 0, 0);
		float divisor = 0;
		for (int areaListNodeIndex = node.firstAreaListNodeIndex; areaListNodeIndex; areaListNodeIndex = m_areaListNodes[areaListNodeIndex].nextIndex)
		{
			const AreaListNode& areaListNode = m_areaListNodes[areaListNodeIndex];
			ColorF areaColour = colorsArray[areaListNode.areaIndex % (sizeof(colorsArray) / sizeof(ColorB))];
			colour += areaColour;
			++divisor;
		}
		if (divisor)
			colour /= divisor;
		ColorB colourB = colour;

		Vec3 points[] = {
			vGridCentre + Vec3(-GRID_SIZE * 0.5f, -GRID_SIZE * 0.5f, 0.0f),
			vGridCentre + Vec3(+GRID_SIZE * 0.5f, -GRID_SIZE * 0.5f, 0.0f),
			vGridCentre + Vec3(+GRID_SIZE * 0.5f, +GRID_SIZE * 0.5f, 0.0f),
			vGridCentre + Vec3(-GRID_SIZE * 0.5f, +GRID_SIZE * 0.5f, 0.0f)
		};
		enum {NUM_POINTS = sizeof(points) / sizeof(points[0])};
		for (int i = 0; i < NUM_POINTS; ++i)
			points[i].z = p3DEngine->GetTerrainElevation(points[i].x, points[i].y);
		pRC->DrawTriangle(points[0], colourB, points[1], colourB, points[2], colourB);
		pRC->DrawTriangle(points[2], colourB, points[3], colourB, points[0], colourB);
	}
}
