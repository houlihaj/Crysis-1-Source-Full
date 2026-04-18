// Viewport.cpp: implementation of the CViewport class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ViewManager.h"
#include "Viewport.h"
#include "EditTool.h"
#include "Heightmap.h"
#include "Settings.h"
#include "ViewPane.h"

#include "TerrainVoxelTool.h"

#include "Util\AVI_Writer.h"
#include "Objects\ObjectManager.h"
#include "Include\ITransformManipulator.h"
#include <ITimer.h>

#include <IPhysics.h>
#include <I3DEngine.h>

// For some reasons, it does not compile if this file is not reincluded
#include "resource.h"

IMPLEMENT_DYNAMIC(CViewport,CWnd)

BEGIN_MESSAGE_MAP (CViewport, CWnd)
	//{{AFX_MSG_MAP(CViewport)
	ON_WM_MOUSEMOVE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MBUTTONDBLCLK()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	//}}AFX_MSG_MAP
	ON_WM_SETCURSOR()
END_MESSAGE_MAP ()

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
bool CViewport::m_bDegradateQuality = false;
//Matrix34 CViewport::m_constructionMatrix[LAST_COORD_SYSTEM];

CViewport::CViewport()
{
	m_cViewMenu.LoadMenu(IDR_VIEW_OPTIONS);

	m_selectionTollerance = 0;
	m_fGridZoom = 1.0f;

	m_nLastUpdateFrame = 0;
	m_nLastMouseMoveFrame = 0;

	// View mode
	m_eViewMode = NothingMode;

	//////////////////////////////////////////////////////////////////////////
	// Init standart cursors.
	//////////////////////////////////////////////////////////////////////////
	m_hCurrCursor = NULL;
	m_hCursor[STD_CURSOR_DEFAULT] = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	m_hCursor[STD_CURSOR_HIT] = AfxGetApp()->LoadCursor(IDC_POINTER_OBJHIT);
	m_hCursor[STD_CURSOR_MOVE] = AfxGetApp()->LoadCursor(IDC_POINTER_OBJECT_MOVE);
	m_hCursor[STD_CURSOR_ROTATE] = AfxGetApp()->LoadCursor(IDC_POINTER_OBJECT_ROTATE);
	m_hCursor[STD_CURSOR_SCALE] = AfxGetApp()->LoadCursor(IDC_POINTER_OBJECT_SCALE);
	m_hCursor[STD_CURSOR_SEL_PLUS] = AfxGetApp()->LoadCursor(IDC_POINTER_PLUS);
	m_hCursor[STD_CURSOR_SEL_MINUS] = AfxGetApp()->LoadCursor(IDC_POINTER_MINUS);
	m_hCursor[STD_CURSOR_SUBOBJ_SEL] = AfxGetApp()->LoadCursor(IDC_POINTER_SO_SELECT);
	m_hCursor[STD_CURSOR_SUBOBJ_SEL_PLUS] = AfxGetApp()->LoadCursor(IDC_POINTER_SO_SELECT_PLUS);
	m_hCursor[STD_CURSOR_SUBOBJ_SEL_MINUS] = AfxGetApp()->LoadCursor(IDC_POINTER_SO_SELECT_MINUS);
	m_hCursor[STD_CURSOR_CRYSIS] = AfxGetApp()->LoadCursor(IDC_CRYSISCURSOR);

	m_activeAxis = AXIS_TERRAIN;

	for (int i = 0; i < LAST_COORD_SYSTEM; i++)
	{
		m_constructionMatrix[i].SetIdentity();
	}
	m_viewTM.SetIdentity();

	m_bRMouseDown = false;

	m_pMouseOverObject = 0;

	m_pAVIWriter = 0;
	m_bAVICreation = false;
	m_bAVIPaused = false;

	m_bAdvancedSelectMode = false;
	
	m_pVisibleObjectsCache = new CBaseObjectsCache;

	m_constructionPlane.SetPlane(Vec3(0.0f,0.0f,1.0f),Vec3(0.0f,0.0f,0.0f));
	m_constructionPlaneAxisX.Set(0,0,0);
	m_constructionPlaneAxisY.Set(0,0,0);

	GetIEditor()->GetViewManager()->RegisterViewport(this);

	m_pLocalEditTool = 0;
}

CViewport::~CViewport()
{
	m_pLocalEditTool = 0;
	delete m_pVisibleObjectsCache;

	StopAVIRecording();
	GetIEditor()->GetViewManager()->UnregisterViewport(this);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::CreateViewportWindow()
{
	if (!m_hWnd)
	{
		//////////////////////////////////////////////////////////////////////////
		// Create window.
		//////////////////////////////////////////////////////////////////////////
		CRect rcDefault(0,0,100,100);
		LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW|CS_OWNDC,	AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
		VERIFY( CreateEx( NULL,lpClassName,"",WS_CHILD|WS_CLIPCHILDREN,rcDefault, AfxGetMainWnd(), NULL));
	}
}

//////////////////////////////////////////////////////////////////////////
void CViewport::PostNcDestroy()
{
	delete this;
}

void CViewport::OnDestroy() 
{
	////////////////////////////////////////////////////////////////////////
	// Remove the view from the list
	////////////////////////////////////////////////////////////////////////
	CWnd::OnDestroy();

	//GetIEditor()->GetViewManager()->UnregisterView( this ); // Register the view
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetActive( bool bActive )
{
	m_bActive = bActive;
	if (!m_bActive)
	{
		// If any AVI recording goes on, turn it off.
		if (IsAVIRecording())
			StopAVIRecording();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::IsActive() const
{
	return m_bActive;
}

//////////////////////////////////////////////////////////////////////////
CEditTool* CViewport::GetEditTool()
{
	if (m_pLocalEditTool)
		return m_pLocalEditTool;
	return GetIEditor()->GetEditTool();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetEditTool( CEditTool *pEditTool,bool bLocalToViewport/*=false */ )
{
	if (m_pLocalEditTool == pEditTool)
		return;

	if (m_pLocalEditTool)
		m_pLocalEditTool->EndEditParams();
	m_pLocalEditTool = 0;

	if (bLocalToViewport)
	{
		m_pLocalEditTool = pEditTool;
		m_pLocalEditTool->BeginEditParams( GetIEditor(),0 );
	}
	else
	{
		m_pLocalEditTool = 0;
		GetIEditor()->SetEditTool(pEditTool);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CViewport::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	// TODO: Add your message handler code here and/or call default
	// TODO: Add your message handler code here and/or call default
	float z = GetZoomFactor() + (zDelta / 120.0f) * 0.5f;

	SetZoomFactor( z );

	GetIEditor()->GetViewManager()->SetZoomFactor( z );

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
CString CViewport::GetName() const
{
	CString name;
	if (IsWindow(GetSafeHwnd()))
	{
		GetWindowText(name);
	}

	return name;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetName( const CString &name )
{
	if (IsWindow(GetSafeHwnd()))
	{
		SetWindowText(name);
		if (GetParent())
		{
			if (GetParent()->IsKindOf(RUNTIME_CLASS(CLayoutViewPane)))
				GetParent()->SendMessage( WM_VIEWPORT_ON_TITLE_CHANGE );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CCryEditDoc* CViewport::GetDocument()
{
	return GetIEditor()->GetDocument();
}

//////////////////////////////////////////////////////////////////////////
BOOL CViewport::OnEraseBkgnd(CDC* pDC) 
{
	////////////////////////////////////////////////////////////////////////
	// Erase the background of the window (only if no render has been
	// attached)
	////////////////////////////////////////////////////////////////////////

	RECT rect;
	CBrush cFillBrush;
	
	// Get the rect of the client window
	GetClientRect(&rect);
	
	// Create the brush
	cFillBrush.CreateSolidBrush(0x00F0F0F0);

	// Fill the entire client area
	pDC->FillRect(&rect, &cFillBrush);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	
	RECT rect;
	// Get the rect of the client window
	GetClientRect(&rect);
	
	// Create the brush
	CBrush cFillBrush;
	cFillBrush.CreateSolidBrush(0x00F0F0F0);

	// Fill the entire client area
	dc.FillRect(&rect, &cFillBrush);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnActivate()
{
	////////////////////////////////////////////////////////////////////////
	// Make this edit window the current one
	////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnDeactivate()
{
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ResetContent()
{
	m_pMouseOverObject = 0;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::UpdateContent( int flags )
{
	if (flags & eRedrawViewports)
		Invalidate(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::Update()
{
	m_bAdvancedSelectMode = false;
	bool bSpaceClick = CheckVirtualKey(VK_SPACE) & !CheckVirtualKey(VK_SHIFT) & !CheckVirtualKey(VK_CONTROL);
	CPoint point;
	::GetCursorPos( &point );
	ScreenToClient(&point);
	if (bSpaceClick && GetFocus() == this)
		m_bAdvancedSelectMode = true;

	m_nLastUpdateFrame++;

	// Record AVI.
	if (m_pAVIWriter)
		AVIRecordFrame();
}

//////////////////////////////////////////////////////////////////////////
CPoint CViewport::WorldToView( Vec3 wp )
{
	CPoint p;
	p.x = wp.x;
	p.y = wp.y;
	return p;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::WorldToView3D( Vec3 wp,int nFlags )
{
	CPoint p = WorldToView(wp);
	Vec3 out;
	out.x = p.x;
	out.y = p.y;
	out.z = wp.z;
	return out;
}

//////////////////////////////////////////////////////////////////////////
Vec3	CViewport::ViewToWorld( CPoint vp,bool *collideWithTerrain,bool onlyTerrain )
{
	Vec3 wp;
	wp.x = vp.x;
	wp.y = vp.y;
	wp.z = 0;
	if (collideWithTerrain)
		*collideWithTerrain = true;
	return wp;
}

//////////////////////////////////////////////////////////////////////////
Vec3	CViewport::ViewToWorldNormal( CPoint vp, bool onlyTerrain )
{
	return Vec3(0, 0, 0);
}


void	CViewport::ViewToWorldRay( CPoint vp,Vec3 &raySrc,Vec3 &rayDir )
{
	raySrc(0,0,0);
	rayDir(0,0,-1);
}

void CViewport::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if (GetFocus() != this)
		SetFocus();
	// Save the mouse down position
	m_cMouseDownPos = point;
	
	if (MouseCallback( eMouseLDown,point,nFlags ))
		return;

	CWnd::OnLButtonDown(nFlags, point);
}

void CViewport::OnLButtonUp(UINT nFlags, CPoint point) 
{
	// Check Edit Tool.
	if (MouseCallback( eMouseLUp,point,nFlags ))
		return;

	CWnd::OnLButtonUp(nFlags, point);
}
//////////////////////////////////////////////////////////////////////////
void CViewport::OnRButtonDown(UINT nFlags, CPoint point) 
{
	if (GetFocus() != this)
		SetFocus();

	if (MouseCallback( eMouseRDown,point,nFlags ))
		return;

	CWnd::OnRButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnRButtonUp(UINT nFlags, CPoint point) 
{
	if (MouseCallback( eMouseRUp,point,nFlags ))
		return;

	CWnd::OnRButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnMButtonDown(UINT nFlags, CPoint point) 
{
	if (GetFocus() != this)
		SetFocus();

	// Check Edit Tool.
	if (MouseCallback( eMouseMDown,point,nFlags ))
		return;

	CWnd::OnMButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnMButtonUp(UINT nFlags, CPoint point) 
{
	// Move the viewer to the mouse location.
	// Check Edit Tool.
	if (MouseCallback( eMouseMUp,point,nFlags ))
		return;

	CWnd::OnMButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnMButtonDblClk(UINT nFlags, CPoint point) 
{
	if (MouseCallback( eMouseMDblClick,point,nFlags ))
		return;

	CWnd::OnMButtonDblClk(nFlags, point);
}


//////////////////////////////////////////////////////////////////////////
void CViewport::OnMouseMove(UINT nFlags, CPoint point)
{
	if (MouseCallback( eMouseMove,point,nFlags ))
		return;
	
	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ResetSelectionRegion()
{
	BBox box( Vec3(0,0,0),Vec3(0,0,0) );
	GetIEditor()->SetSelectedRegion( box );
	m_selectedRect.SetRectEmpty();
}

void CViewport::SetSelectionRectangle( CPoint p1,CPoint p2 )
{
	m_selectedRect = CRect(p1,p2);
	m_selectedRect.NormalizeRect();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnDragSelectRectangle( CPoint pnt1,CPoint pnt2,bool bNormilizeRect )
{
	Vec3 org;
	BBox box;
	box.Reset();

	Vec3 p1 = ViewToWorld( pnt1 ); 
	Vec3 p2 = ViewToWorld( pnt2 );
	org = p1;
	// Calculate selection volume.
	if (!bNormilizeRect)
	{
		box.Add( p1 );
		box.Add( p2 );
	} else {
		CRect rc( pnt1,pnt2 );
		rc.NormalizeRect();
		box.Add( ViewToWorld( CPoint(rc.left,rc.top) ));
		box.Add( ViewToWorld( CPoint(rc.right,rc.top) ));
		box.Add( ViewToWorld( CPoint(rc.left,rc.bottom) ));
		box.Add( ViewToWorld( CPoint(rc.right,rc.bottom) ));
	}

	box.min.z = -10000;
	box.max.z = 10000;	
	GetIEditor()->SetSelectedRegion( box );

	// Show marker position in the status bar
	float w = box.max.x - box.min.x;
	float h = box.max.y - box.min.y;
	char szNewStatusText[512];
	sprintf(szNewStatusText, "X:%g Y:%g Z:%g  W:%g H:%g",org.x,org.y,org.z,w,h );
	GetIEditor()->SetStatusText(szNewStatusText);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore double clicks while in game.
		return;
	}

	if (MouseCallback( eMouseLDblClick,point,nFlags ))
		return;
	
	CWnd::OnLButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::OnRButtonDblClk(UINT nFlags, CPoint point) 
{
	if (MouseCallback( eMouseRDblClick,point,nFlags ))
		return;

	CWnd::OnRButtonDblClk(nFlags, point);
}

void CViewport::CaptureMouse()
{
	if (GetCapture() != this)
	{
		SetCapture();
	}
}
	
void CViewport::ReleaseMouse()
{
	if (GetCapture() == this)
	{
		ReleaseCapture();
	}
}

void CViewport::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore key downs while in game.
		return;
	}

	if (GetEditTool())
	{
		if (GetEditTool()->OnKeyDown( this,nChar,nRepCnt,nFlags ))
			return;
	}

	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CViewport::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore key downs while in game.
		return;
	}

	if (GetEditTool())
	{
		if (GetEditTool()->OnKeyUp( this,nChar,nRepCnt,nFlags ))
			return;
	}
	
	CWnd::OnKeyUp(nChar, nRepCnt, nFlags);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetCursor( HCURSOR hCursor )
{
	m_hCurrCursor = hCursor;
	if (m_hCurrCursor)
		::SetCursor( m_hCurrCursor );
	else
		::SetCursor( m_hCursor[STD_CURSOR_DEFAULT] );
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetCurrentCursor( HCURSOR hCursor,const CString &cursorString )
{
	SetCursor(hCursor);
	m_cursorStr = cursorString;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetCurrentCursor( EStdCursor stdCursor,const CString &cursorString )
{
	HCURSOR hCursor = m_hCursor[stdCursor];
	SetCursor(hCursor);
	m_cursorStr = cursorString;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetCurrentCursor( EStdCursor stdCursor )
{
	HCURSOR hCursor = m_hCursor[stdCursor];
	SetCursor(hCursor);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::ResetCursor()
{
	SetCurrentCursor( STD_CURSOR_DEFAULT,"" );
}

//////////////////////////////////////////////////////////////////////////perforce
BOOL CViewport::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	// Check Edit Tool.
	if (GetEditTool())
	{
		if (GetEditTool()->OnSetCursor( this ))
		{
			return TRUE;
		}
	}
	if (m_hCurrCursor != NULL)
	{
		SetCursor( m_hCurrCursor );
		return TRUE;
	}

	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetConstructionOrigin( const Vec3 &worldPos )
{
	Matrix34 tm;
	tm.SetIdentity();
	tm.SetTranslation(worldPos);
	SetConstructionMatrix( COORDS_LOCAL,tm );
	SetConstructionMatrix( COORDS_PARENT,tm );
	SetConstructionMatrix( COORDS_USERDEFINED,tm );
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetConstructionMatrix( RefCoordSys coordSys,const Matrix34 &xform )
{
	m_constructionMatrix[coordSys] = xform;
	m_constructionMatrix[coordSys].OrthonormalizeFast(); // Remove scale component from matrix.
	if (coordSys == COORDS_LOCAL)
	{
		m_constructionMatrix[COORDS_VIEW].SetTranslation( xform.GetTranslation() );
		m_constructionMatrix[COORDS_WORLD].SetTranslation( xform.GetTranslation() );
		m_constructionMatrix[COORDS_USERDEFINED].SetIdentity();
		m_constructionMatrix[COORDS_USERDEFINED].SetTranslation( xform.GetTranslation() );
		m_constructionMatrix[COORDS_PARENT] = xform;
	}
	MakeConstructionPlane(GetAxisConstrain());
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CViewport::GetConstructionMatrix( RefCoordSys coordSys )
{
	if (coordSys == COORDS_VIEW)
		return m_constructionMatrix[COORDS_WORLD];
	return m_constructionMatrix[coordSys];
}

//////////////////////////////////////////////////////////////////////////
void CViewport::AssignConstructionPlane( const Vec3 &p1,const Vec3 &p2,const Vec3 &p3 )
{
	m_constructionPlane.SetPlane( p1,p2,p3 );
	m_constructionPlaneAxisX = p3-p1;
	m_constructionPlaneAxisY = p2-p1;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::MakeConstructionPlane( int axis )
{
	CPoint cursor;
	GetCursorPos(&cursor);
	ScreenToClient(&cursor);
	Vec3 raySrc(0,0,0),rayDir(1,0,0);
	ViewToWorldRay( cursor,raySrc,rayDir );
	//Vec3 rayDir = GetViewTM().GetColumn(1);

	int coordSys = GetIEditor()->GetReferenceCoordSys();

	Vec3 xAxis(1,0,0);
	Vec3 yAxis(0,1,0);
	Vec3 zAxis(0,0,1);

	xAxis = m_constructionMatrix[coordSys].TransformVector(xAxis);
	yAxis = m_constructionMatrix[coordSys].TransformVector(yAxis);
	zAxis = m_constructionMatrix[coordSys].TransformVector(zAxis);

	Vec3 pos = m_constructionMatrix[coordSys].GetTranslation();

	if (axis == AXIS_X)
	{
		// X direction.
		Vec3 n;
		float d1 = fabs(rayDir.Dot(yAxis));
		float d2 = fabs(rayDir.Dot(zAxis));
		if (d1 > d2) n = yAxis; else n = zAxis;
		if (rayDir.Dot(n) < 0) n = -n; // face construction plane to the ray.

		//Vec3 n = Vec3(0,0,1);
		Vec3 v1 = n.Cross( xAxis );
		Vec3 v2 = n.Cross( v1 );
		AssignConstructionPlane( pos,pos+v2,pos+v1 );
	}
	else if (axis == AXIS_Y)
	{
		// Y direction.
		Vec3 n;
		float d1 = fabs(rayDir.Dot(xAxis));
		float d2 = fabs(rayDir.Dot(zAxis));
		if (d1 > d2) n = xAxis; else n = zAxis;
		if (rayDir.Dot(n) < 0) n = -n; // face construction plane to the ray.

		Vec3 v1 = n.Cross( yAxis );
		Vec3 v2 = n.Cross( v1 );
		AssignConstructionPlane( pos,pos+v2,pos+v1 );
	}
	else if (axis == AXIS_Z)
	{
		// Z direction.
		Vec3 n;
		float d1 = fabs(rayDir.Dot(xAxis));
		float d2 = fabs(rayDir.Dot(yAxis));
		if (d1 > d2) n = xAxis; else n = yAxis;
		if (rayDir.Dot(n) < 0) n = -n; // face construction plane to the ray.

		Vec3 v1 = n.Cross( zAxis );
		Vec3 v2 = n.Cross( v1 );
		AssignConstructionPlane( pos,pos+v2,pos+v1 );
	}
	else if (axis == AXIS_XY)
	{
		AssignConstructionPlane( pos,pos+yAxis,pos+xAxis );
	}
	else if (axis == AXIS_XZ)
	{
		AssignConstructionPlane( pos,pos+zAxis,pos+xAxis );
	}
	else if (axis == AXIS_YZ)
	{
		AssignConstructionPlane( pos,pos+zAxis,pos+yAxis );
	}
	else
	{
		AssignConstructionPlane( pos,pos+yAxis,pos+xAxis );
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::MapViewToCP( CPoint point,int axis )
{
	if (axis == AXIS_TERRAIN)
	{
		return SnapToGrid(ViewToWorld(point));
	}

	MakeConstructionPlane(axis);

	Vec3 raySrc(0,0,0),rayDir(1,0,0);
	ViewToWorldRay( point,raySrc,rayDir );

	Vec3 v;

	Ray ray(raySrc,rayDir);
	if (!Intersect::Ray_Plane( ray,m_constructionPlane,v ))
	{
		Plane inversePlane = m_constructionPlane;
		inversePlane.n = -inversePlane.n;
		inversePlane.d = -inversePlane.d;
		if (!Intersect::Ray_Plane( ray,inversePlane,v ))
		{
			v.Set(0,0,0);
		}
	}

	// Snap value to grid.
	v = SnapToGrid(v);

	return v;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::GetCPVector( const Vec3 &p1,const Vec3 &p2,int axis )
{
	Vec3 v = p2 - p1;

	int coordSys = GetIEditor()->GetReferenceCoordSys();

	Vec3 xAxis(1,0,0);
	Vec3 yAxis(0,1,0);
	Vec3 zAxis(0,0,1);
	// In local coordinate system transform axises by construction matrix.
	
	xAxis = m_constructionMatrix[coordSys].TransformVector(xAxis);
	yAxis = m_constructionMatrix[coordSys].TransformVector(yAxis);
	zAxis = m_constructionMatrix[coordSys].TransformVector(zAxis);

	if (axis == AXIS_X || axis == AXIS_Y || axis == AXIS_Z)
	{
		// Project vector v on transformed axis x,y or z.
		Vec3 axisVector;
		if (axis == AXIS_X)
			axisVector = xAxis;
		if (axis == AXIS_Y)
			axisVector = yAxis;
		if (axis == AXIS_Z)
			axisVector = zAxis;
		
		// Project vector on construction plane into the one of axises.
		v = v.Dot(axisVector) * axisVector;
	}
	else if (axis == AXIS_XY)
	{
		// Project vector v on transformed plane x/y.
		Vec3 planeNormal = xAxis.Cross(yAxis);
		Vec3 projV = v.Dot(planeNormal) * planeNormal;
		v = v - projV;
	}
	else if (axis == AXIS_XZ)
	{
		// Project vector v on transformed plane x/y.
		Vec3 planeNormal = xAxis.Cross(zAxis);
		Vec3 projV = v.Dot(planeNormal) * planeNormal;
		v = v - projV;
	}
	else if (axis == AXIS_YZ)
	{
		// Project vector v on transformed plane x/y.
		Vec3 planeNormal = yAxis.Cross(zAxis);
		Vec3 projV = v.Dot(planeNormal) * planeNormal;
		v = v - projV;
	}
	else if (axis == AXIS_TERRAIN)
	{
		v.z = 0;
	}
	return v;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetAxisConstrain( int axis )
{
	m_activeAxis = axis;
};

//////////////////////////////////////////////////////////////////////////
bool CViewport::HitTest( CPoint point,HitContext &hitInfo )
{
	Vec3 raySrc(0,0,0),rayDir(1,0,0);
	ViewToWorldRay( point,hitInfo.raySrc,hitInfo.rayDir );
	hitInfo.view = this;
	hitInfo.point2d = point;
	if (m_bAdvancedSelectMode)
		hitInfo.bUseSelectionHelpers = true;

	return GetIEditor()->GetObjectManager()->HitTest( hitInfo );
}

//////////////////////////////////////////////////////////////////////////
void CViewport::SetZoomFactor(float fZoomFactor)
{
	m_fZoomFactor = fZoomFactor;
	if (gSettings.viewports.bSync2DViews && GetType() != ET_ViewportCamera && GetType() != ET_ViewportModel)
		GetViewManager()->SetZoom2D( fZoomFactor );
};

//////////////////////////////////////////////////////////////////////////
float CViewport::GetZoomFactor() const
{
	if (gSettings.viewports.bSync2DViews && GetType() != ET_ViewportCamera && GetType() != ET_ViewportModel)
	{
    m_fZoomFactor = GetViewManager()->GetZoom2D();
	}
	return m_fZoomFactor;
};

//////////////////////////////////////////////////////////////////////////
Vec3 CViewport::SnapToGrid( Vec3 vec )
{
	return m_viewManager->GetGrid()->Snap(vec,m_fGridZoom);
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::CheckVirtualKey( int virtualKey )
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey) & (1<<15))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::BeginUndo()
{
	DegradateQuality(true);
	GetIEditor()->BeginUndo();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::AcceptUndo( const CString &undoDescription )
{
	DegradateQuality(false);
	GetIEditor()->AcceptUndo( undoDescription );
	GetIEditor()->UpdateViews(eUpdateObjects);
}

//////////////////////////////////////////////////////////////////////////
void CViewport::CancelUndo()
{
	DegradateQuality(false);
	GetIEditor()->CancelUndo();
	GetIEditor()->UpdateViews(eUpdateObjects);
}
	
//////////////////////////////////////////////////////////////////////////
void CViewport::RestoreUndo()
{
	GetIEditor()->RestoreUndo();
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::IsUndoRecording() const
{
	return GetIEditor()->IsUndoRecording();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::DegradateQuality( bool bEnable )
{
	m_bDegradateQuality = bEnable;
}

//////////////////////////////////////////////////////////////////////////
CSize CViewport::GetIdealSize() const
{
	return CSize(0,0);
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::IsBoundsVisible( const BBox &box ) const
{
	// Always visible in standart implementation.
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::HitTestLine( const Vec3 &lineP1,const Vec3 &lineP2,CPoint hitpoint,int pixelRadius,float *pToCameraDistance )
{
	CPoint p1 = WorldToView( lineP1 );
	CPoint p2 = WorldToView( lineP2 );

	float dist = PointToLineDistance2D( Vec3(p1.x,p1.y,0),Vec3(p2.x,p2.y,0),Vec3(hitpoint.x,hitpoint.y,0) );
	if (dist <= pixelRadius)
	{
		if (pToCameraDistance)
		{
			Vec3 raySrc,rayDir;
			ViewToWorldRay( hitpoint,raySrc,rayDir );
			Vec3 rayTrg = raySrc + rayDir * 10000.0f;

			Vec3 pa,pb;
			float mua,mub;
			LineLineIntersect( lineP1,lineP2,raySrc,rayTrg,pa,pb,mua,mub );
			*pToCameraDistance = mub;
		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::StartAVIRecording( const char *filename )
{
	StopAVIRecording();
	m_bAVICreation = true;
	m_pAVIWriter = new CAVI_Writer;

	CRect rc;
	GetClientRect(rc);
	int w = rc.Width() & (~3); // must be dividable by 4
	int h = rc.Height()& (~3); // must be dividable by 4
	m_aviFrame.Allocate( w,h );

	m_aviFrameRate = gSettings.aviSettings.nFrameRate;
	if (!m_pAVIWriter->OpenFile( filename,w,h ))
	{
		delete m_pAVIWriter;
		m_pAVIWriter = 0;
	}
	m_aviLastFrameTime = GetIEditor()->GetSystem()->GetITimer()->GetCurrTime();
	m_bAVICreation = false;

	// Forces parent window (ViewPane) to redraw intself.
	if (GetParent())
		GetParent()->Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CViewport::StopAVIRecording()
{
	m_bAVICreation = true;
	if (m_pAVIWriter)
	{
		delete m_pAVIWriter;
		// Forces parent window (ViewPane) to redraw intself.
		if (GetParent())
			GetParent()->Invalidate();
	}
	m_pAVIWriter = 0;
	if (m_aviFrame.IsValid())
		m_aviFrame.Release();
	m_bAVICreation = false;
}

//////////////////////////////////////////////////////////////////////////
void CViewport::AVIRecordFrame()
{
	if (m_pAVIWriter && !m_bAVICreation && !m_bAVIPaused)
	{
		float currTime = GetIEditor()->GetSystem()->GetITimer()->GetCurrTime();
		if ( ((currTime - m_aviLastFrameTime) > (1.0f / m_aviFrameRate)) || 
					(fabs(currTime-m_aviLastFrameTime) > 1.0f))
		{
			m_aviLastFrameTime = currTime;
			//m_renderer->ReadFrameBuffer( (unsigned char*)m_aviFrame.GetData(),m_aviFrame.GetWidth(),m_aviFrame.GetHeight(),true,false );

			int width = m_aviFrame.GetWidth();
			int height = m_aviFrame.GetHeight();
			//int w = m_rcClient.Width();
			//int h = m_rcClient.Height();

			CBitmap bmp;
			CDC dcMemory;
			CDC *pDC = GetDC();
			dcMemory.CreateCompatibleDC(pDC);

			bmp.CreateCompatibleBitmap(pDC,width,height);

			CBitmap* pOldBitmap = dcMemory.SelectObject(&bmp);
			//dcMemory.BitBlt( 0,0,width,height,pDC,0,0,SRCCOPY );
			dcMemory.StretchBlt( 0,height,width,-height,pDC,0,0,width,height,SRCCOPY );

			BITMAP bmpInfo;
			bmp.GetBitmap( &bmpInfo );
			bmp.GetBitmapBits( width*height*(bmpInfo.bmBitsPixel/8),m_aviFrame.GetData() );
			int bpp = bmpInfo.bmBitsPixel/8;
/*
			char *pTemp = new char [width*height*bpp];
			char *pSrc = (char*)m_aviFrame.GetData();
			char *pTrg = (char*)m_aviFrame.GetData() + width*(height-1)*bpp;
			int lineSize = width*bpp;
			for (int h = 0; h < height; h++)
			{
				memmove( pTemp,pSrc,lineSize );
				memmove( pSrc,pTrg,lineSize );
				memmove( pTrg,pTemp,lineSize );
				pSrc += lineSize;
				pTrg -= lineSize;
			}
			delete []pTemp;
			*/

			dcMemory.SelectObject(pOldBitmap);

			ReleaseDC(pDC);

			m_pAVIWriter->AddFrame( m_aviFrame );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::IsAVIRecording() const
{
	return m_pAVIWriter != NULL;
};

//////////////////////////////////////////////////////////////////////////
void CViewport::PauseAVIRecording( bool bPause )
{
	m_bAVIPaused = bPause;
}

//////////////////////////////////////////////////////////////////////////
bool CViewport::MouseCallback( EMouseEvent event,CPoint &point,int flags )
{
	// Ignore any mouse events in game mode.
	if (GetIEditor()->IsInGameMode())
		return true;

	//////////////////////////////////////////////////////////////////////////
	// Hit test gizmo objects.
	//////////////////////////////////////////////////////////////////////////
	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = (flags & MK_CONTROL);
	bool bShiftClick = (flags & MK_SHIFT);

	switch (event)
	{
	case eMouseMove:

		if (m_nLastUpdateFrame == m_nLastMouseMoveFrame)
		{
			// If mouse move event generated in the same frame, ignore it.
			return false;
		}
		m_nLastMouseMoveFrame = m_nLastUpdateFrame;

		if (!(flags&MK_RBUTTON)/* && m_nLastUpdateFrame != m_nLastMouseMoveFrame*/)
		{
			//m_nLastMouseMoveFrame = m_nLastUpdateFrame;
			Vec3 pos = ViewToWorld( point );
			GetIEditor()->SetMarkerPosition( pos );
		}
		break;
	}

	//////////////////////////////////////////////////////////////////////////
	// Handle viewport manipulators.
	//////////////////////////////////////////////////////////////////////////
	if (!bAltClick && !bCtrlClick && !bShiftClick)
	{
		ITransformManipulator *pManipulator = GetIEditor()->GetTransformManipulator();
		if (pManipulator)
		{
			if (pManipulator->MouseCallback( this,event,point,flags ))
			{
				return true;
			}
		}
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Asks current edit tool to handle mouse callback.
	CEditTool *pEditTool = GetEditTool();
	if (pEditTool)
	{
		if (pEditTool->MouseCallback( this,event,point,flags ))
			return true;
		
		// Ask all chain of parent tools if they are handling mouse event.
		CEditTool *pParentTool = pEditTool->GetParentTool();
		while (pParentTool)
		{
			if (pParentTool->MouseCallback( this,event,point,flags ))
				return true;
			pParentTool = pParentTool->GetParentTool();
		}
	}

	return false;
}