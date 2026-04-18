#if !defined(AFX_PANELDISPLAYRENDER_H__E43E6F4B_62D5_4969_8A7B_CE6BC3492E18__INCLUDED_)
#define AFX_PANELDISPLAYRENDER_H__E43E6F4B_62D5_4969_8A7B_CE6BC3492E18__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PanelDisplayRender.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPanelDisplayRender dialog

class CPanelDisplayRender : public CDialog, public IEditorNotifyListener
{
// Construction
public:
	CPanelDisplayRender(CWnd* pParent = NULL);   // standard constructor
	~CPanelDisplayRender();

// Dialog Data
	enum { IDD = IDD_PANEL_DISPLAY_RENDER };
	
	BOOL	m_roads;
	BOOL	m_decals;
	BOOL	m_detailTex;
	BOOL	m_fog;
	BOOL	m_shadowMaps;
	BOOL	m_skyBox;
	BOOL	m_staticObj;
	BOOL	m_terrain;
	BOOL	m_water;
	BOOL	m_detailObj;
	BOOL	m_particles;
	BOOL	m_voxels;
	BOOL	m_clouds;
	BOOL	m_imposters;
	BOOL	m_beams;
	BOOL  m_alphablend;
/*
		DBG_MEMINFO						= 0x002,
		DBG_MEMSTATS					= 0x004,
		DBG_TEXTURE_MEMINFO		= 0x008,
		DBG_AI_DEBUGDRAW			= 0x010,
		DBG_PHYSICS_DEBUGDRAW	= 0x020,
		DBG_RENDERER_PROFILE	= 0x040,
		DBG_RENDERER_PROFILESHADERS	= 0x080,
		DBG_RENDERER_OVERDRAW	= 0x100,
		DBG_RENDERER_RESOURCES	= 0x100,
	*/

	BOOL m_dbg_frame_profile;
	BOOL m_dbg_aidebugdraw;
	BOOL m_dbg_physicsDebugDraw;
	BOOL m_dbg_memory_info;
	BOOL m_dbg_mem_stats;
	BOOL m_dbg_texture_meminfo;
	BOOL m_dbg_renderer_profile;
	BOOL m_dbg_renderer_profile_shaders;
	BOOL m_dbg_renderer_overdraw;
	BOOL m_dbg_renderer_resources;
	BOOL m_dbg_debug_lights;
	BOOL m_dbg_budget_monitoring;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPanelDisplayRender)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void SetControls();
	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );

	// Generated message map functions
	//{{AFX_MSG(CPanelDisplayRender)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeRenderFlag();
	afx_msg void OnChangeDebugFlag();
	afx_msg void OnDisplayAll();
	afx_msg void OnDisplayNone();
	afx_msg void OnDisplayInvert();
	afx_msg void OnFillMode();
	afx_msg void OnWireframeMode();
	afx_msg void OnBnClickedHideHelpers();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PANELDISPLAYRENDER_H__E43E6F4B_62D5_4969_8A7B_CE6BC3492E18__INCLUDED_)
