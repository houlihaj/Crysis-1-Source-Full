#ifndef __PHOTONTOOL_h__
#define __PHOTONTOOL_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"
#include "LightmapCompiler/GeomCore.h"


class CPhotonTool : public CEditTool
{
private:
	void DrawOctree(COctreeCell* pCell);

public:
	DECLARE_DYNCREATE(CPhotonTool)
	
	CPhotonTool() 
	{ 
		m_pDC = NULL;
	};
	virtual ~CPhotonTool() {};

	
	static void CPhotonTool::ShowOctree();

	static void RegisterTool( CRegistrationContext &rc );

	//! Release this tool.
	virtual void DeleteThis()
	{
		delete this;
	}

	//! Used to pass user defined data to edit tool from ToolButton.
	virtual void SetUserData( void *userData ) {};

	//! Called when user starts using this tool.
	//! Flags is comnination of ObjectEditFlags flags.
	virtual void BeginEditParams( IEditor *ie,int flags )
	{
		return;
	}

	//! Called when user ends using this tool.
	virtual void EndEditParams() 
	{
		return;
	}

	virtual void Display( struct DisplayContext &dc );

	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
	{
		return false;
	}

	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
	{
		return false;
	}

	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
	{
		return false;
	}

private:
	DisplayContext* m_pDC;
};

#endif // __EditTool_h__
