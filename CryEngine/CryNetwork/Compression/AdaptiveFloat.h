#ifndef __ADAPTIVEFLOAT_H__
#define __ADAPTIVEFLOAT_H__

#pragma once

#include "Quantizer.h"
#include "IntegerValuePredictor.h"
#include "Streams/CommStream.h"

class CAdaptiveFloat
{
public:
	CAdaptiveFloat();

	bool Load( XmlNodeRef node, const string& filename, const string& child );

	void ReadMemento( CByteInputStream& stm ) const;
	void WriteMemento( CByteOutputStream& stm ) const;
	void NoMemento() const;
	bool ReadValue( CCommInputStream& stm, float& value, uint32 mementoAge ) const;
	void WriteValue( CCommOutputStream& stm, float value, uint32 mementoAge ) const;

private:
	CQuantizer m_quantizer;

	uint32		m_nHeight;
	uint32		m_nQuantizedMinDifference;
	uint32		m_nQuantizedMaxDifference;
	uint32		m_nQuantizedStartValue;
	uint8			m_nInRangePercentage;

	mutable CIntegerValuePredictor m_predictor;
};

#endif
