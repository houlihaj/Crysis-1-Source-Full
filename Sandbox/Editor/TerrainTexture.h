#if !defined(AFX_TERRAINTEXTURE_H__44644B20_29E9_4005_9F28_003914D5FE37__INCLUDED_)
#define AFX_TERRAINTEXTURE_H__44644B20_29E9_4005_9F28_003914D5FE37__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// TerrainTexture.h : header file
//
#include "ToolbarDialog.h"

// forward declarations.
class CLayer;

// Internal resolution of the final texture preview
#define FINAL_TEX_PREVIEW_PRECISION_CX 256
#define FINAL_TEX_PREVIEW_PRECISION_CY 256

// Hold / fetch temp file
#define HOLD_FETCH_FILE_TTS "Temp\\HoldStateTemp.lay"

/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureDialog dialog

enum SurfaceGenerateFlags {
	GEN_USE_LIGHTING		= 1,
	GEN_SHOW_WATER			= 1<<2,
	GEN_SHOW_WATERCOLOR	= 1<<3,
	GEN_KEEP_LAYERMASKS = 1<<4,
	GEN_ABGR            = 1<<5,
	GEN_STATOBJ_SHADOWS = 1<<6
};

class CTerrainTextureDialog : public CToolbarDialog, public IEditorNotifyListener
{
// Construction
	DECLARE_DYNCREATE(CTerrainTextureDialog)
public:
//	bool GenerateSurface(DWORD *pSurface, UINT iWidth, UINT iHeight, int flags,CBitArray *pLightingBits = NULL, float **ppHeightmapData = NULL);
	void ReloadLayerList();
	
	CTerrainTextureDialog(CWnd* pParent = NULL);   // standard constructor
	~CTerrainTextureDialog();

	static void OnUndoUpdate();

	enum { IDD = IDD_DATABASE };


	//////////////////////////////////////////////////////////////////////////
	// IEditorNotifyListener implementation
	//////////////////////////////////////////////////////////////////////////
	// Called by the editor to notify the listener about the specified event.
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );
	//////////////////////////////////////////////////////////////////////////

// Implementation
protected:
	typedef std::vector<CLayer*> Layers;

	virtual void OnOK() {};
	virtual void OnCancel() {};

	void ClearData();
	void CleanUpSurfaceTypes();

	// Dialog control related functions
	void UpdateControlData();
	void EnableControls();
	void CompressLayers();

	void InitReportCtrl();
	void InitTaskPanel();
	void GeneratePreviewImageList();
	void RecalcLayout();
	void SelectLayer( CLayer *pLayer,bool bSelectUI=true );

	// Assign selected material to the selected layers.
	void OnAssignMaterial();

	// set detail texture info
	void OnSetDetailTextureScale(float x);
	void OnSetDetailTextureProjection(CString& text);
	bool GetSelectedLayers( Layers &layers );

	// The currently active layer (syncronized with the list box selection)
	CLayer *m_pCurrentLayer;
	CCryEditDoc* m_doc;

	// Apply lighting to the previews ?
	static bool m_bUseLighting;

	// Show water in the preview ?
	static bool m_bShowWater;


	CXTPReportControl m_wndReport;
	CXTPTaskPanel m_wndTaskPanel;

	CImageList m_imageList;

	CButton m_showPreviewCheck;
	CStatic m_previewLayerTextureButton;
	CXTPTaskPanelGroupItem *m_pTextureInfoText;
	CXTPTaskPanelGroupItem *m_pLayerInfoText;

	// Generated message map functions
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSelchangeLayerList();
	afx_msg void OnLoadTexture();
	afx_msg void OnImport();
	afx_msg void OnExport();
	afx_msg void OnFileExportLargePreview();
	afx_msg void OnGeneratePreview();
	afx_msg void OnApplyLighting();
	afx_msg void OnSetWaterLevel();
	afx_msg void OnLayerExportTexture();
	afx_msg void OnHold();
	afx_msg void OnFetch();
	afx_msg void OnUseLayer();
	afx_msg void OnOptionsSetLayerBlending();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnAutoGenMask();
	afx_msg void OnLoadMask();
	afx_msg void OnExportMask();
	afx_msg void OnShowWater();
	afx_msg void OnRefineTerrainTextureTiles();

	afx_msg void OnLayersNewItem();
	afx_msg void OnLayersDeleteItem();
	afx_msg void OnLayersMoveItemUp();
	afx_msg void OnLayersMoveItemDown();
		
	afx_msg void OnShowPreviewCheckbox();
	
	afx_msg LRESULT OnTaskPanelNotify( WPARAM wParam, LPARAM lParam );

	afx_msg void OnReportClick(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportRClick(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportSelChange(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportHyperlink(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportKeyDown(NMHDR * pNotifyStruct, LRESULT * /*result*/);
	afx_msg void OnReportPropertyChanged(NMHDR * pNotifyStruct, LRESULT * /*result*/);

private:
	CXmlArchive m_ar;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TERRAINTEXTURE_H__44644B20_29E9_4005_9F28_003914D5FE37__INCLUDED_)
