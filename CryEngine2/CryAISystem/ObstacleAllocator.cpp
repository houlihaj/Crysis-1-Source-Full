/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   ObstacleAllocator.cpp
	$Id$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 2:6:2005   16:26 : Created by Kirill Bulatsev

*********************************************************************/

#include "StdAfx.h"
#include "CAISystem.h"
#include "AIObject.h"
#include "ObstacleAllocator.h"
#include "AIObject.h"
#include "Leader.h"
#include <ISerialize.h>
#include "ObjectTracker.h"



//
//-------------------------------------------------------------------------------------------------------------
const ObstacleData	CObstacleAllocator::GetObstacleData(int index) const
{
	return GetAISystem()->m_VertexList.GetVertex(index); 
}

//
//-------------------------------------------------------------------------------------------------------------
CObstacleRef CObstacleAllocator::GetUnitObstacle(CAIObject* unit) const
{
	MapUnitObstacles::const_iterator it = m_UnitObstacles.find(unit);
	if (it != m_UnitObstacles.end())
		return it->second;
	return CObstacleRef();
}


//
//----------------------------------------------------------------------------------------------------
CObstacleRef CObstacleAllocator::GetOrAllocUnitObstacle(CAIObject* unit, const Vec3& destination)
{
	MapUnitObstacles::iterator it = m_UnitObstacles.find(unit);
	if (it != m_UnitObstacles.end())
		return it->second;
	return AllocObstacle(unit, destination);
}


//
//----------------------------------------------------------------------------------------------------
CObstacleRef CObstacleAllocator::ReallocUnitObstacle(CAIObject* unit, const Vec3& destination,  bool allowSame /*= true*/,float preferredDistance, const Vec3& searchPosition, bool bCollidableOnly)
{
	if (allowSame)
	{
		FreeObstacle(unit);
		CObstacleRef obstacle = AllocObstacle((searchPosition.IsZero() ? unit->GetPos(): searchPosition), destination,bCollidableOnly, preferredDistance);
		if (obstacle)
		{
			m_UnitObstacles[unit] = obstacle;
		}		
		return obstacle;
	}
	else
	{
		CObstacleRef obstacle = AllocObstacle((searchPosition.IsZero() ? unit->GetPos(): searchPosition), destination,bCollidableOnly, preferredDistance);
		if (obstacle)
		{
			FreeObstacle(unit);
			m_UnitObstacles[unit] = obstacle;
		}
		//			else
		//				obstacle = GetOrAllocUnitObstacle(unit, destination);
		return obstacle;
	}
}


//
//----------------------------------------------------------------------------------------------------
void CObstacleAllocator::FreeObstacle(CAIObject* unit)
{
	MapUnitObstacles::iterator find = m_UnitObstacles.find(unit);
	if (find != m_UnitObstacles.end())
	{
		CGraph::FreeObstacle(find->second);
		m_UnitObstacles.erase(find);
	}
}


//
//----------------------------------------------------------------------------------------------------
CObstacleRef CObstacleAllocator::AllocObstacle(CAIObject* unit, const Vec3& destination, bool bCollidableOnly, float preferredDistance)
{
	CObstacleRef obstacle = AllocObstacle(unit->GetPos(), destination,bCollidableOnly, preferredDistance);
	if (obstacle)
		m_UnitObstacles[unit] = obstacle;
	return obstacle;
}


//
//----------------------------------------------------------------------------------------------------
CObstacleRef CObstacleAllocator::AllocObstacle(const Vec3& from, const Vec3& to, bool bCollidableOnly, float preferredDistance)
{
	// preferred position
	// TODO: fix this after CXP
	Vec3 preferedPos = m_pLeader->GetPreferedPos();
	if ( preferedPos == to )
		preferredDistance = 0;
	CObstacleCostFtor costFtor(from, to, preferedPos, preferredDistance);
	costFtor.SetSearchDistance(m_fInitSearchDistance);
	CIncrementalObstacleFinder< CObstacleCostFtor > finder(costFtor,bCollidableOnly);
	costFtor.SetSearchDistance(m_fSearchDistance);
	finder.m_bKnownEnemyPosition = m_bKnownEnemyPosition;
	while (1)
	{
		CObstacleRef obstacle = finder.GetNextObstacle();
		if (!obstacle)
			return obstacle;
		else
			if (CGraph::AllocObstacle(obstacle))
				return obstacle;
	};
}



//
//----------------------------------------------------------------------------------------------------
void		CObstacleAllocator::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{

//	SetUnits					m_Units;
/*
	if(ser.BeginOptionalGroup("m_Units", !m_Units.empty()))
	{
		uint32 count( m_Units.size() );
		ser.Value("count", count);
		while(count--)
		{
			CAIObject *pUnit(NULL);
			if(ser.IsWriting())
			{
				MapUnitObstacles::iterator itr(m_UnitObstacles.begin());
				std::advance(itr,count);
				pUnit = itr->first;
				obstRef = itr->second;
			}
			objectTracker.SerializeObjectPointer(ser, "pUnit", pUnit, false);
			obstRef.Serialize(ser, objectTracker);
			if(ser.IsReading())
				m_UnitObstacles[pUnit] = obstRef;
		}
		ser.EndGroup();
	}	
*/
	if(ser.BeginOptionalGroup("m_UnitObstacles", !m_UnitObstacles.empty()))
	{
		uint32 count( m_UnitObstacles.size() );
		ser.Value("count", count);
		if(ser.IsReading())
			m_UnitObstacles.clear();
		while(count--)
		{
			CAIObject *pUnit(NULL);
			CObstacleRef obstRef;
			if(ser.IsWriting())
			{
				MapUnitObstacles::iterator itr(m_UnitObstacles.begin());
				std::advance(itr,count);
				pUnit = itr->first;
				obstRef = itr->second;
			}
			objectTracker.SerializeObjectPointer(ser, "pUnit", pUnit, false);
			obstRef.Serialize(ser, objectTracker);
			if(ser.IsReading())
				m_UnitObstacles[pUnit] = obstRef;
		}
		ser.EndGroup();
	}

	ser.Value("m_fInitSearchDistance", m_fInitSearchDistance);
	ser.Value("m_fSearchDistance", m_fSearchDistance);
	ser.Value("m_bKnownEnemyPosition",  m_bKnownEnemyPosition);
}

//
//----------------------------------------------------------------------------------------------------

