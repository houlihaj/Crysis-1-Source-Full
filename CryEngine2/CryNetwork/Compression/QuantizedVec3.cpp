#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "Quantizer.h"

class CQuantizedVec3Policy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return 
			m_floats[0].Load(node, filename, "XParams") &&
			m_floats[1].Load(node, filename, "YParams") &&
			m_floats[2].Load(node, filename, "ZParams");
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

	bool ReadValue( CCommInputStream& in, Vec3& value, CArithModel * pModel, uint32 age ) const
	{
		for (int i=0; i<3; i++)
		{
			uint32 q = in.ReadBits(m_floats[i].GetNumBits());
			value[i] = m_floats[i].Dequantize(q);
		}
		return true;
	}
	bool WriteValue( CCommOutputStream& out, Vec3 value, CArithModel * pModel, uint32 age ) const
	{
		for (int i=0; i<3; i++)
		{
			uint32 q = m_floats[i].Quantize(value[i]);
			out.WriteBits(q, m_floats[i].GetNumBits());
		}
		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("QuantizedVec3Policy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("QuantizedVec3Policy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CQuantizedVec3Policy");
		pSizer->Add(*this);
	}

private:
	CQuantizer m_floats[3];
};

REGISTER_COMPRESSION_POLICY(CQuantizedVec3Policy, "QuantizedVec3");
