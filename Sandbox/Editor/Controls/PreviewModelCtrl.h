#if !defined(AFX_PREVIEWMODELCTRL_H__138DA368_C705_4A79_A8FA_5A5D328B8C67__INCLUDED_)
#define AFX_PREVIEWMODELCTRL_H__138DA368_C705_4A79_A8FA_5A5D328B8C67__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PreviewModelCtrl.h : header file
//

struct IEntity;

/////////////////////////////////////////////////////////////////////////////
// CPreviewModelCtrl window

class CPreviewModelCtrl : public CWnd, public IEditorNotifyListener
{
// Construction
public:
	CPreviewModelCtrl();

// Attributes
public:

// Operations
public:
	BOOL Create( CWnd *pWndParent,const CRect &rc,DWORD dwStyle=WS_CHILD|WS_VISIBLE );
	void LoadFile( const CString &modelFile,bool changeCamera=true );
	void LoadParticleEffect( IParticleEffect *pEffect );
	Vec3 GetSize() const { return m_size; };
	CString GetLoadedFile() const { return m_loadedFile; }

	void SetEntity( IRenderNode *entity );
	void SetObject( IStatObj *pObject );
	void SetCameraLookAt( float fRadiusScale,const Vec3 &dir=Vec3(0,1,0) );
	void SetCameraRadius( float fRadius );
	CCamera& GetCamera();
	void SetGrid( bool bEnable ) { m_bGrid = bEnable; }
	void SetAxis( bool bEnable ) { m_bAxis = bEnable; }
	void SetRotation( bool bEnable );
	void SetClearColor( const ColorF &color );
	void UseBackLight( bool bEnable );
	bool UseBackLight() const { return m_bUseBacklight; }

	void EnableUpdate( bool bEnable );
	bool IsUpdateEnabled() const { return m_bUpdate; }
	void Update();

	void SetMaterial(CMaterial * pMaterial);
	CMaterial *  GetMaterial();

	void GetImage( CImage &image );
	void GetImageOffscreen( CImage &image );

	void GetCameraTM(Matrix34& cameraTM);

	// Place camera so that whole object fits on screen.
	void FitToScreen();

	// Get number of triangles in the preview model.
	int GetFaceCount();
	int GetVertexCount();
	int GetLodCount();
	int GetMtlCount();

	void UpdateAnimation();

	void SetShowObject(bool bShowObject) {m_bShowObject = bShowObject;}
	bool GetShowObject() {return m_bShowObject;}

	// Get the character instance.
	ICharacterInstance* GetCharacter() {return m_character;}

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPreviewModelCtrl)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPreviewModelCtrl();

	bool CreateContext();
	void ReleaseObject();
	void DeleteRenderContex();

	// Generated message map functions
protected:
	//{{AFX_MSG(CPreviewModelCtrl)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );


private:
	void SetCamera( CCamera &cam );
	void SetOrbitAngles( const Ang3& ang );
	bool Render();
	void DrawGrid();

	IStatObj *m_object;
	ICharacterInstance *m_character;
	CCamera m_camera;

	IRenderer* m_renderer;
	I3DEngine* m_engine;
  ICharacterManager* m_pAnimationSystem;
	bool m_bContextCreated;


	Vec3 m_size;
	//Vec3 m_objectAngles;
	Vec3 m_pos;
	int m_nTimer;

	CString m_loadedFile;
	std::vector<CDLight> m_lights;

	AABB m_aabb;
	
	// Camera control.
	Vec3 m_camTarget;
	float m_camRadius;
	Vec3 m_camAngles;
	bool m_bInRotateMode;
	bool m_bInMoveMode;
	CPoint m_mousePos;
	IRenderNode* m_entity;
	struct IParticleEmitter* m_pEmitter;

	bool m_bHaveAnythingToRender;
	bool m_bGrid;
	bool m_bAxis;
	bool m_bUpdate;
	float m_fov;
	// Rotate object in preview.
	bool m_bRotate;
	float m_rotateAngle;
	ColorF m_clearColor;
	bool m_bUseBacklight;
	bool m_bShowObject;

	_smart_ptr<CMaterial> m_pCurrentMaterial;

protected:
	virtual void PreSubclassWindow();
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PREVIEWMODELCTRL_H__138DA368_C705_4A79_A8FA_5A5D328B8C67__INCLUDED_)
