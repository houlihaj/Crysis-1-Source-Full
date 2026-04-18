/********************************************************************
Crytek Source File.
Copyright  , Crytek Studios, 2006-2007.
---------------------------------------------------------------------
File name:   PersonalInterestManager.cpp
$Id$
$DateTime$
Description: Interest Manager (tracker) for interested individuals

Notes:       Consider converting interest values float->int 
*********************************************************************/


#include "StdAfx.h"

#include "PersonalInterestManager.h"
#include "CentralInterestManager.h"
#include "CAISystem.h"

// For persistent debugging
#include "IGameFramework.h"

#include <assert.h>

#define PI 3.141592653589793f
#define SCANDIST 10.0f

#define CHECK_CACHE_CVAR(cache, name) if (!cache) { cache = gEnv->pConsole->GetCVar(name); assert(cache); }

ICVar *CPersonalInterestManager::m_cvEnableInterestScan = NULL;
ICVar *CPersonalInterestManager::m_cvInterestEntryBoost = NULL;
ICVar *CPersonalInterestManager::m_cvInterestSwitchBoost = NULL;
ICVar *CPersonalInterestManager::m_cvInterestSpeedMultiplier = NULL;
ICVar *CPersonalInterestManager::m_cvInterestScalingScan = NULL;
ICVar *CPersonalInterestManager::m_cvInterestScalingMovement = NULL;
ICVar *CPersonalInterestManager::m_cvInterestScalingView = NULL;

CPersonalInterestManager::CPersonalInterestManager(void)
{
	m_pCIM = NULL;
	m_pInterestDummy = GetAISystem()->CreateDummyObject("InterestDummy");

	// Check static console variable cache
	CHECK_CACHE_CVAR(m_cvEnableInterestScan, "ai_InterestEnableScan");
	//CHECK_CACHE_CVAR(m_cvInterestEntryBoost, "ai_InterestEntryBoost");
	CHECK_CACHE_CVAR(m_cvInterestSwitchBoost, "ai_InterestSwitchBoost");
	//CHECK_CACHE_CVAR(m_cvInterestSpeedMultiplier, "ai_InterestEntryBoost");
	CHECK_CACHE_CVAR(m_cvInterestScalingScan, "ai_InterestScalingScan");
	//CHECK_CACHE_CVAR(m_cvInterestScalingAmbient, "ai_InterestScalingAmbient");
	CHECK_CACHE_CVAR(m_cvInterestScalingMovement, "ai_InterestScalingMovement");
	//CHECK_CACHE_CVAR(m_cvInterestScalingEyeCatching, "ai_InterestScalingEyeCatching");
	CHECK_CACHE_CVAR(m_cvInterestScalingView, "ai_InterestScalingView");

	Reset();
}

//------------------------------------------------------------------------------------------------------------------------

CPersonalInterestManager::~CPersonalInterestManager(void)
{
	// Might check with the CIM that this is deregistered, but ownership should be made clear.
	GetAISystem()->RemoveDummyObject(m_pInterestDummy);
}

//------------------------------------------------------------------------------------------------------------------------

// Clear tracking cache, don't clear assignment
void CPersonalInterestManager::Reset(void)
{
	m_fBlendTime = 2.0f;
	m_pAIActor = NULL;
	m_eInterestType = eIT_None;
	m_InterestingEntity = NULL;
	SetMinimumInterest(0.0f);
	m_Tracker.clear();

	SPersonalEntityInterest dummy( 50.0f, eIT_Dummy); // For look IK scanning
	
	m_Tracker.push_back(dummy);
	m_prevLookPoint.zero();
	m_fLookBlend = -1.0f;		// Current not look-blending 
	for (int i=0; i<PIM_SCANPOINTS;	i++)
		m_scanPointSeeds[i] = 0;  // Reset to invalid
}

//------------------------------------------------------------------------------------------------------------------------

// You must also ensure the PIM pointer in the CAIActor is set to this object
void CPersonalInterestManager::Assign( CAIActor* pAIActor )
{
	// Check for redundant calls
	if (m_pAIActor == pAIActor) return;

	// Grab a cached pointer to the CIM. We won't need it before we're first assigned.
	if (!m_pCIM) m_pCIM = CCentralInterestManager::GetInstance();

	// Debug draw
	if (m_pCIM->IsDebuggingEnabled())
	{
		if (m_pAIActor)	m_pCIM->AddDebugTag(m_pAIActor->GetEntityID(), "Deassigned PIM");
		if (pAIActor)		m_pCIM->AddDebugTag(pAIActor->GetEntityID(), "Assigned PIM");
	}

	// Reset and initialise
	Reset();						// Clear any info from the previous assignment
	if (pAIActor)	m_prevLookPoint = pAIActor->GetPos() + pAIActor->GetViewDir() * 5.0f;		// Init point for blending
	else m_prevLookPoint.zero();

	// Assign
	m_pAIActor = pAIActor;
}

//------------------------------------------------------------------------------------------------------------------------

CAIActor * CPersonalInterestManager::GetAssigned(void)
{
	return m_pAIActor;	
}

//------------------------------------------------------------------------------------------------------------------------

void CPersonalInterestManager::SetMinimumInterest( float fMinInterest )
{
	assert(fMinInterest >=0.0f); // Better to log this?
	m_fMinInterest = fMinInterest;
	m_fThresholdInterest = MAX(fMinInterest, 5.0f) * 2.0f;		// Initially require at least the interest to double
}

//------------------------------------------------------------------------------------------------------------------------

void CPersonalInterestManager::Update( float fDelta )
{
	SPersonalEntityInterest *bestPEI = NULL;
	SPersonalEntityInterest *oldPEI = NULL;

	// Make sure harmless if not assigned
	if (!m_pAIActor) return;

	// Check if we are allowing scanning
	TrackInterest( &oldPEI, &bestPEI, fDelta );
	
	if (!bestPEI || bestPEI->fAccumulated < m_fThresholdInterest)
	{
		// Nothing at all is interesting enough - end all activity
		if (m_eInterestType == eIT_Dummy) EndScan();
		m_InterestingEntity = NULL;
		m_eInterestType = eIT_None;
	}
	else
	{
		// Something was interesting
		
		// Is our chief interest different?
		if (m_eInterestType != bestPEI->eType || 
			m_eInterestType == eIT_Entity && m_InterestingEntity != bestPEI->pEntity)
		{
			// Try to acquire the new target
			if ( (bestPEI->eType == eIT_Dummy && StartScan())
					|| (bestPEI->eType == eIT_Entity && ChangeInterest( oldPEI, bestPEI )) )
			{
				// Got new target

				// Start or stop scanning
				if (m_eInterestType == eIT_Dummy) EndScan();
			
				// Deal with entity type
				if (bestPEI->eType == eIT_Entity)
				{
					
					// Adjust threshold - i.e. "phew this is pretty exciting, will take a while to notice anything else!"
					// Will only have a significant effect when something new shows up
					//m_fThresholdInterest = fMostInterest;

					// Switch interests
					m_InterestingEntity = bestPEI->pEntity;
				}
				else
				{
					m_InterestingEntity = NULL;
				}
				// Set the new type
				m_eInterestType = bestPEI->eType;

				// Boost the accumulated interest in this item so we will tend to stick with it for a while
				bestPEI->fAccumulated *= m_cvInterestSwitchBoost->GetFVal();				

				// Drop all accumulated interest in the old target
				if (oldPEI) 
					oldPEI->fAccumulated = 0.0f;

				// Signal the AI to let them know
				m_pAIActor->SetSignal(1, "OnInterestSystemEvent", 0,0, g_crcSignals.m_nOnInterestSystemEvent);
			}// (Try to acquire the new target)
		}// (Is our chief interest of a different type now?) 

		if (m_eInterestType == eIT_Dummy)
		{
			// This is the dummy item for scanning
			//m_pCIM->AddDebugTag(m_pAIActor->GetEntityID(), "Yes");
			UpdateScan(fDelta);			// No raycast
		}
	}// (Something was interesting)

	// If nothing currently reaches our threshold, allow it to reduce
	// This brings us back towards a baseline after thrilling events
	// Using natural log would be better here?
	//float fBaseline = MAX(m_fMinInterest, 5.0f) * 2.0f;
	//if (m_fThresholdInterest > fBaseline)
	//	m_fThresholdInterest -= fDelta * (m_fThresholdInterest - fBaseline) * 0.1f;
}

//------------------------------------------------------------------------------------------------------------------------

void CPersonalInterestManager::TrackInterest( SPersonalEntityInterest ** const ppOldPEI, SPersonalEntityInterest ** const ppBestPEI, float fDelta )
{
	Vec3 actorPos = m_pAIActor->GetPos();
	Vec3 actorDir = m_pAIActor->GetViewDir();
	float fMostInterest = 0.0f;

	// Check if we are allowing scanning
	bool bEnableScan = m_cvEnableInterestScan->GetIVal()!=0;

	std::vector<SPersonalEntityInterest>::iterator itPEI;
	for ( itPEI = m_Tracker.begin(); itPEI != m_Tracker.end(); itPEI++ )
	{
		// Basic accumulation of interest
		float fChange = fDelta * itPEI->fInterest * 0.1f;

		// Randomise this a bit
		//fChange *= Random() * 2.0f;

		// Is this in visual range and FOV at all?
		//if (! m_pAIActor->PointVisible(vPoint)) continue;

		// Scale this according to how close and visible it is
		if (itPEI->pEntity)
		{
			const Vec3 vPoint = itPEI->pEntity->GetPos();
			Vec3 actorToObj = vPoint - actorPos;
			fChange *= MIN( 10.0f / actorToObj.GetLength(), 20.0f);
			fChange *= MAX( actorToObj.normalized().Dot( actorDir ), 0.0f );
		}
		else
		{
			// Dummy
			// Update scaling
			itPEI->fInterest = m_cvInterestScalingScan->GetFVal();
		}
		
		// Is this our current interest target? 
		if (m_eInterestType == itPEI->eType)
		{
			if ( (m_eInterestType == eIT_Entity && m_InterestingEntity == itPEI->pEntity)
				|| (m_eInterestType == eIT_Dummy) )
			{
				fChange *= -1.00f;		// Consume some of the interest in this item, not add to it
				*ppOldPEI = &(*itPEI); // Take note of this as the existing interest
			}
		}
		// Adjust interest level of this object
		itPEI->fAccumulated += fChange;

		// Is this the most interesting so far?
		if (fMostInterest < itPEI->fAccumulated)
		{
			if (itPEI->eType == eIT_Dummy && bEnableScan)
			fMostInterest = itPEI->fAccumulated;
			*ppBestPEI = &(*itPEI);
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------

bool CPersonalInterestManager::StartScan(void)
{
	Vec3 actorPos = m_pAIActor->GetPos();
	Vec3 actorDir = m_pAIActor->GetViewDir();

	// Try to acquire a scantarget
	// If we return at any point, we'll just try again next update

	// Try an existing seed (point)
	int iSeedPoint = Random(PIM_SCANPOINTS);
	int *pnSeed = &(m_scanPointSeeds[iSeedPoint]);

	if (! *pnSeed)
	{
		// Generate
		*pnSeed = Random(1<<30);
		m_pCIM->AddDebugTag(m_pAIActor->GetEntityID(), "New scan point");
	}

	Vec3 vPoint = ConvertSeedToPoint(*pnSeed);
	
	// Test that it falls within allows range for this AI
	if (!IsPointAcceptable(vPoint)) { *pnSeed = 0; return false; }	// Invalidate that point
	
	// Check that the CIM can afford a raycast
	if (!m_pCIM->MayFireRay(this)) return false;

	// Can we actually see it?
	if (!CheckVisibilityOfPoint(vPoint)) { *pnSeed = 0; return false; } // Invalidate that point

	m_pCIM->AddDebugTag(m_pAIActor->GetEntityID(), "Started scan");

	// Signal the AI to let them know
	m_pAIActor->SetSignal(1, "OnInterestSystemEvent", 0,0, g_crcSignals.m_nOnInterestSystemEvent);

	/// Initialise the scan
	m_fLookBlend = 0.0f;
	m_prevLookPoint = actorPos + actorDir * SCANDIST;		// Where we're looking now
	m_newLookPoint = vPoint;		
	return true;
}

//------------------------------------------------------------------------------------------------------------------------

void CPersonalInterestManager::UpdateScan(float fDelta)
{
	assert( m_fLookBlend >= 0.0f );

	// Scan
	m_fLookBlend += fDelta / m_fBlendTime;
	
	if (m_fLookBlend > 0.95f)
	{
		// Reached scan point, limit, and try to start a new scan
		m_fLookBlend = 1.0f;
		StartScan();
	}
	return;
}

//------------------------------------------------------------------------------------------------------------------------

void CPersonalInterestManager::EndScan(void)
{
	Vec3 actorPos = m_pAIActor->GetPos();
	Vec3 actorDir = m_pAIActor->GetViewDir();
	m_fLookBlend = -1.0f;
	m_newLookPoint = actorPos + actorDir * SCANDIST;		// Where we're looking now
	m_prevLookPoint = m_newLookPoint;
	m_pCIM->AddDebugTag(m_pAIActor->GetEntityID(), "Ended scanning");
}

//------------------------------------------------------------------------------------------------------------------------

bool CPersonalInterestManager::ChangeInterest( SPersonalEntityInterest * const oldPEI, SPersonalEntityInterest * const bestPEI )
{
	float fMostInterest = bestPEI->fAccumulated;

	// Check this target is actually visible
	// If not visible, might shunt it into some secondary list for a while, for performance
	if (!CheckVisibilityOfPoint(bestPEI->pEntity))
	{
		string sText;
		sText.Format("Can't see interest target: %s [%f,%f]", bestPEI->pEntity->GetName(), bestPEI->fInterest, fMostInterest);
		m_pCIM->AddDebugTag(m_pAIActor->GetEntityID(), sText);

		bestPEI->fAccumulated = 0.0f; // Reset the accumulated interest to ignore for a while
		return false;
	} 

	if (m_pCIM->IsDebuggingEnabled())
	{
		string sText;
		sText.Format("New interest target: %s [%f]", bestPEI->pEntity->GetName(), fMostInterest);
		m_pCIM->AddDebugTag(m_pAIActor->GetEntityID(), sText);

		sText.Format("Interesting to: %s [%f]", m_pAIActor->GetName() , fMostInterest);
		m_pCIM->AddDebugTag(bestPEI->pEntity->GetId(), sText);
	}
	return true;
}

//------------------------------------------------------------------------------------------------------------------------

float CPersonalInterestManager::ConsiderInterestEntity ( const SEntityInterest &entInterest )
{

	// Does this object have any kind of special relevance to me?
	//entInterest.fInterest = 

	SPersonalEntityInterest perInterest(entInterest);
	perInterest.fAccumulated = perInterest.fInterest;  // Boost it proportionally, when it first arrives

	// Check if we already have it, otherwise boost and add

	// For now, always add to tracking list
	m_Tracker.push_back(perInterest);

	return entInterest.fInterest;
}

//------------------------------------------------------------------------------------------------------------------------

// Stop considering the object, usually because it has been deleted
void CPersonalInterestManager::ForgetInterestEntity( IEntity *pEntity )
{
	// Remove entity from tracking lists
	std::vector<SPersonalEntityInterest>::iterator itPEI;
	for ( itPEI = m_Tracker.begin(); itPEI != m_Tracker.end(); itPEI++ )
	{
		if (itPEI->pEntity == pEntity) break;
	}
	if (itPEI != m_Tracker.end()) m_Tracker.erase(itPEI);
	
	// Check for case when we were interested in that at the time, in blunt fashion
	if (m_eInterestType == eIT_Entity && pEntity == m_InterestingEntity) Reset();
}

//------------------------------------------------------------------------------------------------------------------------

Vec3 CPersonalInterestManager::GetInterestPoint(void)
{
	Vec3 interest(0,0,0);
	if (m_eInterestType == eIT_Entity && m_InterestingEntity) 
	{
		interest = m_InterestingEntity->GetPos();
	}
	else
	{
		if (m_fLookBlend < 0.0) interest = m_prevLookPoint;
		else interest = (m_newLookPoint * m_fLookBlend) + (m_prevLookPoint * ( 1.0f - m_fLookBlend ));
	}
	return interest;
}

//------------------------------------------------------------------------------------------------------------------------

IEntity * CPersonalInterestManager::GetInterestEntity(void)
{
	return (m_eInterestType == eIT_Entity ? m_InterestingEntity : NULL);
}

//------------------------------------------------------------------------------------------------------------------------

CAIObject * CPersonalInterestManager::GetInterestDummyPoint(void)
{
	if (m_eInterestType != eIT_None)
	{
		m_pInterestDummy->SetPos( GetInterestPoint() );
		return m_pInterestDummy;
	}
	return NULL;
}

//------------------------------------------------------------------------------------------------------------------------

bool CPersonalInterestManager::CheckVisibilityOfPoint( IEntity * pTargetEnt )
{
	Vec3 vEyePos = m_pAIActor->GetPos();  // For an AI actor with a proxy, this is the eye position
	
	// Choose a location within the entity
	Vec3 vPoint2 = pTargetEnt->GetPos() + Vec3(0,0,1);

	ray_hit hits;
	int intersections =  gEnv->pPhysicalWorld->RayWorldIntersection( vEyePos, vPoint2 - vEyePos, ent_all,
		rwi_stop_at_pierceable|rwi_colltype_any, &hits, 1, m_pAIActor->GetPhysics() );

	if (!intersections) return true;
	IEntity *pEntHit = GetISystem()->GetIEntitySystem()->GetEntityFromPhysics(hits.pCollider);
	
	return (pTargetEnt == pEntHit);

	/*
	ISurfaceType *pSurf = GetISystem()->GetI3DEngine()->GetMaterialManager()->GetSurfaceType(hits.surface_idx);
	IEntity *pEntTarget = GetISystem()->GetIEntitySystem()->GetEntityFromPhysics(pTargetPhysEnt);
	IPersistantDebug * pPersistantDebug = gEnv->pGame->GetIGameFramework()->GetIPersistantDebug();
	pPersistantDebug->Begin("CheckVisibilityOfPoint", false);
	pPersistantDebug->AddSphere( vPoint2, 0.4f, ColorF(1,1,1), 10.0f );
	pPersistantDebug->AddSphere( vEyePos, 0.1f, ColorF(1,1,1), 10.0f );
	pPersistantDebug->AddLine(vEyePos, vPoint2, ColorF(1,1,1), 10.0f );
	*/
}

bool CPersonalInterestManager::CheckVisibilityOfPoint( const Vec3 &vPoint, IPhysicalEntity * pTargetPhysEnt )
{
	// Shouldn't this be functionality provided elsewhere?
	
	// Check visibility cone - is this too conservative?
	//if (! m_pAIActor->PointVisible(vPoint)) return false;

	// Visibilty test above ensures short rays are used, but could double-check this

	// Fire a ray from head to point
	Vec3 vEyePos = m_pAIActor->GetPos();  // For an AI actor with a proxy, this is the eye position

	ray_hit hits;
	int intersections =  gEnv->pPhysicalWorld->RayWorldIntersection( vEyePos, vPoint - vEyePos, ent_all,
		rwi_stop_at_pierceable|rwi_colltype_any, &hits, 1, m_pAIActor->GetPhysics(), pTargetPhysEnt );

	return (intersections == 0);
}

//------------------------------------------------------------------------------------------------------------------------

// Take a seed and map it to the finite set of look points
Vec3 CPersonalInterestManager::ConvertSeedToPoint( unsigned int seed )
{
	int nTrunc = seed & (0xFFF);		// Lower 12 bits
	int nPitch = nTrunc >> 8;				// Take upper 4 bits
	int nBearing = nTrunc & (0xFF); // Take lower 8 bits
	Vec3 actorPos = m_pAIActor->GetPos();

	// Could try looking right down, eventually

	// This will generate points of varying height SCANDIST horizontal metres away
	float fHeight = (nPitch - 8) * 0.3f;
	float fBearing = nBearing * (PI * 2.0f / 255.0f);

	//Old generation code
	//Vec3 vPoint( Random(SCANDIST*2), Random(SCANDIST*2), Random(SCANDIST*2) * 0.1f );
	//vPoint -= Vec3( SCANDIST, SCANDIST, SCANDIST * 0.1f );

	Vec3 vPoint(cos(fBearing) * SCANDIST, sin(fBearing) * SCANDIST, fHeight);
	vPoint += actorPos;
	return vPoint;
}

//------------------------------------------------------------------------------------------------------------------------

bool CPersonalInterestManager::IsPointAcceptable( const Vec3 &vPoint)
{
	Vec3 actorPos = m_pAIActor->GetPos();
	float fDist = vPoint.GetDistance(actorPos);
	if (fDist > SCANDIST * 1.5f) return false;

	//m_pAIActor->IsPointVisible()
	return true;
}

//------------------------------------------------------------------------------------------------------------------------
