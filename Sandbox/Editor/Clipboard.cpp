////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   clipboard.cpp
//  Version:     v1.00
//  Created:     15/8/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Clipboard.h"
#include "Util/CryMemFile.h"				// CCryMemFile

#include <afxole.h>
#include <afxadv.h>

XmlNodeRef CClipboard::m_node;
CString CClipboard::m_title;

//////////////////////////////////////////////////////////////////////////
// Clipboard implementation.
//////////////////////////////////////////////////////////////////////////
void CClipboard::Put(XmlNodeRef &node,const CString &title )
{
	m_title = title;
	if (m_title.IsEmpty())
	{
		m_title = node->getTag();
	}
	m_node = node;

	PutString( m_node->getXML().c_str(),title );

	/*
	COleDataSource	Source;
	CSharedFile	sf(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT);
	CString text = node->getXML();

	sf.Write(text, text.GetLength());

	HGLOBAL hMem = sf.Detach();
	if (!hMem)
		return;
	Source.CacheGlobalData(CF_TEXT, hMem);
	Source.SetClipboard();
	*/
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CClipboard::Get() const
{
	CString str = GetString();
	
	XmlParser parser;
	XmlNodeRef node = parser.parseBuffer( str );
	return node;


	/*
	COleDataObject	obj;

	if (obj.AttachClipboard()) {
		if (obj.IsDataAvailable(CF_TEXT)) {
			HGLOBAL hmem = obj.GetGlobalData(CF_TEXT);
			CCryMemFile sf((BYTE*) ::GlobalLock(hmem), ::GlobalSize(hmem));
			CString buffer;

			LPSTR str = buffer.GetBufferSetLength(::GlobalSize(hmem));
			sf.Read(str, ::GlobalSize(hmem));
			::GlobalUnlock(hmem);

			XmlParser parser;
			XmlNodeRef node = parser.parseBuffer( buffer );
			return node;
		}
	}
	return 0;

	XmlNodeRef node = m_node;
	//m_node = 0;
	return node;
		*/
}

//////////////////////////////////////////////////////////////////////////
void CClipboard::PutString( const CString &text,const CString &title /* = ""  */)
{
	if (!OpenClipboard(NULL))
	{
		AfxMessageBox( "Cannot open the Clipboard" );
		return;
	}
	// Remove the current Clipboard contents
	if( !EmptyClipboard() )
	{
		AfxMessageBox( "Cannot empty the Clipboard" );
		return;
	}

	CSharedFile	sf(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT);

	sf.Write(text, text.GetLength());

	HGLOBAL hMem = sf.Detach();
	if (!hMem)
		return;

	// For the appropriate data formats...
	if ( ::SetClipboardData( CF_TEXT,hMem ) == NULL )
	{
		AfxMessageBox( "Unable to set Clipboard data" );
		CloseClipboard();
		return;
	}
	CloseClipboard();

	/*
	COleDataSource	Source;
	Source.CacheGlobalData(CF_TEXT, hMem);
	Source.SetClipboard();
	*/
}

//////////////////////////////////////////////////////////////////////////
CString CClipboard::GetString() const
{
	CString buffer;
	if(OpenClipboard(NULL))
	{
		buffer = (char*)GetClipboardData(CF_TEXT);
	}
	CloseClipboard();

	return buffer;
}

//////////////////////////////////////////////////////////////////////////
bool CClipboard::IsEmpty() const
{
	return GetString().IsEmpty();

	//if (Get() != NULL)
		//return false;
	/*
	COleDataObject	obj;
	if (obj.AttachClipboard())
	{
		if (obj.IsDataAvailable(CF_TEXT)) 
		{
			return true;
		}
	}
	*/

	//return true;
}

//////////////////////////////////////////////////////////////////////////
void CClipboard::PutImage(const CImage & img)
{
	HBITMAP hBm = CreateBitmap(img.GetWidth(), img.GetHeight(), 1, 32, img.GetData());
	if(!hBm)
	{
		AfxMessageBox( "Cannot create Bitmap" );
		return;
	}

	if (!OpenClipboard(NULL))
	{
		AfxMessageBox( "Cannot open the Clipboard" );
		return;
	}
	// Remove the current Clipboard contents
	if( !EmptyClipboard() )
	{
		AfxMessageBox( "Cannot empty the Clipboard" );
		return;
	}

	SetClipboardData(CF_BITMAP, hBm);
	CloseClipboard();

	DeleteObject(hBm);
}

//////////////////////////////////////////////////////////////////////////
bool CClipboard::GetImage(CImage & img)
{
	if (!OpenClipboard(NULL))
	{
		AfxMessageBox( "Cannot open the Clipboard" );
		return false;
	}

	HANDLE hDib = GetClipboardData(CF_DIB);
	if(!hDib)
	{
		AfxMessageBox( "No Bitmap in clipboard" );
		return false;
	}

	BITMAPINFO * pbi = (BITMAPINFO*)GlobalLock(hDib);

	img.Allocate(pbi->bmiHeader.biWidth, pbi->bmiHeader.biHeight);

	unsigned char * pSrc = (unsigned char *)&pbi->bmiColors;
	unsigned char * pDst = (unsigned char *)img.GetData();

	if(pbi->bmiHeader.biCompression==BI_BITFIELDS)
		pSrc+=3*sizeof(DWORD);

	int stSrc = (pbi->bmiHeader.biBitCount == 24) ? 3 : 4;

	for(int y = 0; y<pbi->bmiHeader.biHeight; y++)
		for(int x = 0; x<pbi->bmiHeader.biWidth; x++)
		{
			pDst[x*4 + (pbi->bmiHeader.biHeight-y-1)*pbi->bmiHeader.biWidth*4] = pSrc[x*stSrc + y*pbi->bmiHeader.biWidth*stSrc];
			pDst[x*4 + (pbi->bmiHeader.biHeight-y-1)*pbi->bmiHeader.biWidth*4+1] = pSrc[x*stSrc + y*pbi->bmiHeader.biWidth*stSrc+1];
			pDst[x*4 + (pbi->bmiHeader.biHeight-y-1)*pbi->bmiHeader.biWidth*4+2] = pSrc[x*stSrc + y*pbi->bmiHeader.biWidth*stSrc+2];
			pDst[x*4 + (pbi->bmiHeader.biHeight-y-1)*pbi->bmiHeader.biWidth*4+3] = 0;
		}

	GlobalUnlock(pbi);
	CloseClipboard();

	return true;
}