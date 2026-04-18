// Viewport.h: interface for the CViewport class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_EDITWND_H__DB6E5168_5AA2_439C_A986_DDD440C82BA3__INCLUDED_)
#define AFX_EDITWND_H__DB6E5168_5AA2_439C_A986_DDD440C82BA3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// forward declarations.
class CBaseObject;
struct DisplayContext;
class CCryEditDoc;
class CViewManager;
class CEditTool;
class CBaseObjectsCache;
struct HitContext;

#define WM_VIEWPORT_ON_TITLE_CHANGE	(WM_USER + 10)

/** Type of viewport.
*/
enum EViewportType
{
	ET_ViewportUnknown = 0,
	ET_ViewportXY,
	ET_ViewportXZ,
	ET_ViewportYZ,
	ET_ViewportCamera,
	ET_ViewportMap,
	ET_ViewportModel,
	ET_ViewportZ, //!< Z Only viewport.
	
	ET_ViewportLast,
};

//////////////////////////////////////////////////////////////////////////
// Standart cursors viewport can display.
//////////////////////////////////////////////////////////////////////////
enum EStdCursor
{
	STD_CURSOR_DEFAULT,
	STD_CURSOR_HIT,
	STD_CURSOR_MOVE,
	STD_CURSOR_ROTATE,
	STD_CURSOR_SCALE,
	STD_CURSOR_SEL_PLUS,
	STD_CURSOR_SEL_MINUS,
	STD_CURSOR_SUBOBJ_SEL,
	STD_CURSOR_SUBOBJ_SEL_PLUS,
	STD_CURSOR_SUBOBJ_SEL_MINUS,

	// TODO: remove hardcoded aplication name
	STD_CURSOR_CRYSIS,

	STD_CURSOR_LAST,
};

/** Base class for all Editor Viewports.
*/
class CViewport : public CWnd  
{
	DECLARE_DYNAMIC(CViewport)
public:
	enum ViewMode
	{
		NothingMode = 0,
		ScrollZoomMode,
		ScrollMode,
		ZoomMode,
		ManipulatorDragMode,
	};

	CViewport();
	virtual ~CViewport();

	//! Called while window is idle.
	virtual void Update();

	virtual void UpdateGameViewport() {}

	/** Set name of this viewport.
	*/
	void SetName( const CString &name );
	
	/** Get name of viewport
	*/
	CString GetName() const;

	/** Must be overriden in derived classes.
	*/
	virtual void SetType( EViewportType type ) {};

	// Must be overriden in derived classes.
	// Returns:
	//   e.g. 4.0/3.0
	virtual float GetAspectRatio() const=0;

	/** Get type of this viewport.
	*/
	virtual EViewportType GetType() const { return ET_ViewportUnknown; }

	//! Make this view active or deactive.
	virtual void SetActive( bool bActive );
	//! Check if this view is active.
	virtual bool IsActive() const;

	virtual void ResetContent();
	virtual void UpdateContent( int flags );

	//! Set current zoom factor for this viewport.
	virtual void SetZoomFactor(float fZoomFactor);

	//! Get current zoom factor for this viewport.
	virtual float GetZoomFactor() const;

	virtual void OnActivate();
	virtual void OnDeactivate();
	virtual void SetCursor( HCURSOR hCursor );

	//! Map world space position to viewport position.
	virtual CPoint	WorldToView( Vec3 wp );
	
	//! Map world space position to 3D viewport position.
	virtual Vec3    WorldToView3D( Vec3 wp,int nFlags=0 );

	//! Map viewport position to world space position.
	virtual Vec3		ViewToWorld( CPoint vp,bool *collideWithTerrain=0,bool onlyTerrain=false );
	//! Convert point on screen to world ray.
	virtual void		ViewToWorldRay( CPoint vp,Vec3 &raySrc,Vec3 &rayDir );
	//! Get normal for viewport position
	virtual Vec3		ViewToWorldNormal( CPoint vp, bool onlyTerrain );

	//! Map view point to world space using current construction plane.
	Vec3 MapViewToCP( CPoint point ) { return MapViewToCP(point,GetAxisConstrain()); };
	virtual Vec3 MapViewToCP( CPoint point,int axis );

	//! This method return a vector (p2-p1) in world space alligned to construction plane and restriction axises.
	//! p1 and p2 must be givven in world space and lie on construction plane.
	virtual Vec3 GetCPVector( const Vec3 &p1,const Vec3 &p2 ) { return GetCPVector(p1,p2,GetAxisConstrain()); }
	virtual Vec3 GetCPVector( const Vec3 &p1,const Vec3 &p2,int axis );

	//! Snap any givven 3D world position to grid lines if snap is enabled.
	virtual Vec3 SnapToGrid( Vec3 vec );

	//! Returns the screen scale factor for a point given in world coordinates.
	//! This factor gives the width in world-space units at the point's distance of the viewport.
	virtual float GetScreenScaleFactor( const Vec3 &worldPoint ) { return 1; };

	virtual void SetViewMode(ViewMode eViewMode) 	{ m_eViewMode = eViewMode; };
	ViewMode GetViewMode() { return m_eViewMode; };

	void SetAxisConstrain( int axis );
	int GetAxisConstrain() const { return GetIEditor()->GetAxisConstrains(); };

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CViewport)
	//}}AFX_VIRTUAL

	virtual void OnTitleMenu( CMenu &menu ) {};
	virtual void OnTitleMenuCommand( int id ) {};

	//////////////////////////////////////////////////////////////////////////
	void CaptureMouse();
	void ReleaseMouse();

	//////////////////////////////////////////////////////////////////////////
	//! Get current document.
	CCryEditDoc* GetDocument();

	//////////////////////////////////////////////////////////////////////////
	// Selection.
	//////////////////////////////////////////////////////////////////////////
	//! Resets current selection region.
	virtual void ResetSelectionRegion();
	//! Set 2D selection rectangle.
	virtual void SetSelectionRectangle( CPoint p1,CPoint p2 );
	//! Return 2D selection rectangle.
	virtual CRect GetSelectionRectangle() const { return m_selectedRect; };
	//! Called when dragging selection rectangle.
	virtual void OnDragSelectRectangle( CPoint p1,CPoint p2,bool bNormilizeRect=false );
	//! Get selection procision tollerance.
	float GetSelectionTolerance() const { return m_selectionTollerance; }
	//! Center viewport on selection.
	virtual void CenterOnSelection() {};

	//////////////////////////////////////////////////////////////////////////
	//! Draw brush on this viewport.
	virtual void DrawBrush( DisplayContext &dc,struct SBrush *brush,const Matrix34 &brushTM,int flags ) {};

	// Draw text in viewport.
	virtual void DrawTextLabel( DisplayContext &dc,const Vec3& pos,float size,const ColorF& color,const char *text, const bool bCenter=false ) {};

	//! Performs hit testing of 2d point in view to find which object hit.
	virtual bool HitTest( CPoint point,HitContext &hitInfo );
	
	//! Do 2D hit testing of line in world space.
	// pToCameraDistance is an optional output parameter in which distance from the camera to the line is returned.
	virtual bool HitTestLine( const Vec3 &lineP1,const Vec3 &lineP2,CPoint hitpoint,int pixelRadius,float *pToCameraDistance=0 );

	//////////////////////////////////////////////////////////////////////////
	// View matrix.
	//////////////////////////////////////////////////////////////////////////
	//! Set current view matrix,
	//! This is a matrix that transforms from world to view space.
	virtual void SetViewTM( const Matrix34 &tm ) { m_viewTM = tm; };
	
	//! Get current view matrix.
	//! This is a matrix that transforms from world space to view space.
	virtual const Matrix34& GetViewTM() const { return m_viewTM; };

	//////////////////////////////////////////////////////////////////////////

	//! Access to view manager.
	CViewManager* GetViewManager() const { return m_viewManager; };

	//////////////////////////////////////////////////////////////////////////
	//! Set construction plane from given position construction matrix refrence coord system and axis settings.
	//////////////////////////////////////////////////////////////////////////
	virtual void MakeConstructionPlane( int axis );
	virtual void SetConstructionMatrix( RefCoordSys coordSys,const Matrix34 &xform );
	virtual const Matrix34& GetConstructionMatrix( RefCoordSys coordSys );
	// Set simple construction plane origin.
	void SetConstructionOrigin( const Vec3 &worldPos );
	//////////////////////////////////////////////////////////////////////////

	void DegradateQuality( bool bEnable );

	//////////////////////////////////////////////////////////////////////////
	// Undo for viewpot operations.
	void BeginUndo();
	void AcceptUndo( const CString &undoDescription );
	void CancelUndo();
	void RestoreUndo();
	bool IsUndoRecording() const;
	//////////////////////////////////////////////////////////////////////////

	//! Get prefered original size for this viewport.
	//! if 0, then no preference.
	virtual CSize GetIdealSize() const;

	//! Check if world space bounding box is visible in this view.
	virtual bool IsBoundsVisible( const BBox &box ) const;

	//////////////////////////////////////////////////////////////////////////
	bool CheckVirtualKey( int virtualKey );

	virtual void StartAVIRecording( const char *filename );
	virtual void StopAVIRecording();
	virtual void PauseAVIRecording( bool bPause );
	virtual bool IsAVIRecording() const;
	virtual bool IsAVIRecordingPaused() const { return m_bAVIPaused; };

	virtual void ToggleCameraObject(){};
	virtual bool GetCameraObjectState(){return false;};

	// Changed my view manager.
	void SetViewManager( CViewManager* viewMgr ) { m_viewManager = viewMgr; };

	// Set`s current cursor string.
	void SetCurrentCursor( HCURSOR hCursor,const CString &cursorString );
	void SetCurrentCursor( EStdCursor stdCursor,const CString &cursorString );
	void SetCurrentCursor( EStdCursor stdCursor );
	void ResetCursor();

	//////////////////////////////////////////////////////////////////////////
	// Drag and drop support on viewports.
	// To be overrided in derived classes.
	//////////////////////////////////////////////////////////////////////////
	virtual bool CanDrop( CPoint point,IDataBaseItem *pItem ) { return false; };
	virtual void Drop( CPoint point,IDataBaseItem *pItem ) {};

	virtual CEditTool* GetEditTool();
	// Assign an edit tool to viewport
	virtual void SetEditTool( CEditTool *pEditTool,bool bLocalToViewport=false );

	//////////////////////////////////////////////////////////////////////////
	// Return visble objects cache.
	CBaseObjectsCache* GetVisibleObjectsCache() { return m_pVisibleObjectsCache; };

protected:
	friend class CViewManager;
	void CreateViewportWindow();
	void AVIRecordFrame();
	bool IsVectorInValidRange( const Vec3 &v ) const { return fabs(v.x) < 1e+8 && fabs(v.y) < 1e+8 && fabs(v.z) < 1e+8; }
	void AssignConstructionPlane( const Vec3 &p1,const Vec3 &p2,const Vec3 &p3 );

	// called to process mouse callback inside the viewport.
	virtual bool MouseCallback( EMouseEvent event,CPoint &point,int flags );

	virtual void PostNcDestroy();

	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	
	DECLARE_MESSAGE_MAP()

	float m_selectionTollerance;
	CMenu m_cViewMenu;

	mutable float m_fZoomFactor;

	bool m_bActive;
	ViewMode m_eViewMode;

	CPoint m_cMouseDownPos;

	//! Current selected rectangle.
	CRect m_selectedRect;

	int m_activeAxis;

	// Viewport matrix.
	Matrix34 m_viewTM;

	//! Current construction plane.
	Plane m_constructionPlane;
	Vec3 m_constructionPlaneAxisX;
	Vec3 m_constructionPlaneAxisY;
	
	CViewManager *m_viewManager;

	// When true selection helpers will be shown and hit tested against.
	bool m_bAdvancedSelectMode;

	//////////////////////////////////////////////////////////////////////////
	// Standart cursors.
	//////////////////////////////////////////////////////////////////////////
	HCURSOR m_hCursor[STD_CURSOR_LAST];
	HCURSOR m_hCurrCursor;

	bool m_bRMouseDown;
	//! Mouse is over this object.
	CBaseObject* m_pMouseOverObject;
	CString m_cursorStr;

	class CAVI_Writer *m_pAVIWriter;
	CImage m_aviFrame;
	float m_aviFrameRate;
	float m_aviLastFrameTime;
	bool m_bAVICreation;
	bool m_bAVIPaused;

	static bool m_bDegradateQuality;

	// Grid size modifier due to zoom.
	float m_fGridZoom;

	int m_nLastUpdateFrame;
	int m_nLastMouseMoveFrame;

	CBaseObjectsCache *m_pVisibleObjectsCache;

	CRect m_rcClient;

	// Same construction matrix is shared by all viewports.
	Matrix34 m_constructionMatrix[LAST_COORD_SYSTEM];

	_smart_ptr<CEditTool> m_pLocalEditTool;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITWND_H__DB6E5168_5AA2_439C_A986_DDD440C82BA3__INCLUDED_)
