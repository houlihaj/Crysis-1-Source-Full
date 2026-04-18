////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   partman.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//	- 03:2006				 : Modified by Jan Müller (Serialization)
//
////////////////////////////////////////////////////////////////////////////

#ifndef PART_MANAGER
#define PART_MANAGER

#define PROFILE_PIPE_SYS PARTICLE

#include "ParticleEffect.h"
#include "CryThread.h"
#include "Pipe.h"
#include "PoolAllocator.h"
#include "BitFiddling.h"

struct CParticleThread;
class CParticleEmitter;
class CParticleContainer;

//////////////////////////////////////////////////////////////////////////
#define bEVENT_TIMINGS				1
#define bPROFILE_LOCKS				(1 && !defined(PS3) && !defined(LINUX))

//#define AUTO_LOCK(lock)		CryAutoLock< CryLock<CRYLOCK_RECURSIVE> > _AutoLock(lock)

#if bPROFILE_LOCKS

	template <class T>
  class CAutoLockProfile
  {
	  T& m_lock;
  public:
	  CAutoLockProfile(T& lock, CFrameProfiler* pProfiler, const CParticleContainer* pCont = 0)
		  : m_lock(lock)
	  {
			if (lock.TryLock())
				return;
			// Profile only when waiting needed.
			CEventProfilerSection ProfilerSection( pProfiler, pCont );
			lock.Lock();
	  }
	  ~CAutoLockProfile()
	  {
		  m_lock.Unlock();
	  }
  };

	#define AUTO_LOCK_PROFILE_T(TYPE, lock, cont) \
		static CFrameProfiler _staticFrameProfiler( GetSystem(), __FUNCTION__ ":LOCK", PROFILE_SYNC); \
		CAutoLockProfile<TYPE> _AutoLock(lock, &_staticFrameProfiler, cont);

	#define AUTO_LOCK_PROFILE(lock, cont) \
		AUTO_LOCK_PROFILE_T(CryLock<CRYLOCK_RECURSIVE>, lock, cont)

#else

	#define AUTO_LOCK_PROFILE_T(TYPE, lock, cont) \
		AUTO_LOCK(lock);

	#define AUTO_LOCK_PROFILE(lock, cont) \
		AUTO_LOCK(lock);

#endif

// Top class of particle system
class CPartManager : public Cry3DEngineBase, public IVisAreaCallback
{
public:
  CPartManager();
  ~CPartManager();

  void Render(CObjManager* pObjManager, CTerrain* pTerrain);
	void Update();
	void OnFrameStart();

	void Reset(bool bIndependentOnly);
	void Serialize(TSerialize ser);

	// Unload resources for all effects & emitters.
	void ClearRenderResources( bool bForceClear );

	void OnEntityDeleted(IRenderNode * pRenderNode) {}

	//////////////////////////////////////////////////////////////////////////
	// Particle effects interface.
	//////////////////////////////////////////////////////////////////////////
	IParticleEffect* CreateEffect();
	void RenameEffect( IParticleEffect *pEffect, const char* sNewName );
	void RemoveEffect( IParticleEffect *pEffect );
	IParticleEffect* FindEffect( const char* sEffectName, const char* sCallerType = "", const char* sCallerName = "", bool bLoad = true );

	bool LoadLibrary( const char* sParticlesLibrary, const char* sParticlesLibraryFile=NULL, bool bLoadResources = false );

	// Whether params are selected for current config.
	bool IsActive( ParticleParams const& params ) const;

	//////////////////////////////////////////////////////////////////////////
	// Emitters.
	//////////////////////////////////////////////////////////////////////////
	IParticleEmitter* CreateEmitter( bool bIndependent, Matrix34 const& mLoc, const IParticleEffect* pEffect, const ParticleParams* pParams = NULL );
	void DeleteEmitter( IParticleEmitter *pEmitter );
	void RegisterEmitter( CParticleEmitter* pEmitter );
	void UpdateEmitters( IParticleEffect *pEffect );

	int DeleteEmitter( int i );

	//////////////////////////////////////////////////////////////////////////
	void PlaySound( ISound *pSound );

	IMaterial* GetLightShader() const
	{
		return m_pPartLightShader;
	}

	static AABB const& GetUpdatedAreaBB()
	{
		return m_bbAreasChanged;
	}

	//
	// Debugging and stats.
	//
  void GetCounts( SParticleCounts& counts );
	void GetMemoryUsage( ICrySizer* pSizer );
	void PrintTextures( IParticleEffect* pEffect );
	void RenderDebugInfo();
	void PostSerialize( bool bReading)
	{
		if (bReading)
			ListEmitters("after load");
	}
	void ListEmitters( const char* sDesc );

#if bEVENT_TIMINGS
	struct SEventTiming
	{
		const CParticleEffect* pEffect;
		uint32 nContainerId;
		int nThread;
		const char* sEvent;
		float	timeStart, timeEnd;
	};
	int AddEventTiming( const char* sEvent, const CParticleContainer* pCont );

	inline int StartEventTiming( const char* sEvent, const CParticleContainer* pCont )
	{
		if (pCont && (GetCVars()->e_particles_debug & AlphaBits('ed')))
			return AddEventTiming( sEvent, pCont ) * m_iEventSwitch;
		else
			return 0;
	}
	inline void EndEventTiming( int iEvent )
	{
		iEvent *= m_iEventSwitch;
		if (iEvent > 0)
			m_aEvents[iEvent].timeEnd = GetTimer()->GetAsyncCurTime();
	}

#endif

	//
	// Thread support.
	//
	void OnThreadUpdate();							// Executed from aux thread.

	void QueueUpdate( CParticleContainer* pCont, bool bNext )
	{
		if (m_pThreadTask)
		{
			if (bNext)
			{
				m_NextQueue.Push(pCont);
			}
			else
			{
				m_UpdateQueue.Push(pCont);
			}
		}
	}
	void UnqueueUpdate( CParticleContainer* pCont )
	{
		if (m_pThreadTask)
		{
			// Erase from current and/or next queues.
			m_UpdateQueue.Replace(pCont, 0);
			m_NextQueue.Replace(pCont, 0);
		}
	}

	void WakeParticleThread(bool force);

	inline bool IsThreading() const
	{
		return GetCVars()->e_particles_thread && gEnv->pi.numCoresAvailableToProcess > 1;
	}

	inline CryLock<CRYLOCK_RECURSIVE>& GetLock()
	{
		return m_Lock;
	}

	CryLock<CRYLOCK_RECURSIVE>* AllocLock();

	void FreeLock(CryLock<CRYLOCK_RECURSIVE>* pLock);

	inline CCamera const& GetRenderCamera() const
	{
		return m_RenderCam;
	}

	// Fill rate limiting.
	void TrackScrFill( float fScrFill )
	{
		m_afScrFills.push_back(fScrFill);
	}
	float GetMaxContainerScrFill() const
	{
		return m_fMaxContainerScrFill;
	}

private:

	// Implements a basic auto-reset event class.
	class Event
	{
	public:
		Event();
		~Event();
		void Wait();
		void Notify();

	private:
#if defined(PS3)
	// Lock for synchronization of notifications.
	CryFastLock m_lockNotify;
	CryCond<CryFastLock> m_cond;
	bool m_flag;
#else //defined(PS3)
		EVENT_HANDLE m_hEvent;
#endif //defined(PS3)
	};

	//////////////////////////////////////////////////////////////////////////
	// Array of all registered particle effects.
	SmartPtrArray<CParticleEffect> m_effects;

	// Map of particle effect case-insensitive name to interface pointer.
	typedef std::map<string,IParticleEffect_AutoPtr,stl::less_stricmp<string> > EffectsMap;
	EffectsMap m_effectsMap;

	//////////////////////////////////////////////////////////////////////////
	// Loaded particle libs.
	std::set<string,stl::less_stricmp<string> > m_loadedLibs;

	//////////////////////////////////////////////////////////////////////////
	// Particle effects emitters, top-level only.
	//////////////////////////////////////////////////////////////////////////
	SmartPtrArray<CParticleEmitter>	m_allEmitters;

	bool								m_bRuntime;
	CTimeValue					m_tLastUpdate;
	IMaterial*          m_pPartLightShader;
	CCamera							m_RenderCam;
	DynArray<float>							m_afScrFills;
	float												m_fMaxContainerScrFill;			// Per-frame computed fill-rate limit per container.

	void ComputeMaxContainerScrFill();

	// Listener for physics events. Static data will be fine, there's just one physics system.
	static int OnPhysAreaChange(const EventPhys *pEvent);
	static AABB m_bbAreasChanged;

	// Listener for vis area events.
	virtual void OnVisAreaDeleted( IVisArea* pVisArea );

	//
	// Multi-threading support.
	//

	mutable CryLock<CRYLOCK_RECURSIVE>
														m_Lock;										// Mutex for general shared data (e.g. stats).
	mutable CryLock<CRYLOCK_RECURSIVE>
														m_ThreadLock;							// Mutex for running update thread.

	CParticleThread*					m_pThreadTask;

	Pipe<CParticleContainer*>	m_UpdateQueue;						// Containers needing update this frame.
	Pipe<CParticleContainer*>	m_NextQueue;							// Containers updated last frame.

	Event											m_UpdateQueueEvent;				// Used to signal the particle thread that there is work to do.
	int												m_EventFrequencyCounter;	// Used simply to lower the frequency at which we signal the event for perf reasons.

	// Pooled, reusable locks.
	stl::TPoolAllocator< CryLock<CRYLOCK_RECURSIVE> >
														m_poolLocks;							// Pool for generating new locks.
	DynArray< CryLock<CRYLOCK_RECURSIVE>* >
														m_aFreeLocks;							// Internal free list of locks, not released to pool.
																											// Reused to minimise initialisation.
	void											ReleaseLocks();						// When cleaning up.

#if bEVENT_TIMINGS
	DynArray<SEventTiming> m_aEvents;
	int m_iEventSwitch;
	void LogEvents();
#endif
};

#if bEVENT_TIMINGS

class CEventProfilerSection: public CFrameProfilerSection, public Cry3DEngineBase
{
public:
	CEventProfilerSection( CFrameProfiler *pProfiler, const CParticleContainer* pCont = 0 )
	: CFrameProfilerSection(pProfiler)
	{
		m_iEvent = m_pPartManager->StartEventTiming(pProfiler->m_name, pCont);
	}
	~CEventProfilerSection()
	{
		m_pPartManager->EndEventTiming(m_iEvent);
	}
protected:
	int m_iEvent;
};

	#define FUNCTION_PROFILER_CONTAINER(pCont) \
		static CFrameProfiler staticFrameProfiler( gEnv->pSystem, __FUNCTION__, PROFILE_PARTICLE ); \
		CEventProfilerSection eventProfilerSection( &staticFrameProfiler, pCont );

#else

	#define FUNCTION_PROFILER_CONTAINER(pCont)	FUNCTION_PROFILER_SYS(PARTICLE)

#endif

//---------------------------------------------------------------------------------------------
// Memory stats support 

#define for_const_container(Type, cont, it) \
	for (Type::const_iterator it = (cont).begin(); it != (cont).end(); ++it)

// Default sizer template
template<class T>
void GetMemoryUsage( ICrySizer* pSizer, T const& t )
	{}

// Container sizer
template<class Cont> 
void AddContainer( ICrySizer* pSizer, Cont const& cont )
{
	if (pSizer->AddContainer(cont))
		for_const_container (typename Cont, cont, it)
			GetMemoryUsage(pSizer, *it);
}

template<class Cont> 
void AddPtrContainer( ICrySizer* pSizer, Cont const& cont )
{
	if (pSizer->AddContainer(cont))
		for_const_container (typename Cont, cont, it)
		{
			if (pSizer->Add(**it))
				GetMemoryUsage(pSizer, **it);
		}
}

template<class T1, class T2>
void GetMemoryUsage( ICrySizer* pSizer, std::pair<T1,T2> const& p )
{
	GetMemoryUsage(pSizer, p.first);
	GetMemoryUsage(pSizer, p.second);
}

template<> inline
void GetMemoryUsage( ICrySizer* pSizer, string const& str )
{
	pSizer->AddString(str);
}



// Macro for standard specialisation
#define Declare_GetMemoryUsage(T) \
	template<> inline void GetMemoryUsage( ICrySizer* pSizer, T const& t ) \
		{ const_cast<T&>(t).GetMemoryUsage(pSizer); }

#endif // PART_MANAGER
	
