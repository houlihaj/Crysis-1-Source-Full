#ifndef __ICOMPRESSIONPOLICY_H__
#define __ICOMPRESSIONPOLICY_H__

#pragma once

#include "CompressionManager.h"
#include "Streams/ByteStream.h"
#include "Streams/CommStream.h"

class CArithModel;

struct ICompressionPolicy : public CMultiThreadRefCount
{
	const uint32 key;

	ICompressionPolicy(uint32 k) : key(k) 
	{
		++g_objcnt.compressionPolicy;
	}

	virtual ~ICompressionPolicy() 
	{
		--g_objcnt.compressionPolicy;
	}

	virtual bool Load( XmlNodeRef node, const string& filename ) = 0;

#define SERIALIZATION_TYPE(T) \
	virtual bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState ) const = 0; \
	virtual bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState ) const = 0;
#include "SerializationTypes.h"
#undef SERIALIZATION_TYPE
	virtual bool ReadValue( CCommInputStream& in, SSerializeString& value, CArithModel * pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState ) const = 0;
	virtual bool WriteValue( CCommOutputStream& out, const SSerializeString& value, CArithModel * pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState ) const = 0;

	virtual void GetMemoryStatistics(ICrySizer* pSizer) const = 0;
};

typedef _smart_ptr<ICompressionPolicy> ICompressionPolicyPtr;

template <class T>
class CCompressionPolicy : public ICompressionPolicy
{
public:
	CCompressionPolicy(uint32 key) : ICompressionPolicy(key) {}
	CCompressionPolicy(uint32 key, const T& impl) : ICompressionPolicy(key), m_impl(impl) {}

	virtual bool Load( XmlNodeRef node, const string& filename )
	{
		return m_impl.Load(node, filename);
	}
#define SERIALIZATION_TYPE(T) \
	virtual bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState ) const \
	{ \
		Setup(pCurState); \
		if (!m_impl.ReadValue( in, value, pModel, age )) \
			return false; \
		Complete(pNewState); \
		return true; \
	} \
	virtual bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState ) const \
	{ \
		Setup(pCurState); \
		if (!m_impl.WriteValue( out, value, pModel, age )) \
			return false; \
		Complete(pNewState); \
		return true; \
	}
#include "SerializationTypes.h"
#undef SERIALIZATION_TYPE

	virtual bool ReadValue( CCommInputStream& in, SSerializeString& value, CArithModel * pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState ) const
	{
		Setup(pCurState);
		if (!m_impl.ReadValue( in, value, pModel, age ))
			return false;
		Complete(pNewState);
		return true;
	}
	virtual bool WriteValue( CCommOutputStream& out, const SSerializeString& value, CArithModel * pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState ) const
	{
		Setup(pCurState);
		if (!m_impl.WriteValue( out, value, pModel, age ))
			return false;
		Complete(pNewState);
		return true;
	}

	virtual void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		m_impl.GetMemoryStatistics(pSizer);
	}

private:
	void Setup( CByteInputStream * pCurState ) const
	{
		if (pCurState)
		{
#if VERIFY_MEMENTO_BUFFERS
			uint32 tag = pCurState->GetTyped<uint32>();
			NET_ASSERT(tag == 0x12345678);
#endif
			m_impl.ReadMemento( *pCurState );
#if VERIFY_MEMENTO_BUFFERS
			tag = pCurState->GetTyped<uint32>();
			NET_ASSERT(tag == 0x87654321);
#endif
		}
		else
			m_impl.NoMemento();
	}
	void Complete( CByteOutputStream * pNewState ) const
	{
		if (pNewState)
		{
#if VERIFY_MEMENTO_BUFFERS
			pNewState->PutTyped<uint32>() = 0x12345678;
#endif
			m_impl.WriteMemento( *pNewState );
#if VERIFY_MEMENTO_BUFFERS
			pNewState->PutTyped<uint32>() = 0x87654321;
#endif
		}
	}

	T m_impl;
};

struct CompressionPolicyFactoryBase {};

template <class T>
struct CompressionPolicyFactory : public CompressionPolicyFactoryBase
{
	CompressionPolicyFactory(string name)
	{
		CCompressionRegistry::Get()->RegisterPolicy(name, Create);
	}

	static ICompressionPolicyPtr Create(uint32 key)
	{
		return new CCompressionPolicy<T>(key);
	}
};

#define REGISTER_COMPRESSION_POLICY(cls, name) \
	static CompressionPolicyFactory<cls> cls##_Factory(name); \
  CompressionPolicyFactoryBase * cls##_FactoryPtr = &cls##_Factory;

#endif
