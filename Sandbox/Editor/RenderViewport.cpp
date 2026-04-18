// RenderViewport.cpp : implementation filefov
//

#include "stdafx.h"
#include "RenderViewport.h"
#include "DisplaySettings.h"
#include "EditTool.h"
#include "CryEditDoc.h"

#include "GameEngine.h"

#include "Objects\ObjectManager.h"
#include "Objects\BaseObject.h"
#include "Objects\CameraObject.h"
#include "VegetationMap.h"
#include "ViewManager.h"

#include "ProcessInfo.h"
#include "Settings.h"

#include <I3DEngine.h>
#include "IPhysics.h"
#include <IAISystem.h>
#include <IConsole.h>
#include <ITimer.h>
#include ".\renderviewport.h"
#include "ITestSystem.h"							// ITestSystem
#include "IRenderAuxGeom.h"
#include "IHardwareMouse.h"

IMPLEMENT_DYNCREATE(CRenderViewport,CViewport)

HWND CRenderViewport::m_currentContextWnd = 0;

#define MAX_ORBIT_DISTANCE (2000.0f)

/////////////////////////////////////////////////////////////////////////////
// CRenderViewport
/////////////////////////////////////////////////////////////////////////////
CRenderViewport::CRenderViewport()
{
	m_pSkipEnts = 0;
	m_numSkipEnts = 0;
	m_PlayerControl=0;

	m_pSkipEnts = new PIPhysicalEntity[1024];

	InitCommon();

	//////////////////////////////////////////////////////////////////////////
	// Create window.
	CreateViewportWindow();
	m_prevViewTM.SetIdentity();

	GetIEditor()->RegisterNotifyListener( this );

	m_CamFOV=gSettings.viewports.fDefaultFov;
}

CRenderViewport::~CRenderViewport()
{
	GetIEditor()->UnregisterNotifyListener( this );
	delete [] m_pSkipEnts;
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::InitCommon()
{
	m_bRenderContextCreated = false;

	m_bInRotateMode = false;
	m_bInMoveMode = false;
	m_bInOrbitMode = false;
	m_bInZoomMode = false;

	m_mousePos.SetPoint(0,0);

	m_bWireframe = GetIEditor()->GetDisplaySettings()->GetDisplayMode() == DISPLAYMODE_WIREFRAME;
	m_bDisplayLabels = GetIEditor()->GetDisplaySettings()->IsDisplayLabels();

	m_cameraObjectId = GUID_NULL;

	m_moveSpeed = 1;

	m_bSequenceCamera = false;
	m_bRMouseDown = false;
	m_bUpdating = false;
	m_bLockCameraMovement = true;

	m_nPresedKeyState = 0;
	m_prevViewTM.SetIdentity();

	m_Camera = GetIEditor()->GetSystem()->GetViewCamera();
	SetViewTM( m_Camera.GetMatrix() );
}

BEGIN_MESSAGE_MAP(CRenderViewport, CViewport)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_KEYDOWN()
	ON_WM_SETCURSOR()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CRenderViewport message handlers

int CRenderViewport::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CViewport::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_renderer = GetIEditor()->GetRenderer();
	m_engine = GetIEditor()->Get3DEngine();
	assert( m_engine );

	CreateRenderContext();
	
	return 0;
}

void CRenderViewport::OnSize(UINT nType, int cx, int cy) 
{
	CViewport::OnSize(nType, cx, cy);

	if(!cx || !cy)
		return;

	RECT rcWindow;
	GetWindowRect(&rcWindow);

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_MOVE,rcWindow.left,rcWindow.top);

	GetClientRect( m_rcClient );

	m_viewSize.cx = cx;
	m_viewSize.cy = cy;

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_RESIZE,cx,cy);
}

BOOL CRenderViewport::OnEraseBkgnd(CDC* pDC) 
{
	CGameEngine *ge = GetIEditor()->GetGameEngine();
	if (ge && ge->IsLevelLoaded())
	{
		return TRUE;
	}
	return FALSE;
}

void CRenderViewport::OnPaint() 
{
	// TODO: Add your message handler code here
	
	// Do not call CViewport::OnPaint() for painting messages
	CGameEngine *ge = GetIEditor()->GetGameEngine();
	if ((ge && ge->IsLevelLoaded()) || (GetType() != ET_ViewportCamera))
  {
		{
			CPaintDC dc(this); // device context for painting
		}
		static bool bUpdating = false;
		if (!bUpdating)
		{
			bUpdating = true;
			Update();
			bUpdating = false;
		}
  }
	else
	{
    CPaintDC dc(this); // device context for painting
	}
}

void CRenderViewport::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if (GetIEditor()->IsInGameMode())
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->Event(point.x,point.y,HARDWAREMOUSEEVENT_LBUTTONDOWN);
		}
		return;
	}

	// Convert point to position on terrain.
	if (!m_renderer)
		return;

	// Force the visible object cache to be updated - this is to ensure that
	// selection will work properly even if helpers are not being displayed,
	// in which case the cache is not updated every frame.
	if (m_displayContext.settings && !m_displayContext.settings->IsDisplayHelpers())
		GetIEditor()->GetObjectManager()->ForceUpdateVisibleObjectCache(this->m_displayContext);

	// TODO: Add your message handler code here and/or call default
	CViewport::OnLButtonDown(nFlags, point);
}

void CRenderViewport::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (GetIEditor()->IsInGameMode())
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->Event(point.x,point.y,HARDWAREMOUSEEVENT_LBUTTONUP);
		}
		return;
	}

	// Convert point to position on terrain.
	if (!m_renderer)
		return;
	// TODO: Add your message handler code here and/or call default
	CViewport::OnLButtonUp(nFlags, point);
}

void CRenderViewport::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (GetIEditor()->IsInGameMode())
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->Event(point.x,point.y,HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK);
		}
		return;
	}

	// TODO: Add your message handler code here and/or call default
	CViewport::OnLButtonDblClk(nFlags, point);
}

void CRenderViewport::OnRButtonDown(UINT nFlags, CPoint point) 
{
	m_bRMouseDown = true;
	if (GetIEditor()->IsInGameMode())
		return;

	SetFocus();

	CViewport::OnRButtonDown(nFlags, point);

	bool bAlt = (GetAsyncKeyState(VK_MENU) & (1<<15)) != 0;
	if (gSettings.bAlternativeCameraControls)
	{
		if (bAlt)
			m_bInZoomMode = true;
		else
			m_bInMoveMode = true;
	}
	else
	{
	if (bAlt)
		m_bInZoomMode = true;
	else
		m_bInRotateMode = true;
	}
	m_mousePos = point;

	CaptureMouse();
}

void CRenderViewport::OnRButtonUp(UINT nFlags, CPoint point) 
{
	m_bRMouseDown = false;
	if (GetIEditor()->IsInGameMode())
		return;
	
	CViewport::OnRButtonUp(nFlags, point);

	if (gSettings.bAlternativeCameraControls)
		m_bInMoveMode = false;
	else
	m_bInRotateMode = false;
	m_bInZoomMode = false;

	ReleaseMouse();

	/*
	//////////////////////////////////////////////////////////////////////////
	// To update object cursor.
	//////////////////////////////////////////////////////////////////////////
	// Track mouse movements.
	ObjectHitInfo hitInfo(this,point);
	HitTest( point,hitInfo );
	SetObjectCursor(hitInfo.object,true);
	*/

	// Update viewports after done with rotating.
	GetIEditor()->UpdateViews(eUpdateObjects);
}

void CRenderViewport::OnMButtonDown(UINT nFlags, CPoint point) 
{
	if (GetIEditor()->IsInGameMode())
		return;

	if (!(nFlags & MK_CONTROL) && !(nFlags & MK_SHIFT))
	{
		bool bAlt = ((GetAsyncKeyState(VK_MENU) & (1<<15)) != 0) || gSettings.bAlternativeCameraControls;
		if (bAlt)
		{
			m_bInOrbitMode = true;
//*
//			if (GetIEditor()->GetSelection()->GetCount() != 0)
//				m_orbitTarget = GetIEditor()->GetSelection()->GetCenter();
//			else
				m_orbitTarget = ViewToWorld(point);

			float orbitDistance = m_orbitTarget.GetDistance(GetViewTM().GetTranslation());
			if (orbitDistance > MAX_ORBIT_DISTANCE)
				m_orbitTarget = GetViewTM().GetTranslation() + (m_orbitTarget - GetViewTM().GetTranslation()) * (MAX_ORBIT_DISTANCE / orbitDistance);
/*/
			float orbitDistance = 10.0f;
			if (GetIEditor()->GetSelection()->GetCount() != 0)
			{
				orbitDistance = GetIEditor()->GetSelection()->GetCenter().GetDistance( GetViewTM().GetTranslation() );
			}
			else
			{
				// Distance to the center of the screen.
				orbitDistance = ViewToWorld(CPoint(m_rcClient.Width()/2,m_rcClient.Height()/2)).GetDistance(GetViewTM().GetTranslation());
			}
			if (orbitDistance > MAX_ORBIT_DISTANCE)
				orbitDistance = MAX_ORBIT_DISTANCE;
			m_orbitTarget = GetViewTM().GetTranslation() + GetViewTM().TransformVector(FORWARD_DIRECTION)*orbitDistance;
//*/
		}
		else
			m_bInMoveMode = true;
		m_mousePos = point;
		CaptureMouse();
	}
	
	CViewport::OnMButtonDown(nFlags, point);
}

void CRenderViewport::OnMButtonUp(UINT nFlags, CPoint point) 
{
	if (GetIEditor()->IsInGameMode())
		return;

	if (!gSettings.bAlternativeCameraControls)
		m_bInMoveMode = false;
	m_bInOrbitMode = false;
	m_mousePos = point;
	ReleaseMouse();

	// Update viewports after done with moving viewport.
	GetIEditor()->UpdateViews(eUpdateObjects);
	
	CViewport::OnMButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (GetIEditor()->IsInGameMode())
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->Event(point.x,point.y,HARDWAREMOUSEEVENT_MOVE);
		}
		return;
	}

	ResetCursor();


	if(!m_nPresedKeyState)
		m_nPresedKeyState	=	1;

	if (point == m_mousePos)
	{
		CViewport::OnMouseMove(nFlags, point);
		return;
	}

//	if (m_PlayerControl==1)
	if (m_PlayerControl)
	{
		//CViewport::OnMouseMove(nFlags, point);
		if ( m_bInRotateMode )
		{
			f32 MousedeltaX	=(m_mousePos.x-point.x);
			f32 MousedeltaY	=(m_mousePos.y-point.y);
			m_relCameraRotZ += MousedeltaX; 
			m_relCameraRotX += MousedeltaY; 
			CPoint pnt = m_mousePos;
			ClientToScreen( &pnt );
			SetCursorPos( pnt.x,pnt.y );
		}
//	CViewport::OnMouseMove(nFlags, point);
		return;
	}


	float speedScale = 1;
	speedScale *= gSettings.cameraMoveSpeed;
	
	if (nFlags & MK_CONTROL)
	{
		speedScale *= gSettings.cameraFastMoveSpeed;
	}

	if (gSettings.bAlternativeCameraControls && (m_bInZoomMode || m_bInMoveMode))
	{
		// Slide.
		Matrix34 m = GetViewTM();
		Vec3 xdir = m.GetColumn0().GetNormalized();
		Vec3 ydir;

		if (m_bInZoomMode || m_bInOrbitMode)
		{
			ydir = Vec3(0, 0, 1);
		}
		else
		{
			ydir = m.GetColumn1();
			ydir.z = 0;
			if (ydir.GetLength() > 0.01f)
				ydir.Normalize();
			else
				ydir = m.GetColumn2().GetNormalized();
		}

		Vec3 pos = m.GetTranslation();
		pos += 0.1f*xdir*(point.x-m_mousePos.x)*speedScale + 0.1f*ydir*(m_mousePos.y-point.y)*speedScale;
		m.SetTranslation(pos);
		SetViewTM(m);

		CPoint pnt = m_mousePos;
		ClientToScreen( &pnt );
		SetCursorPos( pnt.x,pnt.y );
		return;
	}
	else if ((m_bInRotateMode && m_bInMoveMode) || m_bInZoomMode)
	{
		// Zoom.
		Matrix34 m = GetViewTM();
		//Vec3 xdir(m[0][0],m[1][0],m[2][0]);
		Vec3 xdir(0,0,0);
		//xdir.Normalize();

		Vec3 ydir = m.GetColumn1().GetNormalized();
		Vec3 pos = m.GetTranslation();
		pos = pos - 0.2f*ydir*(m_mousePos.y-point.y)*speedScale;
		m.SetTranslation(pos);
		SetViewTM(m);

		CPoint pnt = m_mousePos;
		ClientToScreen( &pnt );
		SetCursorPos( pnt.x,pnt.y );
		return;
	}
	else if (m_bInRotateMode)
	{
		Ang3 angles( -point.y+m_mousePos.y,0,-point.x+m_mousePos.x );
		angles = angles*0.002f;

		Matrix34 camtm = GetViewTM();
		Ang3 ypr = CCamera::CreateAnglesYPR( Matrix33(camtm) );
		ypr.x += angles.z;
		ypr.y += angles.x;

		ypr.y = CLAMP(ypr.y,-1.5f,1.5f);		// to keep rotation in reasonable range
		ypr.z = 0;		// to have camara always upward
		m_CamFOV=gSettings.viewports.fDefaultFov;

		camtm = Matrix34( CCamera::CreateOrientationYPR(ypr), camtm.GetTranslation() );
		SetViewTM( camtm );

		CPoint pnt = m_mousePos;
		ClientToScreen( &pnt );
		SetCursorPos( pnt.x,pnt.y );
		return;
	}
	else if (m_bInMoveMode)
	{
		// Slide.
		Matrix34 m = GetViewTM();
		Vec3 xdir = m.GetColumn0().GetNormalized();
		Vec3 zdir = m.GetColumn2().GetNormalized();

		Vec3 pos = m.GetTranslation();
		pos += 0.1f*xdir*(point.x-m_mousePos.x)*speedScale + 0.1f*zdir*(m_mousePos.y-point.y)*speedScale;
		m.SetTranslation(pos);
    SetViewTM(m);

		CPoint pnt = m_mousePos;
		ClientToScreen( &pnt );
		SetCursorPos( pnt.x,pnt.y );
		return;
	}
	else if (m_bInOrbitMode)
	{
//*
		Vec3 cam = GetViewTM().GetTranslation();
		float dist = m_orbitTarget.GetDistance(cam);
		Matrix34 cam_matrix = GetViewTM();
		
		Ang3 ypr = CCamera::CreateAnglesYPR( Matrix33(cam_matrix) );
		float delta_yaw = CLAMP((ypr.y + (-point.y+m_mousePos.y)*0.002f), -1.5f, 1.5f) - ypr.y;

		Matrix34 new_cam_matrix = Matrix34::CreateTranslationMat(m_orbitTarget)	*
			Matrix34::CreateRotationZ((-point.x+m_mousePos.x)*0.002f) *
			Matrix34::CreateRotationAA(delta_yaw * cam_matrix.GetColumn0().GetNormalized()) *
			Matrix34::CreateTranslationMat(-m_orbitTarget) *
			cam_matrix;

		SetViewTM(new_cam_matrix);
		if (new_cam_matrix.GetTranslation().GetDistance(GetViewTM().GetTranslation()) > 0.001f)
			SetViewTM(cam_matrix);

		

/*/
		Ang3 angles( -point.y+m_mousePos.y,0,-point.x+m_mousePos.x );
		angles = angles*0.002f;

		Ang3 ypr = CCamera::CreateAnglesYPR( Matrix33(GetViewTM()) );
		ypr.x += angles.z;
		ypr.y = CLAMP(ypr.y,-1.5f,1.5f);		// to keep rotation in reasonable range
		ypr.y += angles.x;

		Matrix33 rotateTM =  CCamera::CreateOrientationYPR(ypr);
		Matrix34 camTM = GetViewTM();

		Vec3 src = GetViewTM().GetTranslation();
		Vec3 trg = m_orbitTarget;
		float fCameraRadius = (trg-src).GetLength();
		
		// Calc new source.
		src = trg - rotateTM * Vec3(0,1,0)* fCameraRadius;
		camTM = rotateTM;
		camTM.SetTranslation( src );

		SetViewTM(camTM);
//*/
		CPoint pnt = m_mousePos;
		ClientToScreen( &pnt );
		SetCursorPos( pnt.x,pnt.y );
		return;
	}

	CViewport::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::Update()
{
	if (!m_renderer || !m_engine)
		return;

	//CGameEngine *ge = GetIEditor()->GetGameEngine();
	//if (ge && !ge->IsLevelLoaded())
		//return;

	if (!IsWindowVisible())
		return;

	if (GetIEditor()->IsInGameMode())
	{
		if (!m_bUpdating)
		{
			m_bUpdating = true;
			CViewport::Update();
			m_bUpdating = false;
		}
		return;
	}

	if (!GetIEditor()->GetDocument()->IsDocumentReady())
		return;

	//CGameEngine *ge = GetIEditor()->GetGameEngine();
	//if (!ge || !ge->IsLevelLoaded())
		//return;

	// Prevents rendering recursion due to recursive Paint messages.
	if (m_bUpdating)
		return;
	m_bUpdating = true;

	FUNCTION_PROFILER( GetIEditor()->GetSystem(),PROFILE_EDITOR );

	m_currentContextWnd = 0;
	m_viewTM = m_Camera.GetMatrix(); // synchornize.




	// Render
	if (!m_bRenderContextCreated)
	{
		if (!CreateRenderContext())
			return;
	}
	m_renderer->SetCurrentContext( m_hWnd );
	m_renderer->ChangeViewport(0,0,m_rcClient.right,m_rcClient.bottom);
	m_renderer->SetCamera( m_Camera );

	m_renderer->SetClearColor( Vec3(0.4f,0.4f,0.4f) );

	// 3D engine stats
	GetIEditor()->GetSystem()->RenderBegin();

	InitDisplayContext();

	OnRender();

	m_displayContext.Flush2D();

	// 3D engine stats

	gEnv->pRenderer->GetIRenderAuxGeom()->Flush();

	CCamera CurCamera = gEnv->pSystem->GetViewCamera();
	gEnv->pSystem->SetViewCamera(m_Camera);
	GetIEditor()->GetSystem()->RenderEnd();
	gEnv->pSystem->SetViewCamera(CurCamera);

	m_renderer->SetCurrentContext(m_hWnd);

	if (GetFocus() == this)
		ProcessKeys();

	CViewport::Update();

	m_bUpdating = false;
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::UpdateGameViewport()
{
	m_renderer->SetCurrentContext( m_hWnd );
	m_renderer->ChangeViewport(0,0,m_rcClient.right,m_rcClient.bottom);
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::SetCameraObject( CBaseObject *cameraObject )
{
	if (cameraObject)
	{
		if (m_cameraObjectId == GUID_NULL)
		{
			m_prevViewName = GetName();
			m_prevViewTM = GetViewTM();
		}
		m_cameraObjectId = cameraObject->GetId();
		SetName( cameraObject->GetName() );
		GetViewManager()->SetCameraObjectId( m_cameraObjectId );
	}
	else
	{
		bool bHadCamObj = (m_cameraObjectId != GUID_NULL);
		// Switch to normal view.
		m_cameraObjectId = GUID_NULL;
	
		GetViewManager()->SetCameraObjectId( m_cameraObjectId );
		if (bHadCamObj)
		{
			SetName( m_prevViewName );
			SetViewTM( m_prevViewTM );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CRenderViewport::GetCameraObject()
{
	CBaseObject *cameraObject = 0;
	
	if (m_bSequenceCamera)
	{
		GUID cameraObjectId;

		cameraObjectId = GetViewManager()->GetCameraObjectId();

		// Camera object can change, update the ID
		if (cameraObjectId != GUID_NULL)
		{
			cameraObject = GetIEditor()->GetObjectManager()->FindObject(cameraObjectId);
		}

		SetCameraObject(cameraObject);

		return cameraObject;
	}

	if (m_cameraObjectId != GUID_NULL)
	{
		// Find camera object from id.
		cameraObject = GetIEditor()->GetObjectManager()->FindObject(m_cameraObjectId);
	}
	return cameraObject;
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnBeginGameMode:
		SetCurrentContext();
		SetCurrentCursor(STD_CURSOR_CRYSIS);
		break;
	case eNotify_OnEndGameMode:
		SetCurrentCursor(STD_CURSOR_DEFAULT);
		m_bInRotateMode = false;
		m_bInMoveMode = false;
		m_bInOrbitMode = false;
		m_bInZoomMode = false;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::OnRender()
{
	FUNCTION_PROFILER( GetIEditor()->GetSystem(),PROFILE_EDITOR );
	
	//float proj = (float)rc.bottom/(float)rc.right;
	//if (proj > 1) proj = 1;
	//m_Camera.Init( rc.right,rc.bottom,3.14f/2.0f,10000.0f,proj );
	//m_Camera.Init( 800,600,3.14f/2.0f,10000.0f,proj );

	float fNearZ = m_Camera.GetNearPlane();
	static ICVar *cl_camera_nearz = 0;
	if (!cl_camera_nearz)
	{
		cl_camera_nearz = gEnv->pConsole->GetCVar("cl_camera_nearz");
		if (cl_camera_nearz)
			fNearZ = cl_camera_nearz->GetFVal();
	}

	CBaseObject *cameraObject = GetCameraObject();
	if (cameraObject)
	{
		// Find camera object
		// This is a camera object.
		float fov = gSettings.viewports.fDefaultFov;
		if (cameraObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
		{
			CCameraObject *camObj = (CCameraObject*)cameraObject;
			fov = camObj->GetFOV();
			//m_Camera.SetFov(fov);
		}
		else
		{
			//m_Camera.SetFov(m_stdFOV);
		}
		/*
		int screenW = m_rcClient.right - m_rcClient.left;
		int screenH = m_rcClient.bottom - m_rcClient.top;	
		float fAspect = gSettings.viewports.fDefaultAspectRatio;
		int w =	 screenH/fAspect;
		int h =	 screenH;
		//int sx = (screenW - w)/2;
		int sx = 0;
		//m_renderer->ChangeViewport( sx,0,w,h );
		//float proj = (float)h/(float)w;
		*/

		m_viewTM = cameraObject->GetWorldTM();
		if (cameraObject->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			IEntity *pCameraEntity = ((CEntity*)cameraObject)->GetIEntity();
			if (pCameraEntity)
				m_viewTM = pCameraEntity->GetWorldTM();
		}

		m_Camera.SetMatrix( m_viewTM );
		
		int w = m_rcClient.right - m_rcClient.left;
		int h = m_rcClient.bottom - m_rcClient.top;	
	//	float fAspectRatio = (float)h/(float)w;
	//	if (fAspectRatio > 1.2f) fAspectRatio = 1.2f;
		m_Camera.SetFrustum( w,h,fov,  fNearZ,m_Camera.GetFarPlane() );
	}
	else
	{
		// Normal camera.
		bool bHadCamObj = (m_cameraObjectId != GUID_NULL);
		m_cameraObjectId = GUID_NULL;
	
		if (bHadCamObj)
		{
			SetName( m_prevViewName );
			SetViewTM( m_prevViewTM );
		}

		//m_Camera.SetFov(m_stdFOV);
		int w = m_rcClient.right - m_rcClient.left;
		int h = m_rcClient.bottom - m_rcClient.top;	
		//m_renderer->ChangeViewport( 0,0,w,h );
		m_Camera.SetFrustum( w,h,m_CamFOV,fNearZ, gEnv->p3DEngine->GetMaxViewDistance() );
	}

	//m_Camera.SetPos( GetViewerPos() );
	//m_Camera.SetAngle( GetViewerAngles() );
//	m_Camera.UpdateFrustum();
	GetIEditor()->GetSystem()->SetViewCamera(m_Camera);
//	m_engine->SetCamera(m_Camera);

	if (GetIEditor()->GetDisplaySettings()->GetDisplayMode() == DISPLAYMODE_WIREFRAME)
		m_renderer->SetPolygonMode(R_WIREFRAME_MODE);
	else
		m_renderer->SetPolygonMode(R_SOLID_MODE);

	if(ITestSystem *pTestSystem = GetISystem()->GetITestSystem())
		pTestSystem->BeforeRender();

	CGameEngine *ge = GetIEditor()->GetGameEngine();
	if (ge && ge->IsLevelLoaded())
	{
		m_renderer->SetViewport(0,0,m_renderer->GetWidth(),m_renderer->GetHeight());
		m_engine->Update();
		m_engine->RenderWorld(SHDF_ALLOW_AO | SHDF_ALLOWPOSTPROCESS | SHDF_ALLOW_WATER | SHDF_ALLOWHDR | SHDF_SORT | SHDF_ZPASS, GetIEditor()->GetSystem()->GetViewCamera(),"CRenderViewport:OnRender");
	}
	else
	{
		m_renderer->ClearBuffer(FRT_CLEAR_COLOR, &Col_DarkGray);
	}

  // Draw engine stats
	IConsole * console = GetIEditor()->GetSystem()->GetIConsole();


	//m_engine->SetRenderCallback( 0,0 );
	m_displayContext.SetView(this);

	m_renderer->EF_StartEf();
	m_renderer->ResetToDefault();
	m_renderer->SelectTMU(0);
	m_renderer->EnableTMU(false);

	//////////////////////////////////////////////////////////////////////////
	// Setup two infinite lights for helpers drawing.
	//////////////////////////////////////////////////////////////////////////
	CDLight light[2];
  light[0].m_Origin = m_Camera.GetPosition();
	light[0].SetLightColor(ColorF(1,1,1,1));
	light[0].m_SpecMult = 1.0f;
  light[0].m_fRadius = 1000000;
	light[0].m_fStartRadius = 0;
	light[0].m_fEndRadius = 1000000;
  light[0].m_Flags |= DLF_DIRECTIONAL;
	m_renderer->EF_ADDDlight( &light[0] );
	
	light[1].m_Origin = Vec3(100000,100000,100000);
	light[1].SetLightColor(ColorF(1,1,1,1));
	light[1].m_SpecMult = 1.0f;
  light[1].m_fRadius = 1000000;
	light[1].m_fStartRadius = 0;
	light[1].m_fEndRadius = 1000000;
  light[1].m_Flags |= DLF_DIRECTIONAL;
	m_renderer->EF_ADDDlight( &light[1] );
	//////////////////////////////////////////////////////////////////////////

	DisplayContext &dc = m_displayContext;

	RenderAll();

	m_engine->SetupDistanceFog();
	m_renderer->EF_EndEf3D(SHDF_SORT | SHDF_NOASYNC);

	//////////////////////////////////////////////////////////////////////////
	// Draw 2D helpers.
	//////////////////////////////////////////////////////////////////////////
	m_renderer->Set2DMode( true,m_rcClient.right,m_rcClient.bottom );
	m_displayContext.SetState( e_Mode3D|e_AlphaBlended|e_FillModeSolid|e_CullModeBack|e_DepthWriteOn|e_DepthTestOn );
	//////////////////////////////////////////////////////////////////////////
	// Draw selected rectangle.
	//////////////////////////////////////////////////////////////////////////
	if (!m_selectedRect.IsRectEmpty())
	{
		dc.SetColor( 1,1,1,0.4f );
		//CPoint p1 = ViewToWorld(m_selectedRect.
		CPoint p1 = CPoint( m_selectedRect.left,m_selectedRect.top );
		CPoint p2 = CPoint( m_selectedRect.right,m_selectedRect.bottom );
		dc.DrawLine( Vec3(p1.x,p1.y,0),Vec3(p2.x,p1.y,0) );
		dc.DrawLine( Vec3(p1.x,p2.y,0),Vec3(p2.x,p2.y,0) );
		dc.DrawLine( Vec3(p1.x,p1.y,0),Vec3(p1.x,p2.y,0) );
		dc.DrawLine( Vec3(p2.x,p1.y,0),Vec3(p2.x,p2.y,0) );
	}

	// Draw Axis arrow in lower right corner.
	if (!IsAVIRecording())
	{
		DrawAxis();
	}
	// Display cursor string.
	RenderCursorString();
	if (gSettings.viewports.bShowSafeFrame)
		RenderSafeFrame();

	m_renderer->Set2DMode( false,m_rcClient.right,m_rcClient.bottom );

	if(ITestSystem *pTestSystem = GetISystem()->GetITestSystem())
		pTestSystem->AfterRender();
}

static IRenderer *g_pRenderer;
static void g_DrawLine(float *v1,float *v2)
{
	Vec3 V1, V2;
	//V1.Set(v1);
	//V2.Set(v2);
	V1.Set(v1[0],v1[1],v1[2]);
	V2.Set(v2[0],v2[1],v2[2]);
 	g_pRenderer->DrawLine(V1,V2);
}

void CRenderViewport::InitDisplayContext()
{
	// Draw all objects.
	DisplayContext &dctx = m_displayContext;
	dctx.settings = GetIEditor()->GetDisplaySettings();
	dctx.view = this;
	dctx.renderer = m_renderer;
	dctx.engine = m_engine;
	dctx.box.min=Vec3( -100000,-100000,-100000 );
	dctx.box.max=Vec3( 100000,100000,100000 );
	dctx.camera = &m_Camera;
	dctx.flags = 0;
	if (!dctx.settings->IsDisplayLabels() || !dctx.settings->IsDisplayHelpers())
	{
		dctx.flags |= DISPLAY_HIDENAMES;
	}
	if (dctx.settings->IsDisplayLinks() && dctx.settings->IsDisplayHelpers())
	{
		dctx.flags |= DISPLAY_LINKS;
	}
	if (m_bDegradateQuality)
	{
		dctx.flags |= DISPLAY_DEGRADATED;
	}
	if (dctx.settings->GetRenderFlags() & RENDER_FLAG_BBOX)
	{
		dctx.flags |= DISPLAY_BBOX;
	}

	if (dctx.settings->IsDisplayTracks() && dctx.settings->IsDisplayHelpers())
	{
		dctx.flags |= DISPLAY_TRACKS;
		dctx.flags |= DISPLAY_TRACKTICKS;
	}

	if (m_bAdvancedSelectMode)
		dctx.flags |= DISPLAY_SELECTION_HELPERS;

	if (GetIEditor()->GetReferenceCoordSys() == COORDS_WORLD)
	{
		dctx.flags |= DISPLAY_WORLDSPACEAXIS;
	}
}

void CRenderViewport::RenderAll()
{
	// Draw all objects.
	DisplayContext &dctx = m_displayContext;

	m_renderer->ResetToDefault();

	//dctx.renderer->SetBlendMode();
	//dctx.renderer->EnableBlend(true);

	dctx.SetState( e_Mode3D|e_AlphaBlended|e_FillModeSolid|e_CullModeBack|e_DepthWriteOn|e_DepthTestOn );
	GetIEditor()->GetObjectManager()->Display( dctx );

	BBox box;
	GetIEditor()->GetSelectedRegion( box );
	if (!IsEquivalent(box.min,box.max,0))
		RenderTerrainGrid( box.min.x,box.min.y,box.max.x,box.max.y );

	if (gSettings.snap.constructPlaneDisplay)
		RenderConstructionPlane();

	RenderSnapMarker();

	g_pRenderer = m_renderer;
	//IPhysicalWorld *physWorld = GetIEditor()->GetSystem()->GetIPhysicalWorld();
	//if (physWorld)
	//	physWorld->DrawPhysicsHelperInformation(g_DrawLine);

	IAISystem *aiSystem = GetIEditor()->GetSystem()->GetAISystem();
	if (aiSystem)
		aiSystem->DebugDraw(m_renderer);

	if (dctx.settings->GetDebugFlags() & DBG_MEMINFO)
	{
		ProcessMemInfo mi;
		CProcessInfo::QueryMemInfo( mi );
		int MB = 1024*1024;
		CString str;
		str.Format( "WorkingSet=%dMb, PageFile=%dMb, PageFaults=%d",mi.WorkingSet/MB,mi.PagefileUsage/MB,mi.PageFaultCount );
		m_renderer->TextToScreenColor( 1,1,1,0,0,1,str );
	}

	// Display editing tool.
	if (GetEditTool())
	{
		GetEditTool()->Display( dctx );
	}
}

//////////////////////////////////////////////////////////////////////////
namespace {
	inline Vec3 NegY( const Vec3 &v,float y )
	{
		return Vec3(v.x,y-v.y,v.z);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::DrawAxis()
{
	DisplayContext &dc = m_displayContext;

	if(!dc.settings->IsDisplayHelpers())			// show axis only if draw halpers is activated
		return;

	float colx[4] = {1,0,0,1};// Red
	float coly[4] = {0,1,0,1};// Green
	float colz[4] = {0,0,1,1};// Blue
	float colw[4] = {1,1,1,1};// White

	int size = 25;
	Vec3 pos( 25,25,0 );

	Vec3 x(size,0,0);
	Vec3 y(0,size,0);
	Vec3 z(0,0,size);

	float height = m_rcClient.Height();
	Vec3 hvec(height,height,height);

	Matrix34 camMatrix = GetViewTM().GetInverted();
	x = camMatrix.TransformVector(x);
	y = camMatrix.TransformVector(y);
	z = camMatrix.TransformVector(z);

	dc.DepthWriteOff();
	dc.DepthTestOff();
	dc.CullOff();

	Vec3 src =	NegY(pos,height);
	Vec3 trgx = NegY(pos+x,height);
	Vec3 trgy = NegY(pos+y,height);
	Vec3 trgz = NegY(pos+z,height);

	dc.SetColor( colx[0],colx[1],colx[2],colx[3] );
	dc.DrawLine( src,trgx );
	dc.SetColor( coly[0],coly[1],coly[2],coly[3] );
	dc.DrawLine( src,trgy );
	dc.SetColor( colz[0],colz[1],colz[2],colz[3] );
	dc.DrawLine( src,trgz );

	m_renderer->Draw2dLabel( trgx.x,trgx.y,1.1f,colw,true,"x" );
	m_renderer->Draw2dLabel( trgy.x,trgy.y,1.1f,colw,true,"y" );
	m_renderer->Draw2dLabel( trgz.x,trgz.y,1.1f,colw,true,"z" );

	dc.DepthWriteOn();
	dc.DepthTestOn();
	dc.CullOn();
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::RenderCursorString()
{
	if (m_cursorStr.IsEmpty())
		return;

	CPoint point;
	GetCursorPos( &point );
	ScreenToClient( &point );

//	float d = GetViewerPos().Distance(pos) * 0.02;

	// Display hit object name.
	float col[4] = { 1,1,1,1 };
	m_renderer->Draw2dLabel( point.x+12,point.y+4,1.2f,col,false,"%s",(const char*)m_cursorStr );
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::RenderSafeFrame()
{
	DisplayContext &dc = m_displayContext;
	dc.DepthTestOff();

	int screenW = m_rcClient.right - m_rcClient.left;
	int screenH = m_rcClient.bottom - m_rcClient.top;	
	float fAspect = gSettings.viewports.fDefaultAspectRatio;
	int w =	 screenH*fAspect;
	int h =	 screenH;
	int sx = (screenW - w)/2;

	//float proj = (float)h/(float)w;
	dc.SetColor( 0,1,1,0.5f ); // cyan
	dc.DrawLine( Vec3(sx,0,0),Vec3(sx+w,0,0) );
	dc.DrawLine( Vec3(sx,1,0),Vec3(sx+w,1,0) );

	dc.DrawLine( Vec3(sx,h-2,0),Vec3(sx+w,h-2,0) );
	dc.DrawLine( Vec3(sx,h-1,0),Vec3(sx+w,h-1,0) );

	dc.DrawLine( Vec3(sx,0,0),Vec3(sx,h,0) );
	dc.DrawLine( Vec3(sx-1,0,0),Vec3(sx-1,h,0) );

	dc.DrawLine( Vec3(sx+w,0,0),Vec3(sx+w,h,0) );
	dc.DrawLine( Vec3(sx+w+1,0,0),Vec3(sx+w+1,h,0) );
	
	dc.DepthTestOn();
}

//////////////////////////////////////////////////////////////////////////
float CRenderViewport::GetAspectRatio() const
{
	return gSettings.viewports.fDefaultAspectRatio;
}


//////////////////////////////////////////////////////////////////////////
void CRenderViewport::RenderSnapMarker()
{
	if (!gSettings.snap.markerDisplay)
		return;

	CPoint point;
	::GetCursorPos(&point);
	ScreenToClient(&point);
	Vec3 p = MapViewToCP( point );

	DisplayContext &dc = m_displayContext;

	float fScreenScaleFactor = GetScreenScaleFactor(p);

	Vec3 x(1,0,0);
	Vec3 y(0,1,0);
	Vec3 z(0,0,1);
	x = x*gSettings.snap.markerSize*fScreenScaleFactor*0.1f;
	y = y*gSettings.snap.markerSize*fScreenScaleFactor*0.1f;
	z = z*gSettings.snap.markerSize*fScreenScaleFactor*0.1f;

	dc.SetColor( gSettings.snap.markerColor );
	dc.DrawLine( p-x,p+x );
	dc.DrawLine( p-y,p+y );
	dc.DrawLine( p-z,p+z );

	point = WorldToView(p);

	int s = 8;
	dc.DrawLine2d( point+CPoint(-s,-s),point+CPoint(s,-s),0 );
	dc.DrawLine2d( point+CPoint(-s,s),point+CPoint(s,s),0 );
	dc.DrawLine2d( point+CPoint(-s,-s),point+CPoint(-s,s),0 );
	dc.DrawLine2d( point+CPoint(s,-s),point+CPoint(s,s),0 );

	/*
	Vec3 p = GetIEditor()->GetMarkerPosition();
	float verts[4][5];
	memset( verts,0,sizeof(verts) );
	float dist = (p - m_Camera.GetPos()).Length();
	float size = 0.5f;
	float offset = 0.1f;
	float x = p.x;
	float y = p.y;

	verts[0][0] = x-size;
	verts[0][1] = y-size;
	verts[0][2] = GetIEditor()->GetTerrainElevation( x-size,y-size ) + offset;

	verts[1][0] = x+size;
	verts[1][1] = y-size;
	verts[1][2] = GetIEditor()->GetTerrainElevation( x+size,y-size ) + offset;

	verts[3][0] = x+size;
	verts[3][1] = y+size;
	verts[3][2] = GetIEditor()->GetTerrainElevation( x+size,y+size ) + offset;

	verts[2][0] = x-size;
	verts[2][1] = y+size;
	verts[2][2] = GetIEditor()->GetTerrainElevation( x-size,y+size ) + offset;

	m_renderer->SetMaterialColor( 1,0,0.8f,0.6f );
	m_renderer->SetBlendMode();
	m_renderer->EnableBlend( true );
	m_renderer->DrawTriStrip(&(CVertexBuffer(verts,VERTEX_FORMAT_P3F_TEX2F)),4);
	m_renderer->SetMaterialColor( 1,1,0,0.6f );
	m_renderer->DrawBall( p,size*0.8f );
	m_renderer->ResetToDefault();
	*/
}

inline bool SortCameraObjectsByName( CCameraObject *pObject1,CCameraObject *pObject2 )
{
	return stricmp(pObject1->GetName(),pObject2->GetName()) < 0;
}

void CRenderViewport::OnTitleMenu( CMenu &menu )
{
	m_bWireframe = GetIEditor()->GetDisplaySettings()->GetDisplayMode() == DISPLAYMODE_WIREFRAME;
	m_bDisplayLabels = GetIEditor()->GetDisplaySettings()->IsDisplayLabels();

	menu.AppendMenu( MF_STRING|((m_bWireframe)?MF_CHECKED:MF_UNCHECKED),1,"Wireframe" );
	menu.AppendMenu( MF_STRING|((m_bDisplayLabels)?MF_CHECKED:MF_UNCHECKED),2,"Labels" );
	menu.AppendMenu( MF_STRING|((gSettings.viewports.bShowSafeFrame)?MF_CHECKED:MF_UNCHECKED),3,"Show Safe Frame" );
	menu.AppendMenu( MF_STRING|((gSettings.snap.constructPlaneDisplay)?MF_CHECKED:MF_UNCHECKED),4,"Show Construction Plane" );
	menu.AppendMenu( MF_STRING|((gSettings.viewports.bShowTriggerBounds)?MF_CHECKED:MF_UNCHECKED),5,"Show Trigger Bounds" );
	menu.AppendMenu( MF_STRING|((gSettings.viewports.bShowIcons)?MF_CHECKED:MF_UNCHECKED),6,"Show Icons" );

	// Add Cameras.
	std::vector<CCameraObject*> objects;
	((CObjectManager*)GetIEditor()->GetObjectManager())->GetCameras( objects );
	if (!objects.empty())
	{
		std::sort( objects.begin(),objects.end(),SortCameraObjectsByName );
		menu.AppendMenu( MF_SEPARATOR,0,"" );
		menu.AppendMenu( MF_STRING|((m_cameraObjectId == GUID_NULL && !m_bSequenceCamera)?MF_CHECKED:MF_UNCHECKED),1000,"Default Camera" );
		menu.AppendMenu( MF_STRING|((m_bSequenceCamera)?MF_CHECKED:MF_UNCHECKED),1001,"Sequence Camera" );
		menu.AppendMenu( MF_STRING|((m_bLockCameraMovement)?MF_CHECKED:MF_UNCHECKED),1002,"Lock Camera Movement" );
		menu.AppendMenu( MF_SEPARATOR,0,"" );

		static CMenu subMenu;
		if (subMenu.m_hMenu)
			subMenu.DestroyMenu();
		subMenu.CreatePopupMenu();
		menu.AppendMenu( MF_POPUP,(UINT_PTR)subMenu.GetSafeHmenu(),"Camera" );
		for (int i = 0; i < objects.size(); i++)
		{
			int state = (m_cameraObjectId == objects[i]->GetId() && !m_bSequenceCamera)?MF_CHECKED:MF_UNCHECKED;
			subMenu.AppendMenu( MF_STRING|state,1100+i,objects[i]->GetName() );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::OnTitleMenuCommand( int id )
{
	if (id >= 1000 && id < 2000)
	{
		if (id == 1000)
		{
			m_bSequenceCamera = false;
			SetCameraObject(0);
		}
		else if (id == 1001)
		{
			m_bSequenceCamera = true;
			SetCameraObject(0);
		}
		else if (id == 1002)
		{
			m_bLockCameraMovement = !m_bLockCameraMovement;
		}
		else
		{
			m_bSequenceCamera = false;
			// Switch to Camera Object.
			std::vector<CCameraObject*> objects;
			((CObjectManager*)GetIEditor()->GetObjectManager())->GetCameras( objects );
			std::sort( objects.begin(),objects.end(),SortCameraObjectsByName );
			int index = id - 1100;
			if (index < objects.size())
			{
				SetCameraObject( objects[index] );
			}
		}
	}
	switch (id)
	{
		case 1:
			m_bWireframe = !m_bWireframe;
			if (m_bWireframe)
				GetIEditor()->GetDisplaySettings()->SetDisplayMode( DISPLAYMODE_WIREFRAME );
			else
				GetIEditor()->GetDisplaySettings()->SetDisplayMode( DISPLAYMODE_SOLID );
			break;
		case 2:
			m_bDisplayLabels = !m_bDisplayLabels;
			GetIEditor()->GetDisplaySettings()->DisplayLabels( m_bDisplayLabels );
			break;
		case 3:
			gSettings.viewports.bShowSafeFrame = !gSettings.viewports.bShowSafeFrame;
			break;
		case 4:
			gSettings.snap.constructPlaneDisplay = !gSettings.snap.constructPlaneDisplay;
			break;
		case 5:
			gSettings.viewports.bShowTriggerBounds = !gSettings.viewports.bShowTriggerBounds;
			break;
		case 6:
			gSettings.viewports.bShowIcons = !gSettings.viewports.bShowIcons;
			break;
	}
}

void CRenderViewport::ToggleCameraObject()
{
	m_bSequenceCamera = !m_bSequenceCamera;
	SetCameraObject(0);
}

bool CRenderViewport::GetCameraObjectState()
{
	return m_bSequenceCamera;
}

//////////////////////////////////////////////////////////////////////////
BOOL CRenderViewport::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	if (GetIEditor()->IsInGameMode())
		return FALSE;

	// TODO: Add your message handler code here and/or call default
	Matrix34 m = GetViewTM();
	Vec3 ydir = m.GetColumn1().GetNormalized();
		
	Vec3 pos = m.GetTranslation();

	if (gSettings.bAlternativeCameraControls)
	{
		float orbitDistance = ViewToWorld(CPoint(m_rcClient.Width()/2,m_rcClient.Height()/2)).GetDistance(GetViewTM().GetTranslation());
		if (orbitDistance > MAX_ORBIT_DISTANCE)
			orbitDistance = MAX_ORBIT_DISTANCE;

		pos += ydir * (orbitDistance * (powf(1.1f, sgn(zDelta)) - 1.0f));
	}
	else
	pos += 0.01f*ydir*(zDelta) * gSettings.wheelZoomSpeed;

	m.SetTranslation(pos);	
	SetViewTM(m);
//	GetIEditor()->MoveViewer(0.2f*ydir*(zDelta));
	
	return CViewport::OnMouseWheel(nFlags, zDelta, pt);
}

void CRenderViewport::SetCamera( const CCamera &camera )
{	
	m_Camera = camera;
	SetViewTM( m_Camera.GetMatrix() );
}

//////////////////////////////////////////////////////////////////////////
void	CRenderViewport::SetViewTM( const Matrix34 &viewTM )
{
	Matrix34 camMatrix = viewTM;

	// If no collision flag set do not check for terrain elevation.
	if (GetType() == ET_ViewportCamera)
	{
		if (!GetCameraObject())
		{
			if ((GetIEditor()->GetDisplaySettings()->GetSettings()&SETTINGS_NOCOLLISION) == 0)
			{
				Vec3 p = camMatrix.GetTranslation();
				float z = GetIEditor()->GetTerrainElevation( p.x,p.y );
				if (p.z < z+1) 
				{
					p.z = z+1;
					camMatrix.SetTranslation(p);
				}
			}
		}
		
		// Also force this position on game.
		if (GetIEditor()->GetGameEngine())
		{
			GetIEditor()->GetGameEngine()->SetPlayerViewMatrix(viewTM);
		}
	}

	CBaseObject *cameraObject = GetCameraObject();
	if (cameraObject)
	{
		// Ignore camera movement if locked.
		if (m_bLockCameraMovement || !GetIEditor()->GetAnimation()->IsRecordMode())
			return;
		if(!m_nPresedKeyState	|| m_nPresedKeyState==1)
		{
			CUndo	undo("Move Camera");
			cameraObject->SetWorldTM(	camMatrix );
		}
		else
		{
			cameraObject->SetWorldTM(	camMatrix );
		}
	}

	if(m_nPresedKeyState==1)
		m_nPresedKeyState=2;

	CViewport::SetViewTM(camMatrix);

	m_Camera.SetMatrix(camMatrix);
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::RenderTerrainGrid( float x1,float y1,float x2,float y2 )
{
	if (!m_engine)
		return;

	DisplayContext &dc = m_displayContext;

	float x,y;

	struct_VERTEX_FORMAT_P3F_TEX2F verts[4];
	memset( verts,0,sizeof(verts) );

	float step = MAX( y2-y1,x2-x1 );
	if (step < 0.1)
		return;
	step = step / 100.0f;
	
	//if (step > 2) step = 2;
	//x1 = (((int)x1)/2)*2;
	//y1 = (((int)y1)/2)*2;
	//x2 = (((int)x2)/2)*2;
	//y2 = (((int)y2)/2)*2;
	dc.SetColor( 1,1,1,1 );

	float z1 = m_engine->GetTerrainElevation( x1,y1 );
	float z2 = m_engine->GetTerrainElevation( x2,y2 );
	float z3 = m_engine->GetTerrainElevation( x2,y1 );
	float z4 = m_engine->GetTerrainElevation( x1,y2 );

	dc.DrawLine( Vec3(x1,y1,z1),Vec3(x1,y1,z1+0.5) );
	dc.DrawLine( Vec3(x2,y2,z2),Vec3(x2,y2,z2+0.5) );
	dc.DrawLine( Vec3(x2,y1,z3),Vec3(x2,y1,z3+0.5) );
	dc.DrawLine( Vec3(x1,y2,z4),Vec3(x1,y2,z4+0.5) );

	dc.SetColor( 1,0,0,0.5 );
	dc.DepthTestOff();
	
	float offset = 0.01f;

	/*
	for (y = y1; y < y2+step; y += step)
	{
		for (x = x1; x < x2+step; x += step)
		{
			verts[0].xyz.x = x;
			verts[0].xyz.y = y;
			verts[0].xyz.z = m_engine->GetTerrainElevation( x,y ) + offset;

			verts[1].xyz.x = x+step;
			verts[1].xyz.y = y;
			verts[1].xyz.z = m_engine->GetTerrainElevation( x+step,y ) + offset;

			verts[3].xyz.x = x+step;
			verts[3].xyz.y = y+step;
			verts[3].xyz.z = m_engine->GetTerrainElevation( x+step,y+step ) + offset;

			verts[2].xyz.x = x;
			verts[2].xyz.y = y+step;
			verts[2].xyz.z = m_engine->GetTerrainElevation( x,y+step ) + offset;
			
			//m_renderer->DrawLine( Vec3(x,y,z),Vec3(x+step,y,z) );
			//m_renderer->DrawLine( Vec3(x,y,z),Vec3(x,y+step,z) );
			/*
			//m_renderer->DrawLine( Vec3(x,y,z),Vec3(x+step,y,z) );
			//m_renderer->DrawLine( Vec3(x,y,z),Vec3(x,y+step,z) );
			m_renderer->DrawQuad( x+step*0.5f,y+step*0.5f,(z1+z2+z3+z4)/4 );

			float data[] = 
			{
				-dx+x, dy+y,-object_Scale4+z,  0, 0, // 1,1,1,alpha,//NOTE: totaly stupid
					dx+x,-dy+y,-object_Scale4+z,   0, 0, // 1,1,1,alpha,
					-dx+x, dy+y, object_Scale4+z,  -1, 1, // 1,1,1,alpha,
					dx+x,-dy+y, object_Scale4+z,   0, 1, // 1,1,1,alpha,
			};
			
			GetRenderer()->SetMaterialColor(1,1,1,alpha);
			GetRenderer()->SetTexture(tid);
			* /
			m_renderer->DrawTriStrip(&(CVertexBuffer(verts,VERTEX_FORMAT_P3F_TEX2F)),4);
		}
	}
	*/

	Vec3 p1,p2;
	// Draw yellow border lines.
	dc.SetColor( 1,1,0,1 );

	for (y = y1; y < y2+step; y += step)
	{
		p1.x = x1;
		p1.y = y;
		p1.z = m_engine->GetTerrainElevation( p1.x,p1.y ) + offset;

		p2.x = x1;
		p2.y = y+step;
		p2.z = m_engine->GetTerrainElevation( p2.x,p2.y ) + offset;
		m_renderer->DrawLine( p1,p2 );

		p1.x = x2+step;
		p1.y = y;
		p1.z = m_engine->GetTerrainElevation( p1.x,p1.y ) + offset;

		p2.x = x2+step;
		p2.y = y+step;
		p2.z = m_engine->GetTerrainElevation( p2.x,p2.y ) + offset;
		m_renderer->DrawLine( p1,p2 );
	}
	for (x = x1; x < x2+step; x += step)
	{
		p1.x = x;
		p1.y = y1;
		p1.z = m_engine->GetTerrainElevation( p1.x,p1.y ) + offset;

		p2.x = x+step;
		p2.y = y1;
		p2.z = m_engine->GetTerrainElevation( p2.x,p2.y ) + offset;
		m_renderer->DrawLine( p1,p2 );

		p1.x = x;
		p1.y = y2+step;
		p1.z = m_engine->GetTerrainElevation( p1.x,p1.y ) + offset;

		p2.x = x+step;
		p2.y = y2+step;
		p2.z = m_engine->GetTerrainElevation( p2.x,p2.y ) + offset;
		m_renderer->DrawLine( p1,p2 );
	}

	dc.DepthTestOn();
}

void CRenderViewport::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	/*CBaseObject *cameraObject = GetCameraObject();
	if (cameraObject)
	{
		// Find camera object
		// This is a camera object.
		float fov = gSettings.viewports.fDefaultFov;
		if (cameraObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
		{
			CCameraObject *camObj = (CCameraObject*)cameraObject;
				
			if (nChar == VK_UP)
			{
				fov = camObj->GetFOV();

				if (fov < gf_PI-0.1f)
					camObj->SetFOV(fov+0.02f);
			}
			else if (nChar == VK_DOWN)
			{
				fov = camObj->GetFOV();

				if (fov > 0.1f)
					camObj->SetFOV(fov-0.02f);
			}
			else if (nChar == VK_LEFT)
			{
				camObj->SetRotation(camObj->GetRotation()*Quat::CreateRotationY(-0.02f));
			}
			else if (nChar == VK_RIGHT)
			{
				camObj->SetRotation(camObj->GetRotation()*Quat::CreateRotationY(0.02f));
			}
		}
	}*/
	/*
	m_Camera.Update();
	Matrix m = m_Camera.GetMatrix();
	Vec3 zdir(m[0][2],m[1][2],m[2][2]);
	zdir.Normalize();
	Vec3 xdir(m[0][0],m[1][0],m[2][0]);
	xdir.Normalize();
		
	//m_Camera.SetPos( m_Camera.GetPos() + ydir*(m_mousePos.y-point.y),xdir*(m_mousePos.x-point.x) );
	Vec3 pos = GetViewerPos();

	if (nChar == VK_UP)
	{
		// move forward
		pos = pos - 0.5f*zdir;
		SetViewerPos( pos );
	}
	if (nChar == VK_DOWN)
	{
		// move backward
		pos = pos + 0.5f*zdir;
		SetViewerPos( pos );
	}
	if (nChar == VK_LEFT)
	{
		// move left
		pos = pos - 0.5f*xdir;
		SetViewerPos( pos );
	}
	if (nChar == VK_RIGHT)
	{
		// move right
		pos = pos + 0.5f*xdir;
		SetViewerPos( pos );
	}
	*/
	
	CViewport::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CRenderViewport::ProcessKeys()
{
//	if (m_PlayerControl==1)
//		return;
	if (m_PlayerControl)
		return;

	if (GetFocus() != this)
		return;

	//m_Camera.UpdateFrustum();
	Matrix34 m = GetViewTM();
	Vec3 ydir = m.GetColumn1().GetNormalized();
	Vec3 xdir = m.GetColumn0().GetNormalized();
		
	if (gSettings.bAlternativeCameraControls)
	{
		ydir = m.GetColumn1();
		ydir.z = 0;
		if (ydir.GetLength() > 0.01f)
			ydir.Normalize();
		else
			ydir = m.GetColumn2().GetNormalized();
	}
		
	//m_Camera.SetPos( m_Camera.GetPos() + ydir*(m_mousePos.y-point.y),xdir*(m_mousePos.x-point.x) );
	Vec3 pos = GetViewTM().GetTranslation();

	IConsole * console = GetIEditor()->GetSystem()->GetIConsole();

	float speedScale = 60.0f * GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();
	if (speedScale > 20) speedScale = 20;

	speedScale *= gSettings.cameraMoveSpeed;

	if (CheckVirtualKey(VK_SHIFT))
	{
		speedScale *= gSettings.cameraFastMoveSpeed;
	}

	bool bCtrl = CheckVirtualKey(VK_LCONTROL) || CheckVirtualKey(VK_RCONTROL);
	//bool bAlt = GetAsyncKeyState(VK_LALT) || GetAsyncKeyState(VK_RCONTROL);

	if (bCtrl)
	{
		if ( CheckVirtualKey(VK_UP))
		{
			m_CamFOV=m_Camera.GetFov();
			if (m_CamFOV < gf_PI-0.1f)
				m_CamFOV+=0.02f;
		}

		if ( CheckVirtualKey(VK_DOWN))
		{
			m_CamFOV=m_Camera.GetFov();
			if (m_CamFOV > 0.1f)
				m_CamFOV-=0.02f;
		}

		if ( CheckVirtualKey(VK_LEFT) || CheckVirtualKey(VK_RIGHT))
		{
			Matrix34 camtm = GetViewTM();
			Ang3 ypr = CCamera::CreateAnglesYPR( Matrix33(camtm) );
			
			ypr.z += CheckVirtualKey(VK_LEFT) ? -0.02f : 0.02;		// Roll camera

			camtm = Matrix34( CCamera::CreateOrientationYPR(ypr), camtm.GetTranslation() );
			SetViewTM( camtm );
		}

		return;
	}

  bool bIsPressedSome = false;

	if ( CheckVirtualKey(VK_UP) || CheckVirtualKey('W'))
	{
		// move forward
		bIsPressedSome = true;
		if(!m_nPresedKeyState)
			m_nPresedKeyState	=	1;
		pos = pos + speedScale*m_moveSpeed*ydir;
		m.SetTranslation(pos);
		SetViewTM(m);
	}

	if ( CheckVirtualKey(VK_DOWN) || CheckVirtualKey('S'))
	{
		// move backward
		bIsPressedSome = true;
		if(!m_nPresedKeyState)
			m_nPresedKeyState	=	1;
		pos	=	pos	-	speedScale*m_moveSpeed*ydir;
		m.SetTranslation(pos);
		SetViewTM(m);
	}

	if ( CheckVirtualKey(VK_LEFT) || CheckVirtualKey('A'))
	{
		// move left
		bIsPressedSome = true;
		if(!m_nPresedKeyState)
			m_nPresedKeyState	=	1;
		pos	=	pos	-	speedScale*m_moveSpeed*xdir;
		m.SetTranslation(pos);
		SetViewTM(m);
	}

	if ( CheckVirtualKey(VK_RIGHT) || CheckVirtualKey('D'))
	{
		// move right
		bIsPressedSome = true;
		if(!m_nPresedKeyState)
			m_nPresedKeyState	=	1;
		pos	=	pos	+	speedScale*m_moveSpeed*xdir;
		m.SetTranslation(pos);
		SetViewTM(m);
	}

	if (CheckVirtualKey(VK_RBUTTON) | CheckVirtualKey(VK_MBUTTON))
	{
		bIsPressedSome = true;
	}

	if(!bIsPressedSome)
		m_nPresedKeyState=0;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CRenderViewport::WorldToView3D( Vec3 wp,int nFlags )
{
	Vec3 out(0,0,0);
	float x,y,z;

	SetCurrentContext();
	m_renderer->ProjectToScreen( wp.x,wp.y,wp.z,&x,&y,&z );
	if (_finite(x) && _finite(y) && _finite(z))
	{
		out.x = (x / 100) * m_rcClient.Width();
		out.y = (y / 100) * m_rcClient.Height();
		out.z = z;
	}
	return out;
}

//////////////////////////////////////////////////////////////////////////
CPoint CRenderViewport::WorldToView( Vec3 wp )
{
	CPoint p;
	float x,y,z;

	SetCurrentContext();
	m_renderer->ProjectToScreen( wp.x,wp.y,wp.z,&x,&y,&z );
	if (_finite(x) || _finite(y))
	{
		p.x = (x / 100) * m_rcClient.Width();
		p.y = (y / 100) * m_rcClient.Height();
	}
	else
		CPoint(0,0);
	return p;
}

//////////////////////////////////////////////////////////////////////////
Vec3	CRenderViewport::ViewToWorld( CPoint vp,bool *collideWithTerrain,bool onlyTerrain )
{
	if (!m_renderer)
		return Vec3(0,0,0);

	SetCurrentContext();

	CRect rc = m_rcClient;

	Vec3 pos0,pos1;
	float wx,wy,wz;
	m_renderer->UnProjectFromScreen( vp.x,rc.bottom-vp.y,0,&wx,&wy,&wz );
	if (!_finite(wx) || !_finite(wy) || !_finite(wz))
		return Vec3(0,0,0);
	pos0( wx,wy,wz );
	if (!IsVectorInValidRange(pos0))
		pos0.Set(0,0,0);

	m_renderer->UnProjectFromScreen( vp.x,rc.bottom-vp.y,1,&wx,&wy,&wz );
	if (!_finite(wx) || !_finite(wy) || !_finite(wz))
		return Vec3(0,0,0);
	pos1( wx,wy,wz );

	Vec3 v = (pos1-pos0);
	if (!IsVectorInValidRange(pos1))
		pos1.Set(1,0,0);

	v = v.GetNormalized();
	v = v*2000.0f;

	if (!_finite(v.x) || !_finite(v.y) || !_finite(v.z))
		return Vec3(0,0,0);

	Vec3 colp(0,0,0);

	IPhysicalWorld *world = GetIEditor()->GetSystem()->GetIPhysicalWorld();
	if (!world)
		return colp;
	
	Vec3 vPos(pos0.x,pos0.y,pos0.z);
	Vec3 vDir(v.x,v.y,v.z);
	int flags = rwi_stop_at_pierceable|rwi_ignore_terrain_holes;
	ray_hit hit;

	CSelectionGroup *sel = GetIEditor()->GetSelection();
	m_numSkipEnts = 0;
	for (int i = 0; i < sel->GetCount() && m_numSkipEnts < 32; i++)
	{
		m_pSkipEnts[m_numSkipEnts++] = sel->GetObject(i)->GetCollisionEntity();
	}

	int col = 0;
	for(int chcnt = 0; chcnt < 3; chcnt++)
	{
		hit.pCollider = 0;
		col = world->RayWorldIntersection( vPos,vDir,ent_all,flags,&hit,1,m_pSkipEnts, m_numSkipEnts);
		if (col == 0)
			break; // No collision.

		if (hit.bTerrain)
			break;
		
		if (onlyTerrain || GetIEditor()->IsTerrainAxisIgnoreObjects())
		{
			//if(hit.pCollider->GetiForeignData()==PHYS_FOREIGN_ID_TERRAIN) // If voxel.
			if(hit.pCollider->GetiForeignData()==PHYS_FOREIGN_ID_STATIC) // If voxel.
			{
				IRenderNode * pNode = (IRenderNode *) hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
				if(pNode && pNode->GetRenderNodeType() == eERType_VoxelObject)
					break;
			}
		}
		else
			break;

		if(m_numSkipEnts>64)
			break;
		m_pSkipEnts[m_numSkipEnts++] = hit.pCollider;

		if (!hit.pt.IsZero()) // Advance ray.
			vPos = hit.pt;
	}

	if (collideWithTerrain)
		*collideWithTerrain = hit.bTerrain;
	
	if (hit.dist > 0)
	{ 
		if (hit.pCollider != 0 && !hit.bTerrain)
		{
			//pe_status_pos statusPos;
			//hit.pCollider->GetStatus( &statusPos );
			//BBox box( statusPos.BBox[0],statusPos.BBox[1] );
		}
		colp = hit.pt;
		if (hit.bTerrain)
		{
			colp.z = m_engine->GetTerrainElevation( colp.x,colp.y );
		}
	}

	return colp;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CRenderViewport::ViewToWorldNormal( CPoint vp, bool onlyTerrain )
{
	if (!m_renderer)
		return Vec3(0,0,1);

	SetCurrentContext();

	CRect rc = m_rcClient;

	Vec3 pos0,pos1;
	float wx,wy,wz;
	m_renderer->UnProjectFromScreen( vp.x,rc.bottom-vp.y,0,&wx,&wy,&wz );
	if (!_finite(wx) || !_finite(wy) || !_finite(wz))
		return Vec3(0,0,1);
	pos0( wx,wy,wz );
	if (!IsVectorInValidRange(pos0))
		pos0.Set(0,0,0);

	m_renderer->UnProjectFromScreen( vp.x,rc.bottom-vp.y,1,&wx,&wy,&wz );
	if (!_finite(wx) || !_finite(wy) || !_finite(wz))
		return Vec3(0,0,1);
	pos1( wx,wy,wz );

	Vec3 v = (pos1-pos0);
	if (!IsVectorInValidRange(pos1))
		pos1.Set(1,0,0);

	v = v.GetNormalized();
	v = v*2000.0f;

	if (!_finite(v.x) || !_finite(v.y) || !_finite(v.z))
		return Vec3(0,0,1);

	Vec3 colp(0,0,0);

	IPhysicalWorld *world = GetIEditor()->GetSystem()->GetIPhysicalWorld();
	if (!world)
		return colp;
	
	Vec3 vPos(pos0.x,pos0.y,pos0.z);
	Vec3 vDir(v.x,v.y,v.z);
	int flags = rwi_stop_at_pierceable|rwi_ignore_terrain_holes;
	ray_hit hit;

	CSelectionGroup *sel = GetIEditor()->GetSelection();
	m_numSkipEnts = 0;
	for (int i = 0; i < sel->GetCount(); i++)
	{
		m_pSkipEnts[m_numSkipEnts++] = sel->GetObject(i)->GetCollisionEntity();
		if(m_numSkipEnts>1023)
			break;
	}

	int col = 1;
	while(col)
	{
		hit.pCollider = 0;
		col = world->RayWorldIntersection( vPos,vDir,ent_terrain | ent_static,flags,&hit,1, m_pSkipEnts, m_numSkipEnts);
		if(hit.dist > 0)
		{
			if( onlyTerrain || GetIEditor()->IsTerrainAxisIgnoreObjects())
			{
				//if(hit.pCollider->GetiForeignData()==PHYS_FOREIGN_ID_TERRAIN)
				if(hit.pCollider->GetiForeignData()==PHYS_FOREIGN_ID_STATIC) // If voxel.
				{
					IRenderNode * pNode = (IRenderNode *) hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
					if(pNode && pNode->GetRenderNodeType() == eERType_VoxelObject)
						break;
				}
			}
			else
				break;
			m_pSkipEnts[m_numSkipEnts++] = hit.pCollider;
		}
		if(m_numSkipEnts>1023)
			break;
	}
	return hit.n;
}


//////////////////////////////////////////////////////////////////////////
void	CRenderViewport::ViewToWorldRay( CPoint vp,Vec3 &raySrc,Vec3 &rayDir )
{
	if (!m_renderer)
		return;

	CRect rc = m_rcClient;

	SetCurrentContext();

	Vec3 pos0,pos1;
	float wx,wy,wz;
	m_renderer->UnProjectFromScreen( vp.x,rc.bottom-vp.y,0,&wx,&wy,&wz );
	if (!_finite(wx) || !_finite(wy) || !_finite(wz))
		return;
	if (fabs(wx) > 1000000 || fabs(wy) > 1000000 || fabs(wz) > 1000000)
		return;
	pos0( wx,wy,wz );
	m_renderer->UnProjectFromScreen( vp.x,rc.bottom-vp.y,1,&wx,&wy,&wz );
	if (!_finite(wx) || !_finite(wy) || !_finite(wz))
		return;
	if (fabs(wx) > 1000000 || fabs(wy) > 1000000 || fabs(wz) > 1000000)
		return;
	pos1( wx,wy,wz );

	Vec3 v = (pos1-pos0);
	v = v.GetNormalized();

	raySrc = pos0;
	rayDir = v;
}

//////////////////////////////////////////////////////////////////////////
float CRenderViewport::GetScreenScaleFactor( const Vec3 &worldPoint )
{
	float dist = m_Camera.GetPosition().GetDistance( worldPoint );
	if (dist < m_Camera.GetNearPlane())
		dist = m_Camera.GetNearPlane();
	return dist;
}

BOOL CRenderViewport::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_hCurrCursor == NULL && !m_cursorStr.IsEmpty())
	{
		m_cursorStr = "";
		// Display cursor string.
	}
	return CViewport::OnSetCursor(pWnd, nHitTest, message);
}

void CRenderViewport::OnDestroy()
{
	DestroyRenderContext();
	CViewport::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::DrawTextLabel( DisplayContext &dc,const Vec3& pos,float size,const ColorF& color,const char *text, const bool bCenter )
{
	float col[4] = { color.r,color.g,color.b,color.a };
	m_renderer->DrawLabelEx( pos,size,col,true,true,text );
}

//////////////////////////////////////////////////////////////////////////
bool CRenderViewport::HitTest( CPoint point,HitContext &hitInfo )
{
	hitInfo.camera = &m_Camera;

	return CViewport::HitTest( point,hitInfo );
}

//////////////////////////////////////////////////////////////////////////
bool CRenderViewport::IsBoundsVisible( const BBox &box ) const
{
	// If at least part of bbox is visible then its visible.
	return m_Camera.IsAABBVisible_F( AABB(box.min,box.max) );
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::CenterOnSelection()
{
	if (!GetIEditor()->GetSelection()->IsEmpty())
	{
		CSelectionGroup *sel = GetIEditor()->GetSelection();
		BBox bounds = sel->GetBounds();
		Vec3 selPos = sel->GetCenter();
		float size = (bounds.max-bounds.min).GetLength();
		if (size == 0)
			return;
		if (size > 10.0f)
			size = 10.0f;
		Vec3 pos = selPos;
		pos += Vec3(0,size*2,size);
		Vec3 dir = (selPos-pos).GetNormalized();

		//pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y)+5;
	//	Matrix34 tm = CCamera::CreateMatrix( pos,dir,0 );
		Matrix34 tm = Matrix34( Matrix33::CreateRotationVDir(dir), pos);
		if (GetIEditor()->GetViewManager()->GetGameViewport())
			GetIEditor()->GetViewManager()->GetGameViewport()->SetViewTM(tm);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CRenderViewport::CreateRenderContext()
{
	// Create context.
	if (m_renderer && !m_bRenderContextCreated)
	{
		m_bRenderContextCreated = true;
		m_renderer->CreateContext( m_hWnd );
		// Make main context current.
		m_renderer->SetCurrentContext(m_hWnd);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::DestroyRenderContext()
{
	// Destroy render context.
	if (m_renderer && m_bRenderContextCreated)
	{
		// Do not delete primary context.
		if (m_hWnd != m_renderer->GetHWND())
			m_renderer->DeleteContext(m_hWnd);
		m_bRenderContextCreated = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::SetDefaultCamera()
{
	m_bSequenceCamera = false;
	SetCameraObject(0);
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::SetSequenceCamera()
{
	m_bSequenceCamera = true;
	SetCameraObject(0);
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::SetSelectedCamera()
{
	CBaseObject *pObject = GetIEditor()->GetSelectedObject();
	if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
		SetCameraObject( pObject );
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::RenderConstructionPlane()
{
	DisplayContext &dc = m_displayContext;

	int prevState = dc.GetState();
	dc.DepthWriteOff();
	// Draw Construction plane.

	RefCoordSys coordSys = GetIEditor()->GetReferenceCoordSys();

	Vec3 p = m_constructionMatrix[coordSys].GetTranslation();
	Vec3 n = m_constructionPlane.n;
	Vec3 dir = Vec3( 1,0,0 );
	if (dir.IsEquivalent(n) || dir.IsEquivalent(-n))
		dir = Vec3(0,1,0);
	//Vec3 u = (dir.Cross(n)).GetNormalized();
	//Vec3 v = (u.Cross(n)).GetNormalized();
	Vec3 u = m_constructionPlaneAxisX;
	Vec3 v = m_constructionPlaneAxisY;
	
	CGrid *pGrid = GetViewManager()->GetGrid();
	float step = pGrid->scale * pGrid->size;
	float size = gSettings.snap.constructPlaneSize;

	dc.SetColor( 1,0,1,0.1f );
	float s = size;
	dc.DrawQuad( p-u*s-v*s, p+u*s-v*s,p+u*s+v*s,p-u*s+v*s );
	
	// Draw X lines.
	dc.SetColor( 1,0,0.2f,0.3f );
	for (s = -size; s < size+step; s += step)
	{
		dc.DrawLine( p-u*size+v*s,p+u*size+v*s );
	}

	// Draw Y lines.
	dc.SetColor( 0.2f,1.0f,0,0.3f );
	for (s = -size; s < size+step; s += step)
	{
		dc.DrawLine( p-v*size+u*s,p+v*size+u*s );
	}

	// Draw origin lines.
	dc.SetLineWidth(2);
	
	//X
	dc.SetColor( 1,0,0 );
	dc.DrawLine( p-u*s,p+u*s );
	//Y
	dc.SetColor( 0,1,0 );
	dc.DrawLine( p-v*s,p+v*s );
	//Z
	dc.SetColor( 0,0,1 );
	dc.DrawLine( p-n*s,p+n*s );

	dc.SetLineWidth(0);

	dc.SetState(prevState);
}

//////////////////////////////////////////////////////////////////////////
void CRenderViewport::SetCurrentContext()
{
	//if (m_currentContextWnd != m_hWnd)
	{
		m_currentContextWnd = m_hWnd;
		m_renderer->SetCurrentContext(m_hWnd);
		m_renderer->ChangeViewport(0,0,m_rcClient.right,m_rcClient.bottom);
		m_renderer->SetCamera( m_Camera );
	}
}
