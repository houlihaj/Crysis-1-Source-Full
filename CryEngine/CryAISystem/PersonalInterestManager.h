/********************************************************************
Crytek Source File.
Copyright  , Crytek Studios, 2006-2007.
---------------------------------------------------------------------
File name:   PersonalInterestManager.h
$Id$
$DateTime$
Description: Interest Manager (tracker) for interested individuals

Note:        PIMs are owned by the CentralInterestManager - delete there
---------------------------------------------------------------------
History:
- 07:03:2007 : Created by Matthew Jack

*********************************************************************/

#ifndef __PersonalInterestManager_H__
#define __PersonalInterestManager_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "CentralInterestManager.h"

#define PIM_SCANPOINTS 16

enum EInterestType { eIT_None, eIT_Entity, eIT_Dummy };

struct SPersonalEntityInterest : public SEntityInterest
{
	SPersonalEntityInterest(float fInt, EInterestType type) 
	{
		fInterest = 50.0f;
		pEntity = NULL;
		fAccumulated = 0.0f;
		eType = type;
	}
	SPersonalEntityInterest(const SEntityInterest &entInterest)
	{
		fInterest = entInterest.fInterest;
		pEntity = entInterest.pEntity;
		fAccumulated = 0.0f;
		eType = eIT_Entity;
	}
	float fAccumulated;
	EInterestType eType;
};

class CPersonalInterestManager : public IPersonalInterestManager
{
public:
	CPersonalInterestManager(void);
	~CPersonalInterestManager(void);

	// Clear tracking cache, don't clear assignment
	void Reset(void);

	// Re(assign) to an actor (helps in object re-use)
	// NULL is acceptable, to leave unassigned
	// You must also ensure the PIM pointer in the CAIActor is set to this object
	void Assign( CAIActor* pAIActor );

	// Get the currently assigned AIActor
	CAIActor *GetAssigned(void);

	// Get/set minimum relevant interest level
	void SetMinimumInterest( float fMinInterest );
	float GetMinimumInterest( void ) { return m_fMinInterest; }

	// Update
	void Update( float fDelta );

	// Consider this interesting object
	// May store it in the tracking cache
	// Returns the initial interest evaluation
	float ConsiderInterestEntity ( const SEntityInterest &entInterest );

	// Stop considering the object, usually because it has been deleted
	void ForgetInterestEntity( IEntity *pEntity );

	// Do we have any interest target right now?
	// Allows us to move on as quickly as possible for the common (uninterested) case
	bool IsInterested(void) { return m_eInterestType != eIT_None; }

	// Get the position we should be looking at
	// Undefined if uninterested
	Vec3 GetInterestPoint(void);

	// Which entity are we currently interested in?
	// Returns NULL if not currently interested in anything, or that target is not an entity
	IEntity * GetInterestEntity(void);

	// Returns the dummy interest object if currently interested or NULL
	// Works regardless of the type of the interesting object
	CAIObject * GetInterestDummyPoint(void);

	// Add methods for inter-interest communication


protected:
																												// Can the actor see this point?
	bool CheckVisibilityOfPoint( const Vec3 &point, IPhysicalEntity * pTargetPhysEnt = NULL );																												
	bool CheckVisibilityOfPoint( IEntity * pEntity );			// Can the actor see this entity?
																												// Track the interest values
	void TrackInterest( SPersonalEntityInterest ** const ppOldPEI, SPersonalEntityInterest ** const ppBestPEI, float fDelta ); 
																												// If visible, switch to this new interest (true iff switched)
	bool ChangeInterest( SPersonalEntityInterest * const ppOldPEI, SPersonalEntityInterest * const ppBestPEI );
	bool IsPointAcceptable( const Vec3 &vPoint);					// Is this point within an acceptable viewing range?
	Vec3 ConvertSeedToPoint( unsigned int seed );					// Take a seed and map it to the finite set of look points
	bool StartScan(void);																	// Begin a scan arc
	void UpdateScan(float fDelta);												// Continue a scan arc
	void EndScan(void);																		// End scanning

																												
	CAIActor * m_pAIActor;																// The actor we are responsible for
	float m_fMinInterest;																	// The minimum interest level currently relevant to that actor
	float m_fThresholdInterest;														// The varying threshold that interest much reach
	std::vector<SPersonalEntityInterest> m_Tracker;				// Tracking interest levels in objects
	EInterestType m_eInterestType;												// Do we currently have an interest target?
	IEntity *m_InterestingEntity;													// Entity we're currently interested in, if any
	CAIObject *m_pInterestDummy;													// Dummy that moves around with any interest target
	CCentralInterestManager *m_pCIM;											// Cached pointer to CIM

	// Look IK state
	Vec3 m_prevLookPoint;																	// The last point we looked at (whatever type)
	Vec3 m_newLookPoint;																	// The new points to look at (whatever type)
	float m_fLookBlend;																		// Blend value progressing from 0.0-1.0 as we look at a new thing
	float m_fBlendTime;																		// Current time a lookBlend can take

	// Scan point state
	int m_scanPointSeeds[PIM_SCANPOINTS];									// The scanpoints we have currently approved

	// Console variable caching
	// No need for everyone to have their own copy of all these
	static ICVar *m_cvEnableInterestScan;
	static ICVar *m_cvInterestEntryBoost;
	static ICVar *m_cvInterestSwitchBoost;
	static ICVar *m_cvInterestSpeedMultiplier;
	static ICVar *m_cvInterestScalingScan;
	static ICVar *m_cvInterestScalingMovement;
	static ICVar *m_cvInterestScalingView;
};


#endif // __PersonalInterestManager_H__