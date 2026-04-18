#ifndef __FLOWDELAYNODE_H__
#define __FLOWDELAYNODE_H__

#pragma once

#include "FlowBaseNode.h"
#include <queue>
#include "FlowBaseNode.h"

class CFlowDelayNode : public CFlowBaseNode
{
public:
	CFlowDelayNode( SActivationInfo * );
	virtual ~CFlowDelayNode();

	// IFlowNode
	virtual IFlowNodePtr Clone( SActivationInfo * );
	virtual void GetConfiguration( SFlowNodeConfig& );
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * );
	virtual void Serialize(SActivationInfo *, TSerialize ser);

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->AddContainer(m_activations);
		for (std::map<int, SDelayData>::iterator iter = m_activations.begin(); iter != m_activations.end(); ++iter)
			iter->second.m_data.GetMemoryStatistics(s);
	}
	// ~IFlowNode

protected:
	// let derived classes decide how long to delay
	virtual float GetDelayTime( SActivationInfo * ) const;

private:
	void RemovePendingTimers();

	SActivationInfo m_actInfo;
	struct SDelayData 
	{
		SDelayData() {}
		SDelayData(const CTimeValue& timeout, const TFlowInputData& data) 
			: m_timeout(timeout), m_data(data) {}

		CTimeValue m_timeout;
		TFlowInputData m_data;
		bool operator<(const SDelayData& rhs) const
		{
			return m_timeout < rhs.m_timeout;
		}

		void Serialize(TSerialize ser)
		{
			ser.Value("m_timeout", m_timeout);
			ser.Value("m_data", m_data);
		}
	};
	std::map<int, SDelayData> m_activations;
	static void OnTimer( void * pUserData, int ref );
};

#endif
