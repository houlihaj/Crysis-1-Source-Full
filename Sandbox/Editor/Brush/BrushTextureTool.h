////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BrushTextureTool.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BrushTextureTool_h__
#define __BrushTextureTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

// {1BE7BDB1-D537-44de-8922-878BC7E8A5AD}
DEFINE_GUID(BRUSHTEXTURE_TOOL_GUID, 0x1be7bdb1, 0xd537, 0x44de, 0x89, 0x22, 0x87, 0x8b, 0xc7, 0xe8, 0xa5, 0xad);


#include "..\EditTool.h"

class CBaseObject;
class CSolidBrushObject;

//////////////////////////////////////////////////////////////////////////
class CBrushTextureTool : public CEditTool
{
	DECLARE_DYNCREATE(CBrushTextureTool)
public:
	CBrushTextureTool();

	static void RegisterTool( CRegistrationContext &rc );

	//////////////////////////////////////////////////////////////////////////
	// CEditTool overrides.
	//////////////////////////////////////////////////////////////////////////
	virtual bool Activate( CEditTool *pPreviousTool );

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();

	virtual void Display( DisplayContext &dc );
	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	// Key down.
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnMouseMove( CViewport *view,int nFlags, CPoint point );

	virtual void OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void FitTexture( float tileU,float tileV );
	void SelectMatID( int matId );
	void AssignMatID( int matId );
	void ApplyTextureMapping( struct SBrushTexInfo &texInfo,bool bAdd );
	//////////////////////////////////////////////////////////////////////////

protected:
	virtual ~CBrushTextureTool();
	// Delete itself.
	void DeleteThis() { delete this; };
	
private:
	bool CheckVirtualKey( int virtualKey );
	void OnSelectionChange();
	bool HitTest( CViewport *view,CPoint point,CRect rc,int nFlags );
	bool HasSelectedFaces( CSolidBrushObject *pObject );

	void AddObjectToSelection( CBaseObject *pObject );

	// Specific mouse events handlers.
	bool OnLButtonDown( CViewport *view,UINT nFlags,CPoint point );
	bool OnLButtonUp( CViewport *view,UINT nFlags,CPoint point );
	//bool OnMouseMove( CViewport *view,UINT nFlags,CPoint point );

	//! Operation modes of this viewport.
	enum OpMode
	{
		SelectMode,
		DragSelectRectMode,
		DragMode,
	};

	std::vector<CSolidBrushObject*> m_objects;
	
	//! Current brush tool operation mode.
	OpMode m_mode;
	bool m_bMouseCaptured;
	CPoint m_mouseDownPos;
	int m_numSelectedFaces;

	static float m_fGizmoScale;
};


#endif // __BrushTextureTool_h__
