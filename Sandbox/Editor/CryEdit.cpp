// CryEdit.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"

#include <gdiplus.h>
#pragma comment (lib, "Gdiplus.lib")

#include "CryEdit.h"

#include "GameExporter.h"
#include "GameResourcesExporter.h"
#include "Brush\BrushExporter.h"

#include "MainFrm.h"
#include "CryEditDoc.h"
#include "ViewPane.h"
#include "StartupDialog.h"
#include "StringDlg.h"
#include "NumberDlg.h"
#include "SelectObjectDlg.h"
#include "LinkTool.h"
#include "AlignTool.h"
#include "VoxelAligningTool.h"
#include "missionscript.h"
#include "NewLevelDialog.h"
#include "TerrainDialog.h"
#include "SkyDialog.h"
#include "TerrainLighting.h"
#include "TerrainTexture.h"
#include "SetHeightDlg.h"
#include "VegetationMap.h"
#include "GridSettingsDialog.h"
#include "LayoutConfigDialog.h"

#ifdef USING_LICENSE_PROTECTION
#include "Dialogs/LicenseKeyDialog.h"
#include "Dialogs/VersionUpdateDialog.h"
#endif // USING_LICENSE_PROTECTION

#include "ProcessInfo.h"

#include "ViewManager.h"
#include "ModelViewport.h"
#include "CharacterEditor\ModelViewportCE.h"
#include "RenderViewport.h"
#include "FileTypeUtils.h"

#include "PluginManager.h"
#include "Objects\ObjectManager.h"
#include "Objects\Group.h"
#include "Objects\AIPoint.h"
#include "Objects\CloudGroup.h"

#include "Prefabs\PrefabManager.h"

#include "IEditorImpl.h"
#include "StartupLogoDialog.h"
#include "DisplaySettings.h"
#include "Mailer.h"

#include "ObjectCloneTool.h"
#include "Brush\BrushTool.h"

#include "Mission.h"
#include "MissionSelectDialog.h"

#include "CustomFileDialog.h"
#include "TipDlg.h"

#include "EquipPackDialog.h"

#include "Undo\\Undo.h"
#include "Objects\\EntityScript.h"

#include "WeaponProps.h"
#include "MissionProps.h"
#include "ThumbnailGenerator.h"
#include "LayersSelectDialog.h"
#include "ToolsConfigPage.h"

#include "TrackViewDialog.h"
#include "GameEngine.h"
#include "LMCompDialog.h"
#include "LightmapCompiler/SceneContext.h"
#include "SrcSafeSettingsDialog.h"
#include "IEngineSettingsManager.h"

#include "AI\AIManager.h"
#include "AI\GenerateSpawners.h"

#include "TerrainMoveTool.h"
#include "ExternalTools.h"
#include "Settings.h"
#include "Geometry\EdMesh.h"
#include "LightmapGen.h"
#include "Material\MaterialDialog.h"

#include "LevelInfo.h"
#include "DynamicHelpDialog.h"
#include "PreferencesDialog.h"

#include "MatEditMainDlg.h"

#include "ExportObjects.h"

#include <io.h>
#include <IScriptSystem.h>
#include <IEntitySystem.h>
#include <I3DEngine.h>
#include <ITimer.h>
#include <ISound.h>
#include <ICryAnimation.h>
#include <IPhysics.h>

#include "ObjectBrowserDialog.h"
#include "TimeOfDayDialog.h"
#include "CryEdit.h"
#include "ShaderCache.h"
#include "GotoPositionDlg.h"
#include "TerrainTextureExport.h"

#include "ConsoleDialog.h"

#include "StringUtils.h"

#include <ICmdLine.h>

#ifdef USING_UNIKEY_SECURITY
#include "UniKeySecurity/IUniKeyManager.h"
#endif // USING_UNIKEY_SECURITY

#ifdef USING_LICENSE_PROTECTION
#include "IProtectionManager.h"
#endif // USING_LICENSE_PROTECTION

/////////////////////////2////////////////////////////////////////////////////
// CCryEditApp

BEGIN_MESSAGE_MAP(CCryEditApp, CWinApp)
	//{{AFX_MSG_MAP(CCryEditApp)
	ON_THREAD_MESSAGE( WM_FILEMONITORCHANGE,OnFileMonitorChange )
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_TERRAIN, ToolTerrain)
	ON_COMMAND(IDC_SKY, ToolSky)
	ON_COMMAND(ID_GENERATORS_LIGHTING, ToolLighting)
	ON_COMMAND(ID_TERRAIN_TEXTURE_EXPORT, TerrainTextureExport)
	ON_COMMAND(ID_GENERATORS_TEXTURE, ToolTexture)
	ON_COMMAND(ID_FILE_EXPORTTOGAME, ExportToGame)
	ON_COMMAND(ID_FILE_EXPORTTOOBJ, ExportToOBJ)
	ON_COMMAND(ID_EDIT_HOLD, OnEditHold)
	ON_COMMAND(ID_EDIT_FETCH, OnEditFetch)
	ON_COMMAND(ID_GENERATORS_STATICOBJECTS, OnGeneratorsStaticobjects)
	ON_COMMAND(ID_FILE_EXPORTTOGAMENOSURFACETEXTURE, OnFileExportToGameNoSurfaceTexture)
	ON_COMMAND(ID_VIEW_SWITCHTOGAME, OnViewSwitchToGame)
	ON_COMMAND(ID_EDIT_SELECTALL, OnEditSelectAll)
	ON_COMMAND(ID_EDIT_SELECTNONE, OnEditSelectNone)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_COMMAND(ID_MOVE_OBJECT, OnMoveObject)
	ON_COMMAND(ID_SELECT_OBJECT, OnSelectObject)
	ON_COMMAND(ID_RENAME_OBJ, OnRenameObj)
	ON_COMMAND(ID_SET_HEIGHT, OnSetHeight)
	ON_COMMAND(ID_SCRIPT_COMPILESCRIPT, OnScriptCompileScript)
	ON_COMMAND(ID_SCRIPT_EDITSCRIPT, OnScriptEditScript)
	ON_COMMAND(ID_EDITMODE_MOVE, OnEditmodeMove)
	ON_COMMAND(ID_EDITMODE_ROTATE, OnEditmodeRotate)
	ON_COMMAND(ID_EDITMODE_SCALE, OnEditmodeScale)
	ON_COMMAND(ID_EDITTOOL_LINK, OnEditToolLink)
	ON_COMMAND(ID_EDITTOOL_UNLINK, OnEditToolUnlink)
	ON_COMMAND(ID_EDITMODE_SELECT, OnEditmodeSelect)
	ON_COMMAND(ID_SELECTION_DELETE, OnSelectionDelete)
	ON_COMMAND(ID_EDIT_ESCAPE, OnEditEscape)
	ON_COMMAND(ID_OBJECTMODIFY_SETAREA, OnObjectSetArea)
	ON_COMMAND(ID_OBJECTMODIFY_SETHEIGHT, OnObjectSetHeight)
	ON_COMMAND(ID_MODIFY_ALIGNOBJTOSURF, OnAlignToVoxel)
	ON_UPDATE_COMMAND_UI(ID_MODIFY_ALIGNOBJTOSURF, OnUpdateAlignToVoxel)
	ON_UPDATE_COMMAND_UI(ID_EDITTOOL_LINK, OnUpdateEditToolLink)
	ON_UPDATE_COMMAND_UI(ID_EDITTOOL_UNLINK, OnUpdateEditToolUnlink)
	ON_UPDATE_COMMAND_UI(ID_EDITMODE_SELECT, OnUpdateEditmodeSelect)
	ON_UPDATE_COMMAND_UI(ID_EDITMODE_MOVE, OnUpdateEditmodeMove)
	ON_UPDATE_COMMAND_UI(ID_EDITMODE_ROTATE, OnUpdateEditmodeRotate)
	ON_UPDATE_COMMAND_UI(ID_EDITMODE_SCALE, OnUpdateEditmodeScale)
	ON_COMMAND(ID_OBJECTMODIFY_FREEZE, OnObjectmodifyFreeze)
	ON_COMMAND(ID_OBJECTMODIFY_UNFREEZE, OnObjectmodifyUnfreeze)
	ON_COMMAND(ID_EDITMODE_SELECTAREA, OnEditmodeSelectarea)
	ON_UPDATE_COMMAND_UI(ID_EDITMODE_SELECTAREA, OnUpdateEditmodeSelectarea)
	ON_COMMAND(ID_SELECT_AXIS_X, OnSelectAxisX)
	ON_COMMAND(ID_SELECT_AXIS_Y, OnSelectAxisY)
	ON_COMMAND(ID_SELECT_AXIS_Z, OnSelectAxisZ)
	ON_COMMAND(ID_SELECT_AXIS_XY, OnSelectAxisXy)
	ON_UPDATE_COMMAND_UI(ID_SELECT_AXIS_X, OnUpdateSelectAxisX)
	ON_UPDATE_COMMAND_UI(ID_SELECT_AXIS_XY, OnUpdateSelectAxisXy)
	ON_UPDATE_COMMAND_UI(ID_SELECT_AXIS_Y, OnUpdateSelectAxisY)
	ON_UPDATE_COMMAND_UI(ID_SELECT_AXIS_Z, OnUpdateSelectAxisZ)
	ON_COMMAND(ID_UNDO, OnUndo)
	ON_COMMAND(ID_EDIT_CLONE, OnEditClone)
	ON_COMMAND(ID_EXPORT_TERRAIN_GEOM, OnExportTerrainGeom)
	ON_UPDATE_COMMAND_UI(ID_EXPORT_TERRAIN_GEOM, OnUpdateExportTerrainGeom)
	ON_COMMAND(ID_SELECTION_SAVE, OnSelectionSave)
	ON_COMMAND(ID_SELECTION_LOAD, OnSelectionLoad)
	ON_COMMAND(ID_GOTO_SELECTED, OnGotoSelected)
	ON_UPDATE_COMMAND_UI(ID_GOTO_SELECTED, OnUpdateSelected)
	ON_COMMAND(ID_OBJECTMODIFY_ALIGN, OnAlignObject)
	ON_UPDATE_COMMAND_UI(ID_OBJECTMODIFY_ALIGN, OnUpdateAlignObject)
	ON_COMMAND(ID_MODIFY_ALIGNOBJTOSURF, OnAlignToVoxel)
	ON_UPDATE_COMMAND_UI(ID_MODIFY_ALIGNOBJTOSURF, OnUpdateAlignToVoxel)
	ON_COMMAND(ID_OBJECTMODIFY_ALIGNTOGRID, OnAlignToGrid)
	ON_UPDATE_COMMAND_UI(ID_OBJECTMODIFY_ALIGNTOGRID, OnUpdateSelected)
	ON_COMMAND(ID_GROUP_ATTACH, OnGroupAttach)
	ON_UPDATE_COMMAND_UI(ID_GROUP_ATTACH, OnUpdateGroupAttach)
	ON_COMMAND(ID_GROUP_CLOSE, OnGroupClose)
	ON_UPDATE_COMMAND_UI(ID_GROUP_CLOSE, OnUpdateGroupClose)
	ON_COMMAND(ID_GROUP_DETACH, OnGroupDetach)
	ON_UPDATE_COMMAND_UI(ID_GROUP_DETACH, OnUpdateGroupDetach)
	ON_COMMAND(ID_GROUP_MAKE, OnGroupMake)
	ON_UPDATE_COMMAND_UI(ID_GROUP_MAKE, OnUpdateGroupMake)
	ON_COMMAND(ID_GROUP_OPEN, OnGroupOpen)
	ON_UPDATE_COMMAND_UI(ID_GROUP_OPEN, OnUpdateGroupOpen)
	ON_COMMAND(ID_GROUP_UNGROUP, OnGroupUngroup)
	ON_UPDATE_COMMAND_UI(ID_GROUP_UNGROUP, OnUpdateGroupUngroup)
	ON_COMMAND(ID_CLOUDS_CREATE, OnCloudsCreate)
	ON_UPDATE_COMMAND_UI(ID_CLOUDS_CREATE, OnUpdateSelected)
	ON_COMMAND(ID_CLOUDS_DESTROY, OnCloudsDestroy)
	ON_UPDATE_COMMAND_UI(ID_CLOUDS_DESTROY, OnUpdateCloudsDestroy)
	ON_COMMAND(ID_CLOUDS_OPEN, OnCloudsOpen)
	ON_UPDATE_COMMAND_UI(ID_CLOUDS_OPEN, OnUpdateCloudsOpen)
	ON_COMMAND(ID_CLOUDS_CLOSE, OnCloudsClose)
	ON_UPDATE_COMMAND_UI(ID_CLOUDS_CLOSE, OnUpdateCloudsClose)
	ON_COMMAND(ID_MISSION_NEW, OnMissionNew)
	ON_COMMAND(ID_MISSION_DELETE, OnMissionDelete)
	ON_COMMAND(ID_MISSION_DUPLICATE, OnMissionDuplicate)
	ON_COMMAND(ID_MISSION_PROPERTIES, OnMissionProperties)
	ON_COMMAND(ID_MISSION_RENAME, OnMissionRename)
	ON_COMMAND(ID_MISSION_SELECT, OnMissionSelect)
	ON_COMMAND(ID_MISSION_RELOAD, OnMissionReload)
	ON_COMMAND(ID_MISSION_EDIT, OnMissionEdit)
	ON_COMMAND(ID_SHOW_TIPS, OnShowTips)
	ON_COMMAND(ID_LOCK_SELECTION, OnLockSelection)
	ON_COMMAND(ID_EDIT_LEVELDATA, OnEditLevelData)
	ON_COMMAND(ID_FILE_EDITLOGFILE, OnFileEditLogFile)
	ON_COMMAND(ID_FILE_EDITEDITORINI, OnFileEditEditorini)
	ON_COMMAND(ID_SELECT_AXIS_TERRAIN, OnSelectAxisTerrain)
	ON_COMMAND(ID_SELECT_AXIS_SNAPTOALL, OnSelectAxisSnapToAll)
	ON_UPDATE_COMMAND_UI(ID_SELECT_AXIS_TERRAIN, OnUpdateSelectAxisTerrain)
	ON_UPDATE_COMMAND_UI(ID_SELECT_AXIS_SNAPTOALL, OnUpdateSelectAxisSnapToAll)
	ON_COMMAND(ID_PREFERENCES, OnPreferences)
	ON_BN_CLICKED(ID_RELOAD_TEXTURES, OnReloadTextures)
	ON_COMMAND(ID_RELOAD_SCRIPTS, OnReloadScripts)
	ON_COMMAND(ID_RELOAD_GEOMETRY, OnReloadGeometry)
	ON_COMMAND(ID_RELOAD_TERRAIN, OnReloadTerrain)
	ON_COMMAND(ID_REDO, OnRedo)
	ON_UPDATE_COMMAND_UI(ID_REDO, OnUpdateRedo)
	ON_UPDATE_COMMAND_UI(ID_OBJECTMODIFY_SETAREA, OnUpdateSelected)
	ON_UPDATE_COMMAND_UI(ID_OBJECTMODIFY_SETHEIGHT, OnUpdateSelected)
	ON_UPDATE_COMMAND_UI(ID_OBJECTMODIFY_FREEZE, OnUpdateSelected)
	ON_UPDATE_COMMAND_UI(ID_OBJECTMODIFY_UNFREEZE, OnUpdateFreezed)
	ON_COMMAND(ID_RELOAD_TEXTURES, OnReloadTextures)
	ON_UPDATE_COMMAND_UI(ID_SELECTION_SAVE, OnUpdateSelected)
	ON_UPDATE_COMMAND_UI(ID_UNDO, OnUpdateUndo)
	ON_COMMAND(ID_FILE_NEW, OnCreateLevel)
	ON_COMMAND(ID_FILE_OPEN, OnOpenLevel)
	ON_COMMAND(ID_TERRAIN_COLLISION, OnTerrainCollision)
	ON_UPDATE_COMMAND_UI(ID_TERRAIN_COLLISION, OnTerrainCollisionUpdate)
	ON_COMMAND(ID_RESOURCES_GENERATECGFTHUMBNAILS, OnGenerateCgfThumbnails)
	ON_COMMAND(ID_AI_GENERATEALLNAVIGATION, OnAiGenerateAllNavigation)
	ON_COMMAND(ID_AI_GENERATETRIANGULATION, OnAiGenerateTriangulation)
	ON_COMMAND(ID_AI_GENERATEWAYPOINT, OnAiGenerateWaypoint)
	ON_COMMAND(ID_AI_GENERATEFLIGHTNAVIGATION, OnAiGenerateFlightNavigation)
	ON_COMMAND(ID_AI_GENERATE3DVOLUMES, OnAiGenerate3dvolumes)
	ON_COMMAND(ID_AI_VALIDATENAVIGATION, OnAiValidateNavigation)
	ON_COMMAND(ID_AI_GENERATE3DDEBUGVOXELS, OnAiGenerate3DDebugVoxels)
	ON_COMMAND(ID_AI_GENERATESPAWNERS, OnAiGenerateSpawners)
	ON_COMMAND(ID_LAYER_SELECT, OnLayerSelect)
	ON_COMMAND(ID_SWITCH_PHYSICS, OnSwitchPhysics)
	ON_COMMAND(ID_GAME_SYNCPLAYER, OnSyncPlayer)
	ON_UPDATE_COMMAND_UI(ID_SWITCH_PHYSICS, OnSwitchPhysicsUpdate)
	ON_UPDATE_COMMAND_UI(ID_GAME_SYNCPLAYER, OnSyncPlayerUpdate)
	ON_COMMAND(ID_REF_COORDS_SYS, OnRefCoordsSys)
	ON_UPDATE_COMMAND_UI(ID_REF_COORDS_SYS, OnUpdateRefCoordsSys)
	ON_COMMAND(ID_RESOURCES_REDUCEWORKINGSET, OnResourcesReduceworkingset)
	ON_COMMAND(ID_TOOLS_GENERATELIGHTMAPS, OnToolsGeneratelightmaps)
	ON_COMMAND(ID_TOOLS_EQUIPPACKSEDIT, OnToolsEquipPacksEdit)
	ON_COMMAND(ID_TOOLS_UPDATEPROCEDURALVEGETATION, OnToolsUpdateProcVegetation)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_EDIT_HIDE, OnEditHide)
	ON_UPDATE_COMMAND_UI(ID_EDIT_HIDE, OnUpdateEditHide)
	ON_COMMAND(ID_EDIT_UNHIDEALL, OnEditUnhideall)
	ON_COMMAND(ID_EDIT_FREEZE, OnEditFreeze)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FREEZE, OnUpdateEditFreeze)
	ON_COMMAND(ID_EDIT_UNFREEZEALL, OnEditUnfreezeall)

	ON_COMMAND(ID_SNAP_TO_GRID, OnSnap)
	ON_UPDATE_COMMAND_UI(ID_SNAP_TO_GRID, OnUpdateEditmodeSnap)

	ON_COMMAND(ID_WIREFRAME, OnWireframe)
	ON_UPDATE_COMMAND_UI(ID_WIREFRAME, OnUpdateWireframe)
	ON_COMMAND(ID_VIEW_GRIDSETTINGS, OnViewGridsettings)
	ON_COMMAND(ID_VIEW_CONFIGURELAYOUT, OnViewConfigureLayout)

	ON_COMMAND(IDC_MISSION, OnDummyCommand )
	ON_COMMAND(IDC_SELECTION, OnDummyCommand )
	//////////////////////////////////////////////////////////////////////////
	ON_COMMAND(ID_TAG_LOC1, OnTagLocation1)
	ON_COMMAND(ID_TAG_LOC2, OnTagLocation2)
	ON_COMMAND(ID_TAG_LOC3, OnTagLocation3)
	ON_COMMAND(ID_TAG_LOC4, OnTagLocation4)
	ON_COMMAND(ID_TAG_LOC5, OnTagLocation5)
	ON_COMMAND(ID_TAG_LOC6, OnTagLocation6)
	ON_COMMAND(ID_TAG_LOC7, OnTagLocation7)
	ON_COMMAND(ID_TAG_LOC8, OnTagLocation8)
	ON_COMMAND(ID_TAG_LOC9, OnTagLocation9)
	ON_COMMAND(ID_TAG_LOC10, OnTagLocation10)
	ON_COMMAND(ID_TAG_LOC11, OnTagLocation11)
	ON_COMMAND(ID_TAG_LOC12, OnTagLocation12)
	//////////////////////////////////////////////////////////////////////////
	ON_COMMAND(ID_GOTO_LOC1, OnGotoLocation1)
	ON_COMMAND(ID_GOTO_LOC2, OnGotoLocation2)
	ON_COMMAND(ID_GOTO_LOC3, OnGotoLocation3)
	ON_COMMAND(ID_GOTO_LOC4, OnGotoLocation4)
	ON_COMMAND(ID_GOTO_LOC5, OnGotoLocation5)
	ON_COMMAND(ID_GOTO_LOC6, OnGotoLocation6)
	ON_COMMAND(ID_GOTO_LOC7, OnGotoLocation7)
	ON_COMMAND(ID_GOTO_LOC8, OnGotoLocation8)
	ON_COMMAND(ID_GOTO_LOC9, OnGotoLocation9)
	ON_COMMAND(ID_GOTO_LOC10, OnGotoLocation10)
	ON_COMMAND(ID_GOTO_LOC11, OnGotoLocation11)
	ON_COMMAND(ID_GOTO_LOC12, OnGotoLocation12)
	//////////////////////////////////////////////////////////////////////////

	ON_COMMAND(ID_TOOLS_LOGMEMORYUSAGE, OnToolsLogMemoryUsage)
	ON_COMMAND(ID_TERRAIN_EXPORTBLOCK, OnTerrainExportblock)
	ON_COMMAND(ID_TERRAIN_IMPORTBLOCK, OnTerrainImportblock)
	ON_UPDATE_COMMAND_UI(ID_TERRAIN_EXPORTBLOCK, OnUpdateTerrainExportblock)
	ON_UPDATE_COMMAND_UI(ID_TERRAIN_IMPORTBLOCK, OnUpdateTerrainImportblock)
	ON_COMMAND(ID_TOOLS_CUSTOMIZEKEYBOARD, OnCustomizeKeyboard )
	ON_COMMAND(ID_TOOLS_CONFIGURETOOLS, OnToolsConfiguretools)
	ON_COMMAND(ID_BRUSH_TOOL, OnBrushTool)
	ON_UPDATE_COMMAND_UI(ID_BRUSH_TOOL, OnUpdateBrushTool)
	ON_COMMAND(ID_EXPORT_INDOORS, OnExportIndoors)
	ON_COMMAND(ID_VIEW_CYCLE2DVIEWPORT, OnViewCycle2dviewport)
	ON_COMMAND(ID_DISPLAY_GOTOPOSITION, OnDisplayGotoPosition)
	ON_COMMAND(ID_SNAPANGLE, OnSnapangle)
	ON_UPDATE_COMMAND_UI(ID_SNAPANGLE, OnUpdateSnapangle)
	ON_COMMAND(ID_ROTATESELECTION_XAXIS, OnRotateselectionXaxis)
	ON_COMMAND(ID_ROTATESELECTION_YAXIS, OnRotateselectionYaxis)
	ON_COMMAND(ID_ROTATESELECTION_ZAXIS, OnRotateselectionZaxis)
	ON_COMMAND(ID_ROTATESELECTION_ROTATEANGLE, OnRotateselectionRotateangle)
	ON_COMMAND(ID_CONVERTSELECTION_TOBRUSHES, OnConvertselectionTobrushes)
	ON_COMMAND(ID_CONVERTSELECTION_TOSIMPLEENTITY, OnConvertselectionTosimpleentity)
	ON_UPDATE_COMMAND_UI(ID_CONVERTSELECTION_TOBRUSHES, OnUpdateSelected)
	ON_UPDATE_COMMAND_UI(ID_CONVERTSELECTION_TOSIMPLEENTITY, OnUpdateSelected)
	ON_COMMAND(ID_EDIT_RENAMEOBJECT, OnEditRenameobject)
	ON_COMMAND(ID_CHANGEMOVESPEED_INCREASE, OnChangemovespeedIncrease)
	ON_COMMAND(ID_CHANGEMOVESPEED_DECREASE, OnChangemovespeedDecrease)
	ON_COMMAND(ID_CHANGEMOVESPEED_CHANGESTEP, OnChangemovespeedChangestep)
	ON_COMMAND(ID_MODIFY_AIPOINT_PICKLINK, OnModifyAipointPicklink)
	ON_COMMAND(ID_MODIFY_AIPOINT_PICKIMPASSLINK, OnModifyAipointPickImpasslink)
	ON_COMMAND(ID_GEN_LIGHTMAPS_SELECTED, OnGenLightmapsSelected)
	ON_UPDATE_COMMAND_UI(ID_GEN_LIGHTMAPS_SELECTED, OnUpdateSelected)
	ON_COMMAND(ID_MATERIAL_ASSIGNCURRENT, OnMaterialAssigncurrent)
	ON_COMMAND(ID_MATERIAL_RESETTODEFAULT, OnMaterialResettodefault)
	ON_COMMAND(ID_MATERIAL_GETMATERIAL, OnMaterialGetmaterial)
	ON_COMMAND(ID_TOOLS_UPDATELIGHTMAPS, OnToolsUpdatelightmaps)
	ON_COMMAND(ID_PHYSICS_GETPHYSICSSTATE,OnPhysicsGetState )
	ON_COMMAND(ID_PHYSICS_RESETPHYSICSSTATE,OnPhysicsResetState )
	ON_COMMAND(ID_PHYSICS_SIMULATEOBJECTS,OnPhysicsSimulateObjects )
	ON_UPDATE_COMMAND_UI(ID_PHYSICS_GETPHYSICSSTATE, OnUpdateSelected)
	ON_UPDATE_COMMAND_UI(ID_PHYSICS_RESETPHYSICSSTATE, OnUpdateSelected)
	ON_UPDATE_COMMAND_UI(ID_PHYSICS_SIMULATEOBJECTS, OnUpdateSelected)
	//ON_COMMAND(ID_FILE_SOURCESAFESETTINGS, OnFileSourcesafesettings)
	ON_COMMAND(ID_FILE_SAVELEVELRESOURCES, OnFileSavelevelresources)
	ON_COMMAND(ID_VALIDATELEVEL, OnValidatelevel)
	ON_COMMAND(ID_HELP_DYNAMICHELP, OnHelpDynamichelp)
	ON_COMMAND(ID_FILE_CHANGEMOD, OnFileChangemod)
	ON_COMMAND(ID_TERRAIN_RESIZETERRAIN, OnTerrainResizeterrain)
	ON_COMMAND(ID_TOOLS_PREFERENCES, OnToolsPreferences)
	ON_COMMAND(ID_EDIT_INVERTSELECTION, OnEditInvertselection)
	ON_COMMAND(ID_PREFABS_MAKEFROMSELECTION, OnPrefabsMakeFromSelection)
	ON_COMMAND(ID_PREFABS_REFRESHALL, OnPrefabsRefreshAll)
	ON_COMMAND(ID_PREFABS_ADDSELECTIONTOPREFAB, OnAddSelectionToPrefab)
	ON_COMMAND(ID_TOOLTERRAINMODIFY_SMOOTH, OnToolterrainmodifySmooth)
	ON_COMMAND(ID_TERRAINMODIFY_SMOOTH, OnTerrainmodifySmooth)
	ON_COMMAND(ID_TERRAIN_VEGETATION, OnTerrainVegetation)
	ON_COMMAND(ID_TERRAIN_PAINTLAYERS, OnTerrainPaintlayers)
	ON_COMMAND(ID_AVIRECORDER_STARTAVIRECORDING, OnAvirecorderStartavirecording)
	ON_COMMAND(ID_AVIRECORDER_STOPAVIRECORDING, OnAviRecorderStop)
	ON_COMMAND(ID_AVIRECORDER_PAUSEAVIRECORDING, OnAviRecorderPause)
	ON_COMMAND(ID_AVIRECORDER_OUTPUTFILENAME, OnAviRecorderOutputFilename)
	ON_COMMAND(ID_SWITCHCAMERA_DEFAULTCAMERA, OnSwitchcameraDefaultcamera)
	ON_COMMAND(ID_SWITCHCAMERA_SEQUENCECAMERA, OnSwitchcameraSequencecamera)
	ON_COMMAND(ID_SWITCHCAMERA_SELECTEDCAMERA, OnSwitchcameraSelectedcamera)
	ON_COMMAND(ID_OPEN_MATERIAL_EDITOR, OnOpenMaterialEditor)
	ON_COMMAND(ID_OPEN_CHARACTER_EDITOR, OnOpenCharacterEditor)
	ON_COMMAND(ID_OPEN_DATABASE, OnOpenDataBaseView)
	ON_COMMAND(ID_OPEN_FLOWGRAPH, OnOpenFlowGraphView)
	ON_COMMAND(ID_BRUSH_RESETTRANSFORM, OnBrushResettransform)
	ON_COMMAND(ID_BRUSH_MAKEHOLLOW, OnBrushMakehollow)
	ON_COMMAND(ID_BRUSH_CSGCOMBINE, OnBrushCsgcombine)
	ON_COMMAND(ID_BRUSH_CSGSUBSTRUCT, OnBrushCsgsubstruct)
	ON_COMMAND(ID_BRUSH_CLIPTOOL, OnBrushCliptool)
	ON_COMMAND(ID_BRUSH_UVTOOL, OnBrushUvtool)
	ON_COMMAND(ID_BRUSH_CSGINTERSECT, OnBrushCsgintersect)
	ON_COMMAND(ID_SUBOBJECTMODE_VERTEX, OnSubobjectmodeVertex)
	ON_COMMAND(ID_SUBOBJECTMODE_EDGE, OnSubobjectmodeEdge)
	ON_COMMAND(ID_SUBOBJECTMODE_FACE, OnSubobjectmodeFace)
	ON_COMMAND(ID_SUBOBJECTMODE_POLYGON, OnSubobjectmodePolygon)
	ON_COMMAND(ID_BRUSH_CSGSUBSTRUCT2, OnBrushCsgsubstruct2)
	ON_COMMAND(ID_MATERIAL_PICKTOOL, OnMaterialPicktool)
	ON_COMMAND(ID_DISPLAY_SHOWHELPERS, OnShowHelpers)
	ON_COMMAND(ID_TERRAIN_TIMEOFDAY, OnTimeOfDay)
	ON_COMMAND(ID_TOOLS_CLEARLEVELSHADERCACHE,OnClearLevelShaderList)
	ON_COMMAND(ID_TOOLS_GENERATEVERTEXSCATTERING,OnGenerateVertexScattering)
	//ON_UPDATE_COMMAND_UI(ID_SHOW_HELPERS, OnUpdateGroupDetach)

	ON_COMMAND_RANGE(ID_GAME_ENABLELOWSPEC,ID_GAME_ENABLEVERYHIGHSPEC, OnChangeGameSpec)
	ON_UPDATE_COMMAND_UI_RANGE(ID_GAME_ENABLELOWSPEC,ID_GAME_ENABLEVERYHIGHSPEC, OnUpdateGameSpec)
	
	ON_UPDATE_COMMAND_UI(ID_GAME_ENABLESKETCHMODE, OnUpdateSketchMode)
	ON_COMMAND(ID_GAME_ENABLESKETCHMODE,OnGameEnableSketchMode)

	ON_COMMAND(ID_START_STOP,OnStartStop)
	ON_COMMAND(ID_NEXT_KEY,OnNextKey)
	ON_COMMAND(ID_PREV_KEY,OnPrevKey)
	ON_COMMAND(ID_SELECT_ALL,OnSelectAll)
	ON_COMMAND(ID_SET_KEY,OnKeyAll)
	ON_COMMAND(ID_NEXT_FRAME,OnNextFrame)
	ON_COMMAND(ID_PREV_FRAME,OnPrevFrame)

	END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCryEditApp construction
CCryEditApp::CCryEditApp()
{
	m_pFileChangeMonitor = NULL;
	m_mutexApplication = NULL;

	strcpy(m_sPreviewFile,"");

#ifdef _DEBUG
	int tmpDbgFlag;
	tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	// Clear the upper 16 bits and OR in the desired freqency
	tmpDbgFlag = (tmpDbgFlag & 0x0000FFFF) | (32768 << 16);
	//tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
	_CrtSetDbgFlag(tmpDbgFlag);

	// Check heap every 
	//_CrtSetBreakAlloc(119065);
#endif

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
	m_IEditor = 0;
	m_bExiting = false;
	m_bPreviewMode = false;
	m_bConsoleMode = false;
	m_bTestMode = false;
	m_bPrecacheShaderList = false;
  m_bMergeShaders = false;
	m_bMatEditMode = false;
	m_pMatEditDlg = 0;
	m_bSaveAutobackup = false;

	ZeroStruct(m_tagLocations);
	ZeroStruct(m_tagAngles);

	m_fastRotateAngle = 45;
	m_moveSpeedStep = 0.1f;

	m_pConsoleDialog = 0;

	m_bForceProcessIdle = false;
	m_pFileChangeMonitor = NULL;
}

//////////////////////////////////////////////////////////////////////////
CCryEditApp::~CCryEditApp()
{
	if (m_pFileChangeMonitor)
	{
		m_pFileChangeMonitor->StopMonitor();
		SAFE_DELETE( m_pFileChangeMonitor );
	}
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CCryEditApp object
//////////////////////////////////////////////////////////////////////////
CCryEditApp theApp;

class CEditCommandLineInfo : public CCommandLineInfo
{
public:
	int paramNum;
	int exportParamNum;
	bool bTest;
	bool bExport;
	bool bMatEditMode;
	bool bExportTexture;
	bool bExportLM;
	bool bPrecacheShaders;
	bool bPrecacheShaderList;
  bool bMergeShaders;
	bool bExportAI;
	bool bExportVertexScatter;
	CString file;
	CString gameCmdLine;

	CEditCommandLineInfo()
	{
		exportParamNum = -1;
		paramNum = 0;
		bExport = false;
		bMatEditMode = false;
		bPrecacheShaders = false;
		bPrecacheShaderList = false;
    bMergeShaders = false;
		bTest = false;
		bExport = false;
		bExportTexture = false;
		bExportLM = false;
		bExportAI = false;
		bExportVertexScatter = false;
	}
	virtual void ParseParam( LPCTSTR lpszParam, BOOL bFlag, BOOL bLast )
	{
		if (bFlag && stricmp(lpszParam,"export")==0)
		{
			exportParamNum = paramNum;
			bExport = true;
			return;
		}
		else if (bFlag && stricmp(lpszParam,"exportTexture")==0)
		{
			exportParamNum = paramNum;
			bExportTexture = true;
			bExport = true;
			return;
		}
		else if (bFlag && stricmp(lpszParam,"exportAI")==0)
		{
			exportParamNum = paramNum;
			bExportAI = true;
			bExport = true;
			return;
		}
		else if (bFlag && stricmp(lpszParam,"exportLM")==0)
		{
			exportParamNum = paramNum;
			bExportLM = true;
			return;
		}
		else if (bFlag && stricmp(lpszParam,"exportVS")==0)
		{
			exportParamNum = paramNum;
			bExportVertexScatter = true;
			return;
		}
		else if (bFlag && stricmp(lpszParam,"test")==0)
		{
			bTest = true;
			return;
		}
		else if (bFlag && stricmp(lpszParam,"PrecacheShaders")==0)
		{
			bPrecacheShaders = true;
			return;
		}
		else if (bFlag && stricmp(lpszParam,"PrecacheShaderList")==0)
		{
			bPrecacheShaderList = true;
			return;
		}
    else if (bFlag && stricmp(lpszParam,"MergeShaders")==0)
    {
      bMergeShaders = true;
      return;
    }
		else if (bFlag && stricmp(lpszParam,"VTUNE")==0)
		{
			gameCmdLine += " -VTUNE";
			return;
		}
		else if(bFlag && stricmp(lpszParam,"MatEdit")==0)
		{
			bMatEditMode = true;
		}
		if (!bFlag)
		{
			// otherwise it thinks that command parameters included between [] is a filename and tries to load it.
			if (lpszParam[0]=='[')
				return;

			file = lpszParam;			
		}
		paramNum++;
		CCommandLineInfo::ParseParam( lpszParam,bFlag,bLast );
	}
};


#ifdef USING_LICENSE_PROTECTION

bool g_blockMessagePump = false;


//////////////////////////////////////////////////////////////////////////
TAGES_EXPORT void ShowProtectionErrorMessage(HWND hWnd, EPMErrorCode pmec, SClientData* data=NULL)
{
	// cannot use switch statements with Solidshield
	if (pmec==PMEC_CONNECT_FAILED)
	{
		MessageBox( hWnd, "Could not connect to the License Server. Please establish an internet connection.\nIf you still cannot connect, check your firewall or contact Crytek to resolve this issue.", 
			"Connection Error 0x100", MB_ICONINFORMATION | MB_OK | MB_APPLMODAL | MB_TOPMOST);
	}
	else if (pmec==PMEC_PROXY_CONNECT_FAILED)
	{
		MessageBox( hWnd, "Could not authenticate at proxy. Please verify your proxy settings.\nIf you still cannot connect, check your firewall or contact Crytek to resolve this issue.", 
			"Connection Error 0x110", MB_ICONINFORMATION | MB_OK | MB_APPLMODAL | MB_TOPMOST);
	}	
	else if (pmec==PMEC_CANT_WRITE_TO_SOCKET)
	{
		MessageBox( hWnd, "Could not write to the License Server. Please check your firewall or contact Crytek to resolve this issue.", 
			"Connection Error 0x200", MB_ICONINFORMATION | MB_OK | MB_APPLMODAL | MB_TOPMOST);
	}
	else if (pmec==PMEC_CANT_READ_FROM_SOCKET)
	{
		MessageBox( hWnd, "Could not read from the License Server. Please check your firewall or contact Crytek to resolve this issue.", 
			"Connection Error 0x300", MB_ICONINFORMATION | MB_OK | MB_APPLMODAL | MB_TOPMOST);
	}
	else if (pmec==PMEC_KEY_EXPIRED)
	{
		MessageBox( hWnd, "Your License could not be validated.\nPlease contact Crytek to resolve this issue.", 
			"Validation Error 0x100", MB_ICONINFORMATION | MB_OK | MB_APPLMODAL | MB_TOPMOST);
	}
	else if (pmec==PMEC_KEY_INVALID)
	{
		MessageBox( hWnd, "Your License could not be validated.\nPlease contact Crytek to resolve this issue.", 
			"Validation Error 0x200", MB_ICONINFORMATION | MB_OK | MB_APPLMODAL | MB_TOPMOST);
	}
	else if (pmec==PMEC_INVALID_TIMEFRAME)
	{
		MessageBox( hWnd, "Your License could not be validated.\nPlease contact Crytek to resolve this issue.", 
			"Validation Error 0x300", MB_ICONINFORMATION | MB_OK | MB_APPLMODAL | MB_TOPMOST);
	}
	else if (pmec==PMEC_OLD_APP_VERSION)
	{
		CVersionUpdateDialog vud(data->sVersion);
		vud.DoModal();
	}
}


//////////////////////////////////////////////////////////////////////////
TAGES_EXPORT void OnProtectionCallback()
{
	SClientData data;
	IProtectionManager* pPM = gEnv->pSystem->GetProtectionManager();
	EPMErrorCode pmec;

	if ((pmec=pPM->RetrieveClientData(data))!=PMEC_NO_ERROR && pmec!=PMEC_OLD_APP_VERSION)
	{
		g_blockMessagePump = true;

		ShowProtectionErrorMessage(AfxGetMainWnd()->GetSafeHwnd(), pmec);
	}

	g_blockMessagePump = false;
}
#endif // USING_LICENSE_PROTECTION


TAGES_EXPORT LRESULT CALLBACK EditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
#ifdef USING_LICENSE_PROTECTION
	// block as long as the protection callback owns a dialog
	while (g_blockMessagePump)
		Sleep(100);
#endif // USING_LICENSE_PROTECTION

	switch(msg)
	{
	case WM_MOVE:
		if(gEnv && gEnv->pSystem)
		{
			CViewport *pViewport = GetIEditor()->GetViewManager()->GetGameViewport();
			if(pViewport)
			{
				RECT rcWindow;
				GetWindowRect(pViewport->m_hWnd,&rcWindow);
				// The main window moves, but not the RenderViewport one, as it's relative to the main window
				// But we are interested by absolute coordinates of the RenderViewport, so use event from main window
				// And coordinates from RenderViewport window ...
				gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_MOVE,rcWindow.left,rcWindow.top);
			}
		}
		return 0;

	default:
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}


/////////////////////////////////////////////////////////////////////////////
// CTheApp::FirstInstance
//		FirstInstance checks for an existing instance of the application. 
//		If one is found, it is activated.
//
//  	This function uses a technique similar to that described in KB 
//  	article Q141752	to locate the previous instance of the application. .
BOOL CCryEditApp::FirstInstance()
{                                       
	CWnd* pwndFirst = CWnd::FindWindow( _T("CryEditorClass"),NULL);
	if (pwndFirst)
	{
		// another instance is already running - activate it
		CWnd* pwndPopup = pwndFirst->GetLastActivePopup();								   
		pwndFirst->SetForegroundWindow();
		if (pwndFirst->IsIconic())
			pwndFirst->ShowWindow(SW_SHOWNORMAL);
		if (pwndFirst != pwndPopup)
			pwndPopup->SetForegroundWindow(); 

		if (m_bPreviewMode)
		{
			// IF in preview mode send this window copy data message to load new preview file.
			COPYDATASTRUCT cd;
			ZeroStruct(cd);
			cd.dwData = 100;
			cd.cbData = strlen(m_sPreviewFile);
			cd.lpData = m_sPreviewFile;
			pwndFirst->SendMessage( WM_COPYDATA,0,(LPARAM)&cd );
		}
		return FALSE;			
	}
	else
	{   
		// this is the first instance
		// Register your unique class name that you wish to use
		WNDCLASS wndcls;
		ZeroStruct( wndcls );
		wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wndcls.lpfnWndProc = EditorWndProc;
		wndcls.hInstance = AfxGetInstanceHandle();
		wndcls.hIcon = LoadIcon(IDR_MAINFRAME); // or load a different icon.
		wndcls.hCursor = LoadCursor(IDC_ARROW);
		wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wndcls.lpszMenuName = NULL;

		// Specify your own class name for using FindWindow later
		wndcls.lpszClassName = _T("CryEditorClass");

		// Register the new class and exit if it fails
		if(!AfxRegisterClass(&wndcls))
		{
			TRACE("Class Registration Failed\n");
			return FALSE;
		}
//		bClassRegistered = TRUE;

		return TRUE;
	}
}	

//////////////////////////////////////////////////////////////////////////
CCryEditDoc* CCryEditApp::GetDocument()
{
	return GetIEditor()->GetDocument();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::InitDirectory()
{
	//////////////////////////////////////////////////////////////////////////
	// Initializes Root folder of the game.
	//////////////////////////////////////////////////////////////////////////
	WCHAR szExeFileName[_MAX_PATH];

	GetModuleFileNameW( GetModuleHandle(NULL), szExeFileName, sizeof(szExeFileName));
	PathRemoveFileSpecW(szExeFileName);

	// Remove Bin32/Bin64 folder/
	WCHAR *lpPath = StrStrIW(szExeFileName,L"\\Bin32");
	if (lpPath)
		*lpPath = 0;
	lpPath = StrStrIW(szExeFileName,L"\\Bin64");
	if (lpPath)
		*lpPath = 0;

	SetCurrentDirectoryW( szExeFileName );
}

//////////////////////////////////////////////////////////////////////////
// Needed to work with custom memory manager.
//////////////////////////////////////////////////////////////////////////
class CCrySingleDocTemplate : public CSingleDocTemplate
{
public:
	CCrySingleDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass,CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
	:	CSingleDocTemplate(nIDResource,pDocClass,pFrameClass,pViewClass)
	{
	}
	~CCrySingleDocTemplate() {};
};


/////////////////////////////////////////////////////////////////////////////
// CCryEditApp initialization
BOOL CCryEditApp::InitInstance()
{
	////////////////////////////////////////////////////////////////////////
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.
	////////////////////////////////////////////////////////////////////////
	CEditCommandLineInfo cmdInfo;
	
	CProcessInfo::LoadPSApi();

	InitCommonControls();    // initialize common control library
  CWinApp::InitInstance(); // call parent class method

	// Init COM services
	CoInitialize(NULL);
	// Initialize RichEditCtrl.
#if _MFC_VER >= 0x0700 // MFC 7.0
	AfxInitRichEdit2();
#else // MFC 7.0
	AfxInitRichEdit();
#endif // MFC 7.0

#ifdef _AFXDLL
	//Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	//Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	//////////////////////////////////////////////////////////////////////////
	// Initialize GDI+
	//////////////////////////////////////////////////////////////////////////
	Gdiplus::GdiplusStartupInput gdiplusstartupinput;
	Gdiplus::GdiplusStartup (&m_gdiplusToken, &gdiplusstartupinput, NULL);
	//////////////////////////////////////////////////////////////////////////

	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
	SetRegistryKey(_T("Crytek"));

	LoadStdProfileSettings(8);  // Load standard INI file options (including MRU)
	InitDirectory();

	// Check for 32bpp
	if (::GetDeviceCaps(GetDC(NULL), BITSPIXEL) != 32)
		AfxMessageBox("WARNING: Your desktop is not set to 32bpp, this might result in unexpected behavior" \
		"of the editor. Please set your desktop to 32bpp !");


	// Register the application's document templates. Document templates
	// serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CCrySingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CCryEditDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CLayoutViewPane));
	AddDocTemplate(pDocTemplate);

	m_bPreviewMode = false;

	// Parse command line for standard shell commands, DDE, file open
	ParseCommandLine(cmdInfo);

	//! Copy command line params.
	m_bTestMode = cmdInfo.bTest || cmdInfo.bPrecacheShaders || cmdInfo.bMergeShaders || cmdInfo.bPrecacheShaderList;
	if (cmdInfo.bPrecacheShaders || cmdInfo.bMergeShaders || cmdInfo.bPrecacheShaderList)
	{
		m_bPreviewMode = true;
		m_bConsoleMode = true;
	}
	m_bPrecacheShaderList = cmdInfo.bPrecacheShaderList;
  m_bMergeShaders = cmdInfo.bMergeShaders;
	m_bExportMode = cmdInfo.bExport || cmdInfo.bExportLM || cmdInfo.bExportVertexScatter;
	if (m_bExportMode)
	{
		m_exportFile = cmdInfo.file;
		m_bTestMode = true;
	}

	// Do we have a passed filename ?
	if (!cmdInfo.m_strFileName.IsEmpty())
	{
		if (IsPreviewableFileType(cmdInfo.m_strFileName.GetBuffer(0)))
		{
			m_bPreviewMode = true;
			strcpy( m_sPreviewFile,cmdInfo.m_strFileName );
		}
	}
	if (!m_bPreviewMode)
	{
		m_mutexApplication = CreateMutex( NULL, TRUE, "CrytekApplication" );
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			if(MessageBox(GetDesktopWindow(), "There is already a Crytek application running\nDo you want to start another one?", "Too many apps", MB_YESNO)!=IDYES)
				return FALSE;
		}
	}
	
	m_bMatEditMode = cmdInfo.bMatEditMode;

	if (!FirstInstance() && !m_bPrecacheShaderList) // Shader precaching may start multiple editor copies.
		return FALSE;

	// Initialize editor interface.
	m_IEditor = new CEditorImpl;

	m_pFileChangeMonitor = new CFileChangeMonitor;

	((CEditorImpl*)m_IEditor)->SetMatEditMode( m_bMatEditMode );

	IInitializeUIInfo *pInitializeUIInfo = 0;
	CStartupLogoDialog logo;
	//if (!m_bPreviewMode && !m_bMatEditMode)
	if (!m_bConsoleMode)
	{
		// Do not create logo screen for model viewer.
		logo.Create( CStartupLogoDialog::IDD );
		
		Version editorVersion = m_IEditor->GetFileVersion();
		if (editorVersion[3] > 0)
			logo.SetVersion(editorVersion);

		pInitializeUIInfo = &logo;
	}
	else if (m_bConsoleMode)
	{
		m_pConsoleDialog = new CConsoleDialog;
		m_pConsoleDialog->Create(CConsoleDialog::IDD);
		m_pMainWnd = m_pConsoleDialog;
		pInitializeUIInfo = m_pConsoleDialog;
	}


	//////////////////////////////////////////////////////////////////////////
	// Initialize Game System.
	CGameEngine *pGameEngine = new CGameEngine;
	if (!pGameEngine->Init(m_bPreviewMode, m_bTestMode, GetCommandLine(),pInitializeUIInfo ))
	{
		return FALSE;
	}

	// needs to be called after crysystem has been loaded.
	gSettings.LoadDefaultGamePaths();

	/*
	// make sure the right recent file list is used
	int i = 0;
	CString gamePath = Path::ToUnixPath( Path::MakeFullPath( Path::GetGameFolder() ) );
	while ( m_pRecentFileList->GetSize() > 0 && !(*m_pRecentFileList)[0].IsEmpty() &&
		gamePath.CompareNoCase( Path::ToUnixPath((*m_pRecentFileList)[0]).Left(gamePath.GetLength()) ) != 0 )
	{
		m_pRecentFileList->m_strSectionName.Format( "Recent File List %d", ++i );
		m_pRecentFileList->ReadList();
	}

	*/

	((CEditorImpl*)m_IEditor)->SetGameEngine( pGameEngine );

	GetISystem()->GetIPak()->MakeDir( "%USER%/Sandbox" );

	// Register File Change Monitor interface
	GetIEditor()->GetSystem()->SetIFileChangeMonitor( this );

	if (!m_bPreviewMode && !m_bMatEditMode)
	{
		logo.SetInfo( "Starting Game..." );
		const char * sGameDLL = gEnv->pConsole->GetCVar("sys_dll_game")->GetString();
		GetIEditor()->GetGameEngine()->InitGame(sGameDLL);
	}


#ifdef USING_LICENSE_PROTECTION
	SClientData data;
	IProtectionManager* pPM = gEnv->pSystem->GetProtectionManager();
	EPMErrorCode pmec;

	Version v = m_IEditor->GetFileVersion();
	char pVersionText[256];
	_snprintf(pVersionText, 256, "%d.%d.%d", v[3], v[2], v[1]);
	data.sVersion = (char*)&pVersionText;

	// a user won't pass this block until either dongle, or license server check validates
JCheckInstanceValidity:
	if ((pmec=pPM->RetrieveClientData(data))!=PMEC_NO_ERROR)
	{
		switch(pmec)
		{
			case PMEC_KEY_NOT_FOUND:
				{
					// ask for a license key
					CLicenseKeyDialog lkd;
					INT_PTR iResult = lkd.DoModal();
					if (iResult==IDABORT || iResult==IDCANCEL)
						return FALSE;
					goto JCheckInstanceValidity;
				}

			case PMEC_KEY_INVALID:
				{
					// ask for a new license key after one has not been validated
					CLicenseKeyDialog lkd(false);
					INT_PTR iResult = lkd.DoModal();
					if (iResult==IDABORT || iResult==IDCANCEL)
						return FALSE;
					goto JCheckInstanceValidity;
				}
		}

		if (pmec!=PMEC_NO_ERROR)
		{
			ShowProtectionErrorMessage(logo.GetSafeHwnd(), pmec, &data);
			if (pmec!=PMEC_OLD_APP_VERSION)
				return FALSE;
		}
	}
#endif // USING_LICENSE_PROTECTION


	if(!GetIEditor()->GetSystem())
		return false;														// system init failed

	if(!GetIEditor()->GetSystem()->GetIGame())
	{
		// should be after init game (should be executed even if there is no game)
		GetISystem()->ExecuteCommandLine();
	}

	logo.SetInfo( "Loading plugins..." );
	// Load the plugins
	bool bReturn = GetIEditor()->GetPluginManager()->LoadAllPlugins(CString("Editor/Plugins"));

	GetIEditor()->AddUIEnums();

	if (!m_bMatEditMode && !m_bConsoleMode)
	{
		logo.SetInfo( "Initializing Views..." );

		// Initialize new document.
		OnFileNew();

		//! Show main frame.
		((CMainFrame*)m_pMainWnd)->ShowWindowEx( SW_SHOW );
		m_pMainWnd->UpdateWindow();
	}

	// Read configuration.
	ReadConfig();


	if(m_bMatEditMode)
	{
		// Disable all accelerators in preview mode.
		//GetIEditor()->EnableAcceleratos( false );
	}
	else
	if (m_bPreviewMode)
	{
		// Disable all accelerators in preview mode.
		GetIEditor()->EnableAcceleratos( false );

		// Load geometry object.
		if (!cmdInfo.m_strFileName.IsEmpty())
			LoadFile( cmdInfo.m_strFileName );
	}
	else
	{
		CDocument *doc = 0;

		if (m_bExportMode && !m_exportFile.IsEmpty())
		{
			GetIEditor()->SetModifiedFlag(FALSE);
			doc = OpenDocumentFile( m_exportFile );
			if (doc)
			{
				GetIEditor()->SetModifiedFlag(FALSE);
				if (cmdInfo.bExportAI)
				{
					GetIEditor()->GetGameEngine()->GenerateAiAllNavigation();
				}
				if (cmdInfo.bExportVertexScatter)
				{
					((CCryEditDoc *)doc)->GenerateVertexScatter();
				}
				ExportLevel( cmdInfo.bExport,cmdInfo.bExportTexture,cmdInfo.bExportLM,true );
				// Terminate process.
				CLogFile::WriteLine("Editor: Terminate Process after export");
			}
			exit(0);
		}
		else
		{
			bool bOpenLastProject = false;

			logo.SetInfo( "Loading Level..." );

			logo.EndDialog(0);
			//((CMainFrame*)m_pMainWnd)->GetActiveDocument()->SetModifiedFlag(FALSE);
			if (_stricmp(PathFindExtension(cmdInfo.m_strFileName.GetBuffer(0)), ".cry") == 0)
			{
				doc = OpenDocumentFile( cmdInfo.m_strFileName );
				if (doc)
					doc->SetModifiedFlag(FALSE);
			}
			else if (bOpenLastProject) 
			{
				if (m_pRecentFileList->GetSize() > 0 && (*m_pRecentFileList)[0].GetLength() > 0)
				{
					doc = OpenDocumentFile( (*m_pRecentFileList)[0] );
				}
			}
		}
	}

	//m_pMainWnd->SetWindowText( "Crytek Editor" );
	//m_pMainWnd->UpdateWindow();

	// Force display settings.
	GetIEditor()->GetDisplaySettings()->PostInitApply();
	gSettings.PostInitApply();

	logo.DestroyWindow();

	if (!m_bMatEditMode && !m_bConsoleMode && !m_bPreviewMode)
	{
		GetIEditor()->UpdateViews();
		if (m_pMainWnd)
			m_pMainWnd->SetFocus();

		// Show tip of the day.
		CTipDlg dlg;
		if (dlg.m_bStartup)
			dlg.DoModal();
	}

//#ifndef WIN64
	MonitorDirectories();
//#endif

	if(m_bMatEditMode)
	{
		m_pMatEditDlg = new CMatEditMainDlg(0);
		m_pMainWnd=m_pMatEditDlg;
		//m_pMatEditDlg->SetFocus();

		GetIEditor()->Notify( eNotify_OnInit );
		m_pMatEditDlg->DoModal();
		return FALSE;
	}

	GetIEditor()->Notify( eNotify_OnInit );

	if (m_bPrecacheShaderList)
	{
		GetIEditor()->GetSystem()->GetIConsole()->ExecuteString( "r_PrecacheShaderList" );
	}
  else
  if (m_bMergeShaders)
  {
    GetIEditor()->GetSystem()->GetIConsole()->ExecuteString( "r_MergeShaders" );
  }

	// Execute special configs.
	gEnv->pConsole->ExecuteString( "exec autoexec.cfg" );
	gEnv->pConsole->ExecuteString( "exec editor.cfg" );


#ifdef USING_LICENSE_PROTECTION
	// start a thread for constantly checking the license validity
	SetTimer(GetMainWnd()->m_hWnd, 0, 3000, NULL);
	pPM->StartLicenseVerificationThread(OnProtectionCallback);
#endif // USING_LICENSE_PROTECTION

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
bool CCryEditApp::RegisterListener(IFileChangeListener *pListener, const char* sMonitorItem )
{
	bool bRet = false;

	if ( m_pFileChangeMonitor )
	{
		char sRealPath[2048];
		char sRelativePath[2048];

		gEnv->pCryPak->AdjustFileName(sMonitorItem, sRealPath, 0);
		gEnv->pCryPak->AdjustFileName(sMonitorItem, sRelativePath, ICryPak::FLAGS_NO_FULL_PATH);

		if ( true == m_pFileChangeMonitor->MonitorItem( sRealPath ) )
		{
			m_vecFileChangeCallbacks.push_back( SFileChangeCallback( pListener, Path::ToUnixPath(Path::GetGameFolder() + "\\" + CString( sRelativePath )) ) );
			bRet = true;
		}		 
		else
		{
			CryLogAlways( "File Monitor: [%s] not found outside of PAK files. Monitoring disabled for this item", sMonitorItem );
		}
	}
	
	return bRet;
}

bool CCryEditApp::UnregisterListener(IFileChangeListener *pListener)
{
	bool bRet = false;

	// Note that we remove the listener, but we don't currently remove the monitored item
	// from the file monitor. This is fine, but inefficient

	std::vector<SFileChangeCallback>::iterator iter = m_vecFileChangeCallbacks.begin();
	for ( ; iter!=m_vecFileChangeCallbacks.end(); ++iter )
	{
		if ( iter->pListener == pListener)
		{
			m_vecFileChangeCallbacks.erase( iter );
			bRet = true;
			break;
		}
	}

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::MonitorDirectories()
{
	CString masterCD = Path::AddBackslash( CString(GetIEditor()->GetMasterCDFolder()) );

	m_pFileChangeMonitor->MonitorItem( masterCD + Path::GetGameFolder() + "\\Shaders\\HWScripts\\" ); // Monitor scripts directory.
	m_pFileChangeMonitor->MonitorItem( masterCD + Path::GetGameFolder() + "\\Textures\\" ); // Monitor textures directory.
	m_pFileChangeMonitor->MonitorItem( masterCD + Path::GetGameFolder() + "\\Objects\\" ); // Monitor objects directory.
	m_pFileChangeMonitor->MonitorItem( masterCD + Path::GetGameFolder() + "\\Levels\\" ); // Monitor levels directory.
	m_pFileChangeMonitor->MonitorItem( masterCD + Path::GetGameFolder() + "\\Animations\\" ); // Monitor animations directory.
}

// Called when file monitor message is recieve.
void CCryEditApp::OnFileMonitorChange(WPARAM wParam, LPARAM lParam)
{
	if (m_bExiting)
		return;
	std::set<CString> files;
	while (m_pFileChangeMonitor->HaveModifiedFiles())
	{
		CString filename = m_pFileChangeMonitor->GetModifiedFile();
		// Set to ignore duplicates file changes.
		files.insert( filename );
	}
	if (!files.empty())
	{
		for (std::set<CString>::iterator it = files.begin(); it != files.end(); ++it)
		{
			// Process updated file.
			// Make file relative to MasterCD folder.
			CString filename = Path::GetRelativePath(*it);
      filename.Replace( '\\','/' );

			if(!strstr(filename,".log")) // avoid dead loop
			  CryLogAlways( "File changed: %s",(const char*)filename );

			if (!filename.IsEmpty())
			{


				CString ext = filename.Right(filename.GetLength() - filename.ReverseFind('.') - 1);

				// Check for File Monitor callback
				std::vector<SFileChangeCallback>::iterator iter;
				for ( iter=m_vecFileChangeCallbacks.begin(); iter!=m_vecFileChangeCallbacks.end(); ++iter )
				{
					SFileChangeCallback& sCallback = *iter;

					// We compare against length of callback string, so we get directory matches as well as full filenames
					if ( 0 == _strnicmp( filename, sCallback.sItem, sCallback.sItem.GetLength() ) && sCallback.pListener )
					{
						sCallback.pListener->OnFileChange( filename );
					}
				}

				if (stricmp(ext,"caf"))
				{
					GetIEditor()->GetGameEngine()->ReloadResourceFile( filename );
				}
				else
				{
					// If it is playing an animation in a ModelViewport, stop it to prevent a potential crash.
					CModelViewportCE* pCharEdit = NULL;
					for (int i=0; i<GetIEditor()->GetViewManager()->GetViewCount(); ++i)
					{
						if (_strnicmp(GetIEditor()->GetViewManager()->GetView(i)->GetName(), "Model View", strlen("Model View")) == 0)
						{
							pCharEdit = static_cast<CModelViewportCE*>(GetIEditor()->GetViewManager()->GetView(i));
							break;
						}
					}
					if(pCharEdit)
					{
						if(pCharEdit->GetCharacterBase()->GetISkeletonAnim()->GetNumAnimsInFIFO(0) == 0)
							pCharEdit = NULL;
						else
							pCharEdit->GetCharacterBase()->GetISkeletonAnim()->StopAnimationsAllLayers();
					}

					int nModels;

					GetISystem()->GetIAnimationSystem()->GetLoadedModels(0, nModels);

					if (nModels == 0)
						continue;

					ICharacterModel ** pModels = new ICharacterModel *[nModels];
					GetISystem()->GetIAnimationSystem()->GetLoadedModels(pModels, nModels);

					for (uint32 models =0; models < nModels; ++models)
					{
						uint32 numInstances = pModels[models]->GetNumInstances();

						if (numInstances  == 0)
							continue;

						CString subpath = filename;
						string sGameFolder = PathUtil::GetGameFolder() + "/";
						int iGameFolderLen = sGameFolder.length();
						int fs = subpath.Find(sGameFolder, 0);

						string path = subpath.Right(subpath.GetLength() - fs - iGameFolderLen);

						path.replace('\\','/' );
						path.MakeLower();
						int globalID = pModels[models]->GetInstance(0)->GetIAnimationSet()->ReloadCAF(path.c_str(), true);
						// not used this animations
						if (globalID == -1)
							continue;

						for (uint32 inst = 1; inst < numInstances; ++inst)
						{
							pModels[models]->GetInstance(inst)->GetIAnimationSet()->RenewCAF(path.c_str(), true);
						}
					}

					delete []pModels;

					// Restart the animation.
					if(pCharEdit)
						pCharEdit->OnAnimPlay();
				}
				// Set this flag to make sure that the viewport will update at least once,
				// so that the changes will be shown, even if the app does not have focus.
				m_bForceProcessIdle = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::LoadFile( const CString &fileName )
{
	//CEditCommandLineInfo cmdLine;
	//ProcessCommandLine(cmdinfo);

	//bool bBuilding = false;
	//CString file = cmdLine.SpanExcluding()
	if (GetIEditor()->GetViewManager()->GetViewCount() == 0)
		return;
	CViewport *vp = GetIEditor()->GetViewManager()->GetView(0);
	if (vp->IsKindOf( RUNTIME_CLASS(CModelViewport) ))
	{
		((CModelViewport*)vp)->LoadObject( fileName,1 );
	}

	LoadTagLocations();
	if (m_pMainWnd)
		m_pMainWnd->SetWindowText( "CryEngine Sandbox" );

	GetIEditor()->SetModifiedFlag(FALSE);
}

//////////////////////////////////////////////////////////////////////////
inline void ExtractMenuName(CString& str)
{
	// eliminate &
	int pos = str.Find('&');
	if (pos >= 0)
	{
		str = str.Left(pos) + str.Right(str.GetLength() - pos - 1);
	}
	// cut the string
	for (int i = 0; i < str.GetLength(); i++)
		if (str[i] == 9)
			str = str.Left(i);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::EnableAccelerator( bool bEnable )
{
	/*
	if (bEnable)
	{
		//LoadAccelTable( MAKEINTRESOURCE(IDR_MAINFRAME) );
		m_AccelManager.UpdateWndTable();
		CLogFile::WriteLine( "Enable Accelerators" );
	}
	else
	{
		CMainFrame *mainFrame = (CMainFrame*)m_pMainWnd;
		if (mainFrame->m_hAccelTable)
			DestroyAcceleratorTable( mainFrame->m_hAccelTable );
		mainFrame->m_hAccelTable = NULL;
		mainFrame->LoadAccelTable( MAKEINTRESOURCE(IDR_GAMEACCELERATOR) );
		CLogFile::WriteLine( "Disable Accelerators" );
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::SaveAutoRemind()
{
	// Ingore in game mode.
	if (GetIEditor()->IsInGameMode())
		return;
	CString str;
	str.Format( "Auto Reminder: You did not save level for %d minutes\r\nDo you want to save it now?",gSettings.autoRemindTime );
	if (AfxMessageBox( str,MB_YESNO ) == IDYES)
	{
		// Save now.
		GetIEditor()->SaveDocument();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::SaveAutoBackup()
{
	// Ingore in game mode.
	if (GetIEditor()->IsInGameMode())
		return;

	if (!gSettings.autoBackupEnabled)
		return;
	//m_bSaveAutobackup = true;
	CWaitCursor wait;

	if (GetIEditor()->GetGameEngine()->GetLevelPath().IsEmpty())
		return;

	CString filename = Path::Make( GetIEditor()->GetGameEngine()->GetLevelPath(),Path::GetFileName(gSettings.autoBackupFilename),"bak" );

	// Open the file for writing, create it if neededs
	CCryMemFile memFile;
	{
		// Create the archive object
		CArchive ar(&memFile, CArchive::store);
		// Save the state
		GetIEditor()->GetDocument()->Serialize(ar);
	}
	// ovveride old autobackup file.
	if (!CFileUtil::OverwriteFile(filename))
		return;
	// Save memfile to real file.
	CFile file( filename, CFile::modeCreate | CFile::modeWrite );
	int nFileSize = memFile.GetLength();
	file.Write( memFile.Detach(),nFileSize );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::ReadConfig()
{
	// setup perforce by parameter

	const ICmdLineArg *pSCMArg = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Pre,"SCM");
	if (pSCMArg)
	{
		CString pluginName = pSCMArg->GetValue();

		gSettings.ssafeParams.SourceControlOn = true;

		gSettings.ssafeParams.SourceControlPluginName = "";
		ISourceControl* empty = GetIEditor()->GetSourceControl();

		gSettings.ssafeParams.SourceControlPluginName = pluginName;
		ISourceControl* meant = GetIEditor()->GetSourceControl();

		if (meant==empty)
		{
			gSettings.ssafeParams.SourceControlOn = false;
			gSettings.ssafeParams.SourceControlPluginName = "";
		}
	}
	else
	{
		gSettings.ssafeParams.SourceControlOn = false;
		gSettings.ssafeParams.SourceControlPluginName = "";
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::WriteConfig()
{
	if (m_pMainWnd && ::IsWindow(m_pMainWnd->m_hWnd) && m_pMainWnd->IsKindOf(RUNTIME_CLASS(CMainFrame)))
		((CMainFrame*)m_pMainWnd)->SaveConfig();

	IEditor* pEditor = GetIEditor();
	if (pEditor && pEditor->GetDisplaySettings())
	{
		pEditor->GetDisplaySettings()->SaveRegistry();
	}
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg(IEditor* pEditor);

	void SetVersion( const Version &v );

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	
// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	IEditor* m_pEditor;
};

CAboutDlg::CAboutDlg(IEditor* pEditor)
: CDialog(CAboutDlg::IDD),
	m_pEditor(pEditor)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
} 

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BOOL CAboutDlg::OnInitDialog()
{
	////////////////////////////////////////////////////////////////////////
	// Show a list of plugins in the dialog
	////////////////////////////////////////////////////////////////////////

	PluginIt it;

	CDialog::OnInitDialog();
	SetVersion(m_pEditor->GetFileVersion());

	return TRUE;
}

void CAboutDlg::SetVersion( const Version &v )
{
	if (!m_hWnd)
		return;

	// in case the major version is 0, we assume it's a local build
	if (v[3] <= 0)
		return;

	char pVersionText[256];
	char pRevisionText[256];
	char pBuildText[256];

	_snprintf(pVersionText, 256, "Version %d.%d.%d", v[3], v[2], v[1]);
	_snprintf(pBuildText, 256, "Build %d", v[0]);

	SetDlgItemText(IDC_ABOUTVERSION, CString(pVersionText));
	//SetDlgItemText(IDC_ABOUTREVISION, CString(pRevisionText));
	SetDlgItemText(IDC_ABOUTBUILD, CString(pBuildText));
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)	
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CCryEditApp::OnAppAbout()
{
	CAboutDlg aboutDlg(m_IEditor);
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CCryEditApp message handlers


int CCryEditApp::ExitInstance() 
{
	SAFE_DELETE(m_pConsoleDialog);

	if (GetIEditor())
		GetIEditor()->Notify( eNotify_OnQuit );

	m_bExiting = true;
	if (m_pFileChangeMonitor)
	{
		m_pFileChangeMonitor->StopMonitor();
		SAFE_DELETE( m_pFileChangeMonitor );
	}

	HEAP_CHECK
	////////////////////////////////////////////////////////////////////////
	// Executed directly before termination of the editor, just write a
	// quick note to the log so that we can later see that the edtor
	// terminated flawless. Also delete temporary files
	////////////////////////////////////////////////////////////////////////
	WriteConfig();

	//////////////////////////////////////////////////////////////////////////
	// Quick end for editor.
	if (gEnv && gEnv->pSystem)
		gEnv->pSystem->Quit();
	//////////////////////////////////////////////////////////////////////////

	if (m_IEditor)
	{
		m_IEditor->DeleteThis();
		m_IEditor = 0;
	}
	CoUninitialize();

	// save accelerator manager configuration.
	//m_AccelManager.SaveOnExit();

	CProcessInfo::UnloadPSApi();

	Gdiplus::GdiplusShutdown(m_gdiplusToken);

	if (m_mutexApplication)
		CloseHandle(m_mutexApplication);

	return CWinApp::ExitInstance();
}
 
BOOL CCryEditApp::OnIdle(LONG lCount) 
{
	//HEAP_CHECK
	if (!m_pMainWnd)
		return 0;

	////////////////////////////////////////////////////////////////////////
	// Call the update function of the engine
	////////////////////////////////////////////////////////////////////////
	if (m_bTestMode)
	{
		// Terminate process.
		CLogFile::WriteLine("Editor: Terminate Process");
		exit(0);
	}

	CWnd *pWndForeground = CWnd::GetForegroundWindow();
	CWnd *pForegroundOwner = NULL;

	bool bIsAppWindow = (pWndForeground == m_pMainWnd);
	if (pWndForeground)
	{
		DWORD wndProcId = 0;
		DWORD wndThreadId = GetWindowThreadProcessId( pWndForeground->GetSafeHwnd(),&wndProcId );
		if (GetCurrentProcessId() == wndProcId)
		{
			bIsAppWindow = true;
		}
	}
	bool bActive = false;
	int res = 0;
	if (bIsAppWindow || m_bForceProcessIdle)
	{
		res = 1;
		bActive = true;
	}
	m_bForceProcessIdle = false;

	// focus changed
	if (m_bPrevActive != bActive)
		GetIEditor()->GetSystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS,bActive,0);

	m_bPrevActive = bActive;

	if (bActive)
	{
		if (GetIEditor()->IsInGameMode())
		{
			// Update Game
			GetIEditor()->GetGameEngine()->Update();
		}
		else
		{
			if(!m_bMatEditMode)
			{
				// Start profiling frame.
				GetIEditor()->GetSystem()->GetIProfileSystem()->StartFrame();

			// Update UI.
			CDynamicHelpDialog::OnIdle();

				GetIEditor()->GetGameEngine()->Update();

				if (m_IEditor)
					((CEditorImpl*)m_IEditor)->Update();

				GetIEditor()->GetSystem()->GetIProfileSystem()->EndFrame();
			}
		}
	}

	CWinApp::OnIdle(lCount);

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::ExportLevel( bool bExportToGame,bool bExportTexture,bool bExportLM,bool bAutoExport )
{
	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
	if( pObjectManager )
	{
		CObjectLayerManager* pLayerManager = pObjectManager->GetLayersManager();
		if( pLayerManager )
			pLayerManager->SetAutoExportMode( bAutoExport );
	}

	if (bExportTexture)
	{
		CGameExporter gameExporter( GetIEditor()->GetSystem() );
		gameExporter.SetAutoExportMode( bAutoExport );
		gameExporter.Export(true,false);
	}
	else if (bExportToGame)
	{
		CGameExporter gameExporter( GetIEditor()->GetSystem() );
		gameExporter.SetAutoExportMode( bAutoExport );
		gameExporter.Export(false,false);
		// After export.
//		OnAiGenerateTriangulation();
//		OnAiGenerate3dvolumes();	
	}

	if (bExportLM)
	{
/*
		CLMCompDialog cDialog(m_IEditor->GetSystem());
		cDialog.Create( CLMCompDialog::IDD );
		cDialog.ShowWindow( SW_SHOW );
		cDialog.RecompileAll();
		cDialog.DestroyWindow();
*/
		//Get scene context singleton
		CSceneContext& sceneCtx = CSceneContext::GetInstance();
		sceneCtx.m_bDistributedMap = true;
		SSharedLMEditorData sSharedData;
		CLightmapGen cLightmapGen(&sSharedData);
		cLightmapGen.GenerateAll( GetIEditor(),NULL );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditFetch()
{
	////////////////////////////////////////////////////////////////////////
	// Get the latestf state back
	////////////////////////////////////////////////////////////////////////
	GetIEditor()->GetDocument()->FetchFromFile( HOLD_FETCH_FILE );
}

void CCryEditApp::OnEditHold()
{
	////////////////////////////////////////////////////////////////////////
	// Save the current state
	////////////////////////////////////////////////////////////////////////
	GetIEditor()->GetDocument()->HoldToFile( HOLD_FETCH_FILE );
}

void CCryEditApp::ExportToOBJ()
{
	char szFilters[] = "Object files (*.obj)|*.obj|All files (*.*)|*.*||";
	CAutoDirectoryRestoreFileDialog dlg(FALSE, "obj", "*.obj", OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR, szFilters);

	// Obtain the position of the terrain marker

	if (dlg.DoModal() == IDOK) 
	{
		CExportObjects exp;
		exp.Export(dlg.GetPathName().GetBuffer(1));
	}
}


void CCryEditApp::ExportToGame()
{
	////////////////////////////////////////////////////////////////////////
	// Export the map to the game's level folder
	////////////////////////////////////////////////////////////////////////
	
/*	int nResult;

	// Ask first
	nResult = MessageBox( AfxGetMainWnd()->GetSafeHwnd(),"You are about to perform a game data export that" \
		" includes exporting the surface texture. This can take several minutes, do you want to" \
		" continue ?", "Exporting", MB_ICONINFORMATION | MB_YESNO | MB_APPLMODAL | MB_TOPMOST);

	if (nResult == IDYES)
*/	{
		CErrorsRecorder errRecorder;

		if (!GetIEditor()->GetGameEngine()->IsLevelLoaded())
		{
			// If level not loaded first fast export terrain.
			//GetDocument()->ExportToGame(false);
			CGameExporter gameExporter( GetIEditor()->GetSystem() );
			gameExporter.Export(false,true);
		}else
		{
			bool autoBackupEnabledSaved = gSettings.autoBackupEnabled;
			int autoRemindTimeSaved = gSettings.autoRemindTime;
			gSettings.autoBackupEnabled = false;
			gSettings.autoRemindTime = 0;

			//GetDocument()->ExportToGame(true);
			CGameExporter gameExporter( GetIEditor()->GetSystem() );
			gameExporter.Export(true,true,false);

			//restore gSettings values
			gSettings.autoBackupEnabled = autoBackupEnabledSaved;
			gSettings.autoRemindTime = autoRemindTimeSaved;
		}
	}
}

void CCryEditApp::OnFileExportToGameNoSurfaceTexture() 
{
	////////////////////////////////////////////////////////////////////////
	// Export the map to the game's level folder while skipping the
	// exporting of the surface texture
	////////////////////////////////////////////////////////////////////////
	CErrorsRecorder errRecorder;
	CWaitCursor wait;
	
	//GetDocument()->ExportToGame(false);
	CGameExporter gameExporter( GetIEditor()->GetSystem() );
	gameExporter.Export(false,false);
}

void CCryEditApp::ToolTerrain()
{
	((CMainFrame*)m_pMainWnd)->OpenPage( "Terrain Editor" );
}

void CCryEditApp::ToolSky()
{
	////////////////////////////////////////////////////////////////////////
	// Show the sky dialog
	////////////////////////////////////////////////////////////////////////

	CSkyDialog cDialog;

	cDialog.DoModal();
	if (GetIEditor()->GetDocument()->IsModified())
	{
		GetIEditor()->GetGameEngine()->ReloadEnvironment();
	}
}

void CCryEditApp::ToolLighting()
{
	////////////////////////////////////////////////////////////////////////
	// Show the terrain lighting dialog
	////////////////////////////////////////////////////////////////////////

	// Disable all tools. (Possible layer painter tool).
	GetIEditor()->SetEditTool(0);

	CTerrainLighting cDialog;

	if (cDialog.DoModal() == IDOK)
	{
		if (GetIEditor()->GetDocument()->IsModified())
		{
			GetIEditor()->UpdateViews();
		}
	}
}

void CCryEditApp::TerrainTextureExport()
{
	GetIEditor()->SetEditTool(0);

	CTerrainTextureExport cDialog;
	cDialog.DoModal();
}


void CCryEditApp::ToolTexture()
{
	GetIEditor()->SetEditTool(0);		// close e.g. "layer painting tool" , to force reinit of the tool

	////////////////////////////////////////////////////////////////////////
	// Show the terrain texture dialog
	////////////////////////////////////////////////////////////////////////

	GetIEditor()->OpenView( "Terrain Texture Layers" );
	
	/*
	CTerrainTextureDialog cDialog;
	cDialog.DoModal();
	if (GetIEditor()->GetDocument()->IsModified())
	{
		GetIEditor()->UpdateViews( eUpdateHeightmap );
	}
	*/
}

void CCryEditApp::OnGeneratorsStaticobjects() 
{
	////////////////////////////////////////////////////////////////////////
	// Show the static objects dialog
	////////////////////////////////////////////////////////////////////////
/*
	CStaticObjects cDialog;

	cDialog.DoModal();

	BeginWaitCursor();
	GetIEditor()->UpdateViews( eUpdateStatObj );
	GetIEditor()->GetDocument()->GetStatObjMap()->PlaceObjectsOnTerrain();
	EndWaitCursor();
	*/
}

void CCryEditApp::OnFileCreateopenlevel() 
{
	////////////////////////////////////////////////////////////////////////
	// Create a new level or open an existing one
	////////////////////////////////////////////////////////////////////////

	CStartupDialog cDialog;

	cDialog.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditSelectAll() 
{
	////////////////////////////////////////////////////////////////////////
	// Select all map objects
	////////////////////////////////////////////////////////////////////////
	BBox box( Vec3(-FLT_MAX,-FLT_MAX,-FLT_MAX ),Vec3(FLT_MAX,FLT_MAX,FLT_MAX) );
	GetIEditor()->GetObjectManager()->SelectObjects( box );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditSelectNone() 
{
	CUndo undo( "Unselect All" );
	////////////////////////////////////////////////////////////////////////
	// Remove the selection from all map objects
	////////////////////////////////////////////////////////////////////////
	GetIEditor()->ClearSelection();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditInvertselection()
{
	GetIEditor()->GetObjectManager()->InvertSelection();
}


void CCryEditApp::OnEditDelete() 
{
	// If Edit tool active cannot delete object.
	if (GetIEditor()->GetEditTool())
	{
		if (GetIEditor()->GetEditTool()->OnKeyDown( GetIEditor()->GetViewManager()->GetView(0),VK_DELETE,0,0 ))
			return;
	}

	if (GetIEditor()->GetObjectManager()->GetSelection()->IsEmpty())
	{
		AfxMessageBox("You have to select objects before you can delete them !",MB_OK|MB_APPLMODAL );
		return;
	}
	CString strAsk = "Delete selected objects?";
	int iResult = MessageBox( AfxGetMainWnd()->GetSafeHwnd(),strAsk.GetBuffer(0), "Delete", MB_ICONQUESTION|MB_YESNO|MB_APPLMODAL );
	if (iResult == IDYES)
	{
		GetIEditor()->BeginUndo();
		GetIEditor()->GetObjectManager()->DeleteSelection();
		GetIEditor()->AcceptUndo( "Delete Selection" );
		GetIEditor()->SetModifiedFlag();
	}
}

void CCryEditApp::OnEditClone() 
{
	if (GetIEditor()->GetObjectManager()->GetSelection()->IsEmpty())
	{
		AfxMessageBox("You have to select objects before you can clone them !");
		return;
	}

	CEditTool *tool = GetIEditor()->GetEditTool();
	if (tool && tool->IsKindOf( RUNTIME_CLASS(CObjectCloneTool)))
	{
		((CObjectCloneTool*)tool)->Accept();
	}

	GetIEditor()->SetEditTool( new CObjectCloneTool );
	GetIEditor()->SetModifiedFlag();
}


void CCryEditApp::OnEditEscape() 
{
	// Abort current operation.
	if (GetIEditor()->GetEditTool())
	{
		// If Edit tool active cannot delete object.
		CViewport* vp = GetIEditor()->GetActiveView();
		if (GetIEditor()->GetEditTool()->OnKeyDown( vp,VK_ESCAPE,0,0 ))
			return;

		// Disable current tool.
		GetIEditor()->SetEditTool(0);
	}
	else
	{
		// Clear selection on escape.
		GetIEditor()->ClearSelection();
	}
}

void CCryEditApp::OnMoveObject()
{
	////////////////////////////////////////////////////////////////////////
	// Move the selected object to the marker position
	////////////////////////////////////////////////////////////////////////
}

void CCryEditApp::OnSelectObject()
{
	////////////////////////////////////////////////////////////////////////
	// Bring up the select object dialog
	////////////////////////////////////////////////////////////////////////

	GetIEditor()->OpenView( "Select Objects" );
}

void CCryEditApp::OnRenameObj()
{
}

void CCryEditApp::OnSetHeight() 
{
}

void CCryEditApp::OnScriptCompileScript() 
{
	////////////////////////////////////////////////////////////////////////
	// Use the Lua compiler to compile a script
	////////////////////////////////////////////////////////////////////////
	CErrorsRecorder errRecorder;

	std::vector<CString> files;
	if (CFileUtil::SelectMultipleFiles( EFILE_TYPE_ANY,files,"Lua Files (*.lua)|*.lua||","Scripts" ))
	{
		//////////////////////////////////////////////////////////////////////////
		// Lock resources.
		// Speed ups loading a lot.
		ISystem *pSystem = GetIEditor()->GetSystem();
		pSystem->GetI3DEngine()->LockCGFResources();
		pSystem->GetIAnimationSystem()->LockResources();
		pSystem->GetISoundSystem()->LockResources();
		//////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < files.size(); i++)
		{
			if (!CFileUtil::CompileLuaFile( files[i] ))
				return;

			// No errors
			// Reload this lua file.
			GetIEditor()->GetSystem()->GetIScriptSystem()->ReloadScript( files[i],false );
		}
		//////////////////////////////////////////////////////////////////////////
		// Unlock resources.
		// Some uneeded resources that were locked before may get released here.
		pSystem->GetISoundSystem()->UnlockResources();
		pSystem->GetIAnimationSystem()->UnlockResources();
		pSystem->GetI3DEngine()->UnlockCGFResources();
		//////////////////////////////////////////////////////////////////////////
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnScriptEditScript() 
{
	// Let the user choose a LUA script file to edit
	CString file;
	if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,file,"Lua Files (*.lua)|*.lua||","Scripts" ))
	{
		CFileUtil::EditTextFile( file );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeMove() 
{
	// TODO: Add your command handler code here
	GetIEditor()->SetEditMode( eEditModeMove );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeRotate() 
{
	// TODO: Add your command handler code here
	GetIEditor()->SetEditMode( eEditModeRotate );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeScale() 
{
	// TODO: Add your command handler code here
	GetIEditor()->SetEditMode( eEditModeScale );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditToolLink()
{
	// TODO: Add your command handler code here
	GetIEditor()->SetEditTool(new CLinkTool());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditToolLink(CCmdUI* pCmdUI) 
{
	CEditTool *pEditTool=GetIEditor()->GetEditTool();
	if (pEditTool && (pEditTool->GetRuntimeClass()==RUNTIME_CLASS(CLinkTool)))
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditToolUnlink() 
{
	CUndo undo( "Unlink Object(s)" );
	CSelectionGroup *pSelection=GetIEditor()->GetObjectManager()->GetSelection();
	for (int i=0;i<pSelection->GetCount();i++)
	{
		CBaseObject *pBaseObj=pSelection->GetObject(i);
		pBaseObj->DetachThis();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditToolUnlink(CCmdUI* pCmdUI) 
{
	if (!GetIEditor()->GetSelection()->IsEmpty())
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeSelect() 
{
	// TODO: Add your command handler code here
	GetIEditor()->SetEditMode( eEditModeSelect );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditmodeSelectarea() 
{
	// TODO: Add your command handler code here
	GetIEditor()->SetEditMode( eEditModeSelectArea );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeSelectarea(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetEditMode() == eEditModeSelectArea)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeSelect(CCmdUI* pCmdUI)
{
	if (GetIEditor()->GetEditMode() == eEditModeSelect)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeMove(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetEditMode() == eEditModeMove)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeRotate(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetEditMode() == eEditModeRotate)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditmodeScale(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetEditMode() == eEditModeScale)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSelectionDelete() 
{
	CString selection = ((CMainFrame*)m_pMainWnd)->GetSelectionName();
	if (!selection.IsEmpty())
	{
		CUndo undo( "Del Selection Group" );
		GetIEditor()->BeginUndo();
		GetIEditor()->GetObjectManager()->RemoveSelection( selection );
		GetIEditor()->SetModifiedFlag();
		GetIEditor()->Notify( eNotify_OnInvalidateControls );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnObjectSetArea() 
{
	CSelectionGroup *sel = GetIEditor()->GetObjectManager()->GetSelection();
	if (!sel->IsEmpty())
	{
		CNumberDlg dlg;
		if (dlg.DoModal() != IDOK)
			return;
	
		GetIEditor()->BeginUndo();
		float area = dlg.GetValue();
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject *obj = sel->GetObject(i);
			obj->SetArea( area );
		}
		GetIEditor()->AcceptUndo( "Set Area" );
		GetIEditor()->SetModifiedFlag();
	}
	else
		AfxMessageBox("No objects selected");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnObjectSetHeight() 
{
	CSelectionGroup *sel = GetIEditor()->GetObjectManager()->GetSelection();
	if (!sel->IsEmpty())
	{
		float height = 0;
		if (sel->GetCount() == 1) {
			Vec3 pos = sel->GetObject(0)->GetWorldPos();
			height = pos.z - GetIEditor()->GetTerrainElevation( pos.x,pos.y );
		}
		
		CNumberDlg dlg( 0,height,"Enter Height" );
		dlg.SetRange( -10000,10000 );
		if (dlg.DoModal() != IDOK)
			return;

		CUndo undo( "Set Height" );
		height = dlg.GetValue();
		IPhysicalWorld *pPhysics = GetIEditor()->GetSystem()->GetIPhysicalWorld();
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject *obj = sel->GetObject(i);
			Matrix34 wtm = obj->GetWorldTM();
			Vec3 pos = wtm.GetTranslation();
			float z = GetIEditor()->GetTerrainElevation( pos.x,pos.y );
			if (z != pos.z)
			{
				float zdown=FLT_MAX;
				float zup=FLT_MAX;
				ray_hit hit;
				if (pPhysics->RayWorldIntersection( pos,Vec3(0,0,-4000),ent_all,rwi_stop_at_pierceable|rwi_ignore_noncolliding,&hit,1 ) > 0)
				{
					zdown = hit.pt.z;
				}
				if (pPhysics->RayWorldIntersection( pos,Vec3(0,0,4000),ent_all,rwi_stop_at_pierceable|rwi_ignore_noncolliding,&hit,1 ) > 0)
				{
					zup = hit.pt.z;
				}
				if (zdown != FLT_MAX && zup != FLT_MAX)
				{
					if (fabs(zup-z) < fabs(zdown-z))
					{
						z = zup;
					}
					else
					{
						z = zdown;
					}
				}
				else if (zup != FLT_MAX) {
					z = zup;
				}
				else if (zdown != FLT_MAX) {
					z = zdown;
				}
			}
			pos.z = z + height;
			wtm.SetTranslation(pos);
			obj->SetWorldTM( wtm );
		}
		GetIEditor()->SetModifiedFlag();
	}
	else
		AfxMessageBox("No objects selected");
}

void CCryEditApp::OnObjectmodifyFreeze() 
{
	// Freeze selection.
	OnEditFreeze();
}

void CCryEditApp::OnObjectmodifyUnfreeze() 
{
	// Unfreeze all.
	OnEditUnfreezeall();
}

void CCryEditApp::OnViewSwitchToGame() 
{
	if (IsInPreviewMode())
		return;
	// TODO: Add your command handler code here
	bool inGame = !GetIEditor()->IsInGameMode();
	GetIEditor()->SetInGameMode( inGame );
}

void CCryEditApp::OnSelectAxisX() 
{
	if (GetIEditor()->GetAxisConstrains() != AXIS_X)
		GetIEditor()->SetAxisConstrains( AXIS_X );
	else
		GetIEditor()->SetAxisConstrains( AXIS_XY );
}

void CCryEditApp::OnSelectAxisY() 
{
	GetIEditor()->SetAxisConstrains( AXIS_Y );
}

void CCryEditApp::OnSelectAxisZ() 
{
	GetIEditor()->SetAxisConstrains( AXIS_Z );
}

void CCryEditApp::OnSelectAxisXy() 
{
	GetIEditor()->SetAxisConstrains( AXIS_XY );
}

void CCryEditApp::OnUpdateSelectAxisX(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetAxisConstrains() == AXIS_X)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

void CCryEditApp::OnUpdateSelectAxisXy(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetAxisConstrains() == AXIS_XY)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

void CCryEditApp::OnUpdateSelectAxisY(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetAxisConstrains() == AXIS_Y)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

void CCryEditApp::OnUpdateSelectAxisZ(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetAxisConstrains() == AXIS_Z)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSelectAxisTerrain() 
{
	GetIEditor()->SetAxisConstrains( AXIS_TERRAIN );
	GetIEditor()->SetTerrainAxisIgnoreObjects( true );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSelectAxisSnapToAll() 
{
	GetIEditor()->SetAxisConstrains( AXIS_TERRAIN );
	GetIEditor()->SetTerrainAxisIgnoreObjects( false );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSelectAxisTerrain(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetAxisConstrains() == AXIS_TERRAIN && GetIEditor()->IsTerrainAxisIgnoreObjects())
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSelectAxisSnapToAll(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetAxisConstrains() == AXIS_TERRAIN && !GetIEditor()->IsTerrainAxisIgnoreObjects())
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnExportTerrainGeom() 
{
	char szFilters[] = "Object files (*.obj)|*.obj|All files (*.*)|*.*||";
	CAutoDirectoryRestoreFileDialog dlg(FALSE, "obj", "*.obj", OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR, szFilters);

	if (dlg.DoModal() == IDOK)
	{
		BeginWaitCursor();
		BBox box;
		GetIEditor()->GetSelectedRegion(box);

		int unitSize = GetIEditor()->GetHeightmap()->GetUnitSize();
		
		// Swap x/y.
		CRect rc;
		rc.left = box.min.y / unitSize;
		rc.top = box.min.x / unitSize;
		rc.right = box.max.y / unitSize;
		rc.bottom = box.max.x / unitSize;
		GetIEditor()->GetDocument()->OnExportTerrainAsGeometrie( dlg.GetPathName(),rc );
		EndWaitCursor();
	}
}

void CCryEditApp::OnUpdateExportTerrainGeom(CCmdUI* pCmdUI) 
{
	BBox box;
	GetIEditor()->GetSelectedRegion(box);
	if (box.IsEmpty())
		pCmdUI->Enable(FALSE);
	else
		pCmdUI->Enable(TRUE);
}

void CCryEditApp::OnSelectionSave() 
{
	char szFilters[] = "Object Group Files (*.grp)|*.grp||";
	CAutoDirectoryRestoreFileDialog dlg(FALSE, "grp", NULL, OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR, szFilters);
	CFile cFile;

	if (dlg.DoModal() == IDOK) 
	{
		CWaitCursor wait;
		CSelectionGroup *sel = GetIEditor()->GetSelection();
		//CXmlArchive xmlAr( "Objects" );

		
		XmlNodeRef root = CreateXmlNode("Objects");
		CObjectArchive ar( GetIEditor()->GetObjectManager(),root,false );
		// Save all objects to XML.
		for (int i = 0; i < sel->GetCount(); i++)
		{
			ar.SaveObject( sel->GetObject(i) );
		}
		SaveXmlNode( root, dlg.GetPathName() );
		//xmlAr.Save( dlg.GetPathName() );
	}
}

void CCryEditApp::OnSelectionLoad() 
{
	// Load objects from file.
	char szFilters[] = "Object Group Files (*.grp)|*.grp||";
	CAutoDirectoryRestoreFileDialog dlg(TRUE, "grp", "*.grp", OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR, szFilters);
	CFile cFile;

	if (dlg.DoModal() == IDOK) 
	{
		CWaitCursor wait;

		GetIEditor()->ClearSelection();
		
		//CXmlArchive xmlAr;
		//xmlAr.Load( dlg.GetPathName() );

		XmlParser parser;
		XmlNodeRef root = parser.parse( dlg.GetPathName() );

		if (!root)
		{
			AfxMessageBox( "Error loading group file" );
			return;
		}

		CErrorsRecorder errorsRecorder;

		CUndo undo( "Load Selection" );
		// Loading.
		// Load all objects from XML.
		CObjectArchive ar( GetIEditor()->GetObjectManager(),root,true );
		GetIEditor()->GetObjectManager()->LoadObjects( ar,true );

		GetIEditor()->SetModifiedFlag();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGotoSelected() 
{
	CViewport *vp = GetIEditor()->GetActiveView();
	if (vp)
		vp->CenterOnSelection();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSelected(CCmdUI* pCmdUI) 
{
	if (!GetIEditor()->GetSelection()->IsEmpty())
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAlignObject() 
{
	// Align pick callback will release itself.
	CAlignPickCallback *alignCallback = new CAlignPickCallback;
	GetIEditor()->PickObject( alignCallback,0,"Align to Object" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAlignToGrid()
{
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo("Align To Grid");
		Matrix34 tm;
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject *obj = sel->GetObject(i);
			tm = obj->GetWorldTM();

			RefCoordSys coords = GetIEditor()->GetReferenceCoordSys();
			Vec3 snapped = tm.GetTranslation();

			switch (coords)
			{
				case COORDS_VIEW:
					{
						snapped = gSettings.pGrid->Snap( snapped );
						break;
					}
				case COORDS_PARENT:
					{
						if (obj->GetParent())
						{
							Matrix34 parentTM = obj->GetParent()->GetWorldTM();
							Matrix34 invParentTM = parentTM.GetInverted();

							snapped = invParentTM*snapped;
							snapped = gSettings.pGrid->Snap( snapped );
							snapped = parentTM*snapped;;

							snapped.x = floor(snapped.x*16384.0f+0.5f)/16384.0f;
							snapped.y = floor(snapped.y*16384.0f+0.5f)/16384.0f;
							snapped.z = floor(snapped.z*16384.0f+0.5f)/16384.0f;
						}
						else
						{
							snapped = gSettings.pGrid->Snap( snapped );
						}
						break;
					}
				case COORDS_WORLD:
					{
						snapped = gSettings.pGrid->Snap( snapped );
						break;
					}
				case COORDS_USERDEFINED:
					{
						Matrix34 userTM = GetIEditor()->GetViewManager()->GetGrid()->GetMatrix();
						Matrix34 invUserTM = userTM.GetInverted();

						snapped = invUserTM*snapped;
						snapped = gSettings.pGrid->Snap( snapped );
						snapped = userTM*snapped;;

						snapped.x = floor(snapped.x*16384.0f+0.5f)/16384.0f;
						snapped.y = floor(snapped.y*16384.0f+0.5f)/16384.0f;
						snapped.z = floor(snapped.z*16384.0f+0.5f)/16384.0f;
						break;
					}
			}

			tm.SetTranslation( snapped );

			obj->SetWorldTM( tm );
			obj->OnEvent( EVENT_ALIGN_TOGRID );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateAlignObject(CCmdUI* pCmdUI) 
{
	if (!GetIEditor()->GetSelection()->IsEmpty())
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);

	if (CAlignPickCallback::IsActive())
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAlignToVoxel() 
{
	GetIEditor()->SetEditTool(new CVoxelAligningTool());
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateAlignToVoxel(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetSelection()->GetCount()==1)
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);

	CEditTool *pEditTool=GetIEditor()->GetEditTool();
	if (pEditTool && (pEditTool->GetRuntimeClass()==RUNTIME_CLASS(CVoxelAligningTool)))
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateFreezed(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
}

//////////////////////////////////////////////////////////////////////////
// Groups.
//////////////////////////////////////////////////////////////////////////

void CCryEditApp::OnGroupAttach() 
{
	// TODO: Add your command handler code here
	GetIEditor()->GetSelection()->PickAndAttach();
	//GetIEditor()->SetEditTool(new CLinkTool());
}

void CCryEditApp::OnUpdateGroupAttach(CCmdUI* pCmdUI) 
{
	BOOL bEnable = FALSE;
	if (!GetIEditor()->GetSelection()->IsEmpty())
		bEnable = TRUE;
	pCmdUI->Enable( bEnable );
}

void CCryEditApp::OnGroupClose() 
{
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj)
	{
		GetIEditor()->BeginUndo();
		((CGroup*)obj)->Close();
		GetIEditor()->AcceptUndo( "Group Close" );
		GetIEditor()->SetModifiedFlag();
	}
}

void CCryEditApp::OnUpdateGroupClose(CCmdUI* pCmdUI) 
{
	BOOL bEnable = FALSE;
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj && obj->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
	{
		if (((CGroup*)obj)->IsOpen())
			bEnable = TRUE;
	}

	pCmdUI->Enable( bEnable );
}

void CCryEditApp::OnGroupDetach() 
{
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj && obj->GetParent())
	{
		CUndo undo( "Group Detach" );
		obj->DetachThis();
	}
}


void CCryEditApp::OnShowHelpers()
{
	GetIEditor()->GetDisplaySettings()->DisplayHelpers( !GetIEditor()->GetDisplaySettings()->IsDisplayHelpers() );
	GetIEditor()->Notify(eNotify_OnDisplayRenderUpdate);
}

void CCryEditApp::OnStartStop()
{
	GetIEditor()->GetCommandManager()->Execute("FaceEditor.StartStop");
}

void CCryEditApp::OnNextKey()
{
	GetIEditor()->GetCommandManager()->Execute("FaceEditor.NextKey");
}

void CCryEditApp::OnPrevKey()
{
	GetIEditor()->GetCommandManager()->Execute("FaceEditor.PrevKey");
}

void CCryEditApp::OnNextFrame()
{
	GetIEditor()->GetCommandManager()->Execute("FaceEditor.NextFrame");
}

void CCryEditApp::OnPrevFrame()
{
	GetIEditor()->GetCommandManager()->Execute("FaceEditor.PrevFrame");
}

void CCryEditApp::OnSelectAll()
{
	GetIEditor()->GetCommandManager()->Execute("FaceEditor.SelectAll");
}

void CCryEditApp::OnKeyAll()
{
	GetIEditor()->GetCommandManager()->Execute("FaceEditor.KeyAll");
}

void CCryEditApp::OnUpdateGroupDetach(CCmdUI* pCmdUI) 
{
	BOOL bEnable = FALSE;
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj && obj->GetParent())
	{
		bEnable = TRUE;
	}
	pCmdUI->Enable( bEnable );
}

void CCryEditApp::OnGroupMake() 
{
	CStringDlg dlg( "Group Name" );
	dlg.m_strString = GetIEditor()->GetObjectManager()->GenUniqObjectName( "Group" );
	if (dlg.DoModal() == IDOK)
	{
		GetIEditor()->BeginUndo();

		CGroup *group = (CGroup*)GetIEditor()->NewObject( "Group" );
		if (!group)
		{
			GetIEditor()->CancelUndo();
			return;
		}
		GetIEditor()->GetObjectManager()->ChangeObjectName( group,dlg.m_strString );

		CSelectionGroup *selection = GetIEditor()->GetSelection();
		selection->FilterParents();
		
		int i;
		std::vector<CBaseObjectPtr> objects;
		for (i = 0; i < selection->GetFilteredCount(); i++)
		{
			objects.push_back( selection->GetFilteredObject(i) );
		}
		
		// Snap center to grid.
		Vec3 center = gSettings.pGrid->Snap( selection->GetCenter() );
		group->SetPos( center );

		for (i = 0; i < objects.size(); i++)
		{
			GetIEditor()->GetObjectManager()->UnselectObject(objects[i]);
			group->AttachChild( objects[i] );
		}

		if(objects.size())
			group->SetLayer( objects[0]->GetLayer() );

		GetIEditor()->AcceptUndo( "Group Make" );
		GetIEditor()->SetModifiedFlag();
	}
}

void CCryEditApp::OnUpdateGroupMake(CCmdUI* pCmdUI) 
{
	OnUpdateSelected( pCmdUI );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGroupOpen() 
{
	// Ungroup all groups in selection.
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo( "Group Open" );
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject *obj = sel->GetObject(i);
			if (obj && obj->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
			{
				((CGroup*)obj)->Open();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateGroupOpen(CCmdUI* pCmdUI) 
{
	BOOL bEnable = FALSE;
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj)
	{
		if (obj->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
		{
			if (!((CGroup*)obj)->IsOpen())
				bEnable = TRUE;
		}
		pCmdUI->Enable( bEnable );
	}
	else
	{
		OnUpdateSelected( pCmdUI );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGroupUngroup() 
{
	// Ungroup all groups in selection.
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo( "Ungroup" );
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject *obj = sel->GetObject(i);
			if (obj && obj->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
			{
				((CGroup*)obj)->Ungroup();
				GetIEditor()->DeleteObject( obj );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateGroupUngroup(CCmdUI* pCmdUI) 
{
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj)
	{
		if (obj->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
			pCmdUI->Enable( TRUE );
		else
			pCmdUI->Enable( FALSE );
	}
	else
	{
		OnUpdateSelected( pCmdUI );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionNew() 
{
	CStringDlg dlg( "New Mission Name" );
	if (dlg.DoModal() == IDOK)
	{
		GetIEditor()->BeginUndo();
		CMission *mission = new CMission( GetIEditor()->GetDocument() );
		mission->SetName( dlg.m_strString );
		GetDocument()->AddMission( mission );
		GetDocument()->SetCurrentMission( mission );
		GetIEditor()->AcceptUndo( "Mission New" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionDelete() 
{
	// Delete current mission.
	CMission *mission = GetDocument()->GetCurrentMission();
	if (MessageBox( AfxGetMainWnd()->GetSafeHwnd(),CString("Delete Mission ")+mission->GetName()+"?","Confirmation",MB_OKCANCEL ) == IDOK)
	{
		GetIEditor()->BeginUndo();
		GetDocument()->RemoveMission( mission );
		GetIEditor()->AcceptUndo( "Mission Delete" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionDuplicate() 
{
	CMission *mission = GetDocument()->GetCurrentMission();
	CStringDlg dlg( "Duplicate Mission Name" );
	if (dlg.DoModal() == IDOK)
	{
		GetIEditor()->BeginUndo();
		CMission *dupMission = mission->Clone();
		dupMission->SetName( dlg.m_strString );
		GetDocument()->AddMission( dupMission );
		GetDocument()->SetCurrentMission( dupMission );
		GetIEditor()->AcceptUndo( "Mission Duplicate" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionProperties() 
{
	CPropertySheet props("Mission Properties");

	CWeaponProps weaponsPage;
	CMissionProps missionProps;
	props.AddPage(&missionProps);
	// props.AddPage(&weaponsPage);
	props.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionRename() 
{
	CMission *mission = GetDocument()->GetCurrentMission();
	CStringDlg dlg( "Rename Mission" );
	dlg.m_strString = mission->GetName();
	if (dlg.DoModal() == IDOK)
	{
		GetIEditor()->BeginUndo();
		mission->SetName( dlg.m_strString );
		GetIEditor()->AcceptUndo( "Mission Rename" );
	}
	GetIEditor()->Notify( eNotify_OnInvalidateControls );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionSelect()
{
	CMissionSelectDialog dlg;
	if (dlg.DoModal() == IDOK)
	{
		CMission *mission = GetDocument()->FindMission( dlg.GetSelected() );
		if (mission)
		{
			GetDocument()->SetCurrentMission( mission );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionReload()
{
	GetIEditor()->GetDocument()->GetCurrentMission()->GetScript()->Load();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMissionEdit()
{
	GetIEditor()->GetDocument()->GetCurrentMission()->GetScript()->Edit();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnShowTips() 
{
	CTipDlg dlg;
	dlg.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnLockSelection() 
{
	// Invert selection lock.
	GetIEditor()->LockSelection( !GetIEditor()->IsSelectionLocked() );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditLevelData() 
{
	char dir[1024];
	strcpy( dir,GetDocument()->GetPathName() );
	PathRemoveFileSpec( dir );
	CFileUtil::EditTextFile( CString(dir)+"\\LevelData.xml" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileEditLogFile() 
{
	CFileUtil::EditTextFile( CLogFile::GetLogFileName(),0,CFileUtil::FILE_TYPE_SCRIPT, false );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileEditEditorini() 
{
	CFileUtil::EditTextFile( EDITOR_INI_FILE );
}

void CCryEditApp::OnPreferences() 
{
	/*
	//////////////////////////////////////////////////////////////////////////////
	// Accels edit by CPropertyPage
	CAcceleratorManager tmpAccelManager;
	tmpAccelManager = m_AccelManager;
	CAccelMapPage page(&tmpAccelManager);
	CPropertySheet sheet;
	sheet.SetTitle( _T("Preferences") );
	sheet.AddPage(&page);
	if (sheet.DoModal() == IDOK) {
		m_AccelManager = tmpAccelManager;
		m_AccelManager.UpdateWndTable();
	}
	*/
}

void CCryEditApp::OnReloadTextures() 
{
	CWaitCursor wait;
	CLogFile::WriteLine( "Reloading Static objects textures and shaders." );
	GetIEditor()->GetObjectManager()->SendEvent( EVENT_RELOAD_TEXTURES );
	GetIEditor()->GetRenderer()->EF_ReloadTextures();
}

void CCryEditApp::OnReloadScripts() 
{
	CErrorsRecorder errRecorder;
	CWaitCursor wait;

	GetIEditor()->GetIconManager()->Reset();
	// Reload all entities and thier scripts.
	GetIEditor()->GetObjectManager()->SendEvent( EVENT_UNLOAD_ENTITY );
	// reload main.lua script.
	//GetIEditor()->GetSystem()->GetIScriptSystem()->ReloadScript( "Scripts/main.lua" );
	//GetIEditor()->GetSystem()->GetIScriptSystem()->BeginCall("Init");
	//GetIEditor()->GetSystem()->GetIScriptSystem()->EndCall();
	CEntityScriptRegistry::Instance()->Reload();
	// reload game specific (materials)
	//GetIEditor()->GetGame()->ReloadScripts();
	// Reload AI scripts.
	GetIEditor()->GetAI()->ReloadScripts();
	GetIEditor()->GetObjectManager()->SendEvent( EVENT_RELOAD_ENTITY );
}

void CCryEditApp::OnReloadGeometry() 
{
	CErrorsRecorder errRecorder;
	CWaitProgress wait( "Reloading static geometry" );

	CVegetationMap *vegetationMap = GetIEditor()->GetVegetationMap();
	CLogFile::WriteLine( "Reloading Static objects geometries." );
	int i;
	// Reload textures/shaders for exported static objects.
	for (i = 0; i < vegetationMap->GetObjectCount(); i++)
	{
		IStatObj *obj = vegetationMap->GetObject(i)->GetObject();
		if (obj)
		{
			obj->Refresh(FRO_SHADERS|FRO_TEXTURES|FRO_GEOMETRY);
		}
	}
	CEdMesh::ReloadAllGeometries();
	GetIEditor()->GetObjectManager()->SendEvent( EVENT_UNLOAD_GEOM );
	// Force entity system to collect garbage.
	GetIEditor()->GetSystem()->GetIEntitySystem()->Update();
	GetIEditor()->GetObjectManager()->SendEvent( EVENT_RELOAD_GEOM );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnReloadTerrain()
{
	CErrorsRecorder errRecorder;
	// Fast export.
	CGameExporter gameExporter( GetIEditor()->GetSystem() );
	gameExporter.Export(false,true);
	// Export does it. GetIEditor()->GetGameEngine()->ReloadLevel();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUndo() 
{
	//GetIEditor()->GetObjectManager()->UndoLastOp();
	GetIEditor()->Undo();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRedo() 
{
	GetIEditor()->Redo();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateRedo(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetUndoManager()->IsHaveRedo())
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateUndo(CCmdUI* pCmdUI) 
{
	if (GetIEditor()->GetUndoManager()->IsHaveUndo())
		pCmdUI->Enable(TRUE);
	else
		pCmdUI->Enable(FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainCollision()
{
	uint flags = GetIEditor()->GetDisplaySettings()->GetSettings();
	if (flags&SETTINGS_NOCOLLISION)
	{
		flags &= ~SETTINGS_NOCOLLISION;
	}
	else
	{
		flags |= SETTINGS_NOCOLLISION;
	}
	GetIEditor()->GetDisplaySettings()->SetSettings(flags);
}

void CCryEditApp::OnTerrainCollisionUpdate( CCmdUI *pCmdUI )
{
	uint flags = GetIEditor()->GetDisplaySettings()->GetSettings();
	if (flags&SETTINGS_NOCOLLISION)
		pCmdUI->SetCheck(0);
	else
		pCmdUI->SetCheck(1);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchPhysics()
{
	CWaitCursor wait;
	uint flags = GetIEditor()->GetDisplaySettings()->GetSettings();
	if (flags&SETTINGS_PHYSICS)
		flags &= ~SETTINGS_PHYSICS;
	else
		flags |= SETTINGS_PHYSICS;
	GetIEditor()->GetDisplaySettings()->SetSettings(flags);
	
	if ((flags&SETTINGS_PHYSICS) == 0)
	{
		GetIEditor()->GetGameEngine()->SetSimulationMode( false );
	}
	else
	{
		GetIEditor()->GetGameEngine()->SetSimulationMode( true );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchPhysicsUpdate( CCmdUI *pCmdUI )
{
	if (GetIEditor()->GetGameEngine()->GetSimulationMode())
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSyncPlayer()
{
	GetIEditor()->GetGameEngine()->SyncPlayerPosition( !GetIEditor()->GetGameEngine()->IsSyncPlayerPosition() );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSyncPlayerUpdate( CCmdUI *pCmdUI )
{
	if (GetIEditor()->GetGameEngine()->IsSyncPlayerPosition())
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGenerateCgfThumbnails()
{
	m_pMainWnd->BeginWaitCursor();
	CThumbnailGenerator gen;
	gen.GenerateForDirectory( "Objects\\" );
	m_pMainWnd->EndWaitCursor();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAiGenerateAllNavigation()
{
	CErrorsRecorder errRecorder;
	GetIEditor()->GetGameEngine()->GenerateAiAllNavigation();
	// Do game export
	CGameExporter gameExporter( GetIEditor()->GetSystem() );
	gameExporter.Export(false,false);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAiGenerateTriangulation()
{
	CErrorsRecorder errRecorder;
	GetIEditor()->GetGameEngine()->GenerateAiTriangulation();
	// Do game export
	CGameExporter gameExporter( GetIEditor()->GetSystem() );
	gameExporter.Export(false,false);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAiGenerateWaypoint()
{
	CErrorsRecorder errRecorder;
	GetIEditor()->GetGameEngine()->GenerateAiWaypoint();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAiGenerateFlightNavigation()
{
	CErrorsRecorder errRecorder;
	GetIEditor()->GetGameEngine()->GenerateAiFlightNavigation();
	// Do game export.
	CGameExporter gameExporter( GetIEditor()->GetSystem() );
	gameExporter.Export(false,false);
}
//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAiGenerate3dvolumes()
{
	CErrorsRecorder errRecorder;
	GetIEditor()->GetGameEngine()->GenerateAiNavVolumes();
	// Do game export.
	CGameExporter gameExporter( GetIEditor()->GetSystem() );
	gameExporter.Export(false,false);
}
//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAiValidateNavigation()
{
	CErrorsRecorder errRecorder;
	GetIEditor()->GetGameEngine()->ValidateAINavigation();
}
//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAiGenerate3DDebugVoxels()
{
	CErrorsRecorder errRecorder;
	GetIEditor()->GetGameEngine()->Generate3DDebugVoxels();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAiGenerateSpawners()
{
	GenerateSpawners();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCreateLevel()
{
	//CStringDlg nameDlg( "New Level Name",AfxGetMainWnd() );
	//if (nameDlg.DoModal() != IDOK)
		//return;

	CNewLevelDialog dlg;
	if (dlg.DoModal() != IDOK)
		return;

	CString levelName = dlg.GetLevel();
	int resolution = dlg.GetTerrainResolution();
	int unitSize = dlg.GetTerrainUnits();
	bool bUseTerrain = dlg.IsUseTerrain();

	char szLevelRoot[_MAX_PATH];
	char szFileName[_MAX_PATH];
	// Construct the directory name	
	sprintf(szLevelRoot, "%s\\Levels\\%s",(const char*)Path::GetGameFolder(),(const char*)levelName );

	// Does the directory already exist ?
	if (PathFileExists(szLevelRoot))
	{
		AfxMessageBox( "Level with this name aready exists, choose another name.");
		return;
	}

	// Create the directory
	CLogFile::WriteLine("Creating level directory");
	if (!CreateDirectory(szLevelRoot,0))
	{
		CString sError = CString("Failed to create level directory '")+szLevelRoot+"'";

		AfxMessageBox(sError);
		return;
	}

	OnFileNew();

	if (bUseTerrain)
	{
		GetIEditor()->GetDocument()->SetTerrainSize( resolution,unitSize );
	}


	// Save the document to this folder
	PathAddBackslash(szLevelRoot);
	sprintf(szFileName, "%s%s.cry",szLevelRoot,(const char*)levelName );
	CString sLevelPath = CString(szLevelRoot);

	GetIEditor()->GetDocument()->SetPathName(szFileName);
	if (GetIEditor()->GetDocument()->Save())
	{
		GetIEditor()->GetGameEngine()->SetLevelPath(sLevelPath);
		CGameExporter gameExporter( GetIEditor()->GetSystem() );
		gameExporter.Export(false,false);

		GetIEditor()->GetGameEngine()->LoadLevel( sLevelPath,GetIEditor()->GetGameEngine()->GetMissionName(),true,true,true );
		//GetIEditor()->GetGameEngine()->LoadAINavigationData();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenLevel()
{
	CString docFilename;

	/*
	{
		OPENFILENAME ofn;
		char szFile[260];       // buffer for file name
		strcpy(szFile,"");
		ZeroStruct(ofn);
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = "Crytek Level (*.cry)\0*.cry\0All files (*.*)\0*.*\0\0";
		ofn.nFilterIndex = 1;
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrInitialDir = "Levels";
		ofn.Flags = OFN_ENABLESIZING|OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
		if (GetOpenFileName( &ofn ) == 0)
			return;
		docFilename = ofn.lpstrFile;
	}
	*/

	//OnOpenLevel()
	//OnFileOpen();
	{
		CAutoRestoreMasterCDRoot autorestore;

		const char *szFilters = "CryEngine Sandbox Level (*.cry)|*.cry|All files (*.*)|*.*||";
		CAutoDirectoryRestoreFileDialog dlgFile( TRUE, NULL, NULL, OFN_ENABLESIZING|OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR, szFilters );		
		CString initDir(Path::MakeFullPath( Path::GetGameFolder()+"\\Levels" ));
		dlgFile.m_ofn.lpstrInitialDir = initDir;
		if (dlgFile.DoModal() != IDOK)
		{
			return;
		}
		docFilename = dlgFile.GetPathName();
	}
	OpenDocumentFile( docFilename );
}

//////////////////////////////////////////////////////////////////////////
CDocument* CCryEditApp::OpenDocumentFile(LPCTSTR lpszFileName)
{
/*
	// Create the path of the file that we are about to load 
	CString docPath = Path::GetPath(lpszFileName);
	docPath = Path::AddBackslash(docPath);

	// Check if the .cry file is in the correct folder
	if (Path::GetRelativePath(docPath).IsEmpty())
	{
		// Is this .cry file just the Hold / Fetch save ?
		if (stricmp( Path::GetFile(lpszFileName),HOLD_FETCH_FILE) != 0)
		{
			// Display user the warning
			int result = 	AfxMessageBox( _T("WARNING: You won't be able export because the .cry file is not in the Levels" \
				" folder inside the Master CD folder, or the editor executable is not in the Master CD" \
				" folder. This also prevents your from corretly using the engine preview in the 3D view." \
				" Levels must be in Levels\\ folder from the editor executable."),MB_OKCANCEL|MB_ICONWARNING );

			if (result == IDCANCEL)
				return 0;
		}
	}
*/

	CDocument *doc = 0;
	bool bVisible=false;
	bool bTriggerConsole=false;
	if (m_pMainWnd)
	{
		doc = ((CMainFrame*)m_pMainWnd)->GetActiveDocument();
		bVisible = GetIEditor()->ShowConsole(true);
		bTriggerConsole=true;
	}
	if (doc && doc->GetPathName() == lpszFileName)
	{
		// Reloading already opened document.
		doc->GetDocTemplate()->OpenDocumentFile(doc->GetPathName());
	}
	else
	{
		doc = CWinApp::OpenDocumentFile(lpszFileName);
	}
	if (bTriggerConsole)
	{
		GetIEditor()->ShowConsole(bVisible);
	}
	LoadTagLocations();
	return doc;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnLayerSelect()
{
	CPoint point;
	GetCursorPos( &point );
	CLayersSelectDialog dlg( point );
	dlg.SetSelectedLayer( GetIEditor()->GetObjectManager()->GetLayersManager()->GetCurrentLayer()->GetName() );
	if (dlg.DoModal() == IDOK)
	{
		CUndo undo( "Set Current Layer" );
		CObjectLayer *pLayer = GetIEditor()->GetObjectManager()->GetLayersManager()->FindLayerByName( dlg.GetSelectedLayer() );
		if (pLayer)
			GetIEditor()->GetObjectManager()->GetLayersManager()->SetCurrentLayer( pLayer );
	}
}
void CCryEditApp::OnRefCoordsSys()
{
	RefCoordSys coords = GetIEditor()->GetReferenceCoordSys();
	if (coords == COORDS_WORLD)
		coords = COORDS_LOCAL;
	else
		coords = COORDS_WORLD;
	GetIEditor()->SetReferenceCoordSys( coords );
}

void CCryEditApp::OnUpdateRefCoordsSys(CCmdUI *pCmdUI)
{
	RefCoordSys coords = GetIEditor()->GetReferenceCoordSys();
	if (coords == COORDS_WORLD)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnResourcesReduceworkingset()
{
	SetProcessWorkingSetSize( GetCurrentProcess(),-1,-1 );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsGeneratelightmaps()
{
	CErrorsRecorder errRecorder;
	CLMCompDialog cDialog(m_IEditor->GetSystem());
	cDialog.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGenLightmapsSelected()
{
	CWaitCursor cursor;
	CErrorsRecorder errRecorder;
	CLightmapGen lmGen;
	lmGen.GenerateSelected( GetIEditor() );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsUpdatelightmaps()
{
	CWaitCursor cursor;
	CErrorsRecorder errRecorder;
	CLightmapGen lmGen;
	lmGen.GenerateChanged( GetIEditor() );
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsEquipPacksEdit()
{
	CEquipPackDialog Dlg;
	Dlg.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsUpdateProcVegetation()
{
	GetIEditor()->GetGameEngine()->ReloadSurfaceTypes();

	CVegetationMap * pVegetationMap = GetIEditor()->GetVegetationMap();
	if(pVegetationMap)
		pVegetationMap->SetEngineObjectsParams();

	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditHide()
{
	// Hide selection.
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo( "Hide" );
		for (int i = 0; i < sel->GetCount(); i++)
		{
			GetIEditor()->GetObjectManager()->HideObject( sel->GetObject(i),true );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditHide(CCmdUI *pCmdUI)
{
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
		pCmdUI->Enable( TRUE );
	else
		pCmdUI->Enable( FALSE );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditUnhideall()
{
	// Unhide all.
	CUndo undo( "Unhide All" );
	GetIEditor()->GetObjectManager()->UnhideAll();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditFreeze()
{
	// Freeze selection.
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo( "Freeze" );
		for (int i = 0; i < sel->GetCount(); i++)
		{
			GetIEditor()->GetObjectManager()->FreezeObject( sel->GetObject(i),true );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateEditFreeze(CCmdUI *pCmdUI)
{
	OnUpdateEditHide( pCmdUI );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnEditUnfreezeall()
{
	// Unfreeze all.
	CUndo undo( "Unfreeze All" );
	GetIEditor()->GetObjectManager()->UnfreezeAll();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSnap()
{
	// Switch current snap to grid state.
	bool bGridEnabled = gSettings.pGrid->IsEnabled();
	gSettings.pGrid->Enable( !bGridEnabled );
}
	
void CCryEditApp::OnUpdateEditmodeSnap(CCmdUI* pCmdUI)
{
	bool bGridEnabled = gSettings.pGrid->IsEnabled();
	if (bGridEnabled)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

void CCryEditApp::OnWireframe()
{
	bool bWireframe = GetIEditor()->GetDisplaySettings()->GetDisplayMode() == DISPLAYMODE_WIREFRAME;
	if (!bWireframe)
		GetIEditor()->GetDisplaySettings()->SetDisplayMode( DISPLAYMODE_WIREFRAME );
	else
		GetIEditor()->GetDisplaySettings()->SetDisplayMode( DISPLAYMODE_SOLID );
}

void CCryEditApp::OnUpdateWireframe(CCmdUI *pCmdUI)
{
	bool bWireframe = GetIEditor()->GetDisplaySettings()->GetDisplayMode() == DISPLAYMODE_WIREFRAME;
	if (bWireframe)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnViewGridsettings()
{
	CGridSettingsDialog dlg;
	dlg.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnViewConfigureLayout()
{
	CLayoutWnd *layout = GetIEditor()->GetViewManager()->GetLayout();
	if (layout)
	{
		CLayoutConfigDialog dlg;
		dlg.SetLayout( layout->GetLayout() );
		if (dlg.DoModal() == IDOK)
		{
			// Will kill this Pane. so must be last line in this function.
			layout->CreateLayout( dlg.GetLayout() );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::TagLocation( int index )
{
	CViewport *pRenderViewport = GetIEditor()->GetViewManager()->GetGameViewport();
	if (!pRenderViewport)
		return;

	m_tagLocations[index-1] = pRenderViewport->GetViewTM().GetTranslation();
	m_tagAngles[index-1] = Ang3::GetAnglesXYZ( Matrix33(pRenderViewport->GetViewTM()) );
	// Save to file.
	char filename[_MAX_PATH];
	strcpy( filename,GetIEditor()->GetDocument()->GetPathName() );
	PathRemoveFileSpec( filename );
	strcat( filename,"\\tags.txt" );
	SetFileAttributes( filename,FILE_ATTRIBUTE_NORMAL );
	FILE *f = fopen( filename,"wt" );
	if (f)
	{
		for (int i = 0; i < 12; i++)
		{
			fprintf( f,"%f,%f,%f,%f,%f,%f\n",
								m_tagLocations[i].x,m_tagLocations[i].y,m_tagLocations[i].z,
								m_tagAngles[i].x,m_tagAngles[i].y,m_tagAngles[i].z);
		}
		fclose(f);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::GotoTagLocation( int index )
{
	if (!IsVectorsEqual(m_tagLocations[index-1],Vec3(0,0,0)))
	{
		// Change render viewport view TM to the stored one.
		CViewport *pRenderViewport = GetIEditor()->GetViewManager()->GetGameViewport();
		if (pRenderViewport)
		{
			Matrix34 tm = Matrix34::CreateRotationXYZ(  m_tagAngles[index-1] );
			tm.SetTranslation(m_tagLocations[index-1]);
			pRenderViewport->SetViewTM(tm);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::LoadTagLocations()
{
	char filename[_MAX_PATH];
	strcpy( filename,GetIEditor()->GetDocument()->GetPathName() );
	PathRemoveFileSpec( filename );
	strcat( filename,"\\tags.txt" );
	// Load tag locations from file.
	FILE *f = fopen( filename,"rt" );
	if (f)
	{
		for (int i = 0; i < 12; i++)
		{
			float x=0,y=0,z=0,ax=0,ay=0,az=0;
			fscanf( f,"%f,%f,%f,%f,%f,%f\n",&x,&y,&z,&ax,&ay,&az );
			m_tagLocations[i] = Vec3(x,y,z);
			m_tagAngles[i] = Ang3(ax,ay,az);
		}
		fclose(f);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsLogMemoryUsage()
{
	GetIEditor()->GetHeightmap()->LogLayerSizes();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTagLocation1() { TagLocation(1);}
void CCryEditApp::OnTagLocation2() { TagLocation(2);}
void CCryEditApp::OnTagLocation3() { TagLocation(3);}
void CCryEditApp::OnTagLocation4() { TagLocation(4);}
void CCryEditApp::OnTagLocation5() { TagLocation(5);}
void CCryEditApp::OnTagLocation6() { TagLocation(6);}
void CCryEditApp::OnTagLocation7() { TagLocation(7);}
void CCryEditApp::OnTagLocation8() { TagLocation(8);}
void CCryEditApp::OnTagLocation9() { TagLocation(9);}
void CCryEditApp::OnTagLocation10() { TagLocation(10);}
void CCryEditApp::OnTagLocation11() { TagLocation(11);}
void CCryEditApp::OnTagLocation12() { TagLocation(12);}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGotoLocation1() { GotoTagLocation(1);}
void CCryEditApp::OnGotoLocation2() { GotoTagLocation(2);}
void CCryEditApp::OnGotoLocation3() { GotoTagLocation(3);}
void CCryEditApp::OnGotoLocation4() { GotoTagLocation(4);}
void CCryEditApp::OnGotoLocation5() { GotoTagLocation(5);}
void CCryEditApp::OnGotoLocation6() { GotoTagLocation(6);}
void CCryEditApp::OnGotoLocation7() { GotoTagLocation(7);}
void CCryEditApp::OnGotoLocation8() { GotoTagLocation(8);}
void CCryEditApp::OnGotoLocation9() { GotoTagLocation(9);}
void CCryEditApp::OnGotoLocation10() { GotoTagLocation(10);}
void CCryEditApp::OnGotoLocation11() { GotoTagLocation(11);}
void CCryEditApp::OnGotoLocation12() { GotoTagLocation(12);}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainExportblock()
{
	// TODO: Add your command handler code here
	char szFilters[] = "Terrain Block files (*.trb)|*.trb|All files (*.*)|*.*||";
	CString filename;
	if (CFileUtil::SelectSaveFile( szFilters,"trb","",filename ))
	{
		CWaitCursor wait;
		BBox box;
		GetIEditor()->GetSelectedRegion(box);

		CPoint p1 = GetIEditor()->GetHeightmap()->WorldToHmap( box.min );
		CPoint p2 = GetIEditor()->GetHeightmap()->WorldToHmap( box.max );
		CRect rect( p1,p2 );

		CXmlArchive ar("Root");
		GetIEditor()->GetHeightmap()->ExportBlock( rect,ar );

		// Save selected objects.
		CSelectionGroup *sel = GetIEditor()->GetSelection();
		XmlNodeRef objRoot = ar.root->newChild("Objects");
		CObjectArchive objAr( GetIEditor()->GetObjectManager(),objRoot,false );
		// Save all objects to XML.
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject *obj = sel->GetObject(i);
			objAr.node = objRoot->newChild( "Object" );
			obj->Serialize( objAr );
		}

		ar.Save( filename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainImportblock()
{
	// TODO: Add your command handler code here
	char szFilters[] = "Terrain Block files (*.trb)|*.trb|All files (*.*)|*.*||";
	CString filename;
	if (CFileUtil::SelectFile( szFilters,"",filename ))
	{
		CWaitCursor wait;
		CXmlArchive *ar = new CXmlArchive;
		if (!ar->Load( filename ))
		{
			MessageBox( AfxGetMainWnd()->GetSafeHwnd(),_T("Loading of Terrain Block file failed"),_T("Warning"),MB_OK|MB_ICONWARNING );
			delete ar;
			return;
		}

		CErrorsRecorder errorsRecorder;

		// Import terrain area.
		CUndo undo( "Import Terrain Area" );

		CHeightmap *pHeightamp = GetIEditor()->GetHeightmap();
		pHeightamp->ImportBlock( *ar,CPoint(0,0),false );
		// Load selection from archive.
		XmlNodeRef objRoot = ar->root->findChild("Objects");
		if (objRoot)
		{
			GetIEditor()->ClearSelection();
			CObjectArchive ar( GetIEditor()->GetObjectManager(),objRoot,true );
			GetIEditor()->GetObjectManager()->LoadObjects( ar,true );
		}

		delete ar;
		ar = 0;

		/*
		// Archive will be deleted within Move tool.
		CTerrainMoveTool *mt = new CTerrainMoveTool;
		mt->SetArchive( ar );
		GetIEditor()->SetEditTool( mt );
		*/
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateTerrainExportblock(CCmdUI *pCmdUI)
{
	BBox box;
	GetIEditor()->GetSelectedRegion(box);
	if (box.IsEmpty())
		pCmdUI->Enable(FALSE);
	else
		pCmdUI->Enable(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateTerrainImportblock(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCustomizeKeyboard()
{
	((CMainFrame*)m_pMainWnd)->EditAccelerator();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsConfiguretools()
{
	CXTResizePropertySheet dlg( IDS_TOOLSCONFIG,AfxGetMainWnd() );
	CToolsConfigPage page1;
	dlg.AddPage( &page1 );
	if (dlg.DoModal() == IDOK)
	{
		//((CMainFrame*)m_pMainWnd)->UpdateToolsMenu();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushTool()
{
	// Enable brush tool.
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CBrushTool)))
	{
		// Already active.
		return;
	}
	pTool = new CBrushTool;
	GetIEditor()->SetEditTool(pTool);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateBrushTool(CCmdUI *pCmdUI)
{
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CBrushTool)))
	{
		pCmdUI->SetCheck(1);
	}
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnExportIndoors()
{
	//CBrushIndoor *indoor = GetIEditor()->GetObjectManager()->GetCurrentIndoor();
	//CBrushExporter exp;
	//exp.Export( indoor,"C:\\MasterCD\\Objects\\Indoor.bld" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnViewCycle2dviewport()
{
	GetIEditor()->GetViewManager()->Cycle2DViewport();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnDisplayGotoPosition()
{
	CGotoPositionDlg dlg;
	dlg.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSnapangle()
{
	gSettings.pGrid->EnableAngleSnap( !gSettings.pGrid->IsAngleSnapEnabled() );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSnapangle(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck( gSettings.pGrid->IsAngleSnapEnabled()?1:0 );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRotateselectionXaxis()
{
	CUndo undo( "Rotate X" );
	CSelectionGroup *pSelection = GetIEditor()->GetSelection();
	pSelection->Rotate( Ang3(m_fastRotateAngle,0,0),GetIEditor()->GetReferenceCoordSys() );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRotateselectionYaxis()
{
	CUndo undo( "Rotate Y" );
	CSelectionGroup *pSelection = GetIEditor()->GetSelection();
	pSelection->Rotate( Ang3(0,m_fastRotateAngle,0),GetIEditor()->GetReferenceCoordSys() );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRotateselectionZaxis()
{
	CUndo undo( "Rotate Z" );
	CSelectionGroup *pSelection = GetIEditor()->GetSelection();
	pSelection->Rotate( Ang3(0,0,m_fastRotateAngle),GetIEditor()->GetReferenceCoordSys() );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnRotateselectionRotateangle()
{
	CNumberDlg dlg( AfxGetMainWnd(),m_fastRotateAngle,"Rotate Angle" );
	if (dlg.DoModal() == IDOK)
	{
		m_fastRotateAngle = dlg.GetValue();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnConvertselectionTobrushes()
{
	std::vector<CBaseObjectPtr> objects;
	// Convert every possible object in selection to the brush.
	CSelectionGroup *pSelection = GetIEditor()->GetSelection();
	for (int i = 0; i < pSelection->GetCount(); i++)
	{
		objects.push_back( pSelection->GetObject(i) );
	}

	for (int i = 0; i < objects.size(); i++)
	{
		GetIEditor()->GetObjectManager()->ConvertToType( objects[i],OBJTYPE_BRUSH );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnConvertselectionTosimpleentity()
{
	std::vector<CBaseObjectPtr> objects;
	// Convert every possible object in selection to the brush.
	CSelectionGroup *pSelection = GetIEditor()->GetSelection();
	for (int i = 0; i < pSelection->GetCount(); i++)
	{
		objects.push_back( pSelection->GetObject(i) );
	}

	for (int i = 0; i < objects.size(); i++)
	{
		GetIEditor()->GetObjectManager()->ConvertToType( objects[i],OBJTYPE_ENTITY );
	}
}


void CCryEditApp::OnEditRenameobject()
{
	CSelectionGroup *pSelection = GetIEditor()->GetSelection();
	if (pSelection->IsEmpty())
	{
		AfxMessageBox( _T("No Selected Objects!") );
		return;
	}
	CStringDlg dlg( _T("Rename Object(s)"),AfxGetMainWnd() );
	if (dlg.DoModal())
	{
		CUndo undo("Rename Objects");
		CString newName;
		CString str = dlg.GetString();
		int num = 0;
		for (int i = 0; i < pSelection->GetCount(); i++)
		{
			newName.Format( "%s%d",(const char*)str,num );
			num++;
			CBaseObject *pObject = pSelection->GetObject(i);
			GetIEditor()->GetObjectManager()->ChangeObjectName( pObject,newName );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnChangemovespeedIncrease()
{
	gSettings.cameraMoveSpeed += m_moveSpeedStep;
	if (gSettings.cameraMoveSpeed < 0.01f)
		gSettings.cameraMoveSpeed = 0.01f;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnChangemovespeedDecrease()
{
	gSettings.cameraMoveSpeed -= m_moveSpeedStep;
	if (gSettings.cameraMoveSpeed < 0.01f)
		gSettings.cameraMoveSpeed = 0.01f;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnChangemovespeedChangestep()
{
	CNumberDlg dlg( AfxGetMainWnd(),m_moveSpeedStep,"Change Move Increase/Decrease Step" );
	if (dlg.DoModal() == IDOK)
	{
		m_moveSpeedStep = dlg.GetValue();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnModifyAipointPicklink()
{
	// Special command to emulate pressing pick button in ai point.
	CBaseObject *pObject = GetIEditor()->GetSelectedObject();
	if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CAIPoint)))
	{
		((CAIPoint*)pObject)->StartPick();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnModifyAipointPickImpasslink()
{
	// Special command to emulate pressing pick impass button in ai point.
	CBaseObject *pObject = GetIEditor()->GetSelectedObject();
	if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CAIPoint)))
	{
		((CAIPoint*)pObject)->StartPickImpass();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPhysicsGetState()
{
	CSelectionGroup *pSelection = GetIEditor()->GetSelection();
	for (int i = 0; i < pSelection->GetCount(); i++)
	{
		pSelection->GetObject(i)->OnEvent( EVENT_PHYSICS_GETSTATE );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPhysicsResetState()
{
	CSelectionGroup *pSelection = GetIEditor()->GetSelection();
	for (int i = 0; i < pSelection->GetCount(); i++)
	{
		pSelection->GetObject(i)->OnEvent( EVENT_PHYSICS_RESETSTATE );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPhysicsSimulateObjects()
{
	GetIEditor()->GetCommandManager()->Execute( "Physics.SimulateObjects" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileSourcesafesettings()
{
	CSrcSafeSettingsDialog dlg;
	dlg.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileSavelevelresources()
{
	CGameResourcesExporter saver;
	saver.ChooseDirectoryAndSave();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnValidatelevel()
{
	// TODO: Add your command handler code here
	CLevelInfo levelInfo;
	levelInfo.Validate();
}

void CCryEditApp::OnHelpDynamichelp()
{
	// Opens dynamic help window.
	CDynamicHelpDialog::Open();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnFileChangemod()
{
	CStringDlg dlg( "Select Current MOD",AfxGetMainWnd() );
	dlg.SetString( GetIEditor()->GetGameEngine()->GetCurrentMOD() );
	if (dlg.DoModal() == IDOK)
	{
		CString mod = dlg.GetString();
		GetIEditor()->GetGameEngine()->SetCurrentMOD( mod );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainResizeterrain()
{
	CHeightmap *pHeightmap = GetIEditor()->GetHeightmap();

	CNewLevelDialog dlg;
	dlg.SetTerrainResolution( pHeightmap->GetWidth() );
	dlg.SetTerrainUnits( pHeightmap->GetUnitSize() );
	dlg.IsResize(true);
	if (dlg.DoModal() != IDOK)
		return;

	int resolution = dlg.GetTerrainResolution();
	int unitSize = dlg.GetTerrainUnits();

	if (resolution != pHeightmap->GetWidth() || unitSize != pHeightmap->GetUnitSize())
	{
		pHeightmap->Resize( resolution,resolution,unitSize,false );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolsPreferences()
{
	// Open preferences dialog.
	CPreferencesDialog dlg;
	dlg.DoModal();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPrefabsMakeFromSelection()
{
	GetIEditor()->GetPrefabManager()->MakeFromSelection();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnPrefabsRefreshAll()
{
	GetIEditor()->GetObjectManager()->SendEvent( EVENT_PREFAB_REMAKE );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAddSelectionToPrefab()
{
	GetIEditor()->GetPrefabManager()->AddSelectionToPrefab();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnToolterrainmodifySmooth()
{
	GetIEditor()->GetCommandManager()->Execute( "EditTool.TerrainModifyTool.Flatten" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainmodifySmooth()
{
	GetIEditor()->GetCommandManager()->Execute( "EditTool.TerrainModifyTool.Smooth" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainVegetation()
{
	GetIEditor()->GetCommandManager()->Execute( "EditTool.VegetationTool.Activate" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTerrainPaintlayers()
{
	// TODO: Add your command handler code here
}

void CCryEditApp::OnAvirecorderStartavirecording()
{
	CViewport *pViewport = GetIEditor()->GetViewManager()->GetActiveViewport();
	if (pViewport)
	{
		if (m_aviFilename.IsEmpty())
		{
			if (!CFileUtil::SelectSaveFile( "AVI Files (*.avi)|*.avi","avi","",m_aviFilename ))
				return;
		}
		if (!m_aviFilename.IsEmpty())
			pViewport->StartAVIRecording( m_aviFilename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAviRecorderStop()
{
	CViewport *pViewport = GetIEditor()->GetViewManager()->GetActiveViewport();
	if (pViewport)
	{
		pViewport->StopAVIRecording();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAviRecorderPause()
{
	CViewport *pViewport = GetIEditor()->GetViewManager()->GetActiveViewport();
	if (pViewport)
	{
		pViewport->PauseAVIRecording( !pViewport->IsAVIRecordingPaused() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnAviRecorderOutputFilename()
{
	CFileUtil::SelectSaveFile( "AVI Files (*.avi)|*.avi","avi","",m_aviFilename );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchcameraDefaultcamera()
{
	CViewport *vp = GetIEditor()->GetActiveView();
	if (vp && vp->IsKindOf(RUNTIME_CLASS(CRenderViewport)))
		((CRenderViewport*)vp)->SetDefaultCamera();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchcameraSequencecamera()
{
	CViewport *vp = GetIEditor()->GetActiveView();
	if (vp && vp->IsKindOf(RUNTIME_CLASS(CRenderViewport)))
		((CRenderViewport*)vp)->SetSequenceCamera();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSwitchcameraSelectedcamera()
{
	CViewport *vp = GetIEditor()->GetActiveView();
	if (vp && vp->IsKindOf(RUNTIME_CLASS(CRenderViewport)))
		((CRenderViewport*)vp)->SetSelectedCamera();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMaterialAssigncurrent()
{
	GetIEditor()->ExecuteCommand( "Viewport.SetSelectedCamera" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMaterialResettodefault()
{
	GetIEditor()->ExecuteCommand("Material.ResetSelection");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnMaterialGetmaterial()
{
	GetIEditor()->ExecuteCommand("Material.SelectFromObject");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenMaterialEditor()
{
	GetIEditor()->ExecuteCommand("Editor.Open_MaterialEditor");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenCharacterEditor()
{
	GetIEditor()->ExecuteCommand("Editor.Open_CharacterEditor");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenDataBaseView()
{
	GetIEditor()->ExecuteCommand("Editor.Open_DataBaseView");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnOpenFlowGraphView()
{
	GetIEditor()->ExecuteCommand("Editor.Open_FlowGraph");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushResettransform()
{
	GetIEditor()->ExecuteCommand("Brush.ResetTransform");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushMakehollow()
{
	GetIEditor()->ExecuteCommand("Brush.MakeHollow");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushCsgcombine()
{
	GetIEditor()->ExecuteCommand("Brush.CSGCombine");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushCsgintersect()
{
	GetIEditor()->ExecuteCommand("Brush.CSGIntersect");
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushCsgsubstruct()
{
	GetIEditor()->ExecuteCommand("Brush.CSGSubstruct");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushCsgsubstruct2()
{
	GetIEditor()->ExecuteCommand("Brush.CSGSubstruct2");
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushCliptool()
{
	GetIEditor()->SetEditTool("EditTool.ClipBrush");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnBrushUvtool()
{
	GetIEditor()->SetEditTool("EditTool.TextureBrush");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSubobjectmodeVertex()
{
	GetIEditor()->ExecuteCommand("EditMode.SelectVertex");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSubobjectmodeEdge()
{
	GetIEditor()->ExecuteCommand("EditMode.SelectEdge");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSubobjectmodeFace()
{
	GetIEditor()->ExecuteCommand("EditMode.SelectFace");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnSubobjectmodePolygon()
{
	GetIEditor()->ExecuteCommand("EditMode.SelectPolygon");
}

void CCryEditApp::OnMaterialPicktool()
{
	GetIEditor()->SetEditTool("EditTool.PickMaterial");
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloudsCreate() 
{
	CStringDlg dlg( "Cloud Name" );
	dlg.m_strString = GetIEditor()->GetObjectManager()->GenUniqObjectName( "Cloud" );
	if (dlg.DoModal() == IDOK)
	{
		GetIEditor()->BeginUndo();

		CCloudGroup *group = (CCloudGroup*)GetIEditor()->NewObject( "Cloud" );
		if (!group)
		{
			GetIEditor()->CancelUndo();
			return;
		}
		GetIEditor()->GetObjectManager()->ChangeObjectName( group,dlg.m_strString );

		CSelectionGroup *selection = GetIEditor()->GetSelection();
		selection->FilterParents();

		int i;
		std::vector<CBaseObjectPtr> objects;
		for (i = 0; i < selection->GetFilteredCount(); i++)
		{
			objects.push_back( selection->GetFilteredObject(i) );
		}

		// Snap center to grid.
		Vec3 center = gSettings.pGrid->Snap( selection->GetCenter() );
		group->SetPos( center );

		for (i = 0; i < objects.size(); i++)
		{
			GetIEditor()->GetObjectManager()->UnselectObject(objects[i]);
			group->AttachChild( objects[i] );
		}
		GetIEditor()->AcceptUndo( "Cloud Make" );
		GetIEditor()->SetModifiedFlag();
		GetIEditor()->SelectObject(group);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloudsDestroy() 
{
	// Ungroup all groups in selection.
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo( "CloudsDestroy" );
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject *obj = sel->GetObject(i);
			if (obj && obj->GetRuntimeClass() == RUNTIME_CLASS(CCloudGroup))
			{
				((CGroup*)obj)->Ungroup();
				GetIEditor()->DeleteObject( obj );
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateCloudsDestroy(CCmdUI* pCmdUI) 
{
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj)
	{
		if (obj->GetRuntimeClass() == RUNTIME_CLASS(CCloudGroup))
			pCmdUI->Enable( TRUE );
		else
			pCmdUI->Enable( FALSE );
	}
	else
	{
		OnUpdateSelected( pCmdUI );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloudsOpen() 
{
	// Ungroup all groups in selection.
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	if (!sel->IsEmpty())
	{
		CUndo undo( "Clouds Open" );
		for (int i = 0; i < sel->GetCount(); i++)
		{
			CBaseObject *obj = sel->GetObject(i);
			if (obj && obj->GetRuntimeClass() == RUNTIME_CLASS(CCloudGroup))
			{
				((CGroup*)obj)->Open();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateCloudsOpen(CCmdUI* pCmdUI) 
{
	BOOL bEnable = FALSE;
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj)
	{
		if (obj->GetRuntimeClass() == RUNTIME_CLASS(CCloudGroup))
		{
			if (!((CGroup*)obj)->IsOpen())
				bEnable = TRUE;
		}
		pCmdUI->Enable( bEnable );
	}
	else
	{
		OnUpdateSelected( pCmdUI );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnCloudsClose() 
{
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj)
	{
		GetIEditor()->BeginUndo();
		((CGroup*)obj)->Close();
		GetIEditor()->AcceptUndo( "Clouds Close" );
		GetIEditor()->SetModifiedFlag();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateCloudsClose(CCmdUI* pCmdUI) 
{
	BOOL bEnable = FALSE;
	CBaseObject *obj = GetIEditor()->GetSelectedObject();
	if (obj && obj->GetRuntimeClass() == RUNTIME_CLASS(CCloudGroup))
	{
		if (((CGroup*)obj)->IsOpen())
			bEnable = TRUE;
	}

	pCmdUI->Enable( bEnable );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnTimeOfDay()
{
	((CMainFrame*)m_pMainWnd)->OpenPage( "Time Of Day" );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnClearLevelShaderList()
{
	GetIEditor()->GetDocument()->GetShaderCache()->Clear();
	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGenerateVertexScattering()
{
	GetDocument()->GenerateVertexScatter();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnChangeGameSpec(UINT nID)
{
	switch (nID)
	{
	case ID_GAME_ENABLELOWSPEC:
		GetIEditor()->SetEditorConfigSpec(CONFIG_LOW_SPEC);
		break;
	case ID_GAME_ENABLEMEDIUMSPEC:
		GetIEditor()->SetEditorConfigSpec(CONFIG_MEDIUM_SPEC);
		break;
	case ID_GAME_ENABLEHIGHSPEC:
		GetIEditor()->SetEditorConfigSpec(CONFIG_HIGH_SPEC);
		break;
	case ID_GAME_ENABLEVERYHIGHSPEC:
		GetIEditor()->SetEditorConfigSpec(CONFIG_VERYHIGH_SPEC);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateGameSpec(CCmdUI *pCmdUI)
{
	int nCheck = 0;
	bool enable = true;
	switch (pCmdUI->m_nID)
	{
	case ID_GAME_ENABLELOWSPEC:
		if (GetIEditor()->GetEditorConfigSpec() == CONFIG_LOW_SPEC)
			nCheck = 1;
		enable = CONFIG_LOW_SPEC <= GetIEditor()->GetSystem()->GetMaxConfigSpec();
		break;
	case ID_GAME_ENABLEMEDIUMSPEC:
		if (GetIEditor()->GetEditorConfigSpec() == CONFIG_MEDIUM_SPEC)
			nCheck = 1;
		enable = CONFIG_MEDIUM_SPEC <= GetIEditor()->GetSystem()->GetMaxConfigSpec();
		break;
	case ID_GAME_ENABLEHIGHSPEC:
		if (GetIEditor()->GetEditorConfigSpec() == CONFIG_HIGH_SPEC)
			nCheck = 1;
		enable = CONFIG_HIGH_SPEC <= GetIEditor()->GetSystem()->GetMaxConfigSpec();
		break;
	case ID_GAME_ENABLEVERYHIGHSPEC:
		if (GetIEditor()->GetEditorConfigSpec() == CONFIG_VERYHIGH_SPEC)
			nCheck = 1;
		enable = CONFIG_VERYHIGH_SPEC <= GetIEditor()->GetSystem()->GetMaxConfigSpec();
		break;
	}
	pCmdUI->SetCheck(nCheck);
	pCmdUI->Enable(enable ? TRUE : FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGameEnableVeryHighSpec()
{
	GetIEditor()->SetEditorConfigSpec( CONFIG_VERYHIGH_SPEC );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGameEnableHighSpec()
{
	GetIEditor()->SetEditorConfigSpec( CONFIG_HIGH_SPEC );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGameEnableMedSpec()
{
	GetIEditor()->SetEditorConfigSpec( CONFIG_MEDIUM_SPEC );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGameEnableLowSpec()
{
	GetIEditor()->SetEditorConfigSpec( CONFIG_LOW_SPEC );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnGameEnableSketchMode()
{
	if (gEnv && gEnv->pConsole)
	{
		// Switch sketch mode on/off.
		ICVar *e_sketch_mode = gEnv->pConsole->GetCVar("e_sketch_mode");
		if (e_sketch_mode)
		{
			if (e_sketch_mode->GetIVal() == 0)
				e_sketch_mode->Set(1);
			else
				e_sketch_mode->Set(0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::OnUpdateSketchMode( CCmdUI *pCmdUI )
{
	ICVar *e_sketch_mode = 0;
	if (gEnv && gEnv->pConsole)
		e_sketch_mode = gEnv->pConsole->GetCVar("e_sketch_mode");
	if (e_sketch_mode && e_sketch_mode->GetIVal() != 0)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditApp::AddToRecentFileList(LPCTSTR lpszPathName)
{
	__super::AddToRecentFileList(lpszPathName);

	// write the list immediately so it will be remembered even after a crash
	GetRecentFileList()->WriteList();
}
