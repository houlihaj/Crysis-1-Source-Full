/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
File name:   CAILightManager.cpp
$Id$
Description: Keeps track of the logical light level in the game world and
						 provides services to make light levels queries.

-------------------------------------------------------------------------
History:
- 2007				: Created by Mikko Mononen

*********************************************************************/

#ifndef _AILIGHTMANAGER_H_
#define _AILIGHTMANAGER_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include "IAISystem.h"
#include "CAISystem.h"
#include "IRenderer.h"
#include "IRenderAuxGeom.h"	
#include "StlUtils.h"

class CAILightManager
{
public:
	CAILightManager();
	~CAILightManager();

	void Reset();
	void Update(bool forceUpdate);
	void DebugDraw(IRenderer* pRenderer);
	void OnObjectRemoved(IAIObject* pObject);
	void Serialize( TSerialize ser, CObjectTracker& objectTracker );

	void DynOmniLightEvent(const Vec3& pos, float radius, EAILightEventType type, CAIActor* pShooter, float time);
	void DynSpotLightEvent(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightEventType type, CAIActor* pShooter, float time);

	//Returns light level at slecified location.
	EAILightLevel	GetLightLevelAt(const Vec3& pos, const CAIActor* pAgent = 0, bool* outUsingCombatLight = 0);

private:

	void DebugDrawArea(IRenderer* pRenderer, const ListPositions& poly, float zmin, float zmax, ColorB col);
	void UpdateTimeOfDay();
	void UpdateLights();

	struct SAIDynLightSource
	{
		SAIDynLightSource(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightLevel level, EAILightEventType type, CAIActor* pShooter, CAIObject* pAttrib, float t) :
			pos(pos), dir(dir), radius(radius), fov(fov), level(level), type(type), pShooter(pShooter), pAttrib(pAttrib), t(0), tmax(t) {}
		SAIDynLightSource() :
			pos(0,0,0), dir(0,1,0), radius(0), fov(0), level(AILL_NONE), type(AILE_GENERIC), pShooter(NULL), pAttrib(NULL), t(0), tmax(t) {}
		void Serialize( TSerialize ser, CObjectTracker& objectTracker );
		Vec3 pos;
		Vec3 dir;
		float radius;
		float fov;
		EAILightLevel level;
		CAIActor* pShooter;
		CAIObject* pAttrib;
		EAILightEventType type;
		float t;
		float tmax;
	};
	std::vector<SAIDynLightSource> m_dynLights;

	struct SAILightSource
	{
		SAILightSource(const Vec3& pos, float radius, EAILightLevel level) :
			pos(pos), radius(radius), level(level)
		{
			// Empty
		}

		Vec3 pos;
		float radius;
		EAILightLevel level;
	};

	std::vector<SAILightSource> m_lights;
	bool m_updated;
	EAILightLevel	m_ambientLight;

	CTimeValue	m_lastUpdateTime;
};


#endif