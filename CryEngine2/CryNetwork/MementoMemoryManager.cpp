#include "StdAfx.h"
#include "MementoMemoryManager.h"
#include "Network.h"
//#include <IRenderer.h>
#include "ITextModeConsole.h"

CMementoMemoryManager * CMementoMemoryRegion::m_pMMM = 0;

CMementoMemoryManager::CMementoMemoryManager()
{
	ASSERT_GLOBAL_LOCK;
	arith_zeroSizeHdl = InvalidHdl;
	for (int i=0; i<sizeof(pThings)/sizeof(*pThings); i++)
		pThings[i] = 0;
	m_pools[0] = new CPool<8>();
	m_pools[1] = new CPool<16>();
	m_pools[2] = new CPool<32>();
	m_pools[3] = new CPool<64>();
	m_pools[4] = new CPool<128>();
	m_pools[5] = new CPool<256>();
	m_pools[6] = new CPool<512>();
	m_pools[7] = new CPool<1024>();
	m_pools[8] = new CPool<2048>();
	m_pools[9] = new CPool<4096>();
	STATIC_CHECK(NPOOLS == 10, PoolsIncorrectlyInitialized);
	m_totalAllocations = 0;
}

CMementoMemoryManager::~CMementoMemoryManager()
{
	SCOPED_GLOBAL_LOCK;
	FreeHdl(arith_zeroSizeHdl);
	{
		MMM_REGION(this);
		for (int i=0; i<sizeof(pThings)/sizeof(*pThings); i++)
			SAFE_RELEASE(pThings[i]);
	}
	NET_ASSERT(m_totalAllocations == 0);
	NET_ASSERT(m_handles.size() == m_freeHandles.size());
	for (int i=0; i<NPOOLS; i++)
		delete m_pools[i];
}

CMementoMemoryManager::Hdl CMementoMemoryManager::AllocHdl( size_t sz, void * callerOverride )
{
	ASSERT_GLOBAL_LOCK;
	Hdl hdl = InvalidHdl;
	if (m_freeHandles.empty())
	{
		hdl = m_handles.size();
		m_handles.push_back(SHandleData());
	}
	else
	{
		hdl = m_freeHandles.back();
		m_freeHandles.pop_back();
	}

	InitHandleData( m_handles[hdl], sz );
	m_totalAllocations += sz;

#if MMM_CHECK_LEAKS
	void * caller = callerOverride? callerOverride : UP_STACK_PTR;
	m_hdlToAlloc[hdl] = caller;
	m_allocAmt[caller] += sz;
#endif

	return ProtectHdl(hdl);
}

CMementoMemoryManager::Hdl CMementoMemoryManager::CloneHdl( Hdl hdl )
{
	ASSERT_GLOBAL_LOCK;
	Hdl out = AllocHdl( GetHdlSize(hdl), UP_STACK_PTR );
	memcpy( PinHdl(out), PinHdl(hdl), GetHdlSize(hdl) );
	return out;
}

void CMementoMemoryManager::ResizeHdl( Hdl hdl, size_t sz )
{
	ASSERT_GLOBAL_LOCK;
	hdl = UnprotectHdl(hdl);

	NET_ASSERT(hdl != InvalidHdl);
	SHandleData& hd = m_handles[hdl];

#ifdef CHECK_POOL_ALLOCATED_POINTERS
	hd.pPool->CheckPtr(hd.p);
#endif

	m_totalAllocations -= hd.size;
	m_totalAllocations += sz;

#if MMM_CHECK_LEAKS
	void * caller = m_hdlToAlloc[hdl];
	uint32& callerAlloc = m_allocAmt[caller];
	callerAlloc -= hd.size;
	callerAlloc += sz;
#endif

	if (sz > hd.size) // growing
	{
		if (sz > hd.capacity) // growing and changing pools
		{
			SHandleData hdp;
			InitHandleData( hdp, sz );
			memcpy( hdp.p, hd.p, hd.size );
			hd.pPool->Free(hd.p);
			hd = hdp;
		}
		else
		{
			hd.size = sz;
		}
	}
	else if (sz < hd.size) // shrinking
	{
		if (sz < hd.capacity/2) // shrinking and changing pools
		{
			SHandleData hdp;
			InitHandleData( hdp, sz );
			memcpy( hdp.p, hd.p, sz );
			hd.pPool->Free(hd.p);
			hd = hdp;
		}
		else
		{
			hd.size = sz;
		}
	}
}

void CMementoMemoryManager::FreeHdl( Hdl hdl )
{
	ASSERT_GLOBAL_LOCK;
	if (hdl == InvalidHdl)
		return;
	hdl = UnprotectHdl(hdl);
	SHandleData& hd = m_handles[hdl];
	m_totalAllocations -= hd.size;

#if MMM_CHECK_LEAKS
	void * caller = m_hdlToAlloc[hdl];
	m_hdlToAlloc.erase(hdl);
	if (!(m_allocAmt[caller] -= hd.size))
		m_allocAmt.erase(caller);
#endif

	hd.pPool->Free(hd.p);
	m_freeHandles.push_back(hdl);
}

void CMementoMemoryManager::InitHandleData( SHandleData& hd, size_t sz )
{
	ASSERT_GLOBAL_LOCK;
	size_t szP2 = IntegerLog2_RoundUp(sz);
	NET_ASSERT((1u<<szP2) >= sz);
	int pool = std::max( (int)szP2 - FIRST_POOL, 0 );
	hd.size = sz;
	if (pool < NPOOLS)
	{
		hd.pPool = m_pools[pool];
		hd.p = hd.pPool->Allocate( hd.capacity );
	}
	else
	{
		hd.pPool = &m_largeObjectPool;
		hd.p = m_largeObjectPool.AllocateSized( sz, hd.capacity );
	}
	NET_ASSERT(hd.capacity >= sz);

#ifdef CHECK_POOL_ALLOCATED_POINTERS
	hd.pPool->CheckPtr(hd.p);
#endif
}

void * CMementoMemoryManager::AllocPtr( size_t sz, void * callerOverride )
{
	ASSERT_GLOBAL_LOCK;
	SHandleData hd;
	m_totalAllocations += sz;
	InitHandleData( hd, sz );

#if MMM_CHECK_LEAKS
	void * caller = callerOverride? callerOverride : UP_STACK_PTR;
	m_ptrToAlloc[hd.p] = caller;
	m_allocAmt[caller] += sz;
#endif

	return hd.p;
}

void CMementoMemoryManager::FreePtr( void * p, size_t sz )
{
	ASSERT_GLOBAL_LOCK;
	int pool = std::max( (int)IntegerLog2_RoundUp(sz) - FIRST_POOL, 0 );
	if (pool < NPOOLS)
		m_pools[pool]->Free(p);
	else
		m_largeObjectPool.Free(p);

#if MMM_CHECK_LEAKS
	void * caller = m_ptrToAlloc[p];
	m_ptrToAlloc.erase(p);
	if (!(m_allocAmt[caller] -= sz))
		m_allocAmt.erase(caller);
#endif

	m_totalAllocations -= sz;
}

void CMementoMemoryManager::AddHdlToSizer( Hdl hdl, ICrySizer * pSizer )
{
	if (hdl != InvalidHdl)
		pSizer->AddObject( PinHdl(hdl), GetHdlSize(hdl) );
}

void CMementoMemoryManager::GetMemoryStatistics(ICrySizer* pSizer, bool countingThis /* = false */)
{
	SIZER_COMPONENT_NAME(pSizer, "CMementoMemoryManager");

	if (countingThis)
		pSizer->Add(*this);
	pSizer->AddContainer(m_handles);
	pSizer->AddContainer(m_freeHandles);
}

void CMementoMemoryManager::DrawDebugLine( int grp, int x, int y, const char * fmt, ... )
{
	char buffer[512];

	va_list args;
	va_start( args, fmt );
	vsprintf_s( buffer, fmt, args );
	va_end( args );

	float white[] = {1,1,1,1};

	gEnv->pRenderer->Draw2dLabel( x*10+10, (y+grp*12)*10+10, 1, white, false, "%s", buffer );
	if (ITextModeConsole * pC = gEnv->pSystem->GetITextModeConsole())
		pC->PutText( x, y+grp*13, buffer );
}

void CMementoMemoryManager::DebugDraw( int n )
{
#if ENABLE_DEBUG_KIT
	if ((CVARS.MemInfo & eDMM_Mementos) == 0)
		return;

	static const char * names[] = {
		"Memento data",
		"Packet data",
		"Object data",
		"Alphabet soup"
	};

	const char * name = "<unknown manager>";
	if (n >= 0 && n < (sizeof(names)/sizeof(*names)))
		name = names[n];

	int y = 0;
	DrawDebugLine( n, 0, y++, "Memento memory for %s", name );

	SPoolStats aggregateStats;
	for (int i=0; i<NPOOLS; i++)
	{
		SPoolStats stats = m_pools[i]->GetPoolStats();

		if (!stats.allocated)
			continue;

		DrawDebugLine( n, 2, y++, "Pool %d (%d byte blocks): alloc=%d, used=%d, waste=%.1f%%", i, 1<<(i+FIRST_POOL), stats.allocated, stats.used, stats.GetWastePercent() );

		aggregateStats.allocated += stats.allocated;
		aggregateStats.used += stats.used;
	}

	float liveWaste = 0;
	if (aggregateStats.allocated)
		liveWaste = 100.0f * (1.0f - float(m_totalAllocations) / float(aggregateStats.allocated));
	float poolWaste = 0;
	if (aggregateStats.used)
		poolWaste = 100.0f * (1.0f - float(m_totalAllocations) / float(aggregateStats.used));

	DrawDebugLine( n, 2, y++, "Total: live=%d, alloc=%d, used=%d, live-waste=%.1f%%, alloc-waste=%.1f%%, pool-waste=%.1f%%", m_totalAllocations, aggregateStats.allocated, aggregateStats.used, liveWaste, aggregateStats.GetWastePercent(), poolWaste );
#endif
}

/*
 * CMementoStreamAllocator
 */

CMementoStreamAllocator::CMementoStreamAllocator( const CMementoMemoryManagerPtr& mmm ) : m_hdl(CMementoMemoryManager::InvalidHdl), m_pPin(0), m_mmm(mmm)
{
}

void * CMementoStreamAllocator::Alloc( size_t sz, void * callerOverride )
{
	NET_ASSERT( m_hdl == CMementoMemoryManager::InvalidHdl );
	m_hdl = m_mmm->AllocHdl(sz, callerOverride? callerOverride : UP_STACK_PTR);

	return m_pPin = m_mmm->PinHdl(m_hdl);
}

void * CMementoStreamAllocator::Realloc( void * old, size_t sz )
{
	NET_ASSERT(m_pPin == old && m_pPin);
	m_mmm->ResizeHdl( m_hdl, sz );

	return m_pPin = m_mmm->PinHdl(m_hdl);
}

void CMementoStreamAllocator::Free( void *old )
{
}
