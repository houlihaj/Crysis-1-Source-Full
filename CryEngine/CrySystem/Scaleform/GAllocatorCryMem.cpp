#include "StdAfx.h"


#ifndef EXCLUDE_SCALEFORM_SDK


#include "GAllocatorCryMem.h"


static inline size_t GetSize(void* pMem)
{
#if !defined(_DEBUG) && !defined(NOT_USE_CRY_MEMORY_MANAGER)
	return CryGetMemSize(pMem, 0);
#else
	return 0;
#endif	
}


GBlockAllocatorCryMem::GBlockAllocatorCryMem()
: m_stats()
{
	memset(&m_stats, 0, sizeof(m_stats));
}


GBlockAllocatorCryMem::~GBlockAllocatorCryMem()
{
}


void GBlockAllocatorCryMem::Free(void* pMemBlock)
{
#if !defined(_DEBUG) && !defined(NOT_USE_CRY_MEMORY_MANAGER)
	++m_stats.FreeCount;
	if (pMemBlock)
	{
		size_t freeSize(GetSize(pMemBlock));
		m_stats.FreeSize += freeSize;
		m_stats.FreeSizeUsed += freeSize;
	}
#endif
	free(pMemBlock);
}


void* GBlockAllocatorCryMem::Alloc(UPInt size)
{
	void* pMemBlock(malloc(size));
#if !defined(_DEBUG) && !defined(NOT_USE_CRY_MEMORY_MANAGER)
	++m_stats.AllocCount;
	if (pMemBlock)
	{
		size_t allocSize(GetSize(pMemBlock));
		m_stats.AllocSize += allocSize;
		m_stats.AllocSizeUsed += allocSize;
	}
#endif
	return pMemBlock;
}


void* GBlockAllocatorCryMem::Realloc(void* pMemBlock, UPInt newSize)
{
#if !defined(_DEBUG) && !defined(NOT_USE_CRY_MEMORY_MANAGER)
	++m_stats.ReallocCount;
	if (pMemBlock)
	{
		size_t _oldSize(GetSize(pMemBlock));
		m_stats.FreeSize += _oldSize;
		m_stats.FreeSizeUsed += _oldSize;
	}	
#endif
	pMemBlock = realloc(pMemBlock, newSize);
#if !defined(_DEBUG) && !defined(NOT_USE_CRY_MEMORY_MANAGER)
	if (pMemBlock)
	{
		size_t _newSize(GetSize(pMemBlock));
		m_stats.AllocSize += _newSize;
		m_stats.AllocSizeUsed += _newSize;
	}
#endif
	return pMemBlock;
}


const GAllocatorStats* GBlockAllocatorCryMem::GetStats() const
{
	return &m_stats;
}


#endif // #ifndef EXCLUDE_SCALEFORM_SDK