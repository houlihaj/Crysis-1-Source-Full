#if !defined(AFX_PLUGIN_H__434E7C19_6FG4425_599FA_86ECA760280C__INCLUDED_)
#define AFX_PLUGIN_H__434E7C19_6FG4425_599FA_86ECA760280C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef PLUGIN_EXPORTS
#define PLUGIN_API DLL_EXPORT
#else
#define PLUGIN_API DLL_IMPORT
#endif

// forward declarations.
class CBaseObject;
class CCryEditDoc;
class CSelectionGroup;
class CEditTool;
class CAnimationContext;
class CGameEngine;
class CIconManager;
class CExternalToolsManager;
class CClassFactory;
class CEntityPrototypeManager;
class CMaterialManager;
class CMusicManager;
class CMaterail;
class CEntityPrototype;
class CParticleManager;
class CPrefabManager;
class CGameTokenManager;
class CEAXPresetManager;
class CErrorReport;
class CBaseLibraryItem;
class CBaseLibraryDialog;
class CCommandManager;
class CHyperGraphManager;
class CFlowGraphManager;
class CUIEnumsDatabase;
struct ISourceControl;
struct IEditorClassFactory;
struct IDataBaseItem;
struct ITransformManipulator;
struct IDataBaseManager;
class IFacialEditor;
class CDialog;

#define SUPPORTED_MODEL_FILTER "Geometry Files (*.cgf)|*.cgf;*.ccgf;*.cga|All Files (*.*)|*.*||"
#define SUPPORTED_IMAGES_FILTER "All Image Files|*.bmp;*.jpg;*.gif;*.pgm;*.raw|All files|*.*||"
#define SUPPORTED_IMAGES_FILTER_SAVE "BMP Files|*.bmp|JPEG Files|*.jpg|PGM Files|*.pgm|RAW Files|*.raw|All files|*.*||"
#define SUPPOTED_SOUND_FILTER "Sounds Files|*.wav;*.mp2;*.mp3|Wave Files (*.wav)|*.wav|MP2 Files (*.mp2)|*.mp2|MP3 Files (*.mp3)|*.mp3|All Files|*.*||"

// Global editor notify events.
enum EEditorNotifyEvent
{
	// Global events.
	eNotify_OnInit = 10,               // Sent after editor fully initialized.
	eNotify_OnQuit,                    // Sent before editor quits.
	eNotify_OnIdleUpdate,              // Sent every frame while editor is idle.

	// Document events.
	eNotify_OnBeginNewScene,           // Sent when the document is begin to be cleared.
	eNotify_OnEndNewScene,             // Sent after the document have been cleared.
	eNotify_OnBeginSceneOpen,          // Sent when document is about to be opened.
	eNotify_OnEndSceneOpen,            // Sent after document have been opened.
	eNotify_OnBeginSceneSave,          // Sent when document is about to be saved.
	eNotify_OnEndSceneSave,            // Sent after document have been saved.
	eNotify_OnCloseScene,              // Send when the document is about to close.
	eNotify_OnMissionChange,           // Send when the current mission changes.

	//////////////////////////////////////////////////////////////////////////
	// Editing events.
	//////////////////////////////////////////////////////////////////////////
	eNotify_OnEditModeChange,          // Sent when editing mode change (move,rotate,scale,....)
	eNotify_OnEditToolChange,          // Sent when edit tool is changed (ObjectMode,TerrainModify,....)

	// Game related events.
	eNotify_OnBeginGameMode,           // Sent when editor goes to game mode.
	eNotify_OnEndGameMode,             // Sent when editor goes out of game mode.
	eNotify_OnConfigSpecChange,        // Sent when configuration spec changes.

	// AI/Physics simulation related events.
	eNotify_OnBeginSimulationMode,     // Sent when simulation mode is started.
	eNotify_OnEndSimulationMode,       // Sent when editor goes out of simulation mode.

	// UI events.
	eNotify_OnUpdateViewports,         // Sent when editor needs to update data in the viewports.
	eNotify_OnUpdateTrackView,         // Sent when editor needs to update the track view.
	eNotify_OnUpdateTrackViewKeys,     // Sent when editor needs to update keys in the track view.
	eNotify_OnInvalidateControls,      // Sent when editor needs to update some of the data that can be cached by controls like combo boxes.

	// Object events.
	eNotify_OnSelectionChange,         // Sent when object selection change.
	eNotify_OnPlaySequence,            // Sent when editor start playing animation sequence.
	eNotify_OnStopSequence,            // Sent when editor stop playing animation sequence.

	// Task specific events.
	eNotify_OnBeginTerrainTexGen,     // Sent when editor start terrain texture generation.
	eNotify_OnEndTerrainTexGen,       // Sent when editor finish terrain texture generation.
	eNotify_OnVegetationObjectSelection, // When vegetation objects selection change.
	eNotify_OnVegetationPanelUpdate, // When vegetation objects selection change.

	eNotify_OnDisplayRenderUpdate,       // Sent when editor finish terrain texture generation.
};

// UI event handler
struct IUIEvent
{
	virtual void OnClick(DWORD dwId) = 0;
	virtual bool IsEnabled(DWORD dwId) = 0;
	virtual bool IsChecked(DWORD dwId) = 0;
	virtual const char * GetUIElementName(DWORD dwId) = 0;
};

/** Add object that implements this interface to Load listeners of IEditor
		To recieve notifications when new document is loaded.
*/
struct IDocListener
{
	/** Called after new level is created.
	*/
	virtual	void OnNewDocument() = 0;
	/** Called after level have been loaded.
	*/
	virtual	void OnLoadDocument() = 0;
	/** Called when document is being closed.
	*/
	virtual void OnCloseDocument() = 0;
	/** Called when mission changes.
	*/
	virtual void OnMissionChange() = 0;
};

// Description:
//    Derrive from this class if you want to register for getting global editor notifications.
struct IEditorNotifyListener
{
	// Called by the editor to notify the listener about the specified event.
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event ) = 0;
};

enum ERollupControlIds
{
	ROLLUP_OBJECTS = 0,
	ROLLUP_TERRAIN,
	ROLLUP_MODELLING,
	ROLLUP_DISPLAY,
	ROLLUP_LAYERS,
};

//! Axis constrains value.
enum AxisConstrains
{
	AXIS_X	= 1,
	AXIS_Y,
	AXIS_Z,
	AXIS_XY,
	AXIS_YZ,
	AXIS_XZ,
	AXIS_XYZ,
	//! Follow terrain constrain.
	AXIS_TERRAIN,
};

//! Reference coordinate system values.
enum RefCoordSys
{
	COORDS_VIEW = 0,
	COORDS_LOCAL,
	COORDS_PARENT,
	COORDS_WORLD,
	COORDS_USERDEFINED,

	LAST_COORD_SYSTEM, // Must always be a last member.
};

// Insert locations for menu items
enum eMenuInsertLocation
{
	// Custom menu of the plugin
	eMenuPlugin,

	// Pre-defined editor menus
	eMenuEdit,
	eMenuFile,
	eMenuInsert,
	eMenuGenerators,
	eMenuScript,
	eMenuView,
	eMenuHelp
};

//////////////////////////////////////////////////////////////////////////
// Global editor operation mode.
//////////////////////////////////////////////////////////////////////////
enum EOperationMode
{
	eOperationModeNone = 0, // None
	eCompositingMode, // Normal operation mode where objects are composited in the scene.
	eModellingMode    // Geometry modeling mode.
};

enum EEditMode
{
	eEditModeSelect,
	eEditModeSelectArea,
	eEditModeMove,
	eEditModeRotate,
	eEditModeScale,
	eEditModeTool,
};

//! Mouse events that viewport can send.
enum EMouseEvent
{
	eMouseMove,
	eMouseLDown,
	eMouseLUp,
	eMouseLDblClick,
	eMouseRDown,
	eMouseRUp,
	eMouseRDblClick,
	eMouseMDown,
	eMouseMUp,
	eMouseMDblClick,
};

/** Viewports update flags.
*/
enum UpdateConentFlags
{
	eUpdateHeightmap	= 0x01,
	eUpdateStatObj		= 0x02,
	eUpdateObjects		= 0x04,	//! Update objects in viewport.
	eRedrawViewports	= 0x08	//! Just redraw viewports..
};

//////////////////////////////////////////////////////////////////////////
enum MouseCallbackFlags
{
	MK_CALLBACK_FLAGS = 0x100
};

//////////////////////////////////////////////////////////////////////////
// Types of database items.
enum EDataBaseItemType
{
	EDB_TYPE_MATERIAL,
	EDB_TYPE_PARTICLE,
	EDB_TYPE_MUSIC,
	EDB_TYPE_ENTITY_ARCHETYPE,
	EDB_TYPE_PREFAB,
	EDB_TYPE_EAXPRESET,
	EDB_TYPE_SOUNDMOOD,
	EDB_TYPE_GAMETOKEN,
};

enum EEditorPathName
{
	EDITOR_PATH_OBJECTS,
	EDITOR_PATH_TEXTURES,
	EDITOR_PATH_SOUNDS,
	EDITOR_PATH_MATERIALS,
	EDITOR_PATH_LAST,
};

//! Callback class passed to PickObject.
struct IPickObjectCallback
{
	//! Called when object picked.
	virtual void OnPick( CBaseObject *picked ) = 0;
	//! Called when pick mode cancelled.
	virtual void OnCancelPick() = 0;

	//! Return true if specified object is pickable.
	virtual bool OnPickFilter( CBaseObject *filterObject ) { return true; };
};

//////////////////////////////////////////////////////////////////////////
/*! Class provided by editor for various registration functions.
 */
struct CRegistrationContext
{
	CCommandManager *pCommandManager;
	CClassFactory *pClassFactory;
};


// Interface to permit usage of editor functionality inside the plugin
struct IEditor
{
	virtual void DeleteThis() = 0;

	//! Access to Editor ISystem interface.
	virtual ISystem*	GetSystem() = 0;
	virtual IGame*		GetGame() = 0;
	virtual I3DEngine*	Get3DEngine() = 0;
	virtual IRenderer*	GetRenderer() = 0;

	//! Access to class factory.
	virtual IEditorClassFactory* GetClassFactory() = 0;

	//! Access to commands manager.
	virtual CCommandManager* GetCommandManager() = 0;

	// Executes an Editor command.
	virtual void ExecuteCommand( const char *sCommand ) = 0;

	virtual void SetDocument( CCryEditDoc *pDoc ) = 0;
	//! Get active document
	virtual CCryEditDoc* GetDocument() = 0;
	//! Set document modified flag.
	virtual void SetModifiedFlag( bool modified = true ) = 0;
	//! Check if active document is modified.
	virtual bool IsModified() = 0;

	//! Save current document.
	virtual bool SaveDocument() = 0;

	// Write the passed string to the editors console
	virtual void WriteToConsole(const char * pszString) = 0;
	
	//! Set value of console variable.
	virtual void SetConsoleVar( const char *var,float value ) = 0;

	//! Get value of console variable.
	virtual float GetConsoleVar( const char *var ) = 0;

	//! Shows or Hides console window.
	//! @return Previous visibility flag of console.
	virtual bool ShowConsole( bool show ) = 0;

	// Change the message in the status bar
	virtual void SetStatusText(const char * pszString) = 0;

	// Query main window of the editor
	virtual HWND GetEditorMainWnd() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Paths.
	//////////////////////////////////////////////////////////////////////////
	// Returns the path of the editors Master CD folder
	virtual CStringW GetMasterCDFolder() = 0;
	//! Get path to folder of current level.
	virtual CString GetLevelFolder() = 0;
	//! Get path to folder of current level.
	virtual CString GetSearchPath( EEditorPathName path ) = 0;
	//////////////////////////////////////////////////////////////////////////

	//! Execute application and get console output.
	virtual bool ExecuteConsoleApp( const CString &CommandLine, CString &OutputText ) = 0;

	// Sets the document modified flag in the editor
	virtual void SetDataModified() = 0;

	//! Check if editor running in gaming mode.
	virtual bool IsInGameMode() = 0;
	//! Set game mode of editor.
	virtual void SetInGameMode( bool inGame ) = 0;

	//! Return true if Editor runs in the testing mode.
	virtual bool IsInTestMode() = 0;

	//! Return true if Editor runs in the preview mode.
	virtual bool IsInPreviewMode() = 0;

	//! Enable/Disable updates of editor.
	virtual void EnableUpdate( bool enable ) = 0;

	//! Enable/Disable accelerator table, (Enabled by default).
	virtual void EnableAcceleratos( bool bEnable ) = 0;

	virtual Version GetFileVersion() = 0;
	virtual Version GetProductVersion() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Game Engine.
	//////////////////////////////////////////////////////////////////////////
	/** Retrieve pointer to game engine instance.
	*/
	virtual CGameEngine* GetGameEngine() = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Display Settings.
	//////////////////////////////////////////////////////////////////////////
	virtual class CDisplaySettings*	GetDisplaySettings() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Object related methods.
	//////////////////////////////////////////////////////////////////////////
	//! Create new object.
	virtual CBaseObject* NewObject( const CString &type,const CString &file="" ) = 0;
	//! Delete object.
	virtual void				 DeleteObject( CBaseObject *obj ) = 0;
	//! Clone object.
	virtual CBaseObject* CloneObject( CBaseObject *obj ) = 0;
	//! Starts creation of new object.
	virtual void StartObjectCreation( const CString &type,const CString &file="" ) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Objects selection
	//////////////////////////////////////////////////////////////////////////
	//! Get curent Selection group.
	virtual CSelectionGroup*	GetSelection() = 0;
	virtual CBaseObject* GetSelectedObject() = 0;
	virtual int ClearSelection() = 0;
	//! Select object.
	virtual void SelectObject( CBaseObject *obj ) = 0;

	//! Lock current objects selection.
	//! While selection locked, other objects cannot be selected or unselected.
	virtual void LockSelection( bool bLock ) = 0;
	//! Check if selection is curently locked.
	virtual bool IsSelectionLocked() = 0;

	//! Get access to object manager.
	virtual struct IObjectManager* GetObjectManager() = 0;

	//! Set pick object mode.
	//! When object picked callback will be called, with OnPick.
	//! If pick operation is canceled Cancel will be called.
	//! @param targetClass specifies objects of which class are supposed to be picked.
	//! @param Multipick if true pick tool will pick multiple object.
	virtual void PickObject( IPickObjectCallback *callback,CRuntimeClass *targetClass=0,const char *statusText=0,bool bMultipick=false ) = 0;
	//! Cancel current pick operation.
	virtual void CancelPick() = 0;
	//! Return true if editor now in object picking mode.
	virtual bool IsPicking() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Access to various DataBase managers.
	//////////////////////////////////////////////////////////////////////////
	// Get DB manager that own items of specified type.
	virtual IDataBaseManager* GetDBItemManager( EDataBaseItemType itemType ) = 0;

	//! Get Entity prototype manager.
	virtual CEntityPrototypeManager* GetEntityProtManager() = 0;
	//! Get Manager of Materials.
	virtual CMaterialManager* GetMaterialManager() = 0;
	//! Returns IconManager.
	virtual CIconManager* GetIconManager() = 0;
	//! Returns Particle manager.
	virtual CParticleManager* GetParticleManager() = 0;
	//! Get Music Manager.
	virtual CMusicManager* GetMusicManager() = 0;
	//! Get Prefabs Manager.
	virtual CPrefabManager* GetPrefabManager() = 0;
	//! Get Game Tokens Manager.
	virtual CGameTokenManager* GetGameTokenManager() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Terrain related.
	//////////////////////////////////////////////////////////////////////////
	virtual float GetTerrainElevation( float x,float y ) = 0;
	virtual class CHeightmap* GetHeightmap() = 0;
	virtual class CVegetationMap* GetVegetationMap() = 0;

	//////////////////////////////////////////////////////////////////////////
	// AI Related.
	//////////////////////////////////////////////////////////////////////////
	virtual class CAIManager*	GetAI() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Access to IMovieSystem.
	//////////////////////////////////////////////////////////////////////////
	//! Movie system.
	virtual struct IMovieSystem* GetMovieSystem() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Access to CEquipPackLib.
	//////////////////////////////////////////////////////////////////////////
	//! CEquipPackLib.
	virtual class CEquipPackLib* GetEquipPackLib() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Plugins related methods.
	//////////////////////////////////////////////////////////////////////////
	//! Get access to plugin manager.
	virtual class CPluginManager* GetPluginManager() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Sound related methods.
	//////////////////////////////////////////////////////////////////////////

	//! Get access to EAXPresets manager.
	virtual class CEAXPresetMgr* GetEAXPresetMgr() = 0;

	//! Get access to SoundMood manager.
	virtual class CSoundMoodMgr* GetSoundMoodMgr() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Views related methods.
	//////////////////////////////////////////////////////////////////////////
	virtual class CViewManager* GetViewManager() = 0;

	virtual class CViewport* GetActiveView() = 0;

	//! Notify all views that data is changed.
	virtual void UpdateViews( int flags=0xFFFFFFFF,BBox *updateRegion=NULL ) = 0;
	virtual void ResetViews() = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Update information in track view dialog.
	virtual void UpdateTrackView( bool bOnlyKeys=false ) = 0;

	//! Current position marker
	virtual Vec3 GetMarkerPosition() = 0;
	//! Set current position marker.
	virtual void	SetMarkerPosition( const Vec3 &pos ) = 0;

	//! Set current selected region.
	virtual void	SetSelectedRegion( const BBox &box ) = 0;
	//! Get currently selected region.
	virtual void	GetSelectedRegion( BBox &box ) = 0;

	//////////////////////////////////////////////////////////////////////////
	// UI creation function
	//////////////////////////////////////////////////////////////////////////

	// Needs to be called ONCE before anything else is inserted into the plugin menu.
	// Only supposed to be called inside of IPlugin::CreateUIElements()
	virtual bool CreateRootMenuItem(const char *pszName) = 0;

	// Creates a new menu item in the specified parent menu. You need to call
	// CreateRootMenuItem() before you can use eMenuPlugin as eParent.
	// Only supposed to be called inside of IPlugin::CreateUIElements()
	virtual bool AddMenuItem(uint8 iId, bool bIsSeparator, 
		eMenuInsertLocation eParent, IUIEvent *pIHandler) = 0;

	// Select Current rollup bar.
	virtual int SelectRollUpBar( int rollupBarId ) = 0;

	// Insert a new MFC CDialog based page into the roll up bar
	virtual int AddRollUpPage( int rollbarId,LPCTSTR pszCaption, CDialog *pwndTemplate=0,
		bool bAutoDestroyTpl = true, int iIndex = -1,bool bAutoExpand=true ) = 0;

	// Remove a dialog page from the roll up bar
	virtual void RemoveRollUpPage(int rollbarId,int iIndex) = 0;

	// Expand one of the rollup pages
	virtual void ExpandRollUpPage(int rollbarId,int iIndex, BOOL bExpand = true) = 0;

	// Enable or disable one of the rollup pages
	virtual void EnableRollUpPage(int rollbarId,int iIndex, BOOL bEnable = true) = 0;
	
	// Get the window handle of the roll up page container. All CDialog classes
	// which are passed to InsertRollUpPage() need to have this handle as
	// the parent window
	virtual HWND GetRollUpContainerWnd(int rollbarId) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Editing modes.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetOperationMode( EOperationMode mode ) = 0;
	virtual EOperationMode GetOperationMode() = 0;

	// editMode - EEditMode
	virtual void SetEditMode( int editMode ) = 0;
	virtual int GetEditMode() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Edit tools.
	//////////////////////////////////////////////////////////////////////////
	//! Assign current edit tool, destroy previously used edit too.
	virtual void SetEditTool( CEditTool *tool,bool bStopCurrentTool=true ) = 0;
	//! Assign current edit tool by class name.
	virtual void SetEditTool( const CString &sEditToolName,bool bStopCurrentTool=true ) = 0;
	//! Returns current edit tool.
	virtual CEditTool* GetEditTool() = 0;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Transformation methods.
	//////////////////////////////////////////////////////////////////////////
	// Shows/Hides transformation manipulator.
	// if bShow is true also returns a valid ITransformManipulator pointer.
	virtual ITransformManipulator* ShowTransformManipulator( bool bShow ) = 0;
	// Return a pointer to a ITransformManipulator pointer if shown.
	// NULL is manipulator is not shown.
	virtual ITransformManipulator* GetTransformManipulator() = 0;
	
	//! Set constrain on specified axis for objects construction and modifications.
	//! @param axis one of AxisConstrains enumerations.
	virtual void SetAxisConstrains( AxisConstrains axis ) = 0;
	//! Get axis constrain for objects construction and modifications.
	virtual AxisConstrains GetAxisConstrains() = 0;

	//! If set, when axis terrain constrain is selected, snapping only to terrain.
	virtual void SetTerrainAxisIgnoreObjects( bool bIgnore ) = 0;
	virtual bool IsTerrainAxisIgnoreObjects() = 0;

	//! Set current reference coordinate system used when constructing/modifing objects.
	virtual void SetReferenceCoordSys( RefCoordSys refCoords ) = 0;
	//! Get current reference coordinate system used when constructing/modifing objects.
	virtual RefCoordSys GetReferenceCoordSys() = 0;

	//////////////////////////////////////////////////////////////////////////
	// XmlTemplates
	//////////////////////////////////////////////////////////////////////////
	virtual XmlNodeRef FindTemplate( const CString &templateName ) = 0;
	virtual void AddTemplate( const CString &templateName,XmlNodeRef &tmpl ) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Standart Dialogs.
	//////////////////////////////////////////////////////////////////////////
	//! Open database library and select specified item.
	//! If parameter is NULL current selection in material library does not change.
	virtual CBaseLibraryDialog* OpenDataBaseLibrary( EDataBaseItemType type,IDataBaseItem *pItem=NULL ) = 0;

	// Opens a new view by class name.
	virtual CWnd* OpenView( const char *sViewName ) = 0;
	
	// Returns a view by class name but only if it was created before
	virtual CWnd* FindView( const char *sViewName ) = 0;

	//! Opens standard color selection dialog.
	//! Initialized with the color specified in color parameter.
	//! Returns true if selection is made and false if selection is canceled.
	virtual bool SelectColor( COLORREF &color,CWnd *parent=0 ) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Installed Shaders enumerations.
	//////////////////////////////////////////////////////////////////////////
	//! Get shader enumerator.
	virtual class CShaderEnum* GetShaderEnum() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Undo
	//////////////////////////////////////////////////////////////////////////
	virtual class CUndoManager* GetUndoManager() = 0;

	//! Begin opretaion requiering Undo.
	//! Undo manager enters holding state.
	virtual void BeginUndo() = 0;
	//! Restore all undo objects registered since last BeginUndo call.
	//! @param bUndo if true all Undo object registered since BeginUpdate call up to this point will be undone.
	virtual void RestoreUndo( bool undo=true ) = 0;
	//! Accept changes and registers an undo object with the undo manager.
	//! This will allow the user to undo the operation.
	virtual void AcceptUndo( const CString &name ) = 0;
	//! Cancel changes and restore undo objects.
	virtual void CancelUndo() = 0;

	//! Normally this is NOT needed but in special cases this can be useful.
	//! This allows to group a set of Begin()/Accept() sequences to be undone in one operation.
	virtual void SuperBeginUndo() = 0;
	//! When a SuperBegin() used, this method is used to Accept.
	//! This leaves the undo database in its modified state and registers the IUndoObjects with the undo system. 
	//! This will allow the user to undo the operation.
	virtual void SuperAcceptUndo( const CString &name ) = 0;
	//! Cancel changes and restore undo objects.
	virtual void SuperCancelUndo() = 0;

	//! Suspend undo recording.
	virtual void SuspendUndo() = 0;
	//! Resume undo recording.
	virtual void ResumeUndo() = 0;
	// Undo last operation.
	virtual void Undo() = 0;
	//! Redo last undo.
	virtual void Redo() = 0;
	//! Check if undo information is recording now.
	virtual bool IsUndoRecording() = 0;
	//! Check if undo information is suspended now.
	virtual bool IsUndoSuspended() = 0;
	//! Put new undo object, must be called between Begin and Accept/Cancel methods.
	virtual void RecordUndo( struct IUndoObject *obj ) = 0;
	//! Completly flush all Undo and redo buffers.
	//! Must be done on level reloads or global Fetch operation.
	virtual void FlushUndo() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Animation related.
	//////////////////////////////////////////////////////////////////////////
	//! Retrieve current animation context.
	virtual CAnimationContext* GetAnimation() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Return External tools manager.
	//////////////////////////////////////////////////////////////////////////
	//! Returns external tools manager.
	virtual CExternalToolsManager* GetExternalToolsManager() = 0;

	//! Get global Error Report instance.
	virtual CErrorReport* GetErrorReport() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Editor Notify Events.
	//////////////////////////////////////////////////////////////////////////
	// Notify all listeners about the specified event.
	virtual void Notify( EEditorNotifyEvent event ) = 0;
	//! Register Editor notifications listener.
	virtual void RegisterNotifyListener( IEditorNotifyListener *listener ) = 0;
	//! Unregister Editor notifications listener.
	virtual void UnregisterNotifyListener( IEditorNotifyListener *listener ) = 0;
	//////////////////////////////////////////////////////////////////////////
	
	
	//////////////////////////////////////////////////////////////////////////
	// Listeners.
	//////////////////////////////////////////////////////////////////////////
		//! Register document notifications listener.
	virtual void RegisterDocListener( IDocListener *listener ) = 0;
	//! Unregister document notifications listener.
	virtual void UnregisterDocListener( IDocListener *listener ) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Description:
	//    Retrieve interface to the source control.
	virtual ISourceControl* GetSourceControl() = 0;

	// Retrieve pointer to the global flow graph manager.
	virtual CFlowGraphManager* GetFlowGraphManager() = 0;

	virtual CUIEnumsDatabase* GetUIEnumsDatabase() = 0;

	virtual void AddUIEnums() = 0;

	virtual void GetMemoryUsage( ICrySizer* pSizer )=0;
	virtual void ReduceMemory() = 0;

	virtual IFacialEditor* GetFacialEditor(bool openIfNecessary = true) = 0;

	// Set current configuration spec of the editor.
	virtual void SetEditorConfigSpec( ESystemConfigSpec spec ) = 0;
	virtual ESystemConfigSpec GetEditorConfigSpec() const = 0;
};

// Interface for instanciating the plugin from the editor
struct IPlugin
{
	virtual void Release() = 0;

	//! Show a modal about dialog / message box for the plugin
	virtual void ShowAbout() = 0;

	//! Return the GUID of the plugin
	virtual const char * GetPluginGUID() = 0;
	virtual DWORD GetPluginVersion() = 0;

	//! Return the human readable name of the plugin
	virtual const char * GetPluginName() = 0;

	//! Asks if the plugin can exit now. This might involve asking the user if he wants to save
	//! data. The plugin is only supposed to ask for unsaved data which is not serialize into
	//! the editor project file. When data is modified which is saved into the project file, the
	//! plugin should call IEditor::SetDataModified() to make the editor ask
	virtual bool CanExitNow() = 0;

	//! The plugin should write / read its data to the passed stream. The data is saved to or loaded
	//! from the editor project file. This function is called during the usual save / load process of
	//! the editor's project file
	virtual void Serialize(FILE *hFile, bool bIsStoring) = 0;

	//! Resets all content of the plugin, f.e. after the user created a new document
	virtual void ResetContent() = 0;

	//! Create all user interface elements. Calls to IEditor's UI element creation functios are 
	//! only valid during this function
	virtual bool CreateUIElements() = 0;

	//! Give the plugin the opportunity to exoport all its data to the game. This function is
	//! guaranteed to be called after the editor has finished its own exporting process
	virtual bool ExportDataToGame(const char * pszGamePath) = 0;
};

//! For use only inside editor executable.
extern IEditor* GetIEditor();

//! Undo utility class.
class CUndo
{
public:
	CUndo( const char *description )
	{
		GetIEditor()->BeginUndo();
		m_description = description;
	};
	~CUndo()
	{
		GetIEditor()->AcceptUndo(m_description);
	};
	/** Check if undo is recording.
	*/
	static bool IsRecording() { return GetIEditor()->IsUndoRecording(); };
	// Check if undo is suspended.
	static bool IsSuspended() { return GetIEditor()->IsUndoSuspended(); };
	/** Record specified object.
	*/
	static void Record( IUndoObject *undo ) { return GetIEditor()->RecordUndo( undo ); };
private:
	CString m_description;
};

/** CUndoSuspend is a utility undo class.
		Define instance of this class in block of code where you want to suspend undo operations.
*/
class CUndoSuspend
{
public:
	CUndoSuspend() { GetIEditor()->SuspendUndo(); };
	~CUndoSuspend() { GetIEditor()->ResumeUndo(); };
};

// Initialization structure
struct PLUGIN_INIT_PARAM
{
	IEditor * pIEditorInterface;
	struct IGame * pIGameInterface;
};

// Function pointer to be queried from the loaded DLL
typedef IPlugin * (* pfnCreatePluginInstance) (PLUGIN_INIT_PARAM *pInitParam);

//////////////////////////////////////////////////////////////////////////
// Callback used by editor when initializing for info in UI dialogs
//////////////////////////////////////////////////////////////////////////
struct IInitializeUIInfo
{
	virtual void SetInfoText( const char *text ) = 0;
};

// Factory API
extern "C"
{
	PLUGIN_API IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM *pInitParam);
}

#endif // AFX_PLUGIN_H__434E7C19_6FG4425_599FA_86ECA760280C__INCLUDED_