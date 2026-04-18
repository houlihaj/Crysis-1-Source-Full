#ifndef __FIXEDSIZEARENA_H__
#define __FIXEDSIZEARENA_H__

#pragma once

template <int SZ, int N>
class CFixedSizeArena
{
public:
	CFixedSizeArena()
	{
		m_nFree = N;
		for (int i=0; i<m_nFree; i++)
		{
			m_vFree[i] = i;
		}
	}

	template <class T>
	ILINE T * Construct()
	{
		NET_ASSERT(sizeof(T) <= SZ);
		if (!m_nFree)
			return 0;
		int which = m_vFree[--m_nFree];
		void * p = m_members[which].data;
		return new (p) T();
	}

	template <class T, class A0>
	ILINE T * Construct(const A0& a0)
	{
		NET_ASSERT(sizeof(T) <= SZ);
		if (!m_nFree)
			return 0;
		int which = m_vFree[--m_nFree];
		void * p = m_members[which].data;
		return new (p) T(a0);
	}

	template <class T>
	ILINE void Dispose( T* p )
	{
		p->~T();
		NET_ASSERT(m_nFree < N);
		uint8* pp = (uint8*) p;
		int which;
		for (which = 0; which < N; ++which)
			if (pp == m_members[which].data)
				break;
		NET_ASSERT(which != N);
		if (which == N)
			return;
		m_vFree[m_nFree++] = which;
	}

private:
	union Member
	{
		void * dummy; // for alignment
		uint8 data[SZ];
	};
	int m_nFree;
	int m_vFree[N];
	Member m_members[N];
};

#endif
