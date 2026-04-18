/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Manages and controls player spawns.
  
 -------------------------------------------------------------------------
  History:
  - 23:8:2004   15:52 : Created by Márcio Martins

*************************************************************************/
#ifndef __ACTORSYSTEM_H__
#define __ACTORSYSTEM_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IActorSystem.h"
#include "IGameFramework.h"

typedef std::map<string, IGameFramework::IActorCreator*>			TActorClassMap;
typedef std::map<EntityId, IActor *>		TActorMap;

struct IScriptTable;
class CPlayerEntityProxy;

class CActorSystem :
	public IActorSystem
{
	struct DemoSpectatorSystem
	{
		EntityId				m_originalActor;
		EntityId				m_currentActor;

		void SwitchSpectator(TActorMap *pActors, EntityId id = 0);

	} m_demoSpectatorSystem;

	EntityId m_demoPlaybackMappedOriginalServerPlayer;

public:
	CActorSystem(ISystem *pSystem, IEntitySystem *pEntitySystem);
	virtual ~CActorSystem();

	void Release() { delete this; };

	virtual void Reset();

	// IActorSystem
	virtual IActor *GetActor(EntityId entityId);
	virtual IActor *GetActorByChannelId(uint16 channelId);
	virtual IActor *CreateActor(uint16 channelId, const char *name, const char *actorClass, const Vec3 &pos, const Quat &rot, const Vec3 &scale, EntityId id);

	virtual int GetActorCount() const { return (int)m_actors.size(); };	
	virtual IActorIteratorPtr CreateActorIterator();

	virtual void SetDemoPlaybackMappedOriginalServerPlayer(EntityId id);
	virtual EntityId GetDemoPlaybackMappedOriginalServerPlayer() const;
	virtual void SwitchDemoSpectator(EntityId id = 0);
	virtual IActor *GetCurrentDemoSpectator();
	virtual IActor *GetOriginalDemoSpectator();
	// ~IActorSystem

	void RegisterActorClass(const char *name, IGameFramework::IActorCreator *, bool isAI);
	void AddActor(EntityId entityId, IActor *pProxy);
	void RemoveActor(EntityId entityId);

	void GetMemoryStatistics(ICrySizer * s);

private:
//	static IEntityProxy *CreateActor(IEntity *pEntity, SEntitySpawnParams &params, void *pUserData);
	static bool HookCreateActor( IEntity *, IGameObject *, void * );

	struct SSpawnUserData
	{
		SSpawnUserData( const char * cls, uint16 channel ) : className(cls), channelId(channel) {}
		const char * className;
		uint16 channelId;
	};

	struct SActorUserData
	{
		SActorUserData(const char *cls, CActorSystem *pNewActorSystem): className(cls), pActorSystem(pNewActorSystem) {};
		CActorSystem *pActorSystem;
		string className;
	};

	class CActorIterator : public IActorIterator
	{
	public:
		CActorIterator(CActorSystem* pAS)
		{
			m_pAS = pAS;
			m_cur = m_pAS->m_actors.begin();
			m_nRefs = 0;
		}
		void AddRef()
		{
			++m_nRefs;
		}
		void Release()
		{
			if (--m_nRefs <= 0)
			{
				assert (std::find (m_pAS->m_iteratorPool.begin(),
					m_pAS->m_iteratorPool.end(), this) == m_pAS->m_iteratorPool.end());
				m_pAS->m_iteratorPool.push_back(this);
			}
		}
		IActor* Next()
		{
			if (m_cur == m_pAS->m_actors.end())
				return 0;
			IActor* pActor = m_cur->second;
			++m_cur;
			return pActor;
		}
		size_t Count() {
			return m_pAS->m_actors.size();
		}
		CActorSystem* m_pAS;
		TActorMap::iterator m_cur;
		int m_nRefs;
	};

	ISystem						*m_pSystem;
	IEntitySystem			*m_pEntitySystem;

	TActorMap					m_actors;
	TActorClassMap		m_classes;	

	std::vector<CActorIterator*> m_iteratorPool;
};


#endif //__ACTORSYSTEM_H__
