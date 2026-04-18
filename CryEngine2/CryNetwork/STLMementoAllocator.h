// stl allocator wrapped around CMementoMemoryManager, and using the region system

#ifndef __STLMEMENTOALLOCATOR_H__
#define __STLMEMENTOALLOCATOR_H__

#pragma once

#include "Network.h"

template <class T> class STLMementoAllocator
{
public:
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;
	template <class U> struct rebind
	{
		typedef STLMementoAllocator<U> other;
	};

	STLMementoAllocator() throw() : m_pMMM(&MMM()) 
	{
	}

	STLMementoAllocator( const CMementoMemoryManagerPtr& pMMM ) throw() : m_pMMM(pMMM)
	{
	}

	STLMementoAllocator(const STLMementoAllocator& other) throw() : m_pMMM(other.m_pMMM)
	{
	}

	template <class U> STLMementoAllocator(const STLMementoAllocator<U>& other) throw() : m_pMMM(other.m_pMMM)
	{
	}

	~STLMementoAllocator() throw()
	{
	}

	pointer address(reference x) const
	{
		return &x;
	}

	const_pointer address(const_reference x) const
	{
		return &x;
	}

	pointer allocate(size_type n, const void* hint = 0)
	{
		return static_cast<T*>(m_pMMM->AllocPtr(n*sizeof(T)));
	}

	void deallocate(pointer p, size_type n)
	{
		m_pMMM->FreePtr(p, n*sizeof(T));
	}

	size_type max_size() const throw()
	{
		return INT_MAX;
	}

	void construct(pointer p, const T& val)
	{
		new(static_cast<void*>(p)) T(val);
	}

	void construct(pointer p)
	{
		new(static_cast<void*>(p)) T();
	}

	void destroy(pointer p)
	{
		p->~T();
	}

	bool operator==(const STLMementoAllocator& other) {return m_pMMM == other.m_pMMM;}

	CMementoMemoryManagerPtr m_pMMM;
};

template <> class STLMementoAllocator<void>
{
public:
	typedef void* pointer;
	typedef const void* const_pointer;
	typedef void value_type;
	template <class U> 
	struct rebind { typedef STLMementoAllocator<U> other; };
};    

#endif
