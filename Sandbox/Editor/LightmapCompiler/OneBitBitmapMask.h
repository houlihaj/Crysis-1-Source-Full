/*=============================================================================
OneBitBitmapMask.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef __ONEBITBITMAPMASK_H__
#define __ONEBITBITMAPMASK_H__

#if _MSC_VER > 1000
# pragma once
#endif

class COneBitBitmapMask
{
public:
	//===================================================================================
	// Constructor
	//===================================================================================
	COneBitBitmapMask() : m_nWidth(0), m_nHeight(0), m_nMaskSize(0), m_pBitMask(NULL)
	{}

	//===================================================================================
	// Constructor
	//===================================================================================
	COneBitBitmapMask( const int32 nWidth, const int32 nHeight )
	{
		m_nWidth = nWidth;
		m_nHeight = nHeight;
		m_nMaskSize = ( m_nWidth * m_nHeight + 7 )/8;
		m_pBitMask = new uint8[ m_nMaskSize ];
		assert( NULL != m_pBitMask );
		Clear();
	}

	//===================================================================================
	// Destructor
	//===================================================================================
	~COneBitBitmapMask()
	{
		SAFE_DELETE_ARRAY( m_pBitMask );
	}

	//===================================================================================
	// Method				:	Clear
	// Description	:	Clear the mask (all pixel are invalid)
	//===================================================================================
	void Clear()
	{
		assert( NULL != m_pBitMask );
		if( NULL == m_pBitMask )
			return;
		memset( m_pBitMask, 0, m_nMaskSize );
	}

	//===================================================================================
	// Method				:	Copy
	// Description	:	Copy an other bitmask datas, but NEVER CHECK THE BUFFER SIZES!
	//===================================================================================
	void Copy( const COneBitBitmapMask *pOther )
	{
		assert( NULL != m_pBitMask );
		if( NULL == m_pBitMask )
			return;
		assert( NULL != pOther || NULL != pOther->m_pBitMask );
		if( NULL == pOther  || NULL == pOther->m_pBitMask )
			return;

		memcpy( m_pBitMask, pOther->m_pBitMask, m_nMaskSize );
	}

	//===================================================================================
	// Method				:	SetValid
	// Description	:	Set valid the pixel at the given coordinates
	//===================================================================================
	ILINE void SetValid( const int32 nX, const int32 nY )
	{
		assert( nY < m_nHeight );
		assert( nX < m_nWidth );

		if( ( nY >= m_nHeight ) || ( nX >= m_nWidth ) || ( nX < 0 ) || ( nY < 0 ) )
			return;
		SetValid( nY*m_nWidth+nX );
	}

	//===================================================================================
	// Method				:	SetValid
	// Description	:	Set valid the pixel at the given index
	//===================================================================================
	ILINE void SetValid( const int32 nIndex )
	{
		assert( NULL != m_pBitMask );
		if( NULL == m_pBitMask || nIndex < 0 || nIndex >= m_nMaskSize*8 )
			return;
		m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
	}

	//===================================================================================
	// Method				:	IsValid
	// Description	:	Give back that there is a valid pixel at the given coordinates
	//===================================================================================
	ILINE bool IsValid( const int32 nX, const int32 nY ) const
	{
		if( ( nY >= m_nHeight ) || ( nX >= m_nWidth ) || ( nX < 0 ) || ( nY < 0 ) )
			return false;

		return IsValid( nY*m_nWidth+nX );
	}

	//===================================================================================
	// Method				:	IsValid
	// Description	:	Give back that there is a valid pixel at the given index
	//===================================================================================
	ILINE bool	IsValid( const int32 nIndex ) const
	{
		assert( NULL != m_pBitMask );
		if( NULL == m_pBitMask || nIndex < 0 || nIndex >= m_nMaskSize*8 )
			return false;
		return ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) ? true : false;
	}

	//===================================================================================
	// Method				:	GenerateDebugJPG
	// Description	:	Generate a debug JPG from the mask
	//===================================================================================
	void GenerateDebugJPG( char *szName )
	{
		uint8 *pBuffer = new uint8[m_nHeight*m_nWidth*4];

		memset( pBuffer, 0, sizeof(uint8)*m_nHeight*m_nWidth*4);

		for( int32 c = 0; c < 4*4; ++c )
		for( int32 y = 0; y < m_nHeight/(4*4); ++y )
		{
			for( int32 x = 0; x < m_nWidth; ++x )
				{
					if( IsValid(x,y*4*4+c) )
					{
						int32 nIndex = ((y+c*(m_nHeight/(4*4)))*m_nWidth+x)*4;
						pBuffer[ nIndex+0 ] =
						pBuffer[ nIndex+1 ] =
						pBuffer[ nIndex+2 ] =
						pBuffer[ nIndex+3 ] = 255;
					}
				}//X
		}//Y

		GetIEditor()->GetRenderer()->WriteJPG(pBuffer, m_nWidth, m_nHeight, szName,32);
		SAFE_DELETE_ARRAY( pBuffer );
	}

	int		CheckLine( int32 nY, int32 nStartX, int32 nEndX )
	{
		assert( NULL != m_pBitMask );
		if( NULL == m_pBitMask )
			return 10000;

		if( nStartX == nEndX )
			return 0;

		int32 nIndex = nY*m_nWidth+nStartX;
		int32 nLastWrongPos = nIndex-1;

		//search the first free pos
		int32 nSize = nEndX - nStartX;
		int32 nLoopNum = (nSize+15) / 16;
		switch( nSize % 16 )
		{
			do
			{
			case 0:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 15:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 14:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 13:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 12:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 11:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 10:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 9:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 8:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 7:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 6:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 5:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 4:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 3:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 2:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			case 1:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
					nLastWrongPos = nIndex;
				++nIndex;
			} while( --nLoopNum > 0 );
		}

		if( nY*m_nWidth+nEndX-1 == nLastWrongPos  )
		{
/*			for( int32 i = nEndX; i < m_nWidth; ++i )
			{
				if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
					return i - (nStartX-1);
				++nIndex;
			}
*/
			int32 nSize = m_nWidth - nEndX;
			int32 nLoopNum = (nSize+15) / 16;
			switch( nSize % 16 )
			{
				do
				{
				case 0:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 15:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 14:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 13:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 12:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 11:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 10:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 9:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 8:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 7:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 6:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 5:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 4:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 3:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 2:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				case 1:
					if( 0 == ( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) ) )
						return nIndex - (nY*m_nWidth+nStartX-1);
					++nIndex;
				} while( --nLoopNum > 0 );
			}

			return m_nWidth;
		}
		else
			return nLastWrongPos - (nY*m_nWidth+nStartX-1);
	}

	bool	InsertLine( int nY, int nStartX, int nEndX )
	{
		assert( NULL != m_pBitMask );
		if( NULL == m_pBitMask )
			return false;
		int32 nIndex = nY*m_nWidth+nStartX;
/*
		for( int32 i = nStartX; i < nEndX; ++i )
		{
			m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
			++nIndex;
		}
*/
		int32 nSize = nEndX - nStartX;
		int32 nLoopNum = (nSize+15) / 16;
		switch( nSize % 16 )
		{
			do
			{
			case 0:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 15:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 14:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 13:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 12:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 11:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 10:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 9:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 8:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 7:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 6:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 5:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 4:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 3:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 2:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			case 1:
				m_pBitMask[ nIndex >> 3 ] |= 1 << (nIndex & 0x7);
				++nIndex;
			} while( --nLoopNum > 0 );
		}

		return true;
	}


	int		GetBiggestFreeLine( int nY )
	{
		assert( NULL != m_pBitMask );
		if( NULL == m_pBitMask )
			return 0;

		int32 nStart = nY*m_nWidth;
		int32 nIndex = nStart;
		int32 nLastWrongPos = nStart-1;
		int32 nDifference = 0;
/*		for( int32 i = 0; i < m_nWidth; ++i )
		{
			if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
			{
				int32 nDist = i - nLastWrongPos;
				nDifference = nDifference < nDist ? nDist : nDifference;
				nLastWrongPos = i;
			}
			++nIndex;
		}
*/
		int32 nLoopNum = (m_nWidth+15) / 16;
		switch( m_nWidth % 16 )
		{
			do
			{
			case 0:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 15:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 14:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 13:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 12:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 11:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 10:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 9:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 8:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 7:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 6:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 5:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 4:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 3:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 2:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			case 1:
				if( m_pBitMask[ nIndex >> 3 ] & (1 << (nIndex & 0x7) ) )
				{
					int32 nDist = nIndex - nLastWrongPos;
					nDifference = nDifference < nDist ? nDist : nDifference;
					nLastWrongPos = nIndex;
				}
				++nIndex;
			} while( --nLoopNum > 0 );
		}
		int32 nDist = m_nWidth-1 - (nLastWrongPos-nStart);
		nDifference = nDifference < nDist ? nDist : nDifference;
		return nDifference;
	}
public:
	int32		m_nWidth;																								//!< Width of the bitmap
	int32		m_nHeight;																							//!< Height of the bitmap
	int32		m_nMaskSize;																						//!< Mask size in byte
	uint8*	m_pBitMask;																							//!< The mask
};


#endif