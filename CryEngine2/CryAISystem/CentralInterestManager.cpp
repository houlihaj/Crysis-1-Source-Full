#include "StdAfx.h"
#include "CentralInterestManager.h"
#include "PersonalInterestManager.h"
#include "AIActor.h"

// For persistent debugging
#include "IGameFramework.h"

#define CIM_RAYBUDGET 1			// Max number of rays the interest system may fire per update; 2 would be extravagant

//------------------------------------------------------------------------------------------------------------------------

// Zero singleton pointer
CCentralInterestManager * CCentralInterestManager::m_pCIMInstance = NULL;

// Initially set to defaults
int CCentralInterestManager::m_snMaxPotentialAIActors = 32;
int CCentralInterestManager::m_snMaxPIMs = 16;
int CCentralInterestManager::m_snMaxInteresting = 128;


//------------------------------------------------------------------------------------------------------------------------

CCentralInterestManager::CCentralInterestManager(void)
{
	m_bEnabled = false; // Should be cvar	
	m_fMaxDistSq = square(200.0f);
	m_bEntityEventListenerInstalled = false;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::Init(void)
{	
	m_cvDebugInterest = gEnv->pConsole->GetCVar("ai_DebugInterestSystem");
	m_cvInterestScalingEyeCatching = gEnv->pConsole->GetCVar("ai_InterestScalingEyeCatching");
	m_cvInterestScalingAmbient = gEnv->pConsole->GetCVar("ai_InterestScalingAmbient");
	m_cvInterestListenMovement = gEnv->pConsole->GetCVar("ai_InterestDetectMovement");

	Reset();
}

//------------------------------------------------------------------------------------------------------------------------

CCentralInterestManager::~CCentralInterestManager(void)
{
	Cleanup();
}

//------------------------------------------------------------------------------------------------------------------------

CCentralInterestManager * CCentralInterestManager::GetInstance(void)
{
	// Return singleton pointer
	// Create from scratch if need be
	if (!m_pCIMInstance)
	{
		m_pCIMInstance = new CCentralInterestManager();
		m_pCIMInstance->Init();
	}
	return m_pCIMInstance;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::Reset(void)
{
	// Stop listening to all moving entities
	if (m_bEntityEventListenerInstalled)
	{
		gEnv->pEntitySystem->RemoveSink( this );	
		m_bEntityEventListenerInstalled = false;
	}

	// Deallocate vectors
	Cleanup();

	// Reinit to same sizes
	SetMaxNumPotentialInterestedAIActor(m_snMaxPotentialAIActors);
	SetMaxNumInterested(m_snMaxPIMs);
	SetMaxNumInteresting(m_snMaxInteresting);
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::Enable(bool bEnable)
{
	if (m_bEnabled == bEnable) return;

	m_bEnabled = bEnable;
	
	Reset();	// Clear out or create PIMs
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::Update( float fDelta)
{
	if (!m_bEnabled) return;

	// Check event listener
	if (!m_bEntityEventListenerInstalled)
	{
		// Start listening to all moving entities
		gEnv->pEntitySystem->AddSink( this );		
		m_bEntityEventListenerInstalled = true;
	}

	// Cache debug pointer
	m_pPersistantDebug = gEnv->pGame->GetIGameFramework()->GetIPersistantDebug();
	assert(m_pPersistantDebug);			// Make really sure it's created at this stage or debugging (only) will crash

	// Cache camera position and view direction
	const CCamera cam = gEnv->pSystem->GetViewCamera();
	m_vViewerPos = cam.GetPosition();




	// Update the list of potentially interested actors

	/*
	// For now, just find those that are nearest -
	// Just look for one randomly per frame
	
	IEntity LOCAL_PLAYER_ENTITY_ID;
	IAIObject *pRef = cam.GetPosition();
	gEnv->pEntitySystem->GetRandomObjectInRange(pRef, AIOBJECT_PUPPET,0, 100, false);
	*/





	// Consider reallocating PIMs



	// Update all the active PIMs
	for (std::vector<CAIActor*>::iterator itA = m_PotentialAIActors.begin(); itA != m_PotentialAIActors.end(); itA++)
	{
		CAIActor * pAIActor = *itA;
		if (pAIActor)
		{
			IPersonalInterestManager * pPIM = pAIActor->GetInterestManager();
			if (pPIM) pPIM->Update(fDelta);
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::RegisterInterestingEntity( IEntity *pEntity, float fBaseInterest )
{
	if (!m_bEnabled) return false;

	// Check if we already have this entity
	// Bit slow. Can do better by occasionally sorting?
	std::vector<SEntityInterest>::iterator itI;
	for (itI = m_InterestingEntities.begin(); itI != m_InterestingEntities.end(); itI++)
		if (itI->pEntity == pEntity)
		{			
			itI->fInterest = fBaseInterest * m_cvInterestScalingAmbient->GetFVal();
			// TODO: Should have debug message
			return true;
		}

	if (m_InterestingEntities.size() < m_snMaxInteresting)
	{
		// Add this entity
		SEntityInterest entInterest;
		entInterest.pEntity = pEntity;
		entInterest.fInterest = fBaseInterest;
		m_InterestingEntities.push_back(entInterest);

		// Add to quick check table
		m_RegisteredIntEnts.insert(pEntity);
	
		// Tell the PIMs about it
		std::vector<CPersonalInterestManager*>::iterator itPIM;
		for (itPIM = m_PIMs.begin(); itPIM != m_PIMs.end(); itPIM++)
		{
			if ((*itPIM)->GetAssigned()) 
				(*itPIM)->ConsiderInterestEntity(entInterest);
		}

		// Debugging
		string sText;
		sText.Format("Registered [%f]", fBaseInterest);
		AddDebugTag(pEntity->GetId(), sText, 12.0f);

		return true;
	}

	return false;
}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::RegisterInterestedAIActor( IEntity *pEntity, float fForceUntilDistance )
{
	if (!m_bEnabled) return false;

	// Try to get an AI object
	IAIObject *pAIObj = pEntity->GetAI();
	if (!pAIObj) return false;
	
	// Try to cast to actor
	CAIActor* pActor = CastToCAIActorSafe(pAIObj);
	if (!pActor) return false;

	// Check for existing registration to be updated
	std::vector<CAIActor*>::iterator itA;
	for (itA = m_PotentialAIActors.begin(); itA != m_PotentialAIActors.end(); itA++)
		if (*itA == pActor) return true;										// Already registered

	//Find a spare AIActor slot _and_ 
	for (itA = m_PotentialAIActors.begin(); itA != m_PotentialAIActors.end(); itA++)
		if (*itA == NULL) break;
	
	if (itA == m_PotentialAIActors.end()) return false;		// Found no spare actor slot


	// Find a spare PIM
	std::vector<CPersonalInterestManager*>::iterator itPIM;	
	for (itPIM = m_PIMs.begin(); itPIM != m_PIMs.end(); itPIM++)
		if (!(*itPIM)->GetAssigned()) break;
	
	if (itPIM == m_PIMs.end()) return false;							// No spare PIM

	// Make assignments
	*itA = pActor;													// Keep track of this actor
	(*itPIM)->Assign(pActor);								// Assign a PIM to him
	pActor->AssignInterestManager(*itPIM);	// Give him the PIM

	// Pass on the current list of interesting objects
	std::vector<SEntityInterest>::iterator itI;
	for ( itI = m_InterestingEntities.begin(); itI != m_InterestingEntities.end(); itI++ )
		(*itPIM)->ConsiderInterestEntity(*itI);
		
	return true;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::SetMaxNumPotentialInterestedAIActor(int nMaxPotentialAIActors)
{
	if (nMaxPotentialAIActors < 0) nMaxPotentialAIActors = 0;
	if (nMaxPotentialAIActors > 128) nMaxPotentialAIActors = 128;

	// Preserve across resets
	m_snMaxPotentialAIActors = nMaxPotentialAIActors;

	// No cleanup required, NULL pointers are valid
	m_PotentialAIActors.resize(nMaxPotentialAIActors, NULL);
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::SetMaxNumInterested(int nMaxPIMs)
{
	if (nMaxPIMs < 0) nMaxPIMs = 0;
	if (nMaxPIMs > 32) nMaxPIMs = 32;
	
	// Preserve across resets
	m_snMaxPIMs = nMaxPIMs;

	if (!m_bEnabled) 
		nMaxPIMs = 0; // If no enabled, override so don't instantiate any PIMs

	// Create more instances to fill as required
	while ( m_PIMs.size() < nMaxPIMs )
	{
		m_PIMs.push_back( new CPersonalInterestManager() );
	}

	// Throw out some PIMs without any consideration as to their importance :-)
	while ( m_PIMs.size() > nMaxPIMs )
	{
		CPersonalInterestManager *pPIM = m_PIMs.back();
		if (pPIM) 
		{
			pPIM->Assign(NULL);
			delete(pPIM);
		}
		m_PIMs.pop_back();
	}	
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::SetMaxNumInteresting(int nMaxInteresting )
{
	// Silently clamp to sane values
	if (nMaxInteresting < 0) nMaxInteresting = 0;
	if (nMaxInteresting > 1024) nMaxInteresting = 1024;

	// Sort...

	// Shrink
	if (nMaxInteresting < m_InterestingEntities.size()) m_InterestingEntities.resize(nMaxInteresting);	
	// Perhaps need to send deregister message?

	// Store max size of vector (and preserve across resets)
	m_snMaxInteresting = nMaxInteresting;
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::AddDebugTag( EntityId id, const char* pStr, float fTime )
{
	if (IsDebuggingEnabled())	// Extra check may help avoid spam and is cheap compared to string stuff
	{
		//if (fTime < 0.0f) m_pPersistantDebug->AddEntityTagSimple( id, pStr );
		//else m_pPersistantDebug->AddEntityTag( id, pStr, 1.5f, ColorF(1, 1, 1, 1), fTime);
		string text;
		text.Format("Interest: [%s]: %s",gEnv->pEntitySystem->GetEntity(id)->GetName(), pStr);
		gEnv->pLog->Log(text);
	}
}


//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::RegisterObject(IEntity * pEntity, float fInterest)
{




}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::DeregisterObject(IEntity * pEntity)
{
	// Find this entity
	std::vector<SEntityInterest>::iterator itE;
	for (itE = m_InterestingEntities.begin(); itE != m_InterestingEntities.end(); itE++)
	{
		if (itE->pEntity == pEntity) break;
	}
	if (itE == m_InterestingEntities.end()) return; // Wasn't registered

	m_InterestingEntities.erase(itE);

	// Remove from quick check table
	m_RegisteredIntEnts.erase(pEntity);

	// Tell the PIMs about it
	std::vector<CPersonalInterestManager*>::iterator itPIM;
	for (itPIM = m_PIMs.begin(); itPIM != m_PIMs.end(); itPIM++)
	{
		if ((*itPIM)->GetAssigned()) 
			(*itPIM)->ForgetInterestEntity(pEntity);
	}

	// Debugging
	AddDebugTag(pEntity->GetId(), "Deregistered");
}

//------------------------------------------------------------------------------------------------------------------------

// Clean up the vectors
void CCentralInterestManager::Cleanup(void)
{
	m_PotentialAIActors.resize(0);									// That is to say, no cleanup is necessary

	for (int i=0; i<m_PIMs.size(); i++)
	{
		CAIActor *pAIActor = m_PIMs[i]->GetAssigned();
		if (pAIActor)  // Clean up reference from the actor to soon-to-be-invalid pointer
		{
			pAIActor->AssignInterestManager(NULL);
		}
		delete m_PIMs[i];	// Should clean up self
	}
	m_PIMs.resize(0);

	m_InterestingEntities.resize(0);								// That is to say, no cleanup is necessary			
	m_RegisteredIntEnts.clear();
}

//------------------------------------------------------------------------------------------------------------------------

void CCentralInterestManager::OnEvent( IEntity *pEntity, SEntityEvent &event )
{
	// Most of this method is to filter out things we're not interested in with minimal performance impact
	// Thus many if...returns and then finally we may do something

	if (event.event != ENTITY_EVENT_XFORM) return;

	if (m_cvInterestListenMovement->GetIVal() != 1) 
		return;

	// Get distances. Change to use bbox?
	Vec3 vEntPos = pEntity->GetPos();
	float fDistSq = vEntPos.GetSquaredDistance2D(m_vViewerPos); // We specifically use 2D because height is usually better ignored
	
	// If too far away, ignore
	if (fDistSq > m_fMaxDistSq) return;

	// We're only interested in entities with a physical presence
	IEntityPhysicalProxy *pPhysProxy = (IEntityPhysicalProxy*) pEntity->GetProxy(ENTITY_PROXY_PHYSICS);
	IPhysicalEntity* pPhysEnt = (pPhysProxy ? pPhysProxy->GetPhysicalEntity() : NULL);
	if (!pPhysEnt) return;

	// And we're only interested in those that are capable of a velocity
	pe_status_dynamics dyn_status;
	pPhysEnt->GetStatus(&dyn_status);

	// Get the _squared_ speed
	float fSpeedSq = dyn_status.v.GetLengthSquared();

	if (fSpeedSq == 0.0f) // For things which don't track their velocity but are dynamic, I think this is a safe check
	{
		const char * sClassName = pEntity->GetClass()->GetName();

		pe_status_vehicle vehicle_status;
		if (pPhysEnt->GetStatus(&vehicle_status))
		{
			fSpeedSq = vehicle_status.vel.GetLengthSquared();
		}
		else if (strcmp(sClassName, "Boid")==0)
		{
			fSpeedSq = square(m_cvInterestScalingEyeCatching->GetFVal());			
		}
	}

	if (fSpeedSq < 1.0f) 
		return; // Very slow, settling object? We're unlikely to notice this.

	//string text;
	//text.Format("Speed: %f",fSpeed);
	//m_pPersistantDebug->AddEntityTagSimple( pEntity->GetId(), text );

	// Check if we've already registered this (nasty)
	if (m_RegisteredIntEnts.find(pEntity) != m_RegisteredIntEnts.end()) return;

	// Register only if new
	RegisterInterestingEntity( pEntity, sqrtf(fSpeedSq) );

}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::OnRemove( IEntity *pEntity )
{
	DeregisterObject(pEntity);

	return (true);
}

//------------------------------------------------------------------------------------------------------------------------

bool CCentralInterestManager::MayFireRay( CPersonalInterestManager *pPIM )
{
	if ( m_nRaysFiredInUpdate < CIM_RAYBUDGET )
	{
		m_nRaysFiredInUpdate++;
		return true;
	}
	return false;
}