/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  Gamespy NAT negotiation implementation
-------------------------------------------------------------------------
History:
- 12/07/2006   : Stas Spivakov, Created
*************************************************************************/

#ifndef __GSNAT_H__
#define __GSNAT_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameSpy/natneg/natneg.h"

struct sockaddr_in;

class CGSNatNeg : public CMultiThreadRefCount, public INatNeg
{
private:
    struct NatNegCallback;
    typedef std::map<int,NatNegCallback*> CallbackMap;
public:
    CGSNatNeg(INatNegListener* lst);
    ~CGSNatNeg();
    virtual void    StartNegotiation(int cookie,bool server,int socket); 
    virtual void    CancelNegotiation(int cookie);
    virtual void    OnPacket(char *data, int len, sockaddr_in *fromaddr);

    virtual bool    IsAvailable()const;
    bool            IsDead()const;

    void            Think();

		void GetMemoryStatistics(ICrySizer* pSizer)
		{
			SIZER_COMPONENT_NAME(pSizer, "CGSNatNeg");

			pSizer->Add(*this);
			pSizer->AddContainer(m_callbacks);
		}
private:
    static void     TimerCallback(NetTimerId,void*,CTimeValue);
    void    OnCompleted(int cookie, bool success, sockaddr_in  *addr);
    void    DeleteCallback(int cookie);

    INatNegListener*    m_pListener;
    NetTimerId          m_updateTimer;
    CallbackMap         m_callbacks;
};

#endif /*__GSNAT_H__*/

