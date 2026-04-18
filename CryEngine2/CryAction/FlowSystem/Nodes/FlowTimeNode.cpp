////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowTimeNode.h
//  Version:     v1.00
//  Created:     9/5/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowTimeNode.h"
#include "TimeOfDayScheduler.h"
#include <time.h>
#include <I3DEngine.h>

//////////////////////////////////////////////////////////////////////////
class CFlowTimeNode : public CFlowBaseNode
{
public:
	enum EInputs
	{
		IN_PAUSED
	};
	enum EOutputs
	{
		OUT_SECONDS,
		OUT_TICK,
	};
	CFlowTimeNode( SActivationInfo * pActInfo )
	{
//		pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
	}
	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("paused", false, _HELP("When set to true will pause time output")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("seconds", _HELP("Outputs the current time in seconds.")),
			OutputPortConfig_Void("tick", _HELP("Outputs event at this port every frame.")),
			{0}
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Update:
			{
				bool bPaused = GetPortBool(pActInfo,IN_PAUSED);
				if (!bPaused)
				{
					float fSeconds = gEnv->pTimer->GetFrameStartTime().GetSeconds();
					ActivateOutput( pActInfo,OUT_SECONDS,fSeconds );
					ActivateOutput( pActInfo,OUT_TICK,0 );
				}
			}
			break;
		case eFE_Activate:
		case eFE_Initialize:
			if (IsPortActive(pActInfo,IN_PAUSED))
			{
				bool bPaused = GetPortBool(pActInfo,IN_PAUSED);
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, !bPaused );
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Time of day node.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_RealTime : public CFlowBaseNode
{
public:
	enum Outputs
	{
		OUT_HOURS,
		OUT_MINUTES,
		OUT_SECONDS,
	};
	CFlowNode_RealTime( SActivationInfo * pActInfo )
	{
		memset(&m_lasttime, 0, sizeof(m_lasttime));
	}
	
	IFlowNodePtr Clone(SActivationInfo *pActInfo )
	{
		return new CFlowNode_RealTime(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		if (ser.IsReading())
		{
			// forces output on loading
			m_lasttime.tm_hour=-1;
			m_lasttime.tm_min=-1;
			m_lasttime.tm_sec=-1;
		}
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("hours"),
			OutputPortConfig<int>("minutes"),
			OutputPortConfig<int>("seconds"),
			{0}
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
			break;
		case eFE_Update:
			{
				time_t long_time = time( NULL );
				tm *newtime = localtime( &long_time );
				if (newtime->tm_hour != m_lasttime.tm_hour)
					ActivateOutput( pActInfo,OUT_HOURS,newtime->tm_hour );
				if (newtime->tm_min != m_lasttime.tm_min)
					ActivateOutput( pActInfo,OUT_MINUTES,newtime->tm_min );
				if (newtime->tm_sec != m_lasttime.tm_sec)
					ActivateOutput( pActInfo,OUT_SECONDS,newtime->tm_sec );
				m_lasttime = *newtime;
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
private:
	tm m_lasttime;
};

//////////////////////////////////////////////////////////////////////////
// Timer node.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Timer : public CFlowBaseNode
{
public:
	enum EInputs
	{
		IN_PERIOD,
		IN_MIN,
		IN_MAX,
		IN_PAUSED
	};
	enum EOutputs
	{
		OUT_OUT,
	};
	CFlowNode_Timer( SActivationInfo * pActInfo )
	{
		pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
		m_nCurrentCount = -100000;
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_Timer(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_last", m_last);
		ser.Value("m_nCurrentCount", m_nCurrentCount);
		ser.EndGroup();
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>( "period",_HELP("Tick period in seconds") ),
			InputPortConfig<int>( "min",_HELP("Minimal timer output value") ),
			InputPortConfig<int>( "max",_HELP("Maximal timer output value") ),
			InputPortConfig<bool>( "paused",_HELP("Timer will be paused when set to true") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("out"),
			{0}
		};

		config.sDescription = _HELP( "Timer node will output count from min to max, ticking at the specified period of time" );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Update:
			{
				if (GetPortBool(pActInfo,IN_PAUSED))
					return;

				float fPeriod = GetPortFloat(pActInfo,IN_PERIOD);
				CTimeValue time = gEnv->pTimer->GetFrameStartTime();
				CTimeValue dt = time - m_last;
				if (dt.GetSeconds() >= fPeriod)
				{
					m_last = time;
					int nMin = GetPortInt(pActInfo,IN_MIN);
					int nMax = GetPortInt(pActInfo,IN_MAX);
					m_nCurrentCount++;
					if (m_nCurrentCount < nMin)
						m_nCurrentCount = nMin;
					if (m_nCurrentCount > nMax)
					{
						m_nCurrentCount = nMin;
					}
					ActivateOutput( pActInfo,OUT_OUT,m_nCurrentCount );
				}
			}
			break;
		case eFE_Activate:
		case eFE_Initialize:
			{
				if (IsPortActive(pActInfo,IN_PAUSED))
				{
					bool bPaused = GetPortBool(pActInfo,IN_PAUSED);
					pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, !bPaused );
				}
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
private:
	CTimeValue m_last;
	int m_nCurrentCount;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_TimeOfDay : public CFlowBaseNode
{
public:
	enum EInputs
	{
		IN_TIME = 0,
		IN_SET,
		IN_FORCEUPDATE,
		IN_GET,
		IN_SPEED,
		IN_SET_SPEED,
		IN_GET_SPEED
	};
	enum EOutputs
	{
		OUT_CURTIME = 0,
		OUT_CURSPEED
	};

	CFlowNode_TimeOfDay(SActivationInfo * /* pActInfo */ ) {}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float> ("Time", 0.0f, _HELP("TimeOfDay to set (in hours 0-24)") ),
			InputPortConfig_Void("Set",  _HELP("Trigger to Set TimeOfDay to [Time]"), _HELP("SetTime") ),
			InputPortConfig<bool>("ForceUpdate", false, _HELP("Force Immediate Update of Sky if true. Only in Special Cases!") ),
			InputPortConfig_Void("Get",  _HELP("Trigger to Get TimeOfDay to [CurTime]"), _HELP("GetTime") ),
			InputPortConfig<float>("Speed", 1.0f, _HELP("Speed to bet set (via [SetSpeed]")),
			InputPortConfig_Void("SetSpeed", _HELP("Trigger to set TimeOfDay Speed to [Speed]")),
			InputPortConfig_Void("GetSpeed", _HELP("Trigger to get TimeOfDay Speed to [CurSpeed]")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("CurTime", _HELP("Current TimeOfDay (set when [GetTime] is triggered)")),
			OutputPortConfig<float>("CurSpeed", _HELP("Current TimeOfDay Speed (set when [GetSpeed] is triggered)")),
			{0}
		};

		config.sDescription = _HELP("Set/Get TimeOfDay and Speed");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			{
				ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
				if (pTOD == 0)
					return;

				if (IsPortActive(pActInfo,IN_SET))
				{
					const bool bForceUpdate = GetPortBool(pActInfo, IN_FORCEUPDATE);
					pTOD->SetTime(GetPortFloat(pActInfo, IN_TIME), bForceUpdate);
					ActivateOutput(pActInfo, OUT_CURTIME, pTOD->GetTime());
				}
				if (IsPortActive(pActInfo,IN_GET))
				{
					ActivateOutput(pActInfo, OUT_CURTIME, pTOD->GetTime());
				}
				if (IsPortActive(pActInfo,IN_SET_SPEED))
				{
					ITimeOfDay::SAdvancedInfo info;
					pTOD->GetAdvancedInfo(info);
					info.fAnimSpeed = GetPortFloat(pActInfo, IN_SPEED);
					pTOD->SetAdvancedInfo(info);
					ActivateOutput(pActInfo, OUT_CURSPEED, info.fAnimSpeed);
				}
				if (IsPortActive(pActInfo,IN_GET_SPEED))
				{
					ITimeOfDay::SAdvancedInfo info;
					pTOD->GetAdvancedInfo(info);
					ActivateOutput(pActInfo, OUT_CURSPEED, info.fAnimSpeed);
				}
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Timer node.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_MeasureTime : public CFlowBaseNode
{
public:
	enum EInputs
	{
		EIP_Start = 0,
		EIP_Stop
	};
	enum EOutputs
	{
		EOP_Started = 0,
		EOP_Stopped,
		EOP_Time,
	};

	CFlowNode_MeasureTime( SActivationInfo * pActInfo )
	{
		m_last = 0.0f;
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_MeasureTime(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		ser.Value("m_last", m_last);
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "Start",_HELP("Trigger to start measuring") ),
			InputPortConfig_Void( "Stop", _HELP("Trigger to stop measuring") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void( "Started", _HELP("Triggered on Start") ),
			OutputPortConfig_Void( "Stopped", _HELP("Triggered on Stop") ),
			OutputPortConfig<float>("Elapsed", _HELP("Elapsed time in seconds") ),
			{0}
		};

		config.sDescription = _HELP( "Node to measure elapsed time." );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Start))
			{
				m_last = gEnv->pTimer->GetFrameStartTime();
				ActivateOutput(pActInfo, EOP_Started, true);
			}
			if (IsPortActive(pActInfo, EIP_Stop))
			{
				CTimeValue dt = gEnv->pTimer->GetFrameStartTime();
				dt-=m_last;
				m_last = 0.0f;
				ActivateOutput(pActInfo, EOP_Stopped, true);
				ActivateOutput(pActInfo, EOP_Time, dt.GetSeconds());
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
private:
	CTimeValue m_last;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_TimeOfDayTrigger : public CFlowBaseNode
{
public:
	enum EInputs
	{
		EIP_Enabled = 0,
		EIP_Time,
	};
	enum EOutputs
	{
		EOP_Trigger = 0,
	};

	CFlowNode_TimeOfDayTrigger(SActivationInfo * /* pActInfo */ ) {
		m_timerId = CTimeOfDayScheduler::InvalidTimerId;
	}

	~CFlowNode_TimeOfDayTrigger()
	{
		ResetTimer();
	}

	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo )
	{
		return new CFlowNode_TimeOfDayTrigger(pActInfo);
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool> ("Active", true, _HELP("Whether trigger is enabled") ),
			InputPortConfig<float>("Time",  0.0f,  _HELP("Time when to trigger") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("Trigger", _HELP("Triggered when TimeOfDay has been reached. Outputs current timeofday")),
			{0}
		};

		config.sDescription = _HELP("TimeOfDay Trigger");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:
			{
				ResetTimer();
				m_actInfo = *pActInfo;
				const bool bEnabled = IsBoolPortActive(pActInfo, EIP_Enabled);
				if (bEnabled)
					RegisterTimer(pActInfo);
			}
			break;
		}
	}

	void ResetTimer()
	{
		if (m_timerId != CTimeOfDayScheduler::InvalidTimerId)
		{
			CCryAction::GetCryAction()->GetTimeOfDayScheduler()->RemoveTimer(m_timerId);
			m_timerId = CTimeOfDayScheduler::InvalidTimerId;
		}		
	}

	void RegisterTimer(SActivationInfo* pActInfo)
	{
		assert (m_timerId == CTimeOfDayScheduler::InvalidTimerId);
		const float time = GetPortFloat(pActInfo, EIP_Time);
		m_timerId = CCryAction::GetCryAction()->GetTimeOfDayScheduler()->AddTimer(time, OnTODCallback, (void*)this);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			ResetTimer();
			// re-enable if it's enabled
			const bool bEnabled = GetPortBool(pActInfo, EIP_Enabled);
			if (bEnabled)
				RegisterTimer(pActInfo);
		}
	}

protected:
	static void OnTODCallback(CTimeOfDayScheduler::TimeOfDayTimerId timerId, void* pUserData, float curTime)
	{
		CFlowNode_TimeOfDayTrigger * pThis = reinterpret_cast<CFlowNode_TimeOfDayTrigger*>(pUserData);
		if (timerId != pThis->m_timerId)
			return;
		pThis->ActivateOutput(&pThis->m_actInfo, EOP_Trigger, curTime);
	}

	SActivationInfo m_actInfo;
	CTimeOfDayScheduler::TimeOfDayTimerId m_timerId;
};


//////////////////////////////////////////////////////////////////////////
// Time of day node.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_ServerTime : public CFlowBaseNode
{
public:
	enum Inputs
	{
		IN_BASETIME,
		IN_PERIOD,
	};
	enum Outputs
	{
		OUT_SECS,
		OUT_MSECS,
		OUT_PERIOD,
	};
	CFlowNode_ServerTime( SActivationInfo * pActInfo ): m_lasttime(0.0f), m_basetime(0.0f)
	{
	}

	IFlowNodePtr Clone(SActivationInfo *pActInfo )
	{
		return new CFlowNode_ServerTime(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		if (ser.IsReading())
		{
			ser.Value("lasttime", m_lasttime);
			ser.Value("basetime", m_basetime);
			ser.Value("period", m_period);
		}
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float> ("basetime", 0.0f, _HELP("Set base time in seconds. All values output will be relative to this time.")),
			InputPortConfig<float> ("period", 0.0f, _HELP("Set period of the timer in seconds. Timer will reset each time after reaching this value.")),
			{0}
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("secs"),
			OutputPortConfig<int>("msecs"),
			OutputPortConfig<bool>("period"),
			{0}
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
				if (IsPortActive(pActInfo,IN_BASETIME))
					m_basetime =	GetPortFloat(pActInfo, IN_BASETIME);

				if (IsPortActive(pActInfo,IN_PERIOD))
					m_period = GetPortFloat(pActInfo, IN_PERIOD);

				m_msec=0;
			}
			break;
		case eFE_Update:
			{
				CTimeValue now=CCryAction::GetCryAction()->GetServerTime();

				if (now != m_lasttime)
				{
					uint32 msec=(uint32)((now-m_basetime).GetMilliSeconds());
					uint32 period=(uint32)(m_period.GetMilliSeconds());

					if (period>0)
					{
						msec=msec%period;
						if (msec<m_msec)
							ActivateOutput( pActInfo,OUT_PERIOD, 1);

						m_msec=msec;
					}
					ActivateOutput( pActInfo,OUT_MSECS, msec);
				}
				if (now.GetSeconds() != m_lasttime.GetSeconds())
				{
					float secs=(now-m_basetime).GetSeconds();
					float period=m_period.GetSeconds();

					if (period>0.00001f)
						secs=cry_fmod(secs, period);

					ActivateOutput( pActInfo,OUT_SECS, secs);
				}

				m_lasttime = now;
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
private:
	CTimeValue	m_lasttime;
	CTimeValue	m_basetime;
	CTimeValue	m_period;
	uint32			m_msec;
};

REGISTER_FLOW_NODE_SINGLETON( "Time:Time",CFlowTimeNode )
REGISTER_FLOW_NODE( "Time:RealTime",CFlowNode_RealTime )
REGISTER_FLOW_NODE( "Time:Timer",CFlowNode_Timer )
REGISTER_FLOW_NODE_SINGLETON( "Time:TimeOfDay",CFlowNode_TimeOfDay )
REGISTER_FLOW_NODE( "Time:MeasureTime", CFlowNode_MeasureTime )
REGISTER_FLOW_NODE( "Time:TimeOfDayTrigger", CFlowNode_TimeOfDayTrigger )
REGISTER_FLOW_NODE( "Time:ServerTime",CFlowNode_ServerTime )