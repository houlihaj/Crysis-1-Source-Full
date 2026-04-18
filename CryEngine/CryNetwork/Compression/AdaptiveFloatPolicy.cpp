#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "AdaptiveFloat.h"

class CAdaptiveFloatPolicy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return m_float.Load(node, filename, "Params");
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		m_float.ReadMemento(in);
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		m_float.WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		m_float.NoMemento();
	}

	bool ReadValue( CCommInputStream& in, float& value, CArithModel * pModel, uint32 age ) const
	{
		return m_float.ReadValue(in, value, age);
	}
	bool WriteValue( CCommOutputStream& out, float value, CArithModel * pModel, uint32 age ) const
	{
		m_float.WriteValue(out, value, age);
		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("AdaptiveFloatPolicy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("AdaptiveFloatPolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CAdaptiveFloatPolicy");
		pSizer->Add(*this);
	}

private:
	CAdaptiveFloat m_float;
};

REGISTER_COMPRESSION_POLICY(CAdaptiveFloatPolicy, "AdaptiveFloat");
