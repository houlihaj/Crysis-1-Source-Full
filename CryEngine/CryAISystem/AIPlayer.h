#ifndef _AI_PLAYER_
#define _AI_PLAYER_

#if _MSC_VER > 1000
#pragma once
#endif

#include "AIActor.h"
#include "TimeValue.h"
#include "ObjectTracker.h"
#include "AIDbgRecorder.h"

class DynamixTracker
{
public:	

	DynamixTracker();
	void	SetOwner(CAIObject* pOwner);
	void	Update();
	bool	IsMoving() const;
	float	GetSpeed() const {return m_AvrgVelocity.GetLength();}
	void	Serialize(TSerialize ser,CObjectTracker& objectTracker)
	{
		ser.BeginGroup("DynamixTracker");
		ser.Value("m_AvrgVelocity",m_AvrgVelocity);
		ser.Value("m_LastUpdateTime",m_LastUpdateTime);
		objectTracker.SerializeValueContainer(ser,"m_Positions",m_Positions);
		objectTracker.SerializeObjectPointer(ser,"m_pOwner",m_pOwner,false);
		ser.EndGroup();
	}

protected:
	typedef std::list<Vec3, stl::STLPoolAllocator<Vec3, stl::PoolAllocatorSynchronizationSinglethreaded> >	t_PositionsList;

	static const int	m_BufferSize=16;
	t_PositionsList	m_Positions;
	Vec3						m_AvrgVelocity;
	CTimeValue			m_LastUpdateTime;
	CAIObject*			m_pOwner;
};


class CAIPlayer :	public CAIActor, CRecordable
{
	CAIObject* m_pAttentionTarget;
	CTimeValue m_fLastUpdateTargetTime;
	float	m_FOV;
	DynamixTracker	m_Tracker;
	DamagePartVector	m_damageParts;
	int	m_deathCount;

	struct SThrownItem
	{
		SThrownItem(EntityId id) : id(id), time(0.0f), pos(0,0,0), vel(0,0,0), r(0.1f) {}
		inline bool operator<(const SThrownItem& rhs) const { return time < rhs.time; }
		float time;
		Vec3 pos, vel;
		float r;
		EntityId id;
	};
	std::vector<SThrownItem>	m_lastThrownItems;

	struct SStuntTargetPuppet
	{
		SStuntTargetPuppet(CPuppet* pPuppet, const Vec3& pos) : pPuppet(pPuppet), t(0), exposed(0), signalled(false), threatPos(pos) {}
		CPuppet* pPuppet;
		Vec3 threatPos;
		float t;
		float exposed;
		bool signalled;
	};
	std::vector<SStuntTargetPuppet> m_stuntTargets;

	void AddThrownEntity(EntityId id);

	void UpdatePlayerStuntActions();
	void HandleCloaking();

	Vec3 m_stuntDir;

	float m_playerStuntSprinting;
	float m_playerStuntJumping;
	float m_playerStuntCloaking;
	float m_playerStuntUncloaking;

	float m_mercyTimer;

	void CollectExposedCover();
	void ReleaseExposedCoverObjects();
	void AddExposedCoverObject(IPhysicalEntity* pPhysEnt);

	float m_coverExposedTime;
	struct SExposedCoverObject
	{
		SExposedCoverObject(IPhysicalEntity* pPhysEnt, float t) : pPhysEnt(pPhysEnt), t(t) {}
		IPhysicalEntity* pPhysEnt;
		float t;
	};
	std::vector<SExposedCoverObject>	m_exposedCoverObjects;

public:
	CAIPlayer(CAISystem*);
	~CAIPlayer();

	void ParseParameters(const AIObjectParameters & params);
	void Reset(EObjectResetType type);
	
	CAIObject* GetAttentionTarget() const {return m_pAttentionTarget;}
	void	UpdateAttentionTarget(CAIObject* pTarget) ;
	virtual bool IsPointInFOVCone(const Vec3& pos, float distanceScale);
	void	Update(EObjectUpdate type);
	virtual void Serialize(TSerialize ser, class CObjectTracker& objectTracker );
	virtual void OnObjectRemoved(CAIObject *pObject);
	virtual bool IsLowHealthPauseActive() const;
	virtual IEntity* GetGrabbedEntity() const;// consider only player grabbing things, don't care about NPC

	// Inherited
	virtual bool	IsAgent() const { return true; }
	// Inherited
	virtual DamagePartVector*	GetDamageParts();

	virtual void Event(unsigned short eType, SAIEVENT *pEvent);

	virtual IPhysicalEntity* GetPhysics(bool wantCharacterPhysics) const;

	const DynamixTracker& GetDynamixTracker() const {return m_Tracker;} 

	int	GetDeathCount() { return m_deathCount; }
	void	IncDeathCount() { m_deathCount++; }

	virtual void	RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData=NULL);
	virtual void	RecordSnapshot();
	virtual IAIDebugRecord* GetAIDebugRecord();

	bool IsThrownByPlayer(EntityId ent) const;
	bool IsPlayerStuntAffectingTheDeathOf(CAIActor* pDeadActor) const;
	EntityId GetNearestThrownEntity(const Vec3& pos);
	bool IsDoingStuntActionRelatedTo(const Vec3& pos, float nearDistance);

	virtual bool	IsActive() const { return m_bEnabled; }

	virtual void GetPhysicsEntitiesToSkip(std::vector<IPhysicalEntity*>& skips) const;

	void DebugDraw(IRenderer *pRenderer);

	//virtual void SetSignal(int nSignalID, const char * szText, void *pSender=0, IAISignalExtraData *pData=NULL);
};

inline const CAIPlayer* CastToCAIPlayerSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCAIPlayer() : 0; }
inline CAIPlayer* CastToCAIPlayerSafe(IAIObject* pAI) { return pAI ? pAI->CastToCAIPlayer() : 0; }

#endif
