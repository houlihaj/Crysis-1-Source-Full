/*=============================================================================
SpanBuffer.h : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#ifndef __SPANBUFFER_H__
#define __SPANBUFFER_H__

#if _MSC_VER > 1000
# pragma once
#endif

class CFreeSpaceSpanBuffer;
class CFreeSpaceBuffer;
class COneBitBitmapMask;


//SpanBuffer
class CSpanBuffer
{
public:
	typedef std::pair<uint32,uint32>	t_pairSpan;
	typedef std::list<t_pairSpan>		t_lstSpans;
	typedef t_lstSpans::iterator		t_lstSpansIt;

public:
	//===================================================================================
	// Constructor
	//===================================================================================
	CSpanBuffer() : m_nWidth(0), m_nHeight(0), m_nBiggestSpanX( 0 ), m_nBiggestSpanY (0) , m_nBiggestSpanSize (0), m_nBiggestSpanAtAll (0), m_nDefaultWastedSpace (0)

	{
		//create the free texel buffer
		m_Texels.assign(0, t_lstSpans(0));
		m_BiggestSpan.assign(0,0);
	}

	//===================================================================================
	// Constructor
	//===================================================================================
	CSpanBuffer( const int32 nWidth, const int32 nHeight ) : m_nWidth(nWidth) , m_nHeight(nHeight), m_nBiggestSpanX( 0 ), m_nBiggestSpanY (0) , m_nBiggestSpanSize (0), m_nBiggestSpanAtAll (0), m_nDefaultWastedSpace (0)
	{
		assert( nWidth > 0 );
		assert( nHeight > 0 );

		if( nWidth < 1 || nHeight < 1 )
		{
			m_nWidth = 0;
			m_nHeight = 0;
		}

		m_BiggestSpan.assign(0,0);

		//create the texel buffer
		m_Texels.assign( nHeight, t_lstSpans( 0 ) );
	}

	~CSpanBuffer()
	{
/*		CLogFile::FormatLine("Destructor" );
		m_nBiggestSpanAtAll = 0;
		m_nDefaultWastedSpace = 0;*/
	}

	void	Clear();																																	/// clear the span buffer
	void	ConvertOneBitBitmapMaskToSpanBuffer( const COneBitBitmapMask &Bitmap );		/// Convert a COneBitBitmapMask to CSpanBuffer
	int32	GetDefaultWastedSpace() const																							/// give back the "default" wasted space (default based on square...)
	{
		return m_nDefaultWastedSpace;
	};
	void	GenerateDebugJPG( char* szName );																					/// Generate a debug JPG for help the visualisation of the buffer

	int32  SaveToFile( FILE *f );																										/// Save to file
	void  LoadFromFile( FILE *f );																									/// Load from file
	void  LoadFromFile_Dummy( FILE *f );																						/// Skip the space used in file
	void	SearchTheBiggestSpan();																										/// Search the biggest span (help the inserting)
private:
	void	InsertSpan_LightWeight( const int32 nY, const int32 nXStart, const int32 nXEnd );	/// Insert 1 span, don't try to clip anything, just insert, very dumb, but fast.
	void	RegenerateBiggestSpanList();																											/// Do precalulations based on span list

protected:
	int32			m_nWidth;										//!< Width
	int32			m_nHeight;									//!< Height
	int32			m_nBiggestSpanAtAll;				//!< helper for fast rejection
	int32			m_nDefaultWastedSpace;			//!< wasted free spaces...

	int32			m_nBiggestSpanX;						//!< the biggest span start pos x
	int32			m_nBiggestSpanY;						//!< the biggest span start pos y
	int32			m_nBiggestSpanSize;					//!< the biggest span size

	std::vector<int32>	m_BiggestSpan;		//!< the biggest span / scanline
	std::vector<t_lstSpans>	m_Texels;			//!< the vertical buffer of spans....

	friend			CFreeSpaceSpanBuffer;
	friend			CFreeSpaceBuffer;
};


#endif