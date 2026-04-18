#include "StdAfx.h"
#include "SerializationChunk.h"
#include "Streams/ByteStream.h"
#include "Protocol/Serialize.h"

/*
 * helper routines
 */

#if TRACK_ENCODING
# define NAME_FROM_OP(pOp) (pOp)->name.c_str()
#else
# define NAME_FROM_OP(pOp) ""
#endif

namespace
{

	template <class T>
	ILINE void CopyToBuffer( const T& value, CByteOutputStream& s )
	{
		s.PutTyped<T>() = value;
	}

	ILINE void CopyToBuffer( const SSerializeString& value, CByteOutputStream& s )
	{
		uint32 len = value.size();
		s.PutTyped<uint32>() = len;
		s.Put(value.c_str(), value.length());
	}

	template <class T>
	ILINE void CopyFromBuffer( T& value, CByteInputStream& s )
	{
		value = s.GetTyped<T>();
	}

	ILINE void CopyFromBuffer( SSerializeString& value, CByteInputStream& s )
	{
		uint32 len = s.GetTyped<uint32>();
		const char * pBuffer = (const char *) s.Get(len);
		value = SSerializeString( pBuffer, pBuffer+len );
	}

	template <class T>
	struct GenImpl
	{
		static void DecodeFromStream( const char * name, CNetInputSerializeImpl& input, CByteOutputStream& output, ICompressionPolicyPtr pPolicy )
		{
			T temp;
			input.ValueImpl( name, temp, pPolicy );
			CopyToBuffer( temp, output );
		}
		static void EncodeToStream( const char * name, CByteInputStream& input, CNetOutputSerializeImpl& output, ICompressionPolicyPtr pPolicy )
		{
			T temp;
			CopyFromBuffer( temp, input );
			output.ValueImpl( name, temp, pPolicy );
		}
	};

	typedef void (*DecodeFromStreamFunction)( const char * name, CNetInputSerializeImpl& input, CByteOutputStream& output, ICompressionPolicyPtr pPolicy );
	typedef void (*EncodeToStreamFunction)( const char * name, CByteInputStream& input, CNetOutputSerializeImpl& output, ICompressionPolicyPtr pPolicy );

	DecodeFromStreamFunction DecodeFromStreamImpl[] = {
#define SERIALIZATION_TYPE(T) GenImpl<T>::DecodeFromStream,
#include "SerializationTypes.h"
#undef SERIALIZATION_TYPE
	};
	EncodeToStreamFunction EncodeToStreamImpl[] = {
#define SERIALIZATION_TYPE(T) GenImpl<T>::EncodeToStream,
#include "SerializationTypes.h"
#undef SERIALIZATION_TYPE
	};

#if ENABLE_DEBUG_KIT
	const char * EntryNames[] = {
#define SERIALIZATION_TYPE(T) #T,
#include "SerializationTypes.h"
#undef SERIALIZATION_TYPE
	};
#endif

}

/*
 * CToBufferImpl
 */ 

class CSerializationChunk::CToBufferImpl : public CSimpleSerializeImpl<false, eST_Network>
{
public:
	CToBufferImpl( CByteOutputStream& out, const CSerializationChunk * pChunk ) : m_out(out), m_op(0), m_pChunk(pChunk) 
	{
		NET_ASSERT(m_pChunk);
	}
	~CToBufferImpl()
	{
	}
	bool Complete()
	{ 
		if (!m_pChunk)
			return false;
		if (m_op != m_pChunk->m_ops.size())
			return false;
		return true;
	}

	template <class T>
	void Value( const char * name, T& value, uint32 policy )
	{
		NET_ASSERT(m_op < m_pChunk->m_ops.size());
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT( 0 == strcmp( name, op.name.c_str() ) );
#endif
		NET_ASSERT( TypeToId<T>::type == op.type );
		NET_ASSERT( CNetwork::Get()->GetCompressionManager().GetCompressionPolicy(policy) == op.pPolicy );
		CopyToBuffer( value, m_out );
		m_op ++;
	}

	template <class T>
	void Value( const char * name, T& value )
	{
		Value(name, value, 0);
	}

	void Value(	const char * name, SSerializeString& value, uint32 policy )
	{
		NET_ASSERT(m_op < m_pChunk->m_ops.size());
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT( 0 == strcmp( name, op.name.c_str() ) );
#endif
		NET_ASSERT( eO_String == op.type );
		CopyToBuffer( value, m_out );
		m_op ++;
	}

	bool BeginGroup( const char * szName )
	{
		return true;
	}

	bool BeginOptionalGroup( const char * szName, bool cond )
	{
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT( 0 == strcmp( szName, op.name.c_str() ) );
#endif
		if (op.skipFalse)
		{
			m_out.PutByte(cond);
			if (!cond)
				m_op += op.skipFalse;
			m_op ++;
			return cond;
		}
		else
		{
			return false;
		}
	}

	void EndGroup()
	{
	}

private:
	CByteOutputStream& m_out;
	size_t m_op;
	const CSerializationChunk * m_pChunk;
};

/*
 * CFromBufferImpl
 */

class CSerializationChunk::CFromBufferImpl : public CSimpleSerializeImpl<true, eST_Network>
{
public:
	CFromBufferImpl( CByteInputStream& in, const CSerializationChunk * pChunk ) : m_in(in), m_op(0), m_pChunk(pChunk), m_partial(false) {}
	~CFromBufferImpl()
	{
	}

	bool Complete( bool mustHaveFinished )
	{ 
		if (mustHaveFinished)
		{
			if (!m_pChunk)
				return false;
			if (m_op != m_pChunk->m_ops.size())
				return false;
		}
		return true;
	}

	void FlagPartialRead()
	{
		m_partial = true;
	}

	bool WasPartial()
	{
		return m_partial;
	}

	template <class T>
	void Value( const char * name, T& value, uint32 policy )
	{
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT( 0 == strcmp( name, op.name.c_str() ) );
#endif
		NET_ASSERT( TypeToId<T>::type == op.type );
		NET_ASSERT( CNetwork::Get()->GetCompressionManager().GetCompressionPolicy(policy) == op.pPolicy );
		CopyFromBuffer( value, m_in );
		m_op ++;
	}

	template <class T>
	void Value( const char * name, T& value )
	{
		Value(name, value, 0);
	}

	void Value( const char *name, SSerializeString& value, uint32 policy )
	{
		SOp op = m_pChunk->m_ops[m_op];
#if TRACK_ENCODING
		NET_ASSERT( 0 == strcmp( name, op.name.c_str() ) );
#endif
		NET_ASSERT( eO_String == op.type );
		CopyFromBuffer( value, m_in );
		m_op ++;
	}

	bool BeginGroup( const char * szName )
	{
		return true;
	}

	bool BeginOptionalGroup( const char * szName, bool cond )
	{
		if (m_pChunk->m_ops[m_op].skipFalse)
		{
			cond = m_in.GetByte() != 0;
			if (!cond)
				m_op += m_pChunk->m_ops[m_op].skipFalse;
			m_op ++;
			return cond;
		}
		else
		{
			return false;
		}
	}

	void EndGroup()
	{
	}

private:
	CByteInputStream& m_in;
	size_t m_op;
	const CSerializationChunk * m_pChunk;
	bool m_partial;
};

/*
 * CBuildImpl
 */

class CSerializationChunk::CBuildImpl : public CSimpleSerializeImpl<false, eST_Network>
{
public:
	CBuildImpl( CSerializationChunk * pChunk ) : m_pChunk(pChunk) {}

	template <class T>
	void Value( const char * name, T& value, uint32 policy )
	{
		AddValue( name, TypeToId<T>::type, policy );
	}

	void Value( const char * name, SSerializeString& value, uint32 policy )
	{
		AddValue( name, eO_String, 0 );
	}

	template <class T>
	void Value( const char * name, T& value )
	{
		Value(name, value, 0);
	}

	bool BeginGroup( const char * szName )
	{
		m_inOptionalGroup.push_back( 0 );
		return true;
	}

	bool BeginOptionalGroup( const char * szName, bool cond )
	{
		m_inOptionalGroup.push_back( 1 );
		m_resolveConditions.push_back( m_pChunk->m_ops.size() );
		AddValue( szName, eO_OptionalGroup, NULL );
		return true;
	}

	void EndGroup()
	{
		NET_ASSERT( !m_inOptionalGroup.empty() );
		if (m_inOptionalGroup.back())
		{
			NET_ASSERT( !m_resolveConditions.empty() );
			m_pChunk->m_ops[ m_resolveConditions.back() ].skipFalse = m_pChunk->m_ops.size() - m_resolveConditions.back();
			m_pChunk->m_ops[ m_resolveConditions.back() ].skipFalse --; // we always increment after skipping
			m_resolveConditions.pop_back();
		}
		m_inOptionalGroup.pop_back();
	}

	bool Complete()
	{
#if CRC8_ASPECT_FORMAT
		m_pChunk->m_crc = m_crc.Result();
#endif
		return true;
	}

private:
	void AddValue( const char * name, EOps type, uint32 policy )
	{
		SOp op;
		op.type = type;
		op.skipFalse = 127;
		op.pPolicy = CNetwork::Get()->GetCompressionManager().GetCompressionPolicy( policy );
#if TRACK_ENCODING
		op.name = name;
#endif
#if CRC8_ASPECT_FORMAT
		m_crc.Add(type);
		m_crc.Add32(policy);
#endif
		m_pChunk->m_ops.push_back(op);
	}

	CSerializationChunk * m_pChunk;
	std::vector<uint8> m_resolveConditions;
	std::vector<uint8> m_inOptionalGroup;
#if CRC8_ASPECT_FORMAT
	CCRC8 m_crc;
#endif
};

/*
 * CSerializationChunk main
 */

CSerializationChunk::CSerializationChunk()
{
	++g_objcnt.serializationChunk;
}

CSerializationChunk::~CSerializationChunk()
{
	--g_objcnt.serializationChunk;
}

void CSerializationChunk::DecodeFromStream( CNetInputSerializeImpl& input, CByteOutputStream& output, ChunkID chunkID, uint8 nProfile ) const
{
	MiniQueue<TOps::const_iterator, 128> endGroupOps;

	output.PutTyped<ChunkID>() = chunkID;
	output.PutTyped<uint8>() = nProfile;

#if CRC8_ASPECT_FORMAT
	uint8 recCrc = input.GetInput().ReadBits(8);
	NET_ASSERT(recCrc == m_crc);
	if (recCrc != m_crc)
		input.Failed();
	uint8 recLen = input.GetInput().ReadBits(8);
	NET_ASSERT(recLen == m_ops.size());
	if (recLen != m_ops.size())
		input.Failed();
#endif

	for (TOps::const_iterator pOp = m_ops.begin(); pOp != m_ops.end(); ++pOp)
	{
		if (!endGroupOps.Empty() && endGroupOps.Front() == pOp)
		{
			input.EndGroup();
			endGroupOps.Pop();
		}

		switch (pOp->type)
		{
		default:
			DecodeFromStreamImpl[pOp->type]( NAME_FROM_OP(pOp), input, output, pOp->pPolicy );
			break;

		case eO_String:
			GenImpl<SSerializeString>::DecodeFromStream( NAME_FROM_OP(pOp), input, output, pOp->pPolicy );
			break;

		case eO_OptionalGroup:
			{
				if (pOp->skipFalse)
				{
					bool cond = input.BeginOptionalGroup( NAME_FROM_OP(pOp), false );
					output.PutByte(cond);
					if (!cond)
						pOp += pOp->skipFalse;
					else
						endGroupOps.Push(pOp + pOp->skipFalse);
				}
				else
					int k = 0;
			}
			break;
		}
	}

#if CRC8_ASPECT_FORMAT
	if (!input.Ok())
	{
		NetWarning("Readback failed: magic number is %.2x/%.2x:%.2x/%.2x", recCrc, recLen, m_crc, m_ops.size());
	}
#endif
}

void CSerializationChunk::EncodeToStream( CByteInputStream& input, CNetOutputSerializeImpl& output, ChunkID chunkID, uint8 nProfile ) const
{
	MiniQueue<TOps::const_iterator, 128> endGroupOps;

	ChunkID savedChunkID = input.GetTyped<ChunkID>();
	uint8 savedProfile = input.GetTyped<uint8>();
	NET_ASSERT(savedChunkID == chunkID);
	NET_ASSERT(savedProfile == nProfile);

#if CRC8_ASPECT_FORMAT
	output.GetOutput().WriteBits(m_crc, 8);
	output.GetOutput().WriteBits(m_ops.size(), 8);
#endif

	for (TOps::const_iterator pOp = m_ops.begin(); pOp != m_ops.end(); ++pOp)
	{
		if (!endGroupOps.Empty() && endGroupOps.Front() == pOp)
		{
			output.EndGroup();
			endGroupOps.Pop();
		}

		switch (pOp->type)
		{
		default:
			EncodeToStreamImpl[pOp->type]( NAME_FROM_OP(pOp), input, output, pOp->pPolicy );
			break;

		case eO_String:
			GenImpl<SSerializeString>::EncodeToStream( NAME_FROM_OP(pOp), input, output, pOp->pPolicy );
			break;

		case eO_OptionalGroup:
			{
				if (pOp->skipFalse)
				{
					bool cond = input.GetByte() != 0;
					output.BeginOptionalGroup( NAME_FROM_OP(pOp), cond );
					if (!cond)
						pOp += pOp->skipFalse;
					else
						endGroupOps.Push(pOp + pOp->skipFalse);
				}
				else
					int k = 0;
			}
			break;
		}
	}
}

ESynchObjectResult CSerializationChunk::UpdateGame( IGameContext * pCtx, CByteInputStream& input, EntityId id, uint8 nAspect, ChunkID chunkID, uint8 nProfile, bool& wasPartialUpdate ) const
{
	ChunkID savedChunkID = input.GetTyped<ChunkID>();
	uint8 savedProfile = input.GetTyped<uint8>();

	if (savedChunkID != chunkID || savedProfile != nProfile)
	{
		if (CNetCVars::Get().LogLevel > 0)
			NetWarning("Chunk/profile mismatch: chunk is %d and was %d; profile is %d and was %d; skipping update", int(chunkID), int(savedChunkID), int(nProfile), int(savedProfile));
		return eSOR_Skip;
	}

	CFromBufferImpl impl(input, this);
	CSimpleSerialize<CFromBufferImpl> ser(impl);
	ESynchObjectResult result = pCtx->SynchObject( id, nAspect, nProfile, &ser, true );
	wasPartialUpdate = impl.WasPartial();
	if (result == eSOR_Failed)
	{
		NetWarning("CSerializationChunk::UpdateGame: game failed to sync object");
	}
	if (!ser.Ok())
	{
		NetWarning("CSerializationChunk::UpdateGame: serialization failed");
		result = eSOR_Failed;
	}
	if (result != eSOR_Failed)
	{
		if (!impl.Complete(false && result == eSOR_Ok))
		{
			NetWarning("CSerializationChunk::UpdateGame: failed to complete serialization");
			return eSOR_Failed;
		}
	}
	return result;
}

bool CSerializationChunk::FetchFromGame( IGameContext * pCtx, CByteOutputStream& output, EntityId id, uint8 nAspect, ChunkID chunkID, uint8 nProfile ) const
{
	output.PutTyped<ChunkID>() = chunkID;
	output.PutTyped<uint8>() = nProfile;

	CToBufferImpl impl(output, this);
	CSimpleSerialize<CToBufferImpl> ser(impl);
	return pCtx->SynchObject( id, nAspect, nProfile, &ser, true ) == eSOR_Ok && impl.Complete() && ser.Ok();
}

bool CSerializationChunk::Init( IGameContext * pCtx, EntityId id, uint8 nAspect, uint8 nProfile )
{
	CBuildImpl impl(this);
	CSimpleSerialize<CBuildImpl> ser(impl);
	return pCtx->SynchObject( id, nAspect, nProfile, &ser, false ) == eSOR_Ok && impl.Complete() && ser.Ok();
}

bool CSerializationChunk::IsEmpty() const
{
	return m_ops.empty();
}

bool CSerializationChunk::operator <( const CSerializationChunk& rhs ) const
{
	if (m_ops.size() < rhs.m_ops.size())
		return true;
	else if (m_ops.size() > rhs.m_ops.size())
		return false;
#if CRC8_ASPECT_FORMAT
	if (m_crc < rhs.m_crc)
		return true;
	else if (m_crc > rhs.m_crc)
		return false;
#endif
	for (size_t i=0; i<m_ops.size(); i++)
	{
		const SOp& lop = m_ops[i];
		const SOp& rop = rhs.m_ops[i];

		if (lop.type < rop.type)
			return true;
		else if (lop.type > rop.type)
			return false;
		else if (lop.skipFalse < rop.skipFalse)
			return true;
		else if (lop.skipFalse > rop.skipFalse)
			return false;
		else if (lop.pPolicy < rop.pPolicy)
			return true;
		else if (lop.pPolicy > rop.pPolicy)
			return false;
#if TRACK_ENCODING
		else if (lop.name < rop.name)
			return true;
		else if (lop.name > rop.name)
			return false;
#endif
	}
	return false;
}

#if ENABLE_DEBUG_KIT
void CSerializationChunk::Dump( uint32 id )
{
#if CRC8_ASPECT_FORMAT
	NetLog("-------- SERIALIZATION CHUNK %.8x magicnumber=%.2x/%.2x --------", id, int(m_crc), m_ops.size());
#else
	NetLog("-------- SERIALIZATION CHUNK %.8x --------", id);
#endif


	for (TOps::const_iterator pOp = m_ops.begin(); pOp != m_ops.end(); ++pOp)
	{
		string out;

#if TRACK_ENCODING
		out += string().Format(" name='%s'", pOp->name);
#endif

		switch (pOp->type)
		{
		default:
			out = "VALUE" + out + string().Format(" type='%s' policy='%s'", EntryNames[pOp->type], KeyToString(pOp->pPolicy->key));
			break;

		case eO_String:
			out = "VALUE" + out + string().Format(" type='string' policy='%s'", KeyToString(pOp->pPolicy->key));
			break;

		case eO_OptionalGroup:
			out = "OPGRP" + out + string().Format(" jump-if-false=%d", pOp->skipFalse);
			break;
		}

		NetLog("%s", out.c_str());
	}
}
#endif
