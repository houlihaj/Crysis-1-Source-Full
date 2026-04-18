#ifndef __PHOTONVIEWTOOL_h__
#define __PHOTONVIEWTOOL_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"
#include "LightmapCompiler/PhotonMap.h"


class CPhotonViewTool : public CEditTool
{
private:

public:
	DECLARE_DYNCREATE(CPhotonViewTool)

	CPhotonViewTool() 
	{ 
		m_pDC = NULL;
	};
	virtual ~CPhotonViewTool() {};

	static void ShowPhotonMap();

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

#endif // __PhotonViewTool_h__
