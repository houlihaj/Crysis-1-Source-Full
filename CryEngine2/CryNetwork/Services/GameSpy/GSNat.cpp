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
#include "StdAfx.h"
#include "Network.h"
#include "INetworkService.h"
#include "GSNat.h"

static const float UPDATE_INTERVAL = 0.1f;

struct CGSNatNeg::NatNegCallback
{
    NatNegCallback(int cookie,CGSNatNeg* natneg):m_cookie(cookie),m_natNeg(natneg)
    {
    }

    static void OnNegotiateProgress(NegotiateState  res, void* obj)
    {
        NatNegCallback *pThis = (NatNegCallback*)obj;
        const char* res_str[] = {"initsent", "initack", "connectping", "finished", "canceled", "reportsent", "reportack"};
        NetLog("GS NAT: 0x%X Processing...%s.",pThis->m_cookie,res_str[int(res)]);
    }

    static void OnNegotiateCompleted(NegotiateResult res, SOCKET, struct sockaddr_in* addr, void* obj)
    {
        NatNegCallback *pThis = (NatNegCallback*)obj;
        const char* res_str[] = {"success", "deadbeatpartner", "inittimeout", "pingtimeout", "unknownerror", "noresult" };
        NetLog("GS NAT: 0x%X Completed... %s.",pThis->m_cookie,res_str[int(res)]);

        //callback should be the last thing, pThis will be deleted in it
        pThis->m_natNeg->OnCompleted(pThis->m_cookie,res==nr_success,addr);
    }

    int         m_cookie;
    CGSNatNeg*  m_natNeg;
};

CGSNatNeg::CGSNatNeg(INatNegListener* lst)
: m_pListener(lst),m_updateTimer(0)
{
  SCOPED_GLOBAL_LOCK;
  m_updateTimer = TIMER.AddTimer(g_time+UPDATE_INTERVAL,TimerCallback,this);
}

CGSNatNeg::~CGSNatNeg()
{
    m_pListener = 0;
		NNFreeNegotiateList();
		if(m_updateTimer)
		{
			SCOPED_GLOBAL_LOCK;
			TIMER.CancelTimer(m_updateTimer);
			m_updateTimer = 0;
		}
    for(CallbackMap::iterator it=m_callbacks.begin();it!=m_callbacks.end(); ++it)
    {
        delete it->second;
    }
}

void CGSNatNeg::StartNegotiation(int cookie, bool server, int socket)
{
    CallbackMap::iterator it = m_callbacks.find(cookie);
    if(it!=m_callbacks.end())
    {
        NetWarning("GS NAT : attempting to start negotiation with cookie which is already in use.");
        if(m_pListener)
            m_pListener->OnCompleted(cookie,false,0);
        return;
    }
    NatNegCallback* pCB = new NatNegCallback(cookie,this);
    m_callbacks.insert(std::make_pair(cookie,pCB));
    NNBeginNegotiationWithSocket(socket,cookie,server?0:1,NatNegCallback::OnNegotiateProgress,NatNegCallback::OnNegotiateCompleted,pCB);
}

void CGSNatNeg::CancelNegotiation(int cookie)
{
    NNCancel(cookie);
    DeleteCallback(cookie);
/*    SCOPED_GLOBAL_LOCK;
    TIMER.CancelTimer(m_updateTimer);*/
}

void CGSNatNeg::OnPacket(char *data, int len, struct sockaddr_in *fromaddr)//happens in NetSend/NetRecieve thread, lock is held
{
    NNProcessData(data,len,fromaddr);    
}

bool    CGSNatNeg::IsAvailable()const
{
    return true;
}

bool    CGSNatNeg::IsDead()const
{
    return m_pListener == 0;
}

void    CGSNatNeg::Think()
{
    NNThink();
}

void CGSNatNeg::TimerCallback(NetTimerId id,void* obj,CTimeValue)//in NetSend thread
{
    CGSNatNeg* pNeg = (CGSNatNeg*)obj;

    if(!pNeg->IsDead() && pNeg->m_updateTimer == id)
    {
        pNeg->Think();
        SCOPED_GLOBAL_LOCK;
        pNeg->m_updateTimer = TIMER.AddTimer( g_time+UPDATE_INTERVAL, TimerCallback, pNeg );
    }
}

void   CGSNatNeg::OnCompleted(int cookie, bool success, sockaddr_in  *addr)
{
    if(m_pListener)
        m_pListener->OnCompleted(cookie,success,addr);
    DeleteCallback(cookie);
}

void    CGSNatNeg::DeleteCallback(int cookie)
{
    CallbackMap::iterator it = m_callbacks.find(cookie);
    if(it!=m_callbacks.end())
    {
        delete it->second;
        m_callbacks.erase(it);
    }    
}