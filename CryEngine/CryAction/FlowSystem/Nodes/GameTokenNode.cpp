////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   GameTokenNode.cpp
//  Version:     v1.00
//  Created:     03-11-2005 by AlexL
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <FlowSystem/Nodes/FlowBaseNode.h>
#include <IGameTokens.h>

#include "CryAction.h"

namespace 
{
	IGameTokenSystem * GetIGameTokenSystem()
	{
		return CCryAction::GetCryAction()->GetIGameTokenSystem();
	}

	const int bForceCreateDefault = false; // create non-existing game tokens, default false for the moment
};

class CSetGameTokenFlowNode : public CFlowBaseNode /*, public IGameTokenEventListener */
{
public:
	CSetGameTokenFlowNode( SActivationInfo * pActInfo ) : m_pCachedToken(NULL)
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CSetGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = 0;
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void ("Trigger",_HELP("Trigger this input to actually set the game token value"), _HELP("Trigger")),
			InputPortConfig<string> ("gametoken_Token", _HELP("Game token to set"), _HELP("Token")),
			InputPortConfig<string> ("Value", _HELP("Value to set")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType ("OutValue", _HELP("Value which was set (debug)")),
			{0}
		};
		config.sDescription = _HELP("Set the value of a game token");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = 0;
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, 1)) 
				{
					CacheToken(pActInfo);
				}
				if (IsPortActive(pActInfo, 0)) 
				{
					if (m_pCachedToken == NULL)
						CacheToken(pActInfo);

					if (m_pCachedToken != NULL) 
					{
						m_pCachedToken->SetValue(GetPortAny(pActInfo, 2));
						TFlowInputData data;
						m_pCachedToken->GetValue(data);
						ActivateOutput(pActInfo, 0, data);
					}
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
	void CacheToken(SActivationInfo* pActInfo, bool bForceCreate = bForceCreateDefault)
	{
		m_pCachedToken = GetIGameTokenSystem()->FindToken(GetPortString(pActInfo, 1));
		if (m_pCachedToken == 0 && bForceCreate)
			m_pCachedToken = GetIGameTokenSystem()->SetOrCreateToken(GetPortString(pActInfo,1), TFlowInputData(string("<undefined>"), false));
		if (m_pCachedToken == 0) {
			GameWarning("[flow] SetGameToken: cannot find token '%s'", GetPortString(pActInfo, 1).c_str());
		}
	}

private:
	IGameToken* m_pCachedToken;
};

class CGetGameTokenFlowNode : public CFlowBaseNode /*, public IGameTokenEventListener */
{
public:
	CGetGameTokenFlowNode( SActivationInfo * pActInfo ) : m_pCachedToken(NULL)
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CGetGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = 0;
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void    ("Trigger",_HELP("Trigger this input to actually get the game token value"), _HELP("Trigger")),
			InputPortConfig<string> ("gametoken_Token", _HELP("Game token to set"), _HELP("Token")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType ("OutValue", _HELP("Value of the game token")),
			{0}
		};
		config.sDescription = _HELP("Get the value of a game token");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = 0;
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, 1))
				{
					CacheToken(pActInfo);
				}
				if (IsPortActive(pActInfo, 0))
				{
					if (m_pCachedToken == NULL)
						CacheToken(pActInfo);

					if (m_pCachedToken != NULL)
					{
						TFlowInputData data;
						m_pCachedToken->GetValue(data);
						ActivateOutput(pActInfo, 0, data);
					}
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
	void CacheToken(SActivationInfo* pActInfo, bool bForceCreate = bForceCreateDefault)
	{
		m_pCachedToken = GetIGameTokenSystem()->FindToken(GetPortString(pActInfo, 1));
		if (m_pCachedToken == 0 && bForceCreate)
			m_pCachedToken = GetIGameTokenSystem()->SetOrCreateToken(GetPortString(pActInfo,1), TFlowInputData(string("<undefined>"), false));
		if (m_pCachedToken == 0) {
			GameWarning("[flow] GetGameToken: cannot find token '%s'", GetPortString(pActInfo, 1).c_str());
		}
	}

private:
	IGameToken* m_pCachedToken;
};

class CCheckGameTokenFlowNode : public CFlowBaseNode /*, public IGameTokenEventListener */
{
public:
	CCheckGameTokenFlowNode( SActivationInfo * pActInfo ) : m_pCachedToken(NULL)
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CCheckGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = 0;
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void    ("Trigger",_HELP("Trigger this input to actually get the game token value"), _HELP("Trigger")),
			InputPortConfig<string> ("gametoken_Token", _HELP("Game token to set"), _HELP("Token")),
			InputPortConfig<string> ("CheckValue", _HELP("Value to compare the token with")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType ("OutValue", _HELP("Value of the token")),
				OutputPortConfig<bool> ("Result", _HELP("TRUE if equals, FALSE otherwise")),
				OutputPortConfig_Void  ("True", _HELP("Triggered if equal")),
				OutputPortConfig_Void  ("False", _HELP("Triggered if not equal")),
			{0}
		};
		config.sDescription = _HELP("Check if the value of a game token equals to a value");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = 0;
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, 1)) 
				{
					CacheToken(pActInfo);
				}
				if (IsPortActive(pActInfo, 0)) 
				{
					if (m_pCachedToken == NULL)
						CacheToken(pActInfo);

					if (m_pCachedToken != NULL) 
					{
						// now this is a messy thing.
						// we use TFlowInputData to do all the work, because the 
						// game tokens uses them as well. 
						// does for the same values we get the same converted strings
						TFlowInputData tokenData;
						m_pCachedToken->GetValue(tokenData);
						TFlowInputData checkData (tokenData);
						checkData.SetValueWithConversion(GetPortString(pActInfo, 2));
						string tokenString,checkString;
						tokenData.GetValueWithConversion(tokenString);
						checkData.GetValueWithConversion(checkString);
						ActivateOutput(pActInfo, 0, tokenData);
						bool equal = tokenString == checkString;
						ActivateOutput(pActInfo, 1, equal);
						// trigger either the true or false port depending on comparism
						ActivateOutput(pActInfo, 3 - static_cast<int> (equal), true);
					}
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
	void CacheToken(SActivationInfo* pActInfo, bool bForceCreate = bForceCreateDefault)
	{
		m_pCachedToken = GetIGameTokenSystem()->FindToken(GetPortString(pActInfo, 1));
		if (m_pCachedToken == 0 && bForceCreate)
			m_pCachedToken = GetIGameTokenSystem()->SetOrCreateToken(GetPortString(pActInfo,1), TFlowInputData(string("<undefined>"), false));
		if (m_pCachedToken == 0) {
			GameWarning("[flow] CheckGameToken: cannot find token '%s'", GetPortString(pActInfo, 1).c_str());
		}
	}

private:
	IGameToken* m_pCachedToken;
};

class CGameTokenFlowNode : public CFlowBaseNode, public IGameTokenEventListener 
{
public:
	CGameTokenFlowNode( SActivationInfo * pActInfo ) : m_pCachedToken(0), m_actInfo(*pActInfo)
	{
	}

	~CGameTokenFlowNode()
	{
		Unregister();
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			// recache and register at token
			CacheToken(pActInfo);
		}
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType ("In",_HELP("Feed this port to set the game token value"), _HELP("Input")),
			InputPortConfig<string> ("gametoken_Token", _HELP("GameToken to set/get"), _HELP("Token")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType ("Out", _HELP("Outputs the token's value on every change"), _HELP("Output")),
			{0}
		};
		config.sDescription = _HELP("GameToken set/get");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, 1)) // Token port
				{ 
					CacheToken(pActInfo);
					if (m_pCachedToken != 0)
					{
						TFlowInputData data;
						m_pCachedToken->GetValue(data);
						ActivateOutput(pActInfo, 0, data);
					}
				}
				if (event == eFE_Activate)
				{
					if (IsPortActive(pActInfo, 0)) // Input port
					{
						if (m_pCachedToken == 0)
							CacheToken(pActInfo);

						if (m_pCachedToken != 0)
							// the SetValue will call us as back and set the output value
							m_pCachedToken->SetValue(GetPortAny(pActInfo, 0));
					}
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
	void OnGameTokenEvent( EGameTokenEvent event,IGameToken *pGameToken )
	{
		assert (pGameToken == m_pCachedToken);
		if (event == EGAMETOKEN_EVENT_CHANGE)
		{
			TFlowInputData data;
			m_pCachedToken->GetValue(data);
			ActivateOutput(&m_actInfo, 0, data);
		}
		else if (event == EGAMETOKEN_EVENT_DELETE)
		{
			// no need to unregister
			m_pCachedToken = 0;
		}
	}

	void Register()
	{
		IGameTokenSystem *pGTSys = GetIGameTokenSystem();
		if (m_pCachedToken && pGTSys)
			pGTSys->RegisterListener(m_pCachedToken->GetName(), this);
	}

	void Unregister()
	{
		IGameTokenSystem *pGTSys = GetIGameTokenSystem();
		if (m_pCachedToken && pGTSys)
		{
			pGTSys->UnregisterListener(m_pCachedToken->GetName(), this);
			m_pCachedToken = 0;
		}
	}

	void CacheToken(SActivationInfo* pActInfo, bool bForceCreate=bForceCreateDefault)
	{
		Unregister();	
		IGameTokenSystem *pGTSys = GetIGameTokenSystem();
		if (pGTSys == 0)
		{
			CryLogAlways("[flow] GameToken: GameTokenSystem no more existent?");
			return;
		}
		m_pCachedToken = pGTSys->FindToken(GetPortString(pActInfo, 1));
		if (m_pCachedToken == 0 && bForceCreate)
		{
			m_pCachedToken = pGTSys->SetOrCreateToken(GetPortString(pActInfo,1), TFlowInputData(string("<undefined>"), false));
		}
		if (m_pCachedToken)
			Register();
		else
			GameWarning("[flow] GameToken: cannot find token '%s'", GetPortString(pActInfo, 1).c_str());
	}

private:
	SActivationInfo m_actInfo;
	IGameToken* m_pCachedToken;
};

class CModifyGameTokenFlowNode : public CFlowBaseNode /*, public IGameTokenEventListener */
{
public:
	CModifyGameTokenFlowNode( SActivationInfo * pActInfo ) : m_pCachedToken(NULL)
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CModifyGameTokenFlowNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo *, TSerialize ser)
	{
		if (ser.IsReading()) // forces re-caching
			m_pCachedToken = 0;
	}

	enum EOperation {
		EOP_SET = 0,
		EOP_ADD,
		EOP_SUB,
		EOP_MUL,
		EOP_DIV
	};

	enum EType {
		ET_BOOL = 0,
		ET_INT,
		ET_FLOAT,
		ET_VEC3,
		ET_STRING
	};

	template <class Type>
	struct Helper 
	{
		Helper(TFlowInputData& value, const TFlowInputData& operand) 
		{
			Init(value, operand);
		}

		bool Init(TFlowInputData& value, const TFlowInputData& operand)
		{
			m_ok = value.GetValueWithConversion(m_value);
			m_ok &= operand.GetValueWithConversion(m_operand);
			return m_ok;
		}

		bool m_ok;
		Type m_value;
		Type m_operand;

	};

	template<class Type> bool not_zero(Type& val) { return true; }
	bool not_zero(int& val) { return val != 0; }
	bool not_zero(float& val) { return val != 0.0f; }

	template<class Type> void op_set(Helper<Type>& data)  { if (data.m_ok) data.m_value = data.m_operand; }
	template<class Type> void op_add(Helper<Type>& data)  { if (data.m_ok) data.m_value += data.m_operand; }
	template<class Type> void op_sub(Helper<Type>& data)  { if (data.m_ok) data.m_value -= data.m_operand; }
	template<class Type> void op_mul(Helper<Type>& data)  { if (data.m_ok) data.m_value *= data.m_operand; }
	template<class Type> void op_div(Helper<Type>& data)  { if (data.m_ok && not_zero(data.m_operand)) data.m_value /= data.m_operand; }

	void op_div(Helper<string>& data) {};
	void op_mul(Helper<string>& data) {};
	void op_sub(Helper<string>& data) {};
	void op_mul(Helper<Vec3>& data) {};
	void op_div(Helper<Vec3>& data) {};

	void op_add(Helper<bool>& data) {};
	void op_sub(Helper<bool>& data) {};
	void op_div(Helper<bool>& data) {};
	void op_mul(Helper<bool>& data) {};

	template<class Type> void DoOp(EOperation op, Helper<Type>& data)
	{
		switch (op) {
		case EOP_SET: op_set(data); break;
		case EOP_ADD: op_add(data); break;
		case EOP_SUB: op_sub(data); break;
		case EOP_MUL:	op_mul(data); break;
		case EOP_DIV: op_div(data); break;
		}
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void    ("Trigger",_HELP("Trigger this input to actually get the game token value"), _HELP("Trigger")),
			InputPortConfig<string> ("gametoken_Token", _HELP("Game token to set"), _HELP("Token")),
			InputPortConfig<int>    ("Op", EOP_SET, _HELP("Operation token = token OP value"), _HELP("Operation"), _UICONFIG("enum_int:Set=0,Add=1,Sub=2,Mul=3,Div=4")),
			InputPortConfig<int>    ("Type", ET_STRING, _HELP("Type"), _HELP("Type"), _UICONFIG("enum_int:Bool=0,Int=1,Float=2,Vec3=3,String=4")),
			InputPortConfig<string> ("Value", _HELP("Value to operate with")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType ("OutValue", _HELP("Value of the token")),
			{0}
		};
		config.sDescription = _HELP("Operate on a GameToken - Experimental Friedrich only!");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_pCachedToken = 0;
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, 1)) 
				{
					CacheToken(pActInfo);
				}
				if (IsPortActive(pActInfo, 0)) 
				{
					if (m_pCachedToken == NULL)
						CacheToken(pActInfo);

					if (m_pCachedToken != NULL) 
					{
						TFlowInputData value;
						m_pCachedToken->GetValue(value);
						TFlowInputData operand;
						operand.Set(GetPortString(pActInfo, 4));

						EOperation op = static_cast<EOperation> (GetPortInt(pActInfo, 2));
						EType type = static_cast<EType> (GetPortInt(pActInfo, 3));
						bool ok = true;
						switch (type)
						{
						case ET_BOOL:		{ Helper<bool> h (value, operand); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); }   break;
						case ET_INT:		{ Helper<int> h (value, operand); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); }    break;
						case ET_FLOAT:	{ Helper<float> h (value, operand); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); }  break;
						case ET_VEC3:		{ Helper<Vec3> h (value, operand); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); }   break;
						case ET_STRING:	{ Helper<string> h (value, operand); DoOp(op, h); if (h.m_ok) m_pCachedToken->SetValue(TFlowInputData(h.m_value)); } break;
						}
						m_pCachedToken->GetValue(value);
						ActivateOutput(pActInfo, 0, value);
					}
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
	void CacheToken(SActivationInfo* pActInfo, bool bForceCreate = bForceCreateDefault)
	{
		m_pCachedToken = GetIGameTokenSystem()->FindToken(GetPortString(pActInfo, 1));
		if (m_pCachedToken == 0 && bForceCreate)
			m_pCachedToken = GetIGameTokenSystem()->SetOrCreateToken(GetPortString(pActInfo,1), TFlowInputData(string("<undefined>"), false));
		if (m_pCachedToken == 0) {
			CryLogAlways("[flow] ModifyGameToken: cannot find token '%s'", GetPortString(pActInfo, 1).c_str());
		}
	}

private:
	IGameToken* m_pCachedToken;
};

REGISTER_FLOW_NODE( "Mission:ModifyToken", CModifyGameTokenFlowNode );
REGISTER_FLOW_NODE( "Mission:SetGameToken", CSetGameTokenFlowNode );
REGISTER_FLOW_NODE( "Mission:GetGameToken", CGetGameTokenFlowNode );
REGISTER_FLOW_NODE( "Mission:CheckGameToken", CCheckGameTokenFlowNode );
REGISTER_FLOW_NODE( "Mission:GameToken", CGameTokenFlowNode );
