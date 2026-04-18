#include "StdAfx.h"
#include "FlowDelayNode.h"
#include "../../CryAction.h"

CFlowDelayNode::CFlowDelayNode( SActivationInfo * pActInfo ) : m_actInfo(*pActInfo)
{
}

CFlowDelayNode::~CFlowDelayNode()
{
	RemovePendingTimers();
}

void
CFlowDelayNode::RemovePendingTimers()
{
	// remove all old timers we have
	if (GetISystem()->IsEditor() == false)
	{
		std::map<int, SDelayData>::iterator iter;
		if (!m_activations.empty()) {
			for (iter = m_activations.begin(); iter != m_activations.end(); ++iter) {
				CCryAction::GetCryAction()->RemoveTimer((*iter).first);
			}
		}
	}
	// clear in any case (Editor and Game mode)
	m_activations.clear();
}

IFlowNodePtr CFlowDelayNode::Clone( SActivationInfo * pActInfo )
{
	return new CFlowDelayNode(pActInfo);
}

void CFlowDelayNode::Serialize(SActivationInfo *pActInfo, TSerialize ser)
{
	CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();

	ser.BeginGroup("Local");
	// Editor mode: map with
	// key:  abs time in ms
	// data: SDelayData
	//
	// Game mode: map with
	// key:  timer id (we don't care about it)
	// data: SDelayData
	if (ser.IsWriting()) {
		// when writing, currently relative values are stored!
		ser.Value("m_activations", m_activations);
#if 0
		CryLogAlways("CDelayNode write: current time(ms): %f", curTime.GetMilliSeconds());		
		std::map<int, SDelayData>::iterator iter = m_activations.begin();
		while (iter != m_activations.end()) 
		{
			CryLogAlways("CDelayNode write: ms=%d  timevalue(ms): %f",(*iter).first, (*iter).second.m_timeout.GetMilliSeconds());
			++iter;
		}
#endif
	}
	else
	{
		// FIXME: should we read the curTime from the file
		//        or is the FrameStartTime already set to the serialized value?
		// ser.Value("curTime", curTime);

		// when reading we have to differentiate between Editor and Game Mode
		if (GetISystem()->IsEditor()) {
			// we can directly read into the m_activations array
			// regular update is handled by CFlowGraph
			ser.Value("m_activations", m_activations);
			std::map<int, SDelayData>::iterator iter = m_activations.begin();
#if 0
			CryLogAlways("CDelayNode read: current time(ms): %f", curTime.GetMilliSeconds());		
			while (iter != m_activations.end()) 
			{
				CryLogAlways("CDelayNode read: ms=%d  timevalue(ms): %f",(*iter).first, (*iter).second.m_timeout.GetMilliSeconds());
				++iter;
			}
#endif
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, !m_activations.empty());
		} else {
			RemovePendingTimers();
			// read serialized activations and re-register at timer
			std::map<int, SDelayData>::iterator iter;
			std::map<int, SDelayData> activations;
			ser.Value("m_activations", activations);
			for (iter = activations.begin(); iter != activations.end(); ++iter) {
				CTimeValue relTime = (*iter).second.m_timeout - curTime; 
				int timerId = CCryAction::GetCryAction()->AddTimer( relTime, false, OnTimer, this );
				m_activations[timerId] = (*iter).second;
			}
		}
	}
	ser.EndGroup();
}

void CFlowDelayNode::GetConfiguration( SFlowNodeConfig& config )
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig_AnyType("in",_HELP("Value to be passed after [Delay] time"), _HELP("In")),
		InputPortConfig<float>("delay", 1.0f,_HELP("Delay time in seconds"), _HELP("Delay")),
		{0}
	};

	static const SOutputPortConfig outputs[] = {
		OutputPortConfig_AnyType("out", _HELP("Out")),
		{0}
	};

	config.sDescription = _HELP("This node will delay passing the signal from [In] to [Out] for the specified number of seconds in [Delay]");
	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowDelayNode::ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
{
	switch (event)
	{
	case eFE_Initialize:
		RemovePendingTimers();
		pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
		break;

	case eFE_Activate:
		// we request a final activation if the input value changed
		// thus in case we get several inputs in ONE frame, we always only use the LAST one!
		if (IsPortActive(pActInfo, 0))
		{
			pActInfo->pGraph->RequestFinalActivation(pActInfo->myID);
		}
		break;

	case eFE_FinalActivate:
		if (GetISystem()->IsEditor())
		{
			const float delay = GetDelayTime(pActInfo);
			CTimeValue finishTime = gEnv->pTimer->GetFrameStartTime() + delay;
			m_activations[(int)finishTime.GetMilliSeconds()] = SDelayData(finishTime, pActInfo->pInputPorts[0]);
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
		}
		else
		{
			const float delay = GetDelayTime(pActInfo);
			CTimeValue finishTime = gEnv->pTimer->GetFrameStartTime() + delay;
			int timerId = CCryAction::GetCryAction()->AddTimer( delay, false, OnTimer, this );
			m_activations[timerId] = SDelayData(finishTime, pActInfo->pInputPorts[0]);
		}
		break;

	case eFE_Update:
		assert( GetISystem()->IsEditor() );
		assert( !m_activations.empty() );
		CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();

		while (!m_activations.empty() && m_activations.begin()->second.m_timeout < curTime)
		{
			ActivateOutput( pActInfo, 0, m_activations.begin()->second.m_data );
			m_activations.erase( m_activations.begin() );
		}
		if (m_activations.empty())
			pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, false );
		break;
	}
}

float CFlowDelayNode::GetDelayTime( SActivationInfo * pActInfo) const
{
	return GetPortFloat(pActInfo, 1);
}

void CFlowDelayNode::OnTimer( void * pUserData, int ref )
{
	assert(GetISystem()->IsEditor() == false);

	CFlowDelayNode * pThis = static_cast<CFlowDelayNode*>(pUserData);
	std::map<int, SDelayData>::iterator iter = pThis->m_activations.find(ref);
	if (iter == pThis->m_activations.end())
	{
		GameWarning("CFlowDelayNode::OnTimer: Stale reference %d", ref);
		return;
	}
	pThis->ActivateOutput( &pThis->m_actInfo, 0, iter->second.m_data );
	pThis->m_activations.erase( iter );
}

class CFlowRandomDelayNode : public CFlowDelayNode
{
public:
	CFlowRandomDelayNode( SActivationInfo * pActInfo ) : CFlowDelayNode(pActInfo)
	{
	}

	~CFlowRandomDelayNode()
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowRandomDelayNode(pActInfo);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("In",_HELP("Value to be passed after delay time")),
			InputPortConfig<float>("MinDelay", 1.0f, _HELP("Minimum random delay time in seconds")),
			InputPortConfig<float>("MaxDelay", 2.0f, _HELP("Maximum random delay time in seconds")),
			{0}
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_AnyType("Out"),
			{0}
		};

		config.sDescription = _HELP("This node will delay passing the signal from the [In] to [Out] for a random amount of time in the interval [MinDelay,MaxDelay]");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

protected:
	/* virtual */ float GetDelayTime(SActivationInfo * pActInfo) const
	{
		// CryMath random
		return Random(GetPortFloat(pActInfo, 1), GetPortFloat(pActInfo, 2));
	}
};


REGISTER_FLOW_NODE("Time:Delay", CFlowDelayNode);
REGISTER_FLOW_NODE("Time:RandomDelay", CFlowRandomDelayNode);

