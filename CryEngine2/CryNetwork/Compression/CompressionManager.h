#ifndef __COMPRESSIONMANAGER_H__
#define __COMPRESSIONMANAGER_H__

#pragma once

#include "VectorMap.h"
#include "Streams/ByteStream.h"
#include "INetwork.h"
#include "SerializationChunk.h"
#include "STLPoolAllocator_ManyElems.h"

struct ICompressionPolicy;
typedef _smart_ptr<ICompressionPolicy> ICompressionPolicyPtr;
class CNetOutputSerializeImpl;
class CNetInputSerializeImpl;

class CCompressionRegistry
{
public:
	typedef ICompressionPolicyPtr(*CompressionPolicyCreator)(uint32);

	static ILINE CCompressionRegistry * Get()
	{
		if (!m_pSelf)
			Create();
		return m_pSelf;
	}

	void RegisterPolicy(const string& name, CompressionPolicyCreator);
	ICompressionPolicyPtr CreatePolicyOfType( const string& type, uint32 key );

private:
	CCompressionRegistry() {}

	static void Create();

	static CCompressionRegistry * m_pSelf;
	VectorMap<string, CompressionPolicyCreator> m_policyFactories;
};

class CCompressionManager
{
public:
	CCompressionManager();
	~CCompressionManager();

	void Reset( bool useCompression );

	ICompressionPolicyPtr GetCompressionPolicy( uint32 key );

	// chunk management
	void BufferToStream( ChunkID chunk, uint8 profile, CByteInputStream& in, CNetOutputSerializeImpl& out );
	void StreamToBuffer( ChunkID chunk, uint8 profile, CNetInputSerializeImpl& in, CByteOutputStream& out );

	ChunkID GameContextGetChunkID( IGameContext * pCtx, EntityId id, uint8 nAspect, uint8 nProfile );

	ILINE bool GameContextRead( IGameContext * pCtx, CByteOutputStream* pOutput, EntityId id, uint8 nAspect, uint8 nProfile, ChunkID chunkID )
	{
		ASSERT_PRIMARY_THREAD;
		CSerializationChunk* pChunk = GetChunk(chunkID);
		if (!pChunk)
			return false;
		return pChunk->FetchFromGame( pCtx, *pOutput, id, nAspect, chunkID, nProfile );
	}

	ILINE ESynchObjectResult GameContextWrite( IGameContext * pCtx, CByteInputStream* pInput, EntityId id, uint8 nAspect, uint8 nProfile, ChunkID chunkID, bool& wasPartialUpdate )
	{
		ASSERT_PRIMARY_THREAD;
		CSerializationChunkPtr pChunk = GetChunk(chunkID);
		if (!pChunk)
		{
			NetWarning("CCompressionManager::GameContextWrite: failed fetching chunk %d", chunkID);
			return eSOR_Failed;
		}
		return pChunk->UpdateGame( pCtx, *pInput, id, nAspect, chunkID, nProfile, wasPartialUpdate );
	}

	bool IsChunkEmpty( ChunkID chunkID );

	void GetMemoryStatistics(ICrySizer* pSizer);

private:
	class HashTraits : public stl::hash_uint32
	{
	public:
		enum {	// parameters for hash table
			bucket_size = 1,	// 0 < bucket_size
			min_buckets = 128	};// min_buckets = 2 ^^ N, 0 < N
	};

#if USE_SYSTEM_ALLOCATOR
	typedef stl::hash_map<uint32, ICompressionPolicyPtr, HashTraits> TCompressionPoliciesMap;
#else
	typedef stl::hash_map<uint32, ICompressionPolicyPtr, HashTraits, stl::STLPoolAllocator_ManyElems<std::pair<uint32, ICompressionPolicyPtr> > > TCompressionPoliciesMap;
#endif
	TCompressionPoliciesMap m_compressionPolicies;
	ICompressionPolicyPtr m_pDefaultPolicy;

	class CCompareChunks
	{
	public:
		bool operator()( CSerializationChunk*, CSerializationChunk* ) const;
	};

	std::vector<CSerializationChunkPtr> m_chunks;
	std::map<CSerializationChunkPtr, ChunkID, CCompareChunks> m_chunkToId;
	ILINE CSerializationChunk* GetChunk(ChunkID chunk)
	{
		if (chunk >= m_chunks.size())
			return 0;
		else 
			return m_chunks[chunk];
	}

	ICompressionPolicyPtr GetCompressionPolicyFallback( uint32 key );
	ICompressionPolicyPtr CreateRangedInt( int nMax, uint32 key );
	ICompressionPolicyPtr CreatePolicy( XmlNodeRef node, const string& filename, uint32 key );
	void ClearCompressionPolicies(bool final);
};

#endif
