/*=============================================================================
SpanBuffer.cpp : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/

#include "stdafx.h"
#include "SpanBuffer.h"
#include "OneBitBitmapMask.h"

//===================================================================================
// Method				:	Clear
// Description	:	Clear the span buffer
//===================================================================================
void CSpanBuffer::Clear()
{
	for( int32 i = 0; i < m_nHeight; ++i )
		m_Texels[i].resize(0);
	m_Texels.assign( m_nHeight, t_lstSpans( 0 ) );
	m_BiggestSpan.assign( m_nHeight, 0 );
	m_nBiggestSpanAtAll = 0;
	m_nDefaultWastedSpace = 0;
}


//===================================================================================
// Method				:	ConvertOneBitBitmapMaskToSpanBuffer
// Description	:	Convert a COneBitBitmapMask to CSpanBuffer
//===================================================================================
void CSpanBuffer::ConvertOneBitBitmapMaskToSpanBuffer( const COneBitBitmapMask &Bitmap )
{
	//Clear the buffer
	Clear();

	//Insert the bitmap
	int32 nBitmapHeigth = Bitmap.m_nHeight;
	int32 nBitmapWidth = Bitmap.m_nWidth;
	int32 nStartPos;
	bool bSearchStartPos;
	for( int32 y = 0; y < nBitmapHeigth; ++y )
	{
		bSearchStartPos = true;
		//search x start, and x ends...
		int32 x;
		for(x = 0; x < nBitmapWidth; ++x )
		{
			//currently we search a start position (Valid pixel)
			if( bSearchStartPos )
			{
				//if found a valid pixel, we note the pos, and start search the first not valid pixel
				if( Bitmap.IsValid( x,y) )
				{
					nStartPos = x;
					bSearchStartPos = false;
				}
			}
			else
			{
				//search the first not valid pixel
				if( false == Bitmap.IsValid( x,y ) )
				{
					//insert a span and start search the startpos again
					InsertSpan_LightWeight( y, nStartPos, x );
					bSearchStartPos = true;
				}
			}//if bSearchStartPos
		}//width

		//special case.
		if( false == bSearchStartPos )
		{
			//insert a span and start search the startpos again
			InsertSpan_LightWeight( y, nStartPos, x );
		}
	}//height

	RegenerateBiggestSpanList();
	SearchTheBiggestSpan();
}

//===================================================================================
// Method				:	InsertSpan_LightWeight
// Description	:	Insert 1 span, don't try to clip anything, just insert, very dumb, but fast.
//===================================================================================
void CSpanBuffer::InsertSpan_LightWeight( const int32 nY, const int32 nXStart, const int32 nXEnd )
{
	//lightweight == just insert, don't try to "close up" the spans
	assert(nXStart>=0);
	assert(nXEnd<=m_nWidth);

	if (nXStart < 0 || nXEnd > m_nWidth || nXStart > m_nWidth || nXEnd < 0 )
		return;

	t_lstSpans *pList = &(m_Texels[nY]);
	t_lstSpansIt pListEnd = pList->end();
  t_lstSpansIt Span;
	for( Span = pList->begin(); Span != pListEnd; ++Span )
	{
		//search the last span at left side...
		if( Span->second >= nXStart )
			break;
	}

	//insert the span
	pList->insert( Span, t_pairSpan( nXStart, nXEnd ) );
	return;
}

//===================================================================================
// Method				:	RegenerateBiggestSpanList
// Description	:	Do precalculation for early checks
//===================================================================================
void CSpanBuffer::RegenerateBiggestSpanList()
{
	//for fast test I make search the biggest free space / vertical lines and / whole surface..
	//there are so much special cases, that's why I don't try "continously" update this values

	m_nBiggestSpanAtAll = 0;
	m_nDefaultWastedSpace = 0;

	for( int32 y = 0; y < m_nHeight; ++y )
	{
		m_BiggestSpan[y] = 0;

		int32 nLastSpanEnd = 0;
		t_lstSpans *pList = &(m_Texels[y]);
		t_lstSpansIt pListEnd = pList->end();
		for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			int32 nSpace = Span->second - Span->first;
			if( m_BiggestSpan[y] < nSpace )
				m_BiggestSpan[y] = nSpace;

			m_nDefaultWastedSpace = Span->first - nLastSpanEnd;
			nLastSpanEnd = Span->second;
		}

		if( m_nBiggestSpanAtAll < m_BiggestSpan[y] )
			m_nBiggestSpanAtAll = m_BiggestSpan[y];
	}
}

//===================================================================================
// Method				:	GenerateDebugJPG
// Description	:	Generate a debug JPG for help the visualisation of the buffer
//===================================================================================
void CSpanBuffer::GenerateDebugJPG( char *szName )
{
	uint8 *pBuffer = new uint8[m_nHeight*m_nWidth*4];

	memset( pBuffer, 0, sizeof(uint8)*m_nHeight*m_nWidth*4);

	for( int32 y = 0; y < m_nHeight; ++y )
	{
		t_lstSpans *pList = &(m_Texels[y]);
		t_lstSpansIt pListEnd = pList->end();
		for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			int32 nXend = Span->second;
			for( int32 x = Span->first; x < nXend; ++x )
			{
				int32 nIndex = (y*m_nWidth+x)*4;
				pBuffer[ nIndex+0 ] =
				pBuffer[ nIndex+1 ] =
				pBuffer[ nIndex+2 ] =
				pBuffer[ nIndex+3 ] = 255;
			}//X
		}//Span
	}//Y

	GetIEditor()->GetRenderer()->WriteJPG(pBuffer, m_nWidth, m_nHeight, szName,32);

	SAFE_DELETE_ARRAY( pBuffer );
}

int32 CSpanBuffer::SaveToFile( FILE *f )
{
	assert( NULL != f );

	//write common datas
	fwrite( &m_nHeight, sizeof(int32),1,f);
	fwrite( &m_nWidth, sizeof(int32),1,f);

	int32 nSize = sizeof(int32)*2;
	for( int32 y = 0; y < m_nHeight; ++y )
	{
		t_lstSpans *pList = &(m_Texels[y]);

		//write per vertical slices
		int32 nListSize = pList->size();
		fwrite( &nListSize, sizeof(int32), 1, f );
		nSize += sizeof(int32);

		t_lstSpansIt pListEnd = pList->end();
		for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			//write the spans...
			uint32 nSizes[2];
			nSizes[0] = Span->first;
			nSizes[1] = Span->second;

			fwrite( nSizes, sizeof(uint32)*2,1,f );
			nSize += sizeof(uint32)*2;
		}
	}

	return nSize;
}

void CSpanBuffer::LoadFromFile( FILE *f )
{
	assert( NULL != f );

	//reconstruct the spanbuffer
	fread( &m_nHeight, sizeof(int32),1,f);
	fread( &m_nWidth, sizeof(int32),1,f);
	m_Texels.assign( m_nHeight, t_lstSpans( 0 ) );
	m_BiggestSpan.assign( m_nHeight, 0 );
	m_nBiggestSpanAtAll = 0;
	m_nDefaultWastedSpace = 0;

	uint32 nSize[2];
	int32 nListSize;
	for( int32 y = 0; y < m_nHeight; ++y )
	{
		t_lstSpans *pList = &(m_Texels[y]);

		fread( &nListSize, sizeof(int32), 1, f );

		for( int32 i = 0; i < nListSize; ++i )
		{
			fread( nSize, sizeof(uint32)*2,1,f );
			pList->push_back( t_pairSpan( nSize[0], nSize[1] ) );
		}
	}

	RegenerateBiggestSpanList();
	SearchTheBiggestSpan();
}

void CSpanBuffer::LoadFromFile_Dummy( FILE *f )
{
	assert( NULL != f );

	//reconstruct the spanbuffer
	fread( &m_nHeight, sizeof(int32),1,f);
	fread( &m_nWidth, sizeof(int32),1,f);

	uint32 nU[2];
	int32 nListSize;
	for( int32 y = 0; y < m_nHeight; ++y )
	{
		fread( &nListSize, sizeof(int32), 1, f );

		for( int32 i = 0; i < nListSize; ++i )
			fread( nU, sizeof(uint32)*2,1,f );
	}
}

//===================================================================================
// Method				:	SearchTheBiggestSpan
// Description	:	Search the biggest span (help the inserting)
//===================================================================================
void CSpanBuffer::SearchTheBiggestSpan()
{
	for( int32 y = 0; y < m_nHeight; ++y )
	{
		t_lstSpans *pList = &(m_Texels[y]);
		t_lstSpansIt pListEnd = pList->end();
		for( t_lstSpansIt Span = pList->begin(); Span != pListEnd; ++Span )
		{
			int32 nSpace = Span->second - Span->first;
			if( nSpace > m_nBiggestSpanSize )
			{
				m_nBiggestSpanSize = nSpace;
				m_nBiggestSpanX = Span->first;
				m_nBiggestSpanY = y;
			}
		}
	}
}