#if !defined(AFX_TERRAINVOXELPANEL_H__341477A9_CD41_422F_9D74_C674FD62D9C3__INCLUDED_)
#define AFX_TERRAINVOXELPANEL_H__341477A9_CD41_422F_9D74_C674FD62D9C3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TerrainVoxelPanel.h : header file
//

//#include "Controls\SliderCtrlEx.h"				// CSliderCtrlEx
#include "Controls/ColorPickerButton.h"

struct CTerrainVoxelBrush;

/////////////////////////////////////////////////////////////////////////////
// CTerrainVoxelPanel dialog
class CTerrainVoxelTool;

class CTerrainVoxelPanel : public CDialog
{
// Construction
public:
	CTerrainVoxelPanel( CTerrainVoxelTool *tool,CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTerrainVoxelPanel)
	enum { IDD = IDD_PANEL_TERRAIN_VOXEL };
	CComboBox	m_brushType;
	CComboBox	m_brushShape;
	CListBox	m_types;

	//}}AFX_DATA

	void SetBrush( CTerrainVoxelBrush &brush, int voxelObjectType  );

	void SelectVoxelObjectType();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTerrainVoxelPanel)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	void ReloadSurfaceTypes();
	float SliderPositionToRadius(int sliderPosition);
	int RadiusToSliderPosition(float radius);

	// Generated message map functions
	//{{AFX_MSG(CTerrainVoxelPanel)
	virtual BOOL OnInitDialog();
	afx_msg void OnUpdateNumbers();
	afx_msg void OnSelendokBrushType();
	afx_msg void OnSelendokBrushShape();
	afx_msg void OnFlagButton();
	afx_msg void OnHScroll( UINT nSBCode,UINT nPos,CScrollBar* pScrollBar );
	afx_msg void OnSurfTypeSelchange();
	afx_msg void OnInitPosition();
	afx_msg void OnSetupPosition();
	afx_msg void OnAlign();
	afx_msg void OnColorChanged();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CNumberCtrl m_brushRadius;
	CTerrainVoxelTool*	m_tool;
	CSliderCtrl	m_radiusSlider;
	CNumberCtrl m_brushDepth;
	CNumberCtrl m_PlaneSize;

	CColorCheckBox m_SetupPosition;
	CColorCheckBox m_InitPosition;
	CColorCheckBox m_Align;
	CColorPickerButton m_SolidColor;

public:
	afx_msg void OnBnClickedVoxelObject();
	afx_msg void OnBnClickedVoxelTerrain();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TERRAINVOXELPANEL_H__341477A9_CD41_422F_9D74_C674FD62D9C3__INCLUDED_)
