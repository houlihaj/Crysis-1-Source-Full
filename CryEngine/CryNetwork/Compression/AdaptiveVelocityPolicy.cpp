#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "AdaptiveVelocity.h"

class CAdaptiveVelocityPolicy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return 
			m_floats[0].Load(node, filename, "Params") &&
			m_floats[1].Load(node, filename, "Params") &&
			m_floats[2].Load(node, filename, "Params");
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		for (int i=0; i<3; i++)
			m_floats[i].ReadMemento(in);
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		for (int i=0; i<3; i++)
			m_floats[i].WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		for (int i=0; i<3; i++)
			m_floats[i].NoMemento();
	}

	bool ReadValue( CCommInputStream& in, Vec3& value, CArithModel * pModel, uint32 age ) const
	{
		for (int i=0; i<3; i++)
			if (!m_floats[i].ReadValue(in, value[i]))
				return false;
		return true;
	}
	bool WriteValue( CCommOutputStream& out, Vec3 value, CArithModel * pModel, uint32 age ) const
	{
		for (int i=0; i<3; i++)
			m_floats[i].WriteValue(out, value[i]);
		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("AdaptiveVelocityPolicy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("AdaptiveVelocityPolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CAdaptiveVelocityPolicy");
		pSizer->Add(*this);
	}

private:
	CAdaptiveVelocity m_floats[3];
};

//REGISTER_COMPRESSION_POLICY(CAdaptiveVelocityPolicy, "AdaptiveVelocity");
REGISTER_COMPRESSION_POLICY(CAdaptiveVelocityPolicy, "Velocity");
