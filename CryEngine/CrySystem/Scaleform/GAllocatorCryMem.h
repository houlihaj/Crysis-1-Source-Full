/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A wrapper around Scaleform's GAllocator interface to delegate memory allocations to CryMemoryManager
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _GALLOCATOR_CRYMEM_H_
#define _GALLOCATOR_CRYMEM_H_

#pragma once


#ifndef EXCLUDE_SCALEFORM_SDK


#include <GAllocator.h>


class GBlockAllocatorCryMem : public GAllocator
{
public:
	// GAllocator interface
	virtual void Free(void* pMemBlock);
	virtual void* Alloc(UPInt size);
	virtual void* Realloc(void* pMemBlock, UPInt newSize);
	virtual const GAllocatorStats* GetStats() const;

public:
	GBlockAllocatorCryMem();
	~GBlockAllocatorCryMem();

private:
	GAllocatorStats m_stats;
};


#endif // #ifndef EXCLUDE_SCALEFORM_SDK


#endif // #ifndef _GALLOCATOR_CRYMEM_H_