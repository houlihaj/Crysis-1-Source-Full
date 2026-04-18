/********************************************************************
Crytek Source File.
Copyright  , Crytek Studios, 2006-2007.
---------------------------------------------------------------------
File name:   CentralInterestManager.cpp
$Id$
$DateTime$
Description: Global manager that distributes interest events and
             allows personal managers to work together
						 Effectively, this is the interface to the Interest System
---------------------------------------------------------------------
History:
- 06:03:2007 : Created by Matthew Jack

*********************************************************************/

#ifndef __CentralInterestManager_H__
#define __CentralInterestManager_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "IInterestSystem.h"

// Forward declarations
struct IPersistantDebug;
class CPersonalInterestManager;

// Basic structure for storing which objects are interesting and how much
struct SEntityInterest
{
	SEntityInterest(void) : pEntity(NULL), fInterest(0.0f) {}
	IEntity *pEntity;
	float fInterest;
};



// There need only ever be one of these in game, hence a singleton
// Creates on first request, which is lightweight

class CCentralInterestManager : 
	public ICentralInterestManager,
	public IEntitySystemSink
{
public:
	// Get the CIM singleton
	static CCentralInterestManager* GetInstance(void);

	// Reset the CIM system
	// Do this when all caches should be emptied, e.g. on loading levels
	// Resetting during game should do no serious harm
	void Reset();

	// TODO: Remove this?
	// Enable or disable (suppress) the interest system
	void Enable(bool bEnable);

	// Is the Interest System enabled?
	bool IsEnabled(void) { return m_bEnabled; }

	bool IsDebuggingEnabled(void) { return (m_cvDebugInterest ? m_cvDebugInterest->GetIVal() != 0 : false); }	

	// Update the CIM
	void Update( float fDelta );

	// Register/update an interesting entity
	// Returns true if this was accepted as a useful interest target
	// If the object is already registered, it will be updated and still return true
	bool RegisterInterestingEntity( IEntity *pEntity, float fBaseInterest );

	// Register a potential interested AIActor
	// Returns true if this was accepted as a potentially useful interested entity
	// If the object is already registered, it will be updated and still return true
	bool RegisterInterestedAIActor( IEntity *pEntity, float fForceUntilDistance );

	// Set the maximum number of AIActors that we will consider simultaneously
	// Shouldn't change often, so throws out actors without prioritisation
	void SetMaxNumPotentialInterestedAIActor(int nMaxPotentialAIActors);

	// Set the maximum number of AIActors that can have a PIM at once, for performance control
	// Shouldn't change often, so throws out PIMs without prioritisation
	void SetMaxNumInterested(int nMaxPIMs);

	// Set the maximum number of registered interesting objects, for performance control
	// This does prioritise when the number is reduced
	void SetMaxNumInteresting(int nMaxInteresting );

	// Central request point to limit ray casts in the interest system
	bool MayFireRay( CPersonalInterestManager *pPIM );

	// Central shared debugging function, should really be a private/friend of PIM
	void AddDebugTag( EntityId id, const char *pStr, float fTime = -1.0f);

	//-------------------------------------------------------------
	// IEntitySystemSink methods, for listening to moving entities
	//-------------------------------------------------------------
	bool OnBeforeSpawn( SEntitySpawnParams &params ) { return true; };
	void OnSpawn( IEntity *pEntity,SEntitySpawnParams &params ) {};
	bool OnRemove( IEntity *pEntity );
	void OnEvent( IEntity *pEntity, SEntityEvent &event );

private:
	// Construct/destruct
	CCentralInterestManager(void);
	~CCentralInterestManager(void);

	// Accept objects for PIMs to consider
	void RegisterObject(IEntity * pEntity, float fInterest);
	void DeregisterObject(IEntity * pEntity);

	// CIM init/deinit
	void Init(void);																		// Initialise as defaults
	void CreateScanTable(void);
	void Cleanup(void);																	// Deallocate all

	// Store setup values, preserved across resets
	static int m_snMaxPotentialAIActors;				
	static int m_snMaxPIMs;
	static int m_snMaxInteresting;

	// Basic
	static CCentralInterestManager * m_pCIMInstance;		// The singleton
	bool m_bEnabled;																		// Toggle Interest system on/off

	// CVars
	ICVar *m_cvDebugInterest;
	ICVar *m_cvInterestListenMovement;
	ICVar *m_cvInterestScalingEyeCatching;
	ICVar *m_cvInterestScalingAmbient;
	
	IPersistantDebug* m_pPersistantDebug;								// The persistant debugging framework
	bool m_bEntityEventListenerInstalled;								// Have we hooked into the entity system yet?

	// The tracking lists
	std::vector<CAIActor*> m_PotentialAIActors;					// The actors we might _consider_ assigning a PIM. Some can be NULL
	std::vector<CPersonalInterestManager*> m_PIMs;			// Instantiated PIMs, no NULL, some can be unassigned
	std::vector<SEntityInterest> m_InterestingEntities;	// Interesting objects we might consider pointing out to PIMs

	// Caching basic relevancy criteria
	Vec3 m_vViewerPos;																	// Where is our viewer? (Concentrate efforts where will be seen)
	float m_fMaxDistSq;																	// What is the maximum distance we will consider?
	AABB m_AABB;																				// Bounding box for interesting objects
	std::set<IEntity*> m_RegisteredIntEnts;							// Nasty hack for basic optimisation IMHO

	// Performance
	int m_nRaysFiredInUpdate;														// Keep track of ray count so far this update - usually 0
};





#endif __CentralInterestManager_H__







