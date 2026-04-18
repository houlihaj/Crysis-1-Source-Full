// CryEdit.h : main header file for the CRYEDIT application
//

#if !defined(AFX_CRYEDIT_H__41D56446_54D7_49B2_8EF6_884EA7A42365__INCLUDED_)
#define AFX_CRYEDIT_H__41D56446_54D7_49B2_8EF6_884EA7A42365__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

#include "Util\FileChangeMonitor.h"
#include "IFileChangeMonitor.h"
#include "MaterialSender.h"
#include "StartupLogoDialog.h"


class CMatEditMainDlg;


class CCryEditDoc;
/////////////////////////////////////////////////////////////////////////////
// CCryEditApp:
// See CryEdit.cpp for the implementation of this class
//

class CCryEditApp : public CWinApp, public IFileChangeMonitor
{
public:
	CRecentFileList * GetRecentFileList() { return m_pRecentFileList; };
	virtual void AddToRecentFileList(LPCTSTR lpszPathName);

	CCryEditApp();
	~CCryEditApp();
	
	void	LoadFile( const CString &fileName );

	bool IsInTestMode() { return m_bTestMode; };
	bool IsInPreviewMode() { return m_bPreviewMode; };
	bool IsInExportMode() { return m_bExportMode; };
	bool IsInConsoleMode() { return m_bConsoleMode; };
	void EnableAccelerator( bool bEnable );
	void SaveAutoBackup();
	void SaveAutoRemind();

	bool RegisterListener(IFileChangeListener *pListener, const char* sMonitorItem );
	bool UnregisterListener(IFileChangeListener *pListener);

public:
	IEditor* m_IEditor;

	void InitDirectory();
	BOOL FirstInstance();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCryEditApp)

public:
	TAGES_EXPORT virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual BOOL OnIdle(LONG lCount);
	virtual CDocument* OpenDocumentFile(LPCTSTR lpszFileName);
	//}}AFX_VIRTUAL

// Implementation
	//{{AFX_MSG(CCryEditApp)
	afx_msg void OnCreateLevel();
	afx_msg void OnOpenLevel();
	afx_msg void OnAppAbout();
	afx_msg void ToolTerrain();
	afx_msg void ToolSky();
	afx_msg void ToolLighting();
	afx_msg void TerrainTextureExport();
	afx_msg void ToolTexture();
	afx_msg void ExportToGame();
	afx_msg void ExportToOBJ();
	afx_msg void OnEditHold();
	afx_msg void OnEditFetch();
	afx_msg void OnCancelMode();
	afx_msg void OnGeneratorsStaticobjects();
	afx_msg void OnFileCreateopenlevel();
	afx_msg void OnFileExportToGameNoSurfaceTexture();
	afx_msg void OnEditInsertObject();
	afx_msg void OnViewSwitchToGame();
	afx_msg void OnEditSelectAll();
	afx_msg void OnEditSelectNone();
	afx_msg void OnEditDelete();
	afx_msg void OnMoveObject();
	afx_msg void OnSelectObject();
	afx_msg void OnRenameObj();
	afx_msg void OnSetHeight();
	afx_msg void OnScriptCompileScript();
	afx_msg void OnScriptEditScript();
	afx_msg void OnEditmodeMove();
	afx_msg void OnEditmodeRotate();
	afx_msg void OnEditmodeScale();
	afx_msg void OnEditToolLink();
	afx_msg void OnUpdateEditToolLink(CCmdUI* pCmdUI);
	afx_msg void OnEditToolUnlink();
	afx_msg void OnUpdateEditToolUnlink(CCmdUI* pCmdUI);
	afx_msg void OnEditmodeSelect();
	afx_msg void OnSelectionDelete();
	afx_msg void OnEditEscape();
	afx_msg void OnObjectSetArea();
	afx_msg void OnObjectSetHeight();
	afx_msg void OnUpdateEditmodeSelect(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditmodeMove(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditmodeRotate(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditmodeScale(CCmdUI* pCmdUI);
	afx_msg void OnObjectmodifyFreeze();
	afx_msg void OnObjectmodifyUnfreeze();
	afx_msg void OnEditmodeSelectarea();
	afx_msg void OnUpdateEditmodeSelectarea(CCmdUI* pCmdUI);
	afx_msg void OnSelectAxisX();
	afx_msg void OnSelectAxisY();
	afx_msg void OnSelectAxisZ();
	afx_msg void OnSelectAxisXy();
	afx_msg void OnUpdateSelectAxisX(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSelectAxisXy(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSelectAxisY(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSelectAxisZ(CCmdUI* pCmdUI);
	afx_msg void OnUndo();
	afx_msg void OnEditClone();
	afx_msg void OnExportTerrainGeom();
	afx_msg void OnUpdateExportTerrainGeom(CCmdUI* pCmdUI);
	afx_msg void OnSelectionSave();
	afx_msg void OnSelectionLoad();
	afx_msg void OnGotoSelected();
	afx_msg void OnUpdateSelected(CCmdUI* pCmdUI);
	afx_msg void OnAlignObject();
	afx_msg void OnAlignToVoxel();
	afx_msg void OnAlignToGrid();
	afx_msg void OnUpdateAlignObject(CCmdUI* pCmdUI);
	afx_msg void OnUpdateAlignToVoxel(CCmdUI* pCmdUI);
	afx_msg void OnUpdateFreezed(CCmdUI* pCmdUI);
	afx_msg void OnGroupAttach();
	afx_msg void OnUpdateGroupAttach(CCmdUI* pCmdUI);
	afx_msg void OnGroupClose();
	afx_msg void OnUpdateGroupClose(CCmdUI* pCmdUI);
	afx_msg void OnGroupDetach();
	afx_msg void OnUpdateGroupDetach(CCmdUI* pCmdUI);
	afx_msg void OnGroupMake();
	afx_msg void OnUpdateGroupMake(CCmdUI* pCmdUI);
	afx_msg void OnGroupOpen();
	afx_msg void OnUpdateGroupOpen(CCmdUI* pCmdUI);
	afx_msg void OnGroupUngroup();
	afx_msg void OnUpdateGroupUngroup(CCmdUI* pCmdUI);
	afx_msg void OnCloudsCreate();
	afx_msg void OnCloudsDestroy();
	afx_msg void OnUpdateCloudsDestroy(CCmdUI* pCmdUI);
	afx_msg void OnCloudsOpen();
	afx_msg void OnUpdateCloudsOpen(CCmdUI* pCmdUI);
	afx_msg void OnCloudsClose();
	afx_msg void OnUpdateCloudsClose(CCmdUI* pCmdUI);
	afx_msg void OnMissionNew();
	afx_msg void OnMissionDelete();
	afx_msg void OnMissionDuplicate();
	afx_msg void OnMissionProperties();
	afx_msg void OnMissionRename();
	afx_msg void OnMissionSelect();
	afx_msg void OnMissionReload();
	afx_msg void OnMissionEdit();
	afx_msg void OnShowTips();
	afx_msg void OnLockSelection();
	afx_msg void OnEditLevelData();
	afx_msg void OnFileEditLogFile();
	afx_msg void OnFileEditEditorini();
	afx_msg void OnSelectAxisTerrain();
	afx_msg void OnSelectAxisSnapToAll();
	afx_msg void OnUpdateSelectAxisTerrain(CCmdUI* pCmdUI);
	afx_msg void OnUpdateSelectAxisSnapToAll(CCmdUI* pCmdUI);
	afx_msg void OnPreferences();
	afx_msg void OnReloadTextures();
	afx_msg void OnReloadScripts();
	afx_msg void OnReloadGeometry();
	afx_msg void OnReloadTerrain();
	afx_msg void OnRedo();
	afx_msg void OnUpdateRedo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateUndo(CCmdUI* pCmdUI);
	afx_msg void OnLayerSelect();
	afx_msg void OnTerrainCollision();
	afx_msg void OnTerrainCollisionUpdate( CCmdUI *pCmdUI );
	afx_msg void OnGenerateCgfThumbnails();
	afx_msg void OnAiGenerateAllNavigation();
	afx_msg void OnAiGenerateTriangulation();
	afx_msg void OnAiGenerateWaypoint();
	afx_msg void OnAiGenerateFlightNavigation();
	afx_msg void OnAiGenerate3dvolumes();
	afx_msg void OnAiValidateNavigation();
  afx_msg void OnAiGenerate3DDebugVoxels();
	afx_msg void OnAiGenerateSpawners();
	afx_msg void OnSwitchPhysics();
	afx_msg void OnSwitchPhysicsUpdate( CCmdUI *pCmdUI );
	afx_msg void OnSyncPlayer();
	afx_msg void OnSyncPlayerUpdate( CCmdUI *pCmdUI );
	afx_msg void OnRefCoordsSys();
	afx_msg void OnUpdateRefCoordsSys(CCmdUI *pCmdUI);
	afx_msg void OnResourcesReduceworkingset();
	afx_msg void OnToolsGeneratelightmaps();
	afx_msg void OnToolsEquipPacksEdit();
	afx_msg void OnToolsUpdateProcVegetation();
	afx_msg void OnDummyCommand() {};
	afx_msg void OnShowHelpers();
	afx_msg void OnStartStop();
	afx_msg void OnNextKey();
	afx_msg void OnPrevKey();
	afx_msg void OnNextFrame();
	afx_msg void OnPrevFrame();
	afx_msg void OnSelectAll();
	afx_msg void OnKeyAll();

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	//! Initialize accelerator manager
	void InitAccelManager();

	void ReadConfig();
	void WriteConfig();
	
	void TagLocation( int index );
	void GotoTagLocation( int index );
	void LoadTagLocations();
	void MonitorDirectories();
	void ExportLevel( bool bExportToGame,bool bExportTexture,bool bExportLM,bool bAutoExport );

	CCryEditDoc* GetDocument();

	// File Change Monitor stuff
	struct SFileChangeCallback
	{
		IFileChangeListener *pListener;
		CString	sItem;

		SFileChangeCallback() : pListener( NULL ), sItem( "" ) {}
		SFileChangeCallback( IFileChangeListener *pListener, const CString& sItem ) : pListener( pListener ), sItem( sItem ) {}
	};

	std::vector<SFileChangeCallback> m_vecFileChangeCallbacks;
	CFileChangeMonitor *m_pFileChangeMonitor;

	//! True if editor is in test mode.
	//! Test mode is a special mode enabled when Editor ran with /test command line.
	//! In this mode editor starts up, but exit immidiatly after all initialzation.
	bool m_bTestMode;
	bool m_bPrecacheShaderList;
  bool m_bMergeShaders;

	//! In this mode editor will load specified cry file, export t, and then close.
	bool m_bExportMode;
	CString m_exportFile;

	//! If application exiting.
	bool m_bExiting;

	//! True if editor is in preview mode.
	//! In this mode only very limited functionality is available and only for fast preview of models.
	bool m_bPreviewMode;

	// Only console window is created.
	bool m_bConsoleMode;

	//! Current file in preview mode.
	char m_sPreviewFile[_MAX_PATH];

	//! True if editor is in material edit mode.
	//! In this mode only very limited functionality is available and only for fast preview of materials.
	bool m_bMatEditMode;

	CMatEditMainDlg * m_pMatEditDlg;
	class CConsoleDialog *m_pConsoleDialog;
	

	//! This variable rised when autosave must be done on next application update cycle.
	bool m_bSaveAutobackup;

	Vec3 m_tagLocations[12];
	Ang3 m_tagAngles[12];
	float m_fastRotateAngle;
	float m_moveSpeedStep;
	CString m_aviFilename;

	ULONG_PTR m_gdiplusToken;

	HANDLE m_mutexApplication;

	//! was the editor active in the previous frame ... needed to detect if the game lost focus and
	//! dispatch proper SystemEvent (needed to release input keys)
	bool m_bPrevActive;

	// If this flag is set, the next OnIdle() will update, even if the app is in the background, and then
	// this flag will be reset.
	bool m_bForceProcessIdle;

private:
	afx_msg void OnEditHide();
	afx_msg void OnUpdateEditHide(CCmdUI *pCmdUI);
	afx_msg void OnEditUnhideall();
	afx_msg void OnEditFreeze();
	afx_msg void OnUpdateEditFreeze(CCmdUI *pCmdUI);
	afx_msg void OnEditUnfreezeall();

	afx_msg void OnSnap();
	afx_msg void OnUpdateEditmodeSnap(CCmdUI* pCmdUI);

	afx_msg void OnWireframe();
	afx_msg void OnUpdateWireframe(CCmdUI *pCmdUI);
	afx_msg void OnViewGridsettings();
	afx_msg void OnViewConfigureLayout();

	afx_msg void OnFileMonitorChange( WPARAM wParam, LPARAM lParam );

	//////////////////////////////////////////////////////////////////////////
	// Tag Locations.
	afx_msg void OnTagLocation1();
	afx_msg void OnTagLocation2();
	afx_msg void OnTagLocation3();
	afx_msg void OnTagLocation4();
	afx_msg void OnTagLocation5();
	afx_msg void OnTagLocation6();
	afx_msg void OnTagLocation7();
	afx_msg void OnTagLocation8();
	afx_msg void OnTagLocation9();
	afx_msg void OnTagLocation10();
	afx_msg void OnTagLocation11();
	afx_msg void OnTagLocation12();
	//////////////////////////////////////////////////////////////////////////
	afx_msg void OnGotoLocation1();
	afx_msg void OnGotoLocation2();
	afx_msg void OnGotoLocation3();
	afx_msg void OnGotoLocation4();
	afx_msg void OnGotoLocation5();
	afx_msg void OnGotoLocation6();
	afx_msg void OnGotoLocation7();
	afx_msg void OnGotoLocation8();
	afx_msg void OnGotoLocation9();
	afx_msg void OnGotoLocation10();
	afx_msg void OnGotoLocation11();
	afx_msg void OnGotoLocation12();
	afx_msg void OnToolsLogMemoryUsage();
	afx_msg void OnTerrainExportblock();
	afx_msg void OnTerrainImportblock();
	afx_msg void OnUpdateTerrainExportblock(CCmdUI *pCmdUI);
	afx_msg void OnUpdateTerrainImportblock(CCmdUI *pCmdUI);
	afx_msg void OnCustomizeKeyboard();
	afx_msg void OnToolsConfiguretools();
	afx_msg void OnBrushTool();
	afx_msg void OnUpdateBrushTool(CCmdUI *pCmdUI);
	afx_msg void OnExportIndoors();
	afx_msg void OnViewCycle2dviewport();
	afx_msg void OnDisplayGotoPosition();
	afx_msg void OnSnapangle();
	afx_msg void OnUpdateSnapangle(CCmdUI *pCmdUI);
	afx_msg void OnRotateselectionXaxis();
	afx_msg void OnRotateselectionYaxis();
	afx_msg void OnRotateselectionZaxis();
	afx_msg void OnRotateselectionRotateangle();
	afx_msg void OnConvertselectionTobrushes();
	afx_msg void OnConvertselectionTosimpleentity();
	afx_msg void OnEditRenameobject();
	afx_msg void OnChangemovespeedIncrease();
	afx_msg void OnChangemovespeedDecrease();
	afx_msg void OnChangemovespeedChangestep();
	afx_msg void OnModifyAipointPicklink();
	afx_msg void OnModifyAipointPickImpasslink();
	afx_msg void OnGenLightmapsSelected();
	afx_msg void OnMaterialAssigncurrent();
	afx_msg void OnMaterialResettodefault();
	afx_msg void OnMaterialGetmaterial();
	afx_msg void OnToolsUpdatelightmaps();
	afx_msg void OnPhysicsGetState();
	afx_msg void OnPhysicsResetState();
	afx_msg void OnPhysicsSimulateObjects();
	afx_msg void OnFileSourcesafesettings();
	afx_msg void OnFileSavelevelresources();
	afx_msg void OnValidatelevel();
	afx_msg void OnHelpDynamichelp();
	afx_msg void OnFileChangemod();
	afx_msg void OnTerrainResizeterrain();
	afx_msg void OnToolsPreferences();
	afx_msg void OnEditInvertselection();
	afx_msg void OnPrefabsMakeFromSelection();
	afx_msg void OnPrefabsRefreshAll();
	afx_msg void OnAddSelectionToPrefab();
	afx_msg void OnToolterrainmodifySmooth();
	afx_msg void OnTerrainmodifySmooth();
	afx_msg void OnTerrainVegetation();
	afx_msg void OnTerrainPaintlayers();
	afx_msg void OnAvirecorderStartavirecording();
	afx_msg void OnAviRecorderStop();
	afx_msg void OnAviRecorderPause();
	afx_msg void OnAviRecorderOutputFilename();
	afx_msg void OnSwitchcameraDefaultcamera();
	afx_msg void OnSwitchcameraSequencecamera();
	afx_msg void OnSwitchcameraSelectedcamera();
	afx_msg void OnOpenMaterialEditor();
	afx_msg void OnOpenCharacterEditor();
	afx_msg void OnOpenDataBaseView();
	afx_msg void OnOpenFlowGraphView();
	afx_msg void OnBrushResettransform();
	afx_msg void OnBrushMakehollow();
	afx_msg void OnBrushCsgcombine();
	afx_msg void OnBrushCsgsubstruct();
	afx_msg void OnBrushCliptool();
	afx_msg void OnBrushUvtool();
	afx_msg void OnBrushCsgintersect();
	afx_msg void OnSubobjectmodeVertex();
	afx_msg void OnSubobjectmodeEdge();
	afx_msg void OnSubobjectmodeFace();
	afx_msg void OnSubobjectmodePolygon();
	afx_msg void OnBrushCsgsubstruct2();
	afx_msg void OnMaterialPicktool();
	afx_msg void OnTimeOfDay();
	afx_msg void OnClearLevelShaderList();
	afx_msg void OnGenerateVertexScattering();
	afx_msg void OnGameEnableVeryHighSpec();
	afx_msg void OnGameEnableHighSpec();
	afx_msg void OnGameEnableLowSpec();
	afx_msg void OnGameEnableMedSpec();
	
	afx_msg void OnGameEnableSketchMode();
	afx_msg void OnUpdateSketchMode(CCmdUI *pCmdUI);

	afx_msg void OnChangeGameSpec(UINT nID);
	afx_msg void OnUpdateGameSpec(CCmdUI *pCmdUI);
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CRYEDIT_H__41D56446_54D7_49B2_8EF6_884EA7A42365__INCLUDED_)
