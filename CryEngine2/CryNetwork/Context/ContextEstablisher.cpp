#include "StdAfx.h"
#include "ContextEstablisher.h"

class CCET_ExecConsole : public CCET_Base
{
public:
	CCET_ExecConsole( string cmd, string name ) : m_cmd(cmd), m_name(name) {}

	const char * GetName() { return m_name.c_str(); }

	EContextEstablishTaskResult OnStep( SContextEstablishState& )
	{
		gEnv->pConsole->ExecuteString(m_cmd.c_str());
		return eCETR_Ok;
	}

private:
	string m_cmd;
	string m_name;
};


CContextEstablisher::CContextEstablisher()
{
	m_offset = -1;
	m_done = false;
	m_debuggedFrame = -1;
	++g_objcnt.contextEstablisher;

	m_begin = gEnv->pTimer->GetAsyncTime();
}

CContextEstablisher::~CContextEstablisher()
{
	ASSERT_PRIMARY_THREAD;
	--g_objcnt.contextEstablisher;
	for (size_t i=0; i<m_tasks.size(); ++i)
	{
		m_tasks[i].pTask->Release();
	}
}

void CContextEstablisher::Done()
{
	m_done = true;
}

void CContextEstablisher::AddTask( EContextViewState state, IContextEstablishTask * pTask )
{
	NET_ASSERT(m_offset < 0);
	m_tasks.push_back(STask(state, pTask));
#if ENABLE_DEBUG_KIT
//	m_tasks.push_back(STask(state, new CCET_ExecConsole("net_dump_object_state", "DOS: " + string(pTask->GetName()))));
#endif
}

void CContextEstablisher::Start()
{
	NET_ASSERT(m_offset < 0);
	std::stable_sort(m_tasks.begin(), m_tasks.end());
	m_offset = 0;
}

bool CContextEstablisher::StepTo( SContextEstablishState& state )
{
	if (m_offset < 0)
		return eCETR_Failed;

	while (m_offset < m_tasks.size() && m_tasks[m_offset].state <= state.contextState)
	{
#if ENABLE_DEBUG_KIT
		m_tasks[m_offset].numRuns ++;
#endif
		switch (m_tasks[m_offset].pTask->OnStep(state))
		{
		case eCETR_Ok:
#if ENABLE_DEBUG_KIT
			m_tasks[m_offset].done = gEnv->pTimer->GetAsyncTime();
			if (m_tasks[m_offset].pTask->GetName()[0] && CVARS.LogLevel > 0)
				NetLog("Establishment: Did %s on %s in %.2fms", m_tasks[m_offset].pTask->GetName(), state.pSender? state.pSender->GetName() : "Context",
					(m_tasks[m_offset].done - (m_offset? m_tasks[m_offset-1].done : m_begin)).GetMilliSeconds());
#endif
			m_offset++;
#if ENABLE_DEBUG_KIT
			if (m_offset < m_tasks.size())
				if (m_tasks[m_offset].pTask->GetName()[0] && CVARS.LogLevel > 0)
					NetLog("Establishment: Beginning %s on %s", m_tasks[m_offset].pTask->GetName(), state.pSender? state.pSender->GetName() : "Context");
#endif
			break;
		case eCETR_Failed:
			m_offset++;
			Fail( eDC_GameError, string().Format("Couldn't establish context: %s failed", m_tasks[m_offset-1].pTask->GetName()) );
			return false;
		case eCETR_Wait:
			return true;
		}
	}
	return true;
}

void CContextEstablisher::Fail( EDisconnectionCause cause, const string& reason )
{
	m_failureReason = reason;
	for (size_t i=0; i<m_tasks.size(); i++)
	{
		m_tasks[i].pTask->OnFailLoading( i < m_offset );
	}
	m_offset = -1;

	if (m_disconnectOnFail)
	{
		SCOPED_GLOBAL_LOCK;
		m_disconnectOnFail->Disconnect( cause, reason.c_str() );
	}
}

#if ENABLE_DEBUG_KIT
void CContextEstablisher::DebugDraw()
{
	ASSERT_PRIMARY_THREAD;
	int y = 400;
	static int x;
	static int frameid = -1;
	if (frameid != gEnv->pRenderer->GetFrameID())
	{
		x = 10;
		frameid = gEnv->pRenderer->GetFrameID();
	}
	if (frameid != m_debuggedFrame)
	{
		CTimeValue beg = m_begin;
		for (size_t i=0; i<m_tasks.size(); i++)
		{
			if (!m_tasks[i].pTask->GetName()[0])
				continue;
			float tm = -1.0f;
			if (m_tasks[i].done > 0.0f)
			{
				tm = (m_tasks[i].done - beg).GetMilliSeconds();
				beg = m_tasks[i].done;
			}
			float clr[4] = {1,1,1,(i>=m_offset)*0.5f + 0.5f};
			gEnv->pRenderer->Draw2dLabel(x, y, 1, clr, false, "%d: %s [%d @ %.2f]", m_tasks[i].state, m_tasks[i].pTask->GetName(), m_tasks[i].numRuns, tm);
			y += 10;
		}
		x += 130;
	}
}
#endif
