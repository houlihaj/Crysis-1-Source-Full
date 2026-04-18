#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "Context/NetContext.h"
#include "BoolCompress.h"
#include "Protocol/CTPEndpoint.h"

class CEntityIdPolicy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return true;
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		m_hasMemento = true;
		m_lastValue = in.GetTyped<SNetObjectID>();
		m_bool.ReadMemento(in);
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		out.PutTyped<SNetObjectID>() = m_lastValue;
		m_bool.WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		m_hasMemento = false;
		m_lastValue = SNetObjectID();
		m_bool.NoMemento();
	}


	bool ReadValue( CCommInputStream& in, EntityId& value, CArithModel * pModel, uint32 age ) const
	{
		SNetObjectID netID = SNetObjectID();
		ReadValue( in, netID, pModel, age );
		//const SContextObject * pObj = pModel->GetNetContext()->GetContextObject( netID );
		SContextObjectRef obj = pModel->GetNetContextState()->GetContextObject( netID );
		if (!obj.main)
		{
#if LOG_ENTITYID_ERRORS
			if (netID && CNetCVars::Get().LogLevel)
			{
				NetLog("Valid network id (%s) sent, but no valid entity id available (probably that entity hasn't yet been bound)", netID.GetText());
				NetLog("  processing message %s", CCTPEndpoint::GetCurrentProcessingMessageDescription());
			}
#endif
			value = 0;
		}
		else
			value = obj.main->userID;
		return true;
	}
	bool WriteValue( CCommOutputStream& out, EntityId value, CArithModel * pModel, uint32 age ) const
	{
		SNetObjectID netID = pModel->GetNetContextState()->GetNetID( value, false );
#if ENABLE_UNBOUND_ENTITY_DEBUGGING
		if (!netID && value)
			NetWarning( "Entity id %.8x is not known in the network system - probably has not been bound", value );
#endif
		return WriteValue(out, netID, pModel, age);
	}
	bool ReadValue( CCommInputStream& in, SNetObjectID& value, CArithModel * pModel, uint32 age ) const
	{
		if (m_hasMemento)
		{
			bool same = m_bool.ReadValue(in);
			if (!same)
				value = pModel->ReadNetId(in);
			else
				value = m_lastValue;
		}
		else
		{
			value = pModel->ReadNetId(in);
		}
		m_lastValue = value;
		return true;
	}
	bool WriteValue( CCommOutputStream& out, SNetObjectID value, CArithModel * pModel, uint32 age ) const
	{
		if (m_hasMemento)
		{
			m_bool.WriteValue( out, m_lastValue == value );
			if (m_lastValue != value)
				pModel->WriteNetId(out, value);
		}
		else
		{
			pModel->WriteNetId(out, value);
		}
		m_lastValue = value;
		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("EntityIdPolicy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("EntityIdPolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CEntityIdPolicy");
		pSizer->Add(*this);
	}

private:
	mutable bool m_hasMemento;
	mutable SNetObjectID m_lastValue;
	mutable CBoolCompress m_bool;
};

class CSimpleEntityIdPolicy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return true;
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		return true;
	}

	void NoMemento() const
	{
	}

	bool ReadValue( CCommInputStream& in, EntityId& value, CArithModel * pModel, uint32 age ) const
	{
		SNetObjectID netID;
		ReadValue( in, netID, pModel, age );
		//const SContextObject * pObj = pModel->GetNetContext()->GetContextObject( netID );
		SContextObjectRef obj = pModel->GetNetContextState()->GetContextObject( netID );
		if (!obj.main)
		{
#if LOG_ENTITYID_ERRORS
			if (netID && CNetCVars::Get().LogLevel)
			{
				NetLog("Valid network id (%s) sent, but no valid entity id available (probably that entity hasn't yet been bound)", netID.GetText());
				NetLog("  processing message %s", CCTPEndpoint::GetCurrentProcessingMessageDescription());
			}
#endif
			value = 0;
		}
		else
			value = obj.main->userID;
		return true;
	}
	bool WriteValue( CCommOutputStream& out, EntityId value, CArithModel * pModel, uint32 age ) const
	{
		SNetObjectID netID = pModel->GetNetContextState()->GetNetID( value, false );
#if ENABLE_UNBOUND_ENTITY_DEBUGGING
		if (!netID && value)
			NetWarning( "Entity id %.8x is not known in the network system - probably has not been bound", value );
#endif
		return WriteValue(out, netID, pModel, age);
	}
	bool ReadValue( CCommInputStream& in, SNetObjectID& value, CArithModel * pModel, uint32 age ) const
	{
		value.id = in.ReadBits(16);
		value.salt = in.ReadBits(16);
		return true;
	}
	bool WriteValue( CCommOutputStream& out, SNetObjectID value, CArithModel * pModel, uint32 age ) const
	{
		out.WriteBits(value.id, 16);
		out.WriteBits(value.salt, 16);
		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("EntityIdPolicy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("EntityIdPolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CSimpleEntityIdPolicy");
		pSizer->Add(*this);
	}
};

REGISTER_COMPRESSION_POLICY(CEntityIdPolicy, "EntityId");
REGISTER_COMPRESSION_POLICY(CSimpleEntityIdPolicy, "SimpleEntityId");
