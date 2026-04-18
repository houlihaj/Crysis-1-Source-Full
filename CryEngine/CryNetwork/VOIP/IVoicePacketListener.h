#ifndef __IVOICEPACKETLISTENER_H__
#define __IVOICEPACKETLISTENER_H__

#include "VoicePacket.h"
#include "INetwork.h"

struct IVoicePacketListener
{
	virtual void AddRef() = 0;
	virtual void Release() = 0;

	virtual void OnVoicePacket( SNetObjectID object, const TVoicePacketPtr& pPkt ) = 0;
};

typedef _smart_ptr<IVoicePacketListener> IVoicePacketListenerPtr;

#endif
