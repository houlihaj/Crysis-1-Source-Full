#include "StdAfx.h"
#include "BoolCompress.h"

static const uint8 LAST_VALUE_BIT = 0x80;

void CBoolCompress::ReadMemento(CByteInputStream& stm) const
{
	uint8 val = stm.GetTyped<uint8>();
	m_lastValue = (val & LAST_VALUE_BIT) != 0;
	m_prob = val & StateRange;
	NET_ASSERT(m_prob);
}

void CBoolCompress::WriteMemento( CByteOutputStream& stm ) const
{
	stm.PutTyped<uint8>() = m_prob | (m_lastValue * LAST_VALUE_BIT);
}

void CBoolCompress::NoMemento() const
{
	m_lastValue = false;
	m_prob = StateMidpoint;
}

bool CBoolCompress::ReadValue(CCommInputStream& stm) const
{
	uint8 curProb = m_prob;
	bool lastValue = m_lastValue;
	NET_ASSERT((*(uint8*)&lastValue) < 2);
	bool isLastValue = stm.DecodeShift( AdaptBits ) < curProb;
	uint16 sym, low;
	bool value;
	if (isLastValue)
	{
		sym = curProb;
		low = 0;
		m_prob = curProb + (curProb!=StateRange);
		value = lastValue;
	}
	else
	{
		sym = StateRange+1-curProb;
		low = curProb;
		m_prob = curProb - (curProb>1);
		value = !lastValue;
	}
	stm.UpdateShift( AdaptBits, low, sym );
	m_lastValue = value;
	return value;
}

void CBoolCompress::WriteValue(CCommOutputStream& stm, bool value) const
{
	uint8 curProb = m_prob;
	bool lastValue = m_lastValue;
	NET_ASSERT((*(uint8*)&lastValue) < 2);
	uint16 sym, low;
	if (value == lastValue)
	{
		sym = curProb;
		low = 0;
		m_prob = curProb + (curProb!=StateRange);
	}
	else
	{
		sym = StateRange+1-curProb;
		low = curProb;
		m_prob = curProb - (curProb>1);
	}
	stm.EncodeShift( AdaptBits, low, sym );
	m_lastValue = value;
}
