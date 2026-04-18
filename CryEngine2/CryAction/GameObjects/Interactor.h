#ifndef __INTERACTOR_H__
#define __INTERACTOR_H__

#pragma once

#include "IGameObject.h"
#include "IInteractor.h"

class CWorldQuery;
struct IEntitySystem;

class CInteractor : public CGameObjectExtensionHelper<CInteractor, IInteractor>
{
public:
	CInteractor();
	~CInteractor();

	// IGameObjectExtension
	virtual bool Init( IGameObject * pGameObject );
	virtual void InitClient(int channelId) {};
	virtual void PostInit( IGameObject * pGameObject );
	virtual void PostInitClient(int channelId) {};
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual void PostSerialize();
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext& ctx, int slot );
	virtual void HandleEvent( const SGameObjectEvent& );
	virtual void ProcessEvent(SEntityEvent& ) {};
	virtual void SetChannelId(uint16 id) {};
	virtual void SetAuthority(bool auth) {}
	virtual void PostUpdate(float frameTime) { assert(false); }
	virtual void PostRemoteSpawn() {};
	virtual void GetMemoryStatistics(ICrySizer * s);
	// ~IGameObjectExtension


	// IInteractor
	virtual bool IsUsable(EntityId entityId) const;
	virtual bool IsLocked() const { return m_lockEntityId!=0; };
	virtual int GetLockIdx() const { return m_lockIdx; };
	virtual int GetLockedEntityId() const { return m_lockEntityId; };
	//~IInteractor

private:
	CWorldQuery * m_pQuery;

	float m_useHoverTime;
	float m_unUseHoverTime;
	float m_messageHoverTime;
	float m_longHoverTime;

	ITimer * m_pTimer;
	IEntitySystem * m_pEntitySystem;

	string m_queryMethods;

	SmartScriptTable m_pGameRules;
	HSCRIPTFUNCTION m_funcIsUsable;
	HSCRIPTFUNCTION m_funcOnNewUsable;
	HSCRIPTFUNCTION m_funcOnUsableMessage;
	HSCRIPTFUNCTION m_funcOnLongHover;

	EntityId m_lockedByEntityId;
	EntityId m_lockEntityId;
	int m_lockIdx;

	EntityId m_nextOverId;
	int m_nextOverIdx;
	CTimeValue m_nextOverTime;
	EntityId m_overId;
	int m_overIdx;
	CTimeValue m_overTime;
	bool m_sentMessageHover;
	bool m_sentLongHover;

	float m_lastRadius;

	struct SQueryResult
	{
		SQueryResult() : entityId(0), slotIdx(0), hitPosition(0,0,0), hitDistance(0) {}
		EntityId entityId;
		int slotIdx;
		Vec3 hitPosition;
		float hitDistance;
	};

	static ScriptAnyValue EntityIdToScript( EntityId id );
	void PerformQueries( EntityId& id, int& idx );
	void UpdateTimers( EntityId id, int idx );

	bool PerformRayCastQuery( SQueryResult& r );
	bool PerformBoundingBoxQuery( SQueryResult& r );
	bool PerformUsableTestAndCompleteIds( IEntity * pEntity, SQueryResult& r ) const;
};

#endif
