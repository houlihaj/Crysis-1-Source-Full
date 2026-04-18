#include "StdAfx.h"
#include "VoicePacket.h"
#include "ISerialize.h"
#include "PoolAllocator.h"

int32 CVoicePacket::m_count=0;

#if USE_SYSTEM_ALLOCATOR
TVoicePacketPtr CVoicePacket::Allocate()
{
	return new CVoicePacket;
}

void CVoicePacket::Deallocate( CVoicePacket * pPkt )
{
	delete pPkt;
}
#else
typedef stl::PoolAllocator<sizeof(CVoicePacket)> TVoicePool;
static TVoicePool * m_pVoicePool = NULL;

TVoicePacketPtr CVoicePacket::Allocate()
{
	if (!m_pVoicePool)
		m_pVoicePool = new TVoicePool;
	void * pPkt = m_pVoicePool->Allocate();
	new (pPkt) CVoicePacket();
	return (CVoicePacket*) pPkt;
}

void CVoicePacket::Deallocate( CVoicePacket * pPkt )
{
	pPkt->~CVoicePacket();
	m_pVoicePool->Deallocate(pPkt);
}
#endif

CVoicePacket::CVoicePacket() : m_cnt(0), m_length(0)
{
	m_count++;
	++g_objcnt.voicePacket;
}

CVoicePacket::~CVoicePacket()
{
	m_count--;
	--g_objcnt.voicePacket;
}

void CVoicePacket::Serialize( TSerialize ser )
{
	ser.Value( "seq", m_seq );
	ser.EnumValue( "length", m_length, 0, MAX_LENGTH );
	ser.BeginGroup("data");
	for (uint32 i = 0; i<m_length; i++)
		ser.Value( "voicebyte", m_data[i] );
	ser.EndGroup();
}

int32 CVoicePacket::GetCount()
{
	return m_count;
}
