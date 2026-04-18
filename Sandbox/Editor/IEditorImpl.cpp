////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   IEditorImpl.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CEditorImpl class implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "IEditorImpl.h"
#include "CryEdit.h"
#include "CryEditDoc.h"
#include "CustomColorDialog.h"
#include "Plugin.h"

#include "PluginManager.h"
#include "IconManager.h"
#include "ViewManager.h"
#include "ViewPane.h"
#include "InfoProgressBar.h"
#include "Objects\ObjectManager.h"
#include "Objects\GizmoManager.h"
#include "Objects\AxisGizmo.h"
#include "DisplaySettings.h"
#include "ShaderEnum.h"
#include "EAXPresetMgr.h"
#include "SoundMoods\SoundMoodMgr.h"
#include "HyperGraph\FlowGraphManager.h"

#include "EquipPackLib.h"

#include "AI\AIManager.h"

#include "Undo\Undo.h"

#include "Material\MaterialManager.h"
#include "Material\MaterialPickTool.h"
#include "EntityPrototypeManager.h"
#include "AnimationContext.h"
#include "GameEngine.h"
#include "ExternalTools.h"
#include "Settings.h"
#include "BaseLibraryDialog.h"
#include "Material\Material.h"
#include "EntityPrototype.h"
#include "Particles\ParticleManager.h"
#include "Music\MusicManager.h"
#include "Prefabs\PrefabManager.h"
#include "GameTokens\GameTokenManager.h"
#include "Util\SourceControlInterface.h"
#include "DataBaseDialog.h"
#include "LightmapCompiler\SceneContext.h"
#include "UIEnumsDatabase.h"

//////////////////////////////////////////////////////////////////////////
// Tools.
//////////////////////////////////////////////////////////////////////////
#include "EditTool.h"
#include "PickObjectTool.h"
#include "ObjectCreateTool.h"
#include "TerrainModifyTool.h"
#include "VegetationTool.h"
#include "EditMode\ObjectMode.h"
#include "EditMode\VertexMode.h"
#include "TerrainVoxelTool.h"

#include "Brush\BrushClipTool.h"
#include "Brush\BrushTextureTool.h"
#include "Brush\SolidBrushObject.h"

#include "PhotonTool.h"
#include "PhotonViewTool.h"
#include "FacialEditor/FacialEditorDialog.h"

#include "Modelling\PolygonTool.h"
#include "Modelling\ModellingMode.h"

//////////////////////////////////////////////////////////////////////////
#include <I3DEngine.h>
#include <IConsole.h>
#include <IEntitySystem.h>
#include <IMovieSystem.h>

#pragma comment(lib, "version.lib")

//////////////////////////////////////////////////////////////////////////
// Pointer to global document instance.
//////////////////////////////////////////////////////////////////////////
static CCryEditDoc* theDocument;

static IEditor* s_pEditor = NULL;
//////////////////////////////////////////////////////////////////////////
IEditor* GetIEditor()
{
	return s_pEditor;
}

//////////////////////////////////////////////////////////////////////////
static CMainFrame* GetMainFrame()
{
	CWnd *pWnd = AfxGetMainWnd();
	if (!pWnd)
		return 0;
	if (!pWnd->IsKindOf(RUNTIME_CLASS(CMainFrame)))
		return 0;
	return (CMainFrame*)AfxGetMainWnd();
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CEditorImpl::CEditorImpl()
{
	s_pEditor = this;
	m_system = 0;

	m_currEditMode = eEditModeSelect;
	m_prevEditMode = m_currEditMode;
	m_pEditTool = 0;

	SetMasterCDFolder();

	gSettings.Load();

	m_gameEngine = 0;
	m_pErrorReport = new CErrorReport;
	m_classFactory = CClassFactory::Instance();
	m_pCommandManager = new CCommandManager;

	CRegistrationContext regCtx;
	regCtx.pCommandManager = m_pCommandManager;
	regCtx.pClassFactory = m_classFactory;

	m_pUIEnumsDatabase = new CUIEnumsDatabase;
	m_displaySettings = new CDisplaySettings;
	m_shaderEnum = new CShaderEnum;
	m_displaySettings->LoadRegistry();
	m_pluginMan = new CPluginManager;
	m_objectMan = new CObjectManager;
	m_viewMan = new CViewManager;
	m_iconManager = new CIconManager;
	m_undoManager = new CUndoManager;
	m_pEAXPresetMgr = new CEAXPresetMgr;
	m_pSoundMoodMgr = new CSoundMoodMgr;
	m_AI = new CAIManager;
	m_animationContext = new CAnimationContext;
	m_pEquipPackLib = new CEquipPackLib;
	m_externalToolsManager = new CExternalToolsManager;
	m_materialManager = new CMaterialManager(regCtx);
	m_entityManager = new CEntityPrototypeManager;
	m_particleManager = new CParticleManager;
	m_pMusicManager = new CMusicManager;
	m_pPrefabManager = new CPrefabManager;
	m_pGameTokenManager = new CGameTokenManager;
	m_pFlowGraphManager = new CFlowGraphManager;
	m_pSourceControl = new CSourceControlInterface;

	m_pLightMapperSceneCtx = new CSceneContext;

	m_marker(0,0,0);
	m_selectedRegion.min=Vec3(0,0,0);
	m_selectedRegion.max=Vec3(0,0,0);

	m_selectedAxis = AXIS_TERRAIN;
	m_refCoordsSys = COORDS_LOCAL;

	ZeroStruct(m_lastAxis);
	m_lastAxis[eEditModeSelect] = AXIS_TERRAIN;
	m_lastAxis[eEditModeSelectArea] = AXIS_TERRAIN;
	m_lastAxis[eEditModeMove] = AXIS_TERRAIN;
	m_lastAxis[eEditModeRotate] = AXIS_Z;
	m_lastAxis[eEditModeScale] = AXIS_XY;

	ZeroStruct(m_lastCoordSys);
	m_lastCoordSys[eEditModeSelect] = COORDS_LOCAL;
	m_lastCoordSys[eEditModeSelectArea] = COORDS_LOCAL;
	m_lastCoordSys[eEditModeMove] = COORDS_VIEW;
	m_lastCoordSys[eEditModeRotate] = COORDS_LOCAL;
	m_lastCoordSys[eEditModeScale] = COORDS_LOCAL;

	m_bUpdates = true;

	DetectVersion();

	m_bSelectionLocked = true;
	m_bTerrainAxisIgnoreObjects = true;

	RegisterTools();

	m_pickTool = 0;
	m_pAxisGizmo = 0;

	m_bMatEditMode = false;
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::~CEditorImpl()
{
//	if (m_movieSystem)
//		delete m_movieSystem;
	gSettings.Save();

	SAFE_DELETE( m_pSourceControl );
	SAFE_DELETE( m_pPrefabManager );
	SAFE_DELETE( m_pGameTokenManager );
	SAFE_DELETE (m_pFlowGraphManager);

	if (m_pMusicManager)
		delete m_pMusicManager;

	if (m_particleManager)
		delete m_particleManager;

	if (m_entityManager)
		delete m_entityManager;

	m_materialManager->Set3DEngine(NULL);
	if (m_materialManager)
		delete m_materialManager;

	if (m_AI)
		delete m_AI;

	if (m_pEquipPackLib)
		delete m_pEquipPackLib;

	if (m_undoManager)
		delete m_undoManager;
	m_undoManager = 0;

	if (m_iconManager)
		delete m_iconManager;

	if (m_pEAXPresetMgr)
		delete m_pEAXPresetMgr;

	if (m_pSoundMoodMgr)
		delete m_pSoundMoodMgr;

	if (m_viewMan)
		delete m_viewMan;

	if (m_objectMan)
		delete m_objectMan;
	
	if (m_pluginMan)
		delete m_pluginMan;

	if (m_animationContext)
		delete m_animationContext;

	if (m_displaySettings)
	{
		m_displaySettings->SaveRegistry();
		delete m_displaySettings;
	}

	if (m_pLightMapperSceneCtx)
	{
		delete m_pLightMapperSceneCtx;
	}

	if (m_shaderEnum)
		delete m_shaderEnum;

	if (m_gameEngine)
		delete m_gameEngine;

	delete m_externalToolsManager;
	
	delete m_pCommandManager;

	delete m_classFactory;

	delete m_pErrorReport;

	delete m_pUIEnumsDatabase;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetMasterCDFolder()
{
#ifdef WIN32
	WCHAR sFolder[_MAX_PATH];

	GetModuleFileNameW( GetModuleHandle(NULL), sFolder, sizeof(sFolder));
	PathRemoveFileSpecW(sFolder);

	// Remove Bin32/Bin64 folder.
	WCHAR *lpPath = StrStrIW(sFolder,L"\\Bin32");
	if (lpPath)
		*lpPath = 0;
	lpPath = StrStrIW(sFolder,L"\\Bin64");
	if (lpPath)
		*lpPath = 0;

	m_masterCDFolder = sFolder;
	if (!m_masterCDFolder.IsEmpty())
	{
		if (m_masterCDFolder[m_masterCDFolder.GetLength()-1] != L'\\')
			m_masterCDFolder += L'\\';
	}

	SetCurrentDirectoryW( sFolder );
#endif
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetGameEngine( CGameEngine *ge )
{
	m_system = ge->GetSystem();
	m_gameEngine = ge;
	
	if (m_pEAXPresetMgr)
		m_pEAXPresetMgr->Load();
	
	if (m_pSoundMoodMgr)
		m_pSoundMoodMgr->Load();

	m_templateRegistry.LoadTemplates( Path::MakeFullPath("Editor") );
	m_objectMan->LoadClassTemplates( Path::MakeFullPath("Editor") );

	m_materialManager->Set3DEngine( ge->GetSystem()->GetI3DEngine() );
	m_animationContext->Init();

	if (m_system->GetIConsole()->GetCVar("sys_spec"))
		gSettings.editorConfigSpec = (ESystemConfigSpec)m_system->GetIConsole()->GetCVar("sys_spec")->GetIVal();
}

//////////////////////////////////////////////////////////////////////////
// Call registrations.
//////////////////////////////////////////////////////////////////////////
void CEditorImpl::RegisterTools()
{
	CRegistrationContext rc;
	rc.pCommandManager = m_pCommandManager;
	rc.pClassFactory = m_classFactory;

	//////////////////////////////////////////////////////////////////////////
	// Register various tool classes and commands.
	CTerrainModifyTool::RegisterTool( rc );
	CVegetationTool::RegisterTool( rc );
	CObjectMode::RegisterTool( rc );
	CSubObjectModeTool::RegisterTool( rc );

	CPhotonTool::RegisterTool( rc );
	CPhotonViewTool::RegisterTool( rc );
	CBrushClipTool::RegisterTool( rc );
	CBrushTextureTool::RegisterTool( rc );
	CSolidBrushObject::RegisterCommands( rc );
	CMaterialPickTool::RegisterTool( rc );
	CTerrainVoxelTool::RegisterTool( rc );
	CPolygonTool::RegisterTool( rc );
	CModellingModeTool::RegisterTool( rc );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::ExecuteCommand( const char *sCommand )
{
	m_pCommandManager->Execute( sCommand );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Update()
{
	if (!m_bUpdates)
		return;
	
	FUNCTION_PROFILER( GetSystem(),PROFILE_EDITOR );

	GetIEditor()->Notify( eNotify_OnIdleUpdate );

	//@FIXME: Restore this latter.
	//if (GetGameEngine() && GetGameEngine()->IsLevelLoaded())
	{	
		m_animationContext->Update();
		m_objectMan->Update();
	}
	if (IsInPreviewMode())
	{
		SetModifiedFlag(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
ISystem* CEditorImpl::GetSystem()
{
	return m_system;
}

//////////////////////////////////////////////////////////////////////////
I3DEngine* CEditorImpl::Get3DEngine()
{
	if (gEnv)
		return gEnv->p3DEngine;
	return 0;
}

//////////////////////////////////////////////////////////////////////////	
IRenderer*	CEditorImpl::GetRenderer()
{
	if (gEnv)
		return gEnv->pRenderer;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IGame*	CEditorImpl::GetGame()
{
	if (gEnv)
		return gEnv->pGame;
	return 0;
}

IEditorClassFactory* CEditorImpl::GetClassFactory()
{
	return m_classFactory;
}

//////////////////////////////////////////////////////////////////////////
CCryEditDoc* CEditorImpl::GetDocument()
{
	/*
	CFrameWnd *wnd = dynamic_cast<CFrameWnd*>( AfxGetMainWnd() );
	if (wnd)
		return dynamic_cast<CCryEditDoc*>(wnd->GetActiveDocument());
		*/
	return theDocument;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetDocument( CCryEditDoc *pDoc )
{
	theDocument = pDoc;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetModifiedFlag( bool modified )
{
	if (GetDocument())
	{
		GetDocument()->SetModifiedFlag( modified );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsModified()
{
	if (GetDocument())
	{
		return GetDocument()->IsModified();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::SaveDocument()
{
	if (GetDocument())
		return GetDocument()->Save();
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
CStringW CEditorImpl::GetMasterCDFolder()
{ 
	return m_masterCDFolder;
}

//////////////////////////////////////////////////////////////////////////
CString CEditorImpl::GetLevelFolder()
{
	return GetGameEngine()->GetLevelPath();
}

//////////////////////////////////////////////////////////////////////////
CString CEditorImpl::GetSearchPath( EEditorPathName path )
{
	return gSettings.searchPaths[path][0];
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetDataModified()
{ 
	GetDocument()->SetModifiedFlag(); 
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::AddMenuItem(uint8 iId, bool bIsSeparator,
																				 eMenuInsertLocation eParent, 
																				 IUIEvent *pIHandler)
{
	//////////////////////////////////////////////////////////////////////
	// Adds a plugin menu item and binds an event handler to it
	//////////////////////////////////////////////////////////////////////
	/*

	PluginIt it;
	CMenu *pMainMenu = NULL;
	CMenu *pLastPluginMenu = NULL;
	DWORD dwMenuID;

	IPlugin *pIAssociatedPlugin = GetPluginManager()->GetAssociatedPlugin();
	uint8 iAssociatedPluginUIID = GetPluginManager()->GetAssociatedPluginUIID();

	ASSERT(!IsBadReadPtr(pIAssociatedPlugin, sizeof(IPlugin)));
	if (!bIsSeparator)
		ASSERT(!IsBadReadPtr(pIHandler, sizeof(IUIEvent)));

	if (pIAssociatedPlugin == NULL || pIHandler == NULL)
		return false;

	// Get the main menu
	pMainMenu = AfxGetMainWnd()->GetMenu();
	ASSERT(pMainMenu);

	// Create the menu ID. The first 8 bits of the upper 16 bits contain the
	// UI ID of the plugin which owns the UI element, the second 8 bits
	// contain the user interface element ID. Set the lower 16 bits to
	// a high number in order to avoid conflicts
	dwMenuID = (iAssociatedPluginUIID | (iId << 8)) | 0xFFFF0000;
	
	switch (eParent)
	{
		// Custom plugin menu
		case eMenuPlugin:
			pLastPluginMenu = pMainMenu->GetSubMenu(pMainMenu->GetMenuItemCount() - 1);
			break;
		// Pre-defined editor menus
		case eMenuFile:
			pLastPluginMenu = pMainMenu->GetSubMenu(FindMenuItem(pMainMenu, "&File"));
			break;
		case eMenuEdit:
			pLastPluginMenu = pMainMenu->GetSubMenu(FindMenuItem(pMainMenu, "&Edit"));
			break;
		case eMenuInsert:
			pLastPluginMenu = pMainMenu->GetSubMenu(FindMenuItem(pMainMenu, "&Insert"));
			break;
		case eMenuGenerators:
			pLastPluginMenu = pMainMenu->GetSubMenu(FindMenuItem(pMainMenu, "&Generators"));
			break;
		case eMenuScript:
			pLastPluginMenu = pMainMenu->GetSubMenu(FindMenuItem(pMainMenu, "&Script"));
			break;
		case eMenuView:
			pLastPluginMenu = pMainMenu->GetSubMenu(FindMenuItem(pMainMenu, "&View"));
			break;
		case eMenuHelp:
			pLastPluginMenu = pMainMenu->GetSubMenu(FindMenuItem(pMainMenu, "&Help"));
			break;
		// Unknown identifier or parent ID passed
		default:
			CLogFile::WriteLine("AddMenuItem(): Unknown parent menu ID passed, incompatible plugin version ?");
			break;
	}

	// Add an menu item to the menu

	ASSERT(pLastPluginMenu);

	if (pLastPluginMenu)
	{
		// Insert the menu
		if (!pLastPluginMenu->AppendMenu(MF_STRING | bIsSeparator ? MF_SEPARATOR : NULL, dwMenuID, 
			pIHandler->GetUIElementName(iId)))
		{
			return false;
		}

		// Register the associated event and ID
		GetPluginManager()->AddHandlerForCmdID(pIAssociatedPlugin, iId, pIHandler);

		return true;
	}
	else
	{
		CLogFile::WriteLine("Can't find specified menu !");
		return false;
	}
	*/

	return true;
}

/*
//////////////////////////////////////////////////////////////////////////
int CEditorImpl::FindMenuItem(CMenu *pMenu, LPCTSTR pszMenuString)
{
	//////////////////////////////////////////////////////////////////////
	// FindMenuItem() will find a menu item string from the specified
	// popup menu and returns its position (0-based) in the specified 
	// popup menu. It returns -1 if no such menu item string is found
	//////////////////////////////////////////////////////////////////////

	int iCount, i;
	CString str;

  ASSERT(pMenu);
  ASSERT(::IsMenu(pMenu->GetSafeHmenu()));

  iCount = pMenu->GetMenuItemCount();

  for (i=0; i<iCount; i++)
  {
		if (pMenu->GetMenuString(i, str, MF_BYPOSITION) &&
			(strcmp(str, pszMenuString) == 0))
		{
      return i;
		}
  }

  return -1;
}
*/

/*
bool CEditorImpl::RegisterPluginToolTab(HWND hwndContainer, char *pszName)
{
	//////////////////////////////////////////////////////////////////////
	// Register a new tab with an UI container
	//////////////////////////////////////////////////////////////////////

	ASSERT(::IsWindow(hwndContainer));
	ASSERT(pszName);

	(CCryEditView *) ((CFrameWnd *) (AfxGetMainWnd())->
		GetActiveView())->RegisterPluginToolTab(hwndContainer, pszName);

	return true;
}

HWND CEditorImpl::GetToolbarTabContainer()
{
	//////////////////////////////////////////////////////////////////////
	// Return the window handle of the toolbar container
	//////////////////////////////////////////////////////////////////////

	return (CCryEditView *) ((CFrameWnd *) (AfxGetMainWnd())->
		GetActiveView())->GetToolTabContainerWnd();
}
*/

// Change the message in the status bar
void CEditorImpl::SetStatusText(const char * pszString) 
{
	if (!m_bMatEditMode && GetMainFrame())
		GetMainFrame()->SetStatusText(pszString);
};

bool CEditorImpl::CreateRootMenuItem(const char *pszName)
{
	//////////////////////////////////////////////////////////////////////
	// Create the root menu for the plugin
	//////////////////////////////////////////////////////////////////////
	/*

	CMenu *pMainMenu;
	
	IPlugin *pIAssociatedPlugin = GetPluginManager()->GetAssociatedPlugin();
	ASSERT(!IsBadReadPtr(pIAssociatedPlugin, sizeof(IPlugin)));

	// Get the main menu
	pMainMenu = AfxGetMainWnd()->GetMenu();

	// Insert the menu
	if (!pMainMenu->AppendMenu(MF_BYPOSITION | MF_POPUP | MF_STRING, 
		(UINT) pMainMenu->GetSafeHmenu(), pszName))
	{
		return false;
	}

	CLogFile::FormatLine("Root menu for plugin created ('%s')", pszName);
	*/

	return true;
}

//////////////////////////////////////////////////////////////////////////
int CEditorImpl::SelectRollUpBar( int rollupBarId )
{
	if (GetMainFrame())
		return GetMainFrame()->SelectRollUpBar( rollupBarId );
	else
		return 0;
}

//////////////////////////////////////////////////////////////////////////
int CEditorImpl::AddRollUpPage(int rollbarId,LPCTSTR pszCaption, class CDialog *pwndTemplate, 
		                                         bool bAutoDestroyTpl, int iIndex,bool bAutoExpand)
{
	if (!GetMainFrame())
		return 0;

	if (pwndTemplate && !pwndTemplate->m_hWnd)
	{
		if (IDYES == AfxMessageBox( _T("AddRollUpPage called with NULL window handle, Possibly not enough memory for window creation\r\nEditor is unstable and should be restarted.\r\nSave current level?"),MB_YESNO|MB_ICONERROR ))
		{
			SaveDocument();
		}
		return 0;
	}

	//CLogFile::FormatLine("Rollup page inserted ('%s')", pszCaption);
	if (!GetRollUpControl(rollbarId))
		return 0;
	// Preserve Focused window.
	HWND hFocusWnd = GetFocus();
	int id = GetRollUpControl(rollbarId)->InsertPage(pszCaption, pwndTemplate, bAutoDestroyTpl, iIndex,bAutoExpand);

	//GetRollUpControl(rollbarId)->ExpandPage(id,true,false);
	// Make sure focus stay in main wnd.
	if (hFocusWnd && GetFocus() != hFocusWnd)
		SetFocus(hFocusWnd);
	return id;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::RemoveRollUpPage(int rollbarId,int iIndex)
{
	if (!GetRollUpControl(rollbarId))
		return;
	GetRollUpControl(rollbarId)->RemovePage(iIndex);
}

//////////////////////////////////////////////////////////////////////////	
void CEditorImpl::ExpandRollUpPage(int rollbarId,int iIndex, BOOL bExpand)
{
	if (!GetRollUpControl(rollbarId))
		return;

	// Preserve Focused window.
	HWND hFocusWnd = GetFocus();

	GetRollUpControl(rollbarId)->ExpandPage(iIndex, bExpand);
	
	// Preserve Focused window.
	if (hFocusWnd && GetFocus() != hFocusWnd)
		SetFocus(hFocusWnd);
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::EnableRollUpPage(int rollbarId,int iIndex, BOOL bEnable)
{
	if (!GetRollUpControl(rollbarId))
		return;

	// Preserve Focused window.
	HWND hFocusWnd = GetFocus();

	GetRollUpControl(rollbarId)->EnablePage(iIndex, bEnable);
	
	// Preserve Focused window.
	if (hFocusWnd && GetFocus() != hFocusWnd)
		SetFocus(hFocusWnd);
}

//////////////////////////////////////////////////////////////////////////
HWND CEditorImpl::GetRollUpContainerWnd(int rollbarId)
{
	if (!GetRollUpControl(rollbarId))
		return 0;
	return GetRollUpControl(rollbarId)->GetSafeHwnd();
}		

//////////////////////////////////////////////////////////////////////////
int CEditorImpl::GetEditMode()
{
	return m_currEditMode;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetEditMode( int editMode )
{
	m_currEditMode = (EEditMode)editMode;
	m_prevEditMode = m_currEditMode;
	BBox box( Vec3(0,0,0),Vec3(0,0,0) );
	SetSelectedRegion( box );

	if (GetEditTool() && !GetEditTool()->IsNeedMoveTool())
	{
		SetEditTool(0,true);
	}
	
	if (editMode == eEditModeMove || editMode == eEditModeRotate || editMode == eEditModeScale)
	{
		SetAxisConstrains( m_lastAxis[editMode] );
		SetReferenceCoordSys( m_lastCoordSys[editMode] );
	}
	Notify( eNotify_OnEditModeChange );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetOperationMode( EOperationMode mode )
{
	m_operationMode = mode;
	gSettings.operationMode = mode;
}

//////////////////////////////////////////////////////////////////////////
EOperationMode CEditorImpl::GetOperationMode()
{
	return m_operationMode;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetEditTool( CEditTool *tool,bool bStopCurrentTool )
{
	if (tool == 0)
	{
		// Replace tool with the object modify edit tool.
		if (m_pEditTool != 0 && m_pEditTool->IsKindOf(RUNTIME_CLASS(CObjectMode)))
		{
			// Do not change.
			return;
		}
		else
		{
			tool = new CObjectMode;
		}
	}

	if (tool)
	{
		if (!tool->Activate(m_pEditTool))
			return;
	}

	if (bStopCurrentTool)
	{
		if (m_pEditTool != tool && m_pEditTool != 0)
		{
			m_pEditTool->EndEditParams();
			SetStatusText( "Ready" );
		}
	}
	
	/*
	if (tool != 0 && m_pEditTool == 0)
	{
		// Tool set.
		m_currEditMode = eEditModeTool;
	}
	if (tool == 0 && m_pEditTool != 0)
	{
		// Tool set.
		m_currEditMode = m_prevEditMode;
	}
	*/

	m_pEditTool = tool;
	if (m_pEditTool)
	{
		m_pEditTool->BeginEditParams( this,0 );
	}

	// Make sure pick is aborted.
	if (tool != m_pickTool)
	{
		m_pickTool = 0;
	}
	Notify( eNotify_OnEditToolChange );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetEditTool( const CString &sEditToolName,bool bStopCurrentTool )
{
	CEditTool *pTool = GetEditTool();
	if (pTool && pTool->GetClassDesc())
	{
		// Check if alredy selected.
		if (stricmp(pTool->GetClassDesc()->ClassName(),sEditToolName) == 0)
			return;
	}

	IClassDesc *pClass = GetIEditor()->GetClassFactory()->FindClass( sEditToolName );
	if (!pClass)
	{
		Warning( "Editor Tool %s not registered.",(const char*)sEditToolName );
		return;
	}
	if (pClass->SystemClassID() != ESYSTEM_CLASS_EDITTOOL)
	{
		Warning( "Class name %s is not a valid Edit Tool class.",(const char*)sEditToolName );
		return;
	}
	CRuntimeClass *pRtClass = pClass->GetRuntimeClass();
	if (pRtClass && pRtClass->IsDerivedFrom(RUNTIME_CLASS(CEditTool)))
	{
		CEditTool *pEditTool = (CEditTool*)pRtClass->CreateObject();
		GetIEditor()->SetEditTool(pEditTool);
		return;
	}
	else
	{
		Warning( "Class name %s is not a valid Edit Tool class.",(const char*)sEditToolName );
		return;
	}
}
	
//////////////////////////////////////////////////////////////////////////
CEditTool* CEditorImpl::GetEditTool()
{
	return m_pEditTool;
}

//////////////////////////////////////////////////////////////////////////
ITransformManipulator* CEditorImpl::ShowTransformManipulator( bool bShow )
{
	if (bShow)
	{
		if (!m_pAxisGizmo)
		{
			m_pAxisGizmo = new CAxisGizmo;
			m_pAxisGizmo->AddRef();
			GetObjectManager()->GetGizmoManager()->AddGizmo( m_pAxisGizmo );
		}
		return m_pAxisGizmo;
	}
	else
	{
		// Hide gizmo.
		if (m_pAxisGizmo)
		{
			GetObjectManager()->GetGizmoManager()->RemoveGizmo( m_pAxisGizmo );
			m_pAxisGizmo->Release();
		}
		m_pAxisGizmo = 0;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
ITransformManipulator* CEditorImpl::GetTransformManipulator()
{
	return m_pAxisGizmo;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetAxisConstrains( AxisConstrains axisFlags )
{
	m_selectedAxis = axisFlags;
	m_lastAxis[m_currEditMode] = m_selectedAxis;
	m_viewMan->SetAxisConstrain( axisFlags );
	SetTerrainAxisIgnoreObjects( false );

	// Update all views.
	UpdateViews( eUpdateObjects,NULL );
}

//////////////////////////////////////////////////////////////////////////
AxisConstrains CEditorImpl::GetAxisConstrains()
{
	return m_selectedAxis;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetTerrainAxisIgnoreObjects( bool bIgnore )
{
	m_bTerrainAxisIgnoreObjects = bIgnore;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsTerrainAxisIgnoreObjects()
{
	return m_bTerrainAxisIgnoreObjects;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetReferenceCoordSys( RefCoordSys refCoords )
{
	m_refCoordsSys = refCoords;
	m_lastCoordSys[m_currEditMode] = m_refCoordsSys;

	// Update all views.
	UpdateViews( eUpdateObjects,NULL );
}

//////////////////////////////////////////////////////////////////////////
RefCoordSys CEditorImpl::GetReferenceCoordSys()
{
	return m_refCoordsSys;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CEditorImpl::NewObject( const CString &typeName,const CString &file )
{
	SetModifiedFlag();
	return GetObjectManager()->NewObject( typeName,0,file );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::DeleteObject( CBaseObject *obj )
{
	SetModifiedFlag();
	GetObjectManager()->DeleteObject( obj );
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CEditorImpl::CloneObject( CBaseObject *obj )
{
	SetModifiedFlag();
	return GetObjectManager()->CloneObject( obj );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::StartObjectCreation( const CString &type,const CString &file )
{
	CObjectCreateTool *pTool = new CObjectCreateTool;
	GetIEditor()->SetEditTool( pTool );
	pTool->StartCreation( type,file );
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CEditorImpl::GetSelectedObject()
{
	CBaseObject *obj = 0;
	if (m_objectMan->GetSelection()->GetCount() != 1)
		return 0;
	return m_objectMan->GetSelection()->GetObject(0);
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SelectObject( CBaseObject *obj )
{
	GetObjectManager()->SelectObject( obj );
}

//////////////////////////////////////////////////////////////////////////
IObjectManager* CEditorImpl::GetObjectManager()
{
	return m_objectMan;
};

//////////////////////////////////////////////////////////////////////////
CSelectionGroup* CEditorImpl::GetSelection()
{
	return m_objectMan->GetSelection();
}

//////////////////////////////////////////////////////////////////////////
int CEditorImpl::ClearSelection()
{
	return m_objectMan->ClearSelection();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::LockSelection( bool bLock )
{
	// Selection must be not empty to enable selection lock.
	if (!GetSelection()->IsEmpty())
		m_bSelectionLocked = bLock;
	else
		m_bSelectionLocked = false;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsSelectionLocked()
{
	return m_bSelectionLocked;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::PickObject( IPickObjectCallback *callback,CRuntimeClass *targetClass,const char *statusText,bool bMultipick )
{
	m_pickTool = new CPickObjectTool(callback,targetClass);
	((CPickObjectTool*)m_pickTool)->SetMultiplePicks( bMultipick );
	if (statusText)
		m_pickTool->SetStatusText(statusText);

	SetEditTool( m_pickTool );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::CancelPick()
{
	SetEditTool(0);
	m_pickTool = 0;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsPicking()
{
	if (GetEditTool() == m_pickTool && m_pickTool != 0)
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
CViewManager* CEditorImpl::GetViewManager()
{
	return m_viewMan;
}

CViewport* CEditorImpl::GetActiveView()
{
	if (!GetMainFrame())
		return 0;

	CLayoutViewPane* viewPane = (CLayoutViewPane*) GetMainFrame()->GetActiveView();
	if (viewPane)
	{
		CWnd *pWnd = viewPane->GetViewport();
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CViewport)))
			return (CViewport*)pWnd;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::UpdateViews( int flags,BBox *updateRegion )
{
	BBox prevRegion = m_viewMan->GetUpdateRegion();
	if (updateRegion)
		m_viewMan->SetUpdateRegion( *updateRegion );
	m_viewMan->UpdateViews( flags );
	if (updateRegion)
		m_viewMan->SetUpdateRegion( prevRegion );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::UpdateTrackView( bool bOnlyKeys )
{
	if (bOnlyKeys)
		Notify( eNotify_OnUpdateTrackViewKeys );
	else
		Notify( eNotify_OnUpdateTrackView );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::ResetViews()
{
	m_viewMan->ResetViews();

	m_displaySettings->SetRenderFlags( m_displaySettings->GetRenderFlags() );
}

//////////////////////////////////////////////////////////////////////////
float CEditorImpl::GetTerrainElevation( float x,float y )
{
	I3DEngine *engine = m_system->GetI3DEngine();
	if (!engine)
		return 0;
	return engine->GetTerrainElevation( x,y );
}

CHeightmap* CEditorImpl::GetHeightmap()
{
	return &GetDocument()->m_cHeightmap;
}

CVegetationMap* CEditorImpl::GetVegetationMap()
{
	assert( GetHeightmap() != 0 );
	return GetHeightmap()->GetVegetationMap();
}

void CEditorImpl::SetSelectedRegion( const BBox &box )
{
	m_selectedRegion = box;
}
	
void CEditorImpl::GetSelectedRegion( BBox &box )
{
	box = m_selectedRegion;
}

//////////////////////////////////////////////////////////////////////////
CWnd* CEditorImpl::OpenView( const char *sViewName )
{
	if (!GetMainFrame())
		return 0;
	return GetMainFrame()->OpenPage( sViewName );
}

//////////////////////////////////////////////////////////////////////////
CWnd* CEditorImpl::FindView( const char *sViewName )
{
	if (!GetMainFrame())
		return 0;
	CMainFrame* pMainFrame = GetMainFrame();
	return pMainFrame ? pMainFrame->FindPage( sViewName ) : 0;
}

//////////////////////////////////////////////////////////////////////////
IDataBaseManager* CEditorImpl::GetDBItemManager( EDataBaseItemType itemType )
{
	switch (itemType)
	{
	case EDB_TYPE_MATERIAL:
		return m_materialManager;
	case EDB_TYPE_ENTITY_ARCHETYPE:
		return m_entityManager;
	case EDB_TYPE_PREFAB:
		return m_pPrefabManager;
	case EDB_TYPE_GAMETOKEN:
		return m_pGameTokenManager;
	case EDB_TYPE_PARTICLE:
		return m_particleManager;
	case EDB_TYPE_MUSIC:
		return m_pMusicManager;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryDialog* CEditorImpl::OpenDataBaseLibrary( EDataBaseItemType type,IDataBaseItem *pItem )
{
	if (pItem)
		type = pItem->GetType();

	CWnd *pWnd = NULL;
	if (type == EDB_TYPE_MATERIAL)
	{
		pWnd = OpenView( "Material Editor" );
	}
	else
	{
		pWnd = OpenView( "DataBase View" );
	}

	IDataBaseManager *pManager = GetDBItemManager(type);
	if (pManager)
		pManager->SetSelectedItem(pItem);

	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CBaseLibraryDialog)))
	{
		return (CBaseLibraryDialog*)pWnd;
	}
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CDataBaseDialog)))
	{
		CDataBaseDialog *dlgDB = (CDataBaseDialog*)pWnd;
		CDataBaseDialogPage *pPage = dlgDB->SelectDialog( type,pItem );
		if (pPage && pPage->IsKindOf(RUNTIME_CLASS(CBaseLibraryDialog)))
		{
			return (CBaseLibraryDialog*)pPage;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::SelectColor( COLORREF &color,CWnd *parent )
{
	COLORREF col = color;
	CCustomColorDialog dlg( col,CC_FULLOPEN,parent );
	if (dlg.DoModal() == IDOK)
	{
		color = dlg.GetColor();
		return true;
	}
	return false;
}

void CEditorImpl::SetInGameMode( bool inGame )
{
	if (m_gameEngine)
		m_gameEngine->SetGameMode(inGame);
}

bool CEditorImpl::IsInGameMode()
{
	if (m_gameEngine)
		return m_gameEngine->IsInGameMode();
	return false;
}

bool CEditorImpl::IsInTestMode()
{
	return ((CCryEditApp*)AfxGetApp())->IsInTestMode();
}

bool CEditorImpl::IsInPreviewMode()
{
	return ((CCryEditApp*)AfxGetApp())->IsInPreviewMode();
}

void CEditorImpl::EnableAcceleratos( bool bEnable )
{
	if (GetMainFrame())
		GetMainFrame()->EnableAccelerator( bEnable );
}

void CEditorImpl::DetectVersion()
{
	char exe[_MAX_PATH];
	DWORD dwHandle;
	UINT len;
	
	char ver[1024*8];
	
	GetModuleFileName( NULL, exe, _MAX_PATH );
	
	int verSize = GetFileVersionInfoSize( exe,&dwHandle );
	if (verSize > 0)
	{
		GetFileVersionInfo( exe,dwHandle,1024*8,ver );
		VS_FIXEDFILEINFO *vinfo;
		VerQueryValue( ver,"\\",(void**)&vinfo,&len );
		
		m_fileVersion.v[0] = vinfo->dwFileVersionLS & 0xFFFF;
		m_fileVersion.v[1] = vinfo->dwFileVersionLS >> 16;
		m_fileVersion.v[2] = vinfo->dwFileVersionMS & 0xFFFF;
		m_fileVersion.v[3] = vinfo->dwFileVersionMS >> 16;
		
		m_productVersion.v[0] = vinfo->dwProductVersionLS & 0xFFFF;
		m_productVersion.v[1] = vinfo->dwProductVersionLS >> 16;
		m_productVersion.v[2] = vinfo->dwProductVersionMS & 0xFFFF;
		m_productVersion.v[3] = vinfo->dwProductVersionMS >> 16;
		
		//debug( "<kernel> FileVersion: %d.%d.%d.%d",s_fileVersion.v[3],s_fileVersion.v[2],s_fileVersion.v[1],s_fileVersion.v[0] );
		//debug( "<kernel> ProductVersion: %d.%d.%d.%d",s_productVersion.v[3],s_productVersion.v[2],s_productVersion.v[1],s_productVersion.v[0] );
	}
}

XmlNodeRef CEditorImpl::FindTemplate( const CString &templateName )
{
	return m_templateRegistry.FindTemplate( templateName );
}
	
void CEditorImpl::AddTemplate( const CString &templateName,XmlNodeRef &tmpl )
{
	m_templateRegistry.AddTemplate( templateName,tmpl );
}

//////////////////////////////////////////////////////////////////////////
CShaderEnum* CEditorImpl::GetShaderEnum()
{
	return m_shaderEnum;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::ExecuteConsoleApp( const CString &CommandLine, CString &OutputText )
{
	////////////////////////////////////////////////////////////////////////
	// Execute a console application and redirect its output to the
	// console window
	////////////////////////////////////////////////////////////////////////

	SECURITY_ATTRIBUTES sa = { 0 };
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	HANDLE hPipeOutputRead = NULL;
	HANDLE hPipeOutputWrite = NULL;
	HANDLE hPipeInputRead = NULL;
	HANDLE hPipeInputWrite = NULL;
	BOOL bTest = FALSE;
	bool bReturn = true;
	DWORD dwNumberOfBytesRead = 0;
	DWORD dwStartTime = 0;
	char szCharBuffer[65];
	char szOEMBuffer[65];

	CLogFile::FormatLine("Executing console application '%s'", (const char*)CommandLine);
	
	// Initialize the SECURITY_ATTRIBUTES structure
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	// Create a pipe for standard output redirection
	VERIFY(CreatePipe(&hPipeOutputRead, &hPipeOutputWrite, &sa, 0));

	// Create a pipe for standard inout redirection
	VERIFY(CreatePipe(&hPipeInputRead, &hPipeInputWrite, &sa, 0));

	// Make a child process useing hPipeOutputWrite as standard out. Also
	// make sure it is not shown on the screen
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdInput = hPipeInputRead;
	si.hStdOutput = hPipeOutputWrite;
	si.hStdError = hPipeOutputWrite;

	// Save the process start time
	dwStartTime = GetTickCount();

	// Launch the console application
	char cmdLine[1024];
	strcpy( cmdLine,CommandLine );
	if (!CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
		return false;

	// If no process was spawned
	if (!pi.hProcess)
		bReturn = false;

	// Now that the handles have been inherited, close them
	CloseHandle(hPipeOutputWrite);
	CloseHandle(hPipeInputRead);

	// Capture the output of the console application by reading from hPipeOutputRead
	while (true)
	{
		// Read from the pipe
		bTest = ReadFile(hPipeOutputRead, &szOEMBuffer, 64, &dwNumberOfBytesRead, NULL);

		// Break when finished
		if (!bTest)
			break;

		// Break when timeout has been exceeded
		if (GetTickCount() - dwStartTime > 5000)
			break;

		// Null terminate string
		szOEMBuffer[dwNumberOfBytesRead] = '\0';

		// Translate into ANSI
		VERIFY(OemToChar(szOEMBuffer, szCharBuffer));

		// Add it to the output text
		OutputText += szCharBuffer;
	}

	// Wait for the process to finish
	WaitForSingleObject(pi.hProcess, 1000);

	return bReturn;
}

//////////////////////////////////////////////////////////////////////////
// Undo
//////////////////////////////////////////////////////////////////////////
void CEditorImpl::BeginUndo()
{
	if (m_undoManager)
		m_undoManager->Begin();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::RestoreUndo( bool undo )
{
	if (m_undoManager)
		m_undoManager->Restore(undo);
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::AcceptUndo( const CString &name )
{
	if (m_undoManager)
		m_undoManager->Accept(name);
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::CancelUndo()
{
	if (m_undoManager)
		m_undoManager->Cancel();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SuperBeginUndo()
{
	if (m_undoManager)
		m_undoManager->SuperBegin();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SuperAcceptUndo( const CString &name )
{
	if (m_undoManager)
		m_undoManager->SuperAccept(name);
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SuperCancelUndo()
{
	if (m_undoManager)
		m_undoManager->SuperCancel();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SuspendUndo()
{
	if (m_undoManager)
		m_undoManager->Suspend();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::ResumeUndo()
{
	if (m_undoManager)
		m_undoManager->Resume();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Undo()
{
	if (m_undoManager)
		m_undoManager->Undo();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Redo()
{
	if (m_undoManager)
		m_undoManager->Redo();
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsUndoRecording()
{
	if (m_undoManager)
		return m_undoManager->IsUndoRecording();
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsUndoSuspended()
{
	if (m_undoManager)
		return m_undoManager->IsUndoSuspended();
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::RecordUndo( IUndoObject *obj )
{
	if (m_undoManager)
		m_undoManager->RecordUndo( obj );
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::FlushUndo()
{
	if (m_undoManager)
		m_undoManager->Flush();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::SetConsoleVar( const char *var,float value )
{
	ICVar *ivar = GetSystem()->GetIConsole()->GetCVar( var );
	if (ivar)
		ivar->Set( value );
}

//////////////////////////////////////////////////////////////////////////
float CEditorImpl::GetConsoleVar( const char *var )
{
	ICVar *ivar = GetSystem()->GetIConsole()->GetCVar( var );
	if (ivar)
	{
		return ivar->GetFVal();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CAIManager*	CEditorImpl::GetAI()
{
	return m_AI;
}

//////////////////////////////////////////////////////////////////////////
CAnimationContext* CEditorImpl::GetAnimation()
{
	return m_animationContext;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::RegisterDocListener( IDocListener *listener )
{
	CCryEditDoc *doc = GetDocument();
	if (doc)
	{
		doc->RegisterListener( listener );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::UnregisterDocListener( IDocListener *listener )
{
	CCryEditDoc *doc = GetDocument();
	if (doc)
	{
		doc->UnregisterListener( listener );
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Notify( EEditorNotifyEvent event )
{
	std::list<IEditorNotifyListener*>::iterator it = m_listeners.begin();
	while (it != m_listeners.end())
	{
		(*it++)->OnEditorNotifyEvent(event);
	}
	if (event == eNotify_OnBeginNewScene)
	{
		if (m_pAxisGizmo)
			m_pAxisGizmo->Release();
		m_pAxisGizmo = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::RegisterNotifyListener( IEditorNotifyListener *listener )
{
	stl::push_back_unique(m_listeners,listener);
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::UnregisterNotifyListener( IEditorNotifyListener *listener )
{
	m_listeners.remove(listener);
}

//////////////////////////////////////////////////////////////////////////
ISourceControl* CEditorImpl::GetSourceControl()
{
	return m_pSourceControl->GetSCMInterface();
}
void CEditorImpl::SetMatEditMode(bool bIsMatEditMode)
{
	m_bMatEditMode = bIsMatEditMode;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::GetMemoryUsage( ICrySizer *pSizer )
{
	SIZER_COMPONENT_NAME( pSizer, "Editor" );

	if(GetDocument())
	{
		SIZER_COMPONENT_NAME( pSizer, "Document" );

		GetDocument()->GetMemoryUsage(pSizer);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::ReduceMemory()
{
	GetIEditor()->GetUndoManager()->ClearRedoStack();
	GetIEditor()->GetUndoManager()->ClearUndoStack();
	GetIEditor()->GetObjectManager()->SendEvent( EVENT_FREE_GAME_DATA );
	gEnv->pRenderer->FreeResources( FRR_TEXTURES );

	HANDLE hHeap = GetProcessHeap();
	if (hHeap)
	{
		uint64 maxsize = (uint64)HeapCompact(hHeap,0);
		CryLogAlways( "Max Free Memory Block = %I64d Kb",maxsize/1024 );
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialEditor* CEditorImpl::GetFacialEditor(bool openIfNecessary)
{
	CWnd* facialEditorWindow = 0;
	if (openIfNecessary)
		facialEditorWindow = ((CMainFrame*)AfxGetMainWnd())->OpenPage("Facial Editor");
	else
		facialEditorWindow = ((CMainFrame*)AfxGetMainWnd())->FindPage("Facial Editor");
	CFacialEditorDialog* facialEditor = (facialEditorWindow ? (CFacialEditorDialog*)facialEditorWindow : 0);
	IFacialEditor* facialEditorInterface = (facialEditor ? facialEditor : 0);
	return facialEditorInterface;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::AddUIEnums()
{
	// Add global enum for surface types.
  std::vector<const char*> types;
  types.push_back( "" ); // Push empty surface type.
  ISurfaceTypeEnumerator *pSurfaceTypeEnum = Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager()->GetEnumerator();
  for (ISurfaceType *pSurfaceType = pSurfaceTypeEnum->GetFirst(); pSurfaceType; pSurfaceType = pSurfaceTypeEnum->GetNext())
    types.push_back( pSurfaceType->GetName() );

	m_pUIEnumsDatabase->SetEnumStrings( "Surface", &*types.begin(), types.size() );
}

//////////////////////////////////////////////////////////////////////////
// Set current configuration spec of the editor.
void CEditorImpl::SetEditorConfigSpec( ESystemConfigSpec spec )
{
	gSettings.editorConfigSpec = spec;
	if (m_system->GetConfigSpec(true) != spec)
	{
		m_system->SetConfigSpec( spec,true );
		gSettings.editorConfigSpec = m_system->GetConfigSpec(true);
		Notify( eNotify_OnConfigSpecChange );
		GetObjectManager()->SendEvent( EVENT_CONFIG_SPEC_CHANGE );
	}
}

//////////////////////////////////////////////////////////////////////////
ESystemConfigSpec CEditorImpl::GetEditorConfigSpec() const
{
	return gSettings.editorConfigSpec;
}
