#include "StdAfx.h"
#include "CompressionManager.h"
#include "ICompressionPolicy.h"
#include "Quantizer.h"
#include "BoolCompress.h"

class CJumpyPolicy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return m_quantizer.Load(node, filename);
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		m_boolCompress.ReadMemento(in);
		m_lastValue = in.GetTyped<uint32>();
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		m_boolCompress.WriteMemento(out);
		out.PutTyped<uint32>() = m_lastValue;
		return true;
	}

	void NoMemento() const
	{
		m_boolCompress.NoMemento();
		m_lastValue = 0;
	}

	bool ReadValue( CCommInputStream& in, float& value, CArithModel * pModel, uint32 age ) const
	{
		bool changed = m_boolCompress.ReadValue(in);
		if(changed)
		{
			uint32 quantized = in.ReadBits(m_quantizer.GetNumBits());
			m_lastValue = quantized;
		}
		value = m_quantizer.Dequantize(m_lastValue);

		return true;
	}
	bool WriteValue( CCommOutputStream& out, float value, CArithModel * pModel, uint32 age ) const
	{
		uint32 quantized = m_quantizer.Quantize(value);
		bool changed = false;
		if(quantized != m_lastValue)
			changed = true;
		m_boolCompress.WriteValue(out, changed);
		if(changed)
		{
			out.WriteBits(quantized, m_quantizer.GetNumBits());
			m_lastValue = quantized;
		}

		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("JumpyPolicy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("JumpyPolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CJumpyPolicy");
		pSizer->Add(*this);
	}

private:
	CQuantizer m_quantizer;
	CBoolCompress m_boolCompress;
	mutable uint32 m_lastValue;
};

REGISTER_COMPRESSION_POLICY(CJumpyPolicy, "Jumpy");
