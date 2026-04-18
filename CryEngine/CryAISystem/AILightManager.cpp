/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
File name:   CAILightManager.h
$Id$
Description: Keeps track of the logical light level in the game world and
provides services to make light levels queries.

-------------------------------------------------------------------------
History:
- 2007				: Created by Mikko Mononen

*********************************************************************/

#include "StdAfx.h"
#include "IAISystem.h"
#include "CAISystem.h"
#include "IRenderer.h"
#include "IRenderAuxGeom.h"	
#include "StlUtils.h"
#include "AILightManager.h"
#include "CryPodArray.h"
#include "AIDebugDrawHelpers.h"
#include <float.h>


static char g_szLightLevels[4][8] = 
{
	"Default",
	"Light",
	"Medium",
	"Dark",
};

static char g_szLightType[4][16] = 
{
	"Generic",
	"Muzzle Flash",
	"Flash Light",
	"Laser",
};


//===================================================================
// CAILightManager
//===================================================================
CAILightManager::CAILightManager() :
	m_ambientLight(AILL_LIGHT),
	m_updated(false)
{
}

//===================================================================
// ~CAILightManager
//===================================================================
CAILightManager::~CAILightManager()
{
}

//===================================================================
// Reset
//===================================================================
void CAILightManager::Reset()
{
	m_ambientLight = AILL_LIGHT;

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		if (m_dynLights[i].pAttrib)
		{
			GetAISystem()->RemoveObject(m_dynLights[i].pAttrib);
			m_dynLights[i].pAttrib = 0;
		}
	}
	m_dynLights.clear();
}

//===================================================================
// OnObjectRemoved
//===================================================================
void CAILightManager::OnObjectRemoved(IAIObject* pObject)
{
	CAIActor* pActor = pObject->CastToCAIActor();
	if (!pActor)
		return;

	for (unsigned i = 0; i < m_dynLights.size(); )
	{
		if (m_dynLights[i].pShooter == pActor)
		{
			if (m_dynLights[i].pAttrib)
			{
				GetAISystem()->RemoveObject(m_dynLights[i].pAttrib);
				m_dynLights[i].pAttrib = 0;
			}
			m_dynLights[i] = m_dynLights.back();
			m_dynLights.pop_back();
		}
		else
			++i;
	}
}

//===================================================================
// DynOmniLightEvent
//===================================================================
void CAILightManager::DynOmniLightEvent(const Vec3& pos, float radius, EAILightEventType type, CAIActor* pShooter, float time)
{
	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];
		if (pShooter && light.pShooter == pShooter && light.fov < 0)
		{
			light.pos = pos;
			light.radius = radius;
			light.type = type;
			light.t = 0;
			return;
		}
	}
	m_dynLights.push_back(SAIDynLightSource(pos, Vec3(ZERO), radius, -1, AILL_LIGHT, type, pShooter, 0, time));
}

//===================================================================
// DynSpotLightEvent
//===================================================================
void CAILightManager::DynSpotLightEvent(const Vec3& pos, const Vec3& dir, float radius, float fov, EAILightEventType type, CAIActor* pShooter, float time)
{
	if (fov <= 0.0f)
		return;

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];
		if (pShooter && light.pShooter == pShooter && light.fov > 0)
		{
			light.pos = pos;
			light.dir = dir;
			light.radius = radius;
			light.fov = fov;
			light.t = 0;
			if (light.pAttrib)
			{
				if (type == AILE_FLASH_LIGHT)
					light.pAttrib->SetPos(pos + dir * radius/2);
				else
					light.pAttrib->SetPos(pos + dir * radius);
			}
			return;
		}
	}

	CAIObject* pAttrib = 0;
	if (pShooter && (type == AILE_LASER || type == AILE_FLASH_LIGHT))
	{
		if (pShooter->GetType() == AIOBJECT_PLAYER)
		{
			// Since checking the attributes is really expensive, create them only for the player.
			pAttrib = (CAIObject*)GetAISystem()->CreateAIObject(AIOBJECT_ATTRIBUTE, pShooter);
			static int nameCounter = 0;
			char szName[256];
			_snprintf(szName, 256,"Light of %s %d", pShooter->GetName(), nameCounter++);
			pAttrib->SetName(szName);
		}

	}

	m_dynLights.push_back(SAIDynLightSource(pos, dir, radius, fov, AILL_LIGHT, type, pShooter, pAttrib, time));
}

//===================================================================
// Update
//===================================================================
void CAILightManager::Update(bool forceUpdate)
{
	CTimeValue t = GetAISystem()->GetFrameStartTime();
	float dt = (t - m_lastUpdateTime).GetSeconds();

//	if (dt > 0.25f || forceUpdate)
	{
		m_lastUpdateTime = t;

		// Decay dyn lights.
		for (unsigned i = 0; i < m_dynLights.size(); )
		{
			SAIDynLightSource& light = m_dynLights[i];
			light.t += dt;
			if (light.t > light.tmax)
			{
				if (m_dynLights[i].pAttrib)
				{
					GetAISystem()->RemoveObject(m_dynLights[i].pAttrib);
					m_dynLights[i].pAttrib = 0;
				}
				m_dynLights[i] = m_dynLights.back();
				m_dynLights.pop_back();
			}
			else
				++i;
		}

		UpdateTimeOfDay();
		UpdateLights();

		m_updated = true;
	}

}

//===================================================================
// DebugDraw
//===================================================================
void CAILightManager::DebugDraw(IRenderer* pRenderer)
{
	int mode = GetAISystem()->m_cvDebugDrawLightLevel->GetIVal();
	if (mode == 0)
		return;

	static CTimeValue lastTime(0.0f);
	CTimeValue t = GetAISystem()->GetFrameStartTime();
	float dt = (t - lastTime).GetSeconds();
	if (!m_updated && dt < 0.0001f)
		Update(true);
	m_updated = false;
	lastTime = t;

	float white[4] = {1,1,1,1};

	pRenderer->Draw2dLabel(20, pRenderer->GetHeight() - 100, 2.0f, white, false, "Ambient: %s", g_szLightLevels[(int)m_ambientLight]);
	pRenderer->Draw2dLabel(20, pRenderer->GetHeight() - 75, 1.5f, white, false, "Ent Lights: %d", m_lights.size());

	if (mode == 2)
	{
		// Draw the player pos too.
		if (GetAISystem()->GetPlayer())
		{
			Vec3 pos = GetAISystem()->GetPlayer()->GetPos();
			if (pos.IsZero())
				pos = GetDebugCameraPos();
			EAILightLevel playerLightLevel = GetLightLevelAt(pos);
			pRenderer->Draw2dLabel(20, pRenderer->GetHeight() - 35, 1.5f, white, false, "Player: %s", g_szLightLevels[(int)playerLightLevel]);
			pRenderer->GetIRenderAuxGeom()->DrawLine(pos - Vec3(10,0,0), ColorB(255,255,255), pos + Vec3(10,0,0), ColorB(255,255,255));
			pRenderer->GetIRenderAuxGeom()->DrawLine(pos - Vec3(0,10,0), ColorB(255,255,255), pos + Vec3(0,10,0), ColorB(255,255,255));
			pRenderer->GetIRenderAuxGeom()->DrawLine(pos - Vec3(0,0,10), ColorB(255,255,255), pos + Vec3(0,0,10), ColorB(255,255,255));
		}
	}


	ColorB	lightCols[4] = { ColorB(0,0,0),  ColorB(255,255,255), ColorB(255,196,32), ColorB(16,96,128) };

	// Draw light bases and connections.
	for (unsigned i = 0, ni = m_lights.size(); i < ni; ++i)
	{
		SAILightSource& light = m_lights[i];
		ColorB c = lightCols[(int)light.level];
		pRenderer->GetIRenderAuxGeom()->DrawSphere(light.pos, 0.2f, c);
	}

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];
		ColorB c = lightCols[(int)light.level];
		pRenderer->GetIRenderAuxGeom()->DrawSphere(light.pos, 0.2f, c);
	}


	for (unsigned i = 0, ni = m_lights.size(); i < ni; ++i)
	{
		SAILightSource& light = m_lights[i];
		ColorB c = lightCols[(int)light.level];
		ColorB ct(c);
		ct.a = 64;
		pRenderer->GetIRenderAuxGeom()->DrawSphere(light.pos, light.radius, ct);
		DebugDrawWireSphere(pRenderer, light.pos, light.radius, c);

		pRenderer->DrawLabel(light.pos, 1.1f, "%s", g_szLightLevels[(int)light.level]);
	}

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];
		ColorB c = lightCols[(int)light.level];
		ColorB ct(c);
		float a = 1.0f - sqr(1.0f - light.t / light.tmax);
		ct.a = (uint8)(32 + 32 * a);

		if (light.fov < 0)
		{
			// Omni
			pRenderer->GetIRenderAuxGeom()->DrawSphere(light.pos, light.radius, ct);
			DebugDrawWireSphere(pRenderer, light.pos, light.radius, c);
		}
		else
		{
			// Spot
			float coneRadius = sinf(light.fov) * light.radius;
			float coneHeight = cosf(light.fov) * light.radius;
			Vec3 conePos = light.pos + light.dir * coneHeight;
			pRenderer->GetIRenderAuxGeom()->DrawLine(light.pos, c, light.pos + light.dir*light.radius, c);
			pRenderer->GetIRenderAuxGeom()->DrawCone(conePos, -light.dir, coneRadius, coneHeight, ct);
			DebugDrawWireFOVCone(pRenderer, light.pos, light.dir, light.radius, light.fov, c);
		}

		pRenderer->DrawLabel(light.pos, 1.1f, "DYN %s\n%s", g_szLightLevels[(int)light.level], g_szLightType[(int)light.type]);

		if (light.pAttrib)
		{
			pRenderer->GetIRenderAuxGeom()->DrawSphere(light.pAttrib->GetPos(), 0.15f, c);
			pRenderer->DrawLabel(light.pAttrib->GetPos(), 1.1f, "Attrib");
		}
	}


	SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags(oldFlags);
//	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

	// Navigation modifiers
	for (SpecialAreaMap::const_iterator di = GetAISystem()->GetSpecialAreas().begin(), dend = GetAISystem()->GetSpecialAreas().end(); di != dend; ++di)
	{
		const SpecialArea& sa = di->second;
		if (sa.lightLevel == AILL_NONE)
			continue;
		if (sa.fHeight < 0.0001f)
			continue;
		DebugDrawArea(pRenderer, sa.GetPolygon(), sa.fMinZ, sa.fMaxZ, lightCols[(int)sa.lightLevel]);
		Vec3 pos(sa.GetPolygon().front());
		pos.z = (sa.fMinZ + sa.fMaxZ)/2;
		pRenderer->DrawLabel(pos, 1.1f, "%s\n%s", di->first.c_str(), g_szLightLevels[(int)sa.lightLevel]);
	}

	// AIShapes
	for (ShapeMap::const_iterator di = GetAISystem()->GetGenericShapes().begin(), dend = GetAISystem()->GetGenericShapes().end(); di != dend; ++di)
	{
		const SShape& sa = di->second;
		if (sa.lightLevel == AILL_NONE)
			continue;
		if (sa.height < 0.0001f)
			continue;
		DebugDrawArea(pRenderer, sa.shape, sa.aabb.min.z, sa.aabb.min.z + sa.height, lightCols[(int)sa.lightLevel]);
		Vec3 pos(sa.shape.front());
		pos.z = sa.aabb.min.z + sa.height/2;
		pRenderer->DrawLabel(pos, 1.1f, "%s\n%s", di->first.c_str(), g_szLightLevels[(int)sa.lightLevel]);
	}

	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
}

//===================================================================
// DebugDrawArea
//===================================================================
void CAILightManager::DebugDrawArea(IRenderer* pRenderer, const ListPositions& poly, float zmin, float zmax, ColorB col)
{
	static std::vector<Vec3> verts;
	unsigned npts = poly.size();
	verts.resize(npts);
	unsigned i = 0;
	for (ListPositions::const_iterator it = poly.begin(), end = poly.end(); it != end; ++it)
		verts[i++] = *it;

	for (i = 0; i < npts; ++i)
		verts[i].z = zmin;
	pRenderer->GetIRenderAuxGeom()->DrawPolyline(&verts[0], npts, true, col, 1.0f);

	for (i = 0; i < npts; ++i)
		verts[i].z = zmax;
	pRenderer->GetIRenderAuxGeom()->DrawPolyline(&verts[0], npts, true, col, 1.0f);

	for (i = 0; i < npts; ++i)
	{
		pRenderer->GetIRenderAuxGeom()->DrawLine(verts[i], col,
			Vec3(verts[i].x, verts[i].y, zmin), col, 1.0f);
	}

	ColorB colTrans(col);
	colTrans.a = 64;

	for (i = 0; i < npts; ++i)
	{
		unsigned j = (i+1) % npts;
		Vec3 vi(verts[i].x, verts[i].y, zmin);
		Vec3 vj(verts[j].x, verts[j].y, zmin);
		pRenderer->GetIRenderAuxGeom()->DrawTriangle(verts[i], colTrans, vi, colTrans, vj, colTrans);
		pRenderer->GetIRenderAuxGeom()->DrawTriangle(verts[i], colTrans, vj, colTrans, verts[j], colTrans);
	}
}

//===================================================================
// GetLightLevelAt
//===================================================================
EAILightLevel CAILightManager::GetLightLevelAt(const Vec3& pos, const CAIActor* pAgent, bool* outUsingCombatLight)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	// Find ambient light level.

	// Start from time-of-day.
	EAILightLevel	lightLevel = m_ambientLight;

	// Override from Navigation modifiers
	for (SpecialAreaMap::const_iterator di = GetAISystem()->GetSpecialAreas().begin(), dend = GetAISystem()->GetSpecialAreas().end(); di != dend; ++di)
	{
		const SpecialArea& sa = di->second;
		if (sa.lightLevel == AILL_NONE)
			continue;
		if (sa.fHeight < 0.0001f)
			continue;

		AABB aabb = sa.GetAABB();
		aabb.min.z = sa.fMinZ;
		aabb.max.z = sa.fMaxZ;
		if (Overlap::Point_Polygon2D(pos, sa.GetPolygon(), &aabb))
			lightLevel = sa.lightLevel;
	}

	// Override from AIShapes
	for (ShapeMap::const_iterator di = GetAISystem()->GetGenericShapes().begin(), dend = GetAISystem()->GetGenericShapes().end(); di != dend; ++di)
	{
		const SShape& sa = di->second;
		if (sa.lightLevel == AILL_NONE)
			continue;
		if (sa.height < 0.0001f)
			continue;
		if (sa.IsPointInsideShape(pos, true))
			lightLevel = sa.lightLevel;
	}

	// Based on the ambient light level.
	for (unsigned i = 0, ni = m_lights.size(); i < ni; ++i)
	{
		SAILightSource& light = m_lights[i];

		Vec3 diff = pos - light.pos;
		if (diff.GetLengthSquared() > sqr(light.radius))
			continue;

		if ((int)light.level < (int)lightLevel)
			lightLevel = light.level;
	}

	if (outUsingCombatLight)
		*outUsingCombatLight = false;

	for (unsigned i = 0, ni = m_dynLights.size(); i < ni; ++i)
	{
		SAIDynLightSource& light = m_dynLights[i];

		if (outUsingCombatLight)
		{
			if (light.pShooter == pAgent && (light.type == AILE_FLASH_LIGHT || light.type == AILE_LASER))
				*outUsingCombatLight = true;
		}

		Vec3 diff = pos - light.pos;
		if (diff.GetLengthSquared() > sqr(light.radius))
			continue;

		if (light.fov > 0)
		{
			diff.Normalize();
			float dot = light.dir.Dot(diff);
			const float thr = cosf(light.fov);
			if (dot < thr)
				continue;
		}

		if ((int)light.level < (int)lightLevel)
			lightLevel = light.level;
	}

	return lightLevel;
}

//===================================================================
// UpdateLights
//===================================================================
void CAILightManager::UpdateLights()
{
	m_lights.clear();

	const short LIGHTSPOT_LIGHT = 401;
	const short LIGHTSPOT_MEDIUM = 402;
	const short LIGHTSPOT_DARK = 403;

	short types[3] = { LIGHTSPOT_LIGHT, LIGHTSPOT_MEDIUM, LIGHTSPOT_DARK };
	EAILightLevel levels[3] = { AILL_LIGHT, AILL_MEDIUM, AILL_DARK };
	
	// Get light anchors
	for (unsigned i = 0; i < 3; ++i)
	{
		const short type = types[i];
		for (AIObjects::iterator ai = GetAISystem()->m_Objects.find(type), end = GetAISystem()->m_Objects.end(); ai != end && ai->first == type; ++ai)
		{
			CAIObject* pObj = ai->second;
			if (!pObj->IsEnabled()) continue;
			m_lights.push_back(SAILightSource(pObj->GetPos(), pObj->GetRadius(), levels[i]));
		}
	}

	// Associate lights to areas
/*	for (unsigned i = 0, ni = m_lights.size(); i < ni; ++i)
	{
		// Override from Navigation modifiers
		for (SpecialAreaMap::const_iterator di = GetAISystem()->GetSpecialAreas().begin(), dend = GetAISystem()->GetSpecialAreas().end(); di != dend; ++di)
		{
			const SpecialArea& sa = di->second;
			if (sa.lightLevel == AILL_NONE)
				continue;
			if (sa.fHeight < 0.0001f)
				continue;

			AABB aabb = sa.GetAABB();
			aabb.min.z = sa.fMinZ;
			aabb.max.z = sa.fMaxZ;
			if (Overlap::Point_Polygon2D(m_lights[i].pos, sa.GetPolygon(), &aabb))
				m_lights[i].pSa = &sa;
		}

		// Override from AIShapes
		for (ShapeMap::const_iterator di = GetAISystem()->GetGenericShapes().begin(), dend = GetAISystem()->GetGenericShapes().end(); di != dend; ++di)
		{
			const SShape& sa = di->second;
			if (sa.lightLevel == AILL_NONE)
				continue;
			if (sa.height < 0.0001f)
				continue;
			if (sa.IsPointInsideShape(pos, true))
			{
				lightLevel = sa.lightLevel;
			}
		}
	}*/




/*	const PodArray<ILightSource*>* pLightEnts = gEnv->p3DEngine->GetLightEntities();

	for (unsigned i = 0, ni = pLightEnts->size(); i < ni; ++i)
	{
		ILightSource* pLightSource = *pLightEnts->Get(i);
		CDLight& light = pLightSource->GetLightProperties();
		if ((light.m_Flags & DLF_FAKE) || (light.m_Flags & DLF_DIRECTIONAL))
			continue;

		float illum = light.m_Color.r * 0.3f + light.m_Color.r * 0.59f + light.m_Color.r * 0.11f + light.m_SpecMult * 0.1f;

		EAILightLevel level;
		if (illum > 0.4f)
			level = AILL_LIGHT;
		else if (illum > 0.1f)
			level = AILL_MEDIUM;
		else
			level = AILL_DARK;

		if ((light.m_Flags & DLF_PROJECT) && light.m_fLightFrustumAngle < 90.0f)
		{
			// Spot
			Vec3 dir = light.m_ObjMatrix.TransformVector(Vec3(1,0,0));
			float fov = DEG2RAD(light.m_fLightFrustumAngle) * 0.7f;
			float radius = light.m_fRadius * 0.9f;
			m_lights.push_back(SAILightSource(light.m_Origin, dir.GetNormalized(),
				radius, fov, level, illum));
		}
		else
		{
			// Omni
			float radius = light.m_fRadius * 0.9f;
			m_lights.push_back(SAILightSource(light.m_Origin, radius, level, illum));
		}
	}*/
}

//===================================================================
// UpdateTimeOfDay
//===================================================================
void CAILightManager::UpdateTimeOfDay()
{
	ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
	if (!pTOD)
	{
		m_ambientLight = AILL_LIGHT;
		return;
	}

	float hour = pTOD->GetTime();

	if (hour < 5.8f)
	{
		m_ambientLight = AILL_DARK;
	}
	else if (hour < 6.15f)
	{
		m_ambientLight = AILL_MEDIUM;
	}
	else if (hour < 18.0f)
	{
		m_ambientLight = AILL_LIGHT;
	}
	else if (hour < 19.5f)
	{
		m_ambientLight = AILL_MEDIUM;
	}
	else
	{
		m_ambientLight = AILL_DARK;
	}
}


// save/load
//===================================================================
void CAILightManager::SAIDynLightSource::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	ser.Value("pos", pos);
	ser.Value("dir", dir);
	ser.Value("radius", radius);
	ser.Value("fov", fov);
	ser.EnumValue("level", level, AILL_NONE, AILL_LAST);
	objectTracker.SerializeObjectPointer(ser, "pShooter", pShooter, false);
	objectTracker.SerializeObjectPointer(ser, "pAttrib", pAttrib, false);
	ser.EnumValue("type", type, AILE_GENERIC, AILE_LAST);
	ser.Value("t", t);
	ser.Value("tmax", tmax);
}

// save/load
//===================================================================
void CAILightManager::Serialize( TSerialize ser, CObjectTracker& objectTracker )
{
	ser.BeginGroup( "AILightManager" );
		int	dynLightCount(m_dynLights.size());
		ser.Value("DinLightSize", dynLightCount);
		if(ser.IsReading())
		{
			m_dynLights.resize(dynLightCount);
		}
		char AILightGrpName[256];
		for (unsigned i = 0; i < m_dynLights.size(); ++i)
		{
			sprintf(AILightGrpName, "AIDynLightSource-%d", i);
			ser.BeginGroup(AILightGrpName);
			{
				m_dynLights[i].Serialize(ser, objectTracker);
			}
			ser.EndGroup();
		}

		ser.Value("m_updated", m_updated);
		ser.EnumValue("m_ambientLight", m_ambientLight, AILL_NONE, AILL_LAST);
		ser.Value("m_lastUpdateTime", m_lastUpdateTime);
	ser.EndGroup();
}