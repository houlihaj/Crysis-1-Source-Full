#ifndef __BYTESTREAM_H__
#define __BYTESTREAM_H__

#pragma once

#include "MiniQueue.h"
#include "Typelist.h"

class CByteStreamPacker
{
public:
	CByteStreamPacker()
	{
		for (int i=0; i<3; i++)
			m_data[i] = 0;
	}

	uint32 GetNextOfs( uint32 sz );

	template <int N>
	ILINE uint32 GetNextOfs_Fixed( NTypelist::Int2Type<N> )
	{
		return GetNextOfs(N);
	}
	ILINE uint32 GetNextOfs_Fixed( NTypelist::Int2Type<0> )
	{
		return m_data[eD_Size];
	}
	ILINE uint32 GetNextOfs_Fixed( NTypelist::Int2Type<1> )
	{
/*
		CByteStreamPacker orig = *this;
		CByteStreamPacker temp = *this;
		if (temp.m_data[eD_Ofs1]%4 == 0)
		{
			temp.m_data[eD_Ofs1] = temp.m_data[eD_Size];
			temp.m_data[eD_Size] += 4;
		}
		uint32 outShouldBe = temp.m_data[eD_Ofs1];
		temp.m_data[eD_Ofs1] += 1;
*/

		static const uint32 SEL = 0x0e; // binary 1110
		uint32 selector = (SEL >> (m_data[eD_Ofs1] & 3)) & 1;
		uint32 out = m_data[eD_Ofs1] = m_data[selector];
		m_data[eD_Ofs1] ++;
		m_data[eD_Size] += 4-4*selector;

/*
		NET_ASSERT(out == outShouldBe);
		for (int i=0; i<3; i++)
			NET_ASSERT(m_data[i] == temp.m_data[i]);
*/

		return out;
	}
	ILINE uint32 GetNextOfs_Fixed( NTypelist::Int2Type<2> )
	{
/*
		CByteStreamPacker orig = *this;
		CByteStreamPacker temp = *this;
		if (temp.m_data[eD_Ofs2]%4 == 0)
		{
			temp.m_data[eD_Ofs2] = temp.m_data[eD_Size];
			temp.m_data[eD_Size] += 4;
		}
		uint32 outShouldBe = temp.m_data[eD_Ofs2];
		temp.m_data[eD_Ofs2] += 2;
*/

		static const uint32 SEL = 0x1c; // binary 1110<<1
		uint32 selector = (SEL >> (m_data[eD_Ofs2] & 3)) & 2;
		uint32 out = m_data[eD_Ofs2] = m_data[selector];
		m_data[eD_Ofs2] += 2;
		m_data[eD_Size] += 4-2*selector;

/*
		NET_ASSERT(out == outShouldBe);
		for (int i=0; i<3; i++)
			NET_ASSERT(m_data[i] == temp.m_data[i]);
*/

		return out;
	}
	ILINE uint32 GetNextOfs_Fixed( NTypelist::Int2Type<4> )
	{
		uint32 ofs = m_data[eD_Size];
		m_data[eD_Size] += 4;
		return ofs;
	}
	ILINE uint32 GetNextOfs_Fixed( NTypelist::Int2Type<8> )
	{
		uint32 ofs = m_data[eD_Size];
		m_data[eD_Size] += 8;
		return ofs;
	}
	ILINE uint32 GetNextOfs_Fixed( NTypelist::Int2Type<12> )
	{
		uint32 ofs = m_data[eD_Size];
		m_data[eD_Size] += 12;
		return ofs;
	}
	ILINE uint32 GetNextOfs_Fixed( NTypelist::Int2Type<16> )
	{
		uint32 ofs = m_data[eD_Size];
		m_data[eD_Size] += 16;
		return ofs;
	}

protected:
	enum EData
	{
		eD_Size = 0,
		eD_Ofs1,
		eD_Ofs2,
	};
	uint32 m_data[3];
	/*
	uint32 m_size;
	uint32 m_ofs1;
	uint32 m_ofs2;
	*/
};

class CByteOutputStream : private CByteStreamPacker
{
public:
	CByteOutputStream( IStreamAllocator * pSA, size_t initSize = 16, void * caller = 0 ) : m_pSA(pSA), m_capacity(initSize), m_buffer((uint8*)pSA->Alloc(initSize, caller? caller : UP_STACK_PTR)) 
	{
		memset(m_buffer, 0, initSize);
	}

	void Put( const void * pWhat, size_t sz )
	{
		uint32 where = GetNextOfs(sz);
		if (m_data[eD_Size] > m_capacity)
			Grow(m_data[eD_Size]);
		memcpy( m_buffer+where, pWhat, sz );
	}

	template <class T>
	ILINE T& PutTyped()
	{
		uint32 where = GetNextOfs_Fixed(NTypelist::Int2Type<sizeof(T)>());
		if (m_data[eD_Size] > m_capacity)
			Grow(m_data[eD_Size]);
		return *reinterpret_cast<T*>(m_buffer+where);
	}

	ILINE void PutByte( uint8 c )
	{
		PutTyped<uint8>() = c;
	}

	ILINE size_t GetSize()
	{
		return m_data[eD_Size];
	}

	const uint8* GetBuffer() const
	{
		return m_buffer;
	}

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CByteOutputStream");

		pSizer->Add(*this);
		if (m_pSA)
			pSizer->Add(m_buffer, m_capacity);
	}

private:
	void Grow( size_t sz );

	IStreamAllocator * m_pSA;
	uint32 m_capacity;
	uint8 * m_buffer;
};

class CByteInputStream : private CByteStreamPacker
{
public:
	CByteInputStream( const uint8* pData, size_t sz ) : m_pData(pData), m_capacity(sz) 
	{
	}

	~CByteInputStream()
	{
	}

	ILINE void Get( void * pWhere, size_t sz )
	{
		NET_ASSERT(sz <= m_data[eD_Size]);
		memcpy(pWhere, Get(sz), sz);
	}

	ILINE const void * Get( size_t sz )
	{
		uint32 where = GetNextOfs(sz);
		NET_ASSERT((where + sz) <= m_capacity);
		return m_pData + where;
	}

	template <class T>
	ILINE const T& GetTyped()
	{
		uint32 where = GetNextOfs_Fixed(NTypelist::Int2Type<sizeof(T)>());
		NET_ASSERT((where + sizeof(T)) <= m_capacity);
		return *static_cast<const T*>(static_cast<const void*>(m_pData + where));
	}

	ILINE uint8 GetByte()
	{
		return GetTyped<uint8>();
	}

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CByteInputStream");

		pSizer->Add(*this);
		pSizer->Add(m_pData, m_capacity);
	}

	size_t GetSize() const
	{
		return m_data[eD_Size];
	}

private:
	const uint8 * m_pData;
	size_t m_capacity;
};

#endif
