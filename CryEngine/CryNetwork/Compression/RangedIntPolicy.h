#ifndef __RANGEDINTPOLICY_H__
#define __RANGEDINTPOLICY_H__

#pragma once

#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "StationaryInteger.h"

class CRangedIntPolicy
{
public:
	void SetValues( int32 nMin, int32 nMax )
	{
		m_integer.SetValues(nMin, nMax);
	}

	bool Load( XmlNodeRef node, const string& filename )
	{
		return m_integer.Load(node, filename, "Range");
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		m_integer.ReadMemento(in);
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		m_integer.WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		m_integer.NoMemento();
	}

	bool ReadValue( CCommInputStream& in, int32& value, CArithModel * pModel, uint32 age ) const
	{
		value = m_integer.ReadValue(in);
		return true;
	}
	bool WriteValue( CCommOutputStream& out, int32 value, CArithModel * pModel, uint32 age ) const
	{
		m_integer.WriteValue(out, value);
		return true;
	}

#define DECL_PROXY(T) \
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const \
	{ \
		int32 temp; \
		bool ok = ReadValue( in, temp, pModel, age ); \
		if (ok) \
		{ \
			value = temp; \
			NET_ASSERT(value == temp); \
		} \
		return ok; \
	} \
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const \
	{ \
		int32 temp = value; \
		NET_ASSERT(temp == value); \
		return WriteValue(out, temp, pModel, age); \
	}

DECL_PROXY(int8)
DECL_PROXY(int16)
DECL_PROXY(int64)
DECL_PROXY(uint8)
DECL_PROXY(uint16)
DECL_PROXY(uint32)
DECL_PROXY(uint64)

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("RangedIntPolicy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("RangedIntPolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "");
		pSizer->Add(*this);
	}

private:
	CStationaryInteger m_integer;
};

#endif
