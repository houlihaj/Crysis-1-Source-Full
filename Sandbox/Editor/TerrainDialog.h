#if !defined(AFX_TERRAINDIALOG_H__7DCC65C5_79C7_4B64_BDAA_6D8F3A43F7B8__INCLUDED_)
#define AFX_TERRAINDIALOG_H__7DCC65C5_79C7_4B64_BDAA_6D8F3A43F7B8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TerrainDialog.h : header file
//

#include "TopRendererWnd.h"
#include "Controls\SplitterWndEx.h"
#include "Dialogs\BaseFrameWnd.h"

struct SNoiseParams;
class CHeightmap;

/////////////////////////////////////////////////////////////////////////////
// CTerrainDialog dialog

class CTerrainDialog : public CBaseFrameWnd
{
	DECLARE_DYNCREATE(CTerrainDialog)

public:
	CTerrainDialog();
	~CTerrainDialog();

	SNoiseParams* GetLastParam() { return m_sLastParam; };

	enum { IDD = IDD_TERRAIN };

protected:
	class CTerrainModifyTool* GetTerrainTool();
	void Flatten(float fFactor);
	float ExpCurve(float v, unsigned int iCover, float fSharpness);

	void InvalidateViewport();
	void InvalidateTerrain();

	//////////////////////////////////////////////////////////////////////////
	// CBaseFrameWnd implementation
	//////////////////////////////////////////////////////////////////////////
	virtual LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);
	//////////////////////////////////////////////////////////////////////////

	afx_msg LRESULT OnKickIdle(WPARAM wParam, LPARAM);
	afx_msg void OnTerrainLower();
	afx_msg void OnTerrainRaise();
	virtual BOOL OnInitDialog();
	afx_msg void OnTerrainLoad();
	afx_msg void OnTerrainErase();
	afx_msg void OnBrush1();
	afx_msg void OnBrush2();
	afx_msg void OnBrush3();
	afx_msg void OnBrush4();
	afx_msg void OnBrush5();
	afx_msg void OnTerrainResize();
	afx_msg void OnTerrainLight();
	afx_msg void OnTerrainSurface();
	afx_msg void OnTerrainGenerate();
	afx_msg void OnTerrainInvert();
	afx_msg void OnExportHeightmap();
	afx_msg void OnModifyMakeisle();
	afx_msg void OnModifyFlattenLight();
	afx_msg void OnModifyFlattenHeavy();
	afx_msg void OnModifySmooth();
	afx_msg void OnModifyRemovewater();
	afx_msg void OnModifySmoothSlope();
	afx_msg void OnHeightmapShowLargePreview();
	afx_msg void OnModifySmoothBeachesOrCoast();
	afx_msg void OnModifyNoise();
	afx_msg void OnModifyNormalize();
	afx_msg void OnModifyReduceRange();
	afx_msg void OnModifyReduceRangeLight();
	afx_msg void OnModifyRandomize();
	afx_msg void OnLowOpacity();
	afx_msg void OnMediumOpacity();
	afx_msg void OnHighOpacity();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnHold();
	afx_msg void OnFetch();
	afx_msg void OnOptionsShowMapObjects();
	afx_msg void OnOptionsShowWater();
	afx_msg void OnExportTerrainAsGeometrie();
	afx_msg void OnOptionsEditTerrainCurve();
	afx_msg void OnSetWaterLevel();
	afx_msg void OnSetMaxHeight();
	afx_msg void OnSetUnitSize();
	afx_msg void OnShowWaterUpdateUI(CCmdUI* pCmdUI);
	afx_msg void OnShowMapObjectsUpdateUI(CCmdUI* pCmdUI);

	DECLARE_MESSAGE_MAP()

	SNoiseParams* m_sLastParam;
	CHeightmap *m_heightmap;

	class CTerrainModifyTool *m_pTerainTool;

	CRollupCtrl m_rollupCtrl;
	CStatusBar m_wndStatusBar;
	CTopRendererWnd *m_pViewport;

	//////////////////////////////////////////////////////////////////////////
	//Panes
	//////////////////////////////////////////////////////////////////////////
	CXTPDockingPane* m_pDockPane_Rollup;
	//////////////////////////////////////////////////////////////////////////
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TERRAINDIALOG_H__7DCC65C5_79C7_4B64_BDAA_6D8F3A43F7B8__INCLUDED_)
