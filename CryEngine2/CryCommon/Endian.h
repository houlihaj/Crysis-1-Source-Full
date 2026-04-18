////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   Endian.h
//  Version:     v1.00
//  Created:     16/2/2006 by Scott,Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:		 19/3/2007: Separated Endian support from basic TypeInfo declarations.
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Endian_h__
#define __Endian_h__
#pragma once

// Native endian format.
enum EEndian
{
	eLittleEndian = 0, // PC
	eBigEndian = 1, // Consoles
};

/////////////////////////////////////////////////////////////////////////////////////
// NEED_ENDIAN_SWAP is an older define still used in several places to toggle endian swapping.
// It is only used when reading files which are assumed to be little-endian.
// For legacy support, define it to swap on big endian platforms.
/////////////////////////////////////////////////////////////////////////////////////

#if defined(PS3) || defined(XENON)
#define GetPlatformEndian() eBigEndian
#define NEED_ENDIAN_SWAP
#else
#define GetPlatformEndian() eLittleEndian
#undef NEED_ENDIAN_SWAP
#endif

//////////////////////////////////////////////////////////////////////////
// Endian support
//////////////////////////////////////////////////////////////////////////

void SwapEndian(const CTypeInfo& Info, size_t nSizeCheck, void* pData, size_t nCount = 1, bool bWriting = false);

// Default template utilises TypeInfo.
template<class T>
ILINE void SwapEndianBase(T* t, size_t nCount = 1, bool bWriting = false)
{
	SwapEndian(TypeInfo(t), sizeof(T), t, nCount, bWriting);
}

/////////////////////////////////////////////////////////////////////////////////////
// SwapEndianBase functions.
// Always swap the data (the functions named SwapEndian swap based on an optional bSwapEndian parameter).
// The bWriting parameter must be specified in general when the output is for writing, 
// but it matters only for types with bitfields.

// Overrides for base types.

template<>
ILINE void SwapEndianBase(char* p, size_t nCount, bool bWriting)
	{}
template<>
ILINE void SwapEndianBase(uint8* p, size_t nCount, bool bWriting)
	{}
template<>
ILINE void SwapEndianBase(int8* p, size_t nCount, bool bWriting)
	{}

template<>
ILINE void SwapEndianBase(uint16* p, size_t nCount, bool bWriting)
{
	for (; nCount-- > 0; p++)
		*p = ((*p>>8) + (*p<<8) & 0xFFFF);
}

template<>
ILINE void SwapEndianBase(int16* p, size_t nCount, bool bWriting)
	{ SwapEndianBase((uint16*)p, nCount); }

template<>
ILINE void SwapEndianBase(uint32* p, size_t nCount, bool bWriting)
{ 
	for (; nCount-- > 0; p++)
		*p = (*p>>24) + ((*p>>8)&0xFF00) + ((*p&0xFF00)<<8) + (*p<<24);
}
template<>
ILINE void SwapEndianBase(int32* p, size_t nCount, bool bWriting)
	{ SwapEndianBase((uint32*)p, nCount); }
template<>
ILINE void SwapEndianBase(float* p, size_t nCount, bool bWriting)
	{ SwapEndianBase((uint32*)p, nCount); }

template<>
ILINE void SwapEndianBase(uint64* p, size_t nCount, bool bWriting)
{
	for (; nCount-- > 0; p++)
		*p = (*p>>56) + ((*p>>40)&0xFF00) + ((*p>>24)&0xFF0000) + ((*p>>8)&0xFF000000)
			 + ((*p&0xFF000000)<<8) + ((*p&0xFF0000)<<24) + ((*p&0xFF00)<<40) + (*p<<56);
}
template<>
ILINE void SwapEndianBase(int64* p, size_t nCount, bool bWriting)
	{ SwapEndianBase((uint64*)p, nCount); }
template<>
ILINE void SwapEndianBase(double* p, size_t nCount, bool bWriting)
	{ SwapEndianBase((uint64*)p, nCount); }

//---------------------------------------------------------------------------
// SwapEndian functions.
// bSwapEndian argument optional, and defaults to swapping from LittleEndian format.

template<class T>
ILINE void SwapEndian(T* t, size_t nCount, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(t, nCount);
}

// Specify int and uint as well as size_t, to resolve overload ambiguities.
template<class T>
ILINE void SwapEndian(T* t, int nCount, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(t, nCount);
}

#ifdef WIN64
template<class T>
ILINE void SwapEndian(T* t, unsigned int nCount, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(t, nCount);
}
#endif

template<class T>
ILINE void SwapEndian(T& t, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(&t, 1);
}

template<class T>
ILINE T SwapEndianValue(T t, bool bSwapEndian = eLittleEndian)
{
	if (bSwapEndian)
		SwapEndianBase(&t, 1);
	return t;
}

// Object-oriented data extraction for endian-swapping reading.
template<class T>
inline T* StepData(unsigned char*& pData, int* pnDataSize = NULL, int nCount = 1, bool bSwap = true)
{
	T* Elems = (T*)pData;
	if (bSwap)
		SwapEndian(Elems, nCount);
	pData += sizeof(T)*nCount;
	if (pnDataSize)
		*pnDataSize -= sizeof(T)*nCount;
	return Elems;
}

template<class T>
inline void StepData(T*& Result, unsigned char*& pData, int* pnDataSize = NULL, int nCount = 1, bool bSwap = true)
{
	Result = StepData<T>(pData, pnDataSize, nCount, bSwap);
}

template<class T>
inline void StepDataCopy(T* Dest, unsigned char*& pData, int* pnDataSize = NULL, int nCount = 1, bool bSwap = true)
{
	memcpy(Dest, StepData<T>(pData, pnDataSize, nCount, bSwap), nCount*sizeof(T));
}

#endif // __Endian_h__
