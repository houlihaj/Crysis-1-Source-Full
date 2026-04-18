#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"

class CDefaultPolicy
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

	bool ReadValue( CCommInputStream& in, bool& value, CArithModel * pModel, uint32 age ) const
	{
		value = in.ReadBits(1) != 0;
		return true;
	}
	bool WriteValue( CCommOutputStream& out, bool value, CArithModel * pModel, uint32 age ) const
	{
		out.WriteBits(value, 1);
		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		if (!ReadBytes( in, &value, sizeof(value) ))
			return false;
		SwapEndian(value);
		return true;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		SwapEndian(value);
		return WriteBytes( out, &value, sizeof(value) );
	}

	bool ReadValue( CCommInputStream& in, CTimeValue& value, CArithModel * pModel, uint32 age ) const
	{
		value = pModel->ReadTime( in, eTS_Network );
		return true;
	}
	bool WriteValue( CCommOutputStream& out, CTimeValue value, CArithModel * pModel, uint32 age ) const
	{
		pModel->WriteTime( out, eTS_Network, value );
		return true;
	}

	bool ReadValue( CCommInputStream& in, ScriptAnyValue& value, CArithModel * pModel, uint32 age ) const
	{
		NET_ASSERT(!"script values not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}
	bool WriteValue( CCommOutputStream& out, const ScriptAnyValue& value, CArithModel * pModel, uint32 age ) const
	{
		NET_ASSERT(!"script values not supported");
		NetWarning("Network serialization of script types is not supported");
		return false;
	}

	bool ReadValue( CCommInputStream& in, SSerializeString& value, CArithModel * pModel, uint32 age ) const
	{
		string s;
		bool bRes = pModel->ReadString( in, s );
		if (bRes)
			value = s.c_str();
		return bRes;
	}
	bool WriteValue( CCommOutputStream& out, const SSerializeString& value, CArithModel * pModel, uint32 age ) const
	{
		pModel->WriteString( out, string(value.c_str()) );
		return true;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CDefaultPolicy");
		pSizer->Add(*this);
	}

private:
	bool ReadBytes( CCommInputStream& in, void * pValue, size_t nBytes ) const
	{
		uint8 * pAry = (uint8*) pValue;
		for (int i=0; i<nBytes; i++)
			pAry[i] = in.ReadBits(8);
		return true;
	}
	bool WriteBytes( CCommOutputStream& out, const void * pValue, size_t nBytes ) const
	{
		const uint8 * pAry = (const uint8*) pValue;
		for (int i=0; i<nBytes; i++)
			out.WriteBits(pAry[i], 8);
		return true;
	}
};

REGISTER_COMPRESSION_POLICY(CDefaultPolicy, "Default");

#include "TypeInfo_impl.h"
#include "Cry_Vector2_info.h"
#include "Cry_Vector3_info.h"
#include "Cry_Quat_info.h"
#include "ISerialize_info.h"
