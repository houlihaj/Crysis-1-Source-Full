////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowLogicNodes.h
//  Version:     v1.00
//  Created:     30/6/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseNode.h"

class CLogicNode : public CFlowBaseNode
{
public:
	CLogicNode( SActivationInfo * pActInfo ) : m_bResult(2) {};

	void Serialize(SActivationInfo *, TSerialize ser)
	{
		ser.Value("m_bResult", m_bResult);
	}

	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo) = 0;

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>( "A",_HELP("out = A op B") ),
			InputPortConfig<bool>( "B",_HELP("out = A op B") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out",_HELP("out = A op B")),
			OutputPortConfig<bool>("true",_HELP("triggered if out is true")),
			OutputPortConfig<bool>("false",_HELP("triggered if out is false")),
			{0}
		};
		config.sDescription = _HELP( "Do logical operation on input ports and outputs result to [out] port. True port is triggered if the result was true, otherwise the false port is triggered." );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_bResult = 2; // forces output!
		case eFE_Activate:
			{
				bool a = GetPortBool(pActInfo,0);
				bool b = GetPortBool(pActInfo,1);
				int result = Execute(a,b)? 1 : 0;
				if (result != m_bResult)
				{
					m_bResult = result;
					ActivateOutput( pActInfo,0,result );
					if(result)
						ActivateOutput( pActInfo,1,true );
					else
						ActivateOutput( pActInfo,2,true );
				}
			}
		};
	};

private:
	virtual bool Execute( bool a, bool b ) = 0;

	int8 m_bResult;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_AND : public CLogicNode
{
public:
	CFlowNode_AND( SActivationInfo * pActInfo ) : CLogicNode(pActInfo) {}
	IFlowNodePtr Clone(SActivationInfo *pActInfo) { return new CFlowNode_AND(pActInfo); }

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
private:
	bool Execute( bool a, bool b ) { return a && b; }
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_OR : public CLogicNode
{
public:
	CFlowNode_OR( SActivationInfo * pActInfo ) : CLogicNode(pActInfo) {}
	IFlowNodePtr Clone(SActivationInfo *pActInfo) { return new CFlowNode_OR(pActInfo); }

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
private:
	bool Execute( bool a, bool b ) { return a || b; }
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_XOR : public CLogicNode
{
public:
	CFlowNode_XOR( SActivationInfo * pActInfo ) : CLogicNode(pActInfo) {}
	IFlowNodePtr Clone(SActivationInfo *pActInfo) { return new CFlowNode_XOR(pActInfo); }

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
private:
	bool Execute( bool a, bool b ) { return a ^ b; }
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_NOT : public CFlowBaseNode
{
public:
	CFlowNode_NOT( SActivationInfo * pActInfo ) {};
	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>( "in",_HELP("out = not in") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out",_HELP("out = not in")),
			{0}
		};
		config.sDescription = _HELP( "Inverts [in] port, [out] port will output true when [in] is false, and will output false when [in] is true" );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				bool a = GetPortBool(pActInfo,0);
				bool result = !a;
				ActivateOutput( pActInfo,0,result );
			}
		};
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_OnChange : public CFlowBaseNode
{
public:
	CFlowNode_OnChange( SActivationInfo * pActInfo ) { m_bCurrent = false; };
	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo ) { return new CFlowNode_OnChange(pActInfo); }
	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("in"),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("out"),
			{0}
		};
		config.sDescription = _HELP( "Only send [in] value into the [out] when it is different from the previous value" );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			{
				bool a = GetPortBool(pActInfo,0);
				if (a != m_bCurrent)
				{
					ActivateOutput( pActInfo,0,a );
					m_bCurrent = a;
				}
			}
		};
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
private:
	bool m_bCurrent;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Any : public CFlowBaseNode
{
public:
	static const int NUM_INPUTS = 6;

	CFlowNode_Any( SActivationInfo * pActInfo ) {}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("in1", _HELP("Input 1")),
			InputPortConfig_AnyType("in2", _HELP("Input 2")),
			InputPortConfig_AnyType("in3", _HELP("Input 3")),
			InputPortConfig_AnyType("in4", _HELP("Input 4")),
			InputPortConfig_AnyType("in5", _HELP("Input 5")),
			InputPortConfig_AnyType("in6", _HELP("Input 6")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("out", _HELP("Output")),
			{0}
		};
		config.sDescription = _HELP("Port joiner - triggers it's output when any input is triggered");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			for (int i=0; i<NUM_INPUTS; i++)
			{
				if (IsPortActive(pActInfo,i))
					ActivateOutput(pActInfo,0,pActInfo->pInputPorts[i]);
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
class CFlowNode_Blocker : public CFlowBaseNode
{
public:

	CFlowNode_Blocker( SActivationInfo * pActInfo ) {}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool> ( "Block", false, _HELP("If true blocks [In] data. Otherwise passes [In] to [Out]")),
			InputPortConfig_AnyType("In", _HELP("Input")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Output")),
			{0}
		};
		config.sDescription = _HELP("Blocker - If enabled blocks [In] signals, otherwise passes them to [Out]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			if (IsPortActive(pActInfo, 1) && !GetPortBool(pActInfo, 0))
			{
				ActivateOutput(pActInfo,0,GetPortAny(pActInfo, 1));
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
class CFlowNode_LogicGate : public CFlowBaseNode
{
protected:
	bool m_bClosed;

public:
	CFlowNode_LogicGate( SActivationInfo * pActInfo ) : m_bClosed( false ) {}

	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo )
	{
		return new CFlowNode_LogicGate( pActInfo );
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		ser.Value( "m_bClosed", m_bClosed );
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("In", _HELP("Input")),
			InputPortConfig<bool> ( "Closed", false, _HELP("If true blocks [In] data. Otherwise passes [In] to [Out]")),
			InputPortConfig_Void  ( "Open", _HELP("Sets [Closed] to false.")),
			InputPortConfig_Void  ( "Close", _HELP("Sets [Closed] to true.")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Output")),
			{0}
		};
		config.sDescription = _HELP("Gate - If closed blocks [In] signals, otherwise passes them to [Out]");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
		case eFE_Initialize:
			if ( IsPortActive(pActInfo, 1) )
				m_bClosed = GetPortBool( pActInfo, 1 );
			if ( IsPortActive(pActInfo, 2) )
				m_bClosed = false;
			if ( IsPortActive(pActInfo, 3) )
				m_bClosed = true;
			if ( IsPortActive(pActInfo, 0) && !m_bClosed )
				ActivateOutput( pActInfo, 0, GetPortAny(pActInfo, 0) );
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_All : public CFlowBaseNode
{
public:
	static const int NUM_INPUTS = 6;

	CFlowNode_All( SActivationInfo * pActInfo ) : m_inputCount(0)
	{
		ResetState();
	}

	void	ResetState()
	{
		for (unsigned i = 0; i < NUM_INPUTS; i++)
			m_triggered[i] = false;
		m_outputTrig = false;
	}

	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo )
	{
		CFlowNode_All* clone = new CFlowNode_All(pActInfo);
		clone->m_inputCount = m_inputCount;
		return clone;
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		char name[32];
		ser.Value("m_inputCount", m_inputCount);
		ser.Value("m_outputTrig", m_outputTrig);
		for (int i=0; i<NUM_INPUTS; ++i)
		{
			sprintf( name,"m_triggered_%d",i );
			ser.Value(name, m_triggered[i]);
		}
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("in1", _HELP("Input 1")),
			InputPortConfig_Void("in2", _HELP("Input 2")),
			InputPortConfig_Void("in3", _HELP("Input 3")),
			InputPortConfig_Void("in4", _HELP("Input 4")),
			InputPortConfig_Void("in5", _HELP("Input 5")),
			InputPortConfig_Void("in6", _HELP("Input 6")),
			InputPortConfig_Void("Reset", _HELP("Reset")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Out", _HELP("Output")),
			{0}
		};
		config.sDescription = _HELP("All - Triggers output when all connected inputs are triggered.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
	{
		switch (event)
		{
		case eFE_ConnectInputPort:
			// Count the number if inputs connected.
			if (pActInfo->connectPort < NUM_INPUTS)
				m_inputCount++;
			break;
		case eFE_DisconnectInputPort:
			// Count the number if inputs connected.
			if (pActInfo->connectPort < NUM_INPUTS)
				m_inputCount--;
			break;
		case eFE_Initialize:
			ResetState();
			// Fall through to check the input.
		case eFE_Activate:
			// Inputs
			int	ntrig = 0;
			for (int i=0; i<NUM_INPUTS; i++)
			{
				if (!m_triggered[i] && IsPortActive(pActInfo,i))
					m_triggered[i] = (event == eFE_Activate);
				if (m_triggered[i])
					ntrig++;
			}
			// Reset
			if (IsPortActive(pActInfo, NUM_INPUTS))
			{
				ResetState();
				ntrig = 0;
			}
			// If all inputs have been triggered, trigger output.
			// make sure we actually have connected inputs!
			if (!m_outputTrig && m_inputCount > 0 && ntrig == m_inputCount)
			{
				ActivateOutput(pActInfo,0,true);
				m_outputTrig = (event == eFE_Activate);
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	int		m_inputCount;
	bool	m_triggered[NUM_INPUTS];
	bool	m_outputTrig;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_LogicOnce : public CFlowBaseNode
{
public:
	CFlowNode_LogicOnce( SActivationInfo * pActInfo ) : m_bTriggered(false)
	{
	}

	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo )
	{
		return new CFlowNode_All(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		ser.Value("m_bTriggered", m_bTriggered);
	}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("Input1"),
			InputPortConfig_AnyType("Input2"),
			InputPortConfig_AnyType("Input3"),
			InputPortConfig_AnyType("Input4"),
			InputPortConfig_AnyType("Input5"),
			InputPortConfig_AnyType("Input6"),
			InputPortConfig_Void("Reset", _HELP("Reset (and allow new trigger)") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out", _HELP("Output")),
			{0}
		};
		config.sDescription = _HELP("Once - Triggers only once and passes from activated Input port to output port [Out].");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_bTriggered = false;
			break;
		case eFE_Activate:
			if (m_bTriggered == false)
			{
				for (int i=0; i<6; i++)
				{
					if (IsPortActive(pActInfo, i))
					{
						ActivateOutput(pActInfo, 0, GetPortAny(pActInfo, i));
						m_bTriggered = true;
						break;
					}
				}
			}
			if (IsPortActive(pActInfo, 6))
				m_bTriggered = false;
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	bool	m_bTriggered;
};


//////////////////////////////////////////////////////////////////////////
class CFlowNode_RandomSelect : public CFlowBaseNode
{
public:

	CFlowNode_RandomSelect( SActivationInfo * pActInfo ) {}

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("In", _HELP("Input")),
			InputPortConfig<int>("outMin", _HELP("Min number of outputs to activate.")),
			InputPortConfig<int>("outMax", _HELP("Max number of outputs to activate.")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out1", _HELP("Output1")),
			OutputPortConfig_AnyType("Out2", _HELP("Output2")),
			OutputPortConfig_AnyType("Out3", _HELP("Output3")),
			OutputPortConfig_AnyType("Out4", _HELP("Output4")),
			OutputPortConfig_AnyType("Out5", _HELP("Output5")),
			OutputPortConfig_AnyType("Out6", _HELP("Output6")),
			{0}
		};
		config.sDescription = _HELP("RandomSelect - Passes the activated input value to a random amount [outMin <= random <= outMax] outputs.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				int minOut = GetPortInt(pActInfo,1);
				int maxOut = GetPortInt(pActInfo,2);

				minOut = CLAMP(minOut, 0, 6);
				maxOut = CLAMP(maxOut, 0, 6);
				if (maxOut < minOut) 
					std::swap(minOut, maxOut);

				int n = minOut + Random(maxOut - minOut + 1);

				// Collect the outputs to use
				static int	out[6];
				for (unsigned i = 0; i < 6; i++)
					out[i] = -1;
				int nout = 0;
				for (int i = 0; i < 6; i++)
				{
					if (IsOutputConnected(pActInfo, i))
					{
						out[nout] = i;
						nout++;
					}
				}
				if (n > nout)
					n = nout;

				// Shuffle
				for (int i = 0; i < n; i++)
					std::swap(out[i], out[Random(nout)]);

				// Set outputs.
				for (int i = 0; i < n; i++)
				{
					if (out[i] == -1)
						continue;
					ActivateOutput(pActInfo,out[i],GetPortAny(pActInfo, 0));
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
class CFlowNode_RandomTrigger : public CFlowBaseNode
{
public:

	static const int NUM_OUTPUTS = 8;

	CFlowNode_RandomTrigger( SActivationInfo * pActInfo ) : m_nOutputCount(0) { Init(); Reset(); }

	enum INPUTS
	{
		EIP_Input = 0,
		EIP_Reset,
	};

	virtual void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("In", _HELP("Input")),
			InputPortConfig_Void("Reset", _HELP("Reset randomness")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("Out1", _HELP("Output1")),
			OutputPortConfig_AnyType("Out2", _HELP("Output2")),
			OutputPortConfig_AnyType("Out3", _HELP("Output3")),
			OutputPortConfig_AnyType("Out4", _HELP("Output4")),
			OutputPortConfig_AnyType("Out5", _HELP("Output5")),
			OutputPortConfig_AnyType("Out6", _HELP("Output6")),
			OutputPortConfig_AnyType("Out7", _HELP("Output6")),
			OutputPortConfig_AnyType("Out8", _HELP("Output6")),
			OutputPortConfig_Void("Done", _HELP("Triggered after all [connected] outputs have triggered")),
			{0}
		};
		config.sDescription = _HELP("RandomTrigger - On each [In] trigger, triggers one of the connected outputs in random order.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo * pActInfo )
	{
		switch (event)
		{
		case eFE_ConnectOutputPort:
			if (pActInfo->connectPort < NUM_OUTPUTS)
			{
				// check if already connected
				for (int i=0; i<m_nOutputCount; ++i)
				{
					if (m_nConnectedPorts[i] == pActInfo->connectPort) 
						return;
				}
				m_nConnectedPorts[m_nOutputCount++] = pActInfo->connectPort;
				Reset();
			}
			break;
		case eFE_DisconnectOutputPort:
			if (pActInfo->connectPort < NUM_OUTPUTS)
			{
				for (int i=0; i<m_nOutputCount; ++i)
				{
					// check if really connected
					if (m_nConnectedPorts[i] == pActInfo->connectPort)
					{
						m_nConnectedPorts[i] = m_nPorts[m_nOutputCount]; // copy last value to here
						m_nConnectedPorts[m_nOutputCount] = -1;
						--m_nOutputCount;
						Reset();
						return;
					}
				}
			}
			break;
		case eFE_Initialize:
			Reset();
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Reset))
			{
				Reset();
			}
			if (IsPortActive(pActInfo, EIP_Input))
			{
				int numCandidates = m_nOutputCount - m_nTriggered;
				if (numCandidates <= 0)
					return;
				const int cand = Random(numCandidates);
				const int whichPort = m_nPorts[cand];
				m_nPorts[cand] = m_nPorts[numCandidates-1];
				m_nPorts[numCandidates-1] = -1;
				++m_nTriggered;
				assert (whichPort >= 0 && whichPort < NUM_OUTPUTS);
				// CryLogAlways("CFlowNode_RandomTrigger: Activating %d", whichPort);
				ActivateOutput(pActInfo, whichPort, GetPortAny(pActInfo, EIP_Input));
				assert (m_nTriggered > 0 && m_nTriggered <= m_nOutputCount);
				if (m_nTriggered == m_nOutputCount)
				{
					// CryLogAlways("CFlowNode_RandomTrigger: Done.");
					// Done
					ActivateOutput(pActInfo, NUM_OUTPUTS, true);
					Reset();
				}
			}
			break;
		}
	}

	void Init()
	{
		for (int i=0; i<NUM_OUTPUTS; ++i)
		{
			m_nConnectedPorts[i] = -1;
		}
	}

	void Reset()
	{
		memcpy(m_nPorts, m_nConnectedPorts, sizeof(m_nConnectedPorts));
		m_nTriggered = 0;
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo *pActInfo )
	{
		CFlowNode_RandomTrigger* pClone = new CFlowNode_RandomTrigger(pActInfo);
		// copy connected ports to cloned node
		// because atm. we don't send the  eFE_ConnectOutputPort or eFE_DisconnectOutputPort
		// to cloned graphs (see CFlowGraphBase::Clone)
		memcpy(pClone->m_nConnectedPorts, m_nConnectedPorts, sizeof(m_nConnectedPorts));
		pClone->Reset();
		return pClone;
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		char name[32];
		ser.Value("m_nOutputCount", m_nOutputCount);
		ser.Value("m_nTriggered", m_nTriggered);
		for (int i=0; i<NUM_OUTPUTS; ++i)
		{
			sprintf( name,"m_nPorts_%d",i );
			ser.Value(name, m_nPorts[i]);
		}
		// the m_nConnectedPorts must not be serialized. it's generated automatically
		// sanity check
		if (ser.IsReading())
		{
			bool bNeedReset = false;
			for (int i=0; i<NUM_OUTPUTS && !bNeedReset; ++i)
			{
				bool bFound = false;
				for (int j=0; j<NUM_OUTPUTS && !bFound; ++j)
				{
					bFound = (m_nPorts[i] == m_nConnectedPorts[j]);
				}
				bNeedReset = !bFound;
			}
			// if some of the serialized port can not be found, reset
			if (bNeedReset)
				Reset();
		}
	}

	int m_nConnectedPorts[NUM_OUTPUTS]; // array with port-numbers which are connected.
	int m_nPorts[NUM_OUTPUTS]; // permutation of m_nConnectedPorts array with m_nOutputCount valid entries
	int m_nTriggered;
	int m_nOutputCount;
};


REGISTER_FLOW_NODE( "Logic:AND",CFlowNode_AND );
REGISTER_FLOW_NODE( "Logic:OR",CFlowNode_OR );
REGISTER_FLOW_NODE( "Logic:XOR",CFlowNode_XOR );
REGISTER_FLOW_NODE_SINGLETON( "Logic:NOT",CFlowNode_NOT );
REGISTER_FLOW_NODE( "Logic:OnChange",CFlowNode_OnChange );
REGISTER_FLOW_NODE_SINGLETON( "Logic:Any",CFlowNode_Any );
REGISTER_FLOW_NODE_SINGLETON( "Logic:Blocker",CFlowNode_Blocker );
REGISTER_FLOW_NODE( "Logic:All",CFlowNode_All );
REGISTER_FLOW_NODE_SINGLETON( "Logic:RandomSelect",CFlowNode_RandomSelect );
REGISTER_FLOW_NODE( "Logic:RandomTrigger",CFlowNode_RandomTrigger );
REGISTER_FLOW_NODE( "Logic:Once",CFlowNode_LogicOnce );
REGISTER_FLOW_NODE( "Logic:Gate", CFlowNode_LogicGate );
