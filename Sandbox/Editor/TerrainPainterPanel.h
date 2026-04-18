////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrainpainterpanel.h
//  Version:     v1.00
//  Created:     25/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __terrainpainterpanel_h__
#define __terrainpainterpanel_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <XTToolkitPro.h>
#include "Controls/ColorPickerButton.h"

struct CTextureBrush;
class CLayer;

/////////////////////////////////////////////////////////////////////////////
// CTerrainPainterPanel dialog
class CTerrainTexturePainter;

class CTerrainPainterPanel : public CDialog
{
// Construction
public:
	CTerrainPainterPanel( CTerrainTexturePainter *tool,CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CTerrainPainterPanel)
	enum { IDD = IDD_PANEL_TERRAIN_LAYER };
	CComboBox	m_MaskLayerId;
	CSliderCtrl	m_hardnessSlider;
	CSliderCtrl	m_heightSlider;
	CSliderCtrl	m_BrightnessSlider;
	CListBox m_layers;
	//}}AFX_DATA

	void SetBrush( CTextureBrush &brush );
	CString GetSelectedLayer();
	// Arguments:
	//   pLayer - can be 0 to deselect layer
	void SelectLayer( const CLayer *pLayer );

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTerrainPainterPanel)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	void ReloadLayers();

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSliderChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void UpdateTextureBrushSettings();
	afx_msg void OnBrushResetBrightness();
	afx_msg void SetLayerMaskSettingsToLayer();
	afx_msg void GetLayerMaskSettingsFromLayer();
	afx_msg void OnChange();
	//afx_msg void OnExport();
	//afx_msg void OnImport();
	//afx_msg void OnFileBrowse();

	DECLARE_MESSAGE_MAP()

	CNumberCtrl m_brushRadius;
	CNumberCtrl m_brushHardness;
	CTerrainTexturePainter*	m_tool;
	CButton m_optDetailLayer;
	CButton m_MaskLayerSettings;
	CColorPickerButton m_SolidColor;
	CSliderCtrl	m_radiusSlider;

	CColorCheckBox	m_resizeResolution;
	//CColorCheckBox	m_export;
	//CColorCheckBox	m_import;
	//CColorCheckBox	m_fileBrowse;

	CNumberCtrl m_layerAltMin;
	CNumberCtrl m_layerAltMax;
	CNumberCtrl m_layerSlopeMin;
	CNumberCtrl m_layerSlopeMax;
public:
	afx_msg void OnBnClickedBrushSettolayer();
};

#endif // __terrainpainterpanel_h__
