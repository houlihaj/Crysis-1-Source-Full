#include "StdAfx.h"
#include "CET_LevelLoading.h"
#include "ILevelSystem.h"
#include "GameClientChannel.h"
#include "NetHelpers.h"
#include "GameContext.h"

/*
 * Prepare level
 */

class CCET_PrepareLevel : public CCET_Base
{
public:
	const char * GetName() { return "PrepareLevel"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		string levelName = CCryAction::GetCryAction()->GetGameContext()->GetLevelName();
		if (levelName.empty())
		{
			//GameWarning("No level name set");
			return eCETR_Wait;
		}
		if (gEnv->bClient && !gEnv->bServer)
			CCryAction::GetCryAction()->GetILevelSystem()->PrepareNextLevel( levelName );
		return eCETR_Ok;
	}
};

void AddPrepareLevelLoad( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_PrepareLevel() );
}


/*
* Store level checksum
*/

class CCET_StoreLevelChecksum : public CCET_Base
{
public:

	const char* GetName() { return "StoreLevelChecksum"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		// get checksum of the level.pak we're about to load, save it. Sent to clients
		//	when they connect to make sure they have the same level version.
		uint64 nCode=CCryAction::GetCryAction()->GetILevelSystem()->CalcLevelChecksum(
			CCryAction::GetCryAction()->GetGameContext()->GetLevelName());
		CCryAction::GetCryAction()->GetGameContext()->SetLevelChecksum(nCode);
		return eCETR_Ok;
	}
};

void AddStoreLevelChecksum( IContextEstablisher * pEst, EContextViewState state)
{
	CCET_StoreLevelChecksum * pSLC = new CCET_StoreLevelChecksum;
	pEst->AddTask( state, pSLC );
}

/*
 * Load a level
 */

class CCET_LoadLevel : public CCET_Base
{
public:
	CCET_LoadLevel() : m_bStarted(false) {}

	const char * GetName() { return "LoadLevel"; }

	EContextEstablishTaskResult OnStep( SContextEstablishState& state )
	{
		// if we reached this point then connection is ok. Clear the stored server info
		CCryAction::GetCryAction()->ClearStoredServerInfo();

		string levelName = CCryAction::GetCryAction()->GetGameContext()->GetLevelName();
		if (levelName.empty())
		{
			GameWarning("No level name set");
			return eCETR_Failed;
		}
		if (!CCryAction::GetCryAction()->GetILevelSystem()->LoadLevel( levelName ))
			return eCETR_Failed;
		m_bStarted = true;
		CCryAction::GetCryAction()->GetGameContext()->GetNetContext()->SetLevelName(levelName.c_str());
		return eCETR_Ok;
	}

	bool m_bStarted;
};

void AddLoadLevel( IContextEstablisher * pEst, EContextViewState state, bool ** ppStarted )
{
	CCET_LoadLevel * pLL = new CCET_LoadLevel;
	pEst->AddTask( state, pLL );
	*ppStarted = &pLL->m_bStarted;
}

/*
 * Loading complete
 */

class CCET_LoadingComplete : public CCET_Base
{
public:
	CCET_LoadingComplete( bool * pStarted ) : m_pStarted(pStarted) {}
		
	const char * GetName() { return "LoadingComplete"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		ILevelSystem * pLS = CCryAction::GetCryAction()->GetILevelSystem();
		if (pLS->GetCurrentLevel())
		{
			pLS->OnLoadingComplete(pLS->GetCurrentLevel());
			return eCETR_Ok;
		}
		return eCETR_Failed;
	}
	virtual void OnFailLoading( bool hasEntered )
	{
		ILevelSystem * pLS = CCryAction::GetCryAction()->GetILevelSystem();
		if (m_pStarted && *m_pStarted && !hasEntered)
			pLS->OnLoadingError( NULL, "context establisher failed" );
	}

	bool * m_pStarted;
};


void AddLoadingComplete( IContextEstablisher * pEst, EContextViewState state, bool * pStarted )
{
	pEst->AddTask( state, new CCET_LoadingComplete(pStarted) );
}

/*
* Reset Areas
*/
class CCET_ResetAreas : public CCET_Base
{
public:
	const char * GetName() { return "ResetAreas"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		gEnv->pEntitySystem->ResetAreas();
		return eCETR_Ok;
	}
};

void AddResetAreas( IContextEstablisher * pEst, EContextViewState state )
{
	pEst->AddTask( state, new CCET_ResetAreas );
}


/*
 * Lock resources
 */

struct SLockResourcesState : public CMultiThreadRefCount
{
	SLockResourcesState() : m_bLocked(false) {}
	bool m_bLocked;
};
typedef _smart_ptr<SLockResourcesState> SLockResourcesStatePtr;

class CCET_LockResources : public CCET_Base
{
public:
	const char * GetName() { return ""; }

	CCET_LockResources( bool bLock, SLockResourcesStatePtr pState, CGameContext * pGameContext ) : m_pState(pState), m_bLock(bLock), m_pGameContext(pGameContext) {}

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		if (m_bLock && !m_pState->m_bLocked)
		{
			m_pGameContext->LockResources();
			m_pState->m_bLocked = true;
		}
		if (!m_bLock && m_pState->m_bLocked)
		{
			m_pGameContext->UnlockResources();
			m_pState->m_bLocked = false;
		}
		return eCETR_Ok;
	}

	EContextEstablishTaskResult OnLeaveState(SContextEstablishState& state)
	{
		return eCETR_Ok;
	}

	void OnFailLoading( bool hasEntered )
	{
		if (m_pState->m_bLocked)
		{
			m_pGameContext->UnlockResources();
			m_pState->m_bLocked = false;
		}
	}

private:
	SLockResourcesStatePtr m_pState;
	bool m_bLock;
	CGameContext * m_pGameContext;
};

void AddLockResources( IContextEstablisher * pEst, EContextViewState stateBegin, EContextViewState stateEnd, CGameContext * pGameContext )
{
	SLockResourcesStatePtr pState = new SLockResourcesState;
	pEst->AddTask( stateBegin, new CCET_LockResources(true, pState, pGameContext) );
	pEst->AddTask( stateEnd, new CCET_LockResources(false, pState, pGameContext) );
}

/*
 * Action events
 */
class CCET_ActionEvent : public CCET_Base
{
public:
	CCET_ActionEvent( const SActionEvent& evt ) : m_evt(evt) {}

	const char * GetName() { return "ActionEvent"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		CCryAction::GetCryAction()->OnActionEvent(m_evt);
		return eCETR_Ok;
	}

private:
	SActionEvent m_evt;
};

void AddActionEvent( IContextEstablisher * pEst, EContextViewState state, const SActionEvent& evt )
{
	pEst->AddTask( state, new CCET_ActionEvent(evt) );
}
