#include "StdAfx.h"
#include "AdaptiveVelocity.h"
#include "ArithPrimitives.h"

bool CAdaptiveVelocity::Load( XmlNodeRef node, const string& filename, const string& child )
{
	bool ok = true;
	if (m_quantizer.Load(node, filename, child, eFQM_TruncateLeft))
	{
		int height = 1024000;
		node->getAttr("height", height);
		if (height < 0)
			ok = false;

		if (ok)
		{
			m_nHeight = height;
		}
	}
	else
	{
		ok = false;
	}
	return ok;
}

void CAdaptiveVelocity::ReadMemento( CByteInputStream& stm ) const
{
	m_boolCompress.ReadMemento(stm);
	m_nLastQuantized = stm.GetTyped<uint32>();
}

void CAdaptiveVelocity::WriteMemento( CByteOutputStream& stm ) const
{
	m_boolCompress.WriteMemento(stm);
	stm.PutTyped<uint32>() = m_nLastQuantized;
}

void CAdaptiveVelocity::NoMemento() const
{
	m_boolCompress.NoMemento();
	m_nLastQuantized = 0;
}

bool CAdaptiveVelocity::ReadValue(CCommInputStream& stm, float& value) const
{
	uint32 left, right;
	uint32 quantized = left = right = m_nLastQuantized;

	bool moved = m_boolCompress.ReadValue(stm);

	if(moved)
	{
		int32 searchArea = m_quantizer.GetMaxQuantizedValue() / int32(50);
		if(searchArea == 0)
			searchArea = 1;
		left = (uint32)((int64(m_nLastQuantized) - searchArea > 0)? m_nLastQuantized - searchArea : 0);
		right = (uint32)((uint64(m_nLastQuantized) + searchArea < m_quantizer.GetMaxQuantizedValue())? m_nLastQuantized + searchArea : m_quantizer.GetMaxQuantizedValue());

		if (!SquarePulseProbabilityReadImproved(quantized, stm, left, right, 1024, m_quantizer.GetMaxQuantizedValue(), 95, m_quantizer.GetNumBits()))
			return false;
		m_nLastQuantized = quantized;
	}

	value = m_quantizer.Dequantize(quantized);
	return true;
}

void CAdaptiveVelocity::WriteValue(CCommOutputStream& stm, float value) const
{
	//quantize value
	uint32 quantized = m_quantizer.Quantize(value);
	uint32 left, right;
	left = right = m_nLastQuantized;

	bool moved = (quantized != m_nLastQuantized)? true : false;
	m_boolCompress.WriteValue(stm, moved);
	if(moved)
	{
		int32 searchArea = m_quantizer.GetMaxQuantizedValue() / int32(50);
		if(searchArea == 0)
			searchArea = 1;
		left = (uint32)((int64(m_nLastQuantized) - searchArea > 0)? m_nLastQuantized - searchArea : 0);
		right = (uint32)((uint64(m_nLastQuantized) + searchArea < m_quantizer.GetMaxQuantizedValue())? m_nLastQuantized + searchArea : m_quantizer.GetMaxQuantizedValue());
		SquarePulseProbabilityWriteImproved(stm, quantized, left, right, 1024, m_quantizer.GetMaxQuantizedValue(), 95, m_quantizer.GetNumBits());

		m_nLastQuantized = quantized;
	}
}
