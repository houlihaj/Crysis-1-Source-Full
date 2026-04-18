/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   CAISystem.cpp
$Id$
Description: all the update related functionality here

-------------------------------------------------------------------------
History:
- 2007				: Created by Kirill Bulatsev

*********************************************************************/





#include "StdAfx.h"

#include <stdio.h>

#include <limits>
#include <map>
#include <numeric>
#include <algorithm>

#include "IPhysics.h"
#include "IEntitySystem.h"
#include "IScriptSystem.h"
#include "I3DEngine.h"
#include "ILog.h"
#include "CryFile.h"
#include "Cry_Math.h"
#include "ISystem.h"
#include "IEntityRenderState.h"
#include "IRenderer.h"
#include "ITimer.h"
#include "IConsole.h"
#include "ISerialize.h"
//#include "ICryPak.h"
#include "IAgent.h"
#include "VectorSet.h"

#include "CAISystem.h"
#include "CryAISystem.h"
#include "AILog.h"
#include "CTriangulator.h"
#include "TriangularNavRegion.h"
#include "WaypointHumanNavRegion.h"
#include "Waypoint3DSurfaceNavRegion.h"
#include "VolumeNavRegion.h"
#include "FlightNavRegion.h"
#include "RoadNavRegion.h"
#include "SmartObjectNavRegion.h"
#include "Free2DNavRegion.h"
#include "Graph.h"
#include "AStarSolver.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "GoalOp.h"
#include "AIPlayer.h"
#include "PipeUser.h"
#include "AIAutoBalance.h"
#include "Leader.h"
#include "ObjectTracker.h"
#include "VehicleFinder.h"
#include "SmartObjects.h"
#include "AIActions.h"
#include "AICollision.h"
#include "TickCounter.h"
#include "AIRadialOcclusion.h"
#include "GraphNodeManager.h"
#include "CentralInterestManager.h"



//====================================================================
// DoesRegionAContainRegionB
// assume that if all points from one region are in the other then
// that's sufficient - only true if regions are convex
//====================================================================
bool DoesRegionAContainRegionB(const CGraph::CRegion & regionA, const CGraph::CRegion & regionB)
{
	for (std::list<Vec3>::const_iterator it = regionB.m_outline.begin() ; it != regionB.m_outline.end() ; ++it)
	{
		if (!Overlap::Point_Polygon2D(*it, regionA.m_outline))
			return false;
	}
	return true;
}


//====================================================================
// CombineRegions
//====================================================================
static void CombineRegions(CGraph::CRegions & regions)
{
	CGraph::CRegions origRegions = regions;
	regions.m_regions.clear();

	for (std::list<CGraph::CRegion>::const_iterator it = origRegions.m_regions.begin() ; 
		it != origRegions.m_regions.end() ; 
		++it)
	{
		// skip *it if it's completely contained in any of regions
		bool skip = false;
		for (std::list<CGraph::CRegion>::const_iterator existingIt = regions.m_regions.begin() ; 
			existingIt != regions.m_regions.end() ; 
			++existingIt)
		{
			if (DoesRegionAContainRegionB(*existingIt, *it))
			{
				skip = true;
				break;
			}
		}
		if (skip)
			continue;

		// remove any of regions that are contained in *it
		for (std::list<CGraph::CRegion>::iterator existingIt = regions.m_regions.begin() ; 
			existingIt != regions.m_regions.end() ; )
		{
			if (DoesRegionAContainRegionB(*it, *existingIt))
				existingIt = regions.m_regions.erase(existingIt);
			else
				++existingIt;
		}
		// now add
		regions.m_regions.push_front(*it);
	}

	// update the bounding box;
	regions.m_AABB.Reset();
	for (std::list<CGraph::CRegion>::const_iterator it = regions.m_regions.begin() ; 
		it != regions.m_regions.end() ; 
		++it)
	{
		regions.m_AABB.Add(it->m_AABB);
	}
}





//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::TakeDetectionSnapshot()
{
	// Reset detection on player and squadmates
	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
	if (pPlayer)
	{
		pPlayer->SnapshotDetectionLevels();
		pPlayer->ResetDetectionLevels();

		int groupid = pPlayer->GetGroupId();
		AIObjects::iterator itSquadMates = m_mapGroups.find(groupid);
		while (itSquadMates != m_mapGroups.end() && itSquadMates->first == groupid)
		{
			CAIObject* pObject = itSquadMates->second;
			++itSquadMates;

			CPuppet* pPuppet = pObject->CastToCPuppet();
			if (pPuppet)
			{
				pPuppet->SnapshotDetectionLevels();
				pPuppet->ResetDetectionLevels();
			}
		}
	}
}

//
//-----------------------------------------------------------------------------------------------------------
static bool IsPuppetOnScreen(CPuppet* pPuppet)
{
	IEntity* pEntity = pPuppet->GetEntity();
	if (!pEntity)
		return false;
	IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
	if (!pRenderProxy || !pRenderProxy->GetRenderNode())
		return false;
	int frameDiff = gEnv->pRenderer->GetFrameID() - pRenderProxy->GetRenderNode()->GetDrawFrame();
	if (frameDiff > 2)
		return false;
	return true;
}

//
//-----------------------------------------------------------------------------------------------------------
static EPuppetUpdatePriority CalcPuppetUpdatePriority(CPuppet* pPuppet, const Vec3& camPos)
{
	// Calculate the update priority of the puppet.
	float distToCameraSq = Distance::Point_PointSq(camPos, pPuppet->GetPos());
	if (IsPuppetOnScreen(pPuppet))
	{
		if (distToCameraSq < sqr(pPuppet->GetSightRange(0)))
			return AIPUP_VERY_HIGH;
		else
			return AIPUP_HIGH;
	}
	else
	{
		if (distToCameraSq < sqr(pPuppet->GetSightRange(0)))
			return AIPUP_MED;
		else
			return AIPUP_LOW;
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::Update(CTimeValue frameStartTime, float frameDeltaTime)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	static CTimeValue lastFrameStartTime;
	if ( frameStartTime == lastFrameStartTime )
		return;
	lastFrameStartTime = frameStartTime;


	if (!m_bInitialized || !IsEnabled())
		return;

	if (!m_cvAiSystem->GetIVal()) 
		return;

//	for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
//		it->second->Validate();

	// Call this every frame to be sure it gets started!
	if (!GetISystem()->IsEditor()) m_Recorder.Start();

	{
		FRAME_PROFILER("AIUpdate 1", m_pSystem, PROFILE_AI)
			m_pProfileTickCounter->Update((frameStartTime - m_fFrameStartTime).GetSeconds());

		if ( !m_pSmartObjects )
			InitSmartObjects();

		if ( m_pActionManager )
			m_pActionManager->Update();

		ClearBadFloorRecords();

		m_fFrameStartTime = frameStartTime;
		m_fFrameDeltaTime = frameDeltaTime;

		CAIRadialOcclusionRaycast::UpdateActiveCount();

		m_lightManager.Update(false);

		if (m_navDataState == NDS_BAD)
		{
			static CTimeValue lastTime = frameStartTime;
			if ((frameStartTime - lastTime).GetSeconds() > 5.0f)
			{
				AIWarning("*** AI navigation is bad. Please regenerate. AI SYSTEM IS NOT UPDATED");
				lastTime = frameStartTime;
			}
			return;
		}
		else if (m_navDataState != NDS_OK)
		{
			return;
		}

		IConsole *pConsole = m_pSystem->GetIConsole();
		CCalculationStopper::m_useCounter = m_cvUseCalculationStopperCounter->GetIVal() != 0;

		static CTimeValue lastLogFlushTime = frameStartTime;
		if ((frameStartTime - lastLogFlushTime).GetSeconds() > 1.0f)
		{
			m_DbgRecorder.FlushLog();
			lastLogFlushTime = frameStartTime;
		}

		for (unsigned i = 0; i < m_ObstructSpheres.size(); )
		{
			if (!m_ObstructSpheres[i].Update(GetFrameDeltaTime()))
			{
				m_ObstructSpheres[i] = m_ObstructSpheres.back();
				m_ObstructSpheres.pop_back();
			}
			else
				++i;
		}

		if (!m_pendingBrokenRegions.m_regions.empty())
		{
			if (m_pGraph)
			{
				InvalidatePathsThroughPendingBrokenRegions();
				m_brokenRegions.m_regions.splice(m_brokenRegions.m_regions.begin(), m_pendingBrokenRegions.m_regions);
				CombineRegions(m_brokenRegions);
				m_pendingBrokenRegions.m_regions.clear(); // should be done in splice...
				m_pGraph->RestoreBrokenLinks();
				m_pGraph->DisableNavigationInBrokenRegions(m_brokenRegions);
				// zap the current request - get it to restart
				RescheduleCurrentPathfindRequest();
			}
		}

		UpdateNavRegions();

		// update all the explosion spots, remove expired ones
		for (unsigned i = 0; i < m_explosionSpots.size(); )
		{
			// decrease timeout
			m_explosionSpots[i].t -= m_fFrameDeltaTime;
			if (m_explosionSpots[i].t > 0.0f) 
			{
				++i;
				continue;
			}
			m_explosionSpots[i] = m_explosionSpots.back();
			m_explosionSpots.pop_back();
		}

		// Update all the dead bodies, remove expired ones
		for (unsigned i = 0; i < m_deadBodies.size(); )
		{
			// decrease timeout
			m_deadBodies[i].t -= m_fFrameDeltaTime;
			if (m_deadBodies[i].t > 0.0f) 
			{
				++i;
				continue;
			}
			m_deadBodies[i] = m_deadBodies.back();
			m_deadBodies.pop_back();
		}

		// Update banned SOs
		SmartObjectFloatMap::iterator it = m_bannedNavSmartObjects.begin();
		while (it != m_bannedNavSmartObjects.end())
		{
			it->second -= m_fFrameDeltaTime;
			if (it->second <= 0.0f)
				it = m_bannedNavSmartObjects.erase(it);
			else
				++it;
		}
	}

	CheckVisibilityBodies();
	UpdateAmbientFire();
	UpdateExpensiveAccessoryQuota();

	// Collect possible visible pairs.
	VisCheckBroadPhase();

	{
		FRAME_PROFILER("AIUpdate 2", m_pSystem, PROFILE_AI)

			// update player
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
			if (pPlayer)
				pPlayer->Update(AIUPDATE_FULL);

			// Autobalancing timer
			if (m_fAutoBalanceCollectingTimeout < 1.f)
				m_fAutoBalanceCollectingTimeout+=m_fFrameDeltaTime;
			else
				m_bCollectingAllowed = false;

			// trace any paths that were requested
			UpdatePathFinder();

			{
				FRAME_PROFILER("AIUpdate groups", m_pSystem, PROFILE_AI)
				const float dt = (frameStartTime - m_lastGroupUpdateTime).GetSeconds();
				if (dt > m_cvAIUpdateInterval->GetFVal())
				{
					for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
						it->second->Update();
					m_lastGroupUpdateTime = frameStartTime;
				}
			}
			UpdateAuxSignalsMap();
	}

	{
		FRAME_PROFILER("AIUpdate 3", m_pSystem, PROFILE_AI)

		if (!m_enabledPuppetsSet.empty())
		{
			unsigned activePuppetCount = m_enabledPuppetsSet.size();
			const float updateInterval = m_cvAIUpdateInterval->GetFVal();
			const float updatesPerSecond = (activePuppetCount / updateInterval) + m_enabledPuppetsUpdateError;
			unsigned puppetUpdateCount = (unsigned)floorf(updatesPerSecond * m_fFrameDeltaTime);
			if (m_fFrameDeltaTime > 0.0f)
				m_enabledPuppetsUpdateError = updatesPerSecond - puppetUpdateCount / m_fFrameDeltaTime;

			// Collect list of enabled priority targets (players, grenades, projectiles, etc).
			static std::vector<CAIObject*> priorityTargets;
			priorityTargets.resize(0);
			for (unsigned i = 0, ni = m_priorityObjectTypes.size(); i < ni; ++i)
			{
				short type = m_priorityObjectTypes[i];
				AIObjects::iterator ai = m_Objects.find(type);
				for ( ; ai != m_Objects.end() && ai->first == type; ++ai)
				{
					CAIObject* pTarget = ai->second;
					if (!pTarget->IsEnabled())
						continue;
					priorityTargets.push_back(pTarget);
				}
			}

			// Reset per update stats.
			m_visChecks = 0;
			m_visChecksRays = 0;


			int64 fst(m_fFrameStartTime.GetMilliSecondsAsInt64());
			bool bRunOutOfRays = false;
			unsigned fullUpdates = 0;
			m_enabledPuppetsUpdateHead %= activePuppetCount;
			int idx = m_enabledPuppetsUpdateHead;

			Vec3 camPos = gEnv->pSystem->GetViewCamera().GetPosition();

			m_nRaysThisUpdateFrame = 0;

			// Note: cannot cache the set size, since a puppet may be removed from the list as part of the update.
			for (unsigned i = 0; i < m_enabledPuppetsSet.size(); ++i)
			{
				CPuppet *pPuppet = m_enabledPuppetsSet[idx];
				idx++;
				if (idx >= m_enabledPuppetsSet.size())
					idx = 0;

				if (fullUpdates < puppetUpdateCount && !bRunOutOfRays)
				{
					pPuppet->SetUpdatePriority(CalcPuppetUpdatePriority(pPuppet, camPos));

					// Full update
					bRunOutOfRays = SingleFullUpdate2(pPuppet, priorityTargets);

					// TODO/NOTE: This should be inside the SingleFullUpdate, but since there
					// are 2 versions currently, it is here for the time being. [Mikko]
					ITimer *pTimer = m_pSystem->GetITimer();
					CTimeValue timeLast;
					if (m_cvAllTime->GetIVal())
						timeLast = pTimer->GetAsyncTime();

					pPuppet->Update(AIUPDATE_FULL);

					if (m_cvAllTime->GetIVal())
					{
						FRAME_PROFILER( "AISFU:TimeMeasuringBlock", m_pSystem,PROFILE_AI);
						CTimeValue timeCurr = pTimer->GetAsyncTime();
						float fTime = (timeCurr-timeLast).GetSeconds();
						timeLast=timeCurr;

						TimingMap::iterator ti = m_mapDEBUGTiming.find(pPuppet->GetName());
						if (ti!=m_mapDEBUGTiming.end())
							m_mapDEBUGTiming.erase(ti);
						m_mapDEBUGTiming.insert(TimingMap::iterator::value_type(pPuppet->GetName(),fTime));
					}

					if (!pPuppet->m_bUpdatedOnce && m_bUpdateSmartObjects && pPuppet->IsEnabled() &&
						(!pPuppet->GetCurrentGoalPipe() || !pPuppet->GetCurrentGoalPipe()->GetSubpipe()))
						pPuppet->m_bUpdatedOnce = true;

					m_totalPuppetsUpdateCount++;

					if (m_totalPuppetsUpdateCount >= activePuppetCount)
					{
						// full update cycle finished on all ai objects
						// now allow updating smart objects
						m_bUpdateSmartObjects = true;
						// Take snapshot of the perception values when all puppets have been updated.
						TakeDetectionSnapshot();
						m_totalPuppetsUpdateCount = 0;
					}

					fullUpdates++;
				}
				else
				{
					// Dry update
					SingleDryUpdate(pPuppet);
				}
			}

			// Advance update head.
			m_enabledPuppetsUpdateHead += fullUpdates;

			// Collect per update stats
			{
				m_visChecksHistory[m_visChecksHistoryHead] = m_visChecks;
				m_visChecksRaysHistory[m_visChecksHistoryHead] = m_visChecksRays;
				m_visChecksUpdatesHistory[m_visChecksHistoryHead] = fullUpdates;

				m_visChecksHistoryHead = (m_visChecksHistoryHead + 1) % VIS_CHECK_HISTORY_SIZE;

				if (m_visChecksHistorySize < VIS_CHECK_HISTORY_SIZE)
					m_visChecksHistorySize++;

				m_visChecksMax = max(m_visChecks, m_visChecksMax);
				m_visChecksRaysMax = max(m_visChecksRays, m_visChecksRaysMax);
			}
		}
		else
		{
			// No active puppets, make sure the detection gets still updated.
			TakeDetectionSnapshot();
		}

		// Update disabled
		if (!m_disabledPuppetsSet.empty())
		{
			unsigned inactivePuppetCount = m_disabledPuppetsSet.size();
			const float updateInterval = 0.3f;
			const float updatesPerSecond = (inactivePuppetCount / updateInterval) + m_disabledPuppetsUpdateError;
			unsigned puppetDisabledUpdateCount = (unsigned)floorf(updatesPerSecond * m_fFrameDeltaTime);
			if (m_fFrameDeltaTime > 0.0f)
				m_disabledPuppetsUpdateError = updatesPerSecond - puppetDisabledUpdateCount / m_fFrameDeltaTime;

			m_disabledPuppetsHead %= inactivePuppetCount;
			int idx = m_disabledPuppetsHead;

			for (unsigned i = 0; i < puppetDisabledUpdateCount; ++i)
			{
				CPuppet *pPuppet = m_disabledPuppetsSet[idx];
				idx++;
				if (idx >= m_disabledPuppetsSet.size())
					idx = 0;
				pPuppet->UpdateDisabled(AIUPDATE_FULL);
			}

			// Advance update head.
			m_disabledPuppetsHead += puppetDisabledUpdateCount;
		}

		//
		//	update all leaders here (should be not every update (full update only))
		const static float leaderUpdateRate(.2f);
		static float leaderNoUpdatedTime(.0f);
		leaderNoUpdatedTime += m_fFrameDeltaTime;
		if(leaderNoUpdatedTime > leaderUpdateRate)
		{
			leaderNoUpdatedTime = .0f;
			AIObjects::iterator aio = m_Objects.find(AIOBJECT_LEADER);
			for (;aio!=m_Objects.end();++aio)
			{
				if (aio->first != AIOBJECT_LEADER)
					break;
				CLeader* pLeader = aio->second->CastToCLeader();
				if (pLeader)
					pLeader->Update(AIUPDATE_FULL);
			}
		}
		// leaders update over

		// Vehicle finder update
		if(m_pVehicleFinder)
			m_pVehicleFinder->Update();

		UpdatePlayerPerception();
		UpdateGlobalAlertness();

		if ( m_bUpdateSmartObjects )
			m_pSmartObjects->Update();

		//	fLastUpdateTime = currTime;
		++m_nTickCount;

		// Update path devalues.
		for(ShapeMap::iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
			it->second.devalueTime = max(0.0f, it->second.devalueTime - m_fFrameDeltaTime);

		// Update sound events
		for(unsigned i = 0; i < m_soundEvents.size();)
		{
			SAISoundEvent& se = m_soundEvents[i];
			se.t += m_fFrameDeltaTime;
			if (se.t > se.tmax)
			{
				m_soundEvents[i] = m_soundEvents.back();
				m_soundEvents.pop_back();
			}
			else
				++i;
		}

		// Update grenade events
		for(unsigned i = 0; i < m_grenadeEvents.size();)
		{
			SAIGrenadeEvent& ge = m_grenadeEvents[i];
			ge.t += m_fFrameDeltaTime;
			if (ge.t > ge.tmax)
			{
				m_grenadeEvents[i] = m_grenadeEvents.back();
				m_grenadeEvents.pop_back();
			}
			else
				++i;
		}

		UpdateCollisionEvents();
		UpdateBulletEvents();

		UpdateDebugStuff();

		// Update interest system
		ICentralInterestManager * pInterestManager = CCentralInterestManager::GetInstance();
		pInterestManager->Enable( m_cvInterestSystem->GetIVal()!=0 );
		pInterestManager->Update(frameDeltaTime); // May return immediately

	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::VisCheckBroadPhase()
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	float dt = (m_fFrameStartTime - m_lastVisBroadPhaseTime).GetSeconds();
	if (dt < 0.2f)
		return;
	m_lastVisBroadPhaseTime = m_fFrameStartTime;

	if (m_enabledPuppetsSet.empty())
		return;

	// Find player vehicles (driver inside but disabled).
	static std::vector<CAIVehicle*>	playerVehicles;
	playerVehicles.resize(0);

	const AIObjects::iterator vehicleBegin = m_Objects.find(AIOBJECT_VEHICLE);
	const AIObjects::iterator aiEnd = m_Objects.end();
	for (AIObjects::iterator ai = vehicleBegin; ai != aiEnd && ai->first == AIOBJECT_VEHICLE ; ++ai)
	{
		CAIVehicle* pVehicle = (CAIVehicle*)ai->second;
		if (!pVehicle->IsEnabled() && pVehicle->IsDriverInside())
			playerVehicles.push_back(pVehicle);
	}

	// Clear target lists.
	for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
		m_enabledPuppetsSet[i]->ClearProbableTargets();

	// Find potential targets.
	for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
	{
		CPuppet* pFirst = m_enabledPuppetsSet[i];
		const Vec3& firstPos = pFirst->GetPos();

		// Check against other AIs
		for (unsigned j = i+1; j < ni; ++j)
		{
			CPuppet* pSecond = m_enabledPuppetsSet[j];

			// Skip non-hostile.
			if (!pFirst->IsHostile(pSecond))
				continue;

			const Vec3& secondPos = pSecond->GetPos();

			float distSq = Distance::Point_PointSq(firstPos, secondPos);
			if (distSq < sqr(pFirst->GetSightRange(pSecond)))
				pFirst->AddProbableTarget(pSecond);
			if (distSq < sqr(pSecond->GetSightRange(pFirst)))
				pSecond->AddProbableTarget(pFirst);
		}

		// Check against player vehicles.
		for (unsigned j = 0, nj = playerVehicles.size(); j < nj; ++j)
		{
			CAIVehicle* pSecond = playerVehicles[j];

			// Skip non-hostile.
			if (!pFirst->IsHostile(pSecond))
				continue;

			const Vec3& secondPos = pSecond->GetPos();

			float distSq = Distance::Point_PointSq(firstPos, secondPos);
			if (distSq < sqr(pFirst->GetSightRange(pSecond)))
				pFirst->AddProbableTarget(pSecond);
//			if (distSq < sqr(pSecond->GetSightRange(pFirst)))
//				pSecond->AddProbableTarget(pFirst);
		}

	}


/*	// Clear target lists.
	for (AIObjects::iterator ai = m_mapSpecies.begin(), aiend = m_mapSpecies.end(); ai != aiend; ++ai)
	{
		CAIActor* pActor = (CAIActor*)ai->second;
		if (pActor->IsActive())
			pActor->ClearProbableTargets();
	}

	AIObjects::iterator aiend = m_mapSpecies.end();
	for (AIObjects::iterator ai = m_mapSpecies.begin(); ai != aiend; ++ai)
	{
		CAIActor* pActor = (CAIActor*)ai->second;
		if (!pActor->IsActive())
			continue;

		// Visit only unvisited pairs.
		AIObjects::iterator aio = ai;
		++aio;
		for ( ; aio != aiend; ++aio)
		{
			// Skip same species
			if (ai->first == aio->first)
				continue;
			// Skip inactive
			CAIActor* pOtherActor = (CAIActor*)aio->second;
			if (!pOtherActor->IsActive())
				continue;

			float distSq = Distance::Point_PointSq(pActor->GetPos(), pOtherActor->GetPos());
			if (distSq < sqr(pActor->GetSightRange(pOtherActor)))
				pActor->AddProbableTarget(pOtherActor);
			if (distSq < sqr(pOtherActor->GetSightRange(pActor)))
				pOtherActor->AddProbableTarget(pActor);
		}
	}*/


/*
	const AIObjects::iterator puppetBegin = m_Objects.find(AIOBJECT_PUPPET);
	const AIObjects::iterator vehicleBegin = m_Objects.find(AIOBJECT_VEHICLE);
	const AIObjects::iterator aiEnd = m_Objects.end();

	// Clear target lists.
	for (AIObjects::iterator ai = puppetBegin; ai != aiEnd && ai->first == AIOBJECT_PUPPET ; ++ai)
	{
		CPuppet* pPuppet = (CPuppet*)ai->second;
		pPuppet->ClearProbableTargets();
	}
	for (AIObjects::iterator ai = vehicleBegin; ai != aiEnd && ai->first == AIOBJECT_VEHICLE ; ++ai)
	{
		CPuppet* pPuppet = (CPuppet*)ai->second;
		pPuppet->ClearProbableTargets();
	}

	// Check puppets first.
	for (AIObjects::iterator ai = puppetBegin; ai != aiEnd && ai->first == AIOBJECT_PUPPET ; ++ai)
	{
		CPuppet* pPuppet = (CPuppet*)ai->second;
		if (!pPuppet->IsEnabled())
			continue;

		// Visit only unvisited pairs.
		AIObjects::iterator aio = ai;
		++aio;
		for ( ; aio != aiEnd && aio->first == AIOBJECT_PUPPET ; ++aio)
		{
			CPuppet* pOtherPuppet = (CPuppet*)aio->second;
			if (!pOtherPuppet->IsEnabled() || !pPuppet->IsHostile(pOtherPuppet))
				continue;

			float distSq = Distance::Point_PointSq(pPuppet->GetPos(), pOtherPuppet->GetPos());
			if (distSq < sqr(pPuppet->GetSightRange(pOtherPuppet)))
				pPuppet->AddProbableTarget(pOtherPuppet);
			if (distSq < sqr(pOtherPuppet->GetSightRange(pPuppet)))
				pOtherPuppet->AddProbableTarget(pPuppet);
		}

		for (AIObjects::iterator aiv = vehicleBegin; aiv != aiEnd && aiv->first == AIOBJECT_VEHICLE ; ++aiv)
		{
			CAIVehicle* pOtherVehicle = (CAIVehicle*)aiv->second;

			if (!pPuppet->IsHostile(pOtherVehicle) || !pOtherVehicle->IsDriverInside())
				continue;

			float distSq = Distance::Point_PointSq(pPuppet->GetPos(), pOtherVehicle->GetPos());
			if (distSq < sqr(pPuppet->GetSightRange(pOtherVehicle)))
				pPuppet->AddProbableTarget(pOtherVehicle);
			if (distSq < sqr(pOtherVehicle->GetSightRange(pPuppet)))
				pOtherVehicle->AddProbableTarget(pPuppet);
		}

	}

	// Vehicles
	for (AIObjects::iterator ai = vehicleBegin; ai != aiEnd && ai->first == AIOBJECT_VEHICLE ; ++ai)
	{
		CAIVehicle* pVehicle = (CAIVehicle*)ai->second;
		if (!pVehicle->IsDriverInside())
			continue;

		// Visit only unvisited pairs.
		AIObjects::iterator aio = ai;
		++aio;
		for ( ; aio != aiEnd && aio->first == AIOBJECT_PUPPET ; ++aio)
		{
			CAIVehicle* pOtherVehicle = (CAIVehicle*)aio->second;

			if (!pVehicle->IsHostile(pOtherVehicle) || !pOtherVehicle->IsDriverInside())
				continue;

			float distSq = Distance::Point_PointSq(pVehicle->GetPos(), pOtherVehicle->GetPos());
			if (distSq < sqr(pVehicle->GetSightRange(pOtherVehicle)))
				pVehicle->AddProbableTarget(pOtherVehicle);
			if (distSq < sqr(pOtherVehicle->GetSightRange(pVehicle)))
				pOtherVehicle->AddProbableTarget(pVehicle);
		}
	}
*/

}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateCollisionEvents()
{
	// Update collision events
	for(unsigned i = 0; i < m_collisionEvents.size();)
	{
		SAICollisionEvent& ce = m_collisionEvents[i];
		ce.t += m_fFrameDeltaTime;

		// Check if to be ignored
		bool ignored = m_ignoreCollisionEventsFrom.find( ce.id ) != m_ignoreCollisionEventsFrom.end();

		if (!ignored && !ce.signalled && ce.t > 0.06f)
		{
			if (ce.type == AICO_SMALL)
			{
				SoundEvent(ce.pos, ce.soundRadius, AISE_COLLISION, NULL);
			}
			else
			{
				SoundEvent(ce.pos, ce.soundRadius, AISE_COLLISION_LOUD, NULL);

				// Allow to react to larger objects which collide nearby.
				for (unsigned j = 0, nj = m_enabledPuppetsSet.size(); j < nj; ++j)
				{
					CPuppet* pReceiverPuppet = m_enabledPuppetsSet[j];
					EntityId id = pReceiverPuppet->GetEntity()->GetId();
					if (ce.processed.find(id) != ce.processed.end()) continue;
					ce.processed.insert(id);

					const float scale = pReceiverPuppet->GetParameters().m_PerceptionParams.collisionReactionScale;
					const float rangeSq = sqr(ce.affectRadius * scale);

					float distSq = Distance::Point_PointSq(ce.pos, pReceiverPuppet->GetPos());
					if (distSq > rangeSq) continue;

					IAISignalExtraData* pData = CreateSignalExtraData();
					pData->iValue = ce.thrownByPlayer ? 1 : 0;
					pData->fValue = sqrtf(distSq);
					pData->point = ce.pos;
					pReceiverPuppet->SetSignal(0, "OnCloseCollision", 0, pData);

					if (ce.thrownByPlayer)
						pReceiverPuppet->SetAlarmed();
				}
			}	

			m_collisionEvents[i].signalled = true;
		}

		if (ignored || ce.t > ce.tmax)
		{
			m_collisionEvents[i] = m_collisionEvents.back();
			m_collisionEvents.pop_back();
		}
		else
			++i;
	}

	MapIgnoreCollisionsFrom::iterator it = m_ignoreCollisionEventsFrom.begin();
	while (it != m_ignoreCollisionEventsFrom.end())
	{
		it->second -= m_fFrameDeltaTime;
		if (it->second <= 0.0f)
			it = m_ignoreCollisionEventsFrom.erase(it);
		else
			++it;
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateBulletEvents()
{
	// Send bullet events
	for (unsigned i = 0, ni = m_enabledPuppetsSet.size(); i < ni; ++i)
	{
		CPuppet* pReceiverPuppet = m_enabledPuppetsSet[i];

		AABB bounds;
		pReceiverPuppet->GetEntity()->GetWorldBounds(bounds);

		for (unsigned j = 0, nj = m_bulletEvents.size(); j < nj; ++j)
		{
			SAIBulletEvent& be = m_bulletEvents[j];
			if (be.signalled) continue;

			// Skip own fire.
			if (be.shooterId == pReceiverPuppet->GetEntityID())
				continue;

			float d = Distance::Point_AABBSq(be.pos, bounds);
			float radius2 = sqr(be.radius);
			float bulletHitRadius2 = pReceiverPuppet->m_Parameters.m_PerceptionParams.bulletHitRadius;
			bulletHitRadius2 *= bulletHitRadius2;

			if (d < radius2 || d < bulletHitRadius2)
			{
				IAISignalExtraData* pData = CreateSignalExtraData();
				pData->point = be.pos;
				pData->nID = be.shooterId;
				IEntity* pEnt = gEnv->pEntitySystem->GetEntity(be.shooterId);
				if (pEnt)
				{
					// Skip friendly fire.
					if (pEnt->GetAI())
					{
						if (!pReceiverPuppet->IsHostile(pEnt->GetAI()))
							continue;
					}
				}
				else
				{
					// The OnBulletRain assumes that the caller is valid.
					pEnt = pReceiverPuppet->GetEntity();
				}
				if(d<radius2) // always give priority to OnBulletRain
					pReceiverPuppet->SetSignal(0, "OnBulletRain", pEnt, pData,g_crcSignals.m_nOnBulletRain);
				else if(d < bulletHitRadius2)
					pReceiverPuppet->SetSignal(0, "OnBulletHit", pEnt, pData,g_crcSignals.m_nOnBulletHit);

				pReceiverPuppet->SetAlarmed();

				break;	// One on bullet rain per update is enough.
			}
		}
	}

	// Update bullet events
	for (unsigned i = 0; i < m_bulletEvents.size(); )
	{
		SAIBulletEvent& be = m_bulletEvents[i];
		be.t += m_fFrameDeltaTime;
		m_bulletEvents[i].signalled = true;
		if (be.t > be.tmax)
		{
			m_bulletEvents[i] = m_bulletEvents.back();
			m_bulletEvents.pop_back();
		}
		else
			++i;
	}

}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateDebugStuff()
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );
	// Delete the debug lines if the debug draw is not on.
	if((m_cvDebugDraw->GetIVal()==0))
	{
		m_vecDebugLines.clear();
		m_vecDebugBoxes.clear();
	}

	// Update fake tracers
	if(m_cvDrawFakeTracers->GetIVal() > 0)
	{
		for(size_t i = 0; i < m_DEBUG_fakeTracers.size();)
		{
			m_DEBUG_fakeTracers[i].t -= m_fFrameDeltaTime;
			if(m_DEBUG_fakeTracers[i].t < 0.0f)
			{
				m_DEBUG_fakeTracers[i] = m_DEBUG_fakeTracers.back();
				m_DEBUG_fakeTracers.pop_back();
			}
			else
			{
				++i;
			}
		}
	}
	else
	{
		m_DEBUG_fakeTracers.clear();
	}

	// Update fake hit effects
	if(m_cvDrawFakeHitEffects->GetIVal() > 0)
	{
		for(size_t i = 0; i < m_DEBUG_fakeHitEffect.size();)
		{
			m_DEBUG_fakeHitEffect[i].t -= m_fFrameDeltaTime;
			if(m_DEBUG_fakeHitEffect[i].t < 0.0f)
			{
				m_DEBUG_fakeHitEffect[i] = m_DEBUG_fakeHitEffect.back();
				m_DEBUG_fakeHitEffect.pop_back();
			}
			else
			{
				++i;
			}
		}
	}
	else
	{
		m_DEBUG_fakeHitEffect.clear();
	}

	// Update fake damage indicators
	if(m_cvDrawFakeDamageInd->GetIVal() > 0)
	{
		for(unsigned i = 0; i < m_DEBUG_fakeDamageInd.size();)
		{
			m_DEBUG_fakeDamageInd[i].t -= m_fFrameDeltaTime;
			if(m_DEBUG_fakeDamageInd[i].t < 0)
			{
				m_DEBUG_fakeDamageInd[i] = m_DEBUG_fakeDamageInd.back();
				m_DEBUG_fakeDamageInd.pop_back();
			}
			else
				++i;
		}
		m_DEBUG_screenFlash = max(0.0f, m_DEBUG_screenFlash - m_fFrameDeltaTime);
	}
	else
	{
		m_DEBUG_fakeDamageInd.clear();
		m_DEBUG_screenFlash = 0.0f;
	}
}
//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateNavRegions()
{
//return;
	if (m_cvDynamicWaypointUpdateTime->GetFVal() > 0.000001f && m_pWaypointHumanNavRegion)
	{
		CCalculationStopper stopper("DynamicWaypoint", m_cvDynamicWaypointUpdateTime->GetFVal(), 60000);
		m_pWaypointHumanNavRegion->Update(stopper);
	}

	if (m_cvDynamicTriangularUpdateTime->GetFVal() > 0.000001f && m_pTriangularNavRegion)
	{
		CCalculationStopper stopper("DynamicTrianglular", m_cvDynamicTriangularUpdateTime->GetFVal(), 10000); // guess
		m_pTriangularNavRegion->Update(stopper);
	}

	if (m_cvDynamicVolumeUpdateTime->GetFVal() > 0.000001f && m_pVolumeNavRegion)
	{
		CCalculationStopper stopper("DynamicVolume", m_cvDynamicVolumeUpdateTime->GetFVal(), 10000); // guess
		m_pVolumeNavRegion->Update(stopper);
	}
}


//===================================================================
// GetUpdateAllAlways
//===================================================================
bool CAISystem::GetUpdateAllAlways() const
{
	bool updateAllAlways = m_cvUpdateAllAlways->GetIVal() != 0;
	return updateAllAlways;
}


// Helper structure to sort indices pointing to an array of weights.
/*struct SAmbientFireSorter
{
SDamageLUTSorter(const std::vector<float> &values) : m_values(values) {}
bool operator()(int lhs, int rhs) const { return m_values[lhs] < m_values[rhs]; }
const std::vector<float>&	m_values;
};*/

struct SSortedPuppet
{
	SSortedPuppet(CPuppet* o, float w, float dot, float d) : obj(o), weight(w), dot(dot), dist(d) {}
	bool	operator<(const SSortedPuppet& rhs) const { return weight < rhs.weight;	}
	bool	operator>(const SSortedPuppet& rhs) const	{ return weight > rhs.weight;	}

	float			weight, dot, dist;
	CPuppet*	obj;
};

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateAmbientFire()
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );
	CAIPlayer* pPlayer = CastToCAIPlayerSafe(GetPlayer());
	// if MP game, no player - do nothing
	if(!pPlayer)
		return;

	float	dt = (GetFrameStartTime() - m_lastAmbientFireUpdateTime).GetSeconds();
	if(dt < m_cvAmbientFireUpdateInterval->GetFVal())
		return;

	m_lastAmbientFireUpdateTime = GetFrameStartTime();

	const Vec3&	playerPos = pPlayer->GetPos();
	const Vec3& playerDir = pPlayer->GetMoveDir();

	typedef std::list<SSortedPuppet, stl::STLPoolAllocator<SSortedPuppet> > TShooters;
	TShooters shooters;

	float maxDist = 0.0f;

	// Update
	for (AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET), end =  m_Objects.end(); ai != end && ai->first == AIOBJECT_PUPPET; ++ai)
	{
		CAIObject* obj = ai->second;
		if(!obj->IsEnabled())
			continue;
		CPuppet* pPuppet = obj->CastToCPuppet();
		if(!pPuppet)
			continue;

		// By default make every AI in ambient fire, only the ones that are allowed to shoot right are set explicitly.
		pPuppet->SetAllowedToHitTarget(false);

		CAIObject* pTarget = (CAIObject*)pPuppet->GetAttentionTarget();

		if(pTarget && pTarget->IsAgent())
		{
			if(pTarget == pPlayer)
			{
				Vec3	dirPlayerToPuppet = pPuppet->GetPos() - playerPos;
				float	dist = dirPlayerToPuppet.NormalizeSafe();
				if(dist > 0.01f && dist < pPuppet->GetParameters().m_fAttackRange)
				{
					maxDist = max(maxDist, dist);
					float dot = playerDir.Dot(dirPlayerToPuppet);
					shooters.push_back(SSortedPuppet(pPuppet, 1.0f - dot, dot, dist));
				}
			}
			else
			{
				// Shooting something else than player, allow to hit.
				pPuppet->SetAllowedToHitTarget(true);
			}
		}
	}

	// Update
	for (AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_VEHICLE), end =  m_Objects.end(); ai != end && ai->first == AIOBJECT_VEHICLE; ++ai)
	{
		CAIVehicle* obj = ai->second->CastToCAIVehicle();
		if(!obj->IsDriverInside())
			continue;
		CPuppet* pPuppet = obj->CastToCPuppet();
		if(!pPuppet)
			continue;

		// By default make every AI in ambient fire, only the ones that are allowed to shoot right are set explicitly.
		pPuppet->SetAllowedToHitTarget(false);

		CAIObject* pTarget = (CAIObject*)pPuppet->GetAttentionTarget();

		if(pTarget && pTarget->IsAgent())
		{
			if(pTarget == pPlayer)
			{
				Vec3	dirPlayerToPuppet = pPuppet->GetPos() - playerPos;
				float	dist = dirPlayerToPuppet.NormalizeSafe();
				if(dist > 0.01f && dist < pPuppet->GetParameters().m_fAttackRange)
				{
					maxDist = max(maxDist, dist);
					float dot = playerDir.Dot(dirPlayerToPuppet);
					shooters.push_back(SSortedPuppet(pPuppet, 1.0f - dot, dot, dist));
				}
			}
			else
			{
				// Shooting something else than player, allow to hit.
				pPuppet->SetAllowedToHitTarget(true);
			}
		}
	}

	if(!shooters.empty() && maxDist > 0.01f)
	{
		// Find nearest shooter
		for(TShooters::iterator it = shooters.begin(); it != shooters.end(); ++it)
			it->weight = sqr((1.0f - it->dot) / 2) * (0.3f + 0.7f*it->dist/maxDist);
		shooters.sort();

		Vec3 dirToNearest = shooters.front().obj->GetPos() - playerPos;
		dirToNearest.NormalizeSafe();

		for(TShooters::iterator it = shooters.begin(); it != shooters.end(); ++it)
		{
			Vec3	dirPlayerToPuppet = it->obj->GetPos() - playerPos;
			float	dist = dirPlayerToPuppet.NormalizeSafe();
			float dot = dirToNearest.Dot(dirPlayerToPuppet);
			it->weight = sqr((1.0f - dot) / 2) * (dist/maxDist);
		}
		shooters.sort();

		unsigned quota = m_cvAmbientFireQuota->GetIVal();
		unsigned i = 0;

		for(TShooters::iterator it = shooters.begin(); it != shooters.end(); ++it)
		{
			it->obj->SetAllowedToHitTarget(true);
			++i;
			if(i >= quota)
				break;
		}
	}
}


inline bool PuppetFloatSorter(const std::pair<CPuppet*, float>& lhs, const std::pair<CPuppet*, float>& rhs)
{
	return lhs.second < rhs.second;
}

//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateExpensiveAccessoryQuota()
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	for (unsigned i = 0; i < m_delayedExpAccessoryUpdates.size(); )
	{
		SAIDelayedExpAccessoryUpdate& update = m_delayedExpAccessoryUpdates[i];
		update.time -= m_fFrameDeltaTime;

		if (update.time < 0.0f)
		{
			update.pPuppet->SetAllowedToUseExpensiveAccessory(update.state);
			m_delayedExpAccessoryUpdates[i] = m_delayedExpAccessoryUpdates.back();
			m_delayedExpAccessoryUpdates.pop_back();
		}
		else
			++i;
	}

	const float updateTime = 3.0f;

	float	dt = (GetFrameStartTime() - m_lastExpensiveAccessoryUpdateTime).GetSeconds();
	if (dt < updateTime)
		return;

	m_lastExpensiveAccessoryUpdateTime = GetFrameStartTime();

	m_delayedExpAccessoryUpdates.clear();

	Vec3 interestPos = gEnv->pSystem->GetViewCamera().GetPosition();

	std::vector<std::pair<CPuppet*, float> > puppets;
	VectorSet<CPuppet*>	stateRemoved;

	// Choose the best of each group, then best of the best.
	for (AIGroupMap::iterator it = m_mapAIGroups.begin(), itend = m_mapAIGroups.end(); it != itend; ++it)
	{
		CAIGroup* pGroup = it->second;

		CPuppet* pBestUnit = 0;
		float bestVal = FLT_MAX;

		for (TUnitList::iterator itu = pGroup->GetUnits().begin(), ituend = pGroup->GetUnits().end(); itu != ituend; ++itu)
		{
			CPuppet* pPuppet = itu->m_pUnit->CastToCPuppet();
			if (!pPuppet)
				continue;
			if (!pPuppet->IsEnabled())
				continue;

			if (pPuppet->IsAllowedToUseExpensiveAccessory())
				stateRemoved.insert(pPuppet);

//			pPuppet->SetAllowedToUseExpensiveAccessory(false);

			const int accessories = pPuppet->GetParameters().m_weaponAccessories;
			if ((accessories & (AIWEPA_COMBAT_LIGHT|AIWEPA_PATROL_LIGHT)) == 0)
				continue;

			if (pPuppet->GetProxy())
			{
				SAIWeaponInfo wi;
				pPuppet->GetProxy()->QueryWeaponInfo(wi);
				if (!wi.hasLightAccessory)
					continue;
			}

			float val = Distance::Point_Point(interestPos, pPuppet->GetPos());

			if (pPuppet->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
				val *= 0.5f;
			else if (pPuppet->GetAttentionTargetThreat() >= AITHREAT_INTERESTING)
				val *= 0.8f;

			if (val < bestVal)
			{
				bestVal = val;
				pBestUnit = pPuppet;
			}
		}

		if (pBestUnit)
			puppets.push_back(std::make_pair(pBestUnit, bestVal));

	}

/*
	// Alternative logic considering best of all units.
	for (AIObjects::const_iterator ai = m_Objects.find(AIOBJECT_PUPPET), end =  m_Objects.end(); ai != end && ai->first == AIOBJECT_PUPPET; ++ai)
	{
		CAIObject* obj = ai->second;
		if(!obj->IsEnabled())
			continue;
		CPuppet* pPuppet = CastToCPuppet(obj);
		if(!pPuppet)
			continue;

		pPuppet->SetAllowedToUseExpensiveAccessory(false);

		const int accessories = pPuppet->GetParameters().m_weaponAccessories;
		if ((accessories & (AIWEPA_COMBAT_LIGHT|AIWEPA_PATROL_LIGHT)) == 0)
			continue;

		if (pPuppet->GetProxy())
		{
			SAIWeaponInfo wi;
			pPuppet->GetProxy()->QueryWeaponInfo(wi);
			if (!wi.hasLightAccessory)
				continue;
		}

		float val = Distance::Point_Point(interestPos, pPuppet->GetPos());
		int perceivedTarget = pPuppet->GetPerceivedAttentionTargetType();
		if (perceivedTarget & AITGT_THREAT)
			val *= 0.5f;
		else if (perceivedTarget & AITGT_INTERESTING)
			val *= 0.8f;

		puppets.push_back(std::make_pair(pPuppet, val));
	}
*/

	std::sort(puppets.begin(), puppets.end(), PuppetFloatSorter);

	unsigned maxExpensiveAccessories = 3;
	for (unsigned i = 0, ni = puppets.size(); i < ni && i < maxExpensiveAccessories; ++i)
	{
		stateRemoved.erase(puppets[i].first);

		if (!puppets[i].first->IsAllowedToUseExpensiveAccessory())
		{
//		puppets[i].first->SetAllowedToUseExpensiveAccessory(true);

			float t = updateTime*0.5f + ai_frand()*updateTime*0.4f;
			m_delayedExpAccessoryUpdates.push_back(SAIDelayedExpAccessoryUpdate(puppets[i].first, t, true));
		}
	}

	for (unsigned i = 0, ni = stateRemoved.size(); i < ni; ++i)
	{
		float t = ai_frand()*updateTime*0.4f;
		m_delayedExpAccessoryUpdates.push_back(SAIDelayedExpAccessoryUpdate(stateRemoved[i], t, false));
	}

}

//====================================================================
// UpdatePlayerPerception
//====================================================================
void CAISystem::UpdatePlayerPerception()
{

}

//
//to be deleted - this functionality is moved now to FLOW_NODE
void CAISystem::UpdateGlobalAlertness()
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );
	int maxAlertness = 0;
	for (int i = 3; i; i--)
		if (m_AlertnessCounters[i])
		{
			maxAlertness = i;
			break;
		}

		if (maxAlertness != m_CurrentGlobalAlertness)
		{
			// global alertness state was changed - invoke events!
			AIObjects::iterator itTrigger = m_Objects.find(AIOBJECT_GLOBALALERTNESS), itEnd = m_Objects.end();
			for ( ; itTrigger != itEnd && itTrigger->first == AIOBJECT_GLOBALALERTNESS; ++itTrigger)
			{
				CAIObject* trigger = itTrigger->second;
				IEntity* triggerEntity = trigger->GetEntity();
				IScriptTable* scriptTable = triggerEntity->GetScriptTable();

				m_pSystem->GetIScriptSystem()->BeginCall(scriptTable, "SetAlertness");
				m_pSystem->GetIScriptSystem()->PushFuncParam(scriptTable);
				m_pSystem->GetIScriptSystem()->PushFuncParam(maxAlertness);
				m_pSystem->GetIScriptSystem()->EndCall();
			}
			m_CurrentGlobalAlertness = maxAlertness;
		}
}



//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::SingleDryUpdate(CPuppet * pObject)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );
	if (pObject->IsEnabled())
		pObject->Update(AIUPDATE_DRY);
	else
		pObject->UpdateDisabled(AIUPDATE_DRY);
}

//
//-----------------------------------------------------------------------------------------------------------
bool CAISystem::SingleFullUpdate2(CPuppet* pPuppet, std::vector<CAIObject*>& priorityTargets)
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	std::vector<IPhysicalEntity*> skipList;

	EPuppetUpdatePriority	pup = pPuppet->GetUpdatePriority();

	if (pPuppet->IsEnabled() && pPuppet->GetParameters().m_nSpecies >= 0)
	{
		if (!m_priorityObjectTypes.empty())
		{
			FRAME_PROFILER( "AIPlayerVisibilityCheck", m_pSystem,PROFILE_AI);

			// Priority targets.
			for (unsigned i = 0, ni = priorityTargets.size(); i < ni; ++i)
			{
				CAIObject* pTarget = priorityTargets[i];
				if (!pPuppet->IsHostile(pTarget))
					continue;
				if (pPuppet->IsDevalued(pTarget))
					continue;
				if (!pPuppet->CanAcquireTarget(pTarget))
					continue;
				float distSq = Distance::Point_PointSq(pPuppet->GetPos(), pTarget->GetPos());
				if (distSq > sqr(pPuppet->GetSightRange(pTarget)))
					continue;
				if (!pPuppet->IsObjectInFOVCone(pTarget))
					continue;

				CAIActor* pTargetActor = pTarget->CastToCAIActor();
				if (pTargetActor && pTargetActor->IsInvisibleFrom(pPuppet->GetPos()))
					continue;

				int los = -1;

				bool lowPriorityTarget = pTarget->GetType() == AIOBJECT_ATTRIBUTE;

				// Check to reuse the line of sight information.
				if (pup == AIPUP_LOW || lowPriorityTarget)
				{
					// Allow to use positive and negative results from last 2 updates.
					for (unsigned j = 0, nj = pPuppet->m_priorityTargets.size(); j < nj; ++j)
					{
						CAIActor::SProbableTarget& pri = pPuppet->m_priorityTargets[j];
						if (pri.pTarget == pTarget)
						{
							if (pri.updates <= 2)
								los = pri.lineOfSight;
							break;
						}
					}
				}
				else if (pup == AIPUP_HIGH || pup == AIPUP_MED)
				{
					// Allow to use positive and negative results from last update.
					for (unsigned j = 0, nj = pPuppet->m_priorityTargets.size(); j < nj; ++j)
					{
						CAIActor::SProbableTarget& pri = pPuppet->m_priorityTargets[j];
						if (pri.pTarget == pTarget)
						{
							if (pri.updates <= 1)
								los = pri.lineOfSight;
							break;
						}
					}
				}
				else // AIPUP_VERY_HIGH
				{
					// Allow to use positive results from last update.
					for (unsigned j = 0, nj = pPuppet->m_priorityTargets.size(); j < nj; ++j)
					{
						CAIActor::SProbableTarget& pri = pPuppet->m_priorityTargets[j];
						if (pri.pTarget == pTarget)
						{
							if (pri.updates <= 1 && pri.lineOfSight == 1)
								los = pri.lineOfSight;
							break;
						}
					}
				}

				m_visChecks++;

				if (los == -1)
				{
					// Check visibility if uncertain line of sight.
					skipList.clear();
					pPuppet->GetPhysicsEntitiesToSkip(skipList);
					if (pTargetActor)
						pTargetActor->GetPhysicsEntitiesToSkip(skipList);

					const Vec3& puppetPos = pPuppet->GetPos();
					const Vec3& targetPos = pTarget->GetPos();

					bool skipSoftCover = false;
					// See grenades through vegetation.
					if (pTarget->GetType() == AIOBJECT_GRENADE || pTarget->GetType() == AIOBJECT_RPG)
						skipSoftCover = true;
					// See live and memory target through vegetation.
					bool hasLiveTarget = (pPuppet->GetAttentionTargetType() == AITARGET_VISUAL ||
																pPuppet->GetAttentionTargetType() == AITARGET_MEMORY) &&
																pPuppet->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE;
					if (hasLiveTarget && distSq < sqr(pPuppet->GetParameters().m_fMeleeDistance * 1.2f))
						skipSoftCover = true;

					int flags = HIT_COVER;
					if (!skipSoftCover)
						flags |= HIT_SOFT_COVER;

					int rwi = 0;
					// Obstruction spheres
					rwi = RayObstructionSphereIntersection(puppetPos, targetPos);

					// Occlusions planes
					if (!rwi)
						rwi = RayOcclusionPlaneIntersection(puppetPos, targetPos);

					// World
					if (!rwi)
					{
						ray_hit hit;
						TICK_COUNTER_FUNCTION
						TICK_COUNTER_FUNCTION_BLOCK_START

						rwi = m_pWorld->RayWorldIntersection(targetPos, puppetPos - targetPos, COVER_OBJECT_TYPES, flags,
							&hit, 1, skipList.empty() ? 0 : &skipList[0], skipList.size());

						TICK_COUNTER_FUNCTION_BLOCK_END
						++m_nRaysThisUpdateFrame;
					}

					los = (!rwi) ? 1 : 0;

					pPuppet->RefreshPriorityTarget(pTarget, los);

					m_visChecksRays++;
				}

				if (los == 1)
				{
					// Notify visual perception.
					SAIEVENT event;
					event.pSeen = pTarget;
					event.bFuzzySight = false;
					if (event.pSeen)
						pPuppet->Event(AIEVENT_ONVISUALSTIMULUS, &event);
				}

				if (pTarget->IsEnabled() && ((pTarget->GetType()==AIOBJECT_PLAYER) || (pTarget->GetType()==AIOBJECT_PUPPET) ))
				{
					if (pPuppet->GetAttentionTarget() == pTarget)
					{
						if (distSq < sqr(pPuppet->GetParameters().m_fMeleeDistance))
						{
							if (pPuppet->CloseContactEnabled())
							{
								pPuppet->SetSignal(1, "OnCloseContact", pTarget->GetEntity(), 0, g_crcSignals.m_nOnCloseContact);		
								pPuppet->SetCloseContact(true);
							}
						}
					}
				}
			}
			//
			pPuppet->UpdatePriorityTargets();
		}


		{
			FRAME_PROFILER( "ProbableTargets", m_pSystem,PROFILE_AI);
			// Probable targets.
			for (unsigned i = 0, ni = pPuppet->m_probableTargets.size(); i < ni; ++i)
			{
				CAIActor::SProbableTarget& prob = pPuppet->m_probableTargets[i];

				CPuppet* pTargetPuppet = prob.pTarget->CastToCPuppet();
				AIAssert(pTargetPuppet);

				if (pTargetPuppet->m_Parameters.m_bInvisible || pPuppet->IsDevalued(prob.pTarget))
					continue;
				if (!pPuppet->CanAcquireTarget(pTargetPuppet))
					continue;
				if (!pPuppet->IsHostile(pTargetPuppet))
					continue;
				float distSq = Distance::Point_PointSq(pPuppet->GetPos(), pTargetPuppet->GetPos());
				if (distSq > sqr(pPuppet->GetSightRange(pTargetPuppet)))
					continue;
				if (!pPuppet->IsObjectInFOVCone(pTargetPuppet))
					continue;

				int los = -1;
				if (prob.updates <= 3)
					los = prob.lineOfSight;
				if (los == -1)
					los = pTargetPuppet->HasLineOfSightTo(pPuppet);

				m_visChecks++;

				if (los == -1)
				{
					// Check visibility if uncertain line of sight.

					const Vec3& puppetPos = pPuppet->GetPos();
					const Vec3& targetPos = pTargetPuppet->GetPos();

					int rwi = 0;
					// Obstruction spheres
					rwi = RayObstructionSphereIntersection(puppetPos, targetPos);

					// Occlusions planes
					if (!rwi)
						rwi = RayOcclusionPlaneIntersection(puppetPos, targetPos);

					// World
					if (!rwi)
					{
						ray_hit hit;

						skipList.clear();
						pPuppet->GetPhysicsEntitiesToSkip(skipList);
						pTargetPuppet->GetPhysicsEntitiesToSkip(skipList);

						TICK_COUNTER_FUNCTION
						TICK_COUNTER_FUNCTION_BLOCK_START
						
						rwi = m_pWorld->RayWorldIntersection(puppetPos, targetPos - puppetPos, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER,
							&hit, 1, skipList.empty() ? 0 : &skipList[0], skipList.size());
						
						TICK_COUNTER_FUNCTION_BLOCK_END
						++m_nRaysThisUpdateFrame;
					}

					prob.lineOfSight = (rwi == 0) ? 1 : 0;
					prob.updates = 0;
					los = prob.lineOfSight;

					m_visChecksRays++;
				}

				if (los == 1)
				{
					SAIEVENT event;
					event.pSeen = pTargetPuppet;
					event.bFuzzySight = false;
					event.fThreat = 1.f;
					if (event.pSeen)
						pPuppet->Event(AIEVENT_ONVISUALSTIMULUS, &event);
//					pPuppet->NotifyEnemyInFOV(pTargetPuppet);
					// handle OnCloseContact for both
					pPuppet->CheckCloseContact(pTargetPuppet, sqrtf(distSq));
				}
			}
			pPuppet->UpdateProbableTargets();
		}
	}

	if (m_nRaysThisUpdateFrame > m_cvAIVisRaysPerFrame->GetIVal())
		return true;

	return false;
}


//
//-----------------------------------------------------------------------------------------------------------
void CAISystem::UpdateAuxSignalsMap()
{
	FUNCTION_PROFILER( m_pSystem,PROFILE_AI );

	if (!m_mapAuxSignalsFired.empty())
	{
		MapSignalStrings::iterator mss = m_mapAuxSignalsFired.begin();
		while (mss!=m_mapAuxSignalsFired.end())
		{
			(mss->second).fTimeout-= m_fFrameDeltaTime;
			if ((mss->second).fTimeout < 0)
			{
				MapSignalStrings::iterator mss_to_erase = mss;
				++mss;
				m_mapAuxSignalsFired.erase(mss_to_erase);
			}
			else
				++mss;
		}
	}
}
