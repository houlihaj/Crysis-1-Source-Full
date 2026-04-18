/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  implements communications between two machines, with 
               context setup
 -------------------------------------------------------------------------
 History:
 - 26/07/2004   : Created by Craig Tiller
*************************************************************************/

#ifndef __NULLSENDABLE_H__
#define __NULLSENDABLE_H__

#pragma once

#include "Network.h"

class CNullSendable : public INetSendable
{
public:
	~CNullSendable()
	{
		--g_objcnt.nullSendable;
	}

	virtual size_t GetSize() { return sizeof(*this); }
	virtual EMessageSendResult Send( INetSender * pSender ) { return eMSR_SentOk; }
	virtual void UpdateState( uint32 nFromSeq, ENetSendableStateUpdate update ) {}
	virtual const char * GetDescription() { return "NullSendable"; }
	virtual void GetPositionInfo( SMessagePositionInfo& pos ) {}

	static CNullSendable * Alloc( uint32 flags, ENetReliabilityType reliability );

private:
	CNullSendable( uint32 flags, ENetReliabilityType reliability ) : INetSendable(flags, reliability) 
	{
		++g_objcnt.nullSendable;
	}
	virtual void DeleteThis();

	TMemHdl m_memhdl;
};

#endif
