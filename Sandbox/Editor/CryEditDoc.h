// CryEditDoc.h : interface of the CCryEditDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_CRYEDITDOC_H__94C0E756_37F4_40A3_B8DF_67B09E8BA5A2__INCLUDED_)
#define AFX_CRYEDITDOC_H__94C0E756_37F4_40A3_B8DF_67B09E8BA5A2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Clouds.h"
#include "TerrainLighting.h"
#include "TextureCompression.h"
#include "Heightmap.h"
#include "VSCompStructures.h"

// forward references.
class CSurfaceType;
class CMission;
class CEntityScriptRegistry;
class CLayer;
class CLevelShaderCache;
struct IResourceList;

// Filename of the temporary file used for the hold / fetch
// operation
#define HOLD_FETCH_FILE "EditorHold.cry"

//////////////////////////////////////////////////////////////////////////
//
// CCryEditDoc.
//
//////////////////////////////////////////////////////////////////////////
class CCryEditDoc : public CDocument
{
protected: // Create from serialization only
	CCryEditDoc();
	DECLARE_DYNCREATE(CCryEditDoc)

// Attributes
public:

	// Heightmap
	CHeightmap m_cHeightmap;
	
	// Clouds
	CClouds m_cClouds;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCryEditDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CCryEditDoc();

	//////////////////////////////////////////////////////////////////////////
		//! Ovveridable from CDocument
	virtual void DeleteContents();
	virtual BOOL DoSave(LPCTSTR lpszPathName, BOOL bReplace);
	//////////////////////////////////////////////////////////////////////////

	const CString& GetLevelName() const { return m_level; };

	//! Save document.
	bool Save();
	void ChangeMission();
	
	// Serialize document data.
	void Load( CXmlArchive &xmlAr,const CString &szFilename,bool bReloadEngineLevel=true );
	void Save( CXmlArchive &xmlAr );
	bool SaveToFile( const CString &szFilename );
	bool LoadFromFile( const CString &szFilename,bool bReloadEngineLevel=true );
	void HoldToFile( const CString &szFilename );
	void FetchFromFile( const CString &szFilename );

	void SerializeFogSettings( CXmlArchive &xmlAr );
	void SerializeViewSettings( CXmlArchive &xmlAr );
	void SerializeSurfaceTypes( CXmlArchive &xmlAr, bool bUpdateEngineTerrain = true, bool bUpdateEngine = true );
	void SerializeMissions( CXmlArchive &xmlAr,CString &currentMission );
	void SerializeShaderCache( CXmlArchive &xmlAr );

	LightingSettings* GetLighting();

	CHeightmap&	GetHeightmap() { return m_cHeightmap; };

	void SetTerrainSize( int resolution,int unitSize );

	void SetWaterColor( COLORREF col ) { m_waterColor = col; };
	COLORREF GetWaterColor() { return m_waterColor; };

	// slow
	// Returns:
	//   0xffffffff if the method failed (internal error)
	uint32 GetDetailIdLayerFromLayerId( const uint32 dwLayerId );

	// slow
	// Returns:
	//   0xffffffff if the method failed (internal error)
	uint32 GetLayerIdFromDetailLayerId( const uint32 dwDetailLayerId );

	// needed to convert from old layer structure to the new one
	bool ConvertLayersToRGBLayer();

	// Debug
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	BOOL CanCloseFrame(CFrameWnd* pFrame);
	bool OnExportTerrainAsGeometrie(const char *pszFileName, RECT rcExport);
	void GetMemoryUsage( ICrySizer *pSizer );
	//////////////////////////////////////////////////////////////////////////
	// Surface Types.
	//////////////////////////////////////////////////////////////////////////
	// Returns:
	//   can be 0 if the id does not exit
	CSurfaceType* GetSurfaceTypePtr( int i ) const { if(i >= 0 && i<m_surfaceTypes.size())return m_surfaceTypes[i]; return 0; }
	int  GetSurfaceTypeCount() const { return m_surfaceTypes.size(); }
	//! Find surface type by name, return -1 if name not found.
	int FindSurfaceType( const CString &name );
	void RemoveSurfaceType( int i );
	int  AddSurfaceType( CSurfaceType *srf );
	void RemoveAllSurfaceTypes();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef&	GetFogTemplate() { return m_fogTemplate; };

	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef&	GetEnvironmentTemplate() { return m_environmentTemplate; };

	//////////////////////////////////////////////////////////////////////////
	// Multiple missions per Map support.
	//////////////////////////////////////////////////////////////////////////
	//! Return currently active Mission.
	CMission*	GetCurrentMission();

	//! Get number of missions on Map.
	int	GetMissionCount() const { return m_missions.size(); };
	//! Get Mission by index.
	CMission*	GetMission( int index ) const { return m_missions[index]; };
	//! Find Mission by name.
	CMission*	FindMission( const CString &name ) const;

	//! Makes specified mission current.
	void SetCurrentMission( CMission *mission );

	//! Add new mission to map.
	void AddMission( CMission *mission );
	//! Remove existing mission from map.
	void RemoveMission( CMission *mission );
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	void RegisterListener( IDocListener *listener );
	void UnregisterListener( IDocListener *listener );

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// Layers
	int GetLayerCount() const { return m_layers.size(); };
	CLayer* GetLayer( int layer ) const { return m_layers[layer]; };
	//! Find layer by name.
	CLayer* FindLayer( const char *sLayerName ) const;
	CLayer* FindLayerByLayerId( const uint32 dwLayerId ) const;
	void SwapLayers( int layer1,int layer2 );
	void AddLayer( CLayer *layer ) { m_layers.push_back(layer); };
	void RemoveLayer( int layer );
	void RemoveLayer( CLayer *layer );
	void InvalidateLayers();
	void ClearLayers();
	void SerializeLayerSettings( CXmlArchive &xmlAr );
	void MarkUsedLayerIds( bool bFree[256] ) const;

	//////////////////////////////////////////////////////////////////////////

	void LoadLightmaps();
	void LoadVertexScatter() const;
	void GenerateVertexScatter() const;
	void LogLoadTime( int time );

	//////////////////////////////////////////////////////////////////////////
	bool IsDocumentReady() const { return m_bDocumentReady; }
	void SetDocumentReady( bool bReady ) { m_bDocumentReady = bReady; };

	bool IsNewTerranTextureSystem() { return m_bIsNewTerranTextureSystem; }

	//! For saving binary data (voxel object)
	CXmlArchive * GetTmpXmlArch(){ return m_pTmpXmlArchHack;}

	//////////////////////////////////////////////////////////////////////////
	CLevelShaderCache* GetShaderCache() { return m_pLevelShaderCache; }


protected:
	virtual BOOL OnSaveDocument( LPCTSTR lpszPathName );

	void LoadTemplates();

	//! Called immidiatly after saving the level.
	void AfterSave();

	//! Clear all missions on map.
	void ClearMissions();
	void CollectScatteringRenderNodes(std::vector<struct IRenderNode*>& RenderNodes, std::vector<int>* ObjectsPerBlocks) const;

protected:
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	
	COLORREF m_waterColor;

	// Master CD
	CString m_strMasterCDFolder;
	std::vector<CSurfaceType*> m_surfaceTypes;

	// Level Templates.
	XmlNodeRef m_fogTemplate;
	XmlNodeRef m_environmentTemplate;

	bool m_loadFailed;

	//! Currently active mission.
	CMission*	m_mission;
	//! Name of level.
	CString m_level;
	
	// Collection of missions for this map.
	std::vector<CMission*> m_missions;

	// List of all document listeners.
	std::list<IDocListener*> m_listeners;

	std::vector<CLayer*>		m_layers;											// Terrain layers


	bool										m_bDocumentReady;
	bool										m_bIsNewTerranTextureSystem;	// true=terrain texture is stored in RGB, false=terrain texture is stored in layers

	CXmlArchive * m_pTmpXmlArchHack;

	//////////////////////////////////////////////////////////////////////////
	CLevelShaderCache *m_pLevelShaderCache;

	void RegisterConsoleVariables();

	static void OnValidateSurfaceTypesChanged(ICVar*);
	ICVar* doc_validate_surface_types;

protected: // ---------------------------------------------------------------

	// Generated message map functions
	//{{AFX_MSG(CCryEditDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	
};

class CAutoDocNotReady
{
public:
	CAutoDocNotReady()
	{
		GetIEditor()->GetDocument()->SetDocumentReady(false);
	}
	~CAutoDocNotReady()
	{
		GetIEditor()->GetDocument()->SetDocumentReady(true);
	}
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CRYEDITDOC_H__94C0E756_37F4_40A3_B8DF_67B09E8BA5A2__INCLUDED_)
