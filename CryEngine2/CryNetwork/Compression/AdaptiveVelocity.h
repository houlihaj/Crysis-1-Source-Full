#ifndef __ADAPTIVEVELOCITY_H__
#define __ADAPTIVEVELOCITY_H__

#pragma once

#include "Quantizer.h"
#include "BoolCompress.h"

class CAdaptiveVelocity
{
public:
	bool Load( XmlNodeRef node, const string& filename, const string& child );
	void ReadMemento( CByteInputStream& stm ) const;
	void WriteMemento( CByteOutputStream& stm ) const;
	void NoMemento() const;
	bool ReadValue( CCommInputStream& stm, float& value ) const;
	void WriteValue( CCommOutputStream& stm, float value) const;

private:
	CQuantizer m_quantizer;
	uint32		m_nHeight;
	CBoolCompress m_boolCompress;

	mutable uint32 m_nLastQuantized;
};

#endif
