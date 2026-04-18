#ifndef __POLYMORPHICQUEUE_H__
#define __POLYMORPHICQUEUE_H__

#pragma once

#include "Config.h"

class CEnsureRealtime
{
public:
	CEnsureRealtime() : m_begin(gEnv->pTimer->GetAsyncTime()) {}
	~CEnsureRealtime()
	{
		if ((gEnv->pTimer->GetAsyncTime() - m_begin).GetSeconds() > 1.0f)
			Failed();
	}

private:
	CTimeValue m_begin;

	void Failed();
};

#define ENSURE_REALTIME /*CEnsureRealtime ensureRealtime*/

#if CHECKING_POLYMORPHIC_QUEUE
# define POLY_HEADER DWORD oldUser = m_user; m_user = GetCurrentThreadId(); if (oldUser) NET_ASSERT(oldUser == m_user)
# define POLY_FOOTER NET_ASSERT(m_user == GetCurrentThreadId()); m_user = oldUser
# ifdef _MSC_VER
extern "C" void * _ReturnAddress(void);
#  pragma intrinsic(_ReturnAddress)
#  define POLY_RETADDR _ReturnAddress()
# else
#  define POLY_RETADDR (void*)0
# endif
#else
# define POLY_HEADER
# define POLY_FOOTER
# define POLY_RETADDR (void*)0
#endif

template <class B>
class CPolymorphicQueue
{
public:
	CPolymorphicQueue()
	{
		m_pCurBlock = 0;
#if CHECKING_POLYMORPHIC_QUEUE
		m_user = 0;
#endif
	}
	~CPolymorphicQueue()
	{
		DoNothing f;
		Flush(f);
		NET_ASSERT( m_blocks.empty() );
		NET_ASSERT( m_flushBlocks.empty() );
		while (!m_freeBlocks.empty())
		{
			delete m_freeBlocks.back();
			m_freeBlocks.pop_back();
		}
	}

	template <class T>
	T * Add( const T& value )
	{
		POLY_HEADER;
		T * out = Check(new (Grab(sizeof(T), POLY_RETADDR)) T(value));
		POLY_FOOTER;
		return out;
	}
	template <class T>
	T * Add()
	{
		POLY_HEADER;
		T * out = Check(new (Grab(sizeof(T), POLY_RETADDR)) T());
		POLY_FOOTER;
		return out;
	}

	template <class F>
	void Flush( F& f )
	{
		POLY_HEADER;
		while (BeginFlush()) 
		{
			B * p = 0;
			while (p = Next(p))
			{
				f(p);
			}
			EndFlush();
		}
		POLY_FOOTER;
	}

	template <class F>
	void RealtimeFlush( F& f )
	{
		POLY_HEADER;
		while (BeginFlush()) 
		{
			B * p = 0;
			while (p = Next(p))
			{
				ENSURE_REALTIME;
				f(p);
			}
			EndFlush();
		}
		POLY_FOOTER;
	}

	void Swap( CPolymorphicQueue& pq )
	{
		POLY_HEADER;
		m_freeBlocks.swap(pq.m_freeBlocks);
		m_blocks.swap(pq.m_blocks);
		m_flushBlocks.swap(pq.m_flushBlocks);
		std::swap(m_pCurBlock, pq.m_pCurBlock);
		std::swap(m_flushBlock, pq.m_flushBlock);
		std::swap(m_flushCursor, pq.m_flushCursor);
		POLY_FOOTER;
	}

	bool Empty()
	{
		if (m_pCurBlock)
			if (m_pCurBlock->cursor)
				return false;
		return m_blocks.empty();
	}

	void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false)
	{
		SIZER_COMPONENT_NAME(pSizer, "CPolymorphicQueue");

		if (countingThis)
			pSizer->Add(*this);

		pSizer->AddContainer(m_freeBlocks);
		pSizer->AddContainer(m_blocks);
		pSizer->AddContainer(m_flushBlocks);

		pSizer->AddObject(&m_freeBlocks, m_freeBlocks.size()*sizeof(SBlock));
		pSizer->AddObject(&m_blocks, m_blocks.size()*sizeof(SBlock));
		pSizer->AddObject(&m_flushBlocks, m_flushBlocks.size()*sizeof(SBlock));
	}

private:
#if CHECKING_POLYMORPHIC_QUEUE
	DWORD m_user;
#endif

	template <class T>
	ILINE T * Check1( T * p, B * pp )
	{
		return p;
	}
	template <class T>
	ILINE T * Check( T * p )
	{
		return Check1(p,p);
	}

	typedef INT_PTR TWord;
	static const size_t WORD_SIZE = sizeof(TWord);
	struct SBlock
	{
		static const size_t BLOCK_SIZE = 16*1024; //64*1024;
		static const size_t WORDS = BLOCK_SIZE/WORD_SIZE;
		TWord cursor;
		TWord data[WORDS-1];

		SBlock()
		{
			++g_objcnt.queueBlocks;
		}
		~SBlock()
		{
			--g_objcnt.queueBlocks;
		}
		SBlock(const SBlock&);
		SBlock& operator=(const SBlock&);
	};
	std::vector<SBlock*> m_freeBlocks;
	std::vector<SBlock*> m_blocks;
	std::vector<SBlock*> m_flushBlocks;
	SBlock * m_pCurBlock;
	size_t m_flushBlock;
	TWord m_flushCursor;
#if CHECKING_POLYMORPHIC_QUEUE
	void * m_lastAdder;
#endif


	struct DoNothing
	{
		ILINE void operator()( B * p ) {}
	};

	void * Grab(size_t sz, void * retaddr)
	{
		size_t initsz = sz;

		POLY_HEADER;
		if (sz & (WORD_SIZE-1))
			sz += WORD_SIZE;
		sz /= WORD_SIZE;
		NET_ASSERT(sz > 0);
		NET_ASSERT(sz < SBlock::BLOCK_SIZE - sizeof(TWord));
		if (m_pCurBlock && m_pCurBlock->cursor + sz + 1 + CHECKING_POLYMORPHIC_QUEUE > SBlock::WORDS-1)
		{
			m_blocks.push_back(m_pCurBlock);
			m_pCurBlock = 0;
		}
		if (!m_pCurBlock)
		{
			if (m_freeBlocks.empty())
			{
				m_pCurBlock = new SBlock;
				NET_ASSERT(m_pCurBlock);
			}
			else
			{
				m_pCurBlock = m_freeBlocks.back();
				m_freeBlocks.pop_back();
				NET_ASSERT(m_pCurBlock);
			}
			m_pCurBlock->cursor = 0;
		}
		NET_ASSERT(m_pCurBlock);
		NET_ASSERT(m_pCurBlock->cursor + sz + 1 + CHECKING_POLYMORPHIC_QUEUE <= SBlock::WORDS-1);
		NET_ASSERT(m_pCurBlock->cursor >= 0);
		m_pCurBlock->data[m_pCurBlock->cursor++] = sz;
#if CHECKING_POLYMORPHIC_QUEUE
		m_pCurBlock->data[m_pCurBlock->cursor++] = (TWord) retaddr;
#endif
		TWord * out = m_pCurBlock->data + m_pCurBlock->cursor;
		NET_ASSERT(out >= m_pCurBlock->data);
		NET_ASSERT(out < m_pCurBlock->data + SBlock::WORDS);
		m_pCurBlock->cursor += sz;
		NET_ASSERT(m_pCurBlock->cursor <= SBlock::WORDS-1);
		NET_ASSERT(m_pCurBlock->cursor > 0);
		POLY_FOOTER;
		return out;
	}

	bool BeginFlush()
	{
		POLY_HEADER;
		if (m_pCurBlock)
		{
			m_blocks.push_back(m_pCurBlock);
			m_pCurBlock = 0;
		}
		if (m_blocks.empty())
		{
			POLY_FOOTER;
			return false;
		}
		NET_ASSERT(m_flushBlocks.empty());
		m_flushBlocks.swap(m_blocks);
		m_flushBlock = 0;
		m_flushCursor = 0;
		POLY_FOOTER;
		return true;
	}
	B * Next(B * last)
	{
		POLY_HEADER;
		if (last)
			last->~B();
		if (m_flushBlock == m_flushBlocks.size())
		{
			POLY_FOOTER;
			return 0;
		}
		while (m_flushCursor == m_flushBlocks[m_flushBlock]->cursor)
		{
			m_flushBlock++;
			m_flushCursor = 0;
			if (m_flushBlock == m_flushBlocks.size())
			{
				POLY_FOOTER;
				return 0;
			}
		}
		size_t sz = m_flushBlocks[m_flushBlock]->data[m_flushCursor++];
#if CHECKING_POLYMORPHIC_QUEUE
		m_lastAdder = (void*) m_flushBlocks[m_flushBlock]->data[m_flushCursor++];
#endif
		void * out = &(m_flushBlocks[m_flushBlock]->data[m_flushCursor]);
		m_flushCursor += sz;
		POLY_FOOTER;
		return (B*)out;
	}
	void EndFlush()
	{
		POLY_HEADER;
		NET_ASSERT(m_flushBlock == m_flushBlocks.size());
		while (!m_flushBlocks.empty())
		{
			m_freeBlocks.push_back(m_flushBlocks.back());
			m_flushBlocks.pop_back();
		}
		POLY_FOOTER;
	}

};

#endif
