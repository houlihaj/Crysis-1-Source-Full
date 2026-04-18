/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  declaration of CryNetwork ISerialize classes
-------------------------------------------------------------------------
History:
- 26/07/2004   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __NETWORK_SERIALIZE_H__
#define __NETWORK_SERIALIZE_H__

#pragma once

#include "Streams/CommStream.h"
#include "Config.h"
#include "IConsole.h"

#include "SimpleSerialize.h"
#include "Network.h"
#include "Compression/ICompressionPolicy.h"
#include "Compression/CompressionManager.h"
#include "Compression/ArithModel.h"
#include "DebugKit/DebugKit.h"

class CArithModel;

enum ESerializeChunkResult
{
	eSCR_Ok_Updated,
	eSCR_Ok,
	eSCR_Failed,
};

class CNetSerialize
{
public:
	CNetSerialize() : m_pArithModel(NULL), m_pCurState(0), m_pNewState(0), m_mementoAge(0), m_isOwner(false) {}

	void SetMementoStreams( 
		CByteInputStream* pCurState, 
		CByteOutputStream* pNewState,
		uint32 mementoAge,
		bool isOwner )
	{
		NET_ASSERT(mementoAge < 20000000);
		m_pCurState = pCurState;
		m_pNewState = pNewState;
		m_mementoAge = mementoAge;
		m_isOwner = isOwner;
	}

	ILINE CArithModel * GetArithModel()
	{
		return m_pArithModel;
	}

#if ENABLE_DEBUG_KIT
	static bool m_bEnableLogging;
#endif

	virtual ESerializeChunkResult SerializeChunk( ChunkID chunk, uint8 profile, TMemHdl * phData, CTimeValue * pTimeInfo, CMementoMemoryManager& mmm ) = 0;

protected:
	CByteInputStream * GetCurrentState() { return m_pCurState; }
	CByteOutputStream * GetNewState() { return m_pNewState; }

	ILINE void InvalidateCurrentState() { m_pCurState = NULL; }
	ILINE void SetArithModel( CArithModel * pModel )
	{
		m_pArithModel = pModel;
	}

	ILINE uint32 GetMementoAge() const
	{
		return m_mementoAge;
	}

	ILINE bool IsOwner() const
	{
		return m_isOwner;
	}

	virtual void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CNetSerialize");

		pSizer->Add(*this);
		m_pArithModel->GetMemoryStatistics(pSizer, true);
		m_pCurState->GetMemoryStatistics(pSizer);
		m_pNewState->GetMemoryStatistics(pSizer);
	}

private:
	CArithModel * m_pArithModel;
	CByteInputStream * m_pCurState;
	CByteOutputStream * m_pNewState;
	uint32 m_mementoAge;
	bool m_isOwner;
};

class CNetOutputSerializeImpl : 
	public CSimpleSerializeImpl<false, eST_Network>,
	public CNetSerialize
{
public:
	CNetOutputSerializeImpl( uint8 * pBuffer, size_t nSize, uint8 nBonus=0 );
	CNetOutputSerializeImpl( IStreamAllocator * pAllocator, size_t initialSize, uint8 bonus=0 );

	template <class T_Value>
	ILINE void Value( const char *name, T_Value& value, uint32 policy )
	{
		ValueImpl( name, value, CNetwork::Get()->GetCompressionManager().GetCompressionPolicy(policy) );
	}

	void ResetLogging();

	void SetArithModel( CArithModel * pModel )
	{
		CNetSerialize::SetArithModel(pModel);
	}

	bool BeginOptionalGroup( const char * name, bool condition )
	{
		this->Value(name, condition, 'bool');
		uint8 prev = condition;
		if (GetCurrentState())
			prev = GetCurrentState()->GetTyped<uint8>();
		if (prev != uint8(condition))
			InvalidateCurrentState(); // trash mementos, as 'times, they are a changin''
		if (GetNewState())
			GetNewState()->PutTyped<uint8>() = condition;
		return condition;
	}

	CCommOutputStream& GetOutput() { return m_output; }

	virtual ESerializeChunkResult SerializeChunk( ChunkID chunk, uint8 profile, TMemHdl * phData, CTimeValue * pTimeInfo, CMementoMemoryManager& mmm );

	template <class T_Value>
	ILINE void ValueImpl( const char * name, T_Value& value, ICompressionPolicy * pPolicy )
	{
		if (!Ok())
			return;
		ConditionalPrelude( name );
		DEBUGKIT_SET_VALUE(name);
		DEBUGKIT_SET_KEY(pPolicy->key);
		DEBUGKIT_ADD_DATA_ENT(value);
		//DEBUGKIT_ADD_DATA_ENTITY(name, pPolicy->key, value);
		if (!pPolicy->WriteValue( m_output, value, GetArithModel(), GetMementoAge(), IsOwner(), GetCurrentState(), GetNewState()))
		{
			NetWarning("Failed to compress %s", name);
			Failed();
			return;
		}
		ConditionalPostlude( name );
	}

	virtual void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CNetOutputSerializeImpl");

		pSizer->Add(*this);
		m_output.GetMemoryStatistics(pSizer);
	}

private:
	CCommOutputStream m_output;

	ILINE void ConditionalPrelude( const char * name )
	{
		DEBUGKIT_ANNOTATION( string("begin: ") + name );
#if STATS_COLLECTOR
# if ENABLE_ACCURATE_BANDWIDTH_PROFILING
		m_sizeAtPrelude = m_output.GetApproximateSize();
# else
		m_sizeAtPrelude = m_output.GetBitSize();
# endif
#endif
#if CHECK_ENCODING
		GetArithModel()->WriteString( m_output, name );
#endif
#if MINI_CHECK_ENCODING
		m_output.WriteBits(0,1);
#endif
	}

	ILINE void ConditionalPostlude( const char * name )
	{
#if STATS_COLLECTOR
# if ENABLE_ACCURATE_BANDWIDTH_PROFILING
		STATS.AddData( name, m_output.GetBitSize() - m_sizeAtPrelude );
# else
		STATS.AddData( name, m_output.GetApproximateSize() - m_sizeAtPrelude );
# endif
#endif
#if CHECK_ENCODING
		GetArithModel()->WriteString( m_output, name );
#endif
#if MINI_CHECK_ENCODING
		m_output.WriteBits(0,1);
#endif
		DEBUGKIT_ANNOTATION( string("end: ") + name );
	}

#if STATS_COLLECTOR
	uint32 m_sizeAtPrelude;
#endif
};

class CNetInputSerializeImpl : 
	public CSimpleSerializeImpl<true, eST_Network>,
	public CNetSerialize
{
public:
	CNetInputSerializeImpl( const uint8 * pBuffer, size_t nSize, INetChannel * pChannel );

	void Failed() { CSimpleSerializeImpl<true, eST_Network>::Failed(); }

	CCommInputStream& GetInput() { return m_input; }

	template <class T_Value>
	void Value( const char * name, T_Value& value, uint32 policy )
	{
		ValueImpl( name, value, CNetwork::Get()->GetCompressionManager().GetCompressionPolicy(policy) );
	}

	template <class T_Value>
	void Value( const char * szName, T_Value& value )
	{
		Value( szName, value, 0 );
	}

	void SetArithModel( CArithModel * pModel )
	{
		CNetSerialize::SetArithModel(pModel);
	}

	bool BeginOptionalGroup( const char * name, bool condition )
	{
		Value(name, condition, 'bool');
		uint8 prev = condition;
		if (GetCurrentState())
			prev = GetCurrentState()->GetTyped<uint8>();
		if (prev != uint8(condition))
			InvalidateCurrentState(); // trash mementos, as 'times, they are a changin''
		if (GetNewState())
			GetNewState()->PutTyped<uint8>() = condition;
		return condition;
	}

	virtual ESerializeChunkResult SerializeChunk( ChunkID chunk, uint8 profile, TMemHdl * phData, CTimeValue * pTimeInfo, CMementoMemoryManager& mmm );

	template <class T_Value>
	ILINE void ValueImpl( const char * name, T_Value& value, ICompressionPolicy * pPolicy )
	{
		if (!Ok())
			return;
		ConditionalPrelude( name );
		if (!Ok())
			return;
		bool ok;
		if (m_bCommit)
			ok = pPolicy->ReadValue( m_input, value, GetArithModel(), GetMementoAge(), IsOwner(), GetCurrentState(), GetNewState() );
		else
		{
			T_Value temp;
			ok = pPolicy->ReadValue( m_input, temp, GetArithModel(), GetMementoAge(), IsOwner(), GetCurrentState(), GetNewState() );
		}
		if (!ok)
		{
			NetWarning("Failed to decompress %s", name);
			Failed();
			return;
		}
		ConditionalPostlude( name );
	}

	virtual void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CNetInputSerializeImpl");

		pSizer->Add(*this);
		m_input.GetMemoryStatistics(pSizer);
	}

private:
	void SetInvalid() { m_bCommit = false; }

	ILINE void ConditionalPrelude( const char * name )
	{
		DEBUGKIT_ANNOTATION( string("begin: ") + name );
#if CHECK_ENCODING
		string temp;
		GetArithModel()->ReadString( m_input, temp );
		if (temp != name)
		{
			NetWarning( "Data mismatch: expected %s and got %s", name, temp.c_str() );
			m_pChannel->Disconnect( eDC_ProtocolError, name );
			SetInvalid();
		}
#endif
#if MINI_CHECK_ENCODING
		if (m_input.ReadBits(1))
		{
			NetWarning( "Data mismatch on opening %s", name );
			//m_pChannel->Disconnect( eDC_ProtocolError, name );
			//SetInvalid();
			Failed();
		}
#endif
	}

	ILINE void ConditionalPostlude( const char * name )
	{
#if CHECK_ENCODING
		string temp;
		GetArithModel()->ReadString( m_input, temp );
		if (temp != name)
		{
			NetWarning( "Data mismatch: expected closing %s and got %s", name, temp.c_str() );
			m_pChannel->Disconnect( eDC_ProtocolError, name );
			SetInvalid();
		}
#endif
#if MINI_CHECK_ENCODING
		if (m_input.ReadBits(1))
		{
			NetWarning( "Data mismatch on closing %s", name );
			//m_pChannel->Disconnect( eDC_ProtocolError, name );
			//SetInvalid();
			Failed();
		}
#endif
		DEBUGKIT_ANNOTATION( string("end: ") + name );
	}

	INetChannel * m_pChannel;
	CCommInputStream m_input;
};

// TODO: dirty: find better way
template <class T>
ILINE T * GetNetSerializeImplFromSerialize( TSerialize ser )
{
	ISerialize * pISerialize = GetImpl(ser);
	CSimpleSerialize<T> * pSimpleSerialize = static_cast<CSimpleSerialize<T>*>(pISerialize);
	return pSimpleSerialize->GetInnerImpl();
}

// TODO: extremely dirty... makes the above look clean: find better way
ILINE CNetSerialize * GetNetSerializeImpl( TSerialize ser )
{
	ISerialize * pISerialize = GetImpl(ser);
	if (pISerialize->IsReading())
		return static_cast<CNetSerialize*>(GetNetSerializeImplFromSerialize<CNetInputSerializeImpl>(ser));
	else
		return static_cast<CNetSerialize*>(GetNetSerializeImplFromSerialize<CNetOutputSerializeImpl>(ser));
}

#endif
