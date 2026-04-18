// PreviewModelCtrl.cpp : implementation file
//

#include "stdafx.h"

#include "Material/Material.h"
#include "PreviewModelCtrl.h"

#include <I3DEngine.h>
#include <IEntitySystem.h>
#include <ICryAnimation.h>
#include <IEntityRenderState.h>
#include <IRenderAuxGeom.h>
#include <ParticleParams.h>


/////////////////////////////////////////////////////////////////////////////
// CPreviewModelCtrl
CPreviewModelCtrl::CPreviewModelCtrl()
{
	m_bShowObject = true;
	m_renderer = 0;
	m_engine = 0;
	m_object = 0;
	m_character = 0;
	m_entity = 0;
	m_nTimer = 0;
	m_pEmitter = 0;
	m_size(0,0,0);

	m_bRotate = false;
	m_rotateAngle = 0;

	m_renderer = GetIEditor()->GetRenderer();
	m_engine = GetIEditor()->Get3DEngine();
  m_pAnimationSystem = GetIEditor()->GetSystem()->GetIAnimationSystem();

	m_fov = 60;
	m_camera.SetFrustum( 800,600, DEG2RAD(m_fov), 0.02f,10000.0f );

//	m_camera.SetFov( DEG2RAD(m_fov) );
//	m_camera.SetZMin( 0.02f );
//	m_camera.SetZMax( 10000 );

	m_bInRotateMode = false;
	m_bInMoveMode = false;

	CDLight l;
  l.m_Origin = Vec3(100,-100,100);
	float L = 1.0f;
	l.SetLightColor(ColorF(L,L,L,1));
	l.m_SpecMult = 1.0f;
	l.m_fRadius = 1000;
	l.m_fStartRadius = 0;
	l.m_fEndRadius = 1000;
  l.m_Flags |= DLF_POINT;
	m_lights.push_back( l );

	/*
	l.m_Origin = Vec3(-10,-10,-10);
	l.SetLightColor(ColorF(L,L,L,1));
	l.m_SpecColor.r = L; l.m_SpecColor.g = L; l.m_SpecColor.b = L; l.m_SpecColor.a = 1;
  l.m_fRadius = 1000;
	l.m_fStartRadius = 0;
	l.m_fEndRadius = 1000;
  l.m_Flags |= DLF_POINT;
	m_lights.push_back( l );
	*/

	m_bUseBacklight = false;

	m_bContextCreated = false;

	m_bHaveAnythingToRender = false;
	m_bGrid = true;
	m_bAxis = true;
	m_bUpdate = false;

	m_camAngles.Set(0,0,0);

	m_clearColor.Set(0.5f,0.5f,0.5f);

	m_aabb.min = Vec3(-2,-2,-2);
	m_aabb.max = Vec3(2,2,2);
	SetCameraLookAt( 2.0f,Vec3(1,1,-0.5) );
	
	GetIEditor()->RegisterNotifyListener(this);
}

CPreviewModelCtrl::~CPreviewModelCtrl()
{
	ReleaseObject();
	GetIEditor()->UnregisterNotifyListener(this);
	/*
	IRenderer *pr = CSystem::Instance()->SetCurrentRenderer( m_renderer );
	I3DEngine *pe = CSystem::Instance()->SetCurrent3DEngine( m_3dEngine );

	if (m_renderer)
		m_renderer->ShutDown();
	m_renderer = 0;

		//delete m_renderer;
	/*
	if (m_3dEngine)
		m_3dEngine->Release3DEngine();
	*/
/*
	CSystem::Instance()->SetCurrentRenderer( pr );
	CSystem::Instance()->SetCurrent3DEngine( pe );
	*/
}


BEGIN_MESSAGE_MAP(CPreviewModelCtrl, CWnd)
	//{{AFX_MSG_MAP(CPreviewModelCtrl)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CPreviewModelCtrl message handlers

//////////////////////////////////////////////////////////////////////////
BOOL CPreviewModelCtrl::Create( CWnd *pWndParent,const CRect &rc,DWORD dwStyle )
{
	BOOL bReturn = CreateEx( NULL, AfxRegisterWndClass(CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW|CS_OWNDC, 
		AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL), NULL,dwStyle,
		rc, pWndParent, NULL);

	return bReturn;
}

//////////////////////////////////////////////////////////////////////////
int CPreviewModelCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CPreviewModelCtrl::CreateContext()
{
	// Create context.
	if (m_renderer && !m_bContextCreated)
	{
		m_bContextCreated = true;
    m_renderer->CreateContext( m_hWnd );
		// Make main context current.
		m_renderer->MakeCurrent();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::PreSubclassWindow()
{
	CWnd::PreSubclassWindow();

	/*
	// Create context.
	if (m_renderer && !m_bContextCreated)
	{
		m_bContextCreated = true;
    m_renderer->CreateContext( m_hWnd );
		// Make main context current.
		m_renderer->MakeCurrent();
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::ReleaseObject()
{
	SAFE_RELEASE(m_object);
	SAFE_RELEASE(m_character);
	if (m_pEmitter)
	{
		m_pEmitter->Activate(false);
		m_pEmitter->Release();
		m_pEmitter = 0;
	}
	m_entity = 0;
	m_bHaveAnythingToRender = false;
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::LoadFile( const CString &modelFile,bool changeCamera )
{
	m_bHaveAnythingToRender = false;
	if (!m_hWnd)
		return;
	if (!m_renderer)
		return;

	ReleaseObject();

	if (modelFile.IsEmpty())
	{
		if (m_nTimer != 0)
			KillTimer(m_nTimer);
		m_nTimer = 0;
		Invalidate();
		return;
	}

	m_loadedFile = modelFile;

	if (stricmp( Path::GetExt(modelFile),"cga" ) == 0)
	{
		// Load CGA animated object.
		m_character = m_pAnimationSystem->CreateInstance( modelFile );
		if(!m_character)
		{
			Warning( "Loading of geometry object %s failed.",(const char*)modelFile );
			if (m_nTimer != 0)
				KillTimer(m_nTimer);
			m_nTimer = 0;
			Invalidate();
			return;
		}
		m_character->AddRef();
		m_aabb = m_character->GetAABB();
	}
	else if (stricmp( Path::GetExt(modelFile),CRY_CHARACTER_FILE_EXT ) == 0 ||
		stricmp( Path::GetExt(modelFile),CRY_CHARACTER_DEFINITION_FILE_EXT) == 0 ||
		stricmp( Path::GetExt(modelFile),CRY_ANIM_GEOMETRY_FILE_EXT) == 0
		)
	{
		// Load character.
		m_character = m_pAnimationSystem->CreateInstance( modelFile );
		if(!m_character)
		{
			Warning( "Loading of character %s failed.",(const char*)modelFile );
			if (m_nTimer != 0)
				KillTimer(m_nTimer);
			m_nTimer = 0;
			Invalidate();
			return;
		}
		m_character->AddRef();
		m_aabb = m_character->GetAABB();
	}
	else
	{
		// Load object.
		m_object = m_engine->LoadStatObj( modelFile );
		if(!m_object)
		{
			Warning( "Loading of geometry object %s failed.",(const char*)modelFile );
			if (m_nTimer != 0)
				KillTimer(m_nTimer);
			m_nTimer = 0;
			Invalidate();
			return;
		}
		m_object->AddRef();
		m_aabb.min = m_object->GetBoxMin();
		m_aabb.max = m_object->GetBoxMax();
	}

	m_bHaveAnythingToRender = true;

	// No timer.
	/*
	if (m_nTimer == 0)
		m_nTimer = SetTimer(1,200,NULL);
	*/
	
	if (changeCamera)
	{
		SetCameraLookAt( 2.0f,Vec3(1,1,-0.5) );
	}
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::LoadParticleEffect( IParticleEffect *pEffect )
{
	m_bHaveAnythingToRender = false;
	if (!m_hWnd)
		return;
	if (!m_renderer)
		return;

	ReleaseObject();

	if (!pEffect)
		return;

	RECT rc;
	GetClientRect(&rc);
	if (rc.bottom-rc.top < 2 || rc.right-rc.left < 2)
		return;

	Matrix34 tm;
	tm.SetIdentity();
	tm.SetRotationXYZ( Ang3(DEG2RAD(90.0f),0,0) );

	m_bHaveAnythingToRender = true;
	m_pEmitter = pEffect->Spawn( false,tm );
	m_pEmitter->AddRef();

	SpawnParams sp;
	sp.bIgnoreLocation = true;
	m_pEmitter->SetSpawnParams(sp);

	m_aabb.min = Vec3(-5);
	m_aabb.max = Vec3(5);

	//if (changeCamera)
	{
		//SetCameraLookAt( 2.0f,Vec3(1,1,0.5) );
	}
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::SetEntity( IRenderNode *entity )
{
	m_bHaveAnythingToRender = false;
	if (m_entity != entity)
	{
		m_entity = entity;
		if (m_entity)
		{
			m_bHaveAnythingToRender = true;
			AABB box = m_entity->GetBBox();
			m_aabb.min = box.min;
			m_aabb.max = box.max;
		}
		Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::SetObject( IStatObj *pObject )
{
	if (m_object != pObject)
	{
		m_bHaveAnythingToRender = false;
		m_object = pObject;
		if (m_object)
		{
			m_bHaveAnythingToRender = true;
			m_aabb.min = m_object->GetBoxMin();
			m_aabb.max = m_object->GetBoxMax();
		}
		Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::SetCameraRadius( float fRadius )
{
	m_camRadius = fRadius;

	Matrix34 m = m_camera.GetMatrix();
	Vec3 dir = m.TransformVector( Vec3(0,1,0) );
	Matrix34 tm = Matrix33::CreateRotationVDir(dir,0);
	tm.SetTranslation( m_camTarget - dir*m_camRadius );
	m_camera.SetMatrix(tm);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::SetCameraLookAt( float fRadiusScale,const Vec3 &fromDir )
{
	AABB box(m_aabb);
	Vec3 v = box.max - box.min;
	float radius = v.GetLength()/2.0f;

	m_camTarget = (box.max + box.min) * 0.5f;
	m_camRadius = radius*fRadiusScale;

	Vec3 dir = fromDir.GetNormalized();
	Matrix34 tm = Matrix33::CreateRotationVDir(dir,0);
	tm.SetTranslation( m_camTarget - dir*m_camRadius );
	m_camera.SetMatrix(tm);
}

CCamera& CPreviewModelCtrl::GetCamera()
{
	return m_camera;
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::UseBackLight( bool bEnable )
{
	if (bEnable)
	{
		m_lights.resize(1);
		CDLight l;
		l.m_Origin = Vec3(-100,100,-100);
		float L = 0.5f;
		l.SetLightColor(ColorF(L,L,L,1));
		l.m_SpecMult = 1.0f;
		l.m_fRadius = 1000;
		l.m_fStartRadius = 0;
		l.m_fEndRadius = 1000;
		l.m_Flags |= DLF_POINT;
		m_lights.push_back( l );
	}
	else
	{
		m_lights.resize(1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// TODO: Add your message handler code here
	
	// Do not call CWnd::OnPaint() for painting messages
	bool res = Render();
	if (!res)
	{
		RECT rect;
		// Get the rect of the client window
		GetClientRect(&rect);
		
		// Create the brush
		CBrush cFillBrush;
		cFillBrush.CreateSolidBrush(RGB(128,128,128));
		
		// Fill the entire client area
		dc.FillRect(&rect, &cFillBrush);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CPreviewModelCtrl::OnEraseBkgnd(CDC* pDC) 
{
	if (m_bHaveAnythingToRender)
		return TRUE;

	return CWnd::OnEraseBkgnd(pDC);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::SetCamera( CCamera &cam )
{
	m_camera.SetPosition( cam.GetPosition() );
	//m_camera.SetAngle( cam.GetAngles() );

	CRect rc;
	GetClientRect(rc);
	//m_camera.SetFov(m_stdFOV);
	int w = rc.Width();
	int h = rc.Height();
	//float proj = (float)h/(float)w;
	//if (proj > 1.2f) proj = 1.2f;
	m_camera.SetFrustum( w,h, DEG2RAD(m_fov), m_camera.GetNearPlane(),m_camera.GetFarPlane() );

	//m_camera.UpdateFrustum();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::SetOrbitAngles( const Ang3& ang )
{
	assert(0);
}

//////////////////////////////////////////////////////////////////////////
bool CPreviewModelCtrl::Render()
{
	if (!m_bHaveAnythingToRender)
	{
		//return false;
	}

	CRect rc;
	GetClientRect(rc);
	if (rc.bottom-rc.top < 2 || rc.right-rc.left < 2)
		return false;

	if (!m_bContextCreated)
	{
		if (!CreateContext())
			return false;
	}

	//IRenderer *pr = CSystem::Instance()->SetCurrentRenderer( m_renderer );
	SetCamera( m_camera );

	m_renderer->SetCurrentContext( m_hWnd );
	m_renderer->ChangeViewport(0,0,rc.right,rc.bottom);
	m_renderer->SetClearColor( Vec3(m_clearColor.r,m_clearColor.g,m_clearColor.b) );
	m_renderer->BeginFrame();

	m_renderer->SetCamera( m_camera );

	// Render grid. Explicitly clear color and depth buffer first
	// (otherwise ->EndEf3D() will do that and thereby clear the grid).
	m_renderer->ClearBuffer(FRT_CLEAR, &m_clearColor);
	if (m_bGrid || m_bAxis)
	{
		DrawGrid();
		m_renderer->GetIRenderAuxGeom()->Flush(false);
	}

	// Render object.
	m_renderer->EF_StartEf();
	m_renderer->ResetToDefault();

	m_renderer->SetPolygonMode( R_SOLID_MODE );

// Add lights.
	for (int i = 0; i < m_lights.size(); i++)
	{
		m_renderer->EF_ADDDlight( &m_lights[i] );
	}

	_smart_ptr<IMaterial> pMaterial;
	if (m_pCurrentMaterial)
		pMaterial = m_pCurrentMaterial->GetMatInfo();
		
	SRendParams rp;
	rp.nDLightMask = 0x3;
	rp.AmbientColor = ColorF(1,1,1,1);
  rp.dwFObjFlags |= FOB_TRANS_MASK;
	rp.pMaterial = pMaterial;

	Matrix34 tm;
	tm.SetIdentity();
	rp.pMatrix = &tm;

	if (m_bRotate)
	{
		tm.SetRotationXYZ(Ang3( 0,0,m_rotateAngle ));
		m_rotateAngle += 0.1f;
	}

	if (m_bShowObject)
	{
		if (m_object)
			m_object->Render( rp );

		if (m_entity)
			m_entity->Render( rp );

		if (m_character)
			m_character->Render( rp, QuatTS(IDENTITY)  );

		if (m_pEmitter)
		{
			m_pEmitter->Update();
			m_pEmitter->Render( rp );
		}
	}

	m_renderer->EF_EndEf3D(SHDF_SORT | SHDF_NOASYNC);

	m_renderer->FlushTextMessages();

	m_renderer->RenderDebug();
	m_renderer->EndFrame();

	// Restore main context.
	m_renderer->MakeCurrent();
	
	return true;
}

void CPreviewModelCtrl::DrawGrid()
{
	// Draw grid.
	float step = 0.1f;
	float XR = 5;
	float YR = 5;

	IRenderAuxGeom * pRag = m_renderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags nRendFlags = pRag->GetRenderFlags();

	pRag->SetRenderFlags( e_Def3DPublicRenderflags );
	SAuxGeomRenderFlags nNewFlags = pRag->GetRenderFlags();
	nNewFlags.SetAlphaBlendMode( e_AlphaBlended );
	pRag->SetRenderFlags( nNewFlags );

	int nGridAlpha = 40;
	if (m_bGrid)
	{
		// Draw grid.
		for (float x = -XR; x < XR; x+=step)
		{
			if (fabs(x) > 0.01)
				//m_renderer->DrawLine( Vec3(x,-YR,0),Vec3(x,YR,0) );
				pRag->DrawLine( Vec3(x,-YR,0), ColorB(150, 150, 150 , nGridAlpha), Vec3(x,YR,0) , ColorB(150, 150, 150 , nGridAlpha) );
		}
		for (float y = -YR; y < YR; y+=step)
		{
			if (fabs(y) > 0.01)
				//m_renderer->DrawLine( Vec3(-XR,y,0),Vec3(XR,y,0) );
				pRag->DrawLine( Vec3(-XR,y,0), ColorB(150, 150, 150 , nGridAlpha), Vec3(XR,y,0) , ColorB(150, 150, 150 , nGridAlpha) );
		}

	}

	nGridAlpha = 60;
	if (m_bAxis)
	{
		// Draw axis.
		//m_renderer->SetMaterialColor( 1,0,0,1.0f );
		//m_renderer->DrawLine( Vec3(-XR,0,0),Vec3(XR,0,0) );
		pRag->DrawLine( Vec3(0,0,0), ColorB(255, 0, 0 ,nGridAlpha), Vec3(XR,0,0) , ColorB(255, 0, 0 ,nGridAlpha) );

		//m_renderer->SetMaterialColor( 0,1,0,1.0f );
		//m_renderer->DrawLine( Vec3(0,-YR,0),Vec3(0,YR,0) );
		pRag->DrawLine( Vec3(0,0,0), ColorB(0, 255, 0 ,nGridAlpha), Vec3(0,YR,0) , ColorB(0, 255, 0 ,nGridAlpha) );

		//m_renderer->SetMaterialColor( 0,0,1,1.0f );
		//m_renderer->DrawLine( Vec3(0,0,-YR),Vec3(0,0,YR) );
		pRag->DrawLine(Vec3(0,0,0), ColorB(0, 0, 255 ,nGridAlpha), Vec3(0,0,YR) , ColorB(0, 0, 255 ,nGridAlpha) );
	}
	pRag->SetRenderFlags( nRendFlags );
}

void CPreviewModelCtrl::UpdateAnimation()
{
	if (m_character)
	{
		GetISystem()->GetIAnimationSystem()->Update();
		m_character->GetISkeletonPose()->SetForceSkeletonUpdate(0);
		//Matrix34 tm;
		//tm.SetIdentity();
		//m_character->Update(tm, m_camera);
		m_character->SkeletonPreProcess(QuatT(IDENTITY), QuatTS(IDENTITY), m_camera,0 );
		m_character->SkeletonPostProcess(QuatT(IDENTITY), QuatTS(IDENTITY), 0, 1.0f,0 );
		m_aabb = m_character->GetAABB();
	}
}

void CPreviewModelCtrl::OnTimer(UINT_PTR nIDEvent) 
{
	if (IsWindowVisible())
	{
		if (m_bHaveAnythingToRender)
			Invalidate();
	}
	
	CWnd::OnTimer(nIDEvent);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::DeleteRenderContex()
{
	ReleaseObject();

	// Destroy render context.
	if (m_renderer && m_bContextCreated)
	{
		m_renderer->DeleteContext(m_hWnd);
		m_bContextCreated = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::OnDestroy() 
{
	DeleteRenderContex();

	CWnd::OnDestroy();
	
	if (m_nTimer)
		KillTimer( m_nTimer );
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::OnLButtonDown(UINT nFlags, CPoint point) 
{
	m_bInRotateMode = true;
	m_mousePos = point;
	SetFocus();
	if (!m_bInMoveMode)
		SetCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnLButtonUp(UINT nFlags, CPoint point) 
{
	m_bInRotateMode = false;
	if (!m_bInMoveMode)
		ReleaseCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnMButtonDown(UINT nFlags, CPoint point) 
{
	m_bInRotateMode = true;
	m_bInMoveMode = true;
	m_mousePos = point;
	//if (!m_bInMoveMode)
	SetCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnMButtonUp(UINT nFlags, CPoint point) 
{
	m_bInRotateMode = false;
	m_bInMoveMode = false;
	ReleaseCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnMouseMove(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	CWnd::OnMouseMove(nFlags, point);

	if (point == m_mousePos)
		return;

	if (m_bInRotateMode && m_bInMoveMode)
	{
		// Zoom.
		Matrix34 m = m_camera.GetMatrix();
		//Vec3 xdir(m[0][0],m[1][0],m[2][0]);
		Vec3 xdir(0,0,0);
		//xdir.Normalize();

		Vec3 zdir = m.GetColumn1().GetNormalized();

		float step = 0.002f;
		float dx = (point.x-m_mousePos.x);
		float dy = (point.y-m_mousePos.y);
//		dx = pow(dx,1.05f );
		//dy = pow(dy,1.05f );
		//m_camera.SetPosition( m_camera.GetPos() + ydir*(m_mousePos.y-point.y),xdir*(m_mousePos.x-point.x) );
		m_camera.SetPosition( m_camera.GetPosition() + step*xdir*dx +  step*zdir*dy );
		SetCamera( m_camera );

		CPoint pnt = m_mousePos;
		ClientToScreen( &pnt );
		SetCursorPos( pnt.x,pnt.y );
		Invalidate();
	}
	else if (m_bInRotateMode)
	{
		m_camRadius = Vec3(m_camera.GetMatrix().GetTranslation()-m_camTarget).GetLength();
		// Look
		Ang3 angles( -point.y+m_mousePos.y,0,-point.x+m_mousePos.x );
		angles = angles*0.002f;

		Matrix34 camtm = m_camera.GetMatrix();
		Matrix33 Rz = Matrix33::CreateRotationXYZ(Ang3(0,0,angles.z)); // Rotate around vertical axis.
		Matrix33 Rx = Matrix33::CreateRotationAA(angles.x,camtm.GetColumn0()); // Rotate with angle around x axis in camera space.

		Vec3 dir = camtm.TransformVector( Vec3(0,1,0) );
		Vec3 newdir = (Rx*Rz).TransformVector(dir).GetNormalized();
		camtm = Matrix34( Matrix33::CreateRotationVDir(newdir,0), m_camTarget - newdir*m_camRadius );
		m_camera.SetMatrix( camtm );
		
		CPoint pnt = m_mousePos;
		ClientToScreen( &pnt );
		SetCursorPos( pnt.x,pnt.y );
		Invalidate();
	}
	else if (m_bInMoveMode)
	{
		// Slide.
		float speedScale = 0.001f;
		Matrix34 m = m_camera.GetMatrix();
		Vec3 xdir = m.GetColumn0().GetNormalized();
		Vec3 zdir = m.GetColumn2().GetNormalized();

		Vec3 pos = m_camTarget;
		pos += 0.1f*xdir*(point.x-m_mousePos.x)*speedScale + 0.1f*zdir*(m_mousePos.y-point.y)*speedScale;
		m_camTarget = pos;
		
		Vec3 dir = m.TransformVector( Vec3(0,1,0) );
		m.SetTranslation( m_camTarget - dir*m_camRadius );
		m_camera.SetMatrix(m);

		m_mousePos = point;
		CPoint pnt = m_mousePos;
		ClientToScreen( &pnt );
		SetCursorPos( pnt.x,pnt.y );
		Invalidate();
	}
}

void CPreviewModelCtrl::OnRButtonDown(UINT nFlags, CPoint point) 
{
	m_bInMoveMode = true;
	m_mousePos = point;
	if (!m_bInRotateMode)
		SetCapture();
	Invalidate();
}

void CPreviewModelCtrl::OnRButtonUp(UINT nFlags, CPoint point) 
{
	m_bInMoveMode = false;
	m_mousePos = point;
	if (!m_bInRotateMode)
		ReleaseCapture();
	Invalidate();
}

BOOL CPreviewModelCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	Matrix34 m = m_camera.GetMatrix();
	Vec3 zdir = m.GetColumn1().GetNormalized();

	//m_camera.SetPosition( m_camera.GetPos() + ydir*(m_mousePos.y-point.y),xdir*(m_mousePos.x-point.x) );
	m_camera.SetPosition( m_camera.GetPosition() + 0.002f*zdir*(zDelta) );
	SetCamera( m_camera );
	Invalidate();
	
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize( nType,cx,cy );
	//Render();
	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::EnableUpdate( bool bUpdate )
{
	m_bUpdate = bUpdate;
	// No timer.
/*
	if (bUpdate)
	{
		if (m_nTimer == 0)
			m_nTimer = SetTimer(1,50,NULL);
	}
	else
	{
		if (m_nTimer != 0)
			KillTimer(m_nTimer);
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::Update()
{
	if (m_bUpdate && m_bHaveAnythingToRender)
	{
		if (IsWindowVisible())
		{
			Render();
			//Invalidate(FALSE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::SetRotation( bool bEnable )
{
	m_bRotate = bEnable;
}

void CPreviewModelCtrl::SetMaterial(CMaterial * pMaterial)
{
	if (pMaterial)
	{
		if ((pMaterial->GetFlags() & MTL_FLAG_NOPREVIEW))
		{
			m_pCurrentMaterial = 0;
			return;
		}
	}
	m_pCurrentMaterial = pMaterial;
}

CMaterial * CPreviewModelCtrl::GetMaterial()
{
	return m_pCurrentMaterial;
}

void CPreviewModelCtrl::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	if(event==eNotify_OnIdleUpdate)
		Update();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::GetImageOffscreen( CImage &image )
{
	m_renderer->EnableSwapBuffers(false);
	Render();
	m_renderer->EnableSwapBuffers(true);

	CRect rc;
	GetClientRect(rc);
	int width = rc.Width();
	int height = rc.Height();
	image.Allocate( width,height );

	m_renderer->ReadFrameBuffer((unsigned char*)image.GetData(), rc.Width(), rc.Width(), rc.Height(), eRB_BackBuffer, true);

	// At this point the image is upside-down, so we need to flip it.
	unsigned int* data = image.GetData();
	for (int row = 0; row < (height - 1) / 2; ++row)
	{
		for (int col = 0; col < width; ++col)
		{
			unsigned int pixel = data[row * width + col];
			data[row * width + col] = data[(height - row - 1) * width + col];
			data[(height - row - 1) * width + col] = pixel;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::GetImage( CImage &image )
{
	Render();

	CRect rc;
	GetClientRect(rc);
	int width = rc.Width();
	int height = rc.Height();
	image.Allocate( width,height );

	CBitmap bmp;
	CDC dcMemory;
	CDC *pDC = GetDC();
	dcMemory.CreateCompatibleDC(pDC);

	bmp.CreateCompatibleBitmap(pDC,width,height);

	CBitmap* pOldBitmap = dcMemory.SelectObject(&bmp);
	dcMemory.BitBlt( 0,0,width,height,pDC,0,0,SRCCOPY );
	//dcMemory.StretchBlt( 0,height,width,-height,pDC,0,0,width,height,SRCCOPY );

	BITMAP bmpInfo;
	bmp.GetBitmap( &bmpInfo );
	bmp.GetBitmapBits( width*height*(bmpInfo.bmBitsPixel/8),image.GetData() );
	int bpp = bmpInfo.bmBitsPixel/8;

	dcMemory.SelectObject(pOldBitmap);

	ReleaseDC(pDC);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::SetClearColor( const ColorF &color )
{
	m_clearColor = color;
}

//////////////////////////////////////////////////////////////////////////
int CPreviewModelCtrl::GetFaceCount()
{
	if (m_object && m_object->GetRenderMesh())
	{
		return m_object->GetRenderMesh()->GetSysIndicesCount()/3;
	}
	else if (m_character)
	{
		
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CPreviewModelCtrl::GetVertexCount()
{
	if (m_object && m_object->GetRenderMesh())
	{
		return m_object->GetRenderMesh()->GetVertCount();
	}
	else if (m_character)
	{

	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CPreviewModelCtrl::GetLodCount()
{
	if (m_object)
	{
		// Count lods.
		int lods = 0;
		for (int i = 1; i < 10; i++)
			if (m_object->GetLodObject(i)) lods++;
		return lods;
	}
	else if (m_character)
	{
		return m_character->GetICharacterModel()->GetNumLods();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CPreviewModelCtrl::GetMtlCount()
{
	if (m_object && m_object->GetRenderMesh())
	{
		return m_object->GetRenderMesh()->GetChunks()->size();
	}
	else if (m_character)
	{
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CPreviewModelCtrl::FitToScreen()
{
	SetCameraLookAt( 2.0f,Vec3(-1,1,-0.5) );
}
