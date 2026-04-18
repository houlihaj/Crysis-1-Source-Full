////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   cryeditdoc.cpp
//  Version:     v1.00
//  Created:     31/3/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <ILMSerializationManager.h>
#include <IVertexScatterSerializationManager.h>
#include <IStreamEngine.h>
#include "CryEditDoc.h"

#include "MainFrm.h"
#include "OBJExporter1.h"
#include "PluginManager.h"
#include "DimensionsDialog.h"
#include "SurfaceType.h"
#include "Mission.h"
#include "Layer.h"
#include "equippacklib.h"
#include "StringDlg.h"
#include "VegetationMap.h"
#include "ViewManager.h"
#include "DisplaySettings.h"
#include "GameEngine.h"
#include "GameExporter.h"
#include "MissionSelectDialog.h"

#include "AnimationSerializer.h"

#include "TerrainTexGen.h"
#include "Util\FileUtil.h"
#include "Settings.h"

#include "Objects\ObjectManager.h"
#include "Objects\BaseObject.h"
#include "Objects\Entity.h"
#include "Objects\EntityScript.h"

#include "EntityPrototypeManager.h"
#include "Material\MaterialManager.h"
#include "Particles\ParticleManager.h"
#include "Music\MusicManager.h"
#include "Prefabs\PrefabManager.h"
#include "GameTokens\GameTokenManager.h"

#include <IAgent.h>
#include <IEntitySystem.h>
#include <ISound.h>
#include <IEntityRenderState.h>

#include "LightmapGen.h"
#include "ErrorReportDialog.h"
#include "Undo/Undo.h"
#include "SurfaceTypeValidator.h"

#include "ShaderCache.h"

//#define PROFILE_LOADING_WITH_VTUNE

// profilers api.
//#include "pure.h"
#ifdef PROFILE_LOADING_WITH_VTUNE
#include "C:\Program Files\Intel\Vtune\Analyzer\Include\VTuneApi.h"
#pragma comment(lib,"C:\\Program Files\\Intel\\Vtune\\Analyzer\\Lib\\VTuneApi.lib")
#endif

//////////////////////////////////////////////////////////////////////////
class CAutoLogTime
{
public:
	CAutoLogTime( const char *what )
	{
		m_what = what;
		CLogFile::FormatLine( "---- Start: %s",m_what );
		m_t0 = GetTickCount();
	}
	~CAutoLogTime()
	{
		m_t1 = GetTickCount();
		CLogFile::FormatLine( "---- End: %s (%d seconds)",m_what,(m_t1-m_t0)/1000 );
	}
private:
	const char *m_what;
	int m_t0,m_t1;
};

/////////////////////////////////////////////////////////////////////////////
// CCryEditDoc

IMPLEMENT_DYNCREATE(CCryEditDoc, CDocument)

BEGIN_MESSAGE_MAP(CCryEditDoc, CDocument)
	//{{AFX_MSG_MAP(CCryEditDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CCryEditDoc* theDocument = 0;

/////////////////////////////////////////////////////////////////////////////
// CCryEditDoc construction/destruction

CCryEditDoc::CCryEditDoc()
:	doc_validate_surface_types(0)
{
	////////////////////////////////////////////////////////////////////////
	// Set member variables to initial values
	////////////////////////////////////////////////////////////////////////
	theDocument = this;

	m_loadFailed = false;

 
	m_waterColor = RGB(0,0,255);
	
	m_fogTemplate = GetIEditor()->FindTemplate( "Fog" );

	m_environmentTemplate = GetIEditor()->FindTemplate( "Environment" );
	if (m_environmentTemplate)
	{
		m_fogTemplate = m_environmentTemplate->findChild( "Fog" );
	}
	else
	{
		m_environmentTemplate = CreateXmlNode( "Environment" );
	}
	m_pLevelShaderCache = new CLevelShaderCache;

	m_bDocumentReady = false;
	
	GetIEditor()->SetDocument(this);
	CLogFile::WriteLine("Document created");
	m_bIsNewTerranTextureSystem=true;

	m_pTmpXmlArchHack = 0;

	RegisterConsoleVariables();
}

CCryEditDoc::~CCryEditDoc()
{
	ClearMissions();
	ClearLayers();

	delete m_pLevelShaderCache;

	theDocument = 0;
	CLogFile::WriteLine("Document destroied");
}

bool CCryEditDoc::Save()
{
	return OnSaveDocument(GetPathName()) == TRUE;
}

void CCryEditDoc::ChangeMission()
{
	GetIEditor()->Notify( eNotify_OnMissionChange );
	// Notify listeners.
	for (std::list<IDocListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
		(*it)->OnMissionChange();
}

void CCryEditDoc::SetTerrainSize( int resolution,int unitSize )
{
	m_cHeightmap.Resize( resolution,resolution,unitSize );
}

void CCryEditDoc::DeleteContents()
{
	CAutoDocNotReady autoDocNotReady;

	GetIEditor()->Notify( eNotify_OnCloseScene );

	//////////////////////////////////////////////////////////////////////////
	// Clear all undo info.
	//////////////////////////////////////////////////////////////////////////
	GetIEditor()->FlushUndo();

	// Notify listeners.
	for (std::list<IDocListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
		(*it)->OnCloseDocument();

	////////////////////////////////////////////////////////////////////////
	// Layers
	////////////////////////////////////////////////////////////////////////
	ClearLayers();
	////////////////////////////////////////////////////////////////////////
	// Clouds
	////////////////////////////////////////////////////////////////////////
	m_cClouds.GetLastParam()->bValid = false;

	GetIEditor()->ResetViews();

	GetIEditor()->SetEditTool(0); // Turn off any active edit tools.

	//////////////////////////////////////////////////////////////////////////
	// Objects.
	//////////////////////////////////////////////////////////////////////////
	// Delete all objects from Object Manager.
	GetIEditor()->GetObjectManager()->DeleteAllObjects();
	GetIEditor()->GetObjectManager()->GetLayersManager()->ClearLayers();

	// GetIEditor()->GetEquipPackLib()->Reset();

	for (int i=0; i < m_surfaceTypes.size(); i++)
		delete m_surfaceTypes[i];
	m_surfaceTypes.clear();

	ClearMissions();

	static bool firstTime = true;
	if (!firstTime)
	{
		IRenderer *renderer = GetIEditor()->GetRenderer();
		//if (renderer)
			//renderer->FreeResources(FRR_SHADERS|FRR_TEXTURES|FRR_RESTORE);
	}
	firstTime = false;

	// Load scripts data..
	SetModifiedFlag(FALSE);

	// Close error reports if open.
	CErrorReportDialog::Close();
}

BOOL CCryEditDoc::OnNewDocument()
{
	////////////////////////////////////////////////////////////////////////
	// Reset the document to defaults and free all data
	////////////////////////////////////////////////////////////////////////
	if (!CDocument::OnNewDocument())
		return FALSE;

	GetIEditor()->Notify( eNotify_OnBeginNewScene );
	CLogFile::WriteLine("Preparing new document...");


	////////////////////////////////////////////////////////////////////////
	// Reset Heightmap
	////////////////////////////////////////////////////////////////////////
	m_cHeightmap.SetWaterLevel(16); // Default water level.
	m_cHeightmap.Resize(128, 128,m_cHeightmap.GetUnitSize() );

	////////////////////////////////////////////////////////////////////////
	// Reset the surface texture of the top render window
	////////////////////////////////////////////////////////////////////////

	//cleanup resources!
	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_RELOAD, 0, 0);
	if (GetIEditor())
	{
		/*
		////////////////////////////////////////////////////////////////////////
		// Initialize the source control
		////////////////////////////////////////////////////////////////////////
		
			m_pIDatabase.CreateInstance("CryVS.Database");
			m_pIDatabase->PutLocalPath(GetMasterCDFolder().GetBuffer(1));
		*/
		
		////////////////////////////////////////////////////////////////////////
		// Plugins
		////////////////////////////////////////////////////////////////////////
		
		GetIEditor()->GetPluginManager()->NewDocument();
	}

	//DeleteContents();
	
	//////////////////////////////////////////////////////////////////////////
	// Initialize defaults.
	//////////////////////////////////////////////////////////////////////////
	if (!GetIEditor()->IsInPreviewMode())
	{
		// Create default layer.
		CLayer *layer = new CLayer;
		AddLayer( layer );
		layer->SetLayerName( "Default" );
		layer->LoadTexture( Path::GetGameFolder()+"/Textures/Terrain/Default.dds" );
		layer->SetLayerId(0);

		// Create default surface type.
		CSurfaceType *sfType = new CSurfaceType;
		sfType->SetName( "Default" );
		sfType->SetDetailTexture( Path::GetGameFolder()+"/Textures/Terrain/Detail/Default.dds" );
		uint32 dwDetailLayerId = AddSurfaceType( sfType );

		sfType->SetMaterial("Materials/terrain/plains_grass_green");

		layer->SetSurfaceTypeId(dwDetailLayerId);

		// Make new mission.
		GetCurrentMission();
		GetIEditor()->GetGameEngine()->SetMissionName( GetCurrentMission()->GetName() );
		GetIEditor()->GetGameEngine()->ReloadEnvironment();
	}

	{
		// Notify listeners.
		std::list<IDocListener*> listeners = m_listeners;
		std::list<IDocListener*>::iterator it,next;
		for (it = listeners.begin(); it != listeners.end(); it = next)
		{
			next = it; next++;
			(*it)->OnNewDocument();
		}
	}
	GetIEditor()->Notify( eNotify_OnEndNewScene );

	SetModifiedFlag(FALSE);
	m_bIsNewTerranTextureSystem=true;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CCryEditDoc serialization

void CCryEditDoc::Serialize(CArchive& ar)
{
	unsigned char *pData = NULL, *pImageData = NULL;
	CString currentMissionName;

	CString szFilename = ar.GetFile()->GetFilePath();
	m_level = Path::GetFileName( szFilename );
		
	CXmlArchive xmlAr;

	xmlAr.bLoading = ar.IsLoading() == TRUE;

	CAutoDocNotReady autoDocNotReady;
		
	if (ar.IsStoring())
	{
		Save( xmlAr );

		CLogFile::WriteLine( "Writing binary data..." );
		_smart_ptr<IXmlStringData> pXmlStrData = xmlAr.root->getXMLData( 5000000 );
		ar << pXmlStrData->GetString();
		xmlAr.pNamedData->Serialize( ar );
		CLogFile::WriteString( "Done." );

		//////////////////////////////////////////////////////////////////////////
		AfterSave();

		//////////////////////////////////////////////////////////////////////////
		CLogFile::WriteLine("Level successfully saved");
	}
	else
	{
		int t0 = GetTickCount();

#ifdef PROFILE_LOADING_WITH_VTUNE
		VTResume();
#endif

		// Check for 0 file length.
		if (ar.GetFile()->GetLength() == 0)
		{
			AfxMessageBox( _T("ERROR: Bad Level cry file, size is 0 bytes" ) );
			return;
		}

		m_cHeightmap.m_TerrainRGBTexture.FreeData();
		
		CString str;

		ar >> str;
		xmlAr.pNamedData->Serialize( ar );

		XmlParser parser;
		xmlAr.root = parser.parseBuffer( str );
		if (!xmlAr.root)
		{
			Warning( "Loading of %s failed\r\nError parsing Level XML, look at log file for more information",(const char*)szFilename );
			m_loadFailed = true;
			return;
		}
		Load( xmlAr,szFilename );

//		m_bIsNewTerranTextureSystem = m_cHeightmap.m_TerrainRGBTexture.GetTextureHeight()!=0;

	}
}



//////////////////////////////////////////////////////////////////////////
bool CCryEditDoc::ConvertLayersToRGBLayer()
{
	bool bConvertNeeded = !GetIEditor()->GetHeightmap()->m_TerrainRGBTexture.IsAllocated();

	if(!bConvertNeeded)
		return false;

	std::vector<CLayer*>::iterator it;

	uint32 dwTexResolution=0;

	for(it=m_layers.begin();it!=m_layers.end();++it)
	{
		CLayer *pLayer=*it;

		dwTexResolution = max(dwTexResolution,(uint32)pLayer->GetMaskResolution());
/*
		if(pLayer->GetMask().IsValid())
		{ 
			dwTexResolution=pLayer->GetMask().GetWidth();
			bLayersStillHaveAMask=true;
			break; 
		}
*/
	}

	if(AfxMessageBox("Convert LayeredTerrainTexture to RGBTerrainTexture?",MB_YESNO,0)==IDNO)
		return false;

	if(AfxMessageBox("Double the resolution of the existing data?",MB_YESNO,0)==IDYES)
		dwTexResolution*=2;

	// extract RGB texture
	{
		CTerrainLayerTexGen texGen;

		texGen.Init(dwTexResolution);

		SSectorInfo si;

		GetIEditor()->GetHeightmap()->GetSectorsInfo( si );

		uint32 dwNumSectors = si.numSectors;

		uint32 dwSectorTexResolution = 512;
		uint32 dwSectorResolution = dwTexResolution/dwNumSectors; 
		uint32 dwNumTexSectors = dwTexResolution/dwSectorTexResolution; 

		// no error check because this is allocating only very few bytes
		m_cHeightmap.m_TerrainRGBTexture.AllocateTiles(dwNumTexSectors,dwNumTexSectors,dwSectorTexResolution);	// dwTexResolution x dwTexResolution;
	
		CImage sectorDiffuseImage;

		if (!sectorDiffuseImage.Allocate( dwSectorResolution,dwSectorResolution ))
			return false;

		int flags = ETTG_QUIET|ETTG_NO_TERRAIN_SHADOWS|ETTG_ABGR;

		for (int y = 0; y < dwNumSectors; y++)
			for (int x = 0; x < dwNumSectors; x++)
			{
				CPoint sector(x,y);

				CRect rect(0,0,sectorDiffuseImage.GetWidth(),sectorDiffuseImage.GetHeight());

				if(!texGen.GenerateSectorTexture( sector,rect,flags,sectorDiffuseImage ))
					return false;			// maybe this should be handled better

				// M.M. to take a look at the result of the calculation
/*				char str[80];
				sprintf(str,"c:\\temp\\tile%d_%d.bmp",x,y);
				CImageUtil::SaveBitmap(str,sectorDiffuseImage);
*/
				m_cHeightmap.m_TerrainRGBTexture.SetSubImageRGBLayer(x*dwSectorResolution,y*dwSectorResolution,sectorDiffuseImage);
			}
	}

	// extract Detail LayerId info
	GetIEditor()->GetHeightmap()->CalcSurfaceTypes();
/*
	// M.M. to take a look at the result of the calculation
	uint32 dwExtend = m_cHeightmap.m_TerrainRGBTexture.CalcMinRequiredTextureExtend();
	CImage img;
	img.Allocate(dwExtend,dwExtend);
	m_cHeightmap.m_TerrainRGBTexture.GetSubImageStretched(0,0,1,1,img);
	CImageUtil::SaveBitmap("C:\\temp\\res.bmp",img);
*/
	// we no longer need the layer mask data
	for(it=m_layers.begin();it!=m_layers.end();++it)
	{
		CLayer *pLayer=*it;

		pLayer->ReleaseMask();
	}

	GetIEditor()->GetHeightmap()->UpdateEngineTerrain(false);

	m_bIsNewTerranTextureSystem=true;

	return true;
}


//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::Save( CXmlArchive& xmlAr )
{
	m_pTmpXmlArchHack = &xmlAr;

	CAutoDocNotReady autoDocNotReady;

	CString currentMissionName;

	//////////////////////////////////////////////////////////////////////////
	// Create root xml node.
	xmlAr.root = CreateXmlNode("Level");
	xmlAr.root->setAttr( "HeightmapWidth",m_cHeightmap.GetWidth() );
	xmlAr.root->setAttr( "HeightmapHeight",m_cHeightmap.GetHeight() );
	xmlAr.root->setAttr( "WaterColor",m_waterColor );
	xmlAr.root->setAttr( "SandboxVersion",(const char*)GetIEditor()->GetFileVersion().ToFullString() );

	SerializeViewSettings( xmlAr );

	// Cloud parameters ////////////////////////////////////////////////////

	//m_cClouds.Serialize(ar);
	m_cClouds.Serialize( xmlAr );

	// Heightmap ///////////////////////////////////////////////////////////

	m_cHeightmap.Serialize( xmlAr,true,false );
	//m_cHeightmap.Serialize(ar);
	
	// save terrain heightmap data
	bool bTerrainRefreshRequested=false;
	m_cHeightmap.SerializeTerrain( xmlAr, bTerrainRefreshRequested );

	// Terrain texture /////////////////////////////////////////////////////

	// Surface Types ///////////////////////////////////////////////////////
	SerializeSurfaceTypes( xmlAr );

	// Layers ///////////////////////////////////////////////////////
	SerializeLayerSettings( xmlAr );

	// Fog settings  ///////////////////////////////////////////////////////
	SerializeFogSettings( xmlAr );

	// Serialize Missions //////////////////////////////////////////////////
	SerializeMissions( xmlAr,currentMissionName );

	// Save objects.
	//Timur[9/11/2002]	GetIEditor()->GetObjectManager()->Serialize( xmlAr.root,xmlAr.bLoading,SERIALIZE_ONLY_SHARED );

	// Save EquipPacks.
	// GetIEditor()->GetEquipPackLib()->Serialize(xmlAr.root, xmlAr.bLoading);

	//! Serialize entity prototype manager.
	GetIEditor()->GetEntityProtManager()->Serialize( xmlAr.root, xmlAr.bLoading );

	//! Serialize prefabs manager.
	GetIEditor()->GetPrefabManager()->Serialize( xmlAr.root, xmlAr.bLoading );

	//! Serialize material manager.
	GetIEditor()->GetMaterialManager()->Serialize( xmlAr.root, xmlAr.bLoading );

	//! Serialize particles manager.
	GetIEditor()->GetParticleManager()->Serialize( xmlAr.root, xmlAr.bLoading );

	//! Serialize music manager.
	GetIEditor()->GetMusicManager()->Serialize( xmlAr.root, xmlAr.bLoading );

	//! Serialize game tokens manager.
	GetIEditor()->GetGameTokenManager()->Serialize( xmlAr.root, xmlAr.bLoading );

	CLightmapGen::Serialize(xmlAr);

	//////////////////////////////////////////////////////////////////////////
	// Serialize Shader Cache.
	SerializeShaderCache( xmlAr );

	//////////////////////////////////////////////////////////////////////////
	AfterSave();

	//////////////////////////////////////////////////////////////////////////
	m_pTmpXmlArchHack = 0;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::Load( CXmlArchive& xmlAr,const CString &szFilename,bool bReloadEngineLevel )
{
	m_pTmpXmlArchHack = &xmlAr;
	CAutoDocNotReady autoDocNotReady;

	HEAP_CHECK

	CLogFile::FormatLine("Loading from %s...", (const char*)szFilename );
	CString currentMissionName;
	CString szLevelPath = Path::GetPath(szFilename);
	m_level = Path::GetFileName( szLevelPath );
	
	{
		// Set game g_levelname variable to the name of current level.
		CString szGameLevelName = Path::GetFileName(szFilename);
		ICVar *sv_map = gEnv->pConsole->GetCVar("sv_map");
		if (sv_map)
			sv_map->Set( (const char*)szGameLevelName );
	}

//	gEnv->pCryPak->GetRecorderdResourceList(ICryPak::RFOM_Level)->Clear();
//	gEnv->pCryPak->RecordFileOpen(ICryPak::RFOM_Level);

	GetIEditor()->Notify( eNotify_OnBeginSceneOpen );

	AutoSuspendTimeQuota AutoSuspender(GetIEditor()->GetSystem()->GetStreamEngine());
	// Start recording errors.
	CErrorsRecorder errorsRecorder;

	int t0 = GetTickCount();

#ifdef PROFILE_LOADING_WITH_VTUNE
	VTResume();
#endif

	//////////////////////////////////////////////////////////////////////////
	// Mute sound during loading.
	if (gEnv->pSoundSystem) gEnv->pSoundSystem->Mute(true);
	//////////////////////////////////////////////////////////////////////////

	HEAP_CHECK

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// Serialize Missions //////////////////////////////////////////////////
	SerializeMissions( xmlAr,currentMissionName );

	// If multiple missions, select specific mission to load.
	if (GetMissionCount() > 1)
	{
		CMissionSelectDialog dlg;
		if (dlg.DoModal() == IDOK)
		{
			currentMissionName = dlg.GetSelected();
		}
	}

	HEAP_CHECK

	m_cHeightmap.Serialize(xmlAr,false,false);

	if (bReloadEngineLevel)
	{
		CAutoLogTime logtime( "3D Engine level load" );
		GetIEditor()->GetGameEngine()->LoadLevel( szLevelPath,currentMissionName,true,true,false );
	}

	//////////////////////////////////////////////////////////////////////////
	// Load terrain heightmap data
	//////////////////////////////////////////////////////////////////////////

	bool bTerrainRefreshRequested=false;

	// Surface Types ///////////////////////////////////////////////////////

	{
		CAutoLogTime logtime( "Loading Surface Types" );
		SerializeSurfaceTypes( xmlAr );
	}	

	// Terrain texture /////////////////////////////////////////////////////
	{
		CAutoLogTime logtime( "Loading Terrain Layers Info" );
		SerializeLayerSettings(xmlAr);
	}

	{
		CAutoLogTime logtime( "Load Terrain" );
		m_cHeightmap.SerializeTerrain( xmlAr, bTerrainRefreshRequested );
	}

	{
		CAutoLogTime logtime( "Reposition Vegetation" );
		CVegetationMap *vegMap = GetIEditor()->GetVegetationMap();
		if (vegMap)
			vegMap->PlaceObjectsOnTerrain();
	}

	// Load water color.
	xmlAr.root->getAttr( "WaterColor",m_waterColor );


	//////////////////////////////////////////////////////////////////////////
	// Load materials.
	//////////////////////////////////////////////////////////////////////////
	{
		CAutoLogTime logtime( "Load MaterialManager" );
		GetIEditor()->GetMaterialManager()->Serialize( xmlAr.root, xmlAr.bLoading );
	}

	//////////////////////////////////////////////////////////////////////////
	// Load Particles.
	//////////////////////////////////////////////////////////////////////////
	{
		CAutoLogTime logtime( "Load Particles" );
		GetIEditor()->GetParticleManager()->Serialize( xmlAr.root, xmlAr.bLoading );
	}

	//////////////////////////////////////////////////////////////////////////
	// Load MusicManager.
	//////////////////////////////////////////////////////////////////////////
	{
		CAutoLogTime logtime( "Load Music" );
		GetIEditor()->GetMusicManager()->Serialize( xmlAr.root, xmlAr.bLoading );
	}

	//////////////////////////////////////////////////////////////////////////
	// Load GameTokensManager.
	//////////////////////////////////////////////////////////////////////////
	{
		CAutoLogTime logtime( "Load GameTokens" );
		GetIEditor()->GetGameTokenManager()->Serialize( xmlAr.root, xmlAr.bLoading );
	}

	//////////////////////////////////////////////////////////////////////////
	// Load View Settings
	//////////////////////////////////////////////////////////////////////////
	SerializeViewSettings(xmlAr);

	// Cloud parameters ////////////////////////////////////////////////////

	//m_cClouds.Serialize(ar);

	// Heightmap ///////////////////////////////////////////////////////////
	//m_cHeightmap.Serialize(ar);
	{
		CAutoLogTime logtime( "Load Vegetation" );
		m_cHeightmap.SerializeVegetation(xmlAr);
	}

	// update surf types because layers info only now is available in vegetation groups
	{
		CAutoLogTime logtime( "Loading Surface Types" );
		SerializeSurfaceTypes( xmlAr, false );
	}	

	//////////////////////////////////////////////////////////////////////////
	// Fog settings  ///////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	SerializeFogSettings(xmlAr);

	{
		CAutoLogTime logtime( "Terrain Postprocess" );
		m_cHeightmap.ProcessAfterLoading();		// after loading layers and surfacetypes
	}

	// Load objects ////////////////////////////////////////////////////////
	//GetIEditor()->GetObjectManager()->Serialize( xmlAr.root,xmlAr.bLoading,SERIALIZE_ONLY_SHARED );

	// Load EquipPacks.
#if 0 // equipment packs are no longer per-level but in global libs
	{
		CAutoLogTime logtime( "Load Equipment Packs" );
		GetIEditor()->GetEquipPackLib()->Serialize(xmlAr.root, xmlAr.bLoading);
	}
#endif

	//! Serialize entity prototype manager.
	if(gEnv->pGame)
	{
		CAutoLogTime logtime( "Load Entity Archetypes Database" );
		GetIEditor()->GetEntityProtManager()->Serialize( xmlAr.root, xmlAr.bLoading );
	}

	//! Serialize prefabs manager.
	{
		CAutoLogTime logtime( "Load Prefabs Database" );
		GetIEditor()->GetPrefabManager()->Serialize( xmlAr.root, xmlAr.bLoading );
	}

	{
		CString str;
		str.Format( "Activating Mission %s",(const char*)currentMissionName );
		
		CAutoLogTime logtime( str );
		// Select current mission.
		m_mission = FindMission(currentMissionName);
		if (m_mission)
		{
			GetCurrentMission()->SyncContent( true,false );
		} else {
			GetCurrentMission();
		}
	}

  ICVar * p_e_ram_maps = gEnv->pConsole->GetCVar("e_ram_maps");
  if(p_e_ram_maps && p_e_ram_maps->GetIVal())
	{
		CAutoLogTime logtime( "Load RAM maps" );
		CLightmapGen::Serialize(xmlAr);
		LoadLightmaps();
	}

	//! Load vertex scattering
	{
		CAutoLogTime logtime( "Loading Vertex Scattering" );
		LoadVertexScatter();
	}

	//////////////////////////////////////////////////////////////////////////
	// Serialize Shader Cache.
	{
		CAutoLogTime logtime( "Load Level Shader Cache" );
		SerializeShaderCache( xmlAr );
	}

	{
		CAutoLogTime logtime( "Process RGB Terrain Layers" );
		ConvertLayersToRGBLayer();
	}

	{
		// support old version of sequences
		IMovieSystem * pMs = GetIEditor()->GetMovieSystem();
		if(pMs)
		{
			ISequenceIt * It = pMs->GetSequences();
			IAnimSequence *seq=It->first();;
			while (seq)
			{
				CBaseObject * pObj = GetIEditor()->GetObjectManager()->FindObject(seq->GetName());
				if(!pObj)
				{
					pObj = GetIEditor()->GetObjectManager()->NewObject("SequenceObject", 0, seq->GetName());
				}
				seq=It->next();
			}
			It->Release();
		}
	}

	{
		CAutoLogTime logtime( "Post Load" );
		// Notify listeners.
		for (std::list<IDocListener*>::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
			(*it)->OnLoadDocument();

		GetIEditor()->Notify( eNotify_OnEndSceneOpen );

		if (m_pLevelShaderCache && gEnv->pGame)
			m_pLevelShaderCache->ActivateShaders();
	}

	CSurfaceTypeValidator().Validate();

	//////////////////////////////////////////////////////////////////////////
	// UnMute sound after loading.
	if (gEnv->pSoundSystem) gEnv->pSoundSystem->Mute(false);
	//////////////////////////////////////////////////////////////////////////

#ifdef PROFILE_LOADING_WITH_VTUNE
	VTPause();
#endif

	int t1 = GetTickCount();
	LogLoadTime( t1-t0 );

	m_pTmpXmlArchHack = 0;


}

// TODO: Move into LightmapGen.cpp
#include "I3DEngine.h"
#include "LMCompStructures.h"
#include "Objects\ObjectManager.h"
#include "Objects\BrushObject.h"
#include "Objects\VoxelObject.h"
void CCryEditDoc::LoadLightmaps()
{
	////////////////////////////////////////////////////////////////////////
	// Restore lightmaps for all entities
	////////////////////////////////////////////////////////////////////////

	I3DEngine *pI3DEngine = GetIEditor()->GetSystem()->GetI3DEngine();
	ILMSerializationManager *pISerializationManager = pI3DEngine->CreateLMSerializationManager();
	std::vector<IRenderNode *> vGLMs;
	std::vector<CBaseObject*> vObjects;
	IRenderNode *pIEtyRender;
	UINT i;

	vGLMs.clear();

	GetIEditor()->GetObjectManager()->GetObjects(vObjects);
	for (i=0; i<vObjects.size(); i++)
	{
		switch( vObjects[i]->GetType() )
		{
			case OBJTYPE_BRUSH:
				{
					const CBrushObject *pBrushObj = (CBrushObject *) vObjects[i];
					pIEtyRender = pBrushObj->GetEngineNode();
				}
				break;
			case OBJTYPE_VOXEL:
				{
					const CVoxelObject *pVoxelObj = (CVoxelObject *) vObjects[i];
					pIEtyRender = pVoxelObj->GetEngineNode();
				}
				break;
			default:
				continue;
		}

		if (pIEtyRender )
			vGLMs.push_back(pIEtyRender);
	}

	if( 0 == vGLMs.size() )
		pISerializationManager->ApplyLightmapfile(pI3DEngine->GetLevelFilePath(LM_EXPORT_FILE_NAME), NULL, 0 );
	else
	{
		pISerializationManager->ApplyLightmapfile(pI3DEngine->GetLevelFilePath(LM_EXPORT_FILE_NAME), &vGLMs.front(), vGLMs.size() );
		pISerializationManager->LoadFalseLight(pI3DEngine->GetLevelFilePath(LM_FALSE_LIGHTS_EXPORT_FILE_NAME), &vGLMs.front(), vGLMs.size() );
	}

	pISerializationManager->Release();
	pISerializationManager = NULL;
}

static bool CompareRenderNodes(const struct IRenderNode* rendernode1, const struct IRenderNode* rendernode2)
{
	Vec3 pos1 = rendernode1->GetPos();
	Vec3 pos2 = rendernode2->GetPos();

	if (pos1.x != pos2.x)
		return pos1.x < pos2.x;
	else if (pos1.y != pos2.y)
		return pos1.y < pos2.y;
	else
		return pos1.z < pos2.z;
}

#define AVERAGE_REFRACTION_FOR_OBJECT_BLOCK

static struct IRenderNode* GetScatteringRenderNode(CBaseObject* object)
{
	struct IRenderNode* rendernode = NULL;

	switch (object->GetType())
	{
	case OBJTYPE_BRUSH:
		{
			CBrushObject *pBrushObj = static_cast<CBrushObject*>(object);
			rendernode = pBrushObj->GetEngineNode();
		}
		break;
	case OBJTYPE_VOXEL:
		{
			CVoxelObject *pVoxelObj = static_cast<CVoxelObject*>(object);
			rendernode = pVoxelObj->GetEngineNode();
		}
		break;
	case OBJTYPE_ENTITY:
		{
			CEntity *pEntity = static_cast<CEntity*>(object);
			IEntity *pIEntity = pEntity->GetIEntity();
			if (pIEntity)
			{
				IEntityProxy *pIEntityProxy = pIEntity->GetProxy(ENTITY_PROXY_RENDER);
				if (pIEntityProxy)
					rendernode = static_cast<IEntityRenderProxy*>(pIEntityProxy)->GetRenderNode();
			}
		}
		break;
	}

	if (NULL == rendernode || !(rendernode->GetRndFlags() & ERF_SUBSURFSCATTER))
			return NULL;

		return rendernode;
	}

void CCryEditDoc::CollectScatteringRenderNodes(std::vector<struct IRenderNode*>& RenderNodes, std::vector<int>* ObjectsPerBlocks) const
{
	assert(0 == RenderNodes.size());

	std::vector<CBaseObject*> vObjects;
	GetIEditor()->GetObjectManager()->GetObjects(vObjects);

	bool* withdrawn = static_cast<bool*>(alloca(sizeof(bool)*vObjects.size()));
	memset(withdrawn, 0, sizeof(bool)*vObjects.size());

	for (int i=0; i<vObjects.size(); ++i)
	{
		if (withdrawn[i])
			continue;

		struct IRenderNode *pActRenderNode = GetScatteringRenderNode(vObjects[i]);
		if (pActRenderNode)
		{
			RenderNodes.push_back(pActRenderNode);
			int objects_per_block = 1;
			Vec3 actpos = pActRenderNode->GetPos();
			for (int j=i+1; j<vObjects.size(); ++j)
			{
				if (withdrawn[j])
					continue;

				struct IRenderNode *pRenderNode = GetScatteringRenderNode(vObjects[j]);
				if (pRenderNode)
				{
					Vec3 pos = pRenderNode->GetPos();
					if (0.001f > (pos - actpos).len2())
					{
						RenderNodes.push_back(pRenderNode);
						++objects_per_block;
						withdrawn[j] = true;
					}
				}
				else
					withdrawn[j] = true;
			}
			if (ObjectsPerBlocks)
				ObjectsPerBlocks->push_back(objects_per_block);
		}
	}
}

void CCryEditDoc::LoadVertexScatter() const
{
	std::vector<struct IRenderNode*> RenderNodes;
	CollectScatteringRenderNodes(RenderNodes, NULL);

	if (0 != RenderNodes.size())
	{
		I3DEngine *pI3DEngine = GetIEditor()->GetSystem()->GetI3DEngine();
		IVertexScatterSerializationManager *pISerializationManager = pI3DEngine->CreateVertexScatterSerializationManager();

		pISerializationManager->ApplyVertexScatterFile(pI3DEngine->GetLevelFilePath(VS_EXPORT_FILE_NAME), &*RenderNodes.begin(), RenderNodes.size());

		pISerializationManager->Release();
	}
}

void CCryEditDoc::GenerateVertexScatter() const
{
	std::vector<struct IRenderNode*> RenderNodes;
	std::vector<int> objects_per_blocks;
	CollectScatteringRenderNodes(RenderNodes, &objects_per_blocks);

	if (0 != RenderNodes.size())
	{
		CWaitProgress* generation_progress = new CWaitProgress( _T("Generating Vertex Scattering") );

		I3DEngine *pI3DEngine = GetIEditor()->Get3DEngine();
		IRenderer *pIRenderer = GetIEditor()->GetRenderer();
		IVertexScatterSerializationManager *pISerializationManager = pI3DEngine->CreateVertexScatterSerializationManager();

		pIRenderer->MarkForDepthMapCapture();
		int block_index = 0;

		ICVar* lod_max_cvar = gEnv->pConsole->GetCVar("e_lod_max");
		int lod_max = lod_max_cvar->GetIVal();
		lod_max_cvar->Set(0);

		ICVar* shadows_cvar = gEnv->pConsole->GetCVar("e_shadows");
		int shadows = shadows_cvar->GetIVal();
		shadows_cvar->Set(1);

		pISerializationManager->CollectDataForVertexScatterGeneration(&RenderNodes[0], RenderNodes.size());
		for (int i=0; i<objects_per_blocks.size(); ++i)
		{
			generation_progress->Step((i+1) * 100 / objects_per_blocks.size());
			pI3DEngine->DetermineBoundingBoxForScattering(&RenderNodes[block_index], objects_per_blocks[i]);
			pISerializationManager->CollectObjectArrayForVertexScatterGeneration(&RenderNodes[block_index], objects_per_blocks[i]);

			pI3DEngine->RenderWorld(SHDF_SORT, GetIEditor()->GetSystem()->GetViewCamera(),"CCryEditDoc::GenerateVertexScatter");

			pISerializationManager->DeleteObjectArray();
			block_index += objects_per_blocks[i];
		}
		pISerializationManager->SetVertexScattering(&RenderNodes[0], RenderNodes.size());

		lod_max_cvar->Set(lod_max);
		shadows_cvar->Set(shadows);

		pISerializationManager->SaveVertexScatter(pI3DEngine->GetLevelFilePath(VS_EXPORT_FILE_NAME));
		pIRenderer->UnmarkForDepthMapCapture();
		pISerializationManager->Release();

		delete generation_progress;
	}
}

void CCryEditDoc::SerializeLayerSettings( CXmlArchive &xmlAr )
{
	////////////////////////////////////////////////////////////////////////
	// Load or restore the layer settings from an XML
	////////////////////////////////////////////////////////////////////////
	if (xmlAr.bLoading)
	{
		// Loading

		CLogFile::WriteLine("Loading layer settings...");
		CWaitProgress progress( _T("Loading Terrain Layers") );

		// Clear old layers
		ClearLayers();

		// Load the layer count
		XmlNodeRef layers = xmlAr.root->findChild( "Layers" );
		if (!layers)
			return;
		
		// Read all layers
		int numLayers = layers->getChildCount();
		for (int i = 0; i < numLayers; i++)
		{
			progress.Step( 100*i/numLayers );
			// Create a new layer
			AddLayer( new CLayer );

			CXmlArchive ar( xmlAr );
			ar.root = layers->getChild(i);
			// Fill the layer with the data
			GetLayer(i)->Serialize( ar );

			CryLog("  loaded editor layer %d  name='%s' LayerID=%d",i,GetLayer(i)->GetLayerName(),GetLayer(i)->GetCurrentLayerId());
		}
	}
	else
	{
		// Storing

		CLogFile::WriteLine("Storing layer settings...");

		// Save the layer count

		XmlNodeRef layers = xmlAr.root->newChild( "Layers" );

		// Write all layers
		for (int i=0; i < GetLayerCount(); i++)
		{
			CXmlArchive ar( xmlAr );
			ar.root = layers->newChild( "Layer" );;
			GetLayer(i)->Serialize( ar );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::AfterSave()
{
	//////////////////////////////////////////////////////////////////////////
	// When saving level also save editor settings.
	//////////////////////////////////////////////////////////////////////////

	// Save settings.
	gSettings.Save();
	GetIEditor()->GetDisplaySettings()->SaveRegistry();
	((CMainFrame*)AfxGetMainWnd())->SaveConfig();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::SerializeViewSettings( CXmlArchive &xmlAr )
{
	////////////////////////////////////////////////////////////////////////
	// Load or restore the viewer settings from an XML
	////////////////////////////////////////////////////////////////////////
	if (xmlAr.bLoading)
	{
		// Loading
		CLogFile::WriteLine("Loading Fog settings...");

		XmlNodeRef view = xmlAr.root->findChild( "View" );
		if (!view)
			return;

		Vec3 vp(ZERO);
		Ang3 va(ZERO);
		view->getAttr( "ViewerPos",vp );
		view->getAttr( "ViewerAngles",va );

		CViewport *pVP = GetIEditor()->GetViewManager()->GetGameViewport();
		if (pVP)
		{
			Matrix34 tm = Matrix34::CreateRotationXYZ(va);
			tm.SetTranslation(vp);
			pVP->SetViewTM(tm);
		}

		// Load grid.
		XmlNodeRef gridNode = xmlAr.root->findChild( "Grid" );
		if (gridNode)
		{
			GetIEditor()->GetViewManager()->GetGrid()->Serialize( gridNode,xmlAr.bLoading );
		}
	}
	else
	{
		// Storing
		CLogFile::WriteLine("Storing Fog settings...");

		XmlNodeRef view = xmlAr.root->newChild( "View" );

		CViewport *pVP = GetIEditor()->GetViewManager()->GetGameViewport();
		if (pVP)
		{
			Vec3 pos = pVP->GetViewTM().GetTranslation();
			Ang3 angles = Ang3::GetAnglesXYZ( Matrix33(pVP->GetViewTM()) );
			view->setAttr( "ViewerPos",pos );
			view->setAttr( "ViewerAngles",angles );
		}

		// Save grid.
		XmlNodeRef gridNode = xmlAr.root->newChild( "Grid" );
		GetIEditor()->GetViewManager()->GetGrid()->Serialize( gridNode,xmlAr.bLoading );
	}
}

void CCryEditDoc::SerializeFogSettings( CXmlArchive &xmlAr )
{
	////////////////////////////////////////////////////////////////////////
	// Load or restore the layer settings from an XML
	////////////////////////////////////////////////////////////////////////
	if (xmlAr.bLoading)
	{
		// Loading
		CLogFile::WriteLine("Loading Fog settings...");

		XmlNodeRef fog = xmlAr.root->findChild( "Fog" );
		if (!fog)
			return;

		/*
		int mode = 0;
		int rgbColor;

		fog->getAttr( "Color",rgbColor );
		fog->getAttr( "Mode",mode );
		fog->getAttr( "Distance",m_sFogSettings.iFogDistance );
		fog->getAttr( "Density",m_sFogSettings.iFogDensity );
		fog->getAttr( "ViewDistance",m_sFogSettings.iViewDist );

		m_sFogSettings.eFogMode = (FogMode)mode;
		m_sFogSettings.rgbFogColor.rgbtRed = GetRValue(rgbColor);
		m_sFogSettings.rgbFogColor.rgbtGreen = GetGValue(rgbColor);
		m_sFogSettings.rgbFogColor.rgbtBlue = GetBValue(rgbColor);
		*/

		if (m_fogTemplate)
		{
			CXmlTemplate::GetValues( m_fogTemplate,fog );
		}
	}
	else
	{
		// Storing
		CLogFile::WriteLine("Storing Fog settings...");

		XmlNodeRef fog = xmlAr.root->newChild( "Fog" );

		/*
		int rgbColor = RGB( m_sFogSettings.rgbFogColor.rgbtRed,m_sFogSettings.rgbFogColor.rgbtGreen,m_sFogSettings.rgbFogColor.rgbtBlue );
		
		fog->setAttr( "Color",rgbColor );
		fog->setAttr( "Mode",(int)m_sFogSettings.eFogMode );
		fog->setAttr( "Distance",m_sFogSettings.iFogDistance );
		fog->setAttr( "Density",m_sFogSettings.iFogDensity );
		fog->setAttr( "ViewDistance",m_sFogSettings.iViewDist );
		*/

		if (m_fogTemplate)
		{
			CXmlTemplate::SetValues( m_fogTemplate,fog );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::SerializeSurfaceTypes( CXmlArchive &xmlAr, bool bUpdateEngineTerrain, bool bUpdateEngine )
{
	if (xmlAr.bLoading)
	{
		// Clear old layers
		RemoveAllSurfaceTypes();

		// Load the layer count
		XmlNodeRef node = xmlAr.root->findChild( "SurfaceTypes" );
		if (!node)
			return;

		// Read all node
		for (int i=0; i < node->getChildCount(); i++)
		{
			CXmlArchive ar( xmlAr );
			ar.root = node->getChild(i);
			CSurfaceType *sf = new CSurfaceType;
			sf->Serialize( ar );
			AddSurfaceType( sf );
		}
		if (bUpdateEngine) GetIEditor()->GetGameEngine()->ReloadSurfaceTypes(bUpdateEngineTerrain);
	}
	else
	{
		// Storing
		CLogFile::WriteLine("Storing surface types...");

		// Save the layer count

		XmlNodeRef node = xmlAr.root->newChild( "SurfaceTypes" );

		// Write all surface types.
		for (int i = 0; i < m_surfaceTypes.size(); i++)
		{
			CXmlArchive ar( xmlAr );
			ar.root = node->newChild( "SurfaceType" );
			m_surfaceTypes[i]->Serialize( ar );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::SerializeMissions( CXmlArchive &xmlAr,CString &currentMissionName )
{
	if (xmlAr.bLoading)
	{
		// Loading
		CLogFile::WriteLine("Loading missions...");

		// Clear old layers
		ClearMissions();

		// Load shared objects and layers.
		XmlNodeRef objectsNode = xmlAr.root->findChild( "Objects" );
		XmlNodeRef objectLayersNode = xmlAr.root->findChild( "ObjectLayers" );

		// Load the layer count
		XmlNodeRef node = xmlAr.root->findChild( "Missions" );
		if (!node)
			return;

		CString current;
		node->getAttr( "Current",current );
		currentMissionName = current;

		// Read all node
		for (int i=0; i < node->getChildCount(); i++)
		{
			CXmlArchive ar( xmlAr );
			ar.root = node->getChild(i);
			CMission *mission = new CMission( this );
			mission->Serialize( ar );
			
			//////////////////////////////////////////////////////////////////////////
			//Timur[9/11/2002] For backward compatability with shared objects.
			if (objectsNode)
				mission->AddObjectsNode(objectsNode);
			if (objectLayersNode)
				mission->SetLayersNode(objectLayersNode);
			//////////////////////////////////////////////////////////////////////////

			AddMission( mission );
		}
	}
	else
	{
		// Storing
		CLogFile::WriteLine("Storing missions...");
		// Save contents of current mission.
		GetCurrentMission()->SyncContent( false,false );

		XmlNodeRef node = xmlAr.root->newChild( "Missions" );

		//! Store current mission name.
		currentMissionName = GetCurrentMission()->GetName();
		node->setAttr( "Current",currentMissionName );

		// Write all surface types.
		for (int i = 0; i < m_missions.size(); i++)
		{
			CXmlArchive ar( xmlAr );
			ar.root = node->newChild( "Mission" );
			m_missions[i]->Serialize( ar );
		}
		CLogFile::WriteString("Done");
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::SerializeShaderCache( CXmlArchive &xmlAr )
{
	if (xmlAr.bLoading)
	{
		void *pData = 0;
		int nSize = 0;
		if(xmlAr.pNamedData->GetDataBlock( "ShaderCache",pData,nSize ))
		{
			if (nSize <= 0)
				return;
			CString buf;
			char *str = buf.GetBuffer(nSize+1);
			memcpy( str,pData,nSize );
			str[nSize] = 0;
			m_pLevelShaderCache->LoadBuffer( str );
		}
	}
	else
	{
		CString buf;
		m_pLevelShaderCache->SaveBuffer( buf );
		if (buf.GetLength() > 0)
		{
			xmlAr.pNamedData->AddDataBlock( "ShaderCache",buf.GetBuffer(),buf.GetLength() );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CCryEditDoc diagnostics

#ifdef _DEBUG
void CCryEditDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CCryEditDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CCryEditDoc commands

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::ClearLayers()
{
	////////////////////////////////////////////////////////////////////////
	// Clear all texture layers
	////////////////////////////////////////////////////////////////////////
	unsigned int i;

	// Free the layer objects
	for (i=0; i<GetLayerCount(); i++)
		delete GetLayer(i);

	// Clear the list
	m_layers.clear();
}


//////////////////////////////////////////////////////////////////////////
uint32 CCryEditDoc::GetDetailIdLayerFromLayerId( const uint32 dwLayerId )
{
	std::vector<CLayer*>::const_iterator it,end=m_layers.end();

	for(it=m_layers.begin();it!=end;++it)
	{
		CLayer &rLayer = *(*it);

		if(dwLayerId==rLayer.GetOrRequestLayerId())
			return rLayer.GetSurfaceTypeId();
	}

	// recreate referenced layer
	if(!GetIEditor()->GetSystem()->GetI3DEngine()->GetITerrain())		// only if terrain loaded successfully
	{
		CString no;

		no.Format("%d",dwLayerId);

		CLayer *pNewLayer = new CLayer;

		pNewLayer->SetLayerName(CString("LAYER_")+no);
		pNewLayer->SetLayerId(dwLayerId);

		AddLayer(pNewLayer);
	}

	return 0xffffffff;	
}

//////////////////////////////////////////////////////////////////////////
uint32 CCryEditDoc::GetLayerIdFromDetailLayerId( const uint32 _dwDetailLayerId )
{
	uint32 dwDetailLayerId=_dwDetailLayerId;

	CSurfaceType *pSurfaceType = GetSurfaceTypePtr(dwDetailLayerId);

	if(!pSurfaceType)
	{
		// recreate referenced surfacetype

		pSurfaceType = new CSurfaceType;

		CString no;

		no.Format("%d",dwDetailLayerId);

		pSurfaceType->SetName(CString("SURFACETYPE_")+no);

		dwDetailLayerId = AddSurfaceType(pSurfaceType);
	}

	std::vector<CLayer*>::iterator it, end=m_layers.end();

	for(it=m_layers.begin();it!=end;++it)
	{
		CLayer &rLayer = *(*it);

		if(rLayer.GetSurfaceTypeId()==dwDetailLayerId)
			return rLayer.GetOrRequestLayerId();
	}

	// recreate referenced layer

	CLayer *pNewLayer = new CLayer;

	uint32 dwRet = pNewLayer->GetOrRequestLayerId();		// to create new LayerId

	assert(dwRet!=0xffffffff);

	pNewLayer->SetSurfaceTypeId(dwDetailLayerId);
	pNewLayer->SetLayerName(CString("DETAILLAYER_")+pSurfaceType->GetName());

	AddLayer(pNewLayer);

	return dwRet;
}



//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::MarkUsedLayerIds( bool bFree[256] ) const
{
	std::vector<CLayer*>::const_iterator it;

	for(it=m_layers.begin();it!=m_layers.end();++it)
	{
		CLayer *pLayer=*it;

		if(pLayer->GetCurrentLayerId()!=0xffffffff)
			bFree[pLayer->GetCurrentLayerId()]=false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::RemoveLayer( int layer )
{
	assert( layer >= 0 && layer < m_layers.size() );
	delete m_layers[layer];
	m_layers.erase( m_layers.begin() + layer );
}

//////////////////////////////////////////////////////////////////////////
CLayer* CCryEditDoc::FindLayer( const char *sLayerName ) const
{
	for (int i = 0; i < GetLayerCount(); i++)
	{
		CLayer *layer = GetLayer(i);
		if (strcmp(layer->GetLayerName(),sLayerName) == 0)
			return layer;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CLayer* CCryEditDoc::FindLayerByLayerId( const uint32 dwLayerId ) const
{
	for (int i = 0; i < GetLayerCount(); i++)
	{
		CLayer *layer = GetLayer(i);
		if (layer->GetCurrentLayerId() == dwLayerId)
			return layer;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::RemoveLayer( CLayer *layer )
{
	if (layer)
	{
		delete layer;
		m_layers.erase( std::remove(m_layers.begin(),m_layers.end(),layer),m_layers.end() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::SwapLayers( int layer1,int layer2 )
{
	assert( layer1 >= 0 && layer1 < m_layers.size() );
	assert( layer2 >= 0 && layer2 < m_layers.size() );
	std::swap( m_layers[layer1],m_layers[layer2] );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::InvalidateLayers()
{
	////////////////////////////////////////////////////////////////////////
	// Set the update needed flag for all layer
	////////////////////////////////////////////////////////////////////////

	unsigned int i;

	// Free the layer objects
	for (i=0; i< GetLayerCount(); i++)
		GetLayer(i)->InvalidateMask();
}

BOOL CCryEditDoc::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	////////////////////////////////////////////////////////////////////////
	// Handle plugin menus
	////////////////////////////////////////////////////////////////////////

	//DWORD dwPluginID, dwEvtID;
			IUIEvent *pIEvt = NULL;

	// If pHandlerInfo is NULL, then handle the message
	if (pHandlerInfo == NULL)
	{
		if (nCode == CN_COMMAND)
		{
			////////////////////////////////////////////////////////////////////////
			// Handle WM_COMMAND message
			////////////////////////////////////////////////////////////////////////
/*
			// Extract the plugin and event IDs
			dwPluginID = GET_PLUGIN_ID_FROM_MENU_ID(nID);
			dwEvtID = GET_UI_ELEMENT_ID_FROM_MENU_ID(nID);

			// Ask the plugin manager for an event handler
			pIEvt = GetIEditor()->GetPluginManager()->GetEventByIDAndPluginID(dwPluginID, (uint8)dwEvtID);

			// Forward the event and return TRUE to indicate that
			// the event has been processed
			if (pIEvt)
			{
				pIEvt->OnClick(dwEvtID);
				return TRUE;
			}
			*/
		}
		else if (nCode == CN_UPDATE_COMMAND_UI)
		{
			////////////////////////////////////////////////////////////////////////
			// Update UI element state
			////////////////////////////////////////////////////////////////////////
		
			// TODO
		}
	}

	return CDocument::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

BOOL CCryEditDoc::CanCloseFrame(CFrameWnd* pFrame)
{
	////////////////////////////////////////////////////////////////////////
	// Ask the base clase to ask for saving, which also includes the save
	// status of the plugins. Addionaly we query if all the plugins can exit
	// now. A reason for a failure might be that one of the plugins isn
	// currently processing data or has other unsaved information which
	// are not serialized in the project file
	////////////////////////////////////////////////////////////////////////

	if (!CDocument::CanCloseFrame(pFrame))
		return FALSE;

	if (!GetIEditor()->GetPluginManager()->CanAllPluginsExitNow())
		return FALSE;

	return TRUE;
}

bool CCryEditDoc::OnExportTerrainAsGeometrie(const char *pszFileName, RECT rcExport)
{
	////////////////////////////////////////////////////////////////////////
	// Store the terrain in the OBJ format on the disk
	////////////////////////////////////////////////////////////////////////

	t_hmap *pHeigthmap = NULL;
	unsigned int i, j;
	COBJExporter cModel;
	CVector3D vVertex[6];
	CTexCoord2D cTexCoord[6];
	
	float fScaling = m_cHeightmap.GetUnitSize();

	int iWidth = m_cHeightmap.GetWidth();
	CFace sNewFace;
	char szBitmapPath[_MAX_PATH];
	char szInfoPath[_MAX_PATH];
	const int iSurfaceTexWidth = 4096;
	DWORD *pSurface = NULL;
	float fScale = (1.0f / m_cHeightmap.GetWidth()) * (float) iSurfaceTexWidth;
	UINT iSrcX, iSrcY, iDestX, iDestY, iSrcWidth, iDestWidth,iDestHeight;

	if ((int)rcExport.bottom - rcExport.top <= 0 && (int)rcExport.right - rcExport.left <= 0)
		return false;

	BeginWaitCursor();

	// Get data from the heightmap
	pHeigthmap = m_cHeightmap.GetData();
	ASSERT(pHeigthmap);

	// Adjust the rectangle to be valid
	if (rcExport.right < 0)
		rcExport.right = 0;
	if (rcExport.left < 0)
		rcExport.left = 0;
	if (rcExport.top < 0)
		rcExport.top = 0;
	if (rcExport.bottom < 0)
		rcExport.bottom = 0;

	if (rcExport.right >= m_cHeightmap.GetWidth())
		rcExport.right = m_cHeightmap.GetWidth() - 1;
	if (rcExport.bottom >= m_cHeightmap.GetHeight())
		rcExport.bottom = m_cHeightmap.GetHeight() - 1;

	////////////////////////////////////////////////////////////////////////
	// Create triangles, texture coordinates and indices
	////////////////////////////////////////////////////////////////////////

	for (j=rcExport.top; j<rcExport.bottom; j++)
		for (i=rcExport.left; i<rcExport.right; i++)
		{
			int sx = i;
			int sy = j;
			
			// we need world offset
			//int dx = i - rcExport.left;
			//int dy = j - rcExport.top;
			int dx = i;
			int dy = j;

			// scale the same like for objects
			float fExScale = 100.0f;

			// First triangle
			vVertex[0].fY = dx * fScaling * fExScale;
			vVertex[0].fX = dy * fScaling * fExScale;
			vVertex[0].fZ = pHeigthmap[sx + sy * iWidth] * fExScale;
			vVertex[2].fY = (dx + 1) * fScaling * fExScale;
			vVertex[2].fX = dy * fScaling * fExScale;
			vVertex[2].fZ = pHeigthmap[(sx + 1) + sy*iWidth] * fExScale;
			vVertex[1].fY = dx * fScaling * fExScale;
			vVertex[1].fX = (dy + 1) * fScaling * fExScale;
			vVertex[1].fZ = pHeigthmap[sx + (sy + 1)*iWidth] * fExScale;

			cTexCoord[0].fU = (float) (dx) / (float) (rcExport.right - rcExport.left);
			cTexCoord[0].fV = -(float) (dy) / (float) (rcExport.bottom - rcExport.top);
			
			cTexCoord[2].fU = (float) ((dx + 1)) / (float) (rcExport.right - rcExport.left);
			cTexCoord[2].fV = -(float) (dy) / (float) (rcExport.bottom - rcExport.top);
			
			cTexCoord[1].fU = (float) (dx) / (float) (rcExport.right - rcExport.left);
			cTexCoord[1].fV = -(float) ((dy + 1)) / (float) (rcExport.bottom - rcExport.top);

			// Second triangle
			vVertex[3].fY = (dx + 1) * fScaling * fExScale;
			vVertex[3].fX = dy * fScaling * fExScale;
			vVertex[3].fZ = pHeigthmap[(sx + 1) + sy * iWidth] * fExScale;
			vVertex[5].fY = (dx + 1) * fScaling * fExScale;
			vVertex[5].fX = (dy + 1) * fScaling * fExScale;
			vVertex[5].fZ = pHeigthmap[(sx + 1) + (sy + 1) * iWidth] * fExScale;
			vVertex[4].fY = dx * fScaling * fExScale;
			vVertex[4].fX = (dy + 1) * fScaling * fExScale;
			vVertex[4].fZ = pHeigthmap[sx + (sy + 1) * iWidth] * fExScale;

			cTexCoord[3].fU = (float) ((dx + 1)) / (float) (rcExport.right - rcExport.left);
			cTexCoord[3].fV = -(float) (dy) / (float) (rcExport.bottom - rcExport.top);
			cTexCoord[5].fU = (float) ((dx + 1)) / (float) (rcExport.right - rcExport.left);
			cTexCoord[5].fV = -(float) ((dy + 1)) / (float) (rcExport.bottom - rcExport.top);
			cTexCoord[4].fU = (float) (dx) / (float) (rcExport.right - rcExport.left);
			cTexCoord[4].fV = -(float) ((dy + 1)) / (float) (rcExport.bottom - rcExport.top);

			// Add the vertices to the model and store the indices in the face
			memset(&sNewFace, 0, sizeof(CFace));
			sNewFace.iVertexIndices[0] = cModel.GetVertexIdx(&vVertex[0]);
			sNewFace.iVertexIndices[1] = cModel.GetVertexIdx(&vVertex[1]);
			sNewFace.iVertexIndices[2] = cModel.GetVertexIdx(&vVertex[2]);
			sNewFace.iTexCoordIndices[0] = cModel.GetTexCoordIdx(&cTexCoord[0]);
			sNewFace.iTexCoordIndices[1] = cModel.GetTexCoordIdx(&cTexCoord[1]);
			sNewFace.iTexCoordIndices[2] = cModel.GetTexCoordIdx(&cTexCoord[2]);
			cModel.AddFace(sNewFace);
			memset(&sNewFace, 0, sizeof(CFace));
			sNewFace.iVertexIndices[0] = cModel.GetVertexIdx(&vVertex[3]);
			sNewFace.iVertexIndices[1] = cModel.GetVertexIdx(&vVertex[4]);
			sNewFace.iVertexIndices[2] = cModel.GetVertexIdx(&vVertex[5]);
			sNewFace.iTexCoordIndices[0] = cModel.GetTexCoordIdx(&cTexCoord[3]);
			sNewFace.iTexCoordIndices[1] = cModel.GetTexCoordIdx(&cTexCoord[4]);
			sNewFace.iTexCoordIndices[2] = cModel.GetTexCoordIdx(&cTexCoord[5]);
			cModel.AddFace(sNewFace);
		}

	////////////////////////////////////////////////////////////////////////
	// Export .OBJ and .MTL files
	////////////////////////////////////////////////////////////////////////

	// Write the model into the file
	if (!cModel.WriteOBJ(pszFileName))
	{
		CLogFile::WriteLine("Error while exporting part of the heightmap as OBJ model !");
		AfxMessageBox("Crititcal error during exporting !");
		return false;
	}

	////////////////////////////////////////////////////////////////////////
	// Export Info
	////////////////////////////////////////////////////////////////////////
	{
		strcpy(szInfoPath, pszFileName);
		PathRemoveExtension(szInfoPath);
		strcat(szInfoPath,".inf");
		if (CFileUtil::OverwriteFile( szInfoPath ))
		{
			FILE *infoFile = fopen( szInfoPath,"wt" );
			if (infoFile)
			{
				fprintf( infoFile,"x=%d,y=%d,width=%d,height=%d",rcExport.left,rcExport.top,rcExport.right-rcExport.left,rcExport.bottom-rcExport.top );
				fclose( infoFile );
			}
		}
	}

	////////////////////////////////////////////////////////////////////////
	// Generate the texture
	////////////////////////////////////////////////////////////////////////
	
	// Allocate memory and generate the surface
	CImage terrainSurface;
	terrainSurface.Allocate( iSurfaceTexWidth,iSurfaceTexWidth );
	pSurface = (DWORD*)terrainSurface.GetData();
	//cTexture.GenerateSurface(pSurface, iSurfaceTexWidth, iSurfaceTexWidth, GEN_USE_LIGHTING|GEN_STATOBJ_SHADOWS|GEN_SHOW_WATER, NULL );
	CTerrainTexGen texGen;
	texGen.GenerateSurfaceTexture( ETTG_LIGHTING|ETTG_STATOBJ_SHADOWS|ETTG_SHOW_WATER,terrainSurface );

	//CImageUtil::SaveJPEG( "Terrain.jpg",iSurfaceTexWidth,iSurfaceTexWidth,pSurface );

	// Scale the export rectangle to texture coordinates
	rcExport.bottom *= fScale;
	rcExport.left *= fScale;
	rcExport.right *= fScale;
	rcExport.top *= fScale;

	CRect rc1 = rcExport;
	rc1 &= CRect( 0,0,iSurfaceTexWidth,iSurfaceTexWidth );
	rcExport = rc1;
	
	iDestWidth = rcExport.right - rcExport.left;
	iDestHeight =  rcExport.bottom - rcExport.top;
	iSrcWidth = iSurfaceTexWidth;

	// Allocate memory for the needed portion of the surface
	CImage exportImage;
	exportImage.Allocate( iDestWidth,iDestHeight );

	// Extract the needed portion of the surface
	for (j=rcExport.top; j<rcExport.bottom; j++)
	{
		iSrcX = rcExport.left;
		iSrcY = j;

		iDestX = 0;
		iDestY = j - rcExport.top;

		memcpy( &exportImage.ValueAt(iDestX,iDestY), &pSurface[iSrcX + iSrcY * iSrcWidth], sizeof(DWORD)*iDestWidth );
	}

	////////////////////////////////////////////////////////////////////////
	// Export the texture
	////////////////////////////////////////////////////////////////////////

	strcpy(szBitmapPath, pszFileName);
	PathRemoveFileSpec(szBitmapPath);
	PathAddBackslash(szBitmapPath);
	strcat(szBitmapPath, "Terrain.bmp");

	if (!CImageUtil::SaveImage( szBitmapPath, exportImage ))
		return false;

	
	EndWaitCursor();

	return true;
}

/*
bool CCryEditDoc::GetHeightmapData16(uint16 *pData, UINT iDestWidth, 
									 bool bSmooth, bool bNoise)
{
	////////////////////////////////////////////////////////////////////////
	// Get the heightmap data interpolated & noised up to 2 byte
	////////////////////////////////////////////////////////////////////////
	
	unsigned int i, j;
	long iXSrcFl, iXSrcCe, iYSrcFl, iYSrcCe;
	float fXSrc, fYSrc;
	float fHeight[4];
	float fHeightWeight[4];
	float fHeightBottom;
	float fHeightTop;
	DWORD *pHeightmapData = NULL;
	UINT dwHeightmapWidth = m_cHeightmap.GetWidth();
	int iYSrcCeSave, iXSrcCeSave;
	uint16 *pDataStart = pData;
	UINT iCurPos;
	CNoise cNoise;
	static bool bFirstQuery = true;
	static CDynamicArray2D cHeightmap(256, 256);
	//float fFrequency = 7.0f;
	float fFrequency = 3.0f;
	float fFrequencyStep = 2.0f;
	float fYScale = 255;
	float fFade = 0.46f;
	float fLowestPoint = 512000.0f, fHighestPoint = -512000.0f;
	float fValueRange;

	// If this is the first run, we need a to generate noise array
	if (bFirstQuery)
	{
		CLogFile::WriteLine("Calculating noise array...");

		// Just once
		bFirstQuery = false;

		// Process layers
		for (i=0; i<8; i++)
		{
			// Apply the fractal noise function to the array
			cNoise.FracSynthPass(&cHeightmap, fFrequency, fYScale, 
				256, 256, TRUE);

			// Modify noise generation parameters
			fFrequency *= fFrequencyStep;
			fYScale *= fFade;	
		}

		// Find the value range
		for (i=0; i<256; i++)
			for (j=0; j<256; j++)
			{
				fLowestPoint = __min(fLowestPoint, cHeightmap.m_Array[i][j]);
				fHighestPoint = __max(fHighestPoint, cHeightmap.m_Array[i][j]);
			}

		// Storing the value range in this way saves us a division and a multiplication
		fValueRange = 1.0f / (fHighestPoint + (float) fabs(fLowestPoint)) * 2048.0f;

		// Normalize the heightmap
		for (i=0; i<256; i++)
			for (j=0; j<256; j++)
			{
				cHeightmap.m_Array[i][j] += (float) (fLowestPoint);
				cHeightmap.m_Array[i][j] *= fValueRange;
			}
	}
		
	// Only log significant allocations. This also prevents us from cluttering the
	// log file during the lightmap preview generation
	if (iDestWidth >= 512)
		CLogFile::FormatLine("Retrieving heightmap data (Width: %i)...", iDestWidth);

	// Verify the pointer to the heightmap destination array
	if (IsBadWritePtr(pData, sizeof(int16) * iDestWidth * iDestWidth))
	{
		ASSERT(FALSE);
		return false;
	}

	// Get the data from the heightmap
	pHeightmapData = new DWORD[m_cHeightmap.GetWidth() * m_cHeightmap.GetHeight()];
	if (!pHeightmapData)
	{
		CLogFile::WriteLine("Memory allocation error while scaling the heightmap");
		AfxMessageBox("Memory allocation error while scaling the heightmap !");
		ASSERT(pHeightmapData);
		return false;
	}
	VERIFY(m_bmpHeightmap.GetBitmapBits(m_cHeightmap.GetWidth() * m_cHeightmap.GetHeight() * 
		sizeof(DWORD), pHeightmapData));

	// Loop trough each field of the new image and interpolate the value
	// from the source heightmap
	for (j=0; j<iDestWidth; j++)
	{
		// Calculate the average source array position
		fYSrc = ((float) j / (float) iDestWidth) * dwHeightmapWidth;
		assert(fYSrc >= 0.0f && fYSrc <= dwHeightmapWidth);
		
		// Precalculate floor and ceiling values. Use fast asm integer floor and
		// fast asm float / integer conversion
		iYSrcFl = ifloor(fYSrc);
		iYSrcCe = FloatToIntRet((float) ceil(fYSrc));

		// Distribution between top and bottom height values
		fHeightWeight[2] = (float) iYSrcCe - fYSrc;
		fHeightWeight[3] = fYSrc - (float) iYSrcFl;

		for (i=0; i<iDestWidth; i++)
		{
			// Calculate the average source array position
			fXSrc = ((float) i / (float) iDestWidth) * dwHeightmapWidth;
			assert(fXSrc >= 0.0f && fXSrc <= dwHeightmapWidth);

			// Precalculate floor and ceiling values. Use fast asm integer floor and
			// fast asm float / integer conversion
			iXSrcFl = ifloor(fXSrc);
			iXSrcCe = FloatToIntRet((float) ceil(fXSrc));
			
			// Distribution between left and right height values
			fHeightWeight[0] = (float) iXSrcCe - fXSrc;
			fHeightWeight[1] = fXSrc - (float) iXSrcFl;

			// Avoid error when floor() and ceil() return the same value
			if (fHeightWeight[0] == 0.0f && fHeightWeight[1] == 0.0f)
			{
				fHeightWeight[0] = 0.5f;
				fHeightWeight[1] = 0.5f;
			}

			// Calculate how much weight each height value has

			// Avoid error when floor() and ceil() return the same value
			if (fHeightWeight[2] == 0.0f && fHeightWeight[3] == 0.0f)
			{
				fHeightWeight[2] = 0.5f;
				fHeightWeight[3] = 0.5f;
			}

			// Clamp the ceiling coordinates to a save range
			if (iYSrcCe >= (int) dwHeightmapWidth)
				iYSrcCeSave = dwHeightmapWidth - 1;
			else 
				iYSrcCeSave = iYSrcCe;

			if (iXSrcCe >= (int) dwHeightmapWidth)
				iXSrcCeSave = dwHeightmapWidth - 1;
			else 
				iXSrcCeSave = iXSrcCe;

			// Get the four nearest height values
			fHeight[0] = (float) (pHeightmapData[iXSrcFl + iYSrcFl * dwHeightmapWidth] & 0x000000FF) * 255;
			fHeight[1] = (float) (pHeightmapData[iXSrcCeSave + iYSrcFl * dwHeightmapWidth] & 0x000000FF) * 255;
			fHeight[2] = (float) (pHeightmapData[iXSrcFl + iYSrcCeSave * dwHeightmapWidth] & 0x000000FF) * 255;
			fHeight[3] = (float) (pHeightmapData[iXSrcCeSave + iYSrcCeSave * dwHeightmapWidth] & 0x000000FF) * 255;

			// Interpolate between the four nearest height values
	
			// Get the height for the given X position trough interpolation between
			// the left and the right height
			fHeightBottom = (fHeight[0] * fHeightWeight[0] + fHeight[1] * fHeightWeight[1]);
			fHeightTop    = (fHeight[2] * fHeightWeight[0] + fHeight[3] * fHeightWeight[1]);

			// Set the new value in the destination heightmap
			*pData++ = (uint16) (fHeightBottom * fHeightWeight[2] + fHeightTop * fHeightWeight[3]);
		}
	}

	// Smooth the heightmap data and add noise
	iCurPos = 0;
	for (j=1; j<iDestWidth - 1; j++)
	{
		// Precalculate for better speed
		iCurPos = j * iDestWidth + 1;

		for (i=1; i<iDestWidth - 1; i++)
		{
			// Next pixel
			iCurPos++;

			if (bSmooth)
			{
				// Smooth it out
				pDataStart[iCurPos] = FloatToIntRet(
					(pDataStart[iCurPos] + pDataStart[iCurPos + 1]  + 
					 pDataStart[iCurPos + iDestWidth]               +
					 pDataStart[iCurPos  + iDestWidth + 1]          + 
					 pDataStart[iCurPos - 1]                        + 
					 pDataStart[iCurPos - iDestWidth]               + 
					 pDataStart[iCurPos  - iDestWidth - 1]          + 
					 pDataStart[iCurPos - iDestWidth + 1]           + 
					 pDataStart[iCurPos + iDestWidth - 1])
					 * 0.11111111111f);
			}

			// Add the signed noise
			if (bNoise)
			{
				pDataStart[iCurPos] = (uint16) __max(0.0f, pDataStart[iCurPos] + 
					cHeightmap.m_Array[i % 256][j % 256]);
			}
		}
	}

	// Free the heightmap data
	delete [] pHeightmapData;
	pHeightmapData = 0;

	return true;
}

bool CCryEditDoc::GetHeightmapData(HEIGHTMAPVALUE *pData, UINT iDestWidth)
{
	////////////////////////////////////////////////////////////////////////
	// Get the data of the heightmap, resize it if neccessary. Heightmap
	// has to be square and a power of two
	//
	// (TODO: Maybe this should be rewritten, can be done a hell lot faster)
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;
	int iXSrcFl, iXSrcCe, iYSrcFl, iYSrcCe;
	float fXSrc, fYSrc;
	float fHeight[4];
	float fHeightWeight[4];
	float fHeightBottom;
	float fHeightTop;
	DWORD *pHeightmapData = NULL;
	UINT dwHeightmapWidth = m_cHeightmap.GetWidth();
	int iYSrcCeSave, iXSrcCeSave;
	HEIGHTMAPVALUE *pDataStart = pData;
	
	// Only log significat allocations. This also prevents us from cluttering the
	// log file during the lightmap preview generation
	if (iDestWidth >= 512)
		CLogFile::FormatLine("Retrieving heightmap data (Width: %i)...", iDestWidth);

	// Verify the pointer to the heightmap destination array
	if (IsBadWritePtr(pData, sizeof(HEIGHTMAPVALUE) * iDestWidth * iDestWidth))
	{
		ASSERT(FALSE);
		return false;
	}

	// Get the data from the heightmap
	pHeightmapData = new DWORD[m_cHeightmap.GetWidth() * m_cHeightmap.GetHeight()];
	if (!pHeightmapData)
	{
		CLogFile::WriteLine("Memory allocation error while scaling the heightmap");
		AfxMessageBox("Memory allocation error while scaling the heightmap !");
		ASSERT(pHeightmapData);
		return false;
	}
	VERIFY(m_bmpHeightmap.GetBitmapBits(m_cHeightmap.GetWidth() * m_cHeightmap.GetHeight() * 
		sizeof(DWORD), pHeightmapData));

	// Loop trough each field of the new image and interpolate the value
	// from the source heightmap
	for (j=0; j<iDestWidth; j++)
	{
		// Calculate the average source array position
		fYSrc = ((float) j / (float) iDestWidth) * dwHeightmapWidth;
		assert(fYSrc >= 0.0f && fYSrc <= dwHeightmapWidth);
		
		// Precalculate floor and ceiling values. Use fast asm integer floor and
		// fast asm float / integer conversion
		iYSrcFl = ifloor(fYSrc);
		iYSrcCe = FloatToIntRet((float) ceil(fYSrc));

		// Distribution between top and bottom height values
		fHeightWeight[2] = (float) iYSrcCe - fYSrc;
		fHeightWeight[3] = fYSrc - (float) iYSrcFl;

		for (i=0; i<iDestWidth; i++)
		{
			// Calculate the average source array position
			fXSrc = ((float) i / (float) iDestWidth) * dwHeightmapWidth;
			assert(fXSrc >= 0.0f && fXSrc <= dwHeightmapWidth);

			// Precalculate floor and ceiling values. Use fast asm integer floor and
			// fast asm float / integer conversion
			iXSrcFl = ifloor(fXSrc);
			iXSrcCe = FloatToIntRet((float) ceil(fXSrc));
			
			// Distribution between left and right height values
			fHeightWeight[0] = (float) iXSrcCe - fXSrc;
			fHeightWeight[1] = fXSrc - (float) iXSrcFl;

			// Avoid error when floor() and ceil() return the same value
			if (fHeightWeight[0] == 0.0f && fHeightWeight[1] == 0.0f)
			{
				fHeightWeight[0] = 0.5f;
				fHeightWeight[1] = 0.5f;
			}

			// Calculate how much weight each height value has

			// Avoid error when floor() and ceil() return the same value
			if (fHeightWeight[2] == 0.0f && fHeightWeight[3] == 0.0f)
			{
				fHeightWeight[2] = 0.5f;
				fHeightWeight[3] = 0.5f;
			}

			// Clamp the ceiling coordinates to a save range
			if (iYSrcCe >= (int) dwHeightmapWidth)
				iYSrcCeSave = dwHeightmapWidth - 1;
			else 
				iYSrcCeSave = iYSrcCe;

			if (iXSrcCe >= (int) dwHeightmapWidth)
				iXSrcCeSave = dwHeightmapWidth - 1;
			else 
				iXSrcCeSave = iXSrcCe;

			// Get the four nearest height values
			fHeight[0] = (float) (pHeightmapData[iXSrcFl + iYSrcFl * dwHeightmapWidth] & 0x000000FF);
			fHeight[1] = (float) (pHeightmapData[iXSrcCeSave + iYSrcFl * dwHeightmapWidth] & 0x000000FF);
			fHeight[2] = (float) (pHeightmapData[iXSrcFl + iYSrcCeSave * dwHeightmapWidth] & 0x000000FF);
			fHeight[3] = (float) (pHeightmapData[iXSrcCeSave + iYSrcCeSave * dwHeightmapWidth] & 0x000000FF);

			// Interpolate between the four nearest height values
	
			// Get the height for the given X position trough interpolation between
			// the left and the right height
			fHeightBottom = (fHeight[0] * fHeightWeight[0] + fHeight[1] * fHeightWeight[1]);
			fHeightTop    = (fHeight[2] * fHeightWeight[0] + fHeight[3] * fHeightWeight[1]);

			// Set the new value in the destination heightmap
			*pData++ = FloatToByte(fHeightBottom * fHeightWeight[2] + fHeightTop * fHeightWeight[3]);
		}
	}

	// Free the heightmap data
	delete [] pHeightmapData;
	pHeightmapData = 0;

	return true;
}

void CCryEditDoc::SetHeightmapData(HEIGHTMAPVALUE *pData)
{
	////////////////////////////////////////////////////////////////////////
	// Store heightmap data in the bitmap
	////////////////////////////////////////////////////////////////////////

	DWORD *pImageData = NULL;
	DWORD *pImageDataStart = NULL;
	DWORD *pImageDataEnd = NULL;
	uint8 iColor;

	CLogFile::WriteLine("Storing heightmap data in the document...");

	// Allocate memory for the image
	pImageData = new DWORD[m_cHeightmap.GetWidth() * m_cHeightmap.GetHeight()];
	pImageDataStart = pImageData;
	pImageDataEnd = &pImageData[m_cHeightmap.GetWidth() * m_cHeightmap.GetHeight()];

	while (pImageData != pImageDataEnd)
	{
		// Get the color value from the source array
		iColor = (uint8) (*pData++);
	
		// Convert it into 0x00BBGGRR format and write the pixel
		*pImageData++ = iColor | iColor << 8 | iColor << 16;
	}
	
	// Load the heightmap image into the bitmap
	VERIFY(m_bmpHeightmap.SetBitmapBits(m_cHeightmap.GetWidth() * m_cHeightmap.GetHeight()
		* sizeof(DWORD), pImageDataStart));

	// Free the image memory
	delete [] pImageDataStart;
	pImageDataStart = 0;
}
*/

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::RemoveSurfaceType( int i )
{
	SetModifiedFlag();
	m_surfaceTypes.erase( m_surfaceTypes.begin()+i );
}

//////////////////////////////////////////////////////////////////////////
int CCryEditDoc::AddSurfaceType( CSurfaceType *srf )
{
	SetModifiedFlag();
	m_surfaceTypes.push_back(srf);

	return m_surfaceTypes.size()-1;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::RemoveAllSurfaceTypes()
{
	SetModifiedFlag();
	m_surfaceTypes.clear();
}

//////////////////////////////////////////////////////////////////////////
int CCryEditDoc::FindSurfaceType( const CString &name )
{
	for (int i = 0; i < m_surfaceTypes.size(); i++)
	{
		if (stricmp(m_surfaceTypes[i]->GetName(),name) == 0)
			return i;
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
BOOL CCryEditDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	// Restore directory to root.
	SetCurrentDirectoryW( GetIEditor()->GetMasterCDFolder() );

	CString relativeLevelName = Path::GetRelativePath(lpszPathName);

	CLogFile::FormatLine("Opening document %s", (const char*)relativeLevelName);

	////////////////////////////////////////////////////////////////////////
	// Write the full filename and path to the log
	////////////////////////////////////////////////////////////////////////
	m_loadFailed = false;

	ICryPak *pIPak = GetIEditor()->GetSystem()->GetIPak();

	if (pIPak->OpenPack( relativeLevelName ))
	{
		CWaitCursor wait;
		CString levelPath = Path::GetPath(relativeLevelName);

		CPakFile pakFile;
		CXmlArchive xmlAr;
		xmlAr.bLoading = true;
		bool bRes = xmlAr.LoadFromPak( levelPath,pakFile );
		// After reading close this pak.
		pIPak->ClosePack( relativeLevelName );
		if (!bRes)
			return FALSE;

		// Load next level resource list.

		pIPak->GetRecorderdResourceList(ICryPak::RFOM_NextLevel)->Load( Path::Make(levelPath,"resourcelist.txt") );
		pIPak->Notify( ICryPak::EVENT_BEGIN_LOADLEVEL );

		DeleteContents();
		SetModifiedFlag();  // dirty during de-serialize

		Load( xmlAr,relativeLevelName );

		// We don't need next level resource list anymore.
		pIPak->GetRecorderdResourceList(ICryPak::RFOM_NextLevel)->Clear();
		pIPak->Notify( ICryPak::EVENT_END_LOADLEVEL );

		SetModifiedFlag(FALSE); // start off with unmodified
	}
	else
	{
		if (!CDocument::OnOpenDocument(relativeLevelName))
			return FALSE;
	}

	if (m_loadFailed)
		return FALSE;

	CLogFile::FormatLine("Successfully opened document %s", (const char*)relativeLevelName);

	return TRUE;
}




//////////////////////////////////////////////////////////////////////////
BOOL CCryEditDoc::OnSaveDocument( LPCTSTR filename )
{
	// Restore directory to root.
	SetCurrentDirectoryW( GetIEditor()->GetMasterCDFolder() );

	CryLog( "Saving to %s...", (const char*)filename );

	MEMORY_DEBUG_POINT(GetIEditor()->GetSystem(),1)			// activate with sys_memory_debug
	
	GetIEditor()->Notify( eNotify_OnBeginSceneSave );

	CString prevPathName = GetPathName();
	bool bFileChanged = prevPathName != CString(filename);

	BOOL res = TRUE;

	//BOOL res = CDocument::OnSaveDocument( filename );
	// Save to Pak file.

	if (!CFileUtil::OverwriteFile( filename ))
		return FALSE;

	if(!m_cHeightmap.m_TerrainRGBTexture.WouldSaveSucceed())
		return FALSE;

	// Changes filename for this document.
	SetPathName( filename );

	CWaitCursor wait;
	
	// Save level to XML archive.
	CXmlArchive xmlAr;

	MEMORY_DEBUG_POINT(GetIEditor()->GetSystem(),2)			// activate with sys_memory_debug

	Save( xmlAr );

	MEMORY_DEBUG_POINT(GetIEditor()->GetSystem(),3)			// activate with sys_memory_debug

	CString tempSaveFile = Path::ReplaceExtension( filename,"tmp" );
	SetFileAttributes( tempSaveFile,FILE_ATTRIBUTE_NORMAL );
	DeleteFile( tempSaveFile );

	CPakFile pakFile;
	if (!pakFile.Open( tempSaveFile,false ))
		return FALSE;

	MEMORY_DEBUG_POINT(GetIEditor()->GetSystem(),4)			// activate with sys_memory_debug

	// Save XML archive to pak file.
	bool bSaved = xmlAr.SaveToPak( Path::GetPath(tempSaveFile),pakFile );

	MEMORY_DEBUG_POINT(GetIEditor()->GetSystem(),5)			// activate with sys_memory_debug

	pakFile.Close();

	MEMORY_DEBUG_POINT(GetIEditor()->GetSystem(),6)			// activate with sys_memory_debug

	if (bSaved)
	{
		if (gSettings.bBackupOnSave)
			CFileUtil::BackupFile( filename );

		SetFileAttributes( filename,FILE_ATTRIBUTE_NORMAL );
		DeleteFile( filename );
		if (!MoveFileEx( tempSaveFile,filename,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH ))
			bSaved = false;
	}
	else
	{
		DeleteFile( tempSaveFile );
	}

	GetIEditor()->Notify( eNotify_OnEndSceneSave );
	SetModifiedFlag(FALSE);

	// Commit changes to the disk.
	_flushall();

	if (!bSaved)
	{
		AfxMessageBox( "Save Failed" ,MB_OK|MB_ICONERROR );
	}
  else
    CLogFile::WriteLine("Level successfully saved");

	MEMORY_DEBUG_POINT(GetIEditor()->GetSystem(),7)			// activate with sys_memory_debug

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CCryEditDoc::SaveToFile( const CString &szFilename )
{
	CFile cFile;

	CWaitCursor wait;

	if (!CFileUtil::OverwriteFile( szFilename ))
		return false;

	m_level = Path::GetFileName( szFilename );

	CXmlArchive xmlAr;
	GetIEditor()->GetDocument()->Save( xmlAr );

	CPakFile pakFile;
	if (!pakFile.Open( szFilename,false ))
		return false;

	DeleteFile( szFilename );
	// Save XML archive to pak file.
	xmlAr.SaveToPak( Path::GetPath(szFilename),pakFile );
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CCryEditDoc::LoadFromFile( const CString &szFilename,bool bReloadEngineLevel )
{
	ICryPak *pIPak = GetIEditor()->GetSystem()->GetIPak();

	if (pIPak->OpenPack( szFilename ))
	{
		DeleteContents();
		SetModifiedFlag();  // dirty during de-serialize

		CWaitCursor wait;
		CString levelPath = Path::GetPath(szFilename);
		CPakFile pakFile;
		CXmlArchive xmlAr;
		xmlAr.bLoading = true;
		if (!xmlAr.LoadFromPak( levelPath,pakFile ))
			return FALSE;
		// After reading close this pak.
		pIPak->ClosePack( szFilename );
		Load( xmlAr,szFilename,bReloadEngineLevel );
		SetModifiedFlag(FALSE); // start off with unmodified
	}
	else
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::HoldToFile( const CString &szFilename )
{
	CString filename = Path::Make( Path::GetPath(GetPathName()),szFilename );
	SaveToFile( filename );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::FetchFromFile( const CString &szFilename )
{
	////////////////////////////////////////////////////////////////////////
	// Get the latestf state back
	////////////////////////////////////////////////////////////////////////
	CString filename = Path::Make( Path::GetPath(GetPathName()),szFilename );
	
	{
		CFile cFile;
		// Open the file for writing, create it if needed
		if (!cFile.Open(filename, CFile::modeRead))
		{
			AfxMessageBox("You have to use 'Hold' before you can fetch !");
			return;
		}
	}

	// Does the document contain unsaved data ?
	if (GetIEditor()->GetDocument()->IsModified())
	{
		if (AfxMessageBox("The document contains unsaved data, it will be lost if fetched\r\nReally fetch old state ?", MB_ICONQUESTION | MB_YESNO, NULL) != IDYES)
		{
			return;
		}
	}

	GetIEditor()->FlushUndo();

	// Load the state
	LoadFromFile( filename,false );

	// Update terrain in engine.
	m_cHeightmap.UpdateEngineTerrain();
	//GetIEditor()->UpdateViews( eUpdateHeightmap );

	GetIEditor()->FlushUndo();
}

//////////////////////////////////////////////////////////////////////////
BOOL CCryEditDoc::DoSave(LPCTSTR lpszPathName, BOOL bReplace)
{
	BOOL bResult = CDocument::DoSave(lpszPathName,bReplace);
	// Restore current directory to root.
	SetCurrentDirectoryW( GetIEditor()->GetMasterCDFolder() );
	return bResult;
}

/*
//////////////////////////////////////////////////////////////////////////
BOOL CCryEditDoc::DoSave(LPCTSTR lpszPathName, BOOL bReplace)
{
	char currentDirectory[_MAX_PATH];
	GetCurrentDirectory( sizeof(currentDirectory),currentDirectory );
	
	CDocument::DoSave( lpszPathName,bReplace );
	if (lpszPathName == NULL)
	{
		// Save As.
		char szFilePath[_MAX_PATH];
		char szRelativePath[_MAX_PATH];

		strcpy( szFilePath,GetPathName() );
		PathRemoveFileSpec( szFilePath );
		CString masterLevelsFolder = CString(GetIEditor()->GetMasterCDFolder()) + "Levels\\";

		if (PathRelativePathTo(szRelativePath, masterLevelsFolder,FILE_ATTRIBUTE_DIRECTORY, szFilePath, FILE_ATTRIBUTE_DIRECTORY))
		{
			CGameExporter gameExporter( GetIEditor()->GetSystem() );
			gameExporter.Export(false,true);
		}
		else
		{
			AfxMessageBox( "Level mast be stored in MasterCD\\Levels\\LevelName folder" );
		}
	}
	// Restore directory.
	SetCurrentDirectory( currentDirectory );
	return TRUE; // success
}
*/

//////////////////////////////////////////////////////////////////////////
CMission*	CCryEditDoc::GetCurrentMission()
{
	if (m_mission)
	{
		return m_mission;
	}
	if (!m_missions.empty())
	{
		// Choose first available mission.
		SetCurrentMission( m_missions[0] );
		return m_mission;
	}
	// Create initial mission.
	m_mission = new CMission(this);
	m_mission->SetName( "Mission0" );
	AddMission( m_mission );
	m_mission->SyncContent( true,false );
	return m_mission;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::SetCurrentMission( CMission *mission )
{
	if (mission != m_mission)
	{
		//CObjectManager *objMan = GetIEditor()->GetObjectManager();
		// This could take a loong time.
		CWaitCursor wait;

		if (m_mission)
			m_mission->SyncContent( false,false );
		m_mission = mission;
		m_mission->SyncContent( true,false );

		GetIEditor()->GetGameEngine()->LoadMission( m_mission->GetName() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::ClearMissions()
{
	// Release missions.
	for (int i = 0; i < m_missions.size(); i++)
	{
		delete m_missions[i];
	}
	m_missions.clear();
	m_mission = 0;
}

//////////////////////////////////////////////////////////////////////////
CMission*	CCryEditDoc::FindMission( const CString &name ) const
{
	for (int i = 0; i < m_missions.size(); i++)
	{
		if (stricmp(name,m_missions[i]->GetName()) == 0)
			return m_missions[i];
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::AddMission( CMission *mission )
{
	ASSERT( std::find( m_missions.begin(),m_missions.end(),mission ) == m_missions.end() );
	m_missions.push_back( mission );
	GetIEditor()->Notify( eNotify_OnInvalidateControls );
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::RemoveMission( CMission *mission )
{
	// If Deleting current mission.
	if (mission == m_mission)
	{
		m_mission = 0;
	}
	m_missions.erase( std::find( m_missions.begin(),m_missions.end(),mission ) );
	GetIEditor()->Notify( eNotify_OnInvalidateControls );
}

//////////////////////////////////////////////////////////////////////////
LightingSettings* CCryEditDoc::GetLighting()
{
	return GetCurrentMission()->GetLighting();
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::RegisterListener( IDocListener *listener )
{
	if (std::find(m_listeners.begin(),m_listeners.end(),listener) == m_listeners.end())
		m_listeners.push_back(listener);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::UnregisterListener( IDocListener *listener )
{
	m_listeners.remove(listener);
}

//////////////////////////////////////////////////////////////////////////
void CCryEditDoc::LogLoadTime( int time )
{
	// Open log file.
	char szExeFileName[_MAX_PATH];
	GetModuleFileName( GetModuleHandle(NULL), szExeFileName, sizeof(szExeFileName));
	CString exePath = Path::GetPath( szExeFileName );
	CString filename = Path::Make( exePath,"LevelLoadTime.log" );

	CString level = GetIEditor()->GetGameEngine()->GetLevelName();
	CLogFile::FormatLine( "[LevelLoadTime] Level %s loaded in %d seconds",(const char*)level,time/1000 );

	SetFileAttributes( filename,FILE_ATTRIBUTE_ARCHIVE );
	FILE *file = fopen( filename,"at" );
	if (file)
	{
		CString version = GetIEditor()->GetFileVersion().ToString();
		CString text;
		time = time/1000;
		text.Format( "\n[%s] Level %s loaded in %d seconds",(const char*)version,(const char*)level,time );
		fwrite( (const char*)text,text.GetLength(),1,file );
		fclose(file);
	}
}


void CCryEditDoc::GetMemoryUsage( ICrySizer *pSizer )
{
	{
	  SIZER_COMPONENT_NAME(pSizer, "UndoManager(estimate)");

		GetIEditor()->GetUndoManager()->GetMemoryUsage(pSizer);
	}

	pSizer->Add(*this);

	{
		SIZER_COMPONENT_NAME(pSizer,"Layers");

		std::vector<CLayer*>::iterator it;

		for(it=m_layers.begin();it!=m_layers.end();++it)
		{
			CLayer *pLayer=*it;

			pLayer->GetMemoryUsage(pSizer);
		}
	}

	{
		SIZER_COMPONENT_NAME(pSizer,"CHeightmap");

		GetHeightmap().GetMemoryUsage(pSizer);
	}
}

void CCryEditDoc::RegisterConsoleVariables()
{
	doc_validate_surface_types = gEnv->pConsole->GetCVar("doc_validate_surface_types");
	if (!doc_validate_surface_types)
	{
		doc_validate_surface_types = gEnv->pConsole->RegisterInt("doc_validate_surface_types",0,0,
			"Flag indicating whether icons are displayed on the animation graph.\n"
			"Default is 1.\n",
			OnValidateSurfaceTypesChanged);
	}
}

void CCryEditDoc::OnValidateSurfaceTypesChanged(ICVar*)
{
	CErrorsRecorder errorsRecorder;
	CSurfaceTypeValidator().Validate();
}
