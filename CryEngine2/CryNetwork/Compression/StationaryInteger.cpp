#include "StdAfx.h"
#include "StationaryInteger.h"
#include <limits>

bool CStationaryInteger::Load(XmlNodeRef node, const string& filename, const string& child)
{
	bool ok = true;
	if (XmlNodeRef params = node->findChild(child))
	{
		int64 mn, mx;
		ok &= params->getAttr("min", mn);
		ok &= params->getAttr("max", mx);
		ok &= mx > mn;
		if ( (mx > std::numeric_limits<int32>::max() || mn < std::numeric_limits<int32>::min()) &&
			(mx > std::numeric_limits<uint32>::max() || mn < std::numeric_limits<uint32>::min()) )
			ok = false;
		if (ok)
		{
			m_nMin = mn;
			m_nMax = mx+1;
		}
	}
	else
	{
		NetWarning("StationaryInteger couldn't find parameters named %s at %s:%d", child.c_str(), filename.c_str(), node->getLine());
		ok = false;
	}
	return ok;
}

bool CStationaryInteger::WriteMemento( CByteOutputStream& stm ) const
{
	stm.PutTyped<uint32>() = m_oldValue;
	stm.PutTyped<uint8>() = m_probabilitySame;
	return true;
}

bool CStationaryInteger::ReadMemento( CByteInputStream& stm ) const
{
	m_oldValue = stm.GetTyped<uint32>();
	m_probabilitySame = stm.GetTyped<uint8>();
	return true;
}

void CStationaryInteger::NoMemento() const
{
	m_oldValue = Quantize(int32((m_nMin + m_nMax)/2));
	m_probabilitySame = 8;
}

uint32 CStationaryInteger::Quantize( int32 x ) const
{
	if(m_nMax > std::numeric_limits<int32>::max())
	{
		uint32 y = x;
		if (y < m_nMin)
			return 0;
		else if (y >= m_nMax)
			return uint32(m_nMax - m_nMin - 1);
		else
			return (uint32)(y - m_nMin);
	}
	else
	{
		if (x < m_nMin)
			return 0;
		else if (x >= m_nMax)
			return uint32(m_nMax - m_nMin - 1);
		else
			return (uint32)(x - m_nMin);
	}
}

int32 CStationaryInteger::Dequantize( uint32 x ) const
{
	return int32(x + m_nMin);
}

void CStationaryInteger::WriteValue( CCommOutputStream& stm, int32 value ) const
{
	uint32 quantized = Quantize(value);
	if (quantized == m_oldValue)
	{
		stm.EncodeShift( 4, 0, m_probabilitySame );
		m_probabilitySame = m_probabilitySame + (m_probabilitySame<15);
	}
	else
	{
		stm.EncodeShift( 4, m_probabilitySame, 16 - m_probabilitySame );
		stm.WriteInt(quantized, uint32(m_nMax - m_nMin));
		m_probabilitySame = 1;
	}

	m_oldValue = quantized;
}

int32 CStationaryInteger::ReadValue( CCommInputStream& stm ) const
{
	uint16 probSame = stm.DecodeShift(4);
	uint32 quantized;
	if (probSame < m_probabilitySame)
	{
		stm.UpdateShift( 4, 0, m_probabilitySame );
		quantized = m_oldValue;
		m_probabilitySame = m_probabilitySame + (m_probabilitySame<15);
	}
	else
	{
		stm.UpdateShift( 4, m_probabilitySame, 16 - m_probabilitySame );
		quantized = stm.ReadInt(uint32(m_nMax - m_nMin));
		m_probabilitySame = 1;
	}
	m_oldValue = quantized;
	return Dequantize(quantized);
}
