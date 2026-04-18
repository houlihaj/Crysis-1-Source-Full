/*=============================================================================
FreeSpaceSpanBuffer.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef __FREESPACESPANBUFFER_H__
#define __FREESPACESPANBUFFER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "OneBitBitmapMask.h"

class CSpanBuffer;

//FreeSpace Span - Buffer
class CFreeSpaceSpanBuffer
{
public:
	typedef std::pair<uint32,uint32>	t_pairSpan;
	typedef std::list<t_pairSpan>		t_lstSpans;
	typedef t_lstSpans::iterator		t_lstSpansIt;

public:
	//===================================================================================
	// Constructor
	//===================================================================================
	CFreeSpaceSpanBuffer() : m_nWidth(0), m_nHeight(0), m_BiggestFreeSpan(NULL), m_nBiggestFreeSpanAtAll(0), m_nChannelNumber(0)
	{
		//create the free texel buffer
		m_FreeTexels.assign(0, t_lstSpans(0));
	}

	//===================================================================================
	// Constructor
	//===================================================================================
	CFreeSpaceSpanBuffer( const int32 nWidth, const int32 nHeight, const int32 nChannelNumber ) : m_nWidth(nWidth) , m_nHeight(nHeight), m_nChannelNumber( nChannelNumber )
	{
		assert( nChannelNumber > 0 );
		assert( nWidth > 0 );
		assert( nHeight > 0 );

		if( nWidth < 1 || nHeight < 1 )
		{
			m_nWidth = 0;
			m_nHeight = 0;
			m_nChannelNumber = 0;
			m_FreeTexels.assign(0, t_lstSpans(0));
			m_BiggestFreeSpan = NULL;
			m_nBiggestFreeSpanAtAll = 0;
			return;
		}

		//create the free texel buffer
		m_FreeTexels.assign( nHeight*m_nChannelNumber, t_lstSpans( 0 ) );
		for( int32 i = 0; i < nHeight*m_nChannelNumber; ++i )
			m_FreeTexels[i].push_back( t_pairSpan(0,nWidth) );

		m_BiggestFreeSpan = new int32[ nHeight*m_nChannelNumber ];
		for( int32 i = 0; i < nHeight*m_nChannelNumber; ++i )
			m_BiggestFreeSpan[i] = nWidth;
		m_nBiggestFreeSpanAtAll = nWidth;
	}

	~CFreeSpaceSpanBuffer()
	{
		SAFE_DELETE_ARRAY( m_BiggestFreeSpan );
	}

	void	Copy( CFreeSpaceSpanBuffer* pBuffer);																												/// Copy an other buffer
	void	Clear();																																										/// Restart the buffers
	void	BeginAllocations();																																					/// Create a backup of the allocations
	void	RevertAllocations();																																				/// Revert the allocation to backup position (Need to use BeginAllocations() before!)
	bool	IsInsertable_EarlyCheck( const CSpanBuffer &SpanBuffer ) const;															/// Very early check, if the SpanBuffer has bigger span than the biggest free space it will return false
	int32	WastedSpace( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY );					/// calculate the not occupied free spaces... try to minimalize (fragmentation)
	int32	IsInsertable( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY, const int8 nFirstChannel, const int8 nChannelNumber );				/// Return values: 0 == Insertable to this position. <> 0 means this is a "nesesary" distance...
	bool	Insert( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY, const int8 nFirstChannel, const int8 nChannelNumber  );							/// Insert a span (covered space) into free space (Every time use the IsInsertable before use it!)
	int32	GetBiggestFreeSpan() const { return m_nBiggestFreeSpanAtAll; }															/// Give back the size of the biggest free space span.
	void	GenerateDebugJPG();																																					/// Generate a debug JPG for help the visualisation of the buffer
	void	RegenerateBiggestSpanList();																																/// Do precalculation for early checks

private:
	int32	WastedSpaceSpan( const int32 nY, const int32 nXStart, const int32 nXEnd, int32 nLastXEnd );	/// Calculate the wasted space / span (calculate only the left side)
	int32	IsInsertableSpan( const int32 nY, const int32 nXStart, const int32 nXEnd );									/// Return values: 0 == Insertable to this position. <> 0 means this is an "at least" distance...
	void	InsertSpan( const int32 nY, const int32 nXStart, const int32 nXEnd );												/// Insert a span (covered space) into free space (Every time use the IsInsertable before use it!)

public:
	std::vector<t_lstSpans>	m_FreeTexels;								//!<  the vertical buffer of spans....

protected:
	int32			m_nWidth;																	//!< Width
	int32			m_nHeight;																//!< Height
	int32			m_nBiggestFreeSpanAtAll;									//!< helper for fast rejection
	int32*		m_BiggestFreeSpan;												//!< the biggest span / scanline
	int32			m_nChannelNumber;													//!< number of used channels

	std::vector<t_lstSpans>	m_FreeTexelsBackup;					//!< the backup buffer.. because of groupping...
};


//FreeSpace - Buffer
class CFreeSpaceBuffer
{
public:
	//===================================================================================
	// Constructor
	//===================================================================================
	CFreeSpaceBuffer() : m_nWidth(0), m_nHeight(0), m_pBitMask(NULL), m_pBitMask_Backup(NULL) , m_BiggestFreeSpan(NULL), m_nBiggestFreeSpanAtAll(0), m_nChannelNumber(0)
	{
	}

	//===================================================================================
	// Constructor
	//===================================================================================
	CFreeSpaceBuffer( const int32 nWidth, const int32 nHeight, const int32 nChannelNumber ) : m_nWidth(nWidth) , m_nHeight(nHeight), m_nChannelNumber( nChannelNumber )
	{
		assert( nWidth > 0 );
		assert( nHeight > 0 );

		if( nWidth < 1 || nHeight < 1 )
		{
			m_nWidth = 0;
			m_nHeight = 0;
			m_nChannelNumber = 0;
			m_BiggestFreeSpan = NULL;
			m_nBiggestFreeSpanAtAll = 0;
			m_pBitMask = NULL;
			m_pBitMask_Backup = NULL;
			return;
		}

		//create the free texel buffer
		m_pBitMask = new COneBitBitmapMask( nWidth, nHeight*m_nChannelNumber );
		m_pBitMask_Backup = new COneBitBitmapMask( nWidth, nHeight*m_nChannelNumber );

		m_BiggestFreeSpan = new int32[ nHeight*m_nChannelNumber ];
		for( int32 i = 0; i < nHeight*m_nChannelNumber; ++i )
							m_BiggestFreeSpan[i] = nWidth;

		m_nBiggestFreeSpanAtAll = nWidth;
	}

	~CFreeSpaceBuffer()
	{
		SAFE_DELETE_ARRAY( m_BiggestFreeSpan );
		SAFE_DELETE_ARRAY( m_pBitMask );
		SAFE_DELETE_ARRAY( m_pBitMask_Backup );
	}

	void	Clear();																																										/// Restart the buffers
	void	BeginAllocations();																																					/// Create a backup of the allocations
	void	RevertAllocations();																																				/// Revert the allocation to backup position (Need to use BeginAllocations() before!)
	bool	IsInsertable_EarlyCheck( const CSpanBuffer &SpanBuffer ) const;															/// Very early check, if the SpanBuffer has bigger span than the biggest free space it will return false
	int32	IsInsertable( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY, const int8 nFirstChannel, const int8 nChannelNumber );				/// Return values: 0 == Insertable to this position. <> 0 means this is a "nesesary" distance...
	bool	Insert( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY, const int8 nFirstChannel, const int8 nChannelNumber );							/// Insert a span (covered space) into free space (Every time use the IsInsertable before use it!)
	int32	GetBiggestFreeSpan() const { return m_nBiggestFreeSpanAtAll; }															/// Give back the size of the biggest free space span.
	void	GenerateDebugJPG();																																					/// Generate a debug JPG for help the visualisation of the buffer
	void	RegenerateBiggestSpanList();																																/// Do precalculation for early checks

public:
	COneBitBitmapMask	*m_pBitMask;
	COneBitBitmapMask	*m_pBitMask_Backup;

protected:
	int32			m_nWidth;																	//!< Width
	int32			m_nHeight;																//!< Height
	int32			m_nBiggestFreeSpanAtAll;									//!< helper for fast rejection
	int32*		m_BiggestFreeSpan;												//!< the biggest span / scanline
	int32			m_nChannelNumber;													//!< number of used channels
};

#endif