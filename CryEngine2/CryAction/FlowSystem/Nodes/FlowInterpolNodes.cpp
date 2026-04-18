////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FlowInterpolNodes.cpp
//  Version:     v1.00
//  Created:     27/4/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseNode.h"

template<typename T> struct Limits
{
	static const char* min_name;
	static const char* max_name;
	static const T min_val;
	static const T max_val;
};

template<typename T> struct LimitsColor
{
	static const char* min_name;
	static const char* max_name;
	static const T min_val;
	static const T max_val;
};

template<typename T> const char* Limits<T>::min_name = "StartValue";
template<typename T> const char* Limits<T>::max_name = "EndValue";

template<> const float Limits<float>::min_val = 0.0f;
template<> const float Limits<float>::max_val = 1.0f;
template<> const int Limits<int>::min_val = 0;
template<> const int Limits<int>::max_val = 255;
template<> const Vec3 Limits<Vec3>::min_val = Vec3(0.0f,0.0f,0.0f);
template<> const Vec3 Limits<Vec3>::max_val = Vec3(1.0f,1.0f,1.0f);
template<> const Vec3 LimitsColor<Vec3>::min_val = Vec3(0.0f,0.0f,0.0f);
template<> const Vec3 LimitsColor<Vec3>::max_val = Vec3(1.0f,1.0f,1.0f);
template<> const char* LimitsColor<Vec3>::min_name = "color_StartValue";
template<> const char* LimitsColor<Vec3>::max_name = "color_EndValue";

//////////////////////////////////////////////////////////////////////////
template<class T, class L=Limits<T> >
class CFlowInterpolNode : public CFlowBaseNode
{
	typedef CFlowInterpolNode<T,L> Self;
public:
	enum EInputs
	{
		EIP_Start,
		EIP_Stop,
		EIP_StartValue,
		EIP_EndValue,
		EIP_Time
	};
	enum EOutputs
	{
		EOP_Value,
		EOP_Done
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	CFlowInterpolNode( SActivationInfo * pActInfo )
		: m_startTime(0.0f),
		m_endTime(0.0f),
		m_startValue(L::min_val),
		m_endValue(L::min_val)
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new Self(pActInfo);
	}

	virtual void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_startTime", m_startTime);
		ser.Value("m_endTime", m_endTime);
		ser.Value("m_startValue", m_startValue);
		ser.Value("m_endValue", m_endValue);
		ser.EndGroup();
		// regular update is handled by FlowGraph serialization
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Start", _HELP("StartInterpol")),
			InputPortConfig_Void("Stop", _HELP("StopInterpol")),
			InputPortConfig<T>(L::min_name,  L::min_val,  _HELP("Start value")),
			InputPortConfig<T>(L::max_name,  L::max_val, _HELP("End value")),
			InputPortConfig<float>("Time", 1.0f, _HELP("Time in Seconds")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<T>("Value", _HELP("Current Value")),
			OutputPortConfig_Void("Done", _HELP("Triggered when finished")),
			{0}
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	bool GetValue(SActivationInfo* pActInfo, int nPort, T& value)
	{
		T* pVal = (pActInfo->pInputPorts[nPort].GetPtr<T>());
		if (pVal)
		{
			value = *pVal;
			return true;
		}
		return false;
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Stop))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, EOP_Done, true);
				}
				if (IsPortActive(pActInfo, EIP_Start))
				{
					m_startTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
					m_endTime = m_startTime + GetPortFloat(pActInfo, EIP_Time) * 1000.0f;
					GetValue(pActInfo, EIP_StartValue, m_startValue);
					GetValue(pActInfo, EIP_EndValue, m_endValue);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					Interpol(pActInfo, 0.0f);
				}
			}
			break;
		case eFE_Update:
			{
				const float fTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
				const float fDuration = m_endTime - m_startTime;
				float fPosition;
				if (fDuration <= 0.0)
					fPosition = 1.0;
				else
				{
					fPosition = (fTime - m_startTime) / fDuration;
					fPosition = CLAMP(fPosition, 0.0f, 1.0f);
				}
				Interpol(pActInfo, fPosition);
				if (fTime >= m_endTime)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, EOP_Done, true);
				}
			}
			break;
		}
	}

protected:
	void Interpol(SActivationInfo* pActInfo, const float fPosition)
	{
		T value = ::Lerp(m_startValue, m_endValue, fPosition);
		ActivateOutput(pActInfo, EOP_Value, value);
	}

	float m_startTime;
	float m_endTime;
	T m_startValue;
	T m_endValue;
};

typedef CFlowInterpolNode<int> IntInterpol;
typedef CFlowInterpolNode<float> FloatInterpol;
typedef CFlowInterpolNode<Vec3> Vec3Interpol;
typedef CFlowInterpolNode<Vec3, LimitsColor<Vec3> > ColorInterpol;

REGISTER_FLOW_NODE("Interpol:Int",   IntInterpol);
REGISTER_FLOW_NODE("Interpol:Float", FloatInterpol);
REGISTER_FLOW_NODE("Interpol:Vec3", Vec3Interpol);
REGISTER_FLOW_NODE("Interpol:Color", ColorInterpol);

