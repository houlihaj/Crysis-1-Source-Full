/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  Message definition to id management
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#ifndef MESSAGE_MAPPER_H
#define MESSAGE_MAPPER_H

#include "INetwork.h"
#include "VectorMap.h"

//! this class maps SNetMessageDef pointers to message-id's and back again
class CMessageMapper
{
public:
	CMessageMapper();

	void Reset( size_t nReservedMessages, const SNetProtocolDef* pProtocols, size_t nProtocols );

	uint32 GetMsgId( const SNetMessageDef * ) const;
	const SNetMessageDef * GetDispatchInfo( uint32 id, size_t * pnSubProtocol );
	uint32 GetNumberOfMsgIds() const;

	bool LookupMessage( const char * name, SNetMessageDef const ** pDef, size_t * pnSubProtocol );

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CMessageMapper");

		pSizer->Add(*this);

		pSizer->AddContainer(m_messages);
		pSizer->AddContainer(m_IdMap);
		pSizer->AddContainer(m_DefMap);
		pSizer->AddContainer(m_nameToDef);
	}

private:
	struct SSinkMessages
	{
		uint32 nStart;
		uint32 nEnd;
		const SNetMessageDef * pFirst;
		const SNetMessageDef * pLast;
	};
	typedef std::vector<SSinkMessages> TMsgVec;
	TMsgVec m_messages;

	typedef VectorMap<const SNetMessageDef *, uint32> TIdMap;
	typedef std::vector<const SNetMessageDef *> TDefMap;
	TIdMap m_IdMap;
	TDefMap m_DefMap;
	uint32 m_DefMapBias;

	struct LTStr
	{
		bool operator()( const char * a, const char * b ) const
		{
			return strcmp( a, b ) < 0;
		}
	};
	std::map<const char *, const SNetMessageDef *, LTStr> m_nameToDef;
};

#endif
