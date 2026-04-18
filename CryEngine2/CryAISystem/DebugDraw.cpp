/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   DebugDraw.cpp
$Id$
Description: move all the debug drawing functionality from AISystem. Need to clean it up eventually and 
make nice debug output functions, instead of one huge.

-------------------------------------------------------------------------
History:
- 29:11:2004   19:02 : Created by Kirill Bulatsev

*********************************************************************/

#include "StdAfx.h"

// AI includes
#include "CAISystem.h"
#include "CTriangulator.h"
#include "Graph.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "GoalPipe.h"
#include "GoalOp.h"
#include "AIPlayer.h"
#include "PipeUser.h"
#include "AIAutoBalance.h"
#include "Leader.h"
#include "VolumeNavRegion.h"
#include "FlightNavRegion.h"
#include "WaypointHumanNavRegion.h"
#include "SmartObjects.h"
#include "AILog.h"
#include "AICollision.h"
#include "TrackPattern.h"
#include "TickCounter.h"
#include "TriangularNavRegion.h"
#include "AIDebugDrawHelpers.h"
#include "PathFollower.h"
#include "Shape.h"
// system includes
#include "IPhysics.h"
#include "IEntitySystem.h"
#include "IScriptSystem.h"
#include "I3DEngine.h"
#include "ILog.h"
#include "Cry_Math.h"
#include "ISystem.h"
#include "ITimer.h"
#include "IConsole.h"
#include "IRenderer.h"
#include "IRenderAuxGeom.h"	

#include <algorithm>
#include <stdio.h>
#include <map>

void CAISystem::DebugDrawRecorderRange(IRenderer *pRenderer) const
{
	if(fabsf(m_recorderDrawEnd - m_recorderDrawStart) < 0.0001f)
		return;

	// output all puppets
	ColorB	blue(13,27,73,220);
	ColorB	lightBlue(240,245,255,220);
	ColorB	red(167,0,0);
	ColorB	redTrans(167,0,0,128);

	Vec3	camPos(GetDebugCameraPos());
	const float drawRangeSq = sqr(40.0f);

	AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET);
	for (;ai!=m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;
		CPuppet* puppet = ai->second->CastToCPuppet();
		IAIDebugRecord*	pRecord(puppet->GetAIDebugRecord());
		if(!pRecord)
			continue;
		IAIDebugStream*	pStream(pRecord->GetStream(IAIRecordable::E_AGENTPOS));
		if(!pStream)
			continue;
		IAIDebugStream*	pStreamTarget(pRecord->GetStream(IAIRecordable::E_ATTENTIONTARGETPOS));
		if(!pStreamTarget)
			continue;

		// Draw range
		if(m_recorderDrawStart < pStream->GetEndTime() && m_recorderDrawEnd > pStream->GetStartTime())
		{
			pStream->Seek(m_recorderDrawStart);
			float	curTime;
			Vec3* pos = (Vec3*)(pStream->GetCurrent(curTime));

			int	j = 0;
			while(curTime <= m_recorderDrawEnd && pStream->GetCurrentIdx() < pStream->GetSize())
			{
				float nextTime;
				Vec3*	nextPos = (Vec3*)(pStream->GetNext(nextTime));

				if(pos && nextPos)
				{
					Vec3	p(*pos);
					Vec3	n(*nextPos);
					if((j & 1) == 0)
						pRenderer->GetIRenderAuxGeom()->DrawLine(p, blue, n, blue);
					else
						pRenderer->GetIRenderAuxGeom()->DrawLine(p, lightBlue, n, lightBlue);
				}
/*				if(pos)
				{
					Vec3	p(*pos);
					pRenderer->GetIRenderAuxGeom()->DrawSphere(p, 0.025f, blue);
				}*/

				curTime = nextTime;
				pos = nextPos;
				++j;
			}

			// Draw cursor pos
			pStream->Seek(m_recorderDrawCursor);
			pos = (Vec3*)(pStream->GetCurrent(curTime));
			if(pos)
			{
				if(Distance::Point_PointSq(*pos, camPos) < drawRangeSq)
				{
					Vec3	p(*pos);
					pRenderer->GetIRenderAuxGeom()->DrawSphere(p, 0.15f, red);
					pRenderer->DrawLabel(*pos + Vec3(0,0,0.8f),1,"%s", puppet->GetName());

					if(curTime >= pStreamTarget->GetStartTime() && curTime < pStreamTarget->GetEndTime())
					{
						pStreamTarget->Seek(m_recorderDrawCursor);
						Vec3*	targetPos = (Vec3*)(pStreamTarget->GetCurrent(curTime));
						if(targetPos)
						{
							Vec3	dir = (*targetPos) - (*pos);
							Vec3	t(*targetPos);
							dir.NormalizeSafe();
							pRenderer->GetIRenderAuxGeom()->DrawLine(p, redTrans, t, redTrans);
							pRenderer->GetIRenderAuxGeom()->DrawCone(t - dir * 0.4f, dir, 0.1f, 0.4f, redTrans);
						}
					}
				}
			}
		}
	}

	CAIPlayer*	player = (CAIPlayer*)GetPlayer();
	if(player)
	{
		IAIDebugRecord*	pRecord(player->GetAIDebugRecord());
		if(!pRecord)
			return;
		IAIDebugStream*	pStreamPos(pRecord->GetStream(IAIRecordable::E_AGENTPOS));
		if(!pStreamPos)
			return;
		IAIDebugStream*	pStreamHitDamage(pRecord->GetStream(IAIRecordable::E_HIT_DAMAGE));
		if(!pStreamHitDamage)
			return;
		IAIDebugStream*	pStreamDeath(pRecord->GetStream(IAIRecordable::E_DEATH));
		if(!pStreamDeath)
			return;

		float	curTime;

		// Draw range pos
		float	posStartTime = pStreamPos->GetStartTime();
		float	posStartEnd = pStreamPos->GetEndTime();
		pStreamPos->Seek(posStartTime);
		Vec3* pos = (Vec3*)(pStreamPos->GetCurrent(curTime));

		int	j = 0;
		while(curTime <= posStartEnd && pStreamPos->GetCurrentIdx() < pStreamPos->GetSize())
		{
			float nextTime;
			Vec3*	nextPos = (Vec3*)(pStreamPos->GetNext(nextTime));

			if(pos && nextPos)
			{
				if(Distance::Point_PointSq(*pos, camPos) < drawRangeSq)
				{
					Vec3	p(*pos);
					Vec3	n(*nextPos);
					if((j & 1) == 0)
						pRenderer->GetIRenderAuxGeom()->DrawLine(p, blue, n, blue);
					else
						pRenderer->GetIRenderAuxGeom()->DrawLine(p, lightBlue, n, lightBlue);
				}
			}

/*			if(pos)
			{
				if(Distance::Point_PointSq(*pos, camPos) < sqr(100.0f))
				{
					Vec3	p(*pos);
					pRenderer->GetIRenderAuxGeom()->DrawSphere(p, 0.025f, blue);
				}
			}*/

			curTime = nextTime;
			pos = nextPos;
			++j;
		}

		// Draw death spots
		float	deathStartTime = pStreamDeath->GetStartTime();
		float	deathStartEnd = pStreamDeath->GetEndTime();
		pStreamDeath->Seek(deathStartTime);
		char* deathString = (char*)(pStreamDeath->GetCurrent(curTime));

		while(curTime <= deathStartEnd && pStreamDeath->GetCurrentIdx() < pStreamDeath->GetSize())
		{
			float	tmpTime;
			pStreamPos->Seek(curTime);
			Vec3*	pos = (Vec3*)(pStreamPos->GetCurrent(tmpTime));

			float nextTime;
			char*	nextDeathString = (char*)(pStreamDeath->GetNext(nextTime));

			if(pos && deathString)
			{
				if(Distance::Point_PointSq(*pos, camPos) < drawRangeSq)
				{
					Vec3	p(*pos);
					pRenderer->GetIRenderAuxGeom()->DrawCone(p + Vec3(0,0,2), Vec3(0,0,-1), 0.2f, 2.0f, ColorB(0,0,0));
					pRenderer->DrawLabel(*pos + Vec3(0,0,2.3f),1,"%s\n%s", player->GetName(), deathString);
				}
			}

			curTime = nextTime;
			deathString = nextDeathString;
		}

		SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags	newFlags;
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		newFlags.SetDepthWriteFlag(e_DepthWriteOff);
		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

		// Draw hit damage spots
		float	hitStartTime = pStreamHitDamage->GetStartTime();
		float	hitStartEnd = pStreamHitDamage->GetEndTime();
		pStreamHitDamage->Seek(hitStartTime);
		const char* hit = (const char*)(pStreamHitDamage->GetCurrent(curTime));

		Vec3	lastPos(0,0,0);
		int	i = 0;
		int	k = 0;
		float	accDamage = 0.0f;
		float	firstTime = 0.0f;
		while(curTime <= hitStartEnd && pStreamHitDamage->GetCurrentIdx() < pStreamHitDamage->GetSize())
		{
			float	tmpTime;
			pStreamPos->Seek(curTime);
			Vec3*	pos = (Vec3*)(pStreamPos->GetCurrent(tmpTime));

			float nextTime;
			const char*	nextHit = (const char*)(pStreamHitDamage->GetNext(nextTime));

			if(pos && hit)
			{
				// The hit is a string in format "123.0 (mat)", where the number indicates the hit damage
				// and the string in brackets the hit material. Extract the damage value.
				char	tmp[32];
				size_t	len = strlen(hit);
				size_t	n = 0;
				while(n < len && n < 31 && hit[n] != '(')
				{
					tmp[n] = hit[n];
					n++;
				}
				tmp[n] = '\0';
				float	damage = (float)atof(tmp);

				if(Distance::Point_PointSq(*pos, camPos) < drawRangeSq)
				{
					if(Distance::Point_PointSq(*pos, lastPos) < sqr(0.2f))
					{
						++i;
						accDamage += damage;
					}
					else
					{
						if(i > 0)
						{
							// Draw previous
							float	size = 0.05f + ((accDamage) / 100.0f) * 2.0f;
							DebugDrawRangeCircle(pRenderer, lastPos + Vec3(0,0,(k & 3)*0.1f), size, size - 0.01f, ColorB(229, 96, 23, 196), ColorB(0,0,0), false);
//							pRenderer->GetIRenderAuxGeom()->DrawSphere(p, size, ColorB(229, 96, 23, 128));
							if(i > 1)
								pRenderer->DrawLabel(lastPos + Vec3(0,0,0.1f),1,"%.1f (%d)\n%.1fs (%.1fs)", accDamage, i, curTime, curTime - firstTime);
							else
								pRenderer->DrawLabel(lastPos + Vec3(0,0,0.1f),1,"%.1f\n%.1fs", accDamage, curTime);
							k++;
						}
						i = 1;
						firstTime = curTime;
						accDamage = damage;
					}
				}

				lastPos = *pos;
			}
			curTime = nextTime;
			hit = nextHit;
		}
		if(i > 0)
		{
			// Draw previous
			float	size = 0.05f + ((accDamage) / 100.0f) * 2.0f;
			DebugDrawRangeCircle(pRenderer, lastPos + Vec3(0,0,(k & 3)*0.1f), size, size - 0.01f, ColorB(229, 96, 23, 196), ColorB(0,0,0), false);
			//							pRenderer->GetIRenderAuxGeom()->DrawSphere(p, size, ColorB(229, 96, 23, 128));
			if(i > 1)
				pRenderer->DrawLabel(lastPos + Vec3(0,0,0.1f),1,"%.1f (%d)\n%.1fs (%.1fs)", accDamage, i, curTime, curTime - firstTime);
			else
				pRenderer->DrawLabel(lastPos + Vec3(0,0,0.1f),1,"%.1f\n%.1fs", accDamage, curTime);
		}

		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);

		// Draw cursor pos
		pStreamPos->Seek(m_recorderDrawCursor);
		pos = (Vec3*)(pStreamPos->GetCurrent(curTime));
		if(pos)
		{
			Vec3	p(*pos);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(p, 0.25f, ColorB(255,255,255));
			pRenderer->GetIRenderAuxGeom()->DrawCone(p + Vec3(0,0,5), Vec3(0,0,-1), 0.5f, 4.0f, ColorB(255,255,255,220));
			pRenderer->DrawLabel(*pos + Vec3(0,0,0.8f),1,"%s\n%.1fs", player->GetName(), m_recorderDrawCursor);
		}

	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawDamageControlGraph(IRenderer *pRenderer) const
{
	Vec3 camPos(GetDebugCameraPos());

	static std::vector<CPuppet*>	shooters;
	shooters.clear();

	AIObjects::const_iterator aio = m_Objects.find(AIOBJECT_PUPPET);
	for(; aio != m_Objects.end(); ++aio)
	{
		if(aio->first != AIOBJECT_PUPPET)
			break;
		CPuppet* pPuppet = aio->second->CastToCPuppet();
		if(!pPuppet)
			continue;
		if(!pPuppet->IsEnabled())
			continue;
		if(!pPuppet->m_targetDamageHealthThrHistory)
			continue;
		if(Distance::Point_PointSq(camPos, pPuppet->GetPos()) > sqr(150.0f))
			continue;
		shooters.push_back(pPuppet);
	}

	if(shooters.empty())
		return;


	SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags(e_Def2DPublicRenderflags);
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

	const Vec3	u(1,0,0);
	const Vec3	v(0,-1,0);
	const Vec3	w(0,0,1);

	static std::vector<Vec3>	values;

	if(m_cvDebugDrawDamageControl->GetIVal() > 2)
	{
		// Combined graph
		static std::vector<CAIActor*>	targets;
		targets.clear();
		for(unsigned i = 0; i < shooters.size(); ++i)
		{
			CPuppet* pPuppet = shooters[i];
			if(pPuppet->GetAttentionTarget())
			{
				CAIActor* pTargetActor = 0;
				CAIObject* pTarget = (CAIObject*)pPuppet->GetAttentionTarget();
				if(pTarget->IsAgent())
					pTargetActor = pTarget->CastToCAIActor();
				else if(pTarget->GetAssociation() && pTarget->GetAssociation()->IsAgent())
					pTargetActor = pTarget->GetAssociation()->CastToCAIActor();
				if(pTargetActor)
				{
					for(unsigned j = 0; j < targets.size(); ++j)
					{
						if(pTargetActor == targets[j])
						{
							pTargetActor = 0;
							break;
						}
					}
					if(pTargetActor)
						targets.push_back(pTargetActor);
				}
			}
		}

		Vec3	u(1,0,0);
		Vec3	v(0,-1,0);
		Vec3	w(0,0,1);

		float	sizex = 0.7f;
		float	sizey = 0.2f;

		Vec3	orig = Vec3(0.15f, 1-0.05f, 0);

		// dim BG
		ColorB	bg1(0,0,0,128); ColorB	bg2(128,128,128,64);
		pRenderer->GetIRenderAuxGeom()->DrawTriangle(orig, bg1, orig+u*sizex, bg1, orig+u*sizex+v*sizey, bg2);
		pRenderer->GetIRenderAuxGeom()->DrawTriangle(orig, bg1, orig+u*sizex+v*sizey, bg2, orig+v*sizey, bg2);

		// Draw time lines
/*		float	timeLen = (PROTO_HEALTH_HISTORY_LEN - 1) * PROTO_HEALTH_HISTORY_INTERVAL;
		const float tickInterval = 1.0f;
		unsigned tickCount = (unsigned)floor(timeLen / tickInterval);
		for(unsigned i = 0; i < tickCount; ++i)
		{
			float	t = ((i * tickInterval) / timeLen) * sizex;
			pRenderer->GetIRenderAuxGeom()->DrawLine(orig + t*u, ColorB(255,255,255,64), orig + t*u + v*sizey, ColorB(255,255,255,64));
		}*/

		SAIValueHistory* history = 0;

		float	timeLen = 1.0f;
		float maxVal = 1.0f;
		for(unsigned i = 0; i < targets.size(); ++i)
		{
			if(!targets[i]->m_healthHistory) continue;

			float maxHealth = targets[i]->GetProxy()->GetActorMaxHealth();
			float maxHealthArmor = targets[i]->GetProxy()->GetActorMaxHealth() + targets[i]->GetProxy()->GetActorMaxArmor();
			maxVal = max(maxVal, maxHealthArmor / maxHealth);

			timeLen = max(timeLen, (float)(targets[i]->m_healthHistory->GetMaxSampleCount() *
				targets[i]->m_healthHistory->GetSampleInterval()));
		}

		// Draw value lines
		pRenderer->GetIRenderAuxGeom()->DrawLine(orig, ColorB(255,255,255,128), orig + u*sizex, ColorB(255,255,255,128));
		pRenderer->GetIRenderAuxGeom()->DrawLine(orig + v*sizey*(0.5f/maxVal), ColorB(255,255,255,64), orig + u*sizex + v*sizey*(0.5f/maxVal), ColorB(255,255,255,64));
		pRenderer->GetIRenderAuxGeom()->DrawLine(orig + v*sizey*(1.0f/maxVal), ColorB(255,255,255,128), orig + u*sizex + v*sizey*(1.0f/maxVal), ColorB(255,255,255,128));

		// Draw time lines
		const float tickInterval = 1.0f; //seconds
		unsigned tickCount = (unsigned)floor(timeLen / tickInterval);
		for(unsigned j = 0; j < tickCount; ++j)
		{
			float	t = ((j * tickInterval) / timeLen) * sizex;
			pRenderer->GetIRenderAuxGeom()->DrawLine(orig + t*u, ColorB(255,255,255,64), orig + t*u + v*sizey, ColorB(255,255,255,64));
		}

		const float s = 1.0f / maxVal;

		// Draw targets
		for (unsigned i = 0; i < targets.size(); ++i)
		{
			history = targets[i]->m_healthHistory;
			if (history)
			{
				unsigned n = history->GetSampleCount();
				values.resize(n);
				float dt = history->GetSampleInterval();

				for(unsigned j = 0; j < n; ++j)
				{
					float	val = min(history->GetSample(j) * s, 1.0f) * sizey;
					float	t = ((timeLen - j * dt) / timeLen) * sizex;
					values[j] = orig + t*u + v*val;
				}
				if(n > 0)
					pRenderer->GetIRenderAuxGeom()->DrawPolyline(&values[0], n, false, ColorB(255,255,255,160),1.5f);
			}
		}


		// Draw shooters
		for(unsigned i = 0; i < shooters.size(); ++i)
		{
			history = shooters[i]->m_targetDamageHealthThrHistory;
			if(history)
			{
				unsigned n = history->GetSampleCount();
				values.resize(n);
				float dt = history->GetSampleInterval();
				for(unsigned j = 0; j < n; ++j)
				{
					float	val = min(history->GetSample(j) * s, 1.0f) * sizey;
					float	t = ((timeLen - j * dt) / timeLen) * sizex;
					values[j] = orig + t*u + v*val;
				}
				if(n > 0)
					pRenderer->GetIRenderAuxGeom()->DrawPolyline(&values[0], n, false, ColorB(240,32,16,240));
			}
		}


/*		Vec3	values[PROTO_HEALTH_HISTORY_LEN];

		// Target health
		{
			unsigned n = target.GetHealthHistorySampleCount();
			for(unsigned i = 0; i < n; ++i)
			{
				float	val = (target.GetHealthHistorySample(i) / target.GetMaxHealth()) * sizey;
				float	t = ((timeLen - i * PROTO_HEALTH_HISTORY_INTERVAL) / timeLen) * sizex;
				values[i] = orig + t*u + v*val;
			}
			if(n > 0)
				pRenderer->GetIRenderAuxGeom()->DrawPolyline(values, n, false, ColorB(255,255,255,160),1.5f);
		}

		// Unit Thresholds
		for(unsigned j = 0; j < UNIT_COUNT; ++j)
		{
			unsigned n = unit[j].GetTargetHealthThrHistorySampleCount();
			for(unsigned i = 0; i < n; ++i)
			{
				float	val = (unit[j].GetTargetHealthThrHistorySample(i) / target.GetMaxHealth()) * sizey;
				float	t = ((timeLen - i * PROTO_HEALTH_HISTORY_INTERVAL) / timeLen) * sizex;
				values[i] = orig + t*u + v*val;
			}
			if(n > 0)
			{
				ColorB	col = trackColors[j % TRACK_COLOR_COUNT];
				col.a = 240;
				pRenderer->GetIRenderAuxGeom()->DrawPolyline(values, n, false, col);
			}
		}*/
	}
	else
	{
		// Separate graphs
		const float	spacingy = min(0.9f/(float)(shooters.size()+1), 0.2f);
		const float	sizex = 0.25f;
		const float	sizey = spacingy * 0.95f;

		int sw = pRenderer->GetWidth();
		int sh = pRenderer->GetHeight();

		float	white[4] = {0, 0.75f, 1, 1};
		float	whiteTrans[4] = {1, 1, 1, 0.7f};

		for(unsigned i = 0; i < shooters.size(); ++i)
		{
			CPuppet* pPuppet = shooters[i];
			Vec3	orig = Vec3(0.05f, 0.05f + (i+1)*spacingy, 0);

			// dim BG
			ColorB	bg1(0,0,0,128); ColorB	bg2(128,128,128,64);
			pRenderer->GetIRenderAuxGeom()->DrawTriangle(orig, bg1, orig+u*sizex, bg1, orig+u*sizex+v*sizey, bg2);
			pRenderer->GetIRenderAuxGeom()->DrawTriangle(orig, bg1, orig+u*sizex+v*sizey, bg2, orig+v*sizey, bg2);

			// Draw time lines
			float	timeLen = pPuppet->m_targetDamageHealthThrHistory->GetMaxSampleCount() *
				pPuppet->m_targetDamageHealthThrHistory->GetSampleInterval();
			const float tickInterval = 1.0f; //seconds
			unsigned tickCount = (unsigned)floor(timeLen / tickInterval);
			for(unsigned j = 0; j < tickCount; ++j)
			{
				float	t = ((j * tickInterval) / timeLen) * sizex;
				pRenderer->GetIRenderAuxGeom()->DrawLine(orig + t*u, ColorB(255,255,255,64), orig + t*u + v*sizey, ColorB(255,255,255,64));
			}

			// Draw curve
			SAIValueHistory* history = 0;

			float s = 1.0f;

			if(pPuppet->GetAttentionTarget())
			{

				CAIActor* pTargetActor = 0;
				CAIObject* pTarget = (CAIObject*)pPuppet->GetAttentionTarget();
				if(pTarget->IsAgent())
					pTargetActor = pTarget->CastToCAIActor();
				else if(pTarget->GetAssociation() && pTarget->GetAssociation()->IsAgent())
					pTargetActor = pTarget->GetAssociation()->CastToCAIActor();

				if(pTargetActor)
				{
					history = pTargetActor->m_healthHistory;
					if(history)
					{
						float maxHealth = pTargetActor->GetProxy()->GetActorMaxHealth();
						float maxHealthArmor = pTargetActor->GetProxy()->GetActorMaxHealth() + pTargetActor->GetProxy()->GetActorMaxArmor();
						float maxVal = maxHealthArmor / maxHealth;

						s = 1.0f / maxVal;

						// Draw value lines
						pRenderer->GetIRenderAuxGeom()->DrawLine(orig, ColorB(255,255,255,128), orig + u*sizex, ColorB(255,255,255,128));
						pRenderer->GetIRenderAuxGeom()->DrawLine(orig + v*sizey*0.5f*s, ColorB(255,255,255,64), orig + u*sizex + v*sizey*0.5f*s, ColorB(255,255,255,64));
						pRenderer->GetIRenderAuxGeom()->DrawLine(orig + v*sizey*s, ColorB(255,255,255,128), orig + u*sizex + v*sizey*s, ColorB(255,255,255,128));


						unsigned n = history->GetSampleCount();
						values.resize(n);
						float dt = history->GetSampleInterval();
						for(unsigned j = 0; j < n; ++j)
						{
							float	val = min(history->GetSample(j) * s, 1.0f) * sizey;
							float	t = ((timeLen - j * dt) / timeLen) * sizex;
							values[j] = orig + t*u + v*val;
						}
						if(n > 0)
							pRenderer->GetIRenderAuxGeom()->DrawPolyline(&values[0], n, false, ColorB(255,255,255,160),1.5f);
					}
				}
			}
			else
			{
				float maxVal = max(1.0f, pPuppet->m_targetDamageHealthThrHistory->GetMaxSampleValue());
				s = 1.0f / maxVal;
			}

			history = pPuppet->m_targetDamageHealthThrHistory;
			if(history)
			{
				unsigned n = history->GetSampleCount();
				values.resize(n);
				float dt = history->GetSampleInterval();
				for(unsigned j = 0; j < n; ++j)
				{
					float	val = min(history->GetSample(j) * s, 1.0f) * sizey;
					float	t = ((timeLen - j * dt) / timeLen) * sizex;
					values[j] = orig + t*u + v*val;
				}
				if(n > 0)
					pRenderer->GetIRenderAuxGeom()->DrawPolyline(&values[0], n, false, ColorB(240,32,16,240));
			}

			char* szZone = "";
			switch(pPuppet->m_targetZone)
			{
			case CPuppet::ZONE_OUT: szZone = "Out"; break;
			case CPuppet::ZONE_WARN: szZone = "Warn"; break;
			case CPuppet::ZONE_COMBAT_NEAR: szZone = "Combat-Near"; break;
			case CPuppet::ZONE_COMBAT_FAR: szZone = "Combat-Far"; break;
			case CPuppet::ZONE_KILL: szZone = "Kill"; break;
			}

			// Shooter name
			pRenderer->Draw2dLabel(orig.x*sw+3, (orig.y-spacingy+sizey)*sh-25, 1.2f, white, false, "%s", pPuppet->GetName());

			float moveMod = 1, stanceMod = 1, dirMod = 1, hitMod = 1, zoneMod = 1;
			float aliveTime = pPuppet->GetTargetAliveTime(&moveMod, &stanceMod, &dirMod, &hitMod, &zoneMod);
			const float accuracy = pPuppet->GetParameters().m_fAccuracy;
			char szAlive[32] = "inf";
			if (accuracy > 0.001f)
				_snprintf(szAlive, 32, "%.1fs", aliveTime / accuracy);

			pRenderer->Draw2dLabel(orig.x*sw+3, (orig.y-spacingy+sizey)*sh-10, 1.1f, whiteTrans, false, "Alive: %s  React: %.1f  Acc: %.3f  Zone: %s  %s",
				szAlive, pPuppet->GetAttentionTarget() ? pPuppet->GetFiringReactionTime(pPuppet->GetAttentionTarget()->GetPos()) : 0.0f,
				accuracy, szZone, pPuppet->IsAllowedToHitTarget() ? "FIRE": "");

/*			const float ss = sizex / 5.0f;
			if(fabsf(moveMod - 1.0f) > 0.001f)
				pRenderer->Draw2dLabel((orig.x+ss*0)*sw+3, (orig.y-spacingy+sizey)*sh-15, 1.2f, whiteTrans, true, "Move:%.2f", moveMod);
			if(fabsf(stanceMod - 1.0f) > 0.001f)
				pRenderer->Draw2dLabel((orig.x+ss*1)*sw+3, (orig.y-spacingy+sizey)*sh-15, 1.2f, whiteTrans, true, "Stance:%.2f", stanceMod);
			if(fabsf(dirMod - 1.0f) > 0.001f)
				pRenderer->Draw2dLabel((orig.x+ss*2)*sw+3, (orig.y-spacingy+sizey)*sh-15, 1.2f, whiteTrans, true, "Dir:%.2f", dirMod);
			if(fabsf(hitMod - 1.0f) > 0.001f)
				pRenderer->Draw2dLabel((orig.x+ss*3)*sw+3, (orig.y-spacingy+sizey)*sh-15, 1.2f, whiteTrans, true, "Hit:%.2f", hitMod);
			if(fabsf(zoneMod - 1.0f) > 0.001f)
				pRenderer->Draw2dLabel((orig.x+ss*4)*sw+3, (orig.y-spacingy+sizey)*sh-15, 1.2f, whiteTrans, true, "Zone:%.2f", zoneMod);*/
		}
	}

/*
	std::vector<Vec3>	values;

	const float maxHealth = target.GetMaxHealth()

	// Player health
	{
		unsigned n = pPlayer->m_healthHistory->GetSampleCount();
		values.resize(n);
		float dt = pPlayer->m_healthHistory->GetSampleInterval();
		for(unsigned i = 0; i < n; ++i)
		{
			float	val = (pPlayer->m_healthHistory.GetSample(i) / target.GetMaxHealth()) * sizey;
			float	t = ((timeLen - i * dt) / timeLen) * sizex;
			values[i] = orig + t*u + v*val;
		}
		if(n > 0)
			pRenderer->GetIRenderAuxGeom()->DrawPolyline(values, n, false, ColorB(255,255,255,160),1.5f);
	}



		unsigned n = pPuppet->m_targetDamageHealthThrHistory->GetSampleCount();
		values.resize(n);
		float dt = pPlayer->m_healthHistory.GetSampleInterval();
		for(unsigned i = 0; i < n; ++i)
		{
			float	val = (unit[j].GetTargetHealthThrHistorySample(i) / target.GetMaxHealth()) * sizey;
			float	t = ((timeLen - i * PROTO_HEALTH_HISTORY_INTERVAL) / timeLen) * sizex;
			values[i] = orig + t*u + v*val;
		}
		if(n > 0)
		{
			ColorB	col = trackColors[j % TRACK_COLOR_COUNT];
			col.a = 240;
			pRenderer->GetIRenderAuxGeom()->DrawPolyline(values, n, false, col);
		}
	}
*/

	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
}

void CAISystem::DrawDebugShape (const SDebugSphere & sphere)
{
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere (sphere.pos, sphere.radius, sphere.color);
}

void CAISystem::DrawDebugShape (const SDebugBox & box)
{
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawOBB (box.obb, box.pos, true, box.color, eBBD_Faceted);
}

void CAISystem::DrawDebugShape (const SDebugLine & line)
{
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(line.start, line.color, line.end, line.color);
}

template <typename ShapeContainer>
void CAISystem::DrawDebugShapes (ShapeContainer & shapes, float dt)
{
	for (unsigned i = 0; i < shapes.size(); )
	{
		typename ShapeContainer::value_type & shape = shapes[i];
		// NOTE Mai 29, 2007: <pvl> draw it at least once
		DrawDebugShape (shape);

		shape.time -= dt;
		if (shape.time < 0)
		{
			shapes[i] = shapes.back();
			shapes.pop_back();
		}
		else
		{
			++i;
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDraw(IRenderer *pRenderer)
{
	if (!m_bInitialized) return;

	//return;
	if (!pRenderer) 
		return;

  if (m_cvDebugDraw->GetIVal()<0) // if -ve ONLY display warnings
    return;

	float	white[4] = {1, 1, 1, 1};

	if(m_cvDebugDraw->GetIVal()!=0 && !IsEnabled())
	{
		pRenderer->Draw2dLabel(100, 200, 4, white, true, "AI System is disabled; ai_DEbugDraw is on");
		return;
	}
	
	DebugDrawUpdate(pRenderer);
	
	if(m_cvDrawFakeTracers->GetIVal() > 0)
	{
		SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags	newFlags;
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

		for(size_t i = 0; i < m_DEBUG_fakeTracers.size(); ++i)
		{
			Vec3 p0 = m_DEBUG_fakeTracers[i].p0;
			Vec3 p1 = m_DEBUG_fakeTracers[i].p1;
			Vec3 dir = p1 - p0;
			float u = 1 - m_DEBUG_fakeTracers[i].t / m_DEBUG_fakeTracers[i].tmax;
			p0 += dir*u*0.5f;
			p1 = p0 + dir*0.5f;

			float	a = (m_DEBUG_fakeTracers[i].a * m_DEBUG_fakeTracers[i].t / m_DEBUG_fakeTracers[i].tmax)*0.75f + 0.25f;
			Vec3	mid((p0+p1)/2);
			pRenderer->GetIRenderAuxGeom()->DrawLine(p0, ColorB(128,128,128,0), mid, ColorB(255,255,255,(uint8)(255*a)), 6.0f);
			pRenderer->GetIRenderAuxGeom()->DrawLine(p1, ColorB(128,128,128,0), mid, ColorB(255,255,255,(uint8)(255*a)), 6.0f);
		}

		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
	}

	if(m_cvDrawFakeHitEffects->GetIVal() > 0)
	{
		SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags	newFlags;
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

		for(size_t i = 0; i < m_DEBUG_fakeHitEffect.size(); ++i)
		{
			float	a = m_DEBUG_fakeHitEffect[i].t / m_DEBUG_fakeHitEffect[i].tmax;
			Vec3	pos = m_DEBUG_fakeHitEffect[i].p + m_DEBUG_fakeHitEffect[i].n * (1 - sqr(a));
			float	r = m_DEBUG_fakeHitEffect[i].r * (0.5f + (1-a)*0.5f);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, r, ColorB(m_DEBUG_fakeHitEffect[i].c.r,m_DEBUG_fakeHitEffect[i].c.g,m_DEBUG_fakeHitEffect[i].c.b,(uint8)(m_DEBUG_fakeHitEffect[i].c.a*a)));
		}

		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
	}

	if(m_cvDrawFakeDamageInd->GetIVal() > 0)
	{
		int mode = m_cvDrawFakeDamageInd->GetIVal();

		CAIObject* pPlayer = GetPlayer();
		if(pPlayer)
		{
			// full screen quad
			SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
			SAuxGeomRenderFlags	newFlags(e_Def2DPublicRenderflags);
			newFlags.SetAlphaBlendMode(e_AlphaBlended);
			newFlags.SetCullMode(e_CullModeNone);
			pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

			// Fullscreen flash for damage indication.
			if(m_DEBUG_screenFlash > 0.0f)
			{
				float	f = m_DEBUG_screenFlash * 2.0f;
				float	a = clamp(f, 0.0f, 1.0f);
				ColorB	col(239, 50, 25, (uint8)(a*255));
				pRenderer->GetIRenderAuxGeom()->DrawTriangle(Vec3(0,0,0), col, Vec3(1,0,0), col, Vec3(1,1,0), col);
				pRenderer->GetIRenderAuxGeom()->DrawTriangle(Vec3(0,0,0), col, Vec3(1,1,0), col, Vec3(0,1,0), col);
			}

			// Damage indicator triangles
			Matrix33	basis;
			basis.SetRotationVDir(pPlayer->GetViewDir());
			Vec3 u = basis.GetColumn0();
			Vec3 v = basis.GetColumn1();
			Vec3 w = basis.GetColumn2();

			float	rw = pRenderer->GetWidth();
			float	rh = pRenderer->GetHeight();
			float	as = rh/rw;

			const float FOV = pRenderer->GetCamera().GetFov() * 0.95f;
			const float cosFOV = cosf(FOV/2);

			for(unsigned i = 0; i < m_DEBUG_fakeDamageInd.size(); ++i)
			{
				Vec3	dir = m_DEBUG_fakeDamageInd[i].p - pPlayer->GetPos();
				dir.NormalizeSafe();
				float	x = u.Dot(dir);
				float	y = v.Dot(dir);
				float	d = sqrtf(sqr(x) + sqr(y));
				if(d < 0.00001f)
					continue;
				x /= d;
				y /= d;
				float	nx = y;
				float	ny = -x;

				const float	r0 = 0.15f;
				const float	r1 = 0.25f;
				const float	wi = 0.04f;

				float	a = 1 - sqr(1 - m_DEBUG_fakeDamageInd[i].t / m_DEBUG_fakeDamageInd[i].tmax);

				bool targetVis = false;

				if(mode == 2)
				{
					Vec3 dirLocal(u.Dot(dir), v.Dot(dir), w.Dot(dir));
					if(dirLocal.y > 0.1f)
					{
						dirLocal.x /= dirLocal.y;
						dirLocal.z /= dirLocal.y;

						float tz = tanf(FOV/2);
						float tx = rw/rh * tz;

						if(fabsf(dirLocal.x) < tx && fabsf(dirLocal.z) < tz)
							targetVis = true;
					}
				}

				ColorB col(239, 50, 25, (uint8)(a*255));

				if(!targetVis)
				{
					pRenderer->GetIRenderAuxGeom()->DrawTriangle(Vec3(0.5f + (x*r0)*as, 0.5f - (y*r0),0), col,
						Vec3(0.5f + (x*r1+nx*wi)*as, 0.5f - (y*r1+ny*wi),0), col,
						Vec3(0.5f + (x*r1-nx*wi)*as, 0.5f - (y*r1-ny*wi),0), col);
				}
				else
				{
					// Draw silhouette when on FOV.
					static std::vector<Vec3> pos2d;
					static std::vector<Vec3> pos2dSil;
					static std::vector<ColorB> colSil;
					pos2d.resize(m_DEBUG_fakeDamageInd[i].verts.size());

					for(unsigned j = 0; j < m_DEBUG_fakeDamageInd[i].verts.size(); ++j)
					{
						const Vec3& v = m_DEBUG_fakeDamageInd[i].verts[j];
						Vec3& o = pos2d[j];
						pRenderer->ProjectToScreen(v.x, v.y, v.z, &o.x, &o.y, &o.z);
						o.x /= 100.0f;
						o.y /= 100.0f;
					}

					pos2dSil.clear();
					ConvexHull2D(pos2dSil, pos2d);

					if(pos2dSil.size() > 2)
					{
						colSil.resize(pos2dSil.size());
						float miny = FLT_MAX, maxy = -FLT_MAX;
						for(unsigned j = 0; j < pos2dSil.size(); ++j)
						{
							miny = min(miny, pos2dSil[j].y);
							maxy = max(maxy, pos2dSil[j].y);
						}
						float range = maxy - miny;
						if(range > 0.0001f)
							range = 1.0f / range;
						for(unsigned j = 0; j < pos2dSil.size(); ++j)
						{
							float aa = clamp( (1 - (pos2dSil[j].y - miny) * range) * 2.0f, 0.0f, 1.0f);
							colSil[j] = col;
							colSil[j].a *= (uint8)(aa * a);
						}
						pRenderer->GetIRenderAuxGeom()->DrawPolyline(&pos2dSil[0], pos2dSil.size(), true, &colSil[0], 2.0f);
					}
				}
			}

			// Draw ambient fire indicators
			if(m_cvDebugDrawDamageControl->GetIVal() > 1)
			{
				const Vec3& playerPos = pPlayer->GetPos();
				AIObjects::const_iterator aio = m_Objects.find(AIOBJECT_PUPPET);
				for(; aio != m_Objects.end(); ++aio)
				{
					if(aio->first != AIOBJECT_PUPPET) break;
					CPuppet* pPuppet = aio->second->CastToCPuppet();
					if(!pPuppet) continue;
					if(!pPuppet->IsEnabled()) continue;
					if(Distance::Point_PointSq(playerPos, pPuppet->GetPos()) > sqr(150.0f)) continue;

					Vec3	dir = pPuppet->GetPos() - playerPos;
					float	x = u.Dot(dir);
					float	y = v.Dot(dir);
					float	d = sqrtf(sqr(x) + sqr(y));
					if(d < 0.00001f)
						continue;
					x /= d;
					y /= d;
					float	nx = y;
					float	ny = -x;

					const float	r0 = 0.25f;
					const float	r1 = 0.28f;
					const float	w = 0.01f;

					ColorB col(255, 255, 255, pPuppet->IsAllowedToHitTarget() ? 255 : 64);
					pRenderer->GetIRenderAuxGeom()->DrawTriangle(Vec3(0.5f + (x*r0)*as, 0.5f - (y*r0),0), col,
						Vec3(0.5f + (x*r1+nx*w)*as, 0.5f - (y*r1+ny*w),0), col,
						Vec3(0.5f + (x*r1-nx*w)*as, 0.5f - (y*r1-ny*w),0), col);
				}
			}

			pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
		}
	}

	if (m_cvDebugDraw->GetIVal()==0)
		return;

  AILogDisplaySavedMsgs(pRenderer);

  if (m_cvDebugDraw->GetIVal()<0) // if -ve ONLY display warnings
    return;

	SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

	DebugDrawRecorderRange(pRenderer);

	if (m_cvDebugDraw->GetIVal() == 2)
	{
		pRenderer->Draw2dLabel(50, 200, 2, white, true, "Vis checks:");
		pRenderer->Draw2dLabel(175, 200, 2, white, true, "%d", m_visChecks);
		pRenderer->Draw2dLabel(225, 200, 1, white, true, "max:%d", m_visChecksMax);

		pRenderer->Draw2dLabel(50, 250, 2, white, true, "Rays:");
		pRenderer->Draw2dLabel(175, 250, 2, white, true, "%d", m_visChecksRays);
		pRenderer->Draw2dLabel(225, 250, 1, white, true, "max:%d", m_visChecksRaysMax);

		pRenderer->Draw2dLabel(50, 300, 2, white, true, "Ratio:");
		if (m_visChecks)
			pRenderer->Draw2dLabel(175, 300, 2, white, true, "%.1f", ((float)m_visChecksRays / (float)m_visChecks) * 100.0f);
		if (m_visChecksMax)
			pRenderer->Draw2dLabel(225, 300, 1, white, true, "max:%.1f", ((float)m_visChecksRaysMax / (float)m_visChecksMax) * 100.0f);

		// full screen quad
		SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags	newFlags(e_Def2DPublicRenderflags);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		newFlags.SetCullMode(e_CullModeNone);
		newFlags.SetDepthTestFlag(e_DepthTestOff);
		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

		float	rw = pRenderer->GetWidth();
		float	rh = pRenderer->GetHeight();
		float	as = rh/rw;

		float vals[VIS_CHECK_HISTORY_SIZE];
		float raysVals[VIS_CHECK_HISTORY_SIZE];
		float updatesVals[VIS_CHECK_HISTORY_SIZE];
		for (unsigned i = 0; i < VIS_CHECK_HISTORY_SIZE; ++i)
		{
			vals[i] = 0;
			raysVals[i] = 0;
			updatesVals[i] = 0;
		}
		for (unsigned i = 0; i < m_visChecksHistorySize; ++i)
		{
			unsigned j = (m_visChecksHistoryHead+(VIS_CHECK_HISTORY_SIZE-1)-i) % VIS_CHECK_HISTORY_SIZE;
			vals[(VIS_CHECK_HISTORY_SIZE-1)-i] = m_visChecksHistory[j];
			raysVals[(VIS_CHECK_HISTORY_SIZE-1)-i] = m_visChecksRaysHistory[j];
			updatesVals[(VIS_CHECK_HISTORY_SIZE-1)-i] = m_visChecksUpdatesHistory[j];
		}

		Vec3	points[VIS_CHECK_HISTORY_SIZE];
		Vec3	raysPoints[VIS_CHECK_HISTORY_SIZE];
		Vec3	updatesPoints[VIS_CHECK_HISTORY_SIZE];

		float scale = 1.0f / rh;

		for (unsigned i = 0; i <= 10; ++i)
		{
			int v = i * 10;
			pRenderer->GetIRenderAuxGeom()->DrawLine(Vec3(0.1f,0.9f - v*scale,0), ColorB(255,255,255,128), Vec3(0.9f,0.9f - v*scale,0), ColorB(255,255,255,128));
		}

		for (unsigned i = 0; i < VIS_CHECK_HISTORY_SIZE; ++i)
		{
			float t = (float)i / (float)VIS_CHECK_HISTORY_SIZE;
			points[i].x = 0.1f + t*0.8f;
			points[i].y = 0.9f - vals[i]*scale;
			points[i].z = 0;

			raysPoints[i].x = 0.1f + t*0.8f;
			raysPoints[i].y = 0.9f - raysVals[i]*scale;
			raysPoints[i].z = 0;

			updatesPoints[i].x = 0.1f + t*0.8f;
			updatesPoints[i].y = 0.9f - updatesVals[i]*scale;
			updatesPoints[i].z = 0;
		}

		pRenderer->GetIRenderAuxGeom()->DrawPolyline(points, VIS_CHECK_HISTORY_SIZE, false, ColorB(255,0,0));
		pRenderer->GetIRenderAuxGeom()->DrawPolyline(raysPoints, VIS_CHECK_HISTORY_SIZE, false, ColorB(0,196,255));
		pRenderer->GetIRenderAuxGeom()->DrawPolyline(updatesPoints, VIS_CHECK_HISTORY_SIZE, false, ColorB(255,255,255));


		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);

		for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
		{
			CPuppet* pPuppet = m_enabledPuppetsSet[i];
			if (!pPuppet->m_bDryUpdate)
				pRenderer->GetIRenderAuxGeom()->DrawSphere(pPuppet->GetPos(), 1.0f, ColorB(255,0,0));
		}
	}

	if (!m_validationErrorMarkers.empty())
	{
		float col[4] = {1,1,1,1};
		for (unsigned i = 0; i < m_validationErrorMarkers.size(); ++i)
		{
			const SValidationErrorMarker& marker = m_validationErrorMarkers[i];
			pRenderer->GetIRenderAuxGeom()->DrawOBB(marker.obb, marker.pos, false, marker.col, eBBD_Faceted);

			const float s = 0.01f;
			pRenderer->GetIRenderAuxGeom()->DrawLine(marker.pos+Vec3(-s,0,0), marker.col, marker.pos+Vec3(s,0,0), marker.col);
			pRenderer->GetIRenderAuxGeom()->DrawLine(marker.pos+Vec3(0,-s,0), marker.col, marker.pos+Vec3(0,s,0), marker.col);
			pRenderer->GetIRenderAuxGeom()->DrawLine(marker.pos+Vec3(0,0,-s), marker.col, marker.pos+Vec3(0,0,s), marker.col);

			col[0] = marker.col.r / 255.0f;
			col[1] = marker.col.g / 255.0f;
			col[2] = marker.col.b / 255.0f;
			pRenderer->DrawLabelEx(marker.pos, 1.1f, col, true, true, "%s", marker.msg.c_str());
		}
	}

	if(m_cvTickCounter->GetIVal()!=0)
		m_pProfileTickCounter->Draw(pRenderer);

	if (!m_pGraph)
	{
		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
		return;
	}

	int	debugDrawVal = m_cvDebugDraw->GetIVal();

	if (m_pAutoBalance && (debugDrawVal==10))
		m_pAutoBalance->DebugDraw(pRenderer);
	else if(debugDrawVal==77)
		m_pVolumeNavRegion->DebugDrawConnections(pRenderer);
	else if(debugDrawVal==78)
		m_pVolumeNavRegion->DebugDrawNavProblems(pRenderer);
	else if(debugDrawVal>=500)
		m_pVolumeNavRegion->DebugDrawConnections(pRenderer, m_cvDebugDraw->GetIVal()-500, true);
  else if(m_cvDebugDraw->GetIVal()==71)
	  DebugDrawForbidden(pRenderer);
  else if(debugDrawVal==72)
	  DebugDrawGraphErrors(pRenderer, m_pGraph);
  else if(debugDrawVal==74)
	  DebugDrawGraph(pRenderer, m_cvDrawHide->GetIVal() ? m_pHideGraph : m_pGraph);
  else if(debugDrawVal==79 || debugDrawVal==179 || debugDrawVal==279)
  {
	  std::vector<Vec3> pos;
	  pos.push_back(GetDebugCameraPos());
	  DebugDrawGraph(pRenderer, m_cvDrawHide->GetIVal() ? m_pHideGraph : m_pGraph, &pos, 15);
  }

	if(m_cvDebugDrawDamageControl->GetIVal() > 1)
		DebugDrawDamageControlGraph(pRenderer);

	static bool recalcHideSpots = true;
	if (debugDrawVal == 81)
	{
		if (recalcHideSpots)
		{
			m_pVolumeNavRegion->CalculateHideSpots();
			recalcHideSpots = false;
		}
	}
	else
	{
		recalcHideSpots = true;
	}
	if (debugDrawVal == 82)
		m_pVolumeNavRegion->DebugDrawHideSpots(pRenderer);

	if(m_cvDrawAreas->GetIVal() > 0)
		DebugDrawAreas(pRenderer);

	if(m_cvDebugDrawDeadBodies->GetIVal() > 0)
	{
		for (unsigned i = 0, ni = m_deadBodies.size(); i < ni; ++i)
		{
			const Vec3& pos = m_deadBodies[i].pos;
			pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, 0.5f, ColorB(0, 0, 0));
			pRenderer->DrawLabel(pos + Vec3(0,0,0.8f),1,"R.I.P. %d (%.1fs)", i, m_deadBodies[i].t);
		}
	}

	if(m_cvDebugDraw->GetIVal()==80)
		DebugDrawTaggedNodes(pRenderer);

	DebugDrawSteepSlopes(pRenderer);
  DebugDrawVegetationCollision(pRenderer);

  if (m_cvDebugDraw->GetIVal() == 90)
    m_pFlightNavRegion->DebugDraw(pRenderer, GetDebugCameraPos(), 200.0f);

  if (m_cvDebugDrawVolumeVoxels->GetIVal() != 0)
    m_pVolumeNavRegion->DebugDrawVoxels(pRenderer);
	if ( m_bUpdateSmartObjects && m_cvDrawSmartObjects->GetIVal())
			m_pSmartObjects->DebugDraw( gEnv->pRenderer );

	DebugDrawPath(pRenderer);
	DebugDrawPathAdjustments(pRenderer);
	DebugDrawNavModifiers(pRenderer);
	DebugDrawStatsTarget(pRenderer, m_cvStatsTarget->GetString());
	DebugDrawFormations(pRenderer);
	DebugDrawNode(pRenderer);
	DebugDrawType(pRenderer);
	DebugDrawSquadmates(pRenderer);
	DebugDrawLocate(pRenderer);
	DebugDrawBadAnchors(pRenderer);
	DebugDrawTargetsList(pRenderer);
	DebugDrawStatsList(pRenderer);
	DebugDrawGroups(pRenderer);
  DebugDrawUnreachableNodeLocations(pRenderer);
  DebugDrawHideSpots(pRenderer);
  DebugDrawSelectedHideSpots(pRenderer);
	DebugDrawDynamicHideObjects(pRenderer);
	DebugDrawHashSpace(pRenderer);

	DebugDrawAgents(pRenderer);
	DebugDrawShooting(pRenderer);
	DebugDrawExplosions(pRenderer);
	DebugDrawObstructSpheres(pRenderer);

  m_pVolumeNavRegion->DebugDraw(pRenderer);

	m_lightManager.DebugDraw(pRenderer);

	{
    // check all entities called CheckVoxelSomething
    std::vector<const IEntity*> checkEntities;
    IEntityItPtr entIt = gEnv->pEntitySystem->GetEntityIterator();
    const char* navName = "CheckVoxel";
    size_t navNameLen = strlen(navName);
    if (entIt)
    {
      entIt->MoveFirst();
      while (!entIt->IsEnd())
      {
        IEntity* ent = entIt->Next();
        if (ent)
        {
          const char* name = ent->GetName();
          if (strnicmp(name, navName, navNameLen) == 0)
          {
            checkEntities.push_back(ent);
          }
        }
      }
    }

    std::vector<const IEntity*> navEntities;
    m_pVolumeNavRegion->GetNavigableSpaceEntities(navEntities);

    for (unsigned iEnt = 0 ; iEnt < checkEntities.size() ; ++iEnt)
    {
		  const IEntity* ent = checkEntities[iEnt];
			const Vec3 & pos = ent->GetPos();
			Vec3 definitelyValidPt;
      if (m_pVolumeNavRegion->GetNavigableSpacePoint(pos, definitelyValidPt, navEntities))
      {
			  static float range = 30.0f;
			  Vec3 regionMin = pos - Vec3(range, range, range);
			  Vec3 regionMax = pos + Vec3(range, range, range);
			  CVolumeNavRegion::ECheckVoxelResult result = m_pVolumeNavRegion->CheckVoxel(pos, definitelyValidPt);
			  ColorF col;
			  switch (result)
			  {
			  case CVolumeNavRegion::VOXEL_VALID:
				  col.Set(1, 0, 0);
				  break;
			  case CVolumeNavRegion::VOXEL_INVALID:
				  col.Set(0, 1, 0);
				  break;
			  }
			  pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, 0.7f, col);
		  }
    }
	}

/*	{
		IEntity* ent0 = m_pSystem->GetIEntitySystem()->FindEntityByName("p0");
		IEntity* ent1 = m_pSystem->GetIEntitySystem()->FindEntityByName("p1");
		if (ent0 && ent1)
		{
			Vec3 p0 = ent0->GetWorldPos();
			Vec3 p1 = ent1->GetWorldPos();

			pRenderer->GetIRenderAuxGeom()->DrawSphere(p0, 0.7f, ColorB(255,255,255,128));
			pRenderer->GetIRenderAuxGeom()->DrawLine(p0, ColorB(255,255,255), p1, ColorB(255,255,255));

			Vec3 hit;
			float hitDist = 0;
			if (IntersectSweptSphere(&hit, hitDist, Lineseg(p0, p1), 0.65f, AICE_STATIC))
			{
				Vec3 dir = p1 - p0;
				dir.Normalize();
				pRenderer->GetIRenderAuxGeom()->DrawSphere(p0 + dir*hitDist, 0.7f, ColorB(255,0,0));
			}
		}
	}*/

  {
    CAIObject *pAI = GetAIObjectByName("DebugRequestPathInDirection");
    if (pAI && pAI->GetType() == AIOBJECT_PUPPET)
    {
      CPuppet* puppet = (CPuppet *) pAI;
      Vec3 dir = puppet->GetMoveDir();

      static CTimeValue lastTime = GetAISystem()->GetFrameStartTime();
      CTimeValue thisTime = GetAISystem()->GetFrameStartTime();
      static float regenTime = 1.0f;

      if ((thisTime - lastTime).GetSeconds() > regenTime)
      {
        lastTime = thisTime;
        puppet->m_Path.Clear("DebugRequestPathInDirection");
        Vec3 startPos = puppet->GetPhysicsPos();
        static float maxDist = 15.0f;

        RequestPathInDirection(startPos, dir, maxDist, puppet, 0.0f);
      }

      DebugDrawPathSingle(pRenderer, puppet);
    }
  }

	{
		IEntity* ent1 = m_pSystem->GetIEntitySystem()->FindEntityByName("GetBestPositionStart");
		IEntity* ent2 = m_pSystem->GetIEntitySystem()->FindEntityByName("GetBestPositionEnd");
		if (ent1 && ent2)
		{
			static float radius = 0.3f;
			static float maxCost = 15.0f;

			const Vec3 & startPos = ent1->GetPos();
			const Vec3 & endPos = ent2->GetPos();

			CStandardHeuristic heuristic;
			IAISystem::tNavCapMask navmask = IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_VOLUME | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_FLIGHT ;
			static const AgentPathfindingProperties navProperties(
				navmask, 
				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
				5.0f, 10000.0f, -10000.0f, 0.0f, 20.0f, 0.0);
			heuristic.SetRequesterProperties(CPathfinderRequesterProperties(radius, navProperties), 0);
			Vec3 pos = m_pGraph->GetBestPosition(
				heuristic, maxCost, startPos, endPos, 0, navmask  );

			ColorF col(1, 1, 1);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(pos + Vec3(0, 0, 1), radius, col);

			DebugDrawCircles(pRenderer, startPos, maxCost, maxCost, 1, Vec3(1, 0, 0), Vec3(1, 1, 0));
		}
	}

	{
		IEntity* ent = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckCapsule");
		if (ent)
		{
			// match the params in CheckWalkability
			static float radius = walkabilityRadius;
			static Vec3 dir(0, 0, 0.9f);

			const Vec3 & pos = ent->GetPos();

			bool result = OverlapCapsule(Lineseg(pos, pos + dir), radius, AICE_ALL);
			ColorF col;
			if (result)
				col.Set(1, 0, 0);
			else
				col.Set(0, 1, 0);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, radius, col);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(pos + dir, radius, col);
		}
	}

	{
		IEntity* entFrom = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckCapsuleFrom");
		IEntity* entTo = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckCapsuleTo");
		if (entFrom && entTo)
		{
			static float radius = 3.0f;

			const Vec3 & posFrom = entFrom->GetPos();
			const Vec3 & posTo = entTo->GetPos();

      bool result = m_pVolumeNavRegion->PathSegmentWorldIntersection(posFrom, posTo, radius, true, true, AICE_ALL);
			ColorF col;
			if (result)
				col.Set(1, 0, 0);
			else
				col.Set(0, 1, 0);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(posFrom, radius, col);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(posTo, radius, col);
		}
	}

	{
		IEntity* entFrom = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckRayFrom");
		IEntity* entTo = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckRayTo");
		if (entFrom && entTo)
		{
			const Vec3 & posFrom = entFrom->GetPos();
			const Vec3 & posTo = entTo->GetPos();

      bool result = OverlapSegment(Lineseg(posFrom, posTo), AICE_ALL);
			ColorF col;
			if (result)
				col.Set(1, 0, 0);
			else
				col.Set(0, 1, 0);
			pRenderer->GetIRenderAuxGeom()->DrawLine(posFrom, col, posTo, col);
		}
	}

  {
		IEntity* ent = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckBody");
		if (ent)
		{
			const Vec3 & pos = ent->GetPos();
			m_pWaypointHumanNavRegion->CheckAndDrawBody(pos, pRenderer);
		}
	}

  {
    IEntity* ent = m_pSystem->GetIEntitySystem()->FindEntityByName("ValidateSmartObjectArea");
    if (ent)
    {
      const Vec3 & pos = ent->GetPos();
      static SSOTemplateArea templateArea = {pos, 1.0f, true};
	  static SSOUser user = {NULL, 0.4f, 1.5f, 0.5f};
      static EAICollisionEntities collEntities = AICE_ALL;
      templateArea.pt = pos;
      Vec3 floorPos = pos;

      bool ok = ValidateSmartObjectArea(templateArea, user, collEntities, floorPos);

      ColorF col;
      if (ok)
        col.Set(0, 1, 0);
      else
        col.Set(1, 0, 0);
      pRenderer->GetIRenderAuxGeom()->DrawSphere(floorPos + Vec3(0, 0, user.groundOffset), user.radius, col);
      pRenderer->GetIRenderAuxGeom()->DrawSphere(floorPos + Vec3(0, 0, user.groundOffset + user.height), user.radius, col);
      pRenderer->GetIRenderAuxGeom()->DrawLine(floorPos, col, floorPos + Vec3(0, 0, user.groundOffset), col);
    }
  }

	{
		IEntity* ent = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckGetEnclosing");
		if (ent)
		{
			static float radius = 0.3f;
			const Vec3 & pos = ent->GetPos();
			Vec3 closestValid;
			unsigned nodeIndex = m_pGraph->GetEnclosing(pos, IAISystem::NAVMASK_ALL, radius, 0, 5.0f, &closestValid, false);
			GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
			ColorF col;
			if (pNode)
				col.Set(0, 1, 0);
			else
				col.Set(1, 0, 0);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, radius, col);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(closestValid, radius, col);
			if (pNode)
				pRenderer->GetIRenderAuxGeom()->DrawSphere(pNode->GetPos() + Vec3(0, 0, 0.5), 0.1f, ColorF(1, 1, 1));
		}
	}

	{
		IEntity* entFrom = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckWalkabilityFrom");
		IEntity* entTo = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckWalkabilityTo");
		if (entFrom && entTo)
		{
      {
        static CTimeValue lastTime = gEnv->pTimer->GetAsyncTime();
        CTimeValue thisTime = gEnv->pTimer->GetAsyncTime();
        float deltaT = (thisTime - lastTime).GetSeconds();
        static float testsPerSec = 0.0f;
        if (deltaT > 2.0f)
        {
          lastTime = thisTime;
          testsPerSec = g_CheckWalkabilityCalls / deltaT;
          g_CheckWalkabilityCalls = 0;
        }
        static int column = 5;
        static int row = 50;
        char buff[256];
        sprintf(buff,"%5.2f walkability calls per sec", testsPerSec);
        float color[4]={1.f,0.f,1.f,1.f};
        DebugDrawLabel(pRenderer, column, row, buff, color);
      }

			const Vec3 & posFrom = entFrom->GetPos();
			const Vec3 & posTo = entTo->GetPos();
			int nBuildingID;
			IVisArea* pVisArea;
			CheckNavigationType(posFrom, nBuildingID, pVisArea, IAISystem::NAV_WAYPOINT_HUMAN);
			const SpecialArea* sa = GetSpecialArea(nBuildingID);
			static float radius = 0.3f;
      static bool all = true;
      bool result = CheckWalkability(posFrom, posTo, 0.0f, true, sa ? sa->GetPolygon() : ListPositions(), all ? AICE_ALL : AICE_STATIC);
			ColorF col;
			if (result)
				col.Set(0, 1, 0);
			else
				col.Set(1, 0, 0);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(posFrom, radius, col);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(posTo, radius, col);
		}
	}

	{
		IEntity* entFrom = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckWalkabilityTimeFrom");
		IEntity* entTo = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckWalkabilityTimeTo");
		if (entFrom && entTo)
		{
			const Vec3 & posFrom = entFrom->GetPos();
			const Vec3 & posTo = entTo->GetPos();
			static float radius = 0.3f;
      static bool all = true;
      bool result;
      static int num = 100;
      CTimeValue startTime = gEnv->pTimer->GetAsyncTime();
      for (unsigned i = 0 ; i < num ; ++i)
        result = CheckWalkability(posFrom, posTo, 0.0f, true, ListPositions(), all ? AICE_ALL : AICE_STATIC);
      CTimeValue endTime = gEnv->pTimer->GetAsyncTime();
			ColorF col;
			if (result)
				col.Set(0, 1, 0);
			else
				col.Set(1, 0, 0);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(posFrom, radius, col);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(posTo, radius, col);
      float time = (endTime - startTime).GetSeconds();

      static int column = 5;
	    static int row = 50;
	    char buff[256];
		  sprintf(buff,"%d walkability calls in %5.2f sec", num, time);
		  float color[4]={1.f,0.f,1.f,1.f};
		  DebugDrawLabel(pRenderer, column, row, buff, color);
		}
	}

	{
		IEntity* ent = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckFloorPos");
		if (ent)
		{
			const Vec3 & pos = ent->GetPos();
			static float up = 0.5f;
			static float down = 5.0f;
			static float radius = 0.1f;
			Vec3 floorPos;
			bool result = GetFloorPos(floorPos, pos, up, down, radius, AICE_ALL);
			ColorF col;
			if (result)
				col.Set(0, 1, 0);
			else
				col.Set(1, 0, 0);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(floorPos, radius, col);
		}
	}

	{
		IEntity* ent = m_pSystem->GetIEntitySystem()->FindEntityByName("CheckGravity");
		if (ent)
    {
      Vec3 pos = ent->GetPos();
      Vec3 gravity;
      pe_params_buoyancy junk;
      gEnv->pPhysicalWorld->CheckAreas(pos, gravity, &junk);
			ColorF col(1, 1, 1);
      static float lenScale = 0.3f;
      pRenderer->GetIRenderAuxGeom()->DrawLine(pos, col, pos + lenScale * gravity, col);
    }
  }

  {
    IEntity* ent = m_pSystem->GetIEntitySystem()->FindEntityByName("GetTeleportPos");
    if (ent)
    {
      const Vec3 &pos = ent->GetPos();
      Vec3 teleportPos = pos;

      IAISystem::tNavCapMask navCapMask = IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN;

      int nBuildingID;
      IVisArea *pArea;
      IAISystem::ENavigationType currentNavType = GetAISystem()->CheckNavigationType(pos, nBuildingID, pArea, navCapMask);

      bool canTeleport = GetAISystem()->GetNavRegion(currentNavType, GetAISystem()->GetGraph())->GetTeleportPosition(pos, teleportPos, "GetTeleportPos");

      if (canTeleport)
        pRenderer->GetIRenderAuxGeom()->DrawSphere(teleportPos, 0.5f, ColorF(0, 1, 0));
      else
        pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, 0.5f, ColorF(1, 0, 0));
    }
  }

/*	{
		IEntity* entFrom = m_pSystem->GetIEntitySystem()->FindEntityByName("PathFrom");
		IEntity* entTo = m_pSystem->GetIEntitySystem()->FindEntityByName("PathTo");
		if (entFrom && entTo)
		{
			static CTimeValue	lastTime(-1.0f);
			if(lastTime.GetSeconds() < 0.0f)
				lastTime = GetFrameStartTime();
			CTimeValue	time = GetFrameStartTime();
			float	dt = (time - lastTime).GetSeconds();
			lastTime = time;
			if(dt > 0.1f) dt = 0.1f;

			const float updateRate = 6.0f;

			static float timePassed = updateRate;

			if (dt > 0.0f)
			{
				timePassed += dt;

				static std::vector<Vec3>	path;
				static bool partial = false;
				static int iters = 0;

				Vec3 startPos = Vec3(1799.9414, 261.01788, 254.05769); //entFrom->GetWorldPos(); //(1797.9703, 273.82657, 234.75288);
				Vec3 endPos = Vec3(1811.0000, 282.00000, 251.00000); //entTo->GetWorldPos(); //(1805.0909, 269.29834, 205.77432);

				static IAISystem::tNavCapMask navCapMask = IAISystem::NAV_VOLUME;
				static float passRadius = 0.65f;

				if (timePassed > updateRate && !m_pCurrentRequest && m_nPathfinderResult == PATHFINDER_POPNEWREQUEST)
				{
					unsigned startIndex = m_pGraph->GetEnclosing(startPos, navCapMask, passRadius, 0, 5.0f, &startPos, true, "Test");
					unsigned endIndex = m_pGraph->GetEnclosing(endPos, navCapMask, passRadius, 0, 5.0f, &endPos, true, "Test");

					if (startIndex && endIndex)
					{
						AgentPathfindingProperties	prop;
						prop.SetToDefaults();
						prop.navCapMask = navCapMask;
						CPathfinderRequesterProperties requesterProperties;
						requesterProperties.properties = prop;
						requesterProperties.radius = passRadius;

						CCalculationStopper stopper("Test", m_cvPathfinderUpdateTime->GetFVal(), 70000);
						NavigationBlockers navigationBlockers;



						static float cost = 1000.0f;
						static bool radialDecay = true;
						static bool directional = true;

						float curRadius = 15.0f;
						float biasTowardsTarget = 0.7f;
						Vec3 mid = Vec3(1793.7927, 285.10379, 271.37958); //endPos * biasTowardsTarget + startPos * (1 - biasTowardsTarget);
						curRadius = 2.5f; //min(curRadius, Distance::Point_Point(endPos, startPos) * 0.8f);
						SNavigationBlocker	blocker(mid, curRadius, 0.f, -1.0f, radialDecay, directional);
//						navigationBlockers.push_back(blocker);



						CStandardHeuristic heuristic;
						heuristic.SetRequesterProperties(requesterProperties, 0);

						heuristic.SetRequesterProperties(requesterProperties, 0);
						heuristic.SetStartEndData(m_pGraph->GetNodeManager().GetNode(startIndex), startPos,
							m_pGraph->GetNodeManager().GetNode(endIndex), endPos, CStandardHeuristic::ExtraConstraints());

						int res = ASTAR_PATHFOUND;
						res = m_pAStarSolver->SetupAStar(stopper, m_pGraph, &heuristic, startIndex, endIndex, navigationBlockers, true);
						iters = 0;
						while (res == ASTAR_STILLSOLVING)
						{
							CCalculationStopper stopper2("Test", m_cvPathfinderUpdateTime->GetFVal(), 70000);
							res = m_pAStarSolver->ContinueAStar(stopper2);
							iters++;
						}

						if (res == ASTAR_PATHFOUND)
						{
							const CAStarSolver::tPathNodes& pathNodes = m_pAStarSolver->GetPathNodes();
							path.clear();
							for (unsigned i = 0, ni = pathNodes.size(); i < ni; ++i)
							{
								const GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(pathNodes[i]);
								path.push_back(pNode->GetPos());
							}
							partial = false;
						}
						else
						{
							const CAStarSolver::tPathNodes& pathNodes = m_pAStarSolver->CalculateAndGetPartialPath();
							path.clear();
							for (unsigned i = 0, ni = pathNodes.size(); i < ni; ++i)
							{
								const GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(pathNodes[i]);
								path.push_back(pNode->GetPos());
							}
							partial = true;
						}
					}

					timePassed = 0.0f;
				}

				DebugDrawWireSphere(pRenderer, Vec3(1793.7927, 285.10379, 271.37958), 2.5f, ColorB(255,196,0));

				if (partial)
					pRenderer->DrawLabel(startPos, 1.5f, "Iters: %d\nPartial", iters);
				else
					pRenderer->DrawLabel(startPos, 1.5f, "Iters: %d", iters);

				for (unsigned i = 0, ni = path.size(); i < ni; ++i)
				{
					AABB aabb(AABB::RESET);
					aabb.Add(path[i]+Vec3(0,0,0.5f), 0.1f);
					pRenderer->GetIRenderAuxGeom()->DrawAABB(aabb, true, ColorB(255,0,0), eBBD_Faceted);
				}
				if (path.size() > 1)
				{
					for (unsigned i = 0, ni = path.size()-1; i < ni; ++i)
						pRenderer->GetIRenderAuxGeom()->DrawLine(path[i]+Vec3(0,0,0.5f), ColorB(255,0,0), path[i+1]+Vec3(0,0,0.5f), ColorB(255,0,0), 3.0f);
				}
			}
			else
			{
				timePassed = updateRate;
			}
		}
	}*/

	{
		if (m_cvDebugDraw->GetIVal() == 1017)
		{
			// Simpler triangular obstacle query.
			IEntity* pEntTest = gEnv->pEntitySystem->FindEntityByName("test");
			if (!pEntTest)
				return;

			const Vec3& pos = pEntTest->GetWorldPos();
			const float rad = 0.6f;

			unsigned nodeIndex = m_pGraph->GetEnclosing(pos, IAISystem::NAV_VOLUME, 0.6f, 0, 5.0f, 0, true, "Test");
			GraphNode* pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
			if (pNode)
			{
				pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,255), pNode->GetPos(), ColorB(255,255,255), 3.0f);
				pRenderer->GetIRenderAuxGeom()->DrawSphere(pNode->GetPos(), 0.1f, ColorB(255,255,255));

				for (unsigned link = pNode->firstLinkIndex; link; link = m_pGraph->GetLinkManager().GetNextLink(link))
				{
					unsigned connectedIndex = m_pGraph->GetLinkManager().GetNextNode(link);
					GraphNode *pConnectedNode = m_pGraph->GetNodeManager().GetNode(connectedIndex);
					if (!pConnectedNode)
						continue;

					bool blocked = false;
					if (m_pGraph->GetLinkManager().GetRadius(link) < rad)
						blocked = true;
					float extraLinkFactor = GetAISystem()->GetExtraLinkCost(pNode->GetPos(), pConnectedNode->GetPos());
					if (extraLinkFactor < 0.0f)
						blocked = true;

					ColorB col(0,196,255);
					if (blocked)
						col = ColorB(255,0,0);

					pRenderer->GetIRenderAuxGeom()->DrawLine(pNode->GetPos(), col, pConnectedNode->GetPos(), col, 3.0f);
				}
			}
			else
			{
				pRenderer->DrawLabel(pos, 1.5f, "No Node");
			}
		}
	}


	// Draw debug shapes
	// These shapes come from outside the AI system (such as some debug code in script bind in CryAction) but related to AI.
	{
		const float dt = GetFrameDeltaTime();
		DrawDebugShapes (m_vecDebugLines, dt);
		DrawDebugShapes (m_vecDebugBoxes, dt);
		DrawDebugShapes (m_vecDebugSpheres, dt);
	}


	if(m_cvDrawGroupTactic->GetIVal() > 0)
	{
		for(AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
			it->second->DebugDraw(pRenderer);
	}

	DebugDrawRadar(pRenderer);

	DebugDrawDistanceLUT(pRenderer);

	// Draw occupied designer paths
	for(ShapeMap::iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
	{
		const SShape&	path = it->second;
		if(path.devalueTime < 0.01f)
			continue;
		pRenderer->DrawLabel(path.shape.front(), 1, "%.1f", path.devalueTime);
	}

	if(m_cvDebugDrawAmbientFire->GetIVal() > 0)
		DebugDrawAmbientFire(pRenderer);

	if(m_cvDebugDrawExpensiveAccessoryQuota->GetIVal() > 0)
		DebugDrawExpensiveAccessoryQuota(pRenderer);

	if(m_cvDebugDrawDamageParts->GetIVal() > 0)
	{
		CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
		if(pPlayer && pPlayer->GetDamageParts())
		{
			DamagePartVector*	parts = pPlayer->GetDamageParts();
			for(DamagePartVector::iterator it = parts->begin(); it != parts->end(); ++it)
			{
				SAIDamagePart&	part = *it;
				pRenderer->DrawLabel(part.pos, 1,"^ DMG:%.2f\n  VOL:%.1f", part.damageMult, part.volume);
			}
		}
	}

	// Draw the approximate stance size for the player.
	if(m_cvDebugDrawStanceSize->GetIVal() > 0)
	{
		CAIObject*	player = GetPlayer();
		if(player && player->GetProxy())
		{
			SAIBodyInfo	bodyInfo;
			player->GetProxy()->QueryBodyInfo(bodyInfo);
			Vec3	pos = player->GetPhysicsPos();
			AABB	aabb(bodyInfo.stanceSize);
			aabb.Move(pos);
			pRenderer->GetIRenderAuxGeom()->DrawAABB(aabb, true, ColorB(255,255,255,128), eBBD_Faceted);
		}
	}

	if(GetAISystem()->m_cvForceStance->GetIVal() != -1)
	{
		float redF[4] = {1,0,0,1};
		const char* szStance = 0;
		switch(GetAISystem()->m_cvForceStance->GetIVal())
		{
		case STANCE_STAND: szStance = "Stand"; break;
		case STANCE_CROUCH: szStance = "Crouch"; break;
		case STANCE_PRONE: szStance = "Prone"; break;
		case STANCE_RELAXED: szStance = "Relaxed"; break;
		case STANCE_STEALTH: szStance = "Stealth"; break;
		case STANCE_SWIM: szStance = "Swim"; break;
		case STANCE_ZEROG: szStance = "Zero-G"; break;
		default: szStance = "<unknown>"; break;
		};
		pRenderer->Draw2dLabel(10, pRenderer->GetHeight() - 30, 2.0f, redF, false, "Forced Stance: %s", szStance);
	}

	if (m_cvDebugDrawAdaptiveUrgency->GetIVal() > 0)
		DebugDrawAdaptiveUrgency(pRenderer);

	// Draw collision events
	if (m_cvDebugDrawCollisionEvents->GetIVal() > 0)
	{
		bool drawSoundEvents = m_cvDebugDrawSoundEvents->GetIVal() > 0;
		for (unsigned i = 0, ni = m_collisionEvents.size(); i < ni; ++i)
		{
			SAICollisionEvent& ce = m_collisionEvents[i];
			float a = 1.0f - clamp(ce.t / ce.tmax, 0.0f, 1.0f);
			ColorB col(255,255,255,(uint8)(128*a));
			if (ce.thrownByPlayer)
				col.Set(255,0,0,128*a);
			
			ColorB colAffect(255,255,255,(uint8)(255*a));
			
			DebugDrawWireSphere(pRenderer, ce.pos, ce.affectRadius, colAffect);

			if (!drawSoundEvents)
			{
				ColorB soundAffect(32,196,255,(uint8)(255*a));
				DebugDrawWireSphere(pRenderer, ce.pos, ce.soundRadius, soundAffect);
			}

			pRenderer->GetIRenderAuxGeom()->DrawSphere(ce.pos, ce.radius, col);
			if (ce.signalled)
			{
				if (ce.type == AICO_SMALL)
					pRenderer->DrawLabel(ce.pos, 1.1f, "SMALL\nR=%.1f", ce.affectRadius);
				else if (ce.type == AICO_MEDIUM)
					pRenderer->DrawLabel(ce.pos, 1.1f, "MEDIUM\nR=%.1f", ce.affectRadius);
				else
					pRenderer->DrawLabel(ce.pos, 1.1f, "LARGE\nR=%.1f", ce.affectRadius);
			}
		}
	}

	// Draw bullet events
	if (m_cvDebugDrawBulletEvents->GetIVal() > 0)
	{
		for (unsigned i = 0, ni = m_bulletEvents.size(); i < ni; ++i)
		{
			SAIBulletEvent& be = m_bulletEvents[i];
			float a = 1.0f - clamp(be.t / be.tmax, 0.0f, 1.0f);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(be.pos, 0.15f, ColorB(0,196,0));
			DebugDrawWireSphere(pRenderer, be.pos, be.radius, ColorB(128,196,128,(uint8)(255*a)));

			if (IEntity* pShooter = gEnv->pEntitySystem->GetEntity(be.shooterId))
				if (pShooter->GetAI())
					pRenderer->GetIRenderAuxGeom()->DrawLine(pShooter->GetAI()->GetPos(), ColorB(128,128,128,0), be.pos, ColorB(128,128,128,128));
		}

		for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
		{
			CPuppet* pReceiverPuppet = m_enabledPuppetsSet[i];
			AABB bounds;
			pReceiverPuppet->GetEntity()->GetWorldBounds(bounds);
			pRenderer->GetIRenderAuxGeom()->DrawAABB(bounds, false, ColorB(128,128,128,128), eBBD_Faceted);
		}

		for (unsigned i = 0, ni = m_bulletEvents.size(); i < ni; ++i)
		{
			SAIBulletEvent& be = m_bulletEvents[i];
			float a = 1.0f - clamp(be.t / be.tmax, 0.0f, 1.0f);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(be.pos, be.radius, ColorB(128,255,128,(uint8)(128*a)));
		}
	}

	// Draw sound events
	if (m_cvDebugDrawSoundEvents->GetIVal() > 0)
	{
		int mode = m_cvDebugDrawSoundEvents->GetIVal();
		for (unsigned i = 0, ni = m_soundEvents.size(); i < ni; ++i)
		{
			SAISoundEvent& se = m_soundEvents[i];

			if (mode == 2 && se.type != AISE_GENERIC) continue;
			if (mode == 3 && se.type != AISE_COLLISION && se.type != AISE_COLLISION_LOUD) continue;
			if (mode == 4 && se.type != AISE_MOVEMENT && se.type != AISE_MOVEMENT_LOUD) continue;
			if (mode == 5 && se.type != AISE_WEAPON) continue;
			if (mode == 6 && se.type != AISE_EXPLOSION) continue;

			float a = 1.0f - clamp(se.t / se.tmax, 0.0f, 1.0f);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(se.pos, 0.15f, ColorB(0,96,128));
			pRenderer->GetIRenderAuxGeom()->DrawLine(se.pos, ColorB(0,96,128), se.pos+Vec3(0,0,se.radius), ColorB(32,196,255,(uint8)(255*a)));
			DebugDrawWireSphere(pRenderer, se.pos, se.radius, ColorB(32,196,255,(uint8)(255*a)));

			if (IEntity* pSender = gEnv->pEntitySystem->GetEntity(se.senderId))
				if (pSender->GetAI())
					pRenderer->GetIRenderAuxGeom()->DrawLine(pSender->GetAI()->GetPos(), ColorB(128,128,128,0), se.pos, ColorB(128,128,128,128));

			if (se.type == AISE_GENERIC)
				pRenderer->DrawLabel(se.pos, 1.1f, "GENERIC\nR=%.1f", se.radius);
			else if (se.type == AISE_MOVEMENT)
				pRenderer->DrawLabel(se.pos, 1.1f, "MOVEMENT\nR=%.1f", se.radius);
			else if (se.type == AISE_WEAPON)
				pRenderer->DrawLabel(se.pos, 1.1f, "WEAPON\nR=%.1f", se.radius);
			else if (se.type == AISE_EXPLOSION)
				pRenderer->DrawLabel(se.pos, 1.1f, "EXPLOSION\nR=%.1f", se.radius);
		}

/*		for (unsigned i = 0, ni = m_soundEvents.size(); i < ni; ++i)
		{
			SAISoundEvent& se = m_soundEvents[i];
			if (mode == 2 && se.type != AISE_GENERIC) continue;
			if (mode == 3 && se.type != AISE_MOVEMENT) continue;
			if (mode == 4 && se.type != AISE_WEAPON) continue;
			if (mode == 5 && se.type != AISE_EXPLOSION) continue;

			float a = 1.0f - Clamp(se.t / se.tmax, 0.0f, 1.0f);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(se.pos, se.radius, ColorB(32,196,255,(uint8)(128*a)));
		}*/
	}

	// Draw grenade events
	if (m_cvDebugDrawGrenadeEvents->GetIVal() > 0)
	{
		for (unsigned i = 0, ni = m_grenadeEvents.size(); i < ni; ++i)
		{
			SAIGrenadeEvent& ge = m_grenadeEvents[i];

			ColorB col(240,32,0);

			pRenderer->GetIRenderAuxGeom()->DrawSphere(ge.pos, 0.15f, col);
			DebugDrawWireSphere(pRenderer, ge.pos, ge.radius, col);

			if (ge.grenadeId)
			{
				if (IEntity* pGrenade = gEnv->pEntitySystem->GetEntity(ge.grenadeId))
					pRenderer->GetIRenderAuxGeom()->DrawLine(pGrenade->GetWorldPos(), ColorB(128,128,128,0), ge.pos, ColorB(128,128,128,200));
			}

			pRenderer->DrawLabel(ge.pos, 1.1f, "GRENADE");
		}
	}

	// Player actions
	if (m_cvDebugDrawPlayerActions->GetIVal() > 0)
	{
		CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
		if (pPlayer)
			pPlayer->DebugDraw(pRenderer);
	}

	if (m_cvDebugDrawCrowdControl->GetIVal() > 0)
		DebugDrawCrowdControl(pRenderer);

	if (m_cvDebugDrawBannedNavsos->GetIVal() > 0)
	{
		// Draw banned smartobjects.
		for (SmartObjectFloatMap::iterator it = m_bannedNavSmartObjects.begin(); it != m_bannedNavSmartObjects.end(); ++it)
		{
			IEntity* pEnt = it->first->GetEntity();
			m_pSmartObjects->DrawTemplate(it->first, false);
			if (pEnt)
				pRenderer->DrawLabel(it->first->GetPos(), 1.2f, "%s\nBanned %.1fs", pEnt->GetName(), it->second);
			else
				pRenderer->DrawLabel(it->first->GetPos(), 1.2f, "Banned %.1fs", it->second);
		}
	}

	// Restore aux render flags.
	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
}

void CAISystem::DebugDrawCrowdControl(IRenderer *pRenderer)
{
	Vec3 camPos = GetDebugCameraPos();

	VectorSet<const CAIActor*>	avoidedActors;

	AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET);
	for (; ai != m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;
		CAIObject* obj = ai->second;
		CPuppet* pPuppet = obj->CastToCPuppet();
		if (!pPuppet)
			continue;
		if (!pPuppet->IsEnabled())
			continue;
		if (!pPuppet->m_steeringEnabled)
			continue;
		if (Distance::Point_PointSq(camPos, pPuppet->GetPos()) > sqr(150.0f))
			continue;

		Vec3 center = pPuppet->GetPhysicsPos() + Vec3(0,0,0.75f);
		const float rad = pPuppet->GetMovementAbility().pathRadius;

		pPuppet->m_steeringOccupancy.DebugDraw(center, ColorB(0,196,255,240), pRenderer);

		for (unsigned i = 0, ni = pPuppet->m_steeringObjects.size(); i < ni; ++i)
		{
			const CAIActor* pActor = pPuppet->m_steeringObjects[i]->CastToCAIActor();
			avoidedActors.insert(pActor);
		}
	}

	for (unsigned i = 0, ni = avoidedActors.size(); i < ni; ++i)
	{
		const CAIActor* pActor = avoidedActors[i];
		if (pActor->GetType() == AIOBJECT_VEHICLE)
		{
			IEntity* pActorEnt = pActor->GetEntity();
			AABB localBounds;
			pActorEnt->GetLocalBounds(localBounds);
			const Matrix34& tm = pActorEnt->GetWorldTM();

			SAIRect3 r;
			GetFloorRectangleFromOrientedBox(tm, localBounds, r);
			// Extend the box based on velocity.
			Vec3 vel = pActor->GetVelocity();
			float speedu = r.axisu.Dot(vel) * 0.25f;
			float speedv = r.axisv.Dot(vel) * 0.25f;
			if (speedu > 0)
				r.max.x += speedu;
			else
				r.min.x += speedu;
			if (speedv > 0)
				r.max.y += speedv;
			else
				r.min.y += speedv;

			Vec3 rect[4];
			rect[0] = r.center + r.axisu * r.min.x + r.axisv * r.min.y;
			rect[1] = r.center + r.axisu * r.max.x + r.axisv * r.min.y;
			rect[2] = r.center + r.axisu * r.max.x + r.axisv * r.max.y;
			rect[3] = r.center + r.axisu * r.min.x + r.axisv * r.max.y;

			pRenderer->GetIRenderAuxGeom()->DrawPolyline(rect, 4, true, ColorB(0,196,255,128));
		}
		else
		{
			Vec3 pos = avoidedActors[i]->GetPhysicsPos() + Vec3(0,0,0.5f);
			const float rad = avoidedActors[i]->GetMovementAbility().pathRadius;
			DebugDrawCircleOutline(pRenderer, pos, rad, ColorB(0,196,255,128));
		}
	}

}

static const char* GetMovemengtUrgencyLabel(int idx)
{
	switch(idx > 0 ? idx : -idx)
	{
	case 0: return "Zero"; break;
	case 1: return "Slow"; break;
	case 2: return "Walk"; break;
	case 3: return "Run"; break;
	default: break;
	}
	return "Sprint"; 
}

void CAISystem::DebugDrawAdaptiveUrgency(IRenderer *pRenderer) const
{
	Vec3 camPos = GetDebugCameraPos();

	int mode = m_cvDebugDrawAdaptiveUrgency->GetIVal();

	AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET);
	for (; ai != m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;
		CAIObject* obj = ai->second;
		CPuppet* pPuppet = obj->CastToCPuppet();
		if (!pPuppet)
			continue;
		if (!pPuppet->IsEnabled())
			continue;
		if (Distance::Point_PointSq(camPos, pPuppet->GetPos()) > sqr(150.0f))
			continue;

		if (pPuppet->m_adaptiveUrgencyScaleDownPathLen <= 0.0001f)
			continue;

		int minIdx = MovementUrgencyToIndex(pPuppet->m_adaptiveUrgencyMin);
		int maxIdx = MovementUrgencyToIndex(pPuppet->m_adaptiveUrgencyMax);
		int curIdx = MovementUrgencyToIndex(pPuppet->GetState()->fMovementUrgency);

		SAIBodyInfo bi;
		AIAssert(pPuppet->GetProxy());
		pPuppet->GetProxy()->QueryBodyInfo(bi);
		bi.stanceSize.Move(pPuppet->GetPhysicsPos());
		Vec3 top = bi.stanceSize.GetCenter() + Vec3(0,0,bi.stanceSize.GetSize().z/2 + 0.3f);

		if (mode > 1)
			pRenderer->GetIRenderAuxGeom()->DrawAABB(bi.stanceSize, false, ColorB(240,220,0,128), eBBD_Faceted);

		if (curIdx < maxIdx)
		{
			pRenderer->GetIRenderAuxGeom()->DrawAABB(bi.stanceSize, true, ColorB(240,220,0,128), eBBD_Faceted);
			pRenderer->DrawLabel(top, 1.1f, "%s -> %s\n%d", GetMovemengtUrgencyLabel(maxIdx), GetMovemengtUrgencyLabel(curIdx), maxIdx - curIdx);
		}
	}
}

static void DrawRadarCircle(IRenderer *pRenderer, const Vec3& pos, float radius, const ColorB& col, const Matrix34& world, const Matrix34& screen)
{
	IRenderAuxGeom * pAux(pRenderer->GetIRenderAuxGeom());
	Vec3	last;

	float r = world.TransformVector(Vec3(radius,0,0)).GetLength();
	unsigned n = (unsigned)((r * gf_PI2) / 5.0f);
	if(n < 5) n = 5;
	if(n > 50) n = 50;

	for(unsigned i = 0; i < n; i++)
	{
		float	a = (float)i / (float)(n - 1) * gf_PI2;
		Vec3	p = world.TransformPoint(pos + Vec3(cosf(a) * radius, sinf(a) * radius, 0));
		if(i > 0)
			pAux->DrawLine(screen.TransformPoint(last), col, screen.TransformPoint(p), col);
		last = p;
	}
}

void CAISystem::DrawRadarPath(IRenderer *pRenderer, CPuppet* puppet, const Matrix34& world, const Matrix34& screen)
{
	const char *pName=m_cvDrawPath->GetString();
	if(!pName )
		return;
	CAIObject *pTargetObject = GetAIObjectByName(pName);
	if (pTargetObject)
	{
		CPuppet *pTargetPuppet = pTargetObject->CastToCPuppet();
		if (!pTargetPuppet)
			return;
		DebugDrawPathSingle( pRenderer, pTargetPuppet );
		return;
	}
	if(strcmp(pName, "all"))
		return;

	IRenderAuxGeom * pAux(pRenderer->GetIRenderAuxGeom());

	// draw the first part of the path in a different colour
	if (!puppet->m_OrigPath.GetPath().empty())	
	{
		TPathPoints::const_iterator li,linext;
		li = puppet->m_OrigPath.GetPath().begin();
		linext = li;
		++linext;
		Vec3 endPt = puppet->m_Path.GetNextPathPos(ZERO);
		while (linext != puppet->m_OrigPath.GetPath().end())
		{
			Vec3 p0 = world.TransformPoint(li->vPos);
			Vec3 p1 = world.TransformPoint(linext->vPos);
			pAux->DrawLine(screen.TransformPoint(p0), ColorF(1,0,1,1), screen.TransformPoint(p1), ColorF(1,0,1,1));
			li=linext++;
		}
	}
}

void CAISystem::DebugDrawRadar(IRenderer *pRenderer)
{
	int	size = m_cvDrawRadar->GetIVal();
	if(size == 0)
		return;

	IRenderAuxGeom * pAux(pRenderer->GetIRenderAuxGeom());
	SAuxGeomRenderFlags	oldFlags = pAux->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags(e_Def2DPublicRenderflags);
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	pAux->SetRenderFlags(newFlags);

	int	w = pRenderer->GetWidth();
	int	h = pRenderer->GetHeight();

	int	centerx = w/2;
	int	centery = h/2;

	int	radarDist = m_cvDrawRadarDist->GetIVal();
	float	worldSize = radarDist * 2.0f;

/*	ColorB	col(255,0,0,128);
	ColorB	col2(255,255,0,128);
	pAux->DrawLine(Vec3(0,0,0), col, Vec3(0.5f,0.5f,0), col);
	pAux->DrawLine(Vec3(1,1,0), col2, Vec3(0.5f,0.5f,0), col2);
	pAux->DrawLine(Vec3(0,0,0), col, Vec3(100,200,0), col);*/

	Matrix34	worldToScreen;
	worldToScreen.SetIdentity();
	CCamera& cam = GetISystem()->GetViewCamera();
	Vec3	camPos = cam.GetPosition();
	Vec3	camForward = cam.GetMatrix().TransformVector(FORWARD_DIRECTION);
	float	rot(atan2f(camForward.x, camForward.y));
	// inv camera
//	worldToScreen.SetRotationZ(-rot);
	worldToScreen.AddTranslation(-camPos);
	worldToScreen = Matrix34::CreateRotationZ(rot) * worldToScreen;
	// world to screenspace conversion
	float	s = (float)size/worldSize;
	worldToScreen = Matrix34::CreateScale(Vec3(s,s,0)) * worldToScreen;
	// offset the position to upper right corner
	worldToScreen.AddTranslation(Vec3(centerx, centery,0));
	// Inverse Y
	worldToScreen = Matrix34::CreateScale(Vec3(1,-1,0)) * worldToScreen;
	worldToScreen.AddTranslation(Vec3(0, h,0));

	Matrix34	screenToNorm;
	screenToNorm.SetIdentity();
	// normalize to 0..1
	screenToNorm = Matrix34::CreateScale(Vec3(1.0f/(float)w, 1.0f/(float)h, 1)) * screenToNorm;

	// Draw 15m circle.
	DrawRadarCircle(pRenderer, camPos, 0.5f, ColorB(255, 255, 255, 128), worldToScreen, screenToNorm);
	for(unsigned i = 0; i < (radarDist / 5); i++)
	{
		ColorB	col(120, 120, 120, 64);
		if(i & 1) col.a = 128;
		DrawRadarCircle(pRenderer, camPos, (1 + i) * 5.0f, col, worldToScreen, screenToNorm);
	}

	ColorB	red(239, 50, 25);
	ColorB	yellow(255, 235, 15);
	ColorB	orange(255, 162, 0);
	ColorB	green(137, 226, 9);
	ColorB	blue(9, 121, 226);
	ColorB	white(255, 255, 255);

	// output all puppets
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
	{
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			CAIObject* obj = ai->second;
			CPuppet* puppet = obj->CastToCPuppet();
			if(!puppet)
				continue;

			if(Distance::Point_PointSq(obj->GetPos(), camPos) > sqr(radarDist * 1.25f))
				continue;

			float	rad(puppet->GetParameters().m_fPassRadius);

			Vec3	pos, forw, right;
			pos = worldToScreen.TransformPoint(obj->GetPos());
			forw = worldToScreen.TransformVector(obj->GetViewDir() * rad);
//			forw.NormalizeSafe();
			right.Set(-forw.y, forw.x, 0);

			ColorB	col(255,0,0,128);

			int alertness = puppet->GetProxy()->GetAlertnessState();
			if(alertness == 0)
				col = green;
			else if(alertness == 1)
				col = orange;
			else if(alertness == 2)
				col = red;
			col.a = 255;
			if(!puppet->IsEnabled())
				col.a = 64;

			float	arrowSize = 1.5f;
			pAux->DrawTriangle(screenToNorm.TransformVector(pos - forw*0.71f*arrowSize + right*0.71f*arrowSize), col,
				screenToNorm.TransformVector(pos - forw*0.71f*arrowSize - right*0.71f*arrowSize), col,
				screenToNorm.TransformVector(pos + forw*arrowSize), col);
			DrawRadarCircle(pRenderer, puppet->GetPos(), rad, ColorB(255,255,255,64), worldToScreen, screenToNorm);

			DrawRadarPath(pRenderer, puppet, worldToScreen, screenToNorm);

			if(puppet->GetState()->fire)
			{
				ColorF	fireCol(1, 0.1f, 0, 1);
				if(!puppet->IsAllowedToHitTarget())
					fireCol.Set(1,1,1,0.5f);
				Vec3	tgt = worldToScreen.TransformPoint(puppet->GetState()->vShootTargetPos);
				pAux->DrawLine(screenToNorm.TransformVector(pos), fireCol, screenToNorm.TransformVector(tgt), fireCol);
			}

			float	whiteF[4] = {1, 1, 1 ,1};
			float	blackF[4] = {0, 0, 0 ,0.7f};

			static char	szMsg[256] = "\0";

			float	accuracy = 0.0f;
			if(puppet->GetFireTargetObject())
				accuracy = puppet->GetAccuracy(puppet->GetFireTargetObject());

			if(puppet->GetProxy()->GetCurrentReadibilityName())
				_snprintf(szMsg, 256, "%s\nAcc:%.3f\nSay:%s", puppet->GetName(), accuracy, puppet->GetProxy()->GetCurrentReadibilityName());
			else if(!puppet->IsAllowedToHitTarget())
				_snprintf(szMsg, 256, "%s\nAcc:%.3f\nAMBIENT", puppet->GetName(), accuracy);
			else
				_snprintf(szMsg, 256, "%s\nAcc:%.3f\n", puppet->GetName(), accuracy);

			pRenderer->Draw2dLabel(pos.x+1, pos.y-1, 1.2f, blackF, true, "%s", szMsg);
			pRenderer->Draw2dLabel(pos.x, pos.y, 1.2f, whiteF, true, "%s", szMsg);
		}
	}

/*	// output all vehicles
	if ((ai = m_Objects.find(AIOBJECT_VEHICLE)) != m_Objects.end())
	{
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_VEHICLE)
				break;
			float dist2 = (GetCameraPos() - (ai->second)->GetPos()).GetLengthSquared();
			if(dist2>drawDist2)
				continue;
			DebugDrawAgent(pRenderer, ai->second);
		}
	}*/


	pAux->SetRenderFlags(oldFlags);
}

void CAISystem::DebugDrawDistanceLUT(IRenderer *pRenderer)
{
	if(m_cvDrawDistanceLUT->GetIVal() < 1)
		return;

/*
	IRenderAuxGeom * pAux(pRenderer->GetIRenderAuxGeom());
	SAuxGeomRenderFlags	oldFlags = pAux->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags(e_Def2DPublicRenderflags);
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	pAux->SetRenderFlags(newFlags);

	int	w = pRenderer->GetWidth();
	int	h = pRenderer->GetHeight();

	Matrix34	screenToNorm;
	screenToNorm.SetIdentity();
	// normalize to 0..1
	screenToNorm = Matrix34::CreateScale(Vec3(1.0f/(float)w, 1.0f/(float)h, 1)) * screenToNorm;

	AILinearLUT&	lut(m_VisDistLookUp);
	if(lut.GetSize() < 2)
	{
		pAux->SetRenderFlags(oldFlags);
		return;
	}

	const float	graphWidth = 400.0f;
	const float	graphHeight = 200.0f;

	float	sx = w/2 - graphWidth/2;
	float	sy = 50 + graphHeight;

	{
		Vec3	v0, v1;

		v0.Set(sx, sy, 0);
		v1.Set(sx+graphWidth, sy, 0);
		pRenderer->GetIRenderAuxGeom()->DrawLine(screenToNorm.TransformVector(v0), ColorB(255,255,255,128),
			screenToNorm.TransformVector(v1), ColorB(255,255,255,128));

		v0.Set(sx, sy-graphHeight/2, 0);
		v1.Set(sx+graphWidth, sy-graphHeight/2, 0);
		pRenderer->GetIRenderAuxGeom()->DrawLine(screenToNorm.TransformVector(v0), ColorB(255,255,255,64),
			screenToNorm.TransformVector(v1), ColorB(255,255,255,64));

		v0.Set(sx, sy-graphHeight, 0);
		v1.Set(sx+graphWidth, sy-graphHeight, 0);
		pRenderer->GetIRenderAuxGeom()->DrawLine(screenToNorm.TransformVector(v0), ColorB(255,255,255,128),
			screenToNorm.TransformVector(v1), ColorB(255,255,255,128));
	}

	float	scx = graphWidth / (lut.GetSize() - 1);
	float	scy = graphHeight / 100.0f;
	for(int i = 0; i < lut.GetSize(); ++i)
	{
		Vec3	v0(sx + i*scx, sy - lut.GetSampleValue(i)*scy, 0);
		if(i+1 < lut.GetSize())
		{
			Vec3	v1(sx + (i+1)*scx, sy - lut.GetSampleValue(i+1)*scy, 0);
			Vec3	v2(sx + i*scx, sy, 0);
			pRenderer->GetIRenderAuxGeom()->DrawLine(screenToNorm.TransformVector(v0), ColorB(255,255,255),
				screenToNorm.TransformVector(v1), ColorB(255,255,255));
			pRenderer->GetIRenderAuxGeom()->DrawLine(screenToNorm.TransformVector(v0), ColorB(255,255,255,32),
				screenToNorm.TransformVector(v2), ColorB(255,255,255,32));
		}

		float	col[4] = {1,1,1,1};
		pRenderer->Draw2dLabel(v0.x, v0.y - 10, 1.2f, col, true, "%.1f", lut.GetSampleValue(i));
	}

	// Draw the current value
	const char*	statsTargetName = m_cvStatsTarget->GetString();
	if(!statsTargetName)
	{
		pAux->SetRenderFlags(oldFlags);
		return;
	}
	CAIObject *pTargetObject = GetAIObjectByName(statsTargetName);
	if (!pTargetObject)
	{
		pAux->SetRenderFlags(oldFlags);
		return;
	}
	CPuppet *pTargetPuppet = pTargetObject->CastToCPuppet();
	if (!pTargetPuppet)
	{
		pAux->SetRenderFlags(oldFlags);
		return;
	}

	float fMaxPriority = -1.f;
	EventData maxEvent;
	CAIObject *pNextTarget = 0;
	PotentialTargetMap::iterator ei = pTargetPuppet->m_perceptionEvents.begin(), eiend = pTargetPuppet->m_perceptionEvents.end();
	for (; ei != eiend; ++ei)
	{
		const SAIPotentialTarget &ed = ei->second;
		// target selection based on priority
		if (ed.fPriority>fMaxPriority)
		{
			fMaxPriority = ed.fPriority;
			maxEvent = ed;
			if (ed.pDummyRepresentation)
				pNextTarget = ed.pDummyRepresentation;
			else
				pNextTarget = ei->first;
		}
	}

	if(pNextTarget)
	{
		float	d = Distance::Point_Point(pNextTarget->GetPos(), pTargetPuppet->GetPos());
		d = clamp(d / pTargetPuppet->GetParameters().m_PerceptionParams.sightRange, 0.0f, 1.0f);

		{
			Vec3	v0(sx + d*graphWidth, sy - (lut.GetValue(d) + 5)*scy, 0);
			Vec3	v1(sx + d*graphWidth, sy, 0);

			pRenderer->GetIRenderAuxGeom()->DrawLine(screenToNorm.TransformVector(v0), ColorB(255,0,0),
				screenToNorm.TransformVector(v1), ColorB(255,0,0));

			float	col[4] = {1,0,0,1};
			pRenderer->Draw2dLabel(v0.x, v0.y - 15, 1.5f, col, true, "%.1f", lut.GetValue(d));
		}
	}

	pAux->SetRenderFlags(oldFlags);
*/
}


struct SSlopeTriangle
{
	SSlopeTriangle(const Triangle& tri, float slope) : triangle(tri), slope(slope) {}
	SSlopeTriangle() {}
	Triangle triangle;
	float slope;
};

//====================================================================
// DebugDrawSteepSlopes
// Caches triangles first time this is enabled - then on subsequent
// calls just draws. Gets reset when the debug draw value changes
//====================================================================
void CAISystem::DebugDrawSteepSlopes(IRenderer *pRenderer)
{
	static std::vector<SSlopeTriangle> triangles;
	/// Current centre for the region we draw
	static Vec3 currentPos(0.0f, 0.0f, 0.0f);

	// Size of the area to calculate/use
	static float drawBoxWidth = 200.0f;
	// update when current position is greater than this (horizontally) 
	// distance from lastPos
	static float drawUpdateDist = drawBoxWidth * 0.25f;
	static float zOffset = 0.05f;

	Vec3 playerPos = GetDebugCameraPos();
	playerPos.z = 0.0f;

	if ( (playerPos - currentPos).GetLength() > drawUpdateDist )
	{
		currentPos = playerPos;
		triangles.resize(0); // force a refresh
	}
	if (m_cvDebugDraw->GetIVal() != 85)
	{
		triangles.resize(0);
		return;
	}

	float criticalSlopeUp = m_cvSteepSlopeUpValue->GetFVal();
	float criticalSlopeAcross = m_cvSteepSlopeAcrossValue->GetFVal();

	if (triangles.empty())
	{
		I3DEngine *pEngine = m_pSystem->GetI3DEngine();

		int terrainArraySize = pEngine->GetTerrainSize();
		float dx = pEngine->GetHeightMapUnitSize();

		float minX = currentPos.x - 0.5f * drawBoxWidth;
		float minY = currentPos.y - 0.5f * drawBoxWidth;
		float maxX = currentPos.x + 0.5f * drawBoxWidth;
		float maxY = currentPos.y + 0.5f * drawBoxWidth;
		int minIx = (int) (minX / dx);
		int minIy = (int) (minY / dx);
		int maxIx = (int) (maxX / dx);
		int maxIy = (int) (maxY / dx);
		Limit(minIx, 1, terrainArraySize-1);
		Limit(minIy, 1, terrainArraySize-1);
		Limit(maxIx, 1, terrainArraySize-1);
		Limit(maxIy, 1, terrainArraySize-1);

		// indices start at +1 so we can use the previous indices
		for (unsigned ix = minIx ; ix < maxIx ; ++ix)
		{
			for (unsigned iy = minIy ; iy < maxIy ; ++iy)
			{
				Vec3 v11(dx * ix, dx * iy, 0.0f);
				Vec3 v01(dx * (ix-1), dx * iy, 0.0f);
				Vec3 v10(dx * ix, dx * (iy-1), 0.0f);
				Vec3 v00(dx * (ix-1), dx * (iy-1), 0.0f);

				v11.z = zOffset + GetDebugDrawZ(v11, true);
				v01.z = zOffset + GetDebugDrawZ(v01, true);
				v10.z = zOffset + GetDebugDrawZ(v10, true);
				v00.z = zOffset + GetDebugDrawZ(v00, true);

				Vec3 n1 = ((v10 - v00) % (v01 - v00)).GetNormalized();
				float slope1 = fabs(sqrtf(1.0f - n1.z * n1.z) / n1.z);
				if (slope1 > criticalSlopeUp)
					triangles.push_back(SSlopeTriangle(Triangle(v00, v10, v01), slope1));
				else if (slope1 > criticalSlopeAcross)
					triangles.push_back(SSlopeTriangle(Triangle(v00, v10, v01), -slope1));

				Vec3 n2 = ((v11 - v10) % (v01 - v10)).GetNormalized();
				float slope2 = fabs(sqrtf(1.0f - n2.z * n2.z) / n2.z);
				if (slope2 > criticalSlopeUp)
					triangles.push_back(SSlopeTriangle(Triangle(v10, v11, v01), slope2));
				else if (slope2 > criticalSlopeAcross)
					triangles.push_back(SSlopeTriangle(Triangle(v10, v11, v01), -slope2));
			}
		}
	}

	unsigned numTris = triangles.size();
	for (unsigned i = 0 ; i < numTris ; ++i)
	{
		const SSlopeTriangle& tri = triangles[i];
		ColorB col;
		// convert slope into 0-1 (the z-value of a unit vector having that slope)
		float slope = tri.slope > 0.0f ? tri.slope : -tri.slope;
		float a = sqrtf(slope * slope / (1.0f + slope * slope));
		if (tri.slope > 0.0f)
			col.Set((1.0f - 0.5f * a) * 255, 0.0f, 0, 255);
		else
			col.Set(0.0f, (1.0f - 0.5f * a) * 255, 1.0f, 255);
		pRenderer->GetIRenderAuxGeom()->DrawTriangle(tri.triangle.v0, col, tri.triangle.v1, col, tri.triangle.v2, col);
	}
}

//===================================================================
// DebugDrawVegetationCollision
//===================================================================
void CAISystem::DebugDrawVegetationCollision(IRenderer *pRenderer)
{
  float range = m_cvDebugDrawVegetationCollisionDist->GetFVal();
  if (range < 0.1f)
    return;

	I3DEngine *pEngine = gEnv->p3DEngine;
	IRenderAuxGeom * pAux = pRenderer->GetIRenderAuxGeom();

  Vec3 playerPos = GetDebugCameraPos();
	playerPos.z = pEngine->GetTerrainElevation(playerPos.x, playerPos.y);

  AABB aabb(AABB::RESET);
  aabb.Add(playerPos + Vec3(range, range, range));
  aabb.Add(playerPos - Vec3(range, range, range));

  ColorB triCol(128, 255, 255);
  static float zOffset = 0.05f;

	IPhysicalEntity** pObstacles;
	int count = m_pWorld->GetEntitiesInBox(aabb.min, aabb.max, pObstacles, ent_static|ent_ignore_noncolliding);
	for (int i = 0 ; i < count ; ++i)
	{
    IPhysicalEntity *pPhysics = pObstacles[i];

    pe_status_pos status;
    status.ipart = 0;
    pPhysics->GetStatus( &status );

		Vec3 obstPos = status.pos;
		Matrix33 obstMat = Matrix33(status.q);
		obstMat *= status.scale;

		IGeometry* geom = status.pGeom;
		int type = geom->GetType();
		if (type == GEOM_TRIMESH)
		{
			const primitives::primitive * prim = geom->GetData();
			const mesh_data * mesh = static_cast<const mesh_data *>(prim);

			int numVerts = mesh->nVertices;
			int numTris = mesh->nTris;

    	static std::vector<Vec3>	vertices;
			vertices.resize( numVerts );

			for( int j = 0; j < numVerts; j++ )
				vertices[j] = obstPos + obstMat * mesh->pVertices[j];

			for (int j = 0; j < numTris; ++j )
			{
				int vidx0 = mesh->pIndices[j*3 + 0];
				int vidx1 = mesh->pIndices[j*3 + 1];
				int vidx2 = mesh->pIndices[j*3 + 2];

        Vec3 pt0 = vertices[vidx0];
        Vec3 pt1 = vertices[vidx1];
        Vec3 pt2 = vertices[vidx2];

        float z0 = pEngine->GetTerrainElevation(pt0.x, pt0.y);
        float z1 = pEngine->GetTerrainElevation(pt1.x, pt1.y);
        float z2 = pEngine->GetTerrainElevation(pt2.x, pt2.y);

        static float criticalAlt = 1.8f;
        if (pt0.z < z0 && pt1.z < z1 && pt2.z < z2)
          continue;
        if (pt0.z > z0 + criticalAlt && pt1.z > z1 + criticalAlt && pt2.z > z2 + criticalAlt)
          continue;
        pt0.z = z0 + zOffset;
        pt1.z = z1 + zOffset;
        pt2.z = z2 + zOffset;

			  pAux->DrawTriangle(pt0, triCol, pt1, triCol, pt2, triCol);
      }
    }
  }
}

struct SHidePos
{
  SHidePos(const Vec3 &pos = ZERO, const Vec3 &dir = ZERO) : pos(pos), dir(dir) {}
  Vec3 pos;
  Vec3 dir;
};

struct SVolumeFunctor
{
  SVolumeFunctor(std::vector<SHidePos> &hidePositions) : hidePositions(hidePositions) {}
  void operator()(SVolumeHideSpot& hs, float) {hidePositions.push_back(SHidePos(hs.pos, hs.dir));}
  std::vector<SHidePos> &hidePositions;
};

//====================================================================
// DebugDrawHideSpots
// need to evaluate all navigation/hide types
//====================================================================
void CAISystem::DebugDrawHideSpots(IRenderer *pRenderer)
{
  float range = m_cvDebugDrawHideSpotRange->GetFVal();
  if (range < 0.1f)
    return;
  
  Vec3 playerPos = GetDebugCameraPos();

  Vec3 groundPos = playerPos;
  I3DEngine *pEngine = gEnv->p3DEngine;
  groundPos.z = pEngine->GetTerrainElevation(groundPos.x, groundPos.y);
  pRenderer->GetIRenderAuxGeom()->DrawSphere(groundPos, 0.01f * (playerPos.z - groundPos.z), ColorB(255, 0, 0, 255));

  MultimapRangeHideSpots hidespots;
  MapConstNodesDistance traversedNodes;
  // can't get SO since we have no entity...
  GetHideSpotsInRange(hidespots, traversedNodes, playerPos, range, 
    IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE | 
    IAISystem::NAV_VOLUME | IAISystem::NAV_SMARTOBJECT, 0.0f, false, 0);

  /// for profiling (without draw overhead)
  static bool skipDrawing = false;
  if (skipDrawing)
    return;

  for (MultimapRangeHideSpots::const_iterator it = hidespots.begin() ; it != hidespots.end() ; ++it)
  {
    float distance = it->first;
    const SHideSpot &hs = it->second;

    const Vec3 &pos = hs.pos;
    Vec3 dir = hs.dir;

    static float radius = 0.3f;
    static float height = 0.3f;
    static float triHeight = 5.0f;
    int alpha = 255;
    if (hs.pObstacle && !hs.pObstacle->bCollidable)
    {
      alpha = 128;
    }
    else if (hs.pAnchorObject && hs.pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT_SECONDARY)
    {
      alpha = 128;
      dir.zero();
    }
    pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, 0.2f * radius, ColorB(0, 255, 0, alpha));
    if (dir.IsZero())
      pRenderer->GetIRenderAuxGeom()->DrawCone(pos + Vec3(0, 0, triHeight), Vec3(0, 0, -1), radius, triHeight, ColorB(0, 255, 0, alpha));
    else
      pRenderer->GetIRenderAuxGeom()->DrawCone(pos + height * dir, -dir, radius, height, ColorB(0, 255, 0, alpha));

  }

  float fColor[4] = {1, 1, 0, 1};
  for (MapConstNodesDistance::const_iterator it = traversedNodes.begin() ; it != traversedNodes.end() ; ++it)
  {
    const GraphNode *pNode = it->first;
    float dist = it->second;
    Vec3 pos = pNode->GetPos();
    float radius = 0.3f;
    float coneHeight = 1.0f;
    pRenderer->GetIRenderAuxGeom()->DrawCone(pos + Vec3(0, 0, coneHeight), Vec3(0, 0, -1), radius, coneHeight, ColorB(255, 255, 0, 100));
    pRenderer->DrawLabelEx(pos + Vec3(0, 0, 0.3f), 1.0f, fColor, true, false, "%5.2f", dist);
  }

}

//====================================================================
// DebugDrawDynamicHideObjects
//====================================================================
void CAISystem::DebugDrawHashSpace(IRenderer *pRenderer)
{
	static bool validate = true;

	const char* szEntityName = m_cvDebugDrawHashSpaceAround->GetString();
	if (!szEntityName)
	{
		validate = true;
		return;
	}
	int len = strlen(szEntityName);
	if (len == 0 || (szEntityName[0] == '0' && len == 1))
	{
		validate = true;
		return;
	}

	// Validate the hash space
	if (validate)
	{
		m_pGraph->ValidateHashSpace();
		validate = false;
	}

	const CAllNodesContainer& allNodes = m_pGraph->GetAllNodes();
	const CHashSpace<CAllNodesContainer::SGraphNodeRecord, CAllNodesContainer::GraphNodeHashSpaceTraits>& hashSpace = allNodes.GetHashSpace();

	// Collect some stats.
	unsigned maxObjectsPerBucket = 0;
	unsigned numFilledBuckets = 0;
	float avgObjectsPerBucket = 0;
	for (unsigned i = 0, ni = hashSpace.GetBucketCount(); i < ni; ++i)
	{
		unsigned objs = hashSpace.GetObjectCountInBucket(i);
		maxObjectsPerBucket = max(maxObjectsPerBucket, objs);
		if (objs)
			numFilledBuckets++;
		avgObjectsPerBucket += objs;
	}
	avgObjectsPerBucket /= (float)hashSpace.GetBucketCount();
	float maxObjScale = !maxObjectsPerBucket ? 1.0f : 1.0f/(float)maxObjectsPerBucket;

	// Draw the hash buckets around the player.
	Vec3 pos(0,0,0);
	IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(szEntityName);
	if (pEntity)
	{
		pos = pEntity->GetWorldPos();
		int i, j, k;
		hashSpace.GetIJKFromPosition(pos, i, j, k);
		AABB cellBounds;
		for (int z = -1; z <= 1; ++z)
		{
			for (int y = -2; y <= 2; ++y)
			{
				for (int x = -2; x <= 2; ++x)
				{
					unsigned bucket = hashSpace.GetHashBucketIndex(i+x, j+y, k+z);
					hashSpace.GetAABBFromIJK(i+x, j+y, k+z, cellBounds);
					unsigned objects = hashSpace.GetObjectCountInBucket(bucket);

					// 'Fullness'
					Vec3 center = cellBounds.GetCenter();
					Vec3 size = cellBounds.GetSize() * (objects*maxObjScale*0.5f);

					if (x == 0 && y == 0 && z == 0)
					{
						// Check how many points in the bucket belongs to his location
						unsigned local = 0;
						float localPercent = 0;
						for (unsigned m = 0; m < objects; ++m)
						{
							const CAllNodesContainer::SGraphNodeRecord& nodeRec = hashSpace.GetObjectInBucket(m, bucket);
							Vec3 nodePos = nodeRec.GetPos(GetAISystem()->GetGraph()->GetNodeManager());
							int ii, jj, kk;
							hashSpace.GetIJKFromPosition(nodePos, ii, jj, kk);
							if (ii == (i+x) && jj == (j+y) && kk == (k+z))
							{
								pRenderer->GetIRenderAuxGeom()->DrawSphere(nodePos, 0.05f, ColorB(255,64,0));
								local++;
							}
						}
						if (objects)
							localPercent = (float)local/(float)objects;

						// Grid
						pRenderer->GetIRenderAuxGeom()->DrawAABB(cellBounds, false, ColorB(255,255,255,64), eBBD_Faceted);
						pRenderer->DrawLabel(center, 1.5f, "Bucket: %d\nObjects: %d\nLocal: %d (%.3f)", bucket, objects, local, localPercent);
						pRenderer->GetIRenderAuxGeom()->DrawAABB(AABB(center-size, center+size), true, ColorB(0,196,255,255), eBBD_Faceted);
					}
					else
					{
						pRenderer->DrawLabel(center, 1.0f, "%d", objects);
						pRenderer->GetIRenderAuxGeom()->DrawAABB(AABB(center-size, center+size), true, ColorB(0,196,255,128), eBBD_Faceted);
					}
				}
			}
		}
	}

	// Test GetAllNodes performance.
	const unsigned testCount = 100;
	static Vec3 testPoints[testCount];
	const float testRange = 20.0f;	// common range for hidepoint and get enclosing query.
	static bool testPointsInitialized = false;

	if (!testPointsInitialized)
	{
		for (unsigned i = 0; i < testCount; ++i)
		{
			float a = ai_frand() * gf_PI2;
			float d = ((1+(ai_rand() % 20)) / 20.0f) * 15.0f;
			testPoints[i].Set(cosf(a)*d, sinf(a)*d, 0);
		}
		testPointsInitialized = true;
	}

	// Test get all nodes
	CTimeValue startTime = gEnv->pTimer->GetAsyncTime();
	static std::vector< std::pair<float, unsigned> > testNodes;
	for (unsigned i = 0; i < testCount; ++i)
	{
		testNodes.clear();
		allNodes.GetAllNodesWithinRange(testNodes, pos+testPoints[i], testRange, IAISystem::NAVMASK_ALL);
	}
	float testElapsedMs = (gEnv->pTimer->GetAsyncTime() - startTime).GetMilliSeconds();


	// Check the node distribution.
	SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags(e_Def2DPublicRenderflags);
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

	const Vec3	u(1,0,0);
	const Vec3	v(0,-1,0);
	const Vec3	w(0,0,1);

	float	sizex = 0.9f;
	float	sizey = 0.2f;

	Vec3	orig = Vec3(0.05f, 1-0.05f, 0);

	// dim BG
/*	ColorB	bg1(0,0,0,160); ColorB	bg2(128,128,128,128);
	pRenderer->GetIRenderAuxGeom()->DrawTriangle(orig, bg1, orig+u*sizex, bg1, orig+u*sizex+v*sizey, bg2);
	pRenderer->GetIRenderAuxGeom()->DrawTriangle(orig, bg1, orig+u*sizex+v*sizey, bg2, orig+v*sizey, bg2);*/

	float scale = maxObjectsPerBucket ? 1.0f/(float)maxObjectsPerBucket : 1.0f;

	static std::vector<unsigned> values;
	static std::vector<Vec3> points;
	unsigned valueCount = hashSpace.GetBucketCount();
	unsigned shift = 0;
	while ((valueCount >> shift) > 1024)
		shift++;
	values.resize(hashSpace.GetBucketCount() >> shift);
	points.resize(hashSpace.GetBucketCount() >> shift);
	for (unsigned i = 0, ni = values.size(); i < ni; ++i)
		values[i] = 0;

	for (unsigned i = 0, ni = hashSpace.GetBucketCount(); i < ni; ++i)
	{
		unsigned j = i >> shift;
		values[j] = max(values[j], hashSpace.GetObjectCountInBucket(i));
	}

	for (unsigned i = 0, ni = values.size(); i < ni; ++i)
	{
		float a = (float)i / (float)values.size();
		points[i] = orig + u*sizex*a + v*sizey*values[i]*scale;
	}

	// Draw value lines
	pRenderer->GetIRenderAuxGeom()->DrawLine(orig, ColorB(0,0,0,128), orig + u*sizex, ColorB(0,0,0,128));
	pRenderer->GetIRenderAuxGeom()->DrawLine(orig + v*sizey*1.0f, ColorB(0,0,0,128), orig + u*sizex + v*sizey*1.0f, ColorB(0,0,0,128));

	pRenderer->GetIRenderAuxGeom()->DrawPolyline(&points[0], points.size(), false, ColorB(255,255,255,160));

	// Stats
	int sw = pRenderer->GetWidth();
	int sh = pRenderer->GetHeight();

	float	white[4] = {1, 1, 1, 1};

	float y = (orig.y-sizey)*sh-90.0f;
	pRenderer->Draw2dLabel(orig.x*sw+3, y, 1.4f, white, false, "Nodes: %d", hashSpace.GetNumObjects());
	y += 20.0f;
	pRenderer->Draw2dLabel(orig.x*sw+3, y, 1.2f, white, false, "Buckets: %d/%d  Usage: %.2f  Mem: %d kB", numFilledBuckets, hashSpace.GetBucketCount(), (float)numFilledBuckets/(float)hashSpace.GetBucketCount()*100.0f, (hashSpace.MemStats()+1023)/1024);
	y += 20.0f;
	pRenderer->Draw2dLabel(orig.x*sw+3, y, 1.2f, white, false, "Max/Bucket: %d", maxObjectsPerBucket);
	y += 20.0f;
	pRenderer->Draw2dLabel(orig.x*sw+3, y, 1.2f, white, false, "Avg/Bucket: %.2f", avgObjectsPerBucket);
	y += 20.0f;

	// Print out the test result
	pRenderer->Draw2dLabel(orig.x*sw+3, (orig.y-sizey)*sh-150.0f, 1.5f, white, false, "Test GetAllNodes(%d) @ (%.1f, %.1f, %.1f): %.2fms", testCount, pos.x, pos.y, pos.z, testElapsedMs);

	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
}

//====================================================================
// DebugDrawDynamicHideObjects
//====================================================================
void CAISystem::DebugDrawDynamicHideObjects(IRenderer *pRenderer)
{
	float range = m_cvDebugDrawDynamicHideObjectsRange->GetFVal();
	if (range < 0.1f)
		return;

	Vec3 cameraPos = GetISystem()->GetViewCamera().GetPosition();
	Vec3 cameraDir = GetISystem()->GetViewCamera().GetViewdir();

	Vec3 pos = cameraPos + cameraDir * (range/2);
	Vec3 size(range/2, range/2, range/2);

	SEntityProximityQuery query;
	query.box.min = pos - size;
	query.box.max = pos + size;
	query.nEntityFlags = ENTITY_FLAG_AI_HIDEABLE; // Filter by entity flag.

	gEnv->pEntitySystem->QueryProximity(query);
	for(int i = 0; i < query.nCount; ++i)
	{
		IEntity* pEntity = query.pEntities[i];
		if (!pEntity) continue;

		AABB bbox;
		pEntity->GetLocalBounds(bbox);
		pRenderer->GetIRenderAuxGeom()->DrawAABB(bbox, pEntity->GetWorldTM(), true, ColorB(255,0,0,128), eBBD_Faceted);
	}

	m_dynHideObjectManager.DebugDraw(pRenderer);
}

//====================================================================
// DebugDrawGraphErrors
//====================================================================
void CAISystem::DebugDrawGraphErrors(IRenderer *pRenderer, CGraph* pGraph) const
{
	unsigned numErrors = pGraph->mBadGraphData.size();

	std::vector<Vec3> positions;
	static float minRadius = 0.1f;
	static float maxRadius = 2.0f;
	static int numCircles = 2;
	for (unsigned iError = 0 ; iError < numErrors ; ++iError)
	{
		const CGraph::SBadGraphData & error = pGraph->mBadGraphData[iError];
		Vec3 errorPos1 = error.mPos1;
		errorPos1.z = GetDebugDrawZ(errorPos1, true);
		Vec3 errorPos2 = error.mPos2;
		errorPos2.z = GetDebugDrawZ(errorPos2, true);

		positions.push_back(errorPos1);

		switch (error.mType)
		{
		case CGraph::SBadGraphData::BAD_PASSABLE:
			DebugDrawCircles(pRenderer, errorPos1, minRadius, maxRadius, numCircles, Vec3(1, 0, 0), Vec3(1, 1, 0));
			DebugDrawCircles(pRenderer, errorPos2, minRadius, maxRadius, numCircles, Vec3(1, 0, 0), Vec3(1, 1, 0));
			pRenderer->GetIRenderAuxGeom()->DrawLine(errorPos1, ColorF(1, 1, 0, 1), errorPos2, ColorF(1, 1, 0, 1));
			break;
		case CGraph::SBadGraphData::BAD_IMPASSABLE:
			DebugDrawCircles(pRenderer, errorPos1, minRadius, maxRadius, numCircles, Vec3(0, 1, 0), Vec3(1, 1, 0));
			DebugDrawCircles(pRenderer, errorPos2, minRadius, maxRadius, numCircles, Vec3(0, 1, 0), Vec3(1, 1, 0));
			pRenderer->GetIRenderAuxGeom()->DrawLine(errorPos1, ColorF(1, 1, 0, 1), errorPos2, ColorF(1, 1, 0, 1));
			break;
		}
	}
	static float errorRadius = 10.0f;
	// draw the basic graph just in this region
	DebugDrawGraph(pRenderer, m_pGraph, &positions, errorRadius);
}

//====================================================================
// CheckDistance
//====================================================================
static inline bool CheckDistance(const Vec3 pos1, const std::vector<Vec3> * focusPositions, float radius)
{
	if (!focusPositions)
		return true;
	const std::vector<Vec3> & positions = *focusPositions;

	for (unsigned i = 0 ; i < positions.size() ; ++i)
	{
		Vec3 delta = pos1 - positions[i];
		delta.z = 0.0f;
		if (delta.GetLengthSquared() < radius*radius)
			return true;
	}
	return false;
}

// A bit ugly, but make this global - it caches the debug graph that needs to be drawn.
// If it's empty then DebugDrawGraph tries to fill it. Otherwise we just draw it
// It will get zeroed when the graph is regenerated.
std::vector<const GraphNode *> g_DebugGraphNodesToDraw;
static CGraph* lastDrawnGraph = 0; // detects swapping between hide and nav

//====================================================================
// DebugDrawGraph
//====================================================================
void CAISystem::DebugDrawGraph(IRenderer *pRenderer, CGraph* pGraph, const std::vector<Vec3> * focusPositions, float focusRadius) const
{
	if (pGraph != lastDrawnGraph)
	{
		lastDrawnGraph = pGraph;
		g_DebugGraphNodesToDraw.clear();
	}

	if (g_DebugGraphNodesToDraw.empty())
	{
		if (!pGraph->m_pSafeFirst)
			return;

		if (!pGraph->m_pSafeFirst->firstLinkIndex)
			return;

		CAllNodesContainer& allNodes = pGraph->GetAllNodes();
		CAllNodesContainer::Iterator it(allNodes, NAV_TRIANGULAR | NAV_WAYPOINT_HUMAN | NAV_WAYPOINT_3DSURFACE | NAV_VOLUME | NAV_ROAD | NAV_SMARTOBJECT);
		while (unsigned currentNodeIndex = it.Increment())
		{
			GraphNode* pCurrent = pGraph->GetNodeManager().GetNode(currentNodeIndex);

			g_DebugGraphNodesToDraw.push_back(pCurrent);
		}
	}
	// now just render
	static bool renderPassRadii = false;
	static float ballRad = 0.04f;

	unsigned nNodes = g_DebugGraphNodesToDraw.size();
	for (unsigned iNode = 0 ; iNode < nNodes ; ++iNode)
	{
		const GraphNode * node = g_DebugGraphNodesToDraw[iNode];
		AIAssert(node);

		// draw the graph node itself in white if outside, black if in
		ColorF col(0, 0, 0);
		if (node->navType == IAISystem::NAV_TRIANGULAR)
		{
			if (node->GetTriangularNavData()->isForbidden || node->GetTriangularNavData()->isForbiddenDesigner)
			{
				col.r = node->GetTriangularNavData()->isForbidden ? 0.5f : 0;
				col.g = node->GetTriangularNavData()->isForbiddenDesigner ? 0.3f : 0;
			}
			else
				col.Set(1,1,1);
		}
		Vec3 pos = node->GetPos();
		pos.z = GetDebugDrawZ(pos, node->navType == IAISystem::NAV_TRIANGULAR);
    if (node->navType == IAISystem::NAV_TRIANGULAR)
    {
		  if (CheckDistance(pos, focusPositions, focusRadius))
		  {
			  pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, ballRad, col);

			  unsigned numVertices = node->GetTriangularNavData()->vertices.size();
			  // connect and draw the vertices in blue
			  col.Set(0, 0, 1);
			  for (unsigned iV = 0 ; iV < numVertices ; ++iV)
			  {
				  unsigned iVNext = (iV + 1) % numVertices;
				  Vec3 v0 = ((CVertexList&)m_VertexList).GetVertex(node->GetTriangularNavData()->vertices[iV]).vPos;
				  Vec3 v1 = ((CVertexList&)m_VertexList).GetVertex(node->GetTriangularNavData()->vertices[iVNext]).vPos;
				  v0.z = GetDebugDrawZ(v0, true);
				  v1.z = GetDebugDrawZ(v1, true);
				  pRenderer->GetIRenderAuxGeom()->DrawLine(v0, ColorF(0, 0.5, 0), v1, ColorF(0, 0.5, 0));
				  pRenderer->GetIRenderAuxGeom()->DrawSphere(v0, ballRad, col);
			  }
		  } // range check
    }// triangular
		for (unsigned link = node->firstLinkIndex; link; link = pGraph->GetLinkManager().GetNextLink(link))
		{
			unsigned nextIndex = pGraph->GetLinkManager().GetNextNode(link);
			const GraphNode * next = pGraph->GetNodeManager().GetNode(nextIndex);
			AIAssert(next);

			ColorF endCol;
			if (pGraph->GetLinkManager().GetRadius(link) > 0.0f)
				endCol = ColorF(1, 1, 1);
			else
				endCol = ColorF(0, 0, 0);

			if (CheckDistance(pos, focusPositions, focusRadius))
			{
				Vec3 v0 = node->GetPos();
				Vec3 v1 = next->GetPos();
				v0.z = GetDebugDrawZ(v0, node->navType == IAISystem::NAV_TRIANGULAR);
				v1.z = GetDebugDrawZ(v1, node->navType == IAISystem::NAV_TRIANGULAR);
				if (pGraph->GetLinkManager().GetStartIndex(link) != pGraph->GetLinkManager().GetEndIndex(link))
				{
					Vec3 mid = pGraph->GetLinkManager().GetEdgeCenter(link);
					mid.z = GetDebugDrawZ(mid, node->navType == IAISystem::NAV_TRIANGULAR);
					pRenderer->GetIRenderAuxGeom()->DrawLine(v0, endCol, mid, ColorF(0, 1, 1));
					if (renderPassRadii) 
						pRenderer->DrawLabel(mid,1,"%.2f",pGraph->GetLinkManager().GetRadius(link));

					int debugDrawVal = m_cvDebugDraw->GetIVal();
					if (debugDrawVal == 179)
						pRenderer->DrawLabel(mid,2,"%x",pGraph->LinkId (link));
					else if (debugDrawVal == 279)
					{
						float waterDepth = pGraph->GetLinkManager().GetMaxWaterDepth(link);
						if (waterDepth > 0.6f)
						{
							float col[4] = { 1,0,0,1 };
							pRenderer->DrawLabelEx(mid,1,col, true, false,"%.2f",waterDepth);
						} else
						{
							float col[4] = { 1,1,1,1 };
							pRenderer->DrawLabelEx(mid,1,col, true, false,"%.2f",waterDepth);
						}
					}
				}
				else
				{
					pRenderer->GetIRenderAuxGeom()->DrawLine(v0, endCol, v1, endCol);
					if (renderPassRadii)
						pRenderer->DrawLabel(0.5f * (v0 + v1),1,"%.2f",pGraph->GetLinkManager().GetRadius(link));
				}
			} // range check
		}
	}

  DebugDrawForbidden(pRenderer);

	// draw broken areas
	if (!m_brokenRegions.m_regions.empty())
	{
		float nRegions = (float) m_brokenRegions.m_regions.size();
		std::list<CGraph::CRegion>::const_iterator regionIt;
		float fRegion = 0;
		for (regionIt = m_brokenRegions.m_regions.begin() ; regionIt != m_brokenRegions.m_regions.end() ; ++regionIt, fRegion += 1.0f)
		{
			const CGraph::CRegion & region = *regionIt;
			ColorF col(1.0f, 0.5f, 0.3f);
			for (std::list<Vec3>::const_iterator it = region.m_outline.begin() ; it != region.m_outline.end() ; ++it)
			{
				std::list<Vec3>::const_iterator itNext = it;
				++itNext;
				if (itNext == region.m_outline.end())
					itNext = region.m_outline.begin();
				Vec3 p0 = *it;
				Vec3 p1 = *itNext;
				p0.z = GetDebugDrawZ(p0, true);
				p1.z = GetDebugDrawZ(p1, true);
				pRenderer->GetIRenderAuxGeom()->DrawLine(p0, col, p1, col);
			}
		}
	}
}

//===================================================================
// DebugDrawForbidden
//===================================================================
void CAISystem::DebugDrawForbidden(IRenderer *pRenderer) const
{
	// draw forbidden areas
	if (!m_forbiddenAreas.GetShapes().empty())
	{
		ColorF col(1, 0, 0);
		for (unsigned i = 0, ni = m_forbiddenAreas.GetShapes().size(); i < ni; ++i)
		{
			const CAIShape* pShape = m_forbiddenAreas.GetShapes()[i];
			const ShapePointContainer& pts = pShape->GetPoints();
			for (unsigned j = 0, nj = pts.size(); j < nj; ++j)
			{
				Vec3 v0 = pts[j];
				Vec3 v1 = pts[(j+1)%nj];
				v0.z += 0.01f;
				v1.z += 0.01f;
//				v0.z = 0.01f + GetDrawZ(v0, true); // avoid clash with triangles
//				v1.z = 0.01f + GetDrawZ(v1, true);
				pRenderer->GetIRenderAuxGeom()->DrawLine(v0, col, v1, col);
			}
		}
	}
//	else
	{
		// If the forbidden areas are empty, try drawing the designer forbidden areas.
		ColorF col(1, 0, 0.75f, 0.5f);
		for (unsigned i = 0, ni = m_designerForbiddenAreas.GetShapes().size(); i < ni; ++i)
		{
			const CAIShape* pShape = m_designerForbiddenAreas.GetShapes()[i];
			const ShapePointContainer& pts = pShape->GetPoints();
			for (unsigned j = 0, nj = pts.size(); j < nj; ++j)
			{
				Vec3 v0 = pts[j];
				Vec3 v1 = pts[(j+1)%nj];
				v0.z += 0.1f;
				v1.z += 0.1f;
//				v0.z = 0.1f + GetDrawZ(v0, true); // avoid clash with triangles
//				v1.z = 0.1f + GetDrawZ(v1, true);
				pRenderer->GetIRenderAuxGeom()->DrawLine(v0, col, v1, col);
			}
		}
	}

	// draw forbidden boundaries
	if (!m_forbiddenBoundaries.GetShapes().empty())
	{
		ColorF col(1, 1, 0);
		for (unsigned i = 0, ni = m_forbiddenBoundaries.GetShapes().size(); i < ni; ++i)
		{
			const CAIShape* pShape = m_forbiddenBoundaries.GetShapes()[i];
			const ShapePointContainer& pts = pShape->GetPoints();
			for (unsigned j = 0, nj = pts.size(); j < nj; ++j)
			{
				Vec3 v0 = pts[j];
				Vec3 v1 = pts[(j+1)%nj];
				v0.z = 0.01f + GetDebugDrawZ(v0, true);
				v1.z = 0.01f + GetDebugDrawZ(v1, true);
				pRenderer->GetIRenderAuxGeom()->DrawLine(v0, col, v1, col);
			}
		}
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawPath(IRenderer *pRenderer) const
{
	const char *pName=m_cvDrawPath->GetString();
	if(!pName )
		return;
	CAIObject *pTargetObject = GetAIObjectByName(pName);
	if (pTargetObject)
	{
		CPuppet* pTargetPuppet = pTargetObject->CastToCPuppet();
		if (!pTargetPuppet)
			return;
		DebugDrawPathSingle( pRenderer, pTargetPuppet );
		return;
	}
	if(strcmp(pName, "all"))
		return;

  m_pTriangularNavRegion->DebugDrawPathLinks(pRenderer);

	// find if there are any puppets
	const Vec3 offset(0, 0, 0.1f);
	int cnt=0;
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
	{
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			cnt++;
			CPuppet *pPuppet = (CPuppet*) ai->second;
			DebugDrawPathSingle( pRenderer, pPuppet );
		}
	}

	if ((ai = m_Objects.find(AIOBJECT_VEHICLE)) != m_Objects.end())
	{
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_VEHICLE)
				break;
			cnt++;
			if(!ai->second)
				continue;
			CPuppet *pTargetPuppet = ai->second->CastToCPuppet();
			if (!pTargetPuppet)
				continue;
			DebugDrawPathSingle( pRenderer, pTargetPuppet );
		}
	}
}

//===================================================================
// DebugDrawPathAdjustments
//===================================================================
void CAISystem::DebugDrawPathAdjustments(IRenderer *pRenderer) const
{
	const char *pName=m_cvDrawPathAdjustment->GetString();
	if(!pName )
		return;
	CAIObject *pTargetObject = GetAIObjectByName(pName);
	if (pTargetObject)
	{
		if (CPuppet *pTargetPuppet = pTargetObject->CastToCPuppet())
      pTargetPuppet->GetPathAdjustmentObstacles(false).DebugDraw(pRenderer);
    if (CAIVehicle *pTargetVehicle = pTargetObject->CastToCAIVehicle())
      pTargetVehicle->GetPathAdjustmentObstacles(false).DebugDraw(pRenderer);
		return;
	}
	if(strcmp(pName, "all"))
		return;
	// find if there are any puppets
	int cnt=0;
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
  {
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			cnt++;
			CPuppet *pPuppet = (CPuppet*) ai->second;
			pPuppet->GetPathAdjustmentObstacles(false).DebugDraw(pRenderer);
		}
  }
  if ((ai = m_Objects.find(AIOBJECT_VEHICLE)) != m_Objects.end())
  {
    for (;ai!=m_Objects.end(); ++ai)
    {
      if (ai->first != AIOBJECT_VEHICLE)
        break;
      cnt++;
      CAIVehicle *pVehicle = (CAIVehicle*) ai->second;
      pVehicle->GetPathAdjustmentObstacles(false).DebugDraw(pRenderer);
    }
  }
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawPathSingle(IRenderer *pRenderer, const CPuppet *pPuppet) const
{
	if (!pPuppet->IsEnabled())
		return;

	// debug path draws all the path
	// m_Path gets nodes popped off the front...
	pPuppet->m_Path.Draw( pRenderer );
  if (pPuppet->m_pPathFollower)
    pPuppet->m_pPathFollower->Draw(pRenderer);

	// draw the first part of the path in a different colour
	if (!pPuppet->m_OrigPath.GetPath().empty())	
	{
		TPathPoints::const_iterator li,linext;
		li = pPuppet->m_OrigPath.GetPath().begin();
		linext = li;
		++linext;
    Vec3 endPt = pPuppet->m_Path.GetNextPathPos(ZERO);
		while (linext != pPuppet->m_OrigPath.GetPath().end())
		{
			Vec3 p0 = li->vPos;
			Vec3 p1 = linext->vPos;
      p0.z = GetDebugDrawZ(p0, li->navType == IAISystem::NAV_TRIANGULAR);
      p1.z = GetDebugDrawZ(p1, li->navType == IAISystem::NAV_TRIANGULAR);
			pRenderer->GetIRenderAuxGeom()->DrawLine(p0, ColorF(1,0,1,1.0), p1, ColorF(1,0,1,1.0));
			endPt.z = li->vPos.z;
			if (endPt.IsEquivalent(li->vPos, 0.1f))
				break;
			li=linext++;
		}
	}
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawAgents(IRenderer *pRenderer) const 
{
	if(m_cvAgentStatsDist->GetFVal() <= 0)
		return;

	float	drawDist2 = static_cast<float>(m_cvAgentStatsDist->GetFVal());
	drawDist2*=drawDist2;
	if (drawDist2<1.0f)
	{
		//		DebugDrawAgent(pRenderer, GetNearestObjectOfType(GetPlayer(),AIOBJECT_PUPPET, 50 ));
		return;
	}
	// output all puppets
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			float dist2 = (GetDebugCameraPos() - (ai->second)->GetPos()).GetLengthSquared();
			if(dist2>drawDist2)
				continue;
			DebugDrawAgent(pRenderer, ai->second);
		}

		// output all vehicles
		if ((ai = m_Objects.find(AIOBJECT_VEHICLE)) != m_Objects.end())
		{
			for (;ai!=m_Objects.end(); ++ai)
			{
				if (ai->first != AIOBJECT_VEHICLE)
					break;
				float dist2 = (GetDebugCameraPos() - (ai->second)->GetPos()).GetLengthSquared();
				if(dist2>drawDist2)
					continue;
				DebugDrawAgent(pRenderer, ai->second);
			}
		}

}

CAIObject * gVehicleToDebug = 0;
CSteeringDebugInfo gSteeringDebugInfo;

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawAgent(IRenderer *pRenderer, CAIObject* pAgentObj) const 
{
	float	fontSize = 1.15f;

	CAIActor* pAgent = CastToCAIActorSafe(pAgentObj);
	if(!pAgentObj || !pAgentObj->IsEnabled())
		return;

	CAIVehicle* pVehicle = pAgent->CastToCAIVehicle();
	if(pVehicle && !pVehicle->IsDriverInside())
		return;

	CPipeUser* pPipeUser = pAgent->CastToCPipeUser();
	Vec3 pos = pAgent->GetPos();
	Vec3 pzpos = pos;
	pzpos.z += 0.75f;
	if (pPipeUser->GetProxy() && pPipeUser->GetProxy()->GetLinkedVehicleEntityId())
		pzpos.z += 1.25f;

	pRenderer->DrawLabel(pzpos, fontSize, "\n%s", pAgent->GetName());

	if(pPipeUser)
	{
		SAIBodyInfo bodyInfo;
		pPipeUser->GetProxy()->QueryBodyInfo(bodyInfo);

		float fColor[4] = {1,0,0,1};

		char	szMsg[32] = "\0";
/*		if (!pPipeUser->m_CurrentHideObject.IsValid())
			strcat( szMsg, "? " );
		if( !pPipeUser->m_bCanReceiveSignals )
			strcat( szMsg, "% " );
		if (!pPipeUser->m_movementAbility.bUsePathfinder)
			strcat( szMsg, "# " );
		if( szMsg[0] )
			pRenderer->DrawLabelEx(pzpos,fontSize * 1.5f,fColor,true,false,szMsg);*/

		{
			strcpy(szMsg, "");

			EAimState	aim = pPipeUser->GetAimState();
			if (aim == AI_AIM_WAITING)
				strcat(szMsg, "\\");
			else if (aim == AI_AIM_READY)
				strcat(szMsg, "--");
			else if (aim == AI_AIM_FORCED)
				strcat(szMsg, "==");

			EFireMode	fireMode = pPipeUser->GetFireMode();
			if(fireMode == FIREMODE_MELEE)
			{
				strcat(szMsg, " Melee");
			}
			else if(fireMode == FIREMODE_SECONDARY)
			{
				if(pPipeUser->m_State.fireSecondary)
					strcat(szMsg, " 2>>**");
				else
					strcat(szMsg, " 2>>");
			}
			else if(fireMode != FIREMODE_OFF && fireMode != FIREMODE_AIM)
			{
				if (pPipeUser->m_State.fire)
					strcat(szMsg, " >>**");
				else
					strcat(szMsg, " >>");
			}

			if(aim == AI_AIM_OBSTRUCTED)
				strcat(szMsg, "|");

			pRenderer->DrawLabelEx(pzpos,fontSize*1.5f,fColor,true,false,szMsg);
		}

//		if (pPipeUser->m_State.aimLook || pPipeUser->m_State.allowStrafing)
		{
			strcpy(szMsg, "\n\n\n");

			switch (pPipeUser->m_State.bodystate)
			{
			case STANCE_NULL:			strcat(szMsg, "Null");		break;
			case STANCE_CROUCH:		strcat(szMsg, "Crouch");	break;
			case STANCE_PRONE:		strcat(szMsg, "Prone");		break;
			case STANCE_RELAXED:	strcat(szMsg, "Relaxed");	break;
			case STANCE_STAND:		strcat(szMsg, "Stand");		break;
			case STANCE_STEALTH:	strcat(szMsg, "Stealth");	break;
			default: strcat(szMsg, "Default"); break;
			}

			if(pPipeUser->m_State.allowStrafing)
				strcat( szMsg, "  Strafe" );
			if(pPipeUser->m_State.lean < -0.01f)
				strcat( szMsg, "  LeanLeft" );
			else if(pPipeUser->m_State.lean > 0.01f)
				strcat( szMsg, "  LeanRight" );

			pRenderer->DrawLabelEx(pzpos,fontSize,fColor,true,false,szMsg);
		}

		static string bufString;
		if (pPipeUser->GetAttentionTarget())
		{
			bufString = ">> ";
			bufString += pPipeUser->GetAttentionTarget()->GetName();

			// my attention target - red
			pRenderer->GetIRenderAuxGeom()->DrawSphere(pPipeUser->GetAttentionTarget()->GetPos(),0.1f, ColorF(1, 0, 0));

			switch (pPipeUser->GetAttentionTargetType())
			{
			case AITARGET_VISUAL: bufString += "  <VIS"; break;
			case AITARGET_MEMORY: bufString += "  <MEM"; break;
			case AITARGET_SOUND: bufString += "  <SND"; break;
			default: bufString +=  "  <-  "; break;
			}

			switch (pPipeUser->GetAttentionTargetThreat())
			{
			case AITHREAT_AGGRESSIVE: bufString +=  " AGG>"; break;
			case AITHREAT_THREATENING: bufString +=  " THR>"; break;
			case AITHREAT_INTERESTING: bufString +=  " INT>"; break;
			default: bufString +=  " -  >"; break;
			}
		}	
		else
		{
			bufString = ">> No Target";
		}
		pRenderer->DrawLabel(pzpos, fontSize, "\n\n%s", bufString.c_str());
		CGoalPipe* pPipe = pPipeUser->GetCurrentGoalPipe();
		if (pPipe)
		{
			static string goalPipes;
      goalPipes = pPipe->m_sDebugName.empty() ? pPipe->m_sName : pPipe->m_sDebugName;
			CGoalPipe* pSubPipe = pPipe;
			while (pSubPipe = pSubPipe->GetSubpipe())
			{
				goalPipes += ", ";
				goalPipes += pSubPipe->m_sDebugName.empty() ? pSubPipe->m_sName : pSubPipe->m_sDebugName;
			}

			// to prevent buffer overrun
			if ( goalPipes.length() > 122 )
			{
				goalPipes = goalPipes.substr( 0, 122 );
				goalPipes += "...";
			}

			pRenderer->DrawLabel(pzpos,fontSize,"\n\n\n\n%s", goalPipes.c_str());

			const char* szGoalopName = "";
			if (pPipeUser->m_lastExecutedGoalop != LAST_AIOP)
				szGoalopName = pPipe->GetGoalOpName(pPipeUser->m_lastExecutedGoalop);
			pRenderer->DrawLabel(pzpos,fontSize,"\n\n\n\n\n%s", szGoalopName);
		}

		// Debug Draw goal ops.
		if( m_cvDrawGoals->GetIVal() )
			pPipeUser->DebugDrawGoals(pRenderer);

		// Output the steering and direction variables for vehicles.
		CAIVehicle* pVehicle = pPipeUser->CastToCAIVehicle();
		if (pVehicle)
		{
			if( pVehicle->GetSubType() == CAIObject::STP_CAR)
			{
				SOBJECTSTATE*	pState = pPipeUser->GetState();
				gVehicleToDebug = pVehicle;
				for (unsigned i = 0 ; i < gSteeringDebugInfo.pts.size() ; ++i)
				{
					Vec3 steeringPos = gSteeringDebugInfo.pts[i];
					steeringPos.z = GetDebugDrawZ(steeringPos, false);
					pRenderer->GetIRenderAuxGeom()->DrawSphere(steeringPos, 0.3f, ColorF(1, 1, 1));
				}
				Vec3 posStart = gSteeringDebugInfo.segStart;
				Vec3 posEnd = gSteeringDebugInfo.segEnd;
				posStart.z = 0.5f + GetDebugDrawZ(posStart, false);
				posEnd.z = 0.5f + GetDebugDrawZ(posEnd, false);
				pRenderer->GetIRenderAuxGeom()->DrawSphere(posEnd, 0.3f, ColorF(0.5,0.5,0.5,1));
				pRenderer->GetIRenderAuxGeom()->DrawLine(posStart, ColorF(0.5,0.5,0.5,1), posEnd, ColorF(0.5,0.5,0.5,1));

				if( pState )
				{
					// Find the sign of the values. Since we are claping the values,
					// the signs gives us feedback if there is some really small value present.
					char	dirSign = ' ';
					if( pState->m_fDEBUGDirection < 0 )
						dirSign = '-';
					else if( pState->m_fDEBUGDirection > 0 )
						dirSign = '+';
					char	steerSign = ' ';
					if( pState->m_fDEBUGSteering < 0 )
						steerSign = '-';
					else if( pState->m_fDEBUGSteering > 0 )
						steerSign = '+';
					pRenderer->DrawLabel(pzpos,fontSize,"\n\n\n\n\n\ndir:%c%1.4f steer:%c%1.4f %s", dirSign, abs(pState->m_fDEBUGDirection), steerSign, abs(pState->m_fDEBUGSteering), pState->m_bDEBUGBraking ? "Braking" : "");
				}
			}
		}

		// Desired view direction and movement direction are drawn in yellow, actual look and move directions are drawn in blue.
		// The lookat direction is marked with sphere, and the movement direction is parked with cone.

		const ColorF	desiredCol(1.0f, 0.8f, 0.05f, 1);
		const ColorF	actualCol(0.1f, 0.2f, 0.8f, 0.5f);

		const float	rad = pPipeUser->m_Parameters.m_fPassRadius;

		Vec3	physPos = pPipeUser->GetPhysicsPos() + Vec3(0,0,0.25f);

		// Pathfinding radius circle
		DebugDrawCircleOutline(pRenderer, physPos, rad, actualCol);

		// The desired movement direction.
		if (pPipeUser->m_State.vMoveDir.GetLengthSquared() > 0.0f)
		{
			const Vec3&	dir = pPipeUser->m_State.vMoveDir;
			const Vec3	target = physPos + dir * (rad + 0.7f);
			pRenderer->GetIRenderAuxGeom()->DrawLine(physPos+dir*rad, desiredCol, target, desiredCol);
			pRenderer->GetIRenderAuxGeom()->DrawCone(target, dir, 0.05f, 0.2f, desiredCol);

			// The desired speed.
/*			const ColorF	speedCol(0.2f,1.0f,0.3f,1.0f);
			const Vec3	speedPos(physPos + Vec3(0, 0, 0.2f));
			const Vec3	speedTarget = speedPos + dir * (rad + 1.0f) * pPipeUser->m_State.fDesiredSpeed;
			pRenderer->GetIRenderAuxGeom()->DrawLine( speedPos, speedCol, speedTarget, speedCol );*/
		}
		// The desired lookat direction.
		if (!pPipeUser->m_State.vLookTargetPos.IsZero())
		{
			Vec3	dir = pPipeUser->m_State.vLookTargetPos - pPipeUser->GetPos();
			dir.NormalizeSafe(Vec3(ZERO));
			const Vec3	target = pos + dir * (rad + 0.7f);
			pRenderer->GetIRenderAuxGeom()->DrawLine(pos, desiredCol, target, desiredCol);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(target, 0.07f, desiredCol);
		}
		// The actual movement direction
		if (!pPipeUser->GetMoveDir().IsZero())
		{
			const Vec3&	dir = pPipeUser->GetMoveDir();
			const Vec3	target = physPos + dir * (rad + 1.0f);
			pRenderer->GetIRenderAuxGeom()->DrawLine(physPos+dir*rad, actualCol, target, actualCol);
			pRenderer->GetIRenderAuxGeom()->DrawCone(target, dir, 0.05f, 0.2f, actualCol);
		}
		// The actual body direction
		if (!pPipeUser->GetBodyDir().IsZero())
		{
			const Vec3&	dir = pPipeUser->GetBodyDir();
			const Vec3	target = physPos + dir * rad;
			pRenderer->GetIRenderAuxGeom()->DrawCone(target, dir, 0.07f, 0.3f, actualCol);
		}
		// The actual lookat direction
		if (!pPipeUser->GetViewDir().IsZero())
		{
			const Vec3&	dir = pPipeUser->GetViewDir();
			const Vec3	target = pos + dir * (rad + 1.0f);
			pRenderer->GetIRenderAuxGeom()->DrawLine(pos, actualCol, target, actualCol);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(target, 0.07f, actualCol);
		}

		// The aim & fire direction
		{
			bool	allowedToHit = false;
			CPuppet* pPuppet = pAgent->CastToCPuppet();
			if (pPuppet)
				allowedToHit = pPuppet->IsAllowedToHitTarget();

			ColorF	fireCol(1,0.1f,0,0.7f); 
//			if (!allowedToHit)
//				fireCol.Set(1,1,1,0.2f);

			Vec3 firePos = pPipeUser->GetFirePos();

			// Aim dir
			if (!pPipeUser->m_State.vAimTargetPos.IsZero())
			{
				Vec3	dir = pPipeUser->m_State.vAimTargetPos - firePos;
				dir.NormalizeSafe(Vec3(ZERO));
				const Vec3	target = firePos + dir * (rad + 0.5f);
				pRenderer->GetIRenderAuxGeom()->DrawLine(firePos, desiredCol, target, desiredCol);
				pRenderer->GetIRenderAuxGeom()->DrawCylinder(target, dir, 0.035f, 0.07f, desiredCol);
//				pRenderer->GetIRenderAuxGeom()->DrawSphere(target, 0.07f, desiredCol);
			}

			// Weapon dir
			{
				const Vec3&	dir = bodyInfo.vFireDir;
				const Vec3	target = firePos + dir * (rad + 0.8f);
				pRenderer->GetIRenderAuxGeom()->DrawLine(firePos, fireCol, target, fireCol);
				pRenderer->GetIRenderAuxGeom()->DrawCylinder(target, dir, 0.035f, 0.07f, fireCol);
			}

/*
			if (pPipeUser->m_State.fire)
			{
				pRenderer->GetIRenderAuxGeom()->DrawCone(pPipeUser->GetFirePos(), bodyInfo.vFireDir, 0.07f, 1.0f, fireCol);
				pRenderer->GetIRenderAuxGeom()->DrawLine(pPipeUser->GetFirePos(), fireCol, pPipeUser->m_State.vShootTargetPos, fireCol);
			}
			else
			{
				pRenderer->GetIRenderAuxGeom()->DrawCone(pPipeUser->GetFirePos(), bodyInfo.vFireDir, 0.07f, 0.75f, actualCol);
			}
*/

			// Aim
/*			EAimState	aim = pPipeUser->GetAimState();
			if (aim == AI_AIM_READY || aim == AI_AIM_FORCED)
				pRenderer->GetIRenderAuxGeom()->DrawSphere(pPipeUser->GetFirePos() + bodyInfo.vFireDir, 0.07f, actualCol);

			if (!pPipeUser->m_State.vAimTargetPos.IsZero())
			{
				Vec3 dir = pPipeUser->m_State.vAimTargetPos - pPipeUser->GetFirePos();
				float len = dir.NormalizeSafe();
				const unsigned maxDashes = 40;
				const float dashSize = 0.25f;
				const float maxLen = maxDashes * 2 * dashSize;
				if (len > maxLen)
					len 

//				pRenderer->GetIRenderAuxGeom()->DrawLine(pPipeUser->GetFirePos() + Vec3(0,0,0.01f), fireCol*0.75f, pPipeUser->m_State.vAimTargetPos + Vec3(0,0,0.01f), fireCol*0.75f);
			}
*/
		}

		const float	drawFOV = m_cvDrawAgentFOV->GetFVal();
		if( drawFOV > 0 )
		{
			// Draw the view cone
			DebugDrawWireFOVCone(pRenderer, pos, pPipeUser->GetViewDir(),
				pPipeUser->m_Parameters.m_PerceptionParams.sightRange * drawFOV * 0.99f,
				DEG2RAD(pPipeUser->m_Parameters.m_PerceptionParams.FOVPrimary) * 0.5f, ColorB(255,255,255));

			DebugDrawWireFOVCone(pRenderer, pos, pPipeUser->GetViewDir(),
				pPipeUser->m_Parameters.m_PerceptionParams.sightRange * drawFOV,
				DEG2RAD(pPipeUser->m_Parameters.m_PerceptionParams.FOVSecondary) * 0.5f, ColorB(128,128,128));
			
			Vec3 midPoint = pos + pPipeUser->GetViewDir()*pPipeUser->m_Parameters.m_PerceptionParams.sightRange * drawFOV/2;
			pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,255), midPoint, ColorB(255,255,255));
			pRenderer->DrawLabel(midPoint,1.5f,"FOV %.1f/%.1f",
				pPipeUser->m_Parameters.m_PerceptionParams.FOVPrimary,
				pPipeUser->m_Parameters.m_PerceptionParams.FOVSecondary);
		}

		// my Ref point - yellow
		string rfname = m_cvDrawRefPoints->GetString();
		if(rfname!="")
		{
			int group =atoi(rfname) ;
			if(rfname=="all" || rfname==pPipeUser->GetName() || ((rfname=="0" || group>0) && group==pPipeUser->GetGroupId()))
			{
				if (pPipeUser->GetRefPoint())
				{
					Vec3 rppos = pPipeUser->GetRefPoint()->GetPos();
					pRenderer->GetIRenderAuxGeom()->DrawLine(pPipeUser->GetPos(),ColorF(1,0,0),rppos,ColorF(1,1,0));
					pRenderer->GetIRenderAuxGeom()->DrawSphere(rppos, .5, ColorF(1,1,0,1));
					rppos.z += 1.5;
					pRenderer->DrawLabel(rppos,fontSize,pPipeUser->GetRefPoint()->GetName());
				}
/* moved in DebugDrawGroup
				CAIObject* pBeacon = (CAIObject*) GetAISystem()->GetBeacon(pAgent->GetGroupId());
				if(pBeacon)
				{
					pRenderer->GetIRenderAuxGeom()->DrawSphere(pBeacon->GetPos(),0.35f, ColorF(0, 1, 1));
					pRenderer->DrawLabel(pBeacon->GetPos()+ Vec3(0,0,1),1,"%s", pBeacon->GetName());

				}
*/
			}
		}

		CPuppet* pPuppet = pAgent->CastToCPuppet();

		// Draw target tracking.
		if( m_cvDrawPatterns->GetIVal() )
		{
			if( pPuppet && pPuppet->m_pTrackPattern )
			{
				pPuppet->m_pTrackPattern->DebugDraw( pRenderer );
			}
		}

		if(m_cvDebugTargetSilhouette->GetIVal() > 0)
		{
			if(pPuppet->m_targetSilhouette.valid && !pPuppet->m_targetSilhouette.points.empty())
			{
				const Vec3& u = pPuppet->m_targetSilhouette.baseMtx.GetColumn0();
				const Vec3& v = pPuppet->m_targetSilhouette.baseMtx.GetColumn2();

				// Draw silhouette
				size_t n = pPuppet->m_targetSilhouette.points.size();
				for(size_t i = 0; i < n; ++i)
				{
					size_t	j = (i + 1) % n;
					Vec3	pi = pPuppet->m_targetSilhouette.center + u * pPuppet->m_targetSilhouette.points[i].x + v * pPuppet->m_targetSilhouette.points[i].y;
					Vec3	pj = pPuppet->m_targetSilhouette.center + u * pPuppet->m_targetSilhouette.points[j].x + v * pPuppet->m_targetSilhouette.points[j].y;
					pRenderer->GetIRenderAuxGeom()->DrawLine(pi, ColorB(255,255,255), pj, ColorB(255,255,255));
				}

				if(!pPuppet->m_targetLastMissPoint.IsZero())
				{
					pRenderer->DrawLabel(pPuppet->m_targetLastMissPoint, 1.0f, "Last Miss");
					pRenderer->GetIRenderAuxGeom()->DrawSphere(pPuppet->m_targetLastMissPoint, 0.1f, ColorB(255,128,0));
				}

				// The attentiontarget on the silhouette plane.
				if(!pPuppet->m_targetPosOnSilhouettePlane.IsZero())
				{
					Vec3	dir = (pPuppet->m_targetPosOnSilhouettePlane - pPuppet->GetFirePos()).GetNormalizedSafe();
					pRenderer->GetIRenderAuxGeom()->DrawCone(pPuppet->m_targetPosOnSilhouettePlane, dir, 0.15f, 0.2f, ColorB(255,0,0,pPuppet->m_targetDistanceToSilhouette < 3.0f ? 255:64));
					pRenderer->GetIRenderAuxGeom()->DrawLine(pPuppet->GetFirePos(), ColorB(255,0,0), pPuppet->m_targetPosOnSilhouettePlane, ColorB(255,0,0));
				}

				{
					const Vec3& u = pPuppet->m_targetSilhouette.baseMtx.GetColumn0();
					const Vec3& v = pPuppet->m_targetSilhouette.baseMtx.GetColumn2();

					Vec3 targetBiasDirectionProj = pPuppet->m_targetSilhouette.ProjectVectorOnSilhouette(pPuppet->m_targetBiasDirection);
					targetBiasDirectionProj.NormalizeSafe(Vec3(0,0,0));
					Vec3 pos = pPuppet->m_targetSilhouette.center + u*targetBiasDirectionProj.x + v*targetBiasDirectionProj.y;

					pRenderer->GetIRenderAuxGeom()->DrawLine(pPuppet->m_targetSilhouette.center, ColorB(255,0,0,0), pos, ColorB(255,0,0));

					pRenderer->GetIRenderAuxGeom()->DrawLine(pPuppet->m_targetSilhouette.center, ColorB(0,0,255,0), pPuppet->m_targetSilhouette.center + pPuppet->m_targetBiasDirection, ColorB(0,0,255));

					pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,255,128), pPuppet->m_targetSilhouette.center + pPuppet->m_targetBiasDirection, ColorB(255,255,255,128));
				}

				// Test miss points
/*				Vec3	missPt(0,0,0);
				if(pPuppet->GetMissPointAroundTarget(0.5f, missPt))
				{
					pRenderer->GetIRenderAuxGeom()->DrawLine(pPuppet->GetFirePos(), ColorB(255,255,255), missPt, ColorB(255,255,255));
				}*/

				char* szZone = "";
				switch(pPuppet->m_targetZone)
				{
				case CPuppet::ZONE_OUT: szZone = "Out"; break;
				case CPuppet::ZONE_WARN: szZone = "Warn"; break;
				case CPuppet::ZONE_COMBAT_NEAR: szZone = "Combat-Near"; break;
				case CPuppet::ZONE_COMBAT_FAR: szZone = "Combat-Far"; break;
				case CPuppet::ZONE_KILL: szZone = "Kill"; break;
				}

				pRenderer->DrawLabel(pPuppet->GetPos() - Vec3(0,0,1.5f), 1.2f, "Focus:%d\nZone:%s", (int)(pPuppet->m_targetFocus*100.0f), szZone);

			}

/*			Vec3	p = m_silhuetteCenter + u * m_projTargetVel.x + v * m_projTargetVel.y;
			pRenderer->GetIRenderAuxGeom()->DrawLine(m_silhuetteCenter, ColorB(255,255,255), p, ColorB(255,255,255));

			if(m_lastMissAngle >= 0.0f)
			{
				float	x = cosf(m_lastMissAngle);
				float	y = sinf(m_lastMissAngle);
				Vec3	p = m_silhuetteCenter + u*x + v*y;
				pRenderer->GetIRenderAuxGeom()->DrawLine(m_silhuetteCenter, ColorB(255,0,0), p, ColorB(255,0,0));
			}*/
		}

		// Display the readibilities.
		if( m_cvDrawReadibilities->GetIVal() )
			pAgent->GetProxy()->DebugDraw( pRenderer, 2 );
	}

	if (m_cvDrawProbableTarget->GetIVal() > 0)
	{
		CPuppet*	pPuppet = pAgent->CastToCPuppet();
		if (pPuppet)
		{
			if (pPuppet->GetTimeSinceLastLiveTarget() >= 0.0f)
			{
				const Vec3&	pos = pPuppet->GetPos();
				Vec3	livePos = pPuppet->GetLastLiveTargetPosition();
				Vec3	livePosConstrained = livePos;

				ColorB	col1(171,60,184);
				ColorB	col1Trans(171,60,184, 128);
				ColorB	col2(110,60,184);
				ColorB	col2Trans(110,60,184, 128);

				if (pPuppet->GetTerritoryShape() && pPuppet->GetTerritoryShape()->ConstrainPointInsideShape(livePosConstrained, true))
				{
					Vec3	mid = livePosConstrained * 0.7f + pos * 0.3f;
					pRenderer->GetIRenderAuxGeom()->DrawLine(mid, col1, livePosConstrained, col1Trans);
					pRenderer->GetIRenderAuxGeom()->DrawLine(livePosConstrained, col2Trans, livePos, col2Trans);
					pRenderer->GetIRenderAuxGeom()->DrawSphere(livePosConstrained, 0.25f, col1Trans);
					pRenderer->GetIRenderAuxGeom()->DrawSphere(livePos, 0.5f, col2Trans);
				}
				else
				{
					Vec3	mid = livePos * 0.7f + pos * 0.3f;
					pRenderer->GetIRenderAuxGeom()->DrawLine(mid, col1, livePos, col1Trans);
					pRenderer->GetIRenderAuxGeom()->DrawSphere(livePos, 0.5f, col1Trans);
				}
			}
		}
	}

	// Display damage parts.
	if(m_cvDebugDrawDamageParts->GetIVal() > 0)
	{
		if(pAgent->GetDamageParts())
		{
			DamagePartVector*	parts = pAgent->GetDamageParts();
			for(DamagePartVector::iterator it = parts->begin(); it != parts->end(); ++it)
			{
				SAIDamagePart&	part = *it;
				pRenderer->DrawLabel(part.pos, 1,"^ DMG:%.2f\n  VOL:%.1f", part.damageMult, part.volume);
			}
		}
	}

	// Draw the approximate stance size.
	if(m_cvDebugDrawStanceSize->GetIVal() > 0)
	{
		if(pAgent->GetProxy())
		{
			SAIBodyInfo	bodyInfo;
			pAgent->GetProxy()->QueryBodyInfo(bodyInfo);
			Vec3	pos = pAgent->GetPhysicsPos();
			AABB	aabb(bodyInfo.stanceSize);
			aabb.Move(pos);
			pRenderer->GetIRenderAuxGeom()->DrawAABB(aabb, true, ColorB(255,255,255,128), eBBD_Faceted);
		}
	}

	// Draw active exact positioning request
	{
		CPuppet*	pPuppet = pAgent->CastToCPuppet();
		if (pPuppet)
		{
			// Draw pending actor target request. These should not be hanging around, so draw one of it is requested.
			if( const SAIActorTargetRequest* pReq = pPuppet->GetActiveActorTargetRequest())
			{
				Vec3	pos = pReq->approachLocation;
				Vec3	dir = pReq->approachDirection;
				DebugDrawArrow(pRenderer, pos - dir*0.5f, dir, 0.2f, ColorB(0,255,0,196));
				pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(0,255,0), pos - Vec3(0,0,0.5f), ColorB(0,255,0));
				pRenderer->GetIRenderAuxGeom()->DrawLine(pReq->approachLocation, ColorB(255,255,255,128), pReq->animLocation, ColorB(255,255,255,128));
				if(!pReq->animation.empty())
					pRenderer->DrawLabel(pos + Vec3(0,0,0.5f), 1.5f,"%s", pReq->animation.c_str());
				else if(!pReq->vehicleName.empty())
					pRenderer->DrawLabel(pos + Vec3(0,0,0.5f), 1.5f,"%s Seat:%d", pReq->vehicleName.c_str(), pReq->vehicleSeat);
			}

			const Vec3	size(0.1f, 0.1f, 0.1f);
			if (!pPuppet->m_DEBUGCanTargetPointBeReached.empty())
			{
				for(ListPositions::iterator it = pPuppet->m_DEBUGCanTargetPointBeReached.begin(); it != pPuppet->m_DEBUGCanTargetPointBeReached.end(); ++it)
				{
					ColorB	col(255,0,0,128);
					Vec3	pos = *it;
					pRenderer->GetIRenderAuxGeom()->DrawAABB(AABB(pos-size, pos+size), true, col, eBBD_Faceted);
					pRenderer->GetIRenderAuxGeom()->DrawLine(pos, col, pos - Vec3(0,0,1.0f), col);
				}
			}
			if (!pPuppet->m_DEBUGUseTargetPointRequest.IsZero())
			{
				ColorB	col(0,0,255,255);
				Vec3	pos = pPuppet->m_DEBUGUseTargetPointRequest + Vec3(0,0,0.4f);
				pRenderer->GetIRenderAuxGeom()->DrawAABB(AABB(pos-size, pos+size), true, col, eBBD_Faceted);
				pRenderer->GetIRenderAuxGeom()->DrawLine(pos, col, pos - Vec3(0,0,1.0f), col);
			}

			// Actor target phase.
			if( pPuppet->m_State.actorTargetReq.id != 0)
			{

				Vec3	pos = pPuppet->m_State.actorTargetReq.approachLocation;
				Vec3	dir = pPuppet->m_State.actorTargetReq.approachDirection;

				DebugDrawRangeCircle(pRenderer, pos + Vec3(0,0,0.3f), pPuppet->m_State.actorTargetReq.locationRadius, pPuppet->m_State.actorTargetReq.locationRadius, ColorB(255,255,255,128), ColorB(255,255,255), true);

				const char*	szPhase = "";
				switch(pPuppet->m_State.curActorTargetPhase)
				{
				case eATP_None: szPhase = "None"; break;
				case eATP_Waiting: szPhase = "Waiting"; break;
				case eATP_Starting: szPhase = "Starting"; break;
				case eATP_Started: szPhase = "Started"; break;
				case eATP_Playing: szPhase = "Playing"; break;
				case eATP_StartedAndFinished: szPhase = "StartedAndFinished"; break;
				case eATP_Finished: szPhase = "Finished"; break;
				case eATP_Error: szPhase = "Error"; break;
				}

				const char*	szType = "<INVALID!>";

				PathPointDescriptor::SmartObjectNavDataPtr pSmartObjectNavData = pPuppet->m_Path.GetLastPathPointAnimNavSOData();
				if(pSmartObjectNavData)
					szType = "NAV_SO";
				else if(pPuppet->GetActiveActorTargetRequest())
					szType = "ACTOR_TGT";

				pRenderer->DrawLabel(pos + Vec3(0,0,1.0f), 1,"%s\nPhase:%s ID:%d", szType, szPhase, pPuppet->m_State.actorTargetReq.id);
			}
		}
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawPendingEvents(IRenderer *pRenderer, CPuppet* pTargetPuppet, int xPos, int yPos) const
{
	typedef std::map<float, string> t_PendingMap;
	t_PendingMap eventsMap;

	PotentialTargetMap::const_iterator ei = pTargetPuppet->m_perceptionEvents.begin();
	for (;ei != pTargetPuppet->m_perceptionEvents.end(); ++ei)
	{
		char	buffString[256];
		const SAIPotentialTarget &ed = ei->second;
		CAIObject *pNextTarget = 0;
		if ((ed.type == AITARGET_VISUAL && ed.threat == AITHREAT_AGGRESSIVE) || !ed.pDummyRepresentation)
			pNextTarget = ei->first;
		else
			pNextTarget = ed.pDummyRepresentation;

		string curName("NULL");
		if (pNextTarget)
			curName = pNextTarget->GetName();

		float timeout = max(0.0f, ed.GetTimeout(pTargetPuppet->GetParameters().m_PerceptionParams));

		const char* szTargetType = "";
		const char* szTargetThreat = "";
	
		switch (ed.type)
		{
		case AITARGET_VISUAL: szTargetType = "VIS"; break;
		case AITARGET_MEMORY: szTargetType = "MEM"; break;
		case AITARGET_SOUND: szTargetType = "SND"; break;
		default: szTargetType = "-  "; break;
		}

		switch (ed.threat)
		{
		case AITHREAT_AGGRESSIVE: szTargetThreat = "AGG"; break;
		case AITHREAT_THREATENING: szTargetThreat = "THR"; break;
		case AITHREAT_INTERESTING: szTargetThreat = "INT"; break;
		default: szTargetThreat = "-  "; break;
		}

		sprintf(buffString, "%.1fs  <%s %s>  %s", timeout, szTargetType, szTargetThreat, curName.c_str());

		//		eventsMap[ed.fPriority] = buffString;
		eventsMap.insert( std::make_pair(ed.priority, buffString));
	}
	int col(40);
	int row(5);
	char buff[256];
	for(t_PendingMap::reverse_iterator itr=eventsMap.rbegin(); itr!=eventsMap.rend(); ++itr)
	{
		sprintf(buff,"%.3f %s", itr->first, itr->second.c_str());

		float color[4]={0.f,1.f,1.f,1.f};
		if (itr == eventsMap.rbegin())
		{
			// Highlight the current target
			color[0] = 1.0f;
			color[1] = 1.0f;
			color[2] = 1.0f;
		}
		else
		{
			color[0] = 0.0f;
			color[1] = 0.75f;
			color[2] = 1.0f;
		}

		DebugDrawLabel(pRenderer, col, row, buff, color);
		++row;
		//		pRenderer->TextToScreen((float)xPos,(float)yPos,"%.1f %s", itr->first, itr->second.c_str());
		//		yPos+=12;
	}
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawStatsTarget(IRenderer *pRenderer, const char *pName)
{
	if(!pName )
		return;
	CAIObject *pTargetObject = GetAIObjectByName(pName);
	if (!pTargetObject)
		return;

	if(!pTargetObject->IsEnabled())
	{
		pRenderer->TextToScreen(0,60,"%s  #####>>>-----    IS DISABLED -----<<<#####",pTargetObject->GetName());
		return;
	}

	CPuppet* pTargetPuppet = pTargetObject->CastToCPuppet();
	if (!pTargetPuppet)
		return;

	DebugDrawAgent(pRenderer, pTargetObject);

	if( pTargetPuppet->m_pTrackPattern )
	{
		pRenderer->TextToScreen( 0, 54, "Pattern state: %s def: %.03f", pTargetPuppet->m_pTrackPattern->GetState() == AITRACKPAT_STATE_EXPOSED ? "> EXPOSED <" : "<ENCLOSED>", pTargetPuppet->m_pTrackPattern->GetGlobalDeformation() );
	}

	if(pTargetPuppet->m_State.fire)
		pRenderer->TextToScreen(0,60,">>>FIRING<<<");
	else if(pTargetPuppet->AllowedToFire())
		pRenderer->TextToScreen(0,60,"---FIRING---");
	else
		pRenderer->TextToScreen(0,60,"no fire");

	// Output stance
	string	stanceStr;
	SAIBodyInfo bodyInfo;
	pTargetPuppet->GetProxy()->QueryBodyInfo( bodyInfo);
	switch( bodyInfo.stance )
	{
	case STANCE_NULL:			stanceStr = "STANCE_NULL";		break;
	case STANCE_CROUCH:		stanceStr = "STANCE_CROUCH";	break;
	case STANCE_PRONE:		stanceStr = "STANCE_PRONE";		break;
	case STANCE_RELAXED:	stanceStr = "STANCE_RELAXED";	break;
	case STANCE_STAND:		stanceStr = "STANCE_STAND";		break;
	case STANCE_STEALTH:	stanceStr = "STANCE_STEALTH";	break;
	default:							stanceStr = "<unknown>";			break;
	};

  pRenderer->TextToScreen( 0, 48, "%s", pTargetPuppet->GetName());
  if(!pTargetPuppet->m_bCanReceiveSignals)
    pRenderer->TextToScreen(0, 50, ">> Cannot Receive Signals! <<");
	pRenderer->TextToScreen( 0, 52, "Stance: %s", stanceStr.c_str() );

	// This logic is borrowed from AIproxy in order to display the same urgency as sent further down the pipeline from AIproxy.
	float urgency = pTargetPuppet->m_State.fMovementUrgency;
	bool wantsToMove = !pTargetPuppet->m_State.vMoveDir.IsZero();
	if (pTargetPuppet && pTargetPuppet->GetProxy() && pTargetPuppet->GetProxy()->IsAnimationBlockingMovement())
		wantsToMove = false;
	if (!wantsToMove)
		urgency = (pTargetPuppet->m_State.curActorTargetPhase == eATP_Waiting) ? fabs(pTargetPuppet->m_State.fMovementUrgency) : 0.0f;

  pRenderer->TextToScreen(0,56, "DesiredSpd (urgency): %5.3f (%5.3f) Dir: (%5.3f, %5.3f, %5.3f)",
    pTargetPuppet->m_State.fDesiredSpeed, urgency,
    pTargetPuppet->m_State.vMoveDir.x, pTargetPuppet->m_State.vMoveDir.y, pTargetPuppet->m_State.vMoveDir.z);

	pRenderer->TextToScreen(0,58, "Turn speed: %-5.3f rad/s  Slope:%-5.3f", pTargetPuppet->m_bodyTurningSpeed, bodyInfo.slopeAngle);

/*	if (pTargetPuppet->GetPathFollower())
	{
		CPathFollower* pathFollower = pTargetPuppet->GetPathFollower();

		float normalSpeed = pTargetPuppet->GetNormalMovementSpeed(pTargetPuppet->m_State.fMovementUrgency, true);
		float slowSpeed = pTargetPuppet->GetManeuverMovementSpeed();

		float minSpeed = 0, maxSpeed = 0;
	
		IPuppetProxy* pPuppetProxy =  0;
		if (pTargetPuppet->GetProxy())
			pTargetPuppet->GetProxy()->QueryProxy(AIPROXY_PUPPET, (void**)&pPuppetProxy);

		bool useAnimSpeed = true;
		if (!pPuppetProxy || !pPuppetProxy->QueryCurrentAnimationSpeedRange(minSpeed, maxSpeed))
		{
			minSpeed = normalSpeed * 0.75f;
			maxSpeed = normalSpeed * 1.1f;
			useAnimSpeed = false;
		}

		normalSpeed = Clamp(normalSpeed, minSpeed, maxSpeed);

		pRenderer->TextToScreen(11,58, "Speeds: %5.3f (min:%5.3f  max:%5.3f) %s", normalSpeed, minSpeed, maxSpeed, useAnimSpeed ? "ANIM" : "PROC");
	}*/

/*
{
	char *pWhatIsMyTargetStr("NONE");
//	char	strBuffer[64];
	int	targetFlags(pTargetPuppet->GetTargetFlags());
	
	if(targetFlags & AITGT_VISUAL )
		pWhatIsMyTargetStr = targetFlags & AITGT_THREAT ? "VISUAL_THREAT" : "VISUAL_INTERESTING";
	else if(targetFlags & AITGT_AUDIO )
		pWhatIsMyTargetStr = targetFlags & AITGT_THREAT ? "AUDIO_THREAT" : "AUDIO_INTERESTING";
	else if(targetFlags & AITGT_MEMORY )
		pWhatIsMyTargetStr = targetFlags & AITGT_THREAT ? "MEMORY_THREAT" : "MEMORY_INTERESTING";
	else if(targetFlags & AITGT_FORCED )
		pWhatIsMyTargetStr = targetFlags & AITGT_THREAT ? "FORCED_THREAT" : "FORCED_INTERESTING";
	else if(targetFlags & AITGT_DUMMY )
		pWhatIsMyTargetStr = "_DUMMY_";
	else
		pWhatIsMyTargetStr = "_NONE_";
	pRenderer->TextToScreen(0, 49, pWhatIsMyTargetStr);
}
*/


	CGoalPipe *pPipe = pTargetPuppet->GetCurrentGoalPipe();
	if (pPipe)
	{
		pRenderer->TextToScreen(0,62,"Goalpipe: %s",pTargetPuppet->GetCurrentGoalPipe()->m_sName.c_str());

		const char* szGoalopName = "--";
		if (pTargetPuppet->m_lastExecutedGoalop != LAST_AIOP)
			szGoalopName = pPipe->GetGoalOpName(pTargetPuppet->m_lastExecutedGoalop);
		pRenderer->TextToScreen(0,64,"Current goal: %s", szGoalopName);

		int i=0;
		CGoalPipe *pSubPipe = pPipe;
		while (pSubPipe->IsInSubpipe())
		{
			pSubPipe = pSubPipe->GetSubpipe();
			char str[1024];
			memset(str,32,sizeof(char)*1024);
			str[0] = '+';
			strcpy(&str[1],pSubPipe->m_sName.c_str());
			pRenderer->TextToScreen(0.f,66.f+2.f*i,str);
			i++;
		}
	}

	DebugDrawPendingEvents(pRenderer, pTargetPuppet, 50, 20);

	if (!pTargetPuppet->m_State.vLookTargetPos.IsZero()) 
		pRenderer->TextToScreen(0,70,"LOOK");
	if (!pTargetPuppet->m_State.vAimTargetPos.IsZero()) 
		pRenderer->TextToScreen(10,70,"AIMLOOK");

	float maxExposure = 0.0f;
	float maxThreat = 0.0f;
	PotentialTargetMap::const_iterator ei = pTargetPuppet->m_perceptionEvents.begin();
	for (;ei != pTargetPuppet->m_perceptionEvents.end(); ++ei)
	{
		const SAIPotentialTarget& ed = ei->second;
		maxExposure = max(maxExposure, ed.exposure);
		maxThreat = max(maxThreat, ed.threatTime);
	}


	if (pTargetPuppet->GetAttentionTarget())
		pRenderer->TextToScreen(0,74,"Attention target: %s (T%.3f  E%.3f) alert: %d  %s", pTargetPuppet->GetAttentionTarget()->GetName(), maxExposure, maxThreat, pTargetPuppet->GetProxy()->GetAlertnessState(), pTargetPuppet->IsAlarmed() ? "Alarmed" : "" );
	else
		pRenderer->TextToScreen(0,74,"Attention target: <no target> (T%.3f  E%.3f) alert: %d  %s", maxExposure, maxThreat, pTargetPuppet->GetProxy()->GetAlertnessState(), pTargetPuppet->IsAlarmed() ? "Alarmed" : "");

	if (!pTargetPuppet->m_State.vSignals.empty())
	{
		int i=0;

		pRenderer->TextToScreen(0,78,"Pending signals:");
		DynArray<AISIGNAL>::iterator sig,iend=pTargetPuppet->m_State.vSignals.end();
		for (sig=pTargetPuppet->m_State.vSignals.begin();sig!=iend;++sig,i++)
			pRenderer->TextToScreen(0.f,80.f+2.f*i,(*sig).strText);
	}

	pRenderer->TextToScreen(50,62,"GR.MEMBERS:%d GROUPID:%d",m_mapGroups.count(pTargetPuppet->GetGroupId()),pTargetPuppet->GetGroupId());

	if (pTargetPuppet->GetProxy())
		pTargetPuppet->GetProxy()->DebugDraw(pRenderer,1);

/*
	if(pTargetObject->m_State.fire)
		pRenderer->GetIRenderAuxGeom()->DrawLine(pTargetObject->GetPos(), ColorF(1.0f,.2f,.1f,1.0f), pTargetObject->GetPos()+pTargetObject->GetViewDir()*125.0f, ColorF(.2f,.2f,1.0f,1.0f));
	else
		pRenderer->GetIRenderAuxGeom()->DrawLine(pTargetObject->GetPos(), ColorF(.2f,.2f,1.0f,1.0f), pTargetObject->GetPos()+pTargetObject->GetViewDir()*125.0f, ColorF(.2f,.2f,1.0f,1.0f));
	pRenderer->GetIRenderAuxGeom()->DrawLine(pTargetObject->GetPos(), ColorF(.1f,1.0f,.1f,1.0f), pTargetObject->GetPos()+pTargetObject->GetMoveDir()*125.0f, ColorF(.2f,.2f,1.0f,1.0f));

//	pRenderer->GetIRenderAuxGeom()->DrawLine(pTargetObject->GetPos(), ColorF(.1f,1.0f,.1f,1.0f), pTargetObject->GetPos()+pTargetObject->m_State.vLookDir*125.0f, ColorF(.2f,.2f,1.0f,1.0f));
*/

	const ColorF	desiredCol( 1.0f, 0.8f, 0.05f, 0.3f );
	const ColorF	actualCol( 0.1f, 0.2f, 0.8f, 0.5f );
	const ColorF	fireCol( 1.0f, 0.1f, 0.0f, 1.0f );

	const Vec3&	pos = pTargetObject->GetPos();
	const float	rad = pTargetPuppet->m_Parameters.m_fPassRadius;
	const float	sightRange = pTargetPuppet->m_Parameters.m_PerceptionParams.sightRange;

	// Extend the direction vectors.

	// The desired movement direction.
/*	if( pTargetObject->m_State.vMoveDir.GetLengthSquared() > 0.0f )
	{
		const Vec3&	dir = pTargetObject->m_State.vMoveDir;
		const Vec3	start = pos + dir * (rad + 1.0f);
		const Vec3	target = pos + dir * sightRange;
		pRenderer->GetIRenderAuxGeom()->DrawLine( start, desiredCol * 0.5f, target, desiredCol * 0.5f );
	}
	// The desired lookat direction.
	{
		const Vec3&	dir = pTargetObject->m_State.vLookDir;
		const Vec3	start = pos + dir * (rad + 2.0f);
		const Vec3	target = pos + dir * (rad + 2.0f);
		pRenderer->GetIRenderAuxGeom()->DrawLine( start, desiredCol * 0.5f, target, desiredCol * 0.5f );
	}
*/
	// The actual movement direction
/*	{
		const Vec3&	dir = pTargetObject->GetMoveDir();
		const Vec3	start = pos + dir * (rad + 1.3f);
		const Vec3	target = pos + dir * sightRange;
		pRenderer->GetIRenderAuxGeom()->DrawLine( start, actualCol * 0.5f, target, actualCol * 0.5f );
	}*/

	// The actual lookat direction
	{
/*		const Vec3	start = pos;
		const Vec3	target = pTargetObject->m_State.vShootTargetPos;

		// Indicate firing.
		if( pTargetObject->m_State.fire )
		{
//			const Vec3	target = pos + dir * (rad + 1.5f);
//			pRenderer->GetIRenderAuxGeom()->DrawCone( target, dir, 0.2f, 1.0f, fireCol );
//			pRenderer->GetIRenderAuxGeom()->DrawCone( target + dir * 0.9f, dir, 0.2f, 1.0f, fireCol );
			pRenderer->GetIRenderAuxGeom()->DrawLine( start, fireCol, target, fireCol );
		}
		else
		{
			if( pTargetPuppet->AllowedToFire() )
			{
//				const Vec3	target = pos + dir * (rad + 1.5f);
//				pRenderer->GetIRenderAuxGeom()->DrawCone( target, dir, 0.2f, 1.0f, fireCol * 0.5f );
//				pRenderer->GetIRenderAuxGeom()->DrawCone( target, dir, 0.07f, 0.2f, fireCol );
			}
//			pRenderer->GetIRenderAuxGeom()->DrawLine( start, fireCol * 0.5f, target, fireCol * 0.5f );
			pRenderer->GetIRenderAuxGeom()->DrawLine( start, actualCol * 0.5f, target, actualCol * 0.5f );
		}*/


//	const ColorF	queueFireCol( .4f, 1.0f, 0.4f, 1.0f );
//for(int dbgIdx(0);dbgIdx<curLimit; ++dbgIdx)
//	pRenderer->GetIRenderAuxGeom()->DrawLine( s_shots[dbgIdx].src, fireCol, s_shots[dbgIdx].dst, fireCol );

//		pRenderer->GetIRenderAuxGeom()->DrawSphere(pTargetPuppet->m_LastMissPoint, .25, ColorF(1,0,1,1));
	}

	// Draw fire command handler stuff.
	if (pTargetPuppet->m_pFireCmdHandler)
		pTargetPuppet->m_pFireCmdHandler->DebugDraw(pRenderer);
	if (pTargetPuppet->m_pFireCmdGrenade)
		pTargetPuppet->m_pFireCmdGrenade->DebugDraw(pRenderer);

	// draw hide point
//	if(pTargetPuppet
//	if(pPipeUser->m_bAllowedToFire)
//		pRC->DrawCone(posSelf+Vec3(0.f,0.f,.7f), Vec3(0,0,-1), .15f, .5f,ColorB(255,20,20));
//	if(pAIObj->m_State.fire)
	if(pTargetPuppet->m_CurrentHideObject.IsValid())
	{
		pRenderer->GetIRenderAuxGeom()->DrawCone( pTargetPuppet->m_CurrentHideObject.GetObjectPos()+Vec3(0,0,5), Vec3(0,0,-1), .2f, 5.f, 
		pTargetPuppet->AllowedToFire() ? ColorF(1.f,.1f,.1f) : ColorF(1.f,1.f,.1f) );
	}

  // draw predicted info
  for (int iPred = 0 ; iPred < pTargetPuppet->m_State.predictedCharacterStates.nStates ; ++iPred)
  {
    const SAIPredictedCharacterState &predState = pTargetPuppet->m_State.predictedCharacterStates.states[iPred];
    ColorF posCol(1.0f, 1.0f, 1.0f);
    ColorF velCol(0.0f, 1.0f, 0.7f);

    pRenderer->GetIRenderAuxGeom()->DrawSphere(predState.position, 0.1f, posCol);

    Vec3 velOffset(0.0f, 0.0f, 0.1f);
    pRenderer->GetIRenderAuxGeom()->DrawSphere(predState.position + velOffset, 0.1f, velCol);
    pRenderer->GetIRenderAuxGeom()->DrawLine(predState.position + velOffset, posCol,
      predState.position + velOffset + predState.velocity * 0.2f, posCol);
    static string speedTxt;
    speedTxt.Format("%5.2f", predState.velocity.GetLength());
    pRenderer->DrawLabel(predState.position + 4 * velOffset, 1.0f, speedTxt.c_str());
  }

	// Track and draw the trajectory of the agent.
  Vec3 physicsPos = pTargetObject->GetPhysicsPos();
	if( m_cvDrawTrajectory->GetIVal() )
	{
		int type = m_cvDrawTrajectory->GetIVal();
		static int lastReason = -1;
		bool	updated = false;

		int	reason = 0;
		if (type == 1)
			reason = pTargetPuppet->m_DEBUGmovementReason;
		else if (type ==2)
			reason = bodyInfo.stance;

		if( !m_lastStatsTargetTrajectoryPoint.IsZero() )
		{
			Vec3	delta = m_lastStatsTargetTrajectoryPoint - physicsPos;
			if( delta.len2() > sqr( 0.1f ) )
			{
				Vec3	prevDelta( delta );
				if( !m_lstStatsTargetTrajectory.empty() )
					prevDelta = m_lstStatsTargetTrajectory.back().end - m_lstStatsTargetTrajectory.back().start;

				float	c = delta.GetNormalizedSafe().Dot( prevDelta.GetNormalizedSafe() );

				if( lastReason != reason || c < cosf(DEG2RAD(15.0f)) || Distance::Point_Point( m_lastStatsTargetTrajectoryPoint, physicsPos ) > 2.0f )
				{
					ColorB	col;
					if (type == 1)
					{
						// Color the path based on movement reason.
						switch( pTargetPuppet->m_DEBUGmovementReason )
						{
						case CPipeUser::AIMORE_UNKNOWN: col.Set( 171, 168, 166, 255 ); break;
						case CPipeUser::AIMORE_TRACE: col.Set( 95, 182, 223, 255 ); break;
						case CPipeUser::AIMORE_MOVE: col.Set( 49, 110, 138, 255 ); break;
						case CPipeUser::AIMORE_MANEUVER: col.Set( 85, 191, 48, 255 ); break;
						case CPipeUser::AIMORE_SMARTOBJECT: col.Set( 240, 169, 16, 255 ); break;
						}
					}
					else if (type == 2)
					{
						// Color the path based on stance.
						switch( bodyInfo.stance )
						{
						case STANCE_NULL:			col.Set(171, 168, 166, 255); break;
						case STANCE_PRONE:		col.Set(85, 191, 48, 255);		break;
						case STANCE_STAND:		col.Set(95, 182, 223, 255);	break;
						case STANCE_STEALTH:	col.Set(49, 110, 138, 255);	break;
						}
					}
          else if (type == 3)
          {
            float sp = pTargetPuppet->m_State.fDesiredSpeed;
            float ur = pTargetPuppet->m_State.fMovementUrgency;
            if (sp <= 0.0f)
              col.Set(0, 0, 255, 255);
            else if (ur <= 1.0f)
              col.Set(0, sp * 255, 0, 255);
            else
              col.Set((sp - 1.0f) * 255, 0, 0, 255);
          }
					m_lstStatsTargetTrajectory.push_back( SDebugLine( m_lastStatsTargetTrajectoryPoint, physicsPos, col, 0 ) );
					m_lastStatsTargetTrajectoryPoint = physicsPos;

					lastReason = reason;
				}
			}

			if( !updated && !m_lstStatsTargetTrajectory.empty() )
				m_lstStatsTargetTrajectory.back().end = physicsPos;
		}
		else
		{
			m_lastStatsTargetTrajectoryPoint = physicsPos;
		}

		for( std::list<SDebugLine>::iterator lineIt = m_lstStatsTargetTrajectory.begin(); lineIt != m_lstStatsTargetTrajectory.end(); ++lineIt )
		{
			SDebugLine&	line = (*lineIt);
			pRenderer->GetIRenderAuxGeom()->DrawLine( line.start, line.color, line.end, line.color );
		}

		if( m_lstStatsTargetTrajectory.size() > 600 )
			m_lstStatsTargetTrajectory.pop_front();
	}

/*
	// Draw ranges relative to the player
	{
		const ColorB targetNoneCol(0,0,0,128);
		const ColorB targetInterestingCol(128,255,0,196);
		const ColorB targetThreateningCol(255,210,0,196);
		const ColorB targetAggressiveCol(255,64,0,196);

		SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags	newFlags;
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		newFlags.SetDepthWriteFlag(e_DepthWriteOff);
		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

		CAIActor* pEventOwner = GetPlayer()->CastToCAIActor();
		if (!pEventOwner)
			return;

		float distToTarget = Distance::Point_Point(pTargetPuppet->GetPos(), pEventOwner->GetPos());
		float sightRange = pTargetPuppet->GetSightRange(pEventOwner);
		float sightRangeThr = sightRange * (0.2f + pTargetPuppet->GetPerceptionAlarmLevel() * 0.3f);

		float sightRangeScale = 1.0f;
		float sightScale = 1.0f;

		// Target under water
		float waterOcclucion = GetAISystem()->GetWaterOcclusionValue(pEventOwner->GetPos());
		sightRangeScale *= 0.1f + (1 - waterOcclucion) * 0.9f;

		// Target stance
		float stanceSize = 1.0f;
		SAIBodyInfo bi;
		if (pEventOwner->GetProxy())
			pEventOwner->GetProxy()->QueryBodyInfo(bi);
		float targetHeight = bi.m_stanceSize.GetSize().z;
		if (pEventOwner->GetType() == AIOBJECT_VEHICLE)
			targetHeight = 0.0f;
		if (targetHeight > 0.0f)
			stanceSize = targetHeight / pTargetPuppet->m_Parameters.m_PerceptionParams.stanceScale;
		sightRangeScale *= 0.25f + stanceSize * 0.75f;

		//use target scale factor
		if (pTargetPuppet->m_Parameters.m_PerceptionParams.bThermalVision)
			sightRangeScale *= pEventOwner->m_Parameters.m_PerceptionParams.heatScale;
		else
			sightRangeScale *= pEventOwner->m_Parameters.m_PerceptionParams.camoScale;

		// Secondly calculate factors that will affect the actual sighting value.

		// Scale the sighting based on the agent FOV.
		if (pTargetPuppet->m_FOVPrimaryCos > 0.0f)
		{
			Vec3 dirToTarget = pEventOwner->GetPos() - pTargetPuppet->GetPos();
			Vec3 viewDir = pTargetPuppet->GetViewDir();
			if (!pTargetPuppet->IsUsing3DNavigation())
			{
				dirToTarget.z = 0;
				viewDir.z = 0;
			}
			dirToTarget.Normalize();
			viewDir.Normalize();

			float dot = viewDir.Dot(dirToTarget);
			float fovFade = 1.0f;
			if (dot < pTargetPuppet->m_FOVPrimaryCos)
				fovFade = LinStep(pTargetPuppet->m_FOVSecondaryCos, pTargetPuppet->m_FOVPrimaryCos, dot);

			// When the target is really close, reduce the effect of the FOV.
			if (distToTarget < pTargetPuppet->m_fRadius * 2)
				fovFade = max(fovFade, LinStep(pTargetPuppet->m_fRadius * 2, pTargetPuppet->m_fRadius, distToTarget));

			sightScale *= fovFade;
		}

		// Target cloak
		if (!pEventOwner->IsInvisibleFrom(pTargetPuppet->GetPos()))
		{
			if (pEventOwner->m_Parameters.m_fCloakScale > 0.5f && !pEventOwner->GetGrabbedEntity())
			{
				// Cloak on and not carrying anything
				// Fade out the perception increment based on the cloak distance parameters.
				const float cloakMinDist = GetAISystem()->m_cvCloakMinDist->GetFVal();
				const float cloakMaxDist = GetAISystem()->m_cvCloakMaxDist->GetFVal();
				sightScale *= LinStep(cloakMaxDist, cloakMinDist, distToTarget);
			}
		}

		sightRange *= sightRangeScale;
		sightRangeThr *= sightRangeScale;

		float visualThreatLevel = 0.0f;
		if (sightRange > 0.0f)
			visualThreatLevel = LinStep(sightRange, sightRangeThr, distToTarget) * sightScale;

		Vec3 dir = pEventOwner->GetPos() - pTargetPuppet->GetPos();
		dir.z = 0;
		dir.Normalize();
		const Vec3 up(0,0,1);
		const Vec3 pos = pTargetPuppet->GetPos();
		const float h = 2.0f;

		pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,255), pos + dir*sightRange, ColorB(255,255,255));
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,255), pos + up*h, ColorB(255,255,255));
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos+up*h, ColorB(255,255,255), pos + up*h + dir*sightRangeThr, ColorB(255,255,255));
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos + dir*sightRange, ColorB(255,255,255), pos + up*h + dir*sightRangeThr, ColorB(255,255,255));

		float d = 0;
		// Interested
		d = sightRange + (sightRangeThr - sightRange) * 0.3f;
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos + dir*d, targetInterestingCol, pos + up*h + dir*d, targetInterestingCol);

		// Threatened
		d = sightRange + (sightRangeThr - sightRange) * 0.6f;
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos + dir*d, targetThreateningCol, pos + up*h + dir*d, targetThreateningCol);

		// Aggressive
		d = sightRange + (sightRangeThr - sightRange) * 0.9f;
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos + dir*d, targetAggressiveCol, pos + up*h + dir*d, targetAggressiveCol);


		pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
	}
*/


	const ColorB targetNoneCol(0,0,0,128);
	const ColorB targetInterestingCol(128,255,0,196);
	const ColorB targetThreateningCol(255,210,0,196);
	const ColorB targetAggressiveCol(255,64,0,196);


	SAuxGeomRenderFlags	oldFlags = pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetDepthWriteFlag(e_DepthWriteOff);
	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

	CCamera& cam = GetISystem()->GetViewCamera();
	Vec3 axisx = cam.GetMatrix().TransformVector(Vec3(1,0,0));
	Vec3 axisy = cam.GetMatrix().TransformVector(Vec3(0,0,1));

	// Draw perception events
	for (PotentialTargetMap::iterator ei = pTargetPuppet->m_perceptionEvents.begin(), end = pTargetPuppet->m_perceptionEvents.end(); ei != end; ++ei)
	{
		SAIPotentialTarget& ed = ei->second;
		CAIObject* pOwner = ei->first;

		ColorB col;
		switch(ed.threat)
		{
		case AITHREAT_INTERESTING: col = targetInterestingCol; break;
		case AITHREAT_THREATENING: col = targetThreateningCol; break;
		case AITHREAT_AGGRESSIVE: col = targetAggressiveCol; break;
		default: col = targetNoneCol;
		}

		Vec3 pos(0,0,0);
		const char* szType = "NONE";
		switch (ed.type)
		{
		case AITARGET_VISUAL:
			szType = "VISUAL";
			{
				ColorB vcol(255,255,255);
				if (ed.threat == AITHREAT_AGGRESSIVE)
				{
					pos = pOwner->GetPos();
				}
				else
				{
					pos = ed.pDummyRepresentation->GetPos();
					vcol.a = 64;
				}
				pRenderer->GetIRenderAuxGeom()->DrawLine(pTargetPuppet->GetPos(), vcol, pos, vcol);
			}
			break;
		case AITARGET_MEMORY:
			szType = "MEMORY";
			pos = ed.pDummyRepresentation->GetPos();
			{
				ColorB vcol(0,0,0,128);
				pRenderer->GetIRenderAuxGeom()->DrawLine(pTargetPuppet->GetPos(), vcol, pos, vcol);
			}
			break;
		case AITARGET_SOUND:
			szType = "SOUND";
			pos = ed.pDummyRepresentation->GetPos();
			break;
		default:
			if (ed.visualThreatLevel > ed.soundThreatLevel)
				pos = ed.visualPos;
			else
				pos = ed.soundPos;
		};

		pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, 0.25f, col);

		const char* szVisType = "";
		switch (ed.visualType)
		{
		case SAIPotentialTarget::VIS_VISIBLE: szVisType = "vis"; break;
		case SAIPotentialTarget::VIS_MEMORY: szVisType = "mem"; break;
		};

		pRenderer->DrawLabel(pos, 1.0f, "%s\nt=%.1fs\nexp=%.3f\nsound=%.3f\nsight=%.3f\n%s", szType, ed.threatTimeout, ed.exposure, ed.soundThreatLevel, ed.visualThreatLevel, szVisType);
	}
//	pRenderer->DrawLabel(pTargetPuppet->GetPos()+Vec3(0,0,2), 1.5f, "alarmed=%.1f\nsightThr=%.2f", pTargetPuppet->m_alarmedTime, pTargetPuppet->m_alarmedSightRangeThr);
//	pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);

}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawFormations(IRenderer *pRenderer) const
{
	if(!m_cvDrawFormations->GetIVal())
		return;
	for(FormationMap::const_iterator itForm=m_mapActiveFormations.begin(); itForm!= m_mapActiveFormations.end();++itForm)
		if(itForm->second)
			(itForm->second)->Draw(pRenderer);
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawNavModifiers(IRenderer *pRenderer) const
{
	if (!m_cvDrawModifiers->GetIVal())
		return;
	Vec3	offset(0.0f,0.0f,0.1f);
	for(SpecialAreaMap::const_iterator si = m_mapSpecialAreas.begin(); si!=m_mapSpecialAreas.end(); ++si)
	{
		const SpecialArea& sa = si->second;
		if(sa.GetPolygon().size()<3)
			continue;
		ColorF col(0.1f, 0.5f, 0.9f); // match editor shape colour
		if(sa.bAltered)
			col.Set(0.95f, 0.8f, 0.25f);

		ListPositions::const_iterator	itrPos=sa.GetPolygon().begin();
		Vec3 prevPoint = *itrPos;

		for(++itrPos; itrPos!=sa.GetPolygon().end(); ++itrPos)
		{
			pRenderer->GetIRenderAuxGeom()->DrawLine(prevPoint + offset, col, (*itrPos) + offset, col);
			prevPoint = *itrPos;
		}
		pRenderer->GetIRenderAuxGeom()->DrawLine(prevPoint + offset, col, sa.GetPolygon().front() + offset, col);
	}
}

//====================================================================
// DebugDrawTaggedNodes
//====================================================================
void CAISystem::DebugDrawTaggedNodes(IRenderer *pRenderer) const
{
	CGraph *pGraph = m_pGraph;

	ColorF col(1,1,1,1.0);

	const VectorConstNodeIndices & nodes = m_pGraph->GetTaggedNodes();
	for (VectorConstNodeIndices::const_iterator it = nodes.begin() ; it != nodes.end() ; ++it)
	{
		const GraphNode* node = m_pGraph->GetNodeManager().GetNode(*it);
    if (node->navType == IAISystem::NAV_TRIANGULAR && node->GetTriangularNavData()->vertices.size() >= 3)
		{
			Vec3 one,two,three;
			one = m_VertexList.GetVertex(node->GetTriangularNavData()->vertices[0]).vPos;
			two = m_VertexList.GetVertex(node->GetTriangularNavData()->vertices[1]).vPos;
			three = m_VertexList.GetVertex(node->GetTriangularNavData()->vertices[2]).vPos;
      one.z = GetDebugDrawZ(one, true);
      two.z = GetDebugDrawZ(two, true);
      three.z = GetDebugDrawZ(three, true);

			pRenderer->GetIRenderAuxGeom()->DrawLine(one, col, two, col);
			pRenderer->GetIRenderAuxGeom()->DrawLine(two, col, three, col);
			pRenderer->GetIRenderAuxGeom()->DrawLine(three, col, one, col);
		}
    Vec3 nodePos = node->GetPos();
    nodePos.z = GetDebugDrawZ(nodePos, node->navType == IAISystem::NAV_TRIANGULAR);
		pRenderer->DrawLabel(nodePos,1,"%.2f", node->fCostFromStart);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawNode(IRenderer *pRenderer) const
{
	const char *pName=m_cvDrawNode->GetString();
	if(!pName )
		return;
	if(!strcmp(pName, "none"))
	{
		return;
	}
	CAIObject *pTargetObject = GetAIObjectByName(pName);
	if (pTargetObject)
	{
		DebugDrawNodeSingle(pRenderer, pTargetObject);
		return;
	}

	if(!strcmp(pName, "player"))
	{
		DebugDrawNodeSingle( pRenderer, GetPlayer() );
		return;
	}

	if(strcmp(pName, "all"))
		return;

	DebugDrawNodeSingle( pRenderer, GetPlayer() );
	// find if there are any puppets
	int cnt=0;
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
  {
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			DebugDrawNodeSingle( pRenderer, ai->second );
		}

    DebugDrawForbidden(pRenderer);
  }
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawNodeSingle(IRenderer *pRenderer, CAIObject* pTargetObject) const
{
	if (!pTargetObject)
		return;
	CGraph *pGraph;
	if (m_cvDrawHide->GetIVal())
		pGraph = m_pHideGraph;
	else
		pGraph = m_pGraph;

	ColorF col(1.0,1,1,1.0);

  Vec3 ppos = pTargetObject->GetPhysicsPos();
	unsigned playerNodeIndex = pGraph->GetEnclosing(ppos, IAISystem::NAVMASK_SURFACE);
	GraphNode *pPlayerNode = pGraph->GetNodeManager().GetNode(playerNodeIndex);
//	AIAssert(pPlayerNode);
	if (!pPlayerNode)
		return;

	int debugTextType = m_cvDrawNodeLinkType->GetIVal();

	bool	bHasPassibleConnections = false;
	for (unsigned link = pPlayerNode->firstLinkIndex; link; link = pGraph->GetLinkManager().GetNextLink(link))
	{
		unsigned connectedIndex = pGraph->GetLinkManager().GetNextNode(link);
		GraphNode *pConnectedNode=pGraph->GetNodeManager().GetNode(connectedIndex);
		if (!pConnectedNode)
			continue;
		if (pPlayerNode->navType == IAISystem::NAV_TRIANGULAR)
		{
			float value;
			switch (debugTextType)
			{
			case 0: value = pGraph->GetLinkManager().GetRadius(link); break;
			case 1: value = pGraph->GetLinkManager().GetExposure(link); break;
			case 2: value = pGraph->GetLinkManager().GetMaxWaterDepth(link); break;
			case 3: value = pGraph->GetLinkManager().GetMinWaterDepth(link); break;
			default: value = -123456.0f; break;
			}
			float cutoff = m_cvDrawNodeLinkCutoff->GetFVal();
			float fColor[4] = {value <= cutoff, value > cutoff, 0 , 1};
			pRenderer->DrawLabelEx(pGraph->GetLinkManager().GetEdgeCenter(link), 1.0f, fColor, true, false, "%.2f", value);
		}
		if (pGraph->GetLinkManager().GetRadius(link)<0)
		{
			col.Set(0,1,0,1);
		}
		else
		{
			col.Set(1,1,1,1);
			bHasPassibleConnections = true;
		}
    Vec3 nodePos = pPlayerNode->GetPos();
    Vec3 otherNodePos = pConnectedNode->GetPos();
    Vec3 edgePos = pGraph->GetLinkManager().GetEdgeCenter(link);
    edgePos.z = GetDebugDrawZ(edgePos, pPlayerNode->navType == IAISystem::NAV_TRIANGULAR);
    nodePos.z = GetDebugDrawZ(nodePos, pPlayerNode->navType == IAISystem::NAV_TRIANGULAR);
    otherNodePos.z = GetDebugDrawZ(otherNodePos, pPlayerNode->navType == IAISystem::NAV_TRIANGULAR);
		pRenderer->GetIRenderAuxGeom()->DrawLine(nodePos, col, edgePos, col);
		pRenderer->GetIRenderAuxGeom()->DrawLine(edgePos, col, otherNodePos, col);
	} //vi
	col.Set(1.0,1.0,0,1.0);
	if(!bHasPassibleConnections)
		col.Set(1,0,0,1);
  static float centerRadius = 0.05f;
  Vec3 nodePos = pPlayerNode->GetPos();
  nodePos.z = GetDebugDrawZ(nodePos, pPlayerNode->navType == IAISystem::NAV_TRIANGULAR);
	pRenderer->GetIRenderAuxGeom()->DrawSphere(nodePos, centerRadius, col);

	if (pPlayerNode->navType == IAISystem::NAV_TRIANGULAR)
	{
		GraphNode *pCurrentNode =pPlayerNode;
		// draw the triangle
		for (unsigned link = pCurrentNode->firstLinkIndex; link; link = pGraph->GetLinkManager().GetNextLink(link))
		{
			Vec3 start = m_VertexList.GetVertex(pCurrentNode->GetTriangularNavData()->vertices[pGraph->GetLinkManager().GetStartIndex(link)]).vPos;
			Vec3 end = m_VertexList.GetVertex(pCurrentNode->GetTriangularNavData()->vertices[pGraph->GetLinkManager().GetEndIndex(link)]).vPos;
			Vec3 horDelta = end - start;
			horDelta.z = 0.0f;
			float horDist = horDelta.GetLength();
			if (pGraph == m_pGraph && pGraph->GetLinkManager().GetRadius(link) > 0.0f && horDist > 0.0001f)
			{
				Vec3 mid = pGraph->GetLinkManager().GetEdgeCenter(link);
				mid.z = start.z;
				Vec3 horDir = horDelta / horDist;
				Vec3 startSafe = mid - pGraph->GetLinkManager().GetRadius(link) * horDir;
				Vec3 endSafe = mid + pGraph->GetLinkManager().GetRadius(link) * horDir;

				// now set the z values
				float distToStartSafe = (startSafe - start).GetLength();
				float distToEndSafe = (endSafe - start).GetLength();
				startSafe.z = start.z + (end.z - start.z) * distToStartSafe / horDist;
				endSafe.z = start.z + (end.z - start.z) * distToEndSafe / horDist;

				static float criticalRadius = 0.4f;
				col.Set(1.0,0,0,1.0);
        start.z = GetDebugDrawZ(start, true);
        startSafe.z = GetDebugDrawZ(startSafe, true);
        endSafe.z = GetDebugDrawZ(endSafe, true);
        end.z = GetDebugDrawZ(end, true);
				pRenderer->GetIRenderAuxGeom()->DrawLine(start, col, startSafe, col);
				pRenderer->GetIRenderAuxGeom()->DrawLine(endSafe, col, end, col);
				if (pGraph->GetLinkManager().GetRadius(link) > criticalRadius)
					col.Set(1.0,1.0,0,1.0);
				pRenderer->GetIRenderAuxGeom()->DrawLine(startSafe, col, endSafe, col);
			}
			else
			{
				col.Set(1.0,0,0,1.0);
        start.z = GetDebugDrawZ(start, true);
        end.z = GetDebugDrawZ(end, true);
				pRenderer->GetIRenderAuxGeom()->DrawLine(start, col, end, col);
			}
		}
		ObstacleIndexVector::iterator oi;
		for (oi=pCurrentNode->GetTriangularNavData()->vertices.begin();oi!=pCurrentNode->GetTriangularNavData()->vertices.end();oi++)
		{
      const ObstacleData &ob = m_VertexList.GetVertex(*oi);
      if (ob.bCollidable)
  			col.Set(1.0f,0.5f,0.5f,1.f);
      else
  			col.Set(0.5f,1.0f,0.5f,1.f);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(ob.vPos, 0.05f, col);
      if (ob.fApproxRadius > 0.0f)
        DebugDrawCircles(pRenderer, ob.vPos, ob.fApproxRadius, ob.fApproxRadius, 1, Vec3(1, 1, 1), Vec3(1, 1, 1));
		}
	}
}

//====================================================================
// DebugDrawUnreachableNodeLocations
//====================================================================
void CAISystem::DebugDrawUnreachableNodeLocations(IRenderer *pRenderer) const
{
  if (m_unreachableNodeLocations.empty())
    return;
  for (unsigned i = 0 ; i < m_unreachableNodeLocations.size() ; ++i)
  {
    const Vec3 &pos = m_unreachableNodeLocations[i];
    pRenderer->GetIRenderAuxGeom()->DrawCone(pos, Vec3(0, 0, 1), 0.1f, 100.0f, ColorB(255, 0, 255));
  }
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawType(IRenderer *pRenderer) const 
{
	int type=m_cvDrawType->GetIVal();

	if(type == 0)
	{
		for(SetAIObjects::const_iterator iDummy=m_setDummies.begin(); iDummy!=m_setDummies.end(); ++iDummy)
			DebugDrawTypeSingle( pRenderer, (*iDummy) );
	}

	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(type)) != m_Objects.end())
		for (;ai!=m_Objects.end(); ++ ai)
		{
			if (ai->first != type)
				break;
			DebugDrawTypeSingle( pRenderer, ai->second );
		}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawTypeSingle(IRenderer *pRenderer, CAIObject* pAIObj) const
{
	Vec3 pos = pAIObj->GetPhysicsPos();
	ColorF col(0.9f,0.9f,.1f,1.0f);
	pRenderer->GetIRenderAuxGeom()->DrawLine(pos, col, pos + pAIObj->GetMoveDir()*2.0f, col);
	if(pAIObj->IsEnabled())
		col.Set(.0f,.0f,.7f,1.0f);
	else
		col.Set(1.0f,.3f,.2f,1.0f);
	pRenderer->GetIRenderAuxGeom()->DrawSphere(pos,.3f, col);
	pRenderer->DrawLabel(pos,1,"%s", pAIObj->GetName());

}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawSquadmates(IRenderer *pRenderer) const
{
	int groupId=0;
	const CAIObject* pAIObj =NULL;
	//
	//	find the leader
	AIObjects::const_iterator aio = m_Objects.find(AIOBJECT_LEADER);
	for (;aio!=m_Objects.end();++aio)
	{
		if (aio->first != AIOBJECT_LEADER)
			break;
		CLeader *pLeader = 0;
		CAIActor* pActor = aio->second->CastToCAIActor();
		if( pActor->GetGroupId() == groupId )
		{
			pAIObj = (aio->second);
			break;
		}
	}
	if (!pAIObj)
		return;
	if (!(aio->second)->CastToCLeader())
		return;
}



//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawTargetsList(IRenderer *pRenderer) const
{
	if(!m_cvDrawTargets->GetIVal())
		return;
	float drawDist2 = static_cast<float>(m_cvDrawTargets->GetIVal());
	drawDist2*=drawDist2;
	int col(1), row(1);
	string eventDescr;
	string atTarget;
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			if (!ai->second->IsEnabled())
				continue;
			eventDescr = "--";
			atTarget = "--";
			float dist2 = (GetDebugCameraPos() - (ai->second)->GetPos()).GetLengthSquared();
			if(dist2>drawDist2)
				continue;
			CPuppet *pTargetPuppet = ai->second->CastToCPuppet();
			if (!pTargetPuppet)
				continue;
			DebugDrawTargetUnit(pRenderer, pTargetPuppet);
			float fMaxPriority = -1.f;
			const SAIPotentialTarget* maxEvent = 0;
			CAIObject *pNextTarget = 0;
			PotentialTargetMap::iterator ei = pTargetPuppet->m_perceptionEvents.begin(), eiend = pTargetPuppet->m_perceptionEvents.end();
			for (; ei != eiend; ++ei)
			{
				const SAIPotentialTarget &ed = ei->second;
				// target selection based on priority
				if (ed.priority>fMaxPriority)
				{
					fMaxPriority = ed.priority;
					maxEvent = &ed;

					if ((ed.type == AITARGET_VISUAL && ed.threat == AITHREAT_AGGRESSIVE) || !ed.pDummyRepresentation)
						pNextTarget = ei->first;
					else
						pNextTarget = ed.pDummyRepresentation;
				}
			}

			if (fMaxPriority >= 0.0f && maxEvent)
			{
				const char* szName = "NULL";
				if (pNextTarget)
					szName = pNextTarget->GetName();

				const char* szTargetType = "";
				const char* szTargetThreat = "";

				switch (maxEvent->type)
				{
				case AITARGET_VISUAL: szTargetType = "VIS"; break;
				case AITARGET_MEMORY: szTargetType = "MEM"; break;
				case AITARGET_SOUND: szTargetType = "SND"; break;
				default: szTargetType = "-  "; break;
				}

				switch (maxEvent->threat)
				{
				case AITHREAT_AGGRESSIVE: szTargetThreat = "AGG"; break;
				case AITHREAT_THREATENING: szTargetThreat = "THR"; break;
				case AITHREAT_INTERESTING: szTargetThreat = "INT"; break;
				default: szTargetThreat = "-  "; break;
				}

				float timeout = maxEvent->GetTimeout(pTargetPuppet->GetParameters().m_PerceptionParams);

				char bfr[256];
				sprintf( bfr, "%.3f %.1f  <%s %s>  %s", fMaxPriority, timeout, szTargetType, szTargetThreat, szName);
				eventDescr = bfr;
			}
			CPipeUser* pPipeUser = ai->second->CastToCPipeUser();
			if (pPipeUser && pPipeUser->GetAttentionTarget())
			{
				atTarget = pPipeUser->GetAttentionTarget()->GetName();
			}
			float color[4]={0.f,1.f,1.f,1.f};
			float colorGray[4]={.5f,.4f,.4f,1.f};
			
//			if(!pTargetPuppet->IsInFrontOfPlayer())
				DebugDrawLabel(pRenderer, col, row, ai->second->GetName(), colorGray );
//			else
//				DebugDrawLabel(pRenderer, col, row, ai->second->GetName(), color);
			DebugDrawLabel(pRenderer, col+12, row, eventDescr.c_str(), color);
			DebugDrawLabel(pRenderer, col+36, row, atTarget.c_str(), color);
			++row;
		}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawStatsList(IRenderer *pRenderer) const
{
	if(!m_cvDrawStats->GetIVal())
		return;
	float drawDist2 = static_cast<float>(m_cvDrawStats->GetIVal());
	drawDist2*=drawDist2;
	int col(1), row(1);
  // avoid memory allocations
	static string goalPipeName;
	static string atTargetName;
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			if (!ai->second->IsEnabled())
				continue;
			float dist2 = (GetDebugCameraPos() - (ai->second)->GetPos()).GetLengthSquared();
			if(dist2>drawDist2)
				continue;
			CPuppet* pTargetPuppet = ai->second->CastToCPuppet();
			if (!pTargetPuppet)
				continue;

			CPipeUser* pPipeUser = ai->second->CastToCPipeUser();
			if (pPipeUser && pPipeUser->GetAttentionTarget())
				atTargetName = pPipeUser->GetAttentionTarget()->GetName();
			else
				atTargetName = "--";

			CGoalPipe* pPipe = pPipeUser ? pPipeUser->GetCurrentGoalPipe() : 0;

			if (pPipe)
				goalPipeName = pPipe->m_sName.c_str();
			else
				goalPipeName = "--";

			if(pPipeUser)
			{
				float color[4]={0.f,1.f,1.f,1.f};
				DebugDrawLabel(pRenderer, col, row, ai->second->GetName(), color);
				DebugDrawLabel(pRenderer, col+12, row, atTargetName.c_str(), color);
				DebugDrawLabel(pRenderer, col+36, row, goalPipeName.c_str(), color);

				const char* szGoalopName = "--";
				if (pPipeUser->m_lastExecutedGoalop != LAST_AIOP && pPipe)
					szGoalopName = pPipe->GetGoalOpName(pPipeUser->m_lastExecutedGoalop);
				DebugDrawLabel(pRenderer, col+52, row, szGoalopName, color);
				++row;
			}
		}
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawUpdate(IRenderer *pRenderer) const
{
	EDrawUpdateMode mode = DRAWUPDATE_NONE;
	switch (m_cvDebugDrawUpdate->GetIVal())
	{
	case 1: mode = DRAWUPDATE_NORMAL; break;
	case 2: mode = DRAWUPDATE_WARNINGS_ONLY; break;
	};
	if (mode == DRAWUPDATE_NONE)
		return;

	int totalCount = 0;
	int	activeVehicleCount = 0;
	int	activePuppetCount = 0;

	int col(1), row(1);

	AIObjects::const_iterator ai;
	for (ai = m_Objects.find(AIOBJECT_PUPPET); ai!=m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;
		CPuppet* pTargetPuppet = ai->second->CastToCPuppet();
		if (pTargetPuppet->IsActive())
		{
			++activePuppetCount;
			if (DebugDrawUpdateUnit(pRenderer, pTargetPuppet, row, mode))
				++row;
		}
		++totalCount;
	}
	for (ai = m_Objects.find(AIOBJECT_VEHICLE); ai!=m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_VEHICLE)
			break;
		CPuppet* pTargetPuppet = ai->second->CastToCPuppet();
		if (pTargetPuppet->IsActive())
		{
			++activeVehicleCount;
			if (DebugDrawUpdateUnit(pRenderer, pTargetPuppet, row, mode))
				++row;
		}
		++totalCount;
	}

	static char	buffString[128];
	sprintf(buffString, "AI UPDATES - Puppets: %3d  Vehicles: %3d  Total: %3d/%d  EnabledPuppetSet: %d",
		activePuppetCount, activeVehicleCount, (activePuppetCount+activeVehicleCount), totalCount, m_enabledPuppetsSet.size());
	float color[4]={0,0.75f,1,1};
	DebugDrawLabel(pRenderer, 1, 0, buffString, color);
}


//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::DebugDrawUpdateUnit(IRenderer *pRenderer, CPuppet* pTargetPuppet, int row, EDrawUpdateMode mode) const
{
	bool bShouldUpdate = pTargetPuppet->GetProxy()->IfShouldUpdate();

	if (mode == DRAWUPDATE_WARNINGS_ONLY && bShouldUpdate)
		return false;

	float colorOk[4] = {1,1,1,1};
	float colorWarning[4] = {1,0.75f,0,1};
	float* pCurColor = bShouldUpdate ? colorOk : colorWarning;

	DebugDrawLabel(pRenderer, 1, row, pTargetPuppet->GetName(), pCurColor);

	char	buffString[32];
	float dist = Distance::Point_Point(pTargetPuppet->GetPos(), GetDebugCameraPos());
	sprintf(buffString, "%6.1fm", dist);
	DebugDrawLabel(pRenderer, 20, row, buffString, pCurColor);

	if (pTargetPuppet->GetProxy()->IsUpdateAlways())
		DebugDrawLabel(pRenderer, 27, row, "Always", pCurColor);

	return true;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawLocate(IRenderer *pRenderer) const
{
	const char *pString=m_cvDrawLocate->GetString();

	if( pString[0] ==0 || !strcmp(pString,"none"))
	{
		return;
	}
	else if(!strcmp(pString,"squad"))
	{
		AIObjects::const_iterator ai;
		if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
			for (;ai!=m_Objects.end(); ++ai)
			{
				if (ai->first != AIOBJECT_PUPPET)
					break;
				CPuppet *pPuppet = (CPuppet*) ai->second;
				if(pPuppet->GetGroupId() == 0)
					DebugDrawLocateUnit(pRenderer, pPuppet);
			}
	}
	else 	if(!strcmp(pString,"enemy"))
	{
		AIObjects::const_iterator ai;
		if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
			for (;ai!=m_Objects.end(); ++ai)
			{
				if (ai->first != AIOBJECT_PUPPET)
					break;
				CPuppet *pPuppet = (CPuppet*) ai->second;
				if(pPuppet->GetGroupId() != 0)
					DebugDrawLocateUnit(pRenderer, pPuppet);
			}
	}
	else 	if(!strcmp(pString,"all"))
	{
		AIObjects::const_iterator ai;
		if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
			for (;ai!=m_Objects.end(); ++ai)
			{
				if (ai->first != AIOBJECT_PUPPET)
					break;
				CPuppet *pPuppet = (CPuppet*) ai->second;
				DebugDrawLocateUnit(pRenderer, pPuppet);
			}
	}
	else if(strlen(pString)>1)
	{
		CAIObject *pTargetObject = GetAIObjectByName(pString);
		if (pTargetObject)
		{
			DebugDrawLocateUnit(pRenderer, pTargetObject);
		}
		else
		{
			int groipId=-1;
			if(sscanf(pString, "%d", &groipId) == 1)
			{
				AIObjects::const_iterator ai;
				if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
					for (;ai!=m_Objects.end(); ++ai)
					{
						if (ai->first != AIOBJECT_PUPPET)
							break;
						CPuppet *pPuppet = (CPuppet*) ai->second;
						if(pPuppet->GetGroupId() == groipId)
							DebugDrawLocateUnit(pRenderer, pPuppet);
					}
			}
		}
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawTargetUnit(IRenderer *pRenderer, CAIObject* pAIObj) const
{
	CPipeUser *pPipeUser = pAIObj->CastToCPipeUser();
	if (!pPipeUser)
		return;
	CAIObject* pTarget = static_cast<CAIObject*>(pPipeUser->GetAttentionTarget());
	if(!pTarget)
		return;
	Vec3	posSelf = pAIObj->GetPos();
	Vec3	posTarget = pTarget->GetPos();
	Vec3	verOffset(0,0,2);
	//Vec3	middlePoint( (posSelf+posTarget)*.5f + Vec3(0,0,3) );
	Vec3	middlePoint( posSelf+(posTarget-posSelf)*.73f + Vec3(0,0,3) );

	ColorB colorSelf(250,250,25);
	ColorB colorTarget(250,50,50);
	ColorB colorMiddle(250,100,50);

	IRenderAuxGeom *pRC = pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	//	renderFlags.SetDepthTestFlag( e_DepthTestOff );
	pRC->SetRenderFlags( renderFlags );
	pRC->DrawLine(posSelf, colorSelf, posSelf+verOffset, colorSelf);
	pRC->DrawLine(posSelf+verOffset, colorSelf, middlePoint, colorMiddle);
	pRC->DrawLine(middlePoint, colorMiddle, posTarget, colorTarget);
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawSelectedHideSpots(IRenderer *pRenderer) const
{
	if(!m_cvDrawHideSpots->GetIVal())
		return;
	float drawDist2 = static_cast<float>(m_cvDrawHideSpots->GetIVal());
	drawDist2*=drawDist2;
	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(AIOBJECT_PUPPET)) != m_Objects.end())
		for (;ai!=m_Objects.end(); ++ai)
		{
			if (ai->first != AIOBJECT_PUPPET)
				break;
			if (!ai->second->IsEnabled())
				continue;
			float dist2 = (GetDebugCameraPos() - (ai->second)->GetPhysicsPos()).GetLengthSquared();
			if(dist2>drawDist2)
				continue;
			DebugDrawMyHideSpot(pRenderer, ai->second);
		}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawMyHideSpot(IRenderer *pRenderer, CAIObject* pAIObj) const
{
	CPipeUser *pPipeUser = pAIObj->CastToCPipeUser();
	if (!pPipeUser)
		return;

	if(!pPipeUser->m_CurrentHideObject.IsValid())
		return;

	pRenderer->GetIRenderAuxGeom()->DrawCone( pPipeUser->m_CurrentHideObject.GetLastHidePos()+Vec3(0,0,5), Vec3(0,0,-1), .2f, 5.f, ColorF(1.f,.1f,.1f));

	Vec3	posSelf = pAIObj->GetPhysicsPos();
	Vec3	posTarget = pPipeUser->m_CurrentHideObject.GetLastHidePos()+Vec3(0,0,5); // ObjectPos()+Vec3(0,0,5);
	Vec3	verOffset(0,0,2);
	Vec3	middlePoint( (posSelf+posTarget)*.5f + Vec3(0,0,3) );

	ColorB colorSelf(250,250,25);
	ColorB colorTarget(250,50,50);
	ColorB colorMiddle(250,100,50);

	pPipeUser->m_CurrentHideObject.DebugDraw(pRenderer);

	IRenderAuxGeom *pRC = pRenderer->GetIRenderAuxGeom();
	pRC->DrawLine(posSelf, colorSelf, posSelf+verOffset, colorSelf);
	pRC->DrawLine(posSelf+verOffset, colorSelf, middlePoint, colorMiddle);
	pRC->DrawLine(middlePoint, colorMiddle, posTarget+verOffset, colorMiddle);
	pRC->DrawLine(posTarget+verOffset, colorMiddle, posTarget, colorTarget);
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawLocateUnit(IRenderer *pRenderer, CAIObject* pAIObj) const
{
	Vec3	posSelf = pAIObj->GetPos();
	Vec3	posMarker = posSelf + Vec3(0,0,3);
	ColorB colorSelf(20,20,250);
	ColorB colorMarker;

	if(pAIObj->IsEnabled())
		colorMarker.Set(250,250,30, 255);
	else
		//		colorMarker.Set(250,27,27, 255);
		return;

	CPipeUser* pPipeUser = pAIObj->CastToCPipeUser();
	if (!pPipeUser)
		return;

	if (pPipeUser->GetAttentionTarget())
	{
		colorMarker.Set(250,27,27, 255);
		pRenderer->DrawLabel(posMarker-Vec3(0.f,0.f,.21f), .8f, "%s\n\r%s", pAIObj->GetName(),pPipeUser->GetAttentionTarget()->GetName());
	}
	else
	{
		colorMarker.Set(250,250,30, 255);
		pRenderer->DrawLabel(posMarker-Vec3(0.f,0.f,.21f), .8f, "%s", pAIObj->GetName());
	}

	IRenderAuxGeom *pRC = pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	//	renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
	renderFlags.SetDepthTestFlag( e_DepthTestOff );
	//renderFlags.SetMode2D3DFlag(e_Mode2D);
	//	renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	pRC->SetRenderFlags( renderFlags );
	pRC->DrawSphere(posMarker, .2f, colorMarker);
	//	pRC->DrawCone(posMarker, Vec3(0,0,-1), .2f, .5f,colorMarker);
	pRC->DrawLine(posMarker, colorMarker, posSelf, colorSelf);

	if(pPipeUser->AllowedToFire())
		pRC->DrawCone(posSelf+Vec3(0.f,0.f,.7f), Vec3(0,0,-1), .15f, .5f,ColorB(255,20,20));
	if(pPipeUser->m_State.fire)
		pRC->DrawCone(posSelf+Vec3(0.f,0.f,.7f), pAIObj->GetMoveDir(), .25f, .25f,ColorB(255,20,250));
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawDummy(IRenderer *pRenderer, CAIObject* pAIObj, const char * typeName) const
{
	IAIObject *pAssoc=pAIObj->GetAssociation();

	if (!pAssoc)
		return;

	IEntity *pEntity=pAssoc->GetEntity();
	
	if (!pEntity)
		return;

	IPhysicalEntity *pPhys=pEntity->GetPhysics();

	if (!pPhys)
		return;

	pe_status_dynamics dyn;
	
	if (!pPhys->GetStatus(&dyn))
		return;

	Vec3	posSelf = pAIObj->GetPos();
	Vec3	posParent = pEntity->GetPos();
	Vec3	posLabel = posParent + Vec3(0,0,0.6f);
	Vec3	posParentLabel = posSelf + Vec3(0,0,0.7f);
	Vec3	posPhys= dyn.centerOfMass;
	ColorB colorSelf(20,200,20, 255);
	ColorB colorParent(20,20,200, 255);
	ColorB colorPhys(250,250,30, 255);

	pRenderer->DrawLabel(posLabel, 1.5f, "%s", pEntity->GetName());
	pRenderer->DrawLabel(posParentLabel, 1.5f, "%s", pEntity->GetName());

	IRenderAuxGeom *pRC = pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	//	renderFlags.SetDrawInFrontMode( e_DrawInFrontOn );
	renderFlags.SetDepthTestFlag( e_DepthTestOff );
	//renderFlags.SetMode2D3DFlag(e_Mode2D);
	//renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	pRC->SetRenderFlags( renderFlags );
	pRC->DrawSphere(posSelf, .2f, colorSelf);
	pRC->DrawSphere(posParent, .3f, colorParent);
	pRC->DrawSphere(posPhys, .1f, colorPhys);
	//pRC->DrawCone(posMarker, Vec3(0,0,-1), .2f, .5f,colorMarker);
	//pRC->DrawLine(posMarker, colorMarker, posSelf, colorSelf);
}
//
//-----------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawBadAnchors(IRenderer *pRenderer) const 
{
	if( !m_pVolumeNavRegion )
		return;

	int type=m_cvDrawBadAnchors->GetIVal();

	AIObjects::const_iterator ai;
	if ((ai = m_Objects.find(type)) != m_Objects.end())
		for (;ai!=m_Objects.end(); ++ ai)
		{
			if (ai->first != type)
				break;
			unsigned nodeIndex = m_pVolumeNavRegion->GetEnclosing( (ai->second)->GetPos() );
			GraphNode*	pNode = m_pGraph->GetNodeManager().GetNode(nodeIndex);
			if( !pNode )
			{
				// Bad anchor.
				pRenderer->GetIRenderAuxGeom()->DrawSphere((ai->second)->GetPos(), .4f, ColorF(1,0.7f,0.7f,1));
			}
		}
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawGroups(IRenderer *pRenderer)
{
	if(m_cvDrawGroups->GetIVal()<-1)
		return;
	if(m_cvDrawGroups->GetIVal()>=0)
		return DebugDrawGroup(pRenderer,m_cvDrawGroups->GetIVal());

	short prevId(-1);
	for(AIObjects::const_iterator itr(m_mapGroups.begin()); itr!=m_mapGroups.end(); ++itr)
	{
		if(prevId!=itr->first)
			DebugDrawGroup(pRenderer,itr->first);
		prevId = itr->first;
	}
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawGroup(IRenderer *pRenderer, short grpID)
{
AIObjects::const_iterator itr(m_mapGroups.find(grpID));
	if(itr == m_mapGroups.end())
		return;
	Vec3	groupDrawPos(0,0,0);
	int		groupCount(0);
	bool hasLeader(false);
	for(;itr!=m_mapGroups.end() && itr->first==grpID; ++itr)
	{
		CAIObject *pCurObj( itr->second );
		if(GetLeader(itr->first) == pCurObj)
			continue;

		if(GetLeaderAIObject(pCurObj) == pCurObj)
		{
			groupDrawPos = pCurObj->GetPos();
			hasLeader = true;
			break;
		}
		groupDrawPos += pCurObj->GetPos();
		++groupCount;
	}
	if(groupCount<2)
		return;
	if(!hasLeader)
		groupDrawPos /= groupCount;
	groupDrawPos += Vec3(0,0,5);

	ColorB colorSelf(250,250,25);
	ColorB colorCenter(250,50,250);
	IRenderAuxGeom *pRC = pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	pRC->SetRenderFlags( renderFlags );
	
	for(itr=m_mapGroups.find(grpID); itr!=m_mapGroups.end() && itr->first==grpID; ++itr)
	{
		CAIObject *pCurObj( itr->second );
		if(GetLeader(itr->first) == pCurObj)
			continue;
		pRC->DrawLine(pCurObj->GetPos(), colorSelf, groupDrawPos, colorCenter);
	}
	if(hasLeader)
	{
		pRenderer->GetIRenderAuxGeom()->DrawSphere(groupDrawPos, 1.3f, colorCenter);
	}

	// draw beacon
	CAIObject* pBeacon = (CAIObject*) GetAISystem()->GetBeacon(grpID);
	if(pBeacon)
	{
		Vec3 pos(pBeacon->GetPos());
		ColorF col(0, 1, 1);
		pRenderer->GetIRenderAuxGeom()->DrawSphere(pos,0.35f, col);
		pRenderer->GetIRenderAuxGeom()->DrawLine(pos,col,pos+Vec3(0,0,2), col);
		pos.z+=2;
		pRenderer->GetIRenderAuxGeom()->DrawCone(pos,Vec3Constants<float>::fVec3_OneZ,0.20f, 0.5f,col);
		pos.z+=1;
		pRenderer->DrawLabel(pos,1,"%s", pBeacon->GetName());
	}
}



//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawShooting(IRenderer *pRenderer) const
{
const char *pName(m_cvDrawShooting->GetString());
	if(!pName )
		return;
	CAIObject *pTargetObject = GetAIObjectByName(pName);
	if (!pTargetObject)
		return;
	CPuppet* pTargetPuppet = pTargetObject->CastToCPuppet();
	if (!pTargetPuppet)
		return;

	int col(1), row(1);
	char	buffString[128];

	float colorOn[4]={.1f,1.f,.1f,1.f};
	float colorOff[4]={1.f,.1f,.1f,1.f};

	SAIWeaponInfo	weaponInfo;
	pTargetObject->GetProxy()->QueryWeaponInfo(weaponInfo);
	if(weaponInfo.outOfAmmo)
	{
		sprintf(buffString, ">>> OUT OF AMMO <<<");
		DebugDrawLabel(pRenderer, col, row, buffString, colorOn);
		++row;
	}
	switch(pTargetPuppet->GetAimState())
	{
		case 	AI_AIM_NONE:			// No aiming requested
			sprintf(buffString, "aimState >>> none");
			break;
		case 	AI_AIM_WAITING:			// Aiming requested, but not yet ready.
			sprintf(buffString, "aimState >>> waiting");
			break;
		case 	AI_AIM_OBSTRUCTED:	// Aiming obstructed.
			sprintf(buffString, "aimState >>> OBSTRUCTED");
			break;
		case 	AI_AIM_READY:
			sprintf(buffString, "aimState >>> ready");
			break;
		case 	AI_AIM_FORCED:
			sprintf(buffString, "aimState >>> forced");
			break;
		default:
			sprintf(buffString, "aimState >>> undefined");
	}
	DebugDrawLabel(pRenderer, col, row, buffString, colorOn);
	++row;
	sprintf(buffString, "current accuracy: %.3f", pTargetPuppet->GetAccuracy((CAIObject*)pTargetPuppet->GetAttentionTarget()));
	DebugDrawLabel(pRenderer, col, row, buffString, colorOn);
	++row;
	if( pTargetPuppet->AllowedToFire())
		DebugDrawLabel(pRenderer, col, row, "fireCommand ON", colorOn);
	else
	{
		DebugDrawLabel(pRenderer, col, row, "fireCommand OFF", colorOff);
		return;
	}
	++row;
	if( pTargetPuppet->m_State.fire)
		DebugDrawLabel(pRenderer, col, row, "trigger DOWN", colorOn);
	else
		DebugDrawLabel(pRenderer, col, row, "trigger OFF", colorOff);
	++row;
	if(pTargetPuppet->m_pFireCmdHandler && stricmp("instant_deprecated", pTargetPuppet->m_pFireCmdHandler->GetName())==0 )
	{
		CFireCommandInstantDeprecated *pFireCmd(static_cast<CFireCommandInstantDeprecated*>(pTargetPuppet->m_pFireCmdHandler));
		if(pFireCmd->IsStartingNow())
		{
			sprintf(buffString, "Starting %d time left %.2f", pFireCmd->m_nDrawFireCounter, pFireCmd->m_fFadingToNormalTime);
			DebugDrawLabel(pRenderer, col, row, buffString, colorOff);
		}
		else
			DebugDrawLabel(pRenderer, col, row, "NORMAL fire ", colorOn);
		++row;
		if(pFireCmd->dbg_FrinedsInLine)
			DebugDrawLabel(pRenderer, col, row, "Friend in way", colorOff);
		else
			DebugDrawLabel(pRenderer, col, row, "no friend", colorOn);
		++row;
		if(!pFireCmd->dbg_IsAiming)
			DebugDrawLabel(pRenderer, col, row, "NO AIMING", colorOff);
		else
			DebugDrawLabel(pRenderer, col, row, "aiming ok", colorOn);
		++row;
		if(pFireCmd->dbg_AngleTooDifferent)
		{
			sprintf(buffString, "Angle too different %.2f", pFireCmd->dbg_AngleDiffValue);
			DebugDrawLabel(pRenderer, col, row, buffString, colorOff);
		}
		else
		{
			sprintf(buffString, "Angle good %.2f", pFireCmd->dbg_AngleDiffValue);
			DebugDrawLabel(pRenderer, col, row, buffString, colorOn);
		}
		++row;
		sprintf(buffString, "shots>>>  %d %d %.2f", pFireCmd->dbg_ShotCounter, pFireCmd->dbg_ShotHitCounter,   
											static_cast<float>(pFireCmd->dbg_ShotHitCounter)/static_cast<float>(pFireCmd->dbg_ShotCounter));
		DebugDrawLabel(pRenderer, col, row, buffString, colorOn);
		++row;

pTargetObject->GetProxy()->DebugDraw(pRenderer);
	}
	else
	{
		if(pTargetPuppet->m_pFireCmdHandler)
		{
			sprintf(buffString, "CAN'T HADLE CURRENT WEAPON -- %s", pTargetPuppet->m_pFireCmdHandler->GetName());
			DebugDrawLabel(pRenderer, col, row, buffString, colorOff);
		}
		else
			DebugDrawLabel(pRenderer, col, row, "ZERO WEAPON !!!", colorOff);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawExplosions(IRenderer *pRenderer) const
{
	if(m_cvDrawExplosions->GetIVal()==0)
		return;
	for (unsigned i = 0, ni = m_explosionSpots.size(); i < ni; ++i)
	{
		float a = m_explosionSpots[i].t / m_ExplosionSpotLifeTime;
		ColorB	curColor(155,141,22,10+(uint8)(128*a));
		pRenderer->GetIRenderAuxGeom()->DrawSphere(m_explosionSpots[i].pos, m_explosionSpots[i].radius, curColor);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawObstructSpheres(IRenderer *pRenderer) const
{
	if (m_cvDebugDrawObstrSpheres->GetIVal()==0)
		return;
	for (unsigned i = 0, ni = m_ObstructSpheres.size(); i < ni; ++i)
	{
		const SObstrSphere& sphere = m_ObstructSpheres[i];
		ColorB	curColor(155,141,22,170);
		pRenderer->GetIRenderAuxGeom()->DrawSphere(sphere.pos, sphere.curRadius, curColor);
	}
}



//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawAreas(IRenderer *pRenderer) const
{
	const int ALERT_STANDBY_IN_RANGE = 340;
	const int ALERT_STANDBY_SPOT = 341;
	const int COMBAT_TERRITORY = 342;

	AIObjects::const_iterator end = m_Objects.end();

	Vec3 camPos = GetDebugCameraPos();
	float	statsDist = m_cvAgentStatsDist->GetFVal();

	// Draw standby areas
	for(ShapeMap::const_iterator it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
	{
		const SShape&	shape = it->second;

		if(Distance::Point_AABBSq(camPos, shape.aabb) > sqr(100.0f))
			continue;

		int	a = 255;
		if(!shape.enabled)
			a = 120;

		switch(shape.type)
		{
		case ALERT_STANDBY_IN_RANGE:
			DebugDrawRangePolygon(pRenderer, shape.shape, 0.75f, ColorB(255,128,0,a/2), ColorB(255,128,0,a), true);
			break;
		case COMBAT_TERRITORY:
			DebugDrawRangePolygon(pRenderer, shape.shape, 0.75f, ColorB(40,85,180,a/2), ColorB(40,85,180,a), true);
			break;
		}
	}

	// Draw attack directions.
	for(AIObjects::const_iterator ai = m_Objects.find(ALERT_STANDBY_SPOT); ai != end; ++ai)
	{
		if (ai->first != ALERT_STANDBY_SPOT) break;
		CAIObject* obj = ai->second;

		const float	rad = obj->GetRadius();

		DebugDrawRangeCircle(pRenderer, obj->GetPos() + Vec3(0,0,0.5f), rad, 0.25f, ColorB(255,255,255,120), ColorB(255,255,255,255), true);

		Vec3	dir(obj->GetMoveDir());
		dir.z = 0;
		dir.NormalizeSafe();
		DebugDrawRangeArc(pRenderer, obj->GetPos() + Vec3(0,0,0.5f), dir, DEG2RAD(120.0f), rad - 0.7f, rad/2, ColorB(255,0,0,64), ColorB(255,255,255,255), false);
	}

	AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET);
	for (;ai!=m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;

		CAIObject* obj = ai->second;
		if(!obj->IsEnabled())
			continue;

		if(Distance::Point_PointSq(obj->GetPos(), camPos) > sqr(statsDist))
			continue;

		CPuppet* puppet = obj->CastToCPuppet();
		if (!puppet)
			continue;

		SShape* territoryShape = puppet->GetTerritoryShape();
		if(territoryShape)
		{
			float dist = 0;
			Vec3 nearestPt;
			ListPositions::const_iterator	it = territoryShape->NearestPointOnPath(puppet->GetPos(), dist, nearestPt);
			Vec3	p0 = puppet->GetPos() - Vec3(0,0,0.5f);
			pRenderer->DrawLabel(p0 * 0.7f + nearestPt * 0.3f,1,"Terr");
			pRenderer->GetIRenderAuxGeom()->DrawLine(p0, ColorB(30,208,224), nearestPt, ColorB(30,208,224));		
		}

		SShape* refShape = puppet->GetRefShape();
		if(refShape)
		{
			float dist = 0;
			Vec3 nearestPt;
			ListPositions::const_iterator	it = refShape->NearestPointOnPath(puppet->GetPos(), dist, nearestPt);
			Vec3	p0 = puppet->GetPos() - Vec3(0,0,1.0f);
			pRenderer->DrawLabel(p0 * 0.7f + nearestPt * 0.3f,1,"Ref");
			pRenderer->GetIRenderAuxGeom()->DrawLine(p0, ColorB(135,224,30), nearestPt, ColorB(135,224,30));		
		}
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawExpensiveAccessoryQuota(IRenderer *pRenderer) const
{
	Vec3 cameraPos = GetDebugCameraPos();

	const float analyzeDist = 350;

	for (AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET) ;ai!=m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;
		CAIObject* obj = ai->second;
		if (!obj->IsEnabled())
			continue;
		CPuppet* pPuppet = obj->CastToCPuppet();
		if (!pPuppet)
			continue;
		if (Distance::Point_PointSq(obj->GetPos(), cameraPos) > sqr(analyzeDist))
			continue;

		if (pPuppet->IsAllowedToUseExpensiveAccessory())
		{
			SAIBodyInfo	bodyInfo;
			pPuppet->GetProxy()->QueryBodyInfo(bodyInfo);
			Vec3	pos = pPuppet->GetPhysicsPos();
			AABB	aabb(bodyInfo.stanceSize);
			aabb.Move(pos);
			pRenderer->GetIRenderAuxGeom()->DrawAABB(aabb, true, ColorB(0,196,255,64), eBBD_Faceted);
		}
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::DebugDrawAmbientFire(IRenderer *pRenderer) const
{
	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
	if(!pPlayer)
		return;

	const float analyzeDist = 350;

	for (AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET) ;ai!=m_Objects.end(); ++ai)
	{
		if (ai->first != AIOBJECT_PUPPET)
			break;
		CAIObject* obj = ai->second;
		if (!obj->IsEnabled())
			continue;
		CPuppet* pPuppet = obj->CastToCPuppet();
		if (!pPuppet)
			continue;
		if (Distance::Point_PointSq(obj->GetPos(), pPlayer->GetPos()) > sqr(analyzeDist))
			continue;

		if (pPuppet->IsAllowedToHitTarget())
		{
//			pRenderer->GetIRenderAuxGeom()->DrawCone(obj->GetPhysicsPos(), Vec3(0,0,1), 0.3f, 1.0f, ColorB(255,255,255));
			SAIBodyInfo	bodyInfo;
			pPuppet->GetProxy()->QueryBodyInfo(bodyInfo);
			Vec3	pos = pPuppet->GetPhysicsPos();
			AABB	aabb(bodyInfo.stanceSize);
			aabb.Move(pos);
			pRenderer->GetIRenderAuxGeom()->DrawAABB(aabb, true, ColorB(255,0,0,64), eBBD_Faceted);
		}
	}

}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::AddDebugLine(const Vec3& start, const Vec3& end, uint8 r, uint8 g, uint8 b, float time)
{
	m_vecDebugLines.push_back(SDebugLine( start, end, ColorB(r, g, b), time));
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::AddDebugBox(const Vec3& pos, const OBB& obb, uint8 r, uint8 g, uint8 b, float time)
{
	m_vecDebugBoxes.push_back(SDebugBox(pos, obb, ColorB(r, g, b), time));
}

//-----------------------------------------------------------------------------------------------------------
void CAISystem::AddDebugSphere(const Vec3& pos, float radius, uint8 r, uint8 g, uint8 b, float time)
{
	m_vecDebugSpheres.push_back(SDebugSphere(pos, radius, ColorB(r, g, b), time));
}
