/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  implementation of CryNetwork ISerialize classes
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   10:34 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "Streams/ArithStream.h"
#include "Serialize.h"
#include "Network.h"
#include "Config.h"
#include "Compression/SerializationChunk.h"
#include "Compression/CompressionManager.h"
#include "Streams/ByteStream.h"

#if ENABLE_DEBUG_KIT
bool CNetSerialize::m_bEnableLogging;
#endif

//
// NetOutputSerialize
//

CNetOutputSerializeImpl::CNetOutputSerializeImpl( uint8 * pBuffer, size_t nSize, uint8 nBonus ) :
	m_output( pBuffer, nSize, nBonus )
{
#if ENABLE_DEBUG_KIT
	if (m_bEnableLogging)
		m_output.EnableLog();
#endif
}

CNetOutputSerializeImpl::CNetOutputSerializeImpl( IStreamAllocator * pAllocator, size_t initialSize, uint8 bonus ) :
	m_output( pAllocator, initialSize, bonus )
{
#if ENABLE_DEBUG_KIT
	if (m_bEnableLogging)
		m_output.EnableLog();
#endif
}

ESerializeChunkResult CNetOutputSerializeImpl::SerializeChunk( ChunkID chunk, uint8 profile, TMemHdl * phData, CTimeValue * pTimeInfo, CMementoMemoryManager& mmm )
{
	CTimeValue temp;
	if (!phData || *phData == CMementoMemoryManager::InvalidHdl)
	{
		return eSCR_Failed;
	}
	else
	{
		CByteInputStream in((const uint8*) mmm.PinHdl(*phData), mmm.GetHdlSize(*phData));
		if (pTimeInfo)
		{
			*pTimeInfo = in.GetTyped<CTimeValue>();
			Value("_tstamp", *pTimeInfo, 'tPhy');
		}
		CNetwork::Get()->GetCompressionManager().BufferToStream( chunk, profile, in, *this );
		return eSCR_Ok;
	}
}

void CNetOutputSerializeImpl::ResetLogging()
{
#if ENABLE_DEBUG_KIT
	m_output.EnableLog( m_bEnableLogging );
#endif
}

//
// NetInputSerialize
//

CNetInputSerializeImpl::CNetInputSerializeImpl( const uint8 * pBuffer, size_t nSize, INetChannel * pChannel ) :
	m_input( pBuffer, nSize ), m_pChannel(pChannel)
{
#if ENABLE_DEBUG_KIT
	if (m_bEnableLogging)
		m_input.EnableLog();
#endif
}

ESerializeChunkResult CNetInputSerializeImpl::SerializeChunk( ChunkID chunk, uint8 profile, TMemHdl * phData, CTimeValue * pTimeInfo, CMementoMemoryManager& mmm )
{
	if (!Ok())
		return eSCR_Failed;

	CMementoStreamAllocator alloc(&mmm);
	size_t sizeHint = 8;
	if (phData && *phData != CMementoMemoryManager::InvalidHdl)
		sizeHint = mmm.GetHdlSize(*phData);
	CByteOutputStream stm(&alloc, sizeHint);
	if (pTimeInfo)
	{
		Value("_tstamp", *pTimeInfo, 'tPhy');
		stm.PutTyped<CTimeValue>() = *pTimeInfo;
	}
	CNetwork::Get()->GetCompressionManager().StreamToBuffer( chunk, profile, *this, stm );
	if (!Ok())
	{
		mmm.FreeHdl( alloc.GetHdl() );
		return eSCR_Failed;
	}
	bool take = false;
	if (phData && m_bCommit)
	{
		take = true;
		if (*phData != CMementoMemoryManager::InvalidHdl)
		{
			if (mmm.GetHdlSize(*phData) == stm.GetSize())
				if (0 == memcmp(mmm.PinHdl(*phData), mmm.PinHdl(alloc.GetHdl()), stm.GetSize()))
					take = false;
			if (take)
			{
				mmm.FreeHdl( *phData );
				mmm.ResizeHdl( alloc.GetHdl(), stm.GetSize() );
				*phData = alloc.GetHdl();
			}
			else
			{
				mmm.FreeHdl( alloc.GetHdl() );
			}
		}
		else
		{
			mmm.ResizeHdl( alloc.GetHdl(), stm.GetSize() );
			*phData = alloc.GetHdl();
		}
	}
	else
	{
		mmm.FreeHdl( alloc.GetHdl() );
	}
	return take? eSCR_Ok_Updated : eSCR_Ok;
}
