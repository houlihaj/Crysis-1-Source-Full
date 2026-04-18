#ifndef __SERIALIZATIONCHUNK_H__
#define __SERIALIZATIONCHUNK_H__

#pragma once

#include "Config.h"
#include "INetwork.h"

class CNetOutputSerializeImpl;
class CNetInputSerializeImpl;
class CByteInputStream;
class CByteOutputStream;
struct ICompressionPolicy;
typedef _smart_ptr<ICompressionPolicy> ICompressionPolicyPtr;

enum EOps
{
	eO_NoOp = -1,
#define SERIALIZATION_TYPE(T) eO_##T,
#include "SerializationTypes.h"
#undef SERIALIZATION_TYPE
	eO_String,
	eO_OptionalGroup
};

template <class T> struct TypeToId;
#define SERIALIZATION_TYPE(T) template <> struct TypeToId<T> { static const EOps type = eO_##T; };
#include "SerializationTypes.h"
#undef SERIALIZATION_TYPE

typedef uint16 ChunkID;
static const ChunkID InvalidChunkID = ~ChunkID(0);

class CSerializationChunk : public CMultiThreadRefCount
{
public:
	CSerializationChunk();
	~CSerializationChunk();

	bool Init( IGameContext * pCtx, EntityId id, uint8 nAspect, uint8 nProfile );

	bool FetchFromGame( IGameContext * pCtx, CByteOutputStream& output, EntityId id, uint8 nAspect, ChunkID chunkID, uint8 nProfile ) const;
	ESynchObjectResult UpdateGame( IGameContext * pCtx, CByteInputStream& input, EntityId id, uint8 nAspect, ChunkID chunkID, uint8 nProfile, bool& wasPartialUpdate ) const;
	void EncodeToStream( CByteInputStream& input, CNetOutputSerializeImpl& output, ChunkID chunkID, uint8 nProfile ) const;
	void DecodeFromStream( CNetInputSerializeImpl& input, CByteOutputStream& output, ChunkID chunkID, uint8 nProfile ) const;

	bool IsEmpty() const;

	bool operator<( const CSerializationChunk& rhs ) const;

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CSerializationChunk");

		pSizer->Add(*this);
		pSizer->AddContainer(m_ops);
	}

#if ENABLE_DEBUG_KIT
	void Dump( uint32 id );
#endif

private:
	class CToBufferImpl;
	class CFromBufferImpl;
	class CBuildImpl;

	struct SOp
	{
		EOps type;
		int8 skipFalse;
		ICompressionPolicyPtr pPolicy;
#if TRACK_ENCODING
		string name;
#endif
	};
	typedef std::vector<SOp> TOps;
	TOps m_ops;
#if CRC8_ASPECT_FORMAT
	uint8 m_crc;
#endif
};

typedef _smart_ptr<CSerializationChunk> CSerializationChunkPtr;

#endif
