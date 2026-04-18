#if !defined(AFX_RENDERVIEWPORT_H__323772B6_5A57_4867_B973_A9102FE3001B__INCLUDED_)
#define AFX_RENDERVIEWPORT_H__323772B6_5A57_4867_B973_A9102FE3001B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RenderViewport.h : header file
//

#include "Viewport.h"
#include "Objects\DisplayContext.h"

// forward declarations.
class CBaseObject;
struct IPhysicalEntity;

typedef IPhysicalEntity * PIPhysicalEntity;


/////////////////////////////////////////////////////////////////////////////
// CRenderViewport window

class CRenderViewport : public CViewport, public IEditorNotifyListener
{
	DECLARE_DYNCREATE(CRenderViewport)
	// Construction
public:
	CRenderViewport();

	/** Get type of this viewport.
	*/
	virtual EViewportType GetType() const { return ET_ViewportCamera; }
	virtual void SetType( EViewportType type ) { assert(type == ET_ViewportCamera); };

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRenderViewport)
	//}}AFX_VIRTUAL

	// Implementation
public:
	virtual ~CRenderViewport();
	void InitCommon();

	virtual void Update();
	virtual void UpdateGameViewport();

	virtual void OnTitleMenu( CMenu &menu );
	virtual void OnTitleMenuCommand( int id );

	void SetCamera( const CCamera &camera );
	const CCamera& GetCamera() const { return m_Camera; };
	void SetViewTM( const Matrix34 &tm );

	//! Map world space position to viewport position.
	virtual CPoint	WorldToView( Vec3 wp );
	virtual Vec3    WorldToView3D( Vec3 wp,int nFlags=0 );

	//! Map viewport position to world space position.
	virtual Vec3		ViewToWorld( CPoint vp,bool *collideWithTerrain=0,bool onlyTerrain=false );
	virtual void		ViewToWorldRay( CPoint vp,Vec3 &raySrc,Vec3 &rayDir );
	virtual Vec3		ViewToWorldNormal( CPoint vp, bool onlyTerrain );
	virtual float		GetScreenScaleFactor( const Vec3 &worldPoint );
	virtual float GetAspectRatio() const;
	virtual void DrawTextLabel( DisplayContext &dc,const Vec3& pos,float size,const ColorF& color,const char *text, const bool bCenter=false );
	virtual bool HitTest( CPoint point,HitContext &hitInfo );
	virtual bool IsBoundsVisible( const BBox &box ) const;
	virtual void CenterOnSelection();

	void SetDefaultCamera();
	void SetSequenceCamera();
	void SetSelectedCamera();

	void SetUniformScaling(f32 us) { m_fUniformScaling=us; };
	void SetPlayerControl(uint32 i) { m_PlayerControl=i; };
	uint32 GetPlayerControl() { return m_PlayerControl; };
	void SetPlayerPos(const Vec3& pos) 
	{ 
		m_AverageFrameTime=0.14f;

		m_AnimatedCharacter.SetIdentity();
		m_PhysEntityLocation.SetIdentity();

		m_EntityMat.SetIdentity();
		m_PrevEntityMat.SetIdentity();

		m_vSmoothEntityPosition	= Vec3(0,0,0);
		m_vSmoothEntityCatchUpPos	= Vec3(0,0,0);
		m_vSmoothEntityDirection	= Vec3(0,1,0);

		m_fUniformScaling	=	1.0f; 
		m_absCameraHigh=2.0f; 
		m_absCameraPos=Vec3(0,3,2); 
		m_absCameraPosVP=Vec3(0,-3,1.5); 

		m_absCurrentSpeed=-1.0f;
		m_absCurrentSlope= 0.0f;


		m_absLookDirectionXY(0,1,0);


		m_LookAt=Vec3(ZERO);
		m_LookAtRate=Vec3(ZERO);
		m_CamPos=Vec3(0,1,0);
		m_CamPosRate=Vec3(ZERO);

		m_relCameraRotX=0;
		m_relCameraRotZ=0;

		uint32 numSample2 = m_arrCamRadYaw.size();
		for (uint32 i=0; i<numSample2; i++)
			m_arrCamRadYaw[i]=0.0f;

		uint32 numSample3 = m_arrCamRadPitch.size();
		for (uint32 i=0; i<numSample3; i++)
			m_arrCamRadPitch[i]=0.0f;

		uint32 numSample6 = m_arrAnimatedCharacterPath.size();
		for (uint32 i=0; i<numSample6; i++)
			m_arrAnimatedCharacterPath[i]=Vec3(ZERO);

		numSample6 = m_arrSmoothEntityPath.size();
		for (uint32 i=0; i<numSample6; i++)
			m_arrSmoothEntityPath[i]=Vec3(ZERO);

		uint32 numSample7 = m_arrRunStrafeSmoothing.size();
		for (uint32 i=0; i<numSample7; i++)
		{
			m_arrRunStrafeSmoothing[i]=0;
		}

		m_vWorldDesiredBodyDirection=Vec2(0,1);
		m_vWorldDesiredBodyDirectionSmooth		=Vec2(0,1);
		m_vWorldDesiredBodyDirectionSmoothRate=Vec2(0,1);


		m_vWorldDesiredMoveDirection=Vec2(0,1);
		m_vWorldDesiredMoveDirectionSmooth		=Vec2(0,1);
		m_vWorldDesiredMoveDirectionSmoothRate=Vec2(0,1);
		m_vLocalDesiredMoveDirection=Vec2(0,1);
		m_vLocalDesiredMoveDirectionSmooth		=Vec2(0,1);
		m_vLocalDesiredMoveDirectionSmoothRate=Vec2(0,1);

		m_vWorldAimBodyDirection=Vec2(0,1);
		m_MoveDirRad=0;




		m_MoveSpeedMSec=5.0f;
		m_PassedTime=5.0f; 
		m_lkey_W=0;
		m_lkey_S=0;
		m_lkey_A=0;
		m_lkey_D=0;

		m_key_W=0;
		m_key_S=0;
		m_key_A=0;
		m_key_D=0;
		m_key_SPACE=0;
		m_key_SPACERCR=0;
		m_ControllMode=0;

		m_WeaponMode = 2;
		m_State=-1;
		m_Stance=1; //combat 

		m_udGround	=0.0f;			
		m_lrGround	=0.0f;			
		AABB aabb		= AABB(Vec3(-40.0f,-40.0f,-0.25f),Vec3(+40.0f,+40.0f,+0.0f));
		m_GroundOBB	=	OBB::CreateOBBfromAABB( Matrix33(IDENTITY),aabb );
		m_GroundOBBPos = Vec3(0,0,-0.01f);
	};



	CCamera m_Camera;

protected:
	// Called to render stuff.
	virtual void OnRender();

	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );

	//! Get currently active camera object.
	CBaseObject* GetCameraObject();
	void SetCameraObject( CBaseObject *cameraObject );
	void ToggleCameraObject();
	bool GetCameraObjectState();

	void RenderTerrainGrid( float x1,float y1,float x2,float y2 );
	void RenderConstructionPlane();
	void RenderSnapMarker();
	void RenderCursorString();
	void RenderSafeFrame();
	void ProcessKeys();

	void RenderAll();
	void DrawAxis();
	void InitDisplayContext();
	void SetCurrentContext();

	virtual bool CreateRenderContext();
	virtual void DestroyRenderContext();

	//! Assigned renderer.
	IRenderer*	m_renderer;
	I3DEngine*	m_engine;
	bool m_bRenderContextCreated;
	bool m_bInRotateMode;
	bool m_bInMoveMode;
	bool m_bInOrbitMode;
	bool m_bInZoomMode;

	CPoint m_mousePos;

	float m_moveSpeed;
	Vec3 m_orbitTarget;

	//-------------------------------------------
	//---   player-control in CharEdit        ---
	//-------------------------------------------
	f32 m_MoveSpeedMSec;
	f32 m_PassedTime; 
	uint32 m_lkey_W;
	uint32 m_lkey_S;
	uint32 m_lkey_A;
	uint32 m_lkey_D;

	uint32 m_key_W;
	uint32 m_key_S;
	uint32 m_key_A;
	uint32 m_key_D;
	uint32 m_key_SPACE;
	uint32 m_key_SPACERCR;
	uint32 m_ControllMode;


	int32 m_WeaponMode;
	int32 m_Stance; 
	int32 m_State;
	f32 m_AverageFrameTime;

	uint32 m_PlayerControl;

	f32 m_fUniformScaling;
	f32 m_absCameraHigh;
	Vec3 m_absCameraPos;
	Vec3 m_absCameraPosVP;

	f32  m_absCurrentSpeed;  //in meters per second
	f32  m_absCurrentSlope;  //in radiants

	Vec3 m_absLookDirectionXY;

	Vec3 m_LookAt;
	Vec3 m_LookAtRate;
	Vec3 m_CamPos;
	Vec3 m_CamPosRate;


	f32 m_relCameraRotX;
	f32 m_relCameraRotZ;

	QuatT m_PhysEntityLocation;
	QuatTS m_AnimatedCharacter;

	Matrix34 m_AnimatedCharacterMat;
	Matrix34 m_EntityMat;
	Matrix34 m_PrevEntityMat;

	Vec3 m_vSmoothEntityPosition;
	Vec3 m_vSmoothEntityCatchUpPos;
	Vec3 m_vSmoothEntityDirection;

	std::vector<Vec3> m_arrVerticesHF;
	std::vector<uint16> m_arrIndicesHF;


	std::vector<f32> m_arrCamRadYaw;
	std::vector<f32> m_arrCamRadPitch;

	std::vector<Vec3> m_arrAnimatedCharacterPath;
	std::vector<Vec3> m_arrSmoothEntityPath;

	std::vector<f32> m_arrRunStrafeSmoothing;



	Vec2 m_vWorldDesiredBodyDirection;
	Vec2 m_vWorldDesiredBodyDirectionSmooth;
	Vec2 m_vWorldDesiredBodyDirectionSmoothRate;



	Vec2 m_vWorldDesiredMoveDirection;
	Vec2 m_vWorldDesiredMoveDirectionSmooth;
	Vec2 m_vWorldDesiredMoveDirectionSmoothRate;
	Vec2 m_vLocalDesiredMoveDirection;
	Vec2 m_vLocalDesiredMoveDirectionSmooth;
	Vec2 m_vLocalDesiredMoveDirectionSmoothRate;
	Vec2 m_vWorldAimBodyDirection;
	f32 m_MoveDirRad;



	f32 m_udGround;			
	f32 m_lrGround;			
	OBB m_GroundOBB;
	Vec3 m_GroundOBBPos;

	//-------------------------------------------


	// Render options.
	bool m_bWireframe;
	bool m_bDisplayLabels;
	bool m_bShowSafeFrame;

	CSize m_viewSize;

	// Index of camera objects.
	GUID m_cameraObjectId;
	bool m_bSequenceCamera;
	bool m_bUpdating;
	bool m_bLockCameraMovement;

	// Camera extras
	float m_CamFOV;

	int m_nPresedKeyState;

	Matrix34 m_prevViewTM;
	CString m_prevViewName;


	DisplayContext m_displayContext;

	PIPhysicalEntity * m_pSkipEnts;
	int m_numSkipEnts;

	static HWND m_currentContextWnd;

	// Generated message map functions
protected:

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RENDERVIEWPORT_H__323772B6_5A57_4867_B973_A9102FE3001B__INCLUDED_)
