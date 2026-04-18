/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   AIActor.h
$Id$
Description: Header for the CAIActor class

-------------------------------------------------------------------------
History:
14:12:2006 -  Created by Kirill Bulatsev



*********************************************************************/
//
//////////////////////////////////////////////////////////////////////

#ifndef AIACTOR_H
#define AIACTOR_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IAgent.h"
#include "AIObject.h"
#include "AStarSolver.h"
#include "IPhysics.h"

#define MAX_RESTRICTED_SPECIES 3

// forward declarations
struct GraphNode;
class CAISystem;
class CFormation;
class COPWaitSignal;
class CPersonalInterestManager;
class IPersonalInterestManager;

enum EObjectUpdate
{
	AIUPDATE_FULL,
	AIUPDATE_DRY,
};

// Structure reflecting the physical entity parts.
// When choosing the target location to hit, the AI chooses amongst one of these.
struct SAIDamagePart
{
	SAIDamagePart() : pos(0,0,0), damageMult(0.0f), volume(0.0f), surfaceIdx(0) {}
	Vec3	pos;				// Position of the part.
	float	damageMult;	// Damage multiplier of the part, can be used to choose the most damage causing part.
	float	volume;			// The volume of the part, can be used to choose the largest part to hit.
	int		surfaceIdx;	// The index of the surface material.
};

typedef std::vector<SAIDamagePart>	DamagePartVector;

// Simple class to track a history of float values.
class SAIValueHistory
{
public:
	SAIValueHistory(unsigned s, float sampleIterval) : sampleIterval(sampleIterval), head(0), size(0), t(0) { data.resize(s); }

	inline void Reset()
	{
		size = 0; head = 0;
		v = 0; t = 0;
	}

	inline void Sample(float nv, float dt)
	{ 
		t += dt;
		v = max(v, nv);

		int iter = 0;
		while(t > sampleIterval && iter < 5)
		{
			data[head] = v;
			head = (head+1) % data.size();
			if(size < data.size()) size++;
			++iter;
			t -= sampleIterval;
		}
		if(iter == 5)
			t = 0;
		v = 0;
	}

	inline unsigned GetSampleCount() const
	{
		return size;
	}

	inline unsigned GetMaxSampleCount() const
	{
		return data.size();
	}

	inline float GetSampleInterval() const
	{
		return sampleIterval;
	}

	inline float GetSample(unsigned i) const
	{
		const unsigned n = data.size();
		return data[(head + (n - 1 - i)) % n];
	}

	inline float GetMaxSampleValue() const
	{
		float maxVal = -FLT_MAX;
		const unsigned n = data.size();
		for (unsigned i = 0; i < size; ++i)
			maxVal = max(maxVal, data[(head + (n - 1 - i)) % n]);
		return maxVal;
	};

private:
	std::vector<float> data;
	unsigned head, size;
	float v, t;
	const float sampleIterval;
};




/*! Basic ai object class. Defines a framework that all puppets and points of interest
	later follow.
*/
class CAIActor 
	: public CAIObject
	, public IAIActor
{
public:
  CAIActor(CAISystem*);
  virtual ~CAIActor();

  virtual const IAIActor* CastToIAIActor() const { return this; }
  virtual IAIActor* CastToIAIActor() { return this; }

  //===================================================================
  // inherited virtual interface functions
  //===================================================================
  virtual void Reset(EObjectResetType type);
//	virtual IUnknownProxy* GetProxy() const { return m_pProxy; };
	virtual void OnObjectRemoved(CAIObject *pObject );
  virtual SOBJECTSTATE * GetState(void) {return &m_State;}
  virtual const SOBJECTSTATE * GetState(void) const {return &m_State;}
	virtual void SetSignal(int nSignalID, const char * szText, IEntity *pSender=0, IAISignalExtraData* pData=NULL, uint32 crcCode = 0);
	virtual void NotifySignalReceived(const char* szText, IAISignalExtraData* pData=NULL );
	virtual void Serialize( TSerialize ser, class CObjectTracker& objectTracker );
	virtual void Update(EObjectUpdate type);
	virtual void UpdateDisabled(EObjectUpdate type);	// when AI object is disabled still may need to send some signals  
	virtual IUnknownProxy* GetProxy() const { return m_pProxy; };
	virtual bool CanAcquireTarget(IAIObject* pOther) const;
	virtual bool IsHostile(const IAIObject* pOther, bool bUsingAIIgnorePlayer=true) const;
	virtual void ParseParameters( const AIObjectParameters &params);
	virtual void Event(unsigned short eType, SAIEVENT *pEvent);
	virtual void SetGroupId(int id);

	//===================================================================
	// virtual functions rooted here
	//===================================================================

	/// Should populate the list of entities that we don't want to consider when pathfinding
	virtual void UpdateEntitiesToSkipInPathfinding() {}
	const std::vector<IPhysicalEntity *> &GetEntitiesToSkipInPathfinding() const {return m_entitiesToSkipInPathFinding;}
	/// This should add any navigation blockers to the list that are specific to the 
	/// AI object (e.g. locations it wishes to avoid when pathfinding).
	/// pfr may be 0. if it's non-zero then the nav blockers can be tailored to suit
	/// the specific path
	virtual void AddNavigationBlockers(NavigationBlockers& navigationBlockers, const struct PathFindRequest *pfr) const {}
	// Returns a list containing the information about the parts that can be shot at.
	virtual DamagePartVector*	GetDamageParts() { return 0; }
	void SetProxy(IUnknownProxy* pProxy) { m_pProxy = pProxy; };
	const AgentParameters &GetParameters() const { return m_Parameters;}
	virtual void SetParameters(AgentParameters &params);
	virtual const AgentMovementAbility& GetMovementAbility() const {return m_movementAbility;}
	virtual void SetMovementAbility(AgentMovementAbility &params) {m_movementAbility = params;}
	virtual bool IsLowHealthPauseActive() const { return false; }
	virtual IEntity* GetGrabbedEntity() const {return 0;}	// consider only player grabbing things, don't care about NPC

	//===================================================================
	// non-virtual functions
	//===================================================================

	// Get the personal interest manager for this AI, if it currently exists
	IPersonalInterestManager* GetInterestManager(void) { return (IPersonalInterestManager*) m_pPersonalInterestManager; }
	// Assign an interest manager to this AI.
	// NULL is fine; replacing an existing PIM works as if you had first called with NULL
	void AssignInterestManager( CPersonalInterestManager * pPIM );

	// Returns true if the agent cannot be seen from the specified point.
	bool IsInvisibleFrom(const Vec3& pos) const;

	// Returns true if the cloak is effective (includes check for cloak and 
	bool IsCloakEffective() const;
	// returns current min/max cloak distances (those are fading in/out)
	float GetCloakMinDist( ) const;
	float GetCloakMaxDist( ) const;

	float	GetSightRange(const CAIObject* pTarget) const;

	// Gets the light level at actors position.
	inline EAILightLevel GetLightLevel() const { return m_lightLevel; }

	// Populates list of physics entities to skip for raycasting.
	virtual void GetPhysicsEntitiesToSkip(std::vector<IPhysicalEntity*>& skips) const;

	inline void RegisterDetectionLevels(const SAIDetectionLevels& levels) { m_detectionMax.Append(levels); }
	inline void ResetDetectionLevels() { m_detectionMax.Reset(); }
	inline const SAIDetectionLevels& GetDetectionLevels() const { return m_detectionSnapshot; }
	inline void SnapshotDetectionLevels() { m_detectionSnapshot = m_detectionMax; }

	SOBJECTSTATE		m_State;
	AgentParameters	m_Parameters;
	AgentMovementAbility	m_movementAbility;

	// list of wait goal operations to be notified when a signal is received
	typedef std::list< COPWaitSignal* > ListWaitGoalOps;
	ListWaitGoalOps m_listWaitGoalOps;

	SAIValueHistory* m_healthHistory;

	CPersonalInterestManager * m_pPersonalInterestManager;

	// WIP

	struct SProbableTarget
	{
		SProbableTarget(CAIObject* pTarget, int los = -1) :
			pTarget(pTarget), lineOfSight(los), updates(0), dirty(false)
		{
			// Empty
		}

		CAIObject* pTarget;
		int lineOfSight;
		int updates;
		bool dirty;
	};

	std::vector<SProbableTarget>	m_probableTargets;
	std::vector<SProbableTarget>	m_priorityTargets;


	void	AddProbableTarget(CAIObject* pTarget)
	{
		for (unsigned i = 0, ni = m_probableTargets.size(); i < ni; ++i)
		{
			if (m_probableTargets[i].pTarget == pTarget)
			{
				m_probableTargets[i].dirty = false;
				return;
			}
		}
		m_probableTargets.push_back(SProbableTarget(pTarget));
	}

	void	ClearProbableTargets()
	{
		for (unsigned i = 0, ni = m_probableTargets.size(); i < ni; ++i)
			m_probableTargets[i].dirty = true;
	}

	int	HasLineOfSightTo(CAIObject* pTarget)
	{
		for (unsigned i = 0, ni = m_probableTargets.size(); i < ni; ++i)
		{
			const SProbableTarget& prob = m_probableTargets[i];
			if (prob.pTarget == pTarget)
			{
				if (prob.dirty || prob.updates > 3)
					return -1;
				return prob.lineOfSight;
			}
		}
		return -1;
	}

	void	UpdateProbableTargets()
	{
		for (unsigned i = 0; i < m_probableTargets.size(); )
		{
			m_probableTargets[i].updates++;
			if (m_probableTargets[i].dirty)
			{
				m_probableTargets[i] = m_probableTargets.back();
				m_probableTargets.pop_back();
			}
			else
				++i;
		}
	}
	
	void	UpdatePriorityTargets()
	{
		for (unsigned i = 0; i < m_priorityTargets.size(); )
		{
			m_priorityTargets[i].updates++;
			if (m_priorityTargets[i].updates > 2)
			{
				m_priorityTargets[i] = m_priorityTargets.back();
				m_priorityTargets.pop_back();
			}
			else
				++i;
		}
	}

	void	RefreshPriorityTarget(CAIObject* pTarget, int los)
	{
		for (unsigned i = 0, ni = m_priorityTargets.size(); i < ni; ++i)
		{
			if (m_priorityTargets[i].pTarget == pTarget)
			{
				m_priorityTargets[i].lineOfSight = los;
				m_priorityTargets[i].updates = 0;
				return;
			}
		}
		m_priorityTargets.push_back(SProbableTarget(pTarget, los));
	}

	virtual bool	IsActive() const { return false; }

	inline bool IsUsingCombatLight() const { return m_usingCombatLight; }

	float GetHostilityRangeSqr(int species) const;
	void IncreaseHostilityRangeSqr(int species, float rangesqr);
	void SetupHostilityRange(int species, float min_range, float max_range, float degen, float increase, float cooldown);

	void IgnoreSpecies(int species, bool ignore);
protected:

	void UpdateHealthHistory();
	//process cloak_scale fading
	void UpdateCloakScale();

	// Updates the properties of the damage parts.
	void	UpdateDamageParts(DamagePartVector& parts);
	
	void UpdateHostilityRange();
	bool IsSpeciesRelevant(int species) const;

	IUnknownProxy *m_pProxy;

	/// entities that should not affect pathfinding - e.g. if we're carrying a physical object, or we
	/// know our destination is inside a specific entity.
	std::vector<IPhysicalEntity *> m_entitiesToSkipInPathFinding;

	EAILightLevel m_lightLevel;
	bool m_usingCombatLight;
	SAIDetectionLevels	m_detectionMax;
	SAIDetectionLevels	m_detectionSnapshot;

	float m_hostilityRestrictions [MAX_RESTRICTED_SPECIES];
	float m_minHostileRange;
	float m_maxHostileRange;
	float m_degenHostileRange;
	float m_hostilityIncrease;
	CTimeValue m_hostilityCooldown;

	CTimeValue m_lastHostilityTime;

	bool m_ignoreSpecies [MAX_RESTRICTED_SPECIES];
private:
};

inline const CAIActor* CastToCAIActorSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCAIActor() : 0; }
inline CAIActor* CastToCAIActorSafe(IAIObject* pAI) { return pAI ? pAI->CastToCAIActor() : 0; }

#endif 
