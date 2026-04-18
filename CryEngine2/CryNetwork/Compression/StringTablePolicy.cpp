#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"

class CStringTablePolicy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return true;
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

	bool ReadValue( CCommInputStream& in, SSerializeString& value, CArithModel * pModel, uint32 age ) const
	{
		string s;
		pModel->TableReadString( in, s );
		value = s.c_str();
		return true;
	}
	bool WriteValue( CCommOutputStream& out, const SSerializeString& value, CArithModel * pModel, uint32 age ) const
	{
		pModel->TableWriteString( out, string(value.c_str()) );
		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("StringTable does not support arbitrary types (only strings)");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("StringTable does not support arbitrary types (only strings)");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CStringTablePolicy");
		pSizer->Add(*this);
	}
};

REGISTER_COMPRESSION_POLICY(CStringTablePolicy, "StringTable");
