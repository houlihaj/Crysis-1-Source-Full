#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "BoolCompress.h"

class CAdaptiveBoolPolicy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return true;
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		m_bool.ReadMemento(in);
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		m_bool.WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		m_bool.NoMemento();
	}

	bool ReadValue( CCommInputStream& in, bool& value, CArithModel * pModel, uint32 age ) const
	{
		value = m_bool.ReadValue(in);
		return true;
	}
	bool WriteValue( CCommOutputStream& out, bool value, CArithModel * pModel, uint32 age ) const
	{
		m_bool.WriteValue(out, value);
		return true;
	}
	bool ReadValue( CCommInputStream& in, SNetObjectID& value, CArithModel * pModel, uint32 age ) const
	{
		value = pModel->ReadNetId(in);
		return true;
	}
	bool WriteValue( CCommOutputStream& out, SNetObjectID value, CArithModel * pModel, uint32 age ) const
	{
		pModel->WriteNetId(out, value);
		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CAdaptiveBoolPolicy");
		pSizer->Add(*this);
	}

private:
	CBoolCompress m_bool;
};

REGISTER_COMPRESSION_POLICY(CAdaptiveBoolPolicy, "AdaptiveBool");
