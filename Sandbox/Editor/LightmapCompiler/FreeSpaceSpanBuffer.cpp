/*=============================================================================
FreeSpaceSpanBuffer.cpp : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/

#include "stdafx.h"
#include "SpanBuffer.h"
#include "FreeSpaceSpanBuffer.h"

//===================================================================================
// Method				:	Clear
// Description	:	Clear the span buffer
//===================================================================================
void CFreeSpaceSpanBuffer::Clear()
{
	for( int32 i = 0; i < m_nHeight; ++i )
	{
		m_FreeTexels[i].resize(0);
		m_FreeTexels[i].push_back( t_pairSpan(0,m_nWidth) );
	}
	for( int32 i = 0; i < m_nHeight*m_nChannelNumber; ++i )
		m_BiggestFreeSpan[i] = m_nWidth;
	m_nBiggestFreeSpanAtAll = m_nWidth;
}

//===================================================================================
// Method				:	Copy
// Description	:	Copy an other buffer
//===================================================================================
void CFreeSpaceSpanBuffer::Copy( CFreeSpaceSpanBuffer* pBuffer )
{
	for( int32 i = 0; i < m_nHeight; ++i )
	{
		m_FreeTexels[i].resize(0);

		t_lstSpans *pList = &(pBuffer->m_FreeTexels[i]);
		t_lstSpansIt pListEnd = pList->end();
		for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			m_FreeTexels[i].push_back( *Span );
		}
	}

	//update the list
	RegenerateBiggestSpanList();
}


//===================================================================================
// Method				:	BeginAllocations
// Description	:	Create a backup of the allocations
//===================================================================================
void CFreeSpaceSpanBuffer::BeginAllocations()
{
	m_FreeTexelsBackup.assign(m_FreeTexels.begin(), m_FreeTexels.end());
}



//===================================================================================
// Method				:	RevertAllocations
// Description	:	Revert the allocation to backup position (Need to use BeginAllocations() before!)
//===================================================================================
void CFreeSpaceSpanBuffer::RevertAllocations()
{
	m_FreeTexels.assign(m_FreeTexelsBackup.begin(), m_FreeTexelsBackup.end());
}


//===================================================================================
// Method				:	IsInsertable_EarlyCheck
// Description	:	Very early check, if the SpanBuffer has bigger span than the biggest free space it will return false
//===================================================================================
bool CFreeSpaceSpanBuffer::IsInsertable_EarlyCheck( const CSpanBuffer &SpanBuffer ) const
{
	assert( SpanBuffer.m_nHeight < m_nHeight );

	if( SpanBuffer.m_nHeight > m_nHeight )
		return false;

	//no chance to insert this...
	if( SpanBuffer.m_nBiggestSpanAtAll > m_nBiggestFreeSpanAtAll )
		return false;

	//it's not mean it is insertable, just an early check for not insertable spanbuffers...
	return true;
}


//===================================================================================
// Method				:	WastedSpace
// Description	:	calculate the not occupied free spaces... try to minimalize (fragmentation)
//===================================================================================
int32 CFreeSpaceSpanBuffer::WastedSpace( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY )
{
	int32 nWasted = 0;
	int32 nHeight = SpanBuffer.m_nHeight;
	for( int32 y = 0; y < nHeight; ++y )
	{
		int32 nLastSpanEnd = 0;

		//invert the freespace -> covered space...
		t_lstSpans *pList = &(SpanBuffer.m_Texels[y]);
		t_lstSpansIt pListEnd = pList->end();
		for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			nWasted += WastedSpaceSpan( nOffsetY+y, nOffsetX+Span->first, nOffsetX+Span->second, nLastSpanEnd );
			nLastSpanEnd = nOffsetX+Span->second;
		}
	}

	return nWasted;
}


//===================================================================================
// Method				:	IsInsertable
// Description	:	Return values: 0 == Insertable to this position. <> 0 means this is a "nesesary" distance...
//===================================================================================
int32 CFreeSpaceSpanBuffer::IsInsertable( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY, const int8 nFirstChannel, const int8 nChannelNumber )
{
	//no chance to insert this...
	if( SpanBuffer.m_nBiggestSpanAtAll > m_nBiggestFreeSpanAtAll )
		return m_nWidth;

	//can't insert...
	int32 nHeight = SpanBuffer.m_nHeight;
	if( nOffsetY+nHeight >= m_nHeight || nOffsetX+SpanBuffer.m_nWidth >= m_nWidth )
		return m_nWidth;

	int32 nMaxDifference = 0;

	//per scanline fast check...
	int32 y;
	for( y = 0; y < nHeight; ++y )
		for( int32 c = 0; c < nChannelNumber; ++c )
			if( SpanBuffer.m_BiggestSpan[y] > m_BiggestFreeSpan[(nOffsetY+y)*m_nChannelNumber+nFirstChannel+c] )
				return m_nWidth;

	//for fast rejection: test the biggest span - can i insert it ?
	int32 nBigY = (nOffsetY + SpanBuffer.m_nBiggestSpanY)*m_nChannelNumber+nFirstChannel;
	int32 nBigSX = nOffsetX + SpanBuffer.m_nBiggestSpanX;
	int32 nBigEX = nBigSX + SpanBuffer.m_nBiggestSpanSize;

	int32 nBigDifference = 0;
	for( int32 c = 0; c < nChannelNumber; ++c )
	{
		int32 nDifference = IsInsertableSpan( nBigY+c, nBigSX, nBigEX );
		nBigDifference = ( nDifference > nBigDifference ) ? nDifference : nBigDifference;
	}

	//if I can't.. skip that space:
	if( nBigDifference )
		return nBigDifference;


	//let's try, if can't connect, try to find the maximum  difference :)
	for( y = 0; y < nHeight; ++y )
	{
		int32 nActY = (nOffsetY+y)*m_nChannelNumber+nFirstChannel;

		//invert the freespace -> covered space...
		t_lstSpans *pList = &(SpanBuffer.m_Texels[y]);
		t_lstSpansIt pListEnd = pList->end();
		for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			int32 nSX = nOffsetX+Span->first; 
			int32 nEX = nOffsetX+Span->second;
			for( int32 c = 0; c < nChannelNumber; ++c )
			{
				int32 nDifference = IsInsertableSpan( nActY+c, nSX, nEX );
				nMaxDifference = ( nDifference > nMaxDifference ) ? nDifference : nMaxDifference;

				//more than 5 percent -> big enought to skip
				if( nMaxDifference*20 > m_nWidth )
					return nMaxDifference;
			}
/*
			if( nMaxDifference )
			{
				//this isn't give back the "right" distance, but give me a speedup when an line complety wrong (eg. distance == width),
				//this converging to the right position slower, but skip wrong places much faster.
				break;
			}
*/
		}
/*
		if( nMaxDifference )
		{
			//this isn't give back the "right" distance, but give me a speedup when an line complety wrong (eg. distance == width),
			//this converging to the right position slower, but skip wrong places much faster.
			break;
		}
*/
	}

	return nMaxDifference;
}


//===================================================================================
// Method				:	Insert
// Description	:	Insert a span (covered space) into free space (Every time use the IsInsertable before use it!)
//===================================================================================
bool CFreeSpaceSpanBuffer::Insert( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY, const int8 nFirstChannel, const int8 nChannelNumber  )
{
	//insert..
	int32 nHeight = SpanBuffer.m_nHeight;
	for( int32 y = 0; y < nHeight; ++y )
	{
		t_lstSpans *pList = &(SpanBuffer.m_Texels[y]);
		t_lstSpansIt pListEnd = pList->end();
		for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			int32 nY = (y+nOffsetY)*m_nChannelNumber+nFirstChannel;
			int32 nSX = nOffsetX+Span->first; 
			int32 nEX = nOffsetX+Span->second;
			for( int32 c = 0; c < nChannelNumber && nFirstChannel + c < m_nChannelNumber; ++c )
				InsertSpan( nY+c, nSX, nEX );
		}
	}

	//update the list
	//RegenerateBiggestSpanList();
	return true;
}


//===================================================================================
// Method				:	WastedSpaceSpan
// Description	:	Calculate the wasted space / span (calculate only the left side)
//===================================================================================
int32 CFreeSpaceSpanBuffer::WastedSpaceSpan( const int32 nY, const int32 nXStart, const int32 nXEnd, int32 nLastXEnd )
{
	if ( nXEnd > m_nWidth || nXStart == nXEnd || nXStart < 0 || nXEnd < 0 || nXStart > m_nWidth  )
		return 0;

	int32 nWasedSpace = 0;

	t_lstSpans *pList = &(m_FreeTexels[nY]);
	t_lstSpansIt pListEnd = pList->end();
	for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
	{
		int32 nFreeXStart = Span->first;
		int32 nFreeXEnd = Span->second;
		//Cover-end smaller than the new span startpos... not intersted...
		if( nFreeXEnd <= nXStart )
		{
			//if it's an uncounted area add 
			if( nFreeXEnd > nLastXEnd )
			{
				nWasedSpace += nFreeXEnd - nLastXEnd;
				nLastXEnd = nWasedSpace;
			}
			continue;
		}


		//we can be very fast, becosuse, if the there isn't enougth free space for our span, we can't insert the span..
		if( nFreeXStart <= nXStart && nFreeXEnd >= nXEnd )
		{
			//if it's just a "peace" the wasted space
			if( nLastXEnd > nFreeXStart )
				nWasedSpace += nXStart - nLastXEnd;
			else
				nWasedSpace += nXStart - nFreeXStart;
			return nWasedSpace;
		}
	}

	//Error: why ?
	assert(0);
	return m_nWidth*m_nHeight;
}


//===================================================================================
// Method				:	IsInsertableSpan
// Description	:	Return values: 0 == Insertable to this position. <> 0 means this is an "at least" distance...
//===================================================================================
int32 CFreeSpaceSpanBuffer::IsInsertableSpan( const int32 nY, const int32 nXStart, const int32 nXEnd )
{
	if ( nXEnd > m_nWidth || nXStart < 0 || nXEnd < 0 || nXStart > m_nWidth  )
		return m_nWidth;
	//size == 0 -> we can insert of course :)
	if( nXStart == nXEnd )
		return 0;

	t_lstSpans *pList = &(m_FreeTexels[nY]);
	t_lstSpansIt pListEnd = pList->end();
	for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
	{
		int32 nFreeXStart = Span->first;
		int32 nFreeXEnd = Span->second;

		//Cover-end smaller than the new span startpos... not intersted...
		//span-end smaller than the cover startpos... not intersted...
		if( nFreeXEnd < nXStart )
			continue;

		//if that will be the next free space, go, and check that
		if( nFreeXStart > nXStart )
			return nFreeXStart-nXStart;

		//is there enought room for our span ?
		if( nFreeXEnd >= nXEnd )
			return 0;
	}

	//we don't found the "free space" span, so can't insert...
	return m_nWidth;
}


//===================================================================================
// Method				:	InsertSpan
// Description	:	Instert a span (covered space) into free space (Every time use the IsInsertable before use it!)
//===================================================================================
void CFreeSpaceSpanBuffer::InsertSpan( const int32 nY, const int32 nXStart, const int32 nXEnd )
{
	assert(nXStart>=0);
	assert(nXEnd<=m_nWidth);

	if (nXStart < 0 || nXEnd < 0 || nXEnd > m_nWidth || nXStart > m_nWidth || nXStart >= nXEnd )
		return;

	t_lstSpans *pList = &(m_FreeTexels[nY]);
	//check the free space...
	for( t_lstSpansIt Span = pList->begin(); Span != pList->end(); )
	{
		//"free space" end smaller than the new span startpos... not intrested...
		if( Span->second <= nXStart )
		{
			++Span;
			continue;
		}
		//span-end smaller than the "free space" startpos... not intrested...
		if( Span->first >= nXEnd )
		{
			++Span;
			continue;
		}

		//"special" cases
		if( Span->first < nXStart )
		{
			//rigth side covered.
			if( Span->second <= nXEnd )
			{
				Span->second = nXStart;
				++Span;
			}
			else
			{
				//inserted into the middle of the span -> new span generated
				int32 nOldSpanEnd = Span->second;
				Span->second = nXStart;

				//insert a new after this...
				++Span;
				pList->insert( Span, t_pairSpan( nXEnd, nOldSpanEnd ) );
				return;					//can't affect other spans...
			}
		}
		else
		{
			//fully coverted.
			if( Span->second <= nXEnd )
			{
				t_lstSpansIt SpanDelete = Span;
				++Span;
				//remove it...
				pList->erase( SpanDelete );
			}
			else
			{
				//left side not covered.
				Span->first = nXEnd;
				++Span;
			}
		}
	}
	return;
}


//===================================================================================
// Method				:	RegenerateBiggestSpanList
// Description	:	Do precalculation for early checks
//===================================================================================
void CFreeSpaceSpanBuffer::RegenerateBiggestSpanList()
{
	//for fast test I make search the biggest free space / vertical lines and / whole surface..
	//there are so much special cases, that's why I don't try "continously" update this values

	m_nBiggestFreeSpanAtAll = 0;
	int32 *pBiggestY;
	int32 nFreeSpace;
	t_lstSpans *pList;
	t_lstSpansIt pListEnd;
	t_lstSpansIt Span;

	for( int32 y = 0; y < m_nHeight*m_nChannelNumber; ++y )
	{
		pBiggestY		= &(m_BiggestFreeSpan[y]);
		pList				= &(m_FreeTexels[y]);
		*pBiggestY	= 0;
		pListEnd		= pList->end();

		for( Span = pList->begin(); Span != pListEnd; ++Span )
		{
			nFreeSpace = Span->second - Span->first;
			*pBiggestY = ( *pBiggestY < nFreeSpace ) ? nFreeSpace : *pBiggestY;
		}
		m_nBiggestFreeSpanAtAll = ( m_nBiggestFreeSpanAtAll < *pBiggestY ) ? *pBiggestY : m_nBiggestFreeSpanAtAll;
	}
}

//===================================================================================
// Method				:	GenerateDebugJPG
// Description	:	Generate a debug JPG for help the visualisation of the buffer
//===================================================================================
void CFreeSpaceSpanBuffer::GenerateDebugJPG()
{
	uint8 *pBuffer = new uint8[m_nHeight*m_nWidth*m_nChannelNumber*4];
	if( NULL == pBuffer )
		return;

	memset( pBuffer, 0, sizeof(uint8)*m_nHeight*m_nWidth*m_nChannelNumber*4);

	for( int32 c = 0; c < m_nChannelNumber; ++c )
		for( int32 y = 0; y < m_nHeight; ++y )
		{
			t_lstSpans *pList = &(m_FreeTexels[y*m_nChannelNumber+c]);
			t_lstSpansIt pListEnd = pList->end();
			for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
			{
				int32 nXend = Span->second;
				for( int32 x = Span->first; x < nXend; ++x )
				{
					int32 nIndex = ((y+c*m_nHeight)*m_nWidth+x)*4;
					pBuffer[ nIndex+0 ] =
					pBuffer[ nIndex+1 ] =
					pBuffer[ nIndex+2 ] =
					pBuffer[ nIndex+3 ] = 255;
				}//X
			}//Span
		}//Y

	char szFile[128];
	sprintf(szFile, "FreeSpaceSpanBuffer_%i.jpg",(uint32) (this) );
	GetIEditor()->GetRenderer()->WriteJPG(pBuffer, m_nWidth, m_nHeight*m_nChannelNumber, szFile,32);

	SAFE_DELETE_ARRAY( pBuffer );
}




//===================================================================================
// Method				:	Clear
// Description	:	Clear the span buffer
//===================================================================================
void CFreeSpaceBuffer::Clear()
{
	if( m_pBitMask )
		m_pBitMask->Clear();
	for( int32 i = 0; i < m_nHeight*m_nChannelNumber; ++i )
		m_BiggestFreeSpan[i] = m_nWidth;
	m_nBiggestFreeSpanAtAll = m_nWidth;
}

//===================================================================================
// Method				:	BeginAllocations
// Description	:	Create a backup of the allocations
//===================================================================================
void CFreeSpaceBuffer::BeginAllocations()
{
	if( m_pBitMask && m_pBitMask_Backup )
		m_pBitMask_Backup->Copy( m_pBitMask );
}



//===================================================================================
// Method				:	RevertAllocations
// Description	:	Revert the allocation to backup position (Need to use BeginAllocations() before!)
//===================================================================================
void CFreeSpaceBuffer::RevertAllocations()
{
	if( m_pBitMask && m_pBitMask_Backup )
		m_pBitMask->Copy( m_pBitMask_Backup );
}


//===================================================================================
// Method				:	IsInsertable_EarlyCheck
// Description	:	Very early check, if the SpanBuffer has bigger span than the biggest free space it will return false
//===================================================================================
bool CFreeSpaceBuffer::IsInsertable_EarlyCheck( const CSpanBuffer &SpanBuffer ) const
{
	assert( SpanBuffer.m_nHeight < m_nHeight );

	if( SpanBuffer.m_nHeight > m_nHeight )
		return false;

	//no chance to insert this...
	if( SpanBuffer.m_nBiggestSpanAtAll > m_nBiggestFreeSpanAtAll )
		return false;

	//it's not mean it is insertable, just an early check for not insertable spanbuffers...
	return true;
}


//===================================================================================
// Method				:	IsInsertable
// Description	:	Return values: 0 == Insertable to this position. <> 0 means this is a "nesesary" distance...
//===================================================================================
int32 CFreeSpaceBuffer::IsInsertable( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY, const int8 nFirstChannel, const int8 nChannelNumber )
{
	assert( NULL != m_pBitMask );
	if( NULL == m_pBitMask )
		return m_nWidth;

	//can't insert...
	int32 nHeight = SpanBuffer.m_nHeight;
	if( nOffsetY+nHeight >= m_nHeight || nOffsetX+SpanBuffer.m_nWidth >= m_nWidth )
		return m_nWidth;

	int32 nMaxDifference = 0;

	//per scanline fast check...
	int32 y;
	for( y = 0; y < nHeight; ++y )
		for( int32 c = 0; c < nChannelNumber; ++c )
			if( SpanBuffer.m_BiggestSpan[y] > m_BiggestFreeSpan[(nOffsetY+y)*m_nChannelNumber+nFirstChannel+c] )
				return m_nWidth;

	//for fast rejection: test the biggest span - can i insert it ?
	int32 nBigY = (nOffsetY + SpanBuffer.m_nBiggestSpanY)*m_nChannelNumber+nFirstChannel;
	int32 nBigSX = nOffsetX + SpanBuffer.m_nBiggestSpanX;
	int32 nBigEX = nBigSX + SpanBuffer.m_nBiggestSpanSize;

	int32 nBigDifference = 0;
	for( int32 c = 0; c < nChannelNumber; ++c )
	{
		int32 nDifference = m_pBitMask->CheckLine( nBigY+c, nBigSX, nBigEX );
		nBigDifference = ( nDifference > nBigDifference ) ? nDifference : nBigDifference;
	}

	//if I can't.. skip that space:
	if( nBigDifference )
		return nBigDifference;


	//let's try, if can't connect, try to find the maximum  difference :)
	for( y = 0; y < nHeight; ++y )
	{
		int32 nActY = (nOffsetY+y)*m_nChannelNumber+nFirstChannel;

		//invert the freespace -> covered space...
		CFreeSpaceSpanBuffer::t_lstSpans *pList = &(SpanBuffer.m_Texels[y]);
		CFreeSpaceSpanBuffer::t_lstSpansIt pListEnd = pList->end();
		for( CFreeSpaceSpanBuffer::t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			int32 nSX = nOffsetX+Span->first; 
			int32 nEX = nOffsetX+Span->second;
			for( int32 c = 0; c < nChannelNumber; ++c )
			{
				int32 nDifference = m_pBitMask->CheckLine( nActY+c, nSX, nEX );
				nMaxDifference = ( nDifference > nMaxDifference ) ? nDifference : nMaxDifference;

				//more than 5 percent -> big enought to skip
				if( nMaxDifference*20 > m_nWidth )
					return nMaxDifference;
			}
		}
	}
	return nMaxDifference;
}


//===================================================================================
// Method				:	Insert
// Description	:	Insert a span (covered space) into free space (Every time use the IsInsertable before use it!)
//===================================================================================
bool CFreeSpaceBuffer::Insert( CSpanBuffer &SpanBuffer, const int32 nOffsetX, const int32 nOffsetY, const int8 nFirstChannel, const int8 nChannelNumber  )
{
	assert( NULL != m_pBitMask );
	if( NULL == m_pBitMask )
		return false;

	//insert..
	int32 nHeight = SpanBuffer.m_nHeight;
	for( int32 y = 0; y < nHeight; ++y )
	{
		CFreeSpaceSpanBuffer::t_lstSpans *pList = &(SpanBuffer.m_Texels[y]);
		CFreeSpaceSpanBuffer::t_lstSpansIt pListEnd = pList->end();
		for( CFreeSpaceSpanBuffer::t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			int32 nY = (y+nOffsetY)*m_nChannelNumber+nFirstChannel;
			int32 nSX = nOffsetX+Span->first; 
			int32 nEX = nOffsetX+Span->second;
			for( int32 c = 0; c < nChannelNumber; ++c )
				m_pBitMask->InsertLine( nY+c, nSX, nEX );
		}
	}

	//update the list
	//RegenerateBiggestSpanList();
	return true;
}


//===================================================================================
// Method				:	RegenerateBiggestSpanList
// Description	:	Do precalculation for early checks
//===================================================================================
void CFreeSpaceBuffer::RegenerateBiggestSpanList()
{
	assert( NULL != m_pBitMask );
	if( NULL == m_pBitMask )
		return;

	//for fast test I make search the biggest free space / vertical lines and / whole surface..
	//there are so much special cases, that's why I don't try "continously" update this values

	m_nBiggestFreeSpanAtAll = 0;

	for( int32 y = 0; y < m_nHeight*m_nChannelNumber; ++y )
	{
		m_BiggestFreeSpan[y]	= m_pBitMask->GetBiggestFreeLine( y );
		m_nBiggestFreeSpanAtAll = ( m_nBiggestFreeSpanAtAll < m_BiggestFreeSpan[y] ) ? m_BiggestFreeSpan[y] : m_nBiggestFreeSpanAtAll;
	}
}

//===================================================================================
// Method				:	GenerateDebugJPG
// Description	:	Generate a debug JPG for help the visualization of the buffer
//===================================================================================
void CFreeSpaceBuffer::GenerateDebugJPG()
{
	assert( NULL != m_pBitMask );
	if( NULL == m_pBitMask )
		return;

	char szFile[128];
	sprintf(szFile, "FreeSpaceSpanBuffer_%i.jpg",(uint32) (this) );
	m_pBitMask->GenerateDebugJPG( szFile );
}
