#ifndef __axisgizmo_h__
#define __axisgizmo_h__
#pragma once

#include "BaseObject.h"
#include "Gizmo.h"
#include "Include\ITransformManipulator.h"

// forward declarations.
struct DisplayContext;
class  CAxisHelper;

/** Gizmo of Objects animation track.
*/
class CAxisGizmo : public CGizmo, public ITransformManipulator
{
public:
	CAxisGizmo();
	// Creates axis gizmo linked to an object.
	CAxisGizmo( CBaseObject *object );
	~CAxisGizmo();

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CGizmo
	//////////////////////////////////////////////////////////////////////////
	virtual void GetWorldBounds( BBox &bbox );
	virtual void Display( DisplayContext &dc );
	virtual bool HitTest( HitContext &hc );
	virtual const Matrix34& GetMatrix() const;
	//////////////////////////////////////////////////////////////////////////
	

	//////////////////////////////////////////////////////////////////////////
	// ITransformManipulator implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual Matrix34 GetTransformation( RefCoordSys coordSys ) const;
	virtual void SetTransformation( RefCoordSys coordSys,const Matrix34 &tm );
	virtual bool HitTestManipulator( HitContext &hc );
	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	//////////////////////////////////////////////////////////////////////////
	

	//////////////////////////////////////////////////////////////////////////
	void SetWorldBounds( const AABB &bbox );

	void DrawAxis( DisplayContext &dc );

	static int GetGlobalAxisGizmoCount() { return m_axisGizmoCount; }

private:
	void OnObjectEvent( CBaseObject *object,int event );

	CBaseObjectPtr m_object;
	AABB m_bbox;
	CAxisHelper *m_pAxisHelper;

	bool m_bDragging;
	CPoint m_cMouseDownPos;

	int m_highlightAxis;

	Matrix34 m_localTM;
	Matrix34 m_parentTM;
	Matrix34 m_userTM;

	static int m_axisGizmoCount;
};

// Define CGizmoPtr smart pointer.
SMARTPTR_TYPEDEF(CAxisGizmo);

#endif