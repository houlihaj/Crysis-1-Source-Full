////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   PickObjectTool.h
//  Version:     v1.00
//  Created:     18/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Definition of PickObjectTool, tool used to pick objects.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MaterialPickTool_h__
#define __MaterialPickTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"
#include "Objects\ObjectManager.h"

//////////////////////////////////////////////////////////////////////////
class CMaterialPickTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CMaterialPickTool)

	CMaterialPickTool();

	static void RegisterTool( CRegistrationContext &rc );

	//////////////////////////////////////////////////////////////////////////
	// CEditTool implementation
	//////////////////////////////////////////////////////////////////////////
	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	virtual void Display( DisplayContext &dc );
	//////////////////////////////////////////////////////////////////////////

protected:
	bool OnMouseMove( CViewport *view,UINT nFlags,CPoint point );
	void SetMaterial( IMaterial *pMaterial,int nSubMtlId=-1 );

	virtual ~CMaterialPickTool();
	// Delete itself.
	void DeleteThis() { delete this; };

	int m_surfaceId;
	_smart_ptr<IMaterial> m_pMaterial;
	CString m_displayString;
	Vec3 m_hitNormal;
	Vec3 m_hitPos;

	ColorF m_prevDiffuseColor;
	ColorF m_prevEmissiveColor;

	CInputLightMaterial m_prevLightMaterial;
};


#endif // __MaterialPickTool_h__
