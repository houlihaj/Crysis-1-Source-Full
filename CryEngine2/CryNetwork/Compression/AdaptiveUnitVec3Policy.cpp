#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "AdaptiveFloat.h"
#include "BoolCompress.h"

class CAdaptiveUnitVec3Policy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return m_x.Load(node, filename, "Params") && m_y.Load(node, filename, "Params");
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		m_x.ReadMemento(in);
		m_y.ReadMemento(in);
		m_z.ReadMemento(in);
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		m_x.WriteMemento(out);
		m_y.WriteMemento(out);
		m_z.WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		m_x.NoMemento();
		m_y.NoMemento();
		m_z.NoMemento();
	}

	bool ReadValue( CCommInputStream& in, Vec3& value, CArithModel * pModel, uint32 age ) const
	{
		if ( !m_x.ReadValue(in, value.x, age) )
			return false;
		if ( !m_y.ReadValue(in, value.y, age) )
			return false;
		bool sign = m_z.ReadValue(in);
		float ssxy = value.x * value.x + value.y * value.y; // square sum of xy
		if (ssxy <= 1.0f)
		{
			float sssxy = sqrtf(1.0f - ssxy); // square root of square sum of xy
			value.z = sign ? sssxy : -sssxy;
		}
		else
			value.z = 0.0f;
		value.Normalize();
		return true;
	}

	bool WriteValue( CCommOutputStream& out, Vec3 value, CArithModel * pModel, uint32 age ) const
	{
		value.Normalize();

		m_x.WriteValue(out, value.x, age);
		m_y.WriteValue(out, value.y, age);
		m_z.WriteValue(out, value.z >= 0.0f);

		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("AdaptiveUnitVec3Policy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("AdaptiveUnitVec3Policy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CAdaptiveUnitVec3Policy");
		pSizer->Add(*this);
	}

private:
	CAdaptiveFloat m_x;
	CAdaptiveFloat m_y;
	CBoolCompress m_z;
};

REGISTER_COMPRESSION_POLICY(CAdaptiveUnitVec3Policy, "AdaptiveUnitVec3");

