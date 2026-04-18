#include "StdAfx.h"

// For profiling
#include <ISystem.h>

// Undefine malloc for memory manager itself..
#undef malloc
#undef realloc
#undef free

//#include <windows.h>
#include <intrin.h>

#define ALLOCATOR_DEBUG_MODE			0

#define ALLOCATOR_DATA_THRASHING	0
#define ALLOCATOR_DEBUG_REALLOC		0

#define ALLOCATOR_FILL_ALLOC			0
#define ALLOCATOR_SANITY_CHECKS		0


//#define ALLOCATOR_SIZE_LIMIT			256
#define ALLOCATOR_SIZE_LIMIT			512
//#define ALLOCATOR_SIZE_CLASSES			(ALLOCATOR_SIZE_LIMIT/8+1)
#define ALLOCATOR_SIZE_CLASSES			(37+1)

static char gSizeToSizeClass[ALLOCATOR_SIZE_LIMIT/8+1] =
{
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 27, 28, 28, 29, 29,
	30, 30, 31, 31, 32, 32, 32, 33, 33, 33, 34, 34, 34, 34, 35, 35, 35, 35, 35, 36, 36, 36, 36, 36, 37, 37, 37, 37, 37, 37, 37, 37
};

static int gSizeClassToSize[ALLOCATOR_SIZE_CLASSES] =
{
	0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208,
	224, 240, 256, 272, 288, 312, 336, 368, 408, 448, 512
};

#define ALLOCATOR_PAGE_SHIFT			12
#define ALLOCATOR_PAGE_SIZE				(1<<ALLOCATOR_PAGE_SHIFT)
#define ALLOCATOR_VIRTUAL_ALLOC_SIZE	(512*1024)

#if _M_IX86
#define _InterlockedCompareExchangePointer(a, b, c) ((void *)(size_t)_InterlockedCompareExchange((long volatile *)a, (long)(size_t)b, (long)(size_t)c))
#define ALLOCATOR_LUT_SHIFT				27
#define ALLOCATOR_LUT_TOP_SIZE		(1<<(32-ALLOCATOR_LUT_SHIFT))
#define ALLOCATOR_LUT_TABLE_SIZE	(1<<(ALLOCATOR_LUT_SHIFT-ALLOCATOR_PAGE_SHIFT))
#define ALLOCATOR_LUT_MASK				(ALLOCATOR_LUT_TABLE_SIZE-1)
#endif

#if _M_AMD64
#define ALLOCATOR_LUT_SHIFT				30
#define ALLOCATOR_LUT_TOP_SIZE		(1<<(40-ALLOCATOR_LUT_SHIFT))
#define ALLOCATOR_LUT_TABLE_SIZE	(1<<(ALLOCATOR_LUT_SHIFT-ALLOCATOR_PAGE_SHIFT))
#define ALLOCATOR_LUT_MASK				(ALLOCATOR_LUT_TABLE_SIZE-1)
#define ALLOCATOR_LUT_LIMIT				(1ull<<40)
#endif

#define ILINE		__forceinline
#define NOINLINE	__declspec(noinline)

class CAllocator
{
public:
	void *Alloc(size_t size, size_t &allocated)
	{
		if (size > ALLOCATOR_SIZE_LIMIT)
		{
			allocated = size;
			return malloc(size);
		}
		else
		{
			int size_class = SizeToSizeClass(size);
			if (size_class == 0)
				size_class = 1;
			void * volatile *free_list = &FreeList[size_class];
			void *memory, *next;

			do
			{
				memory = *free_list;
				if (!memory)
				{
					if (!(memory = FillList(size_class)))
						return NULL;
				}
#if ALLOCATOR_SANITY_CHECKS
				assert(GetSizeClassFromAddress(memory) == size_class);
#endif
				next = *reinterpret_cast<void **>(memory);
#if ALLOCATOR_SANITY_CHECKS
				assert(!next || GetSizeClassFromAddress(next) == size_class);
#endif
			}
			while (_InterlockedCompareExchangePointer(free_list, next, memory) != memory);

			allocated = SizeClassToSize(size_class);
#if ALLOCATOR_FILL_ALLOC
			memset(memory, 0, allocated);
#endif
			return memory;
		}
	}
	size_t Free(void *memory)
	{
		int size_class = GetSizeClassFromAddress(memory);
		if (!size_class)
		{
			size_t size = _msize(memory);
			free(memory);
			return size;
		}
		else
		{
			void * volatile *free_list = &FreeList[size_class];
			void *next;

			do
			{
				next = *free_list;
#if ALLOCATOR_SANITY_CHECKS
				assert(!next || GetSizeClassFromAddress(next) == size_class);
#endif
				*reinterpret_cast<void **>(memory) = next;
			}
			while (_InterlockedCompareExchangePointer(free_list, memory, next) != next);
			return SizeClassToSize(size_class);
		}
	}
	void *Realloc(void *memory, size_t new_size, size_t &allocated)
	{
		if (!new_size)
		{
			Free(memory);
			return NULL;
		}
		else if (!memory)
		{
			return Alloc(new_size, allocated);
		}
		else 
		{
			size_t old_size = MSize(memory);
			void *new_memory = Alloc(new_size, allocated);
			if (!new_memory)
				return NULL;
			memcpy(new_memory, memory, old_size > new_size ? new_size : old_size);
			Free(memory);
			return new_memory;
		}
	}
	size_t MSize(void *memory)
	{
		int size_class = GetSizeClassFromAddress(memory);
		return size_class ? SizeClassToSize(size_class) : _msize(memory);
	}

private:
	void *FreeList[ALLOCATOR_SIZE_CLASSES];
	volatile long PageAllocatorLock;
	void *NextPage, *EndPages;
	char *PageLut[ALLOCATOR_LUT_TOP_SIZE];

	static ILINE int SizeToSizeClass(size_t size)
	{
		//return int((size + 7) >> 3);
		return gSizeToSizeClass[(size + 7) >> 3];
	}

	static ILINE int SizeClassToSize(int size_class)
	{
		//return size_class << 3;
		return gSizeClassToSize[size_class];
	}

	NOINLINE void *FillList(int size_class)
	{
		void *page, *memory, *next, *end;

		// Get a new page
		page = GetPage(size_class);
		if (!page)
			return NULL;
		
		// Fill the free list chain inside the page
		int size = SizeClassToSize(size_class);
		end = reinterpret_cast<char *>(page) + ALLOCATOR_PAGE_SIZE - size;
		for (memory = page; (next = reinterpret_cast<char *>(memory) + size) <= end; memory = next)
			*reinterpret_cast<void **>(memory) = next;

		// Hook it up
		void * volatile *free_list = &FreeList[size_class];
		do
		{
			next = *free_list;
			*reinterpret_cast<void **>(memory) = next;
		}
		while (_InterlockedCompareExchangePointer(free_list, page, next) != next);
		return page;
	}

	ILINE int GetSizeClassFromAddress(void *memory)
	{
#ifdef ALLOCATOR_LUT_LIMIT
		if ((size_t)memory >= ALLOCATOR_LUT_LIMIT)
			return 0;
#endif
		char *lut = PageLut[((size_t)memory) >> ALLOCATOR_LUT_SHIFT];
		return lut ? lut[(((size_t)memory) >> ALLOCATOR_PAGE_SHIFT) & ALLOCATOR_LUT_MASK] : 0;
	}

	class CSimpleAutoSpinLock
	{
	public:
		CSimpleAutoSpinLock(long volatile *lock)
		{
			Lock = lock;
			while (_InterlockedCompareExchange(Lock, 1, 0) != 0)
				Sleep(0);
		}
		~CSimpleAutoSpinLock()
		{
			*Lock = 0;
		}
	private:
		long volatile *Lock;
	};

	NOINLINE void *GetPage(int size_class)
	{
		CSimpleAutoSpinLock sl(&PageAllocatorLock);

		void *page;

		if (NextPage == EndPages)
		{
			page = VirtualAlloc(NULL, ALLOCATOR_VIRTUAL_ALLOC_SIZE, MEM_COMMIT, PAGE_READWRITE);
			if (!page)
				return NULL;
#ifdef ALLOCATOR_LUT_LIMIT
			if (((size_t)page) + ALLOCATOR_VIRTUAL_ALLOC_SIZE > ALLOCATOR_LUT_LIMIT)
				return NULL;
#endif
			NextPage = page;
			reinterpret_cast<char *&>(EndPages) = reinterpret_cast<char *>(NextPage) + ALLOCATOR_VIRTUAL_ALLOC_SIZE;
		}

		page = NextPage;
		reinterpret_cast<char *&>(NextPage) += ALLOCATOR_PAGE_SIZE;

		char *&lut = PageLut[((size_t)page) >> ALLOCATOR_LUT_SHIFT];
		if (!lut)
		{
			lut = (char *)VirtualAlloc(NULL, ALLOCATOR_LUT_TABLE_SIZE, MEM_COMMIT, PAGE_READWRITE);
			if (!lut)
				return NULL;
		}
		lut[(((size_t)page) >> ALLOCATOR_PAGE_SHIFT) & ALLOCATOR_LUT_MASK] = size_class;

		return page;
	}
};

CAllocator Allocator;

CRYMEMORYMANAGER_API void *CryMalloc(size_t size, size_t &allocated)
{
#if ALLOCATOR_DEBUG_MODE
	allocated = 0;
	size_t orig_size = size;
	size += 3 * sizeof(size_t);
	size_t *p = (size_t *)Allocator.Alloc(size, allocated);
	p[0] = orig_size;
	p[1] = size_t(0xdeadbeefcafebabeull);
	size_t *e = (size_t*)(((char *)p) + 2*sizeof(size_t) + orig_size);
	*e = size_t(0xdeadbeefcafebabeull);
#if ALLOCATOR_DATA_THRASHING
	for (int i=0; i<orig_size; i++)
	{
		char * pc = (char *)p;
		pc += 2*sizeof(size_t);
		pc += i;
		*pc = rand() % 256;
	}
#else
	memset(p+2, 0xCD, orig_size);
#endif
	return p+2;
#else
	if (g_bProfilerEnabled)
	{
		FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
		allocated = 0;
		return Allocator.Alloc(size, allocated);
	}
	else
	{
		allocated = 0;
		return Allocator.Alloc(size, allocated);
	}
#endif
}

CRYMEMORYMANAGER_API void *CryRealloc(void *memblock, size_t new_size, size_t &allocated)
{
#if ALLOCATOR_DEBUG_MODE || ALLOCATOR_DEBUG_REALLOC
	if (!new_size)
	{
		CryFree(memblock);
		return NULL;
	}
	else if (!memblock)
	{
		return CryMalloc(new_size, allocated);
	}
	else 
	{
		size_t old_size = CryGetMemSize(memblock, 0);
		void *new_memory = CryMalloc(new_size, allocated);
		if (!new_memory)
			return NULL;
		memcpy(new_memory, memblock, old_size > new_size ? new_size : old_size);
		CryFree(memblock);
		return new_memory;
	}
#else
	if (g_bProfilerEnabled)
	{
		FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
		allocated = 0;
		return Allocator.Realloc(memblock, new_size, allocated);
	}
	else
	{
		allocated = 0;
		return Allocator.Realloc(memblock, new_size, allocated);
	}
#endif
}


CRYMEMORYMANAGER_API size_t CryFree(void *memblock)
{
#if ALLOCATOR_DEBUG_MODE
	if (!memblock)
		return 0;
	size_t *p = ((size_t*)memblock)-2;
	assert(p[1] == size_t(0xdeadbeefcafebabeull));
	size_t *e = (size_t*)(((char *)p) + 2*sizeof(size_t) + p[0]);
	assert(*e == size_t(0xdeadbeefcafebabeull));
	size_t trample = p[0] + 3*sizeof(size_t);
#if ALLOCATOR_DATA_THRASHING
	for (int i=0; i<trample; i++)
	{
		char * pc = (char *)p;
		pc += i;
		*pc = rand() % 256;
	}
#else
	memset(p, 0xFE, trample);
#endif
	return Allocator.Free(p);
#else
	if (g_bProfilerEnabled)
	{
		FUNCTION_PROFILER_FAST(GetISystem(), PROFILE_SYSTEM, g_bProfilerEnabled);
		return Allocator.Free(memblock);
	}
	else
	{
		return Allocator.Free(memblock);
	}
#endif
}

CRYMEMORYMANAGER_API size_t CryGetMemSize(void *memblock, size_t size)
{
#if ALLOCATOR_DEBUG_MODE
	if (!memblock)
		return 0;
	size_t *p = ((size_t*)memblock)-2;
	return p[0];
#else
	return Allocator.MSize(memblock);
#endif
}

CRYMEMORYMANAGER_API int  CryStats(char *buf)
{
	// FIXME: Implement
	return 0;
}
	
CRYMEMORYMANAGER_API void CryFlushAll()
{
	// FIXME: Implement
}

CRYMEMORYMANAGER_API void CryCleanup()
{
	// FIXME: Implement
}

CRYMEMORYMANAGER_API int  CryGetUsedHeapSize()
{
	// FIXME: Implement
	return 0;
}

CRYMEMORYMANAGER_API int  CryGetWastedHeapSize()
{
	// FIXME: Implement
	return 0;
}

CRYMEMORYMANAGER_API int CryMemoryGetAllocatedSize()
{
	// FIXME: Implement
	return 0;
}

CRYMEMORYMANAGER_API void *CrySystemCrtMalloc(size_t size)
{
	return ::malloc(size);
}

CRYMEMORYMANAGER_API void CrySystemCrtFree(void *p)
{
	return ::free(p);
}
