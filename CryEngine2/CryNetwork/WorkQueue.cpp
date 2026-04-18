#include "StdAfx.h"
#include "WorkQueue.h"

void CWorkQueue::Flush( bool rt )
{
	Executer e;
	if (rt)
		m_jobQueue.RealtimeFlush( e );
	else
		m_jobQueue.Flush( e );
}
