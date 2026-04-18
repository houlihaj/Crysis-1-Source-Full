#include "StdAfx.h"
#include "gameexporter.h"
#include "gameengine.h"
#include "PluginManager.h"
#include "texturecompression.h"
#include "DimensionsDialog.h"
#include "TerrainTexture.h"
#include "TerrainLighting.h"
#include "TerrainTexGen.h"
#include "SurfaceType.h"
#include "EquipPackLib.h"
#include "TopRendererWnd.h"
#include "ViewManager.h"
#include "VegetationObject.h"
#include "CryEditDoc.h"
#include "Mission.h"
#include "VegetationMap.h"

#include "Util\FileUtil.h"
#include "Objects\ObjectManager.h"
#include "Objects\BaseObject.h"
#include "Brush\BrushExporter.h"

#include "AnimationSerializer.h"
#include "Material\MaterialManager.h"
#include "Material\MaterialLibrary.h"
#include "Particles\ParticleManager.h"
#include "GameTokens\GameTokenManager.h"
#include "Music\MusicManager.h"
#include "BaseLibrary.h"

#include "ParticleExporter.h"

#include "Heightmap.h"		

#include "TerrainTexGen.h"
#include "TerrainBeachGen.h"

#include "ShaderCache.h"

#include <IAgent.h>
#include <IAISystem.h>
#include <I3dengine.h>
#include <ICryPak.h>
#include <IGame.h>
#include <IGameFramework.h>
#include <ILevelSystem.h>

//////////////////////////////////////////////////////////////////////////
#define MUSIC_LEVEL_LIBRARY_FILE "Music.xml"
#define GAMETOKENS_LEVEL_LIBRARY_FILE "LevelGameTokens.xml"
#define MATERIAL_LEVEL_LIBRARY_FILE "Materials.xml"
#define RESOURCE_LIST_FILE "ResourceList.txt"
#define SHADER_LIST_FILE "ShadersList.txt"

#define GetAValue(rgb)      ((BYTE)((rgb)>>24))


#pragma pack(push,1)
struct SAOInfo
{
	float fMax;
	float GetMax() { return fMax; }
	void SetMax(float fVal) { fMax = fVal; }
};
#pragma pack(pop)

const float gAOObjectsZRange = 32.f;
double gBuildSectorAODataTime=0;

const ETEX_Format eTerrainPrimaryTextureFormat = eTF_DXT5;
const ETEX_Format eTerrainSecondatyTextureFormat = eTF_DXT5;



//////////////////////////////////////////////////////////////////////////
CGameExporter::CGameExporter( ISystem *system )
{
	m_IEditor = GetIEditor();
	m_ISystem = system;
	m_I3DEngine = m_ISystem->GetI3DEngine();
	m_IAISystem = m_ISystem->GetAISystem();
	m_IEntitySystem = m_ISystem->GetIEntitySystem();

	m_bAutoExportMode = false;
	m_iExportTexWidth = 0;
	m_bHiQualityCompression = true;
	m_bDebugTexture = false;
	m_bUpdateIndirectLighting = false;
//	m_fTerrainGIClamp = 0.3f;
	m_ApplySS = 1;







}

CGameExporter::~CGameExporter(void)
{
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::SetAutoExportMode( bool bAuto )
{
	m_bAutoExportMode = bAuto;
	if (bAuto)
	{
		m_bHiQualityCompression = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::Export( bool bSurfaceTexture, bool bReloadTerrain, const bool bAI )
{
	SetCurrentDirectoryW( GetIEditor()->GetMasterCDFolder() );

	CAutoDocNotReady autoDocNotReady;
	CWaitCursor waitCursor;

	GetIEditor()->SetEditTool(0);		// close e.g. "layer painting tool" , to force reinit of the tool

	CString sLevelPath = Path::AddBackslash( GetIEditor()->GetGameEngine()->GetLevelPath() );
	
	m_levelName = GetIEditor()->GetGameEngine()->GetLevelName();
	m_levelPath = Path::RemoveBackslash(sLevelPath);

	if (bSurfaceTexture && !m_bAutoExportMode)
	{
//		if(GetISystem()->GetIConsole()->GetCVar("r_TextureCompressor")->GetIVal()==1)
//			AfxMessageBox("New texture compression was added (used only if high quality option is activated)\n"
//										"- this might take a bit longer but quality is better");

		CDimensionsDialog cDialog;

		// 4096x4096 is default
		cDialog.SetDimensions(4096);

		// Query the size of the preview
		if (cDialog.DoModal() == IDCANCEL)
			return;

		m_bHiQualityCompression = cDialog.GetCompressionQuality();
		m_bDebugTexture = cDialog.m_bDebugTexture;

		CCryEditDoc *pDocument = m_IEditor->GetDocument();
		LightingSettings *pSettings = pDocument->GetLighting();

		m_bUpdateIndirectLighting = cDialog.m_bUpdateIL;
		m_ApplySS = pSettings->iILApplySS;

		m_iExportTexWidth = cDialog.GetDimensions();
	}
	else if (m_bAutoExportMode)
	{
		m_iExportTexWidth = 16384; // Choose max quality.
		m_bHiQualityCompression = true;				// better quality, takes longer
		m_bDebugTexture = false;
		m_bUpdateIndirectLighting = true;
		m_ApplySS = 1;
	}

	m_missionName = m_IEditor->GetGameEngine()->GetMissionName();

	CString pakFilename = CString(sLevelPath) + "Level.pak";

	//////////////////////////////////////////////////////////////////////////
	// Make sure 3D engine closes texture handle.
	GetIEditor()->Get3DEngine()->CloseTerrainTextureFile();

	// Close this pak file.
	if (!m_ISystem->GetIPak()->ClosePack( pakFilename ))
	{
		Warning( "Export Failed!\r\nCannot Close Pak File %s",(const char*)pakFilename );
	}

	if (m_bAutoExportMode)
	{
		// Remove read-only flags.
		CrySetFileAttributes( pakFilename,FILE_ATTRIBUTE_NORMAL );
		if (bSurfaceTexture)
		{
			// If full export, then delete everything.
			// Delete old pak file.
			DeleteFile( pakFilename );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if (!CFileUtil::OverwriteFile(pakFilename))
		return;

	// Delete old pak file.
	//DeleteFile( pakFilename );

	if (!m_levelPakFile.Open( pakFilename ))
	{
		Warning( "Export Failed!\r\nCannot Open pak file %s for writing.",(const char*)pakFilename );
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// Start Export.
	//////////////////////////////////////////////////////////////////////////
	// Reset all animations before exporting.
	GetIEditor()->GetAnimation()->ResetAnimations(false,true);

	////////////////////////////////////////////////////////////////////////
	// Export all data to the game
	////////////////////////////////////////////////////////////////////////

	ExportMap(sLevelPath, bSurfaceTexture);

	////////////////////////////////////////////////////////////////////////
	// Export the cloud layer
	////////////////////////////////////////////////////////////////////////

	//m_IEditor->SetStatusText("Exporting cloud layer...");
	//CLogFile::WriteLine("Exportin Cloud layer...");

	////////////////////////////////////////////////////////////////////////
	// Export the heightmap, store shadow informations in it
	////////////////////////////////////////////////////////////////////////

	ExportHeightMap(sLevelPath);
	HEAP_CHECK

		////////////////////////////////////////////////////////////////////////
		// Exporting map setttings
		////////////////////////////////////////////////////////////////////////
		//ExportMapIni( sLevelPath );


		//////////////////////////////////////////////////////////////////////////
		// Export Particles.
	{
		CParticlesExporter partExporter;
		partExporter.ExportParticles( sLevelPath,m_levelPath,m_levelPakFile );
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//! Export Level data.
	CLogFile::WriteLine("Exporting LevelData.xml");
	ExportLevelData( sLevelPath );
	CLogFile::WriteLine("Exporting LevelData.xml done.");
	HEAP_CHECK

	ExportLevelInfo( sLevelPath );

	////////////////////////////////////////////////////////////////////////
	// Export the data of all plugin
	////////////////////////////////////////////////////////////////////////

	CLogFile::WriteLine("Exporting plugin data...");

	if (!m_IEditor->GetPluginManager()->CallExport(sLevelPath))
	{
		CLogFile::WriteLine("Error while exporting plugin data !");
		ASSERT(false);
		AfxMessageBox("Error while exporting plugin data !");
	}

	if(bAI)
		ExportAIGraph(sLevelPath);

	//////////////////////////////////////////////////////////////////////////
	// Start Movie System animations.
	//////////////////////////////////////////////////////////////////////////
	ExportAnimations(sLevelPath);

	//////////////////////////////////////////////////////////////////////////
	// Export Brushes.
	//////////////////////////////////////////////////////////////////////////
	ExportBrushes( sLevelPath );

	ExportLevelResourceList( sLevelPath );

	ExportLevelShaderCache( sLevelPath );

	//////////////////////////////////////////////////////////////////////////
	// End Exporting Game data.
	//////////////////////////////////////////////////////////////////////////

	// Close all packs.
	m_levelPakFile.Close();
	//	m_texturePakFile.Close();

	////////////////////////////////////////////////////////////////////////
	// Reload the level in the engine
	////////////////////////////////////////////////////////////////////////
	if (bReloadTerrain)
	{
		GetIEditor()->SetStatusText( _T("Reloading Level...") );
		m_IEditor->GetGameEngine()->LoadLevel( m_levelPath, m_missionName,false,false,false );
	}

	GetIEditor()->SetStatusText( _T("Ready") );
	// Reopen this pak file.
	if (!m_ISystem->GetIPak()->OpenPack( pakFilename ))
	{
		Warning( "Export Failed!\r\nCannot Open Pak File %s",(const char*)pakFilename );
		return;
	}

	// Commit changes to the disk.
	_flushall();

	// finally create filelist.xml
	//	SNH: disabled for now as we're using a simpler approach.
	//ExportFileList(sLevelPath);

	CLogFile::WriteLine("Exporting was successful.");
}



void CGameExporter::ExportMap( const char *pszGamePath, bool bSurfaceTexture)
{
	m_IEditor->ShowConsole( true );

	// Settings
	int iExportTexWidth       = m_iExportTexWidth; // Size of the exported surface texture
	const int iMapPreviewWidth      = 512;  // Size of the map preview

	// No need to generate texture if there are no layers or the caller does
	// not demand the texture to be generated
	if (m_IEditor->GetDocument()->GetLayerCount() == 0 || !bSurfaceTexture)
		return;

	CWaitCursor wait;

	CLogFile::FormatLine("Exporting data to game (%s)...", pszGamePath);


	////////////////////////////////////////////////////////////////////////
	// Export the surface texture
	////////////////////////////////////////////////////////////////////////

	CLogFile::WriteLine("Exporting Surface texture.");

	CHeightmap &HeightMap=m_IEditor->GetDocument()->GetHeightmap();

	m_IEditor->SetStatusText("Exporting surface texture (Generating)...");

	// Check dimensions
	if (HeightMap.GetWidth() != HeightMap.GetHeight() || HeightMap.GetWidth() % 128)
	{
		ASSERT(HeightMap.GetWidth() % 128 == 0);
		AfxMessageBox("Can't export a heightmap with dimensions that can't be" \
			" evenly divided by 128 or that are not square !");
		CLogFile::WriteLine("Can't export a heightmap");
		return;
	}

	uint t0 = GetTickCount();
	// Time how long does it take to generate Surface texture.
	{
		CString ctcFilename;
		ctcFilename.Format( "%sTerrain\\cover.ctc",pszGamePath );

		if(!ExportSurfaceTexture(ctcFilename))
			return;
	}

	HeightMap.SetSurfaceTextureSize( iExportTexWidth,iExportTexWidth );

	int t2 = GetTickCount();
	CLogFile::FormatLine( "Surface Texture Exported in %u seconds.",(t2-t0)/1000 );
}



//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportHeightMap( const char *pszGamePath)
{
	char szFileOutputPath[_MAX_PATH];

	// export compiled terrain
	CLogFile::WriteLine("Exporting terrain...");
	m_IEditor->SetStatusText("Exporting terrain...");
	HEAP_CHECK

  std::vector<struct CStatObj*> * pTempBrushTable = NULL;
  std::vector<struct IMaterial*> * pTempMatsTable = NULL;

	if(int nSize = GetIEditor()->Get3DEngine()->GetITerrain() ? GetIEditor()->Get3DEngine()->GetITerrain()->GetCompiledDataSize() : 0)
	{ // get terrain data from 3dengine and save it into file
		uchar * pData = new uchar[nSize];
		GetIEditor()->Get3DEngine()->GetITerrain()->GetCompiledData(pData, nSize, &pTempBrushTable, &pTempMatsTable);
		sprintf(szFileOutputPath, "%s%s", pszGamePath, COMPILED_HEIGHT_MAP_FILE_NAME);
		CCryMemFile hmapCompiledFile;
		hmapCompiledFile.Write( pData, nSize );
		m_levelPakFile.UpdateFile( szFileOutputPath, hmapCompiledFile, false );
		delete [] pData;
	}

	// export visareas
	CLogFile::WriteLine("Exporting indoors...");
	m_IEditor->SetStatusText("Exporting indoors...");
	HEAP_CHECK
	if(int nSize = GetIEditor()->Get3DEngine()->GetIVisAreaManager() ? GetIEditor()->Get3DEngine()->GetIVisAreaManager()->GetCompiledDataSize() : 0)
	{ // get visareas data from 3dengine and save it into file
		uchar * pData = new uchar[nSize];
		GetIEditor()->Get3DEngine()->GetIVisAreaManager()->GetCompiledData(pData, nSize, &pTempBrushTable, &pTempMatsTable);
		sprintf(szFileOutputPath, "%s%s", pszGamePath, COMPILED_VISAREA_MAP_FILE_NAME);
		CCryMemFile visareasCompiledFile;
		visareasCompiledFile.Write( pData, nSize );
		m_levelPakFile.UpdateFile( szFileOutputPath, visareasCompiledFile, false );
		delete [] pData;
	}

	HEAP_CHECK
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportAnimations( const CString &path )
{
	GetIEditor()->SetStatusText( _T("Exporting Animation Sequences...") );
	CLogFile::WriteLine("Export animation sequences...");
	CAnimationSerializer animSaver;
	animSaver.SaveAllSequences( path,m_levelPakFile );
	CLogFile::WriteString("Done.");

	/*
	char szPath[_MAX_PATH];

	strcpy( szPath,m_levelPath );
	PathAddBackslash(szPath);
	strcat( szPath,"Animation\\" );

	ISequenceIt *It=m_IEditor->GetMovieSystem()->GetSequences();
	IAnimSequence *seq=It->first();
	while (seq)
	{
		char szFile[_MAX_PATH];
		_makepath( szFile,0,szPath,seq->GetName(),"seq" );
		CAnimationSerializer animSerializer;
		animSerializer.SaveSequence( seq,szFile,false );
		seq=It->next();
	}
	It->Release();
	*/
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportSurfaceTypes( const CString &iniFile )
{
	/*
	std::vector<CString> detailObjects;
	int i;

	// Fill detail objects.
	for (i = 0; i < m_IEditor->GetDocument()->GetSurfaceTypeCount(); i++)
	{
		CSurfaceType *sf = m_IEditor->GetDocument()->GetSurfaceType(i);
		for (int j = 0; j < sf->GetDetailObjectCount(); j++)
		{
			if (std::find(detailObjects.begin(),detailObjects.end(),sf->GetDetailObject(j)) == detailObjects.end())
			{
				detailObjects.push_back( sf->GetDetailObject(j) );
			}
		}
	}

	// Write detail objects.
	char str[256];
	char str1[256];
	for (i = 0; i < detailObjects.size(); i++)
	{
		sprintf( str,"DetailObject%d_FileName",i );
		WritePrivateProfileString("DetailObjects", str, detailObjects[i], iniFile );
		sprintf( str,"DetailObject%d_Scale",i );
		WritePrivateProfileString("DetailObjects", str, "0.5",iniFile );
	}

	// Write surface types defenitions.
	for (i = 0; i < m_IEditor->GetDocument()->GetSurfaceTypeCount(); i++)
	{
		CSurfaceType *sf = m_IEditor->GetDocument()->GetSurfaceType(i);
		sprintf( str,"Surface%d_Texture",i );
		WritePrivateProfileString("SurfaceDefinition", str, sf->GetDetailTexture(), iniFile );
		sprintf( str,"Surface%d_DetailObjects",i );
		strcpy( str1,"" );
		for (int j = 0; j < sf->GetDetailObjectCount(); j++)
		{
			int id = std::find(detailObjects.begin(),detailObjects.end(),sf->GetDetailObject(j)) - detailObjects.begin();
			char str2[64];
			strcat( str1,itoa( id,str2,10 ) );
			strcat( str1,"," );
		}
		// remove last comma.
		int len1 = strlen(str1);
		if (len1 > 0)
		{
			str1[len1-1] = 0;
		}
		WritePrivateProfileString("SurfaceDefinition", str, str1, iniFile );
	}
	*/
}

void CGameExporter::ExportLevelData( const CString &path,bool bExportMission )
{
	GetIEditor()->SetStatusText( _T("Exporting LevelData.xml...") );

	int i;
	XmlNodeRef root = CreateXmlNode( "LevelData" );
	root->setAttr( "SandboxVersion",(const char*)GetIEditor()->GetFileVersion().ToFullString() );

	ExportMapInfo( root );

	//////////////////////////////////////////////////////////////////////////
	/// Export vegetation objects.
	XmlNodeRef vegetationNode = root->newChild( "Vegetation" );
	CVegetationMap *pVegetationMap = m_IEditor->GetVegetationMap();
	for (i = 0; i < pVegetationMap->GetObjectCount(); i++)
	{
		XmlNodeRef node = vegetationNode->newChild( "Object" );
		pVegetationMap->GetObject(i)->Serialize( node,false );
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Export materials.
	ExportMaterials( root,path );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Export particle manager.
	GetIEditor()->GetParticleManager()->Export( root );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Export Dymaic Music info.
	ExportMusic( root,path );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Export Level GameTokens.
	ExportGameTokens( root,path );
	//////////////////////////////////////////////////////////////////////////


	//XmlNodeRef objectsRoot = root->newChild( "Objects" );
	//m_IEditor->GetObjectManager()->Export( path,objectsRoot,true );

	// Save contents of current mission.	
	//m_IEditor->GetDocument()->GetCurrentMission()->SyncContent( false );

	CCryEditDoc *pDocument = m_IEditor->GetDocument();
	CMission *pCurrentMission = 0;

	if (bExportMission)
	{
		pCurrentMission = pDocument->GetCurrentMission();
		// Save contents of current mission.	
	}


	//////////////////////////////////////////////////////////////////////////
	// Export missions tag.
	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef missionsNode = root->newChild("Missions");
	CString missionFileName;
	CString currentMissionFileName;
	for (i = 0; i < pDocument->GetMissionCount(); i++)
	{
		CMission *pMission = pDocument->GetMission(i);

		CString name = pMission->GetName();
		name.Replace( ' ','_' );
		missionFileName.Format( "Mission_%s.xml",(const char*)name );

		ILevelSystem *pLevelSystem = gEnv->pGame->GetIGameFramework()->GetILevelSystem();
		int precacheCameraNumber = 0;

		if (pLevelSystem)
		{
			precacheCameraNumber = pLevelSystem->GetPrecacheCameraNumber();
		}

		XmlNodeRef missionDescNode = missionsNode->newChild("Mission");
		missionDescNode->setAttr( "Name",pMission->GetName() );
		missionDescNode->setAttr( "File",missionFileName );
		missionDescNode->setAttr( "CGFCount",m_I3DEngine->GetLoadedObjectCount() );
		missionDescNode->setAttr( "PrecacheCameraCount",precacheCameraNumber );

		int nProgressBarRange = m_numExportedMaterials/10 + m_I3DEngine->GetLoadedObjectCount();
		missionDescNode->setAttr( "ProgressBarRange",nProgressBarRange );

		if (pMission == pCurrentMission)
		{
			currentMissionFileName = missionFileName;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Save Level Data XML
	//////////////////////////////////////////////////////////////////////////
	CString levelDataFile = path + "LevelData.xml";
	//if (!CFileUtil::OverwriteFile( levelDataFile ))
	//return;
	XmlString xmlData = root->getXML();

	CCryMemFile file;
	file.Write( xmlData.c_str(),xmlData.length() );
	m_levelPakFile.UpdateFile( levelDataFile,file );

	if (bExportMission)
	{
		//////////////////////////////////////////////////////////////////////////
		// Export current mission file.
		//////////////////////////////////////////////////////////////////////////
		XmlNodeRef missionNode = root->createNode("Mission");
		pCurrentMission->Export( missionNode );

		missionNode->setAttr( "CGFCount",m_I3DEngine->GetLoadedObjectCount() );

		//if (!CFileUtil::OverwriteFile( path+currentMissionFileName ))
		//			return;

		_smart_ptr<IXmlStringData> pXmlStrData = missionNode->getXMLData( 5000000 );

		CCryMemFile file;
		file.Write( pXmlStrData->GetString(),pXmlStrData->GetStringLength() );
		m_levelPakFile.UpdateFile( path+currentMissionFileName,file );
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportLevelInfo( const CString &path )
{
	//////////////////////////////////////////////////////////////////////////
	// Export short level info xml.
	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef root = CreateXmlNode( "LevelInfo" );
	root->setAttr( "SandboxVersion",(const char*)GetIEditor()->GetFileVersion().ToFullString() );

	CString levelName = GetIEditor()->GetGameEngine()->GetLevelName();
	root->setAttr( "Name",levelName );
	root->setAttr( "HeightmapSize",GetIEditor()->GetHeightmap()->GetWidth() );

	// Save all missions in this level.
	XmlNodeRef missionsNode = root->newChild( "Missions" );
	int numMissions = GetIEditor()->GetDocument()->GetMissionCount();
	for (int i = 0; i < numMissions; i++)
	{
		CMission *pMission = GetIEditor()->GetDocument()->GetMission(i);
		XmlNodeRef missionNode = missionsNode->newChild( "Mission" );
		missionNode->setAttr( "Name",pMission->GetName() );
		missionNode->setAttr( "Description",pMission->GetDescription() );
	}


	//////////////////////////////////////////////////////////////////////////
	// Save LevelInfo file.
	//////////////////////////////////////////////////////////////////////////
	CString filename = path + "LevelInfo.xml";
	XmlString xmlData = root->getXML();

	CCryMemFile file;
	file.Write( xmlData.c_str(),xmlData.length() );
	m_levelPakFile.UpdateFile( filename,file );
}

void CGameExporter::ExportMapIni( const CString &path )
{
	/*
	m_IEditor->SetStatusText("Exporting map settings...");
	CLogFile::WriteLine("Map settings...");

	// Construct the filename of the map INI file
	char szFileOutputPath[1024];
	char szBuffer[1024];
	sprintf(szFileOutputPath, "%smap.ini", path);

	// Delete any old INI file
	if (PathFileExists(szFileOutputPath))
	{
		if (!m_IEditor->GetDocument()->FileDelete(szFileOutputPath))
		{
			AfxMessageBox("Can't rebuilt map.ini file !");
			CLogFile::WriteLine("Can't rebuilt map.ini file !");
		}
	}

	// Write the version of the file format
	VERIFY(WritePrivateProfileString("File", "MapFormatVersion", "1.0", szFileOutputPath));

	// Write the creation time of the file
	_strdate(szBuffer);
	VERIFY(WritePrivateProfileString("File", "MapCreationDate", szBuffer, szFileOutputPath));

	// Write the name of the map
	strcpy(szBuffer, LPCTSTR(m_IEditor->GetDocument()->GetTitle()));
	PathRemoveExtension(szBuffer);
	VERIFY(WritePrivateProfileString("Map", "MapName", szBuffer, szFileOutputPath));

	CHeightmap &HeightMap=m_IEditor->GetDocument()->GetHeightmap();
	// Write the size of the heightmap
	ASSERT(HeightMap.GetHeight() == HeightMap.GetWidth());
	sprintf(szBuffer, "%i", HeightMap.GetWidth());
	VERIFY(WritePrivateProfileString("Map", "HeightmapSize", szBuffer, szFileOutputPath));

	// Write the water level
	sprintf(szBuffer, "%i", (int)HeightMap.GetWaterLevel() );
	VERIFY(WritePrivateProfileString("Map", "MapWaterLevel", szBuffer, szFileOutputPath));

	// Write the water color.
	sprintf(szBuffer, "%x", m_IEditor->GetDocument()->GetWaterColor());
	VERIFY(WritePrivateProfileString("Map", "MapWaterColor", szBuffer, szFileOutputPath));

	//////////////////////////////////////////////////////////////////////////
	CLogFile::WriteLine("...Success");

	ExportSurfaceTypes( szFileOutputPath );
	*/
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportMapInfo( XmlNodeRef &node )
{
	XmlNodeRef info = node->newChild( "LevelInfo" );

	// Write the creation time of the file
	char szBuffer[1024];
	_strdate(szBuffer);
	info->setAttr( "CreationDate",szBuffer );

	strcpy(szBuffer, LPCTSTR(m_IEditor->GetDocument()->GetTitle()));
	PathRemoveExtension(szBuffer);
	info->setAttr( "Name",szBuffer );

	CHeightmap *heightmap = m_IEditor->GetHeightmap();
	if (heightmap)
	{
		info->setAttr( "HeightmapSize",heightmap->GetWidth() );
		info->setAttr( "HeightmapUnitSize",heightmap->GetUnitSize() );
		info->setAttr( "HeightmapMaxHeight",heightmap->GetMaxHeight() );
		info->setAttr( "WaterLevel",heightmap->GetWaterLevel() );

		SSectorInfo sectorInfo;
		heightmap->GetSectorsInfo( sectorInfo );
		int nTerrainSectorSizeInMeters = sectorInfo.sectorSize;
		info->setAttr( "TerrainSectorSizeInMeters",nTerrainSectorSizeInMeters);
	}

	// Serialize surface types.
	CXmlArchive xmlAr;
	xmlAr.bLoading = false;
	xmlAr.root = node;
	m_IEditor->GetDocument()->SerializeSurfaceTypes( xmlAr );
}




// "locally higher texture resolution"

//////////////////////////////////////////////////////////////////////////
bool CGameExporter::ExportSurfaceTexture( const char *szFilePathName )																			 
{

























	CHeightmap *heightmap = m_IEditor->GetHeightmap();

  int nMaxTilesNum = GetIEditor()->Get3DEngine()->GetTerrainSize()/
    GetIEditor()->Get3DEngine()->GetTerrainTextureNodeSizeMeters();

//	heightmap->GetTerrainGrid()->InitSectorGrid(heightmap->m_TerrainRGBTexture.GetTileCountX());		// release the editor textures on the terrain
  heightmap->GetTerrainGrid()->InitSectorGrid(nMaxTilesNum);		// release the editor textures on the terrain

//	heightmap->SetSurfaceTextureSize(m_iExportTexWidth,m_iExportTexWidth);

	SSectorInfo sectorInfo;
	heightmap->GetSectorsInfo( sectorInfo );
	int lSectorDimensions[2];	// [0]=layer 0, [1]=layer 1

	lSectorDimensions[0] = 256;																					// good texture size for streaming
	lSectorDimensions[1] = lSectorDimensions[0]/OCCMAP_DOWNSCALE_FACTOR;

	uint32 dwMinRequiredTextureExtend = heightmap->m_TerrainRGBTexture.CalcMinRequiredTextureExtend();

	// if the requested texture resolution is higher then the painted one, stick to this resolution
//	if(lSectorDimensions[0]*numSectors>dwMinRequiredTextureExtend)
//		lSectorDimensions[0] = dwMinRequiredTextureExtend/numSectors;

	m_IEditor->SetStatusText("Exporting surface texture (Saving)...");

	CLogFile::WriteLine("Generating surface texture...");

	CImage imSectorTexture[2];

	// layer 0
	imSectorTexture[0].Allocate( lSectorDimensions[0],lSectorDimensions[0] );

	// layer 1
	imSectorTexture[1].Allocate( lSectorDimensions[1],lSectorDimensions[1] );

	CCryMemFile ctcFile;

	// write header
	{
		SCommonFileHeader header;

		strncpy(header.signature, "CRY",sizeof(header.signature));
		header.type = eTerrainTextureFile;
		header.version = FILEVERSION_TERRAIN_TEXTURE_FILE;

		ctcFile.Write( &header,sizeof(header) );
	}

	// write sub header
	{
		STerrainTextureFileHeader subHeader;

//		subHeader.nSectorSizeMeters = (heightmap->GetUnitSize()*heightmap->GetWidth())/numSectors;
		subHeader.nLayerCount = 2;			// RGB and Occlusion texture
		subHeader.dwFlags = 0;
		if(m_bUpdateIndirectLighting)
			subHeader.dwFlags |= TTFHF_AO_DATA_IS_VALID;

		CCryEditDoc *pDocument = m_IEditor->GetDocument();
		LightingSettings *pSettings = pDocument->GetLighting();

		subHeader.m_fSunShadowIntensity = pSettings->iShadowIntensity*0.01f;		// 0..100 -> 0..1

		ctcFile.Write( &subHeader,sizeof(subHeader) );
	}

	STerrainTextureLayerFileHeader layerHeader0,layerHeader1;

	// layer 0
	layerHeader0.nSectorSizePixels = lSectorDimensions[0];
	if(!m_bDebugTexture)
	{
		layerHeader0.nSectorSizeBytes = ((lSectorDimensions[0]+3)/4)*((lSectorDimensions[0]+3)/4)*16;		// 16 is the DXT5 blocksize in bytes
		layerHeader0.eTexFormat=eTerrainPrimaryTextureFormat;
	}
	else
	{
		layerHeader0.nSectorSizeBytes = lSectorDimensions[0]*lSectorDimensions[0]*2;
		layerHeader0.eTexFormat=eTF_A4R4G4B4;
	}
	ctcFile.Write( &layerHeader0,sizeof(STerrainTextureLayerFileHeader) );

	// layer 1
	layerHeader1.nSectorSizePixels = lSectorDimensions[1];
	layerHeader1.eTexFormat=eTerrainSecondatyTextureFormat;
	layerHeader1.nSectorSizeBytes = m_ISystem->GetIRenderer()->GetTextureFormatDataSize(
		lSectorDimensions[1], lSectorDimensions[1], 1, 1, layerHeader1.eTexFormat);
	ctcFile.Write( &layerHeader1,sizeof(STerrainTextureLayerFileHeader) );

	// build index block - needed for locally higher/lower resolution
	uint32 dwUsedTextureIds=0;
	std::vector<int16> IndexBlock;		// >=0 means x is texture index, -1 is used as terminator
	{
		uint32 dwMaxTextureRes = heightmap->m_TerrainRGBTexture.CalcMinRequiredTextureExtend();

		_BuildIndexBlockRecursive(lSectorDimensions[0],IndexBlock,dwUsedTextureIds);

		if(m_bDebugTexture)
		{
			OutputDebugString("_BuildIndexBlockRecursive: ");

			std::vector<int16>::const_iterator it, end=IndexBlock.end();

			for(it=IndexBlock.begin();it!=end;++it)
			{
				int iVal=*it;

				char str[256];

				sprintf_s(str,sizeof(str),"%d ",iVal);
				OutputDebugString(str);
			}

			OutputDebugString("\n");
		}


		uint16 dwSize = (uint16)IndexBlock.size();
		ctcFile.Write( &dwSize,sizeof(uint16) );
		ctcFile.Write( &IndexBlock[0],sizeof(uint16)*dwSize );
	}


	ULONGLONG DataSeekPos = ctcFile.GetPosition();		// pos in file after header
	ULONGLONG ElementFileSize = layerHeader0.nSectorSizeBytes+layerHeader1.nSectorSizeBytes;

	CryLog("Generation stats: %d tiles(base: %dx%d) = %.2f MB",dwUsedTextureIds,lSectorDimensions[0],lSectorDimensions[0],(double)(ElementFileSize*dwUsedTextureIds)/(1024.0*1024.0));

	// reserve file size - 
	m_levelPakFile.GetArchive()->StartContinuousFileUpdate(szFilePathName,DataSeekPos + ElementFileSize*dwUsedTextureIds);

	// write header
	m_levelPakFile.GetArchive()->UpdateFileContinuousSegment(szFilePathName,DataSeekPos + ElementFileSize*dwUsedTextureIds,ctcFile.GetMemPtr(),ctcFile.GetLength());

	// release header memory
	ctcFile.Close();

	{
		CTerrainLayerTexGen	LayerTexGen;				// to generate the surface texture
		CTerrainLightGen LightGen(m_ApplySS, &m_levelPakFile, m_bUpdateIndirectLighting);// to generate the surface occlusion/lighting data

		LightGen.Init(heightmap->GetWidth(),false);

		CWaitProgress progress("Generating Surface Texture");

		int nSectorId = 0;

		SProgressHelper phelper(progress,dwUsedTextureIds,LightGen,ctcFile,IndexBlock,DataSeekPos,ElementFileSize,szFilePathName);

		phelper.m_dwLayerExtends[0] = lSectorDimensions[0];
		phelper.m_dwLayerExtends[1] = lSectorDimensions[1];

		int iIndexBlockValue = phelper.m_rIndexBlock[phelper.m_IndexBlockPos++];

		GetIEditor()->SetStatusText( _T("Export Surface-Texture recursive...") );

    gBuildSectorAODataTime = 0;

		if(!_ExportSurfaceTextureRecursive(phelper,iIndexBlockValue,phelper.m_TempMem[0].m_Quarter[0]))
			return false;

    m_IEditor->GetSystem()->GetILog()->Log("* Building AO data took %d seconds *", int(gBuildSectorAODataTime));

		GetIEditor()->SetStatusText( _T("Ready") );
	}


	CLogFile::WriteLine("Update surface texture file...");

	heightmap->m_TerrainRGBTexture.CleanupCache();		// clean up memory

	// close writing, open for reading, compute file CRC, open for writing, update file CRC
	CString sPakPath = m_levelPakFile.GetArchive()->GetFullPath();
	m_levelPakFile.Close();

	gEnv->pCryPak->OpenPack(sPakPath);
	uint32 dwCRC = gEnv->pCryPak->ComputeCRC(szFilePathName);
	gEnv->pCryPak->ClosePack(sPakPath);
	m_levelPakFile.Open(sPakPath);
	m_levelPakFile.GetArchive()->UpdateFileCRC(szFilePathName,dwCRC);
	m_levelPakFile.Close();

	// Reopen the file again, to restore previous open state.
	m_levelPakFile.Open(sPakPath);

	return true;
}



bool CGameExporter::_BuildIndexBlockRecursive( const uint32 dwTileSize, std::vector<int16> &rIndexBlock,
																							uint32 &dwUsedTextureIds, const float fMinX, const float fMinY, const uint32 dwRecursionDepth )
{
	CRGBLayer &TerrainRGBLayer = GetIEditor()->GetDocument()->m_cHeightmap.m_TerrainRGBTexture;

	float fInvSectorCnt = 1.0f/(float)(1<<dwRecursionDepth);

	uint32 dwRes = TerrainRGBLayer.CalcMaxLocalResolution(fMinX,fMinY,fMinX+fInvSectorCnt,fMinY+fInvSectorCnt);

	bool bDescent=true;

	if(dwRecursionDepth)
	{
		if( (dwRes>>dwRecursionDepth) < dwTileSize )
			bDescent=false;																	// data in input does not have more resolution

		if((dwTileSize<<dwRecursionDepth) > m_iExportTexWidth)
			bDescent=false;																	// user defined 
	}

//	char str[256];

//	sprintf(str,"_BuildIndexBlockRecursive %d. bDescent=%c %d>%d, dwRes=%d dwRecursionDepth=%d (%.2f %.2f)-(%.2f,%.2f)\n",dwUsedTextureIds,bDescent?'1':'0',((dwRes*2)>>dwRecursionDepth),dwTileSize,dwRes,dwRecursionDepth,fMinX,fMinY,fMinX+fInvSectorCnt,fMinY+fInvSectorCnt);
//	OutputDebugString(str);


	if(bDescent)
	{
		rIndexBlock.push_back(dwUsedTextureIds++);

		float fInvHalfSectorCnt=fInvSectorCnt*0.5f;

		// split in 4 parts, recursively proceed
    for(uint32 dwX=0;dwX<2;++dwX)
  		for(uint32 dwY=0;dwY<2;++dwY)
			{
				// recursion
				if(!_BuildIndexBlockRecursive(dwTileSize,rIndexBlock,dwUsedTextureIds,fMinX+dwX*fInvHalfSectorCnt,fMinY+dwY*fInvHalfSectorCnt,dwRecursionDepth+1))
					return false;

			}
	}
	else rIndexBlock.push_back(-1);

	return true;
}
/*
float GetAreaMaxZ(int x, int y)
{
	x = CLAMP(x, 0, gAODataSize-1);
	y = CLAMP(y, 0, gAODataSize-1);
	return gAOData[x+y*gAODataSize].GetMax();
}
*/
/*
float GetAreaMinZ(int x, int y)
{
	x = CLAMP(x, 0, gAODataSize-1);
	y = CLAMP(y, 0, gAODataSize-1);
	return gAOData[x+y*gAODataSize].GetMin();
}
*/


/*float GetSkyOcclusion(Vec3 vTestPos)
{
	static Vec3 arrKernel[256];
	static int nGenKernel = 1;
	static uchar ucCurrDir=0;

	if(nGenKernel)
	{
		for(int k=0; k<256; k++)
		{
			Vec3 vDir;
			vDir.x = (rnd()-0.5f)*2.f;
			vDir.y = (rnd()-0.5f)*2.f;
			vDir.z = 0.66f;
			vDir.Normalize();
			arrKernel[k] = vDir;
		}
		nGenKernel = 0;
	};

	float fHits=0;
	const int nRaysNum = 63;
	uint nMaxId = (gAODataSize-1)+(gAODataSize-1)*gAODataSize;
	for(int k=0; k<nRaysNum; k++)
	{
		Vec3 vStep = arrKernel[ucCurrDir++]*0.5f;
		vStep.z *= fAOInfoZRatio;

		Vec3 vPos = vTestPos + Vec3(0,0,2.f);
		vPos.z *= fAOInfoZRatio;

		const int nSamplesNum=16;
		for(int nSampleId=0; nSampleId<nSamplesNum; nSampleId++)
		{
			vPos += vStep;
			vStep *= 1.1f;

			uint x=vPos.x;
			uint y=vPos.y;
			uint nId = min(x+y*gAODataSize,nMaxId);
			SAOInfo * pInf = &gAOData[nId];
			ushort ucZ = vPos.z;
			if((ucZ>pInf->ucMin && ucZ<pInf->ucMax))
			{
				fHits += SATURATE((pInf->ucMax - ucZ)*fAOInfoZRatio_1*0.25f);
				break;
			}
		}
	}

	return fHits/(float)nRaysNum;
}*/

float GetFilteredNoiseVal(int X, int Y)
{
	static float arrNoise2D[256][256];

	static int nBuildNoiseMap = true;
	if(nBuildNoiseMap)
	{
		srand(0);

		// generate
		for(int x=0; x<256; x++)
		{
			for(int y=0; y<256; y++)
			{
				arrNoise2D[x][y] = ((float)rand()/RAND_MAX)-0.5f;
			}
		}

		// blur
		static float arrNoise2DTmp[256][256];
		for(int pass=0; pass<8; pass++)
		{
			memcpy(arrNoise2DTmp,arrNoise2D,sizeof(arrNoise2DTmp));

			for(int x=0; x<256; x++)
			{
				for(int y=0; y<256; y++)
				{
					float fVal = 0;

					fVal += arrNoise2DTmp[(x  )&255][(y+1)&255]*0.6f;
					fVal += arrNoise2DTmp[(x  )&255][(y-1)&255]*0.6f;
					fVal += arrNoise2DTmp[(x+1)&255][(y  )&255]*0.6f;
					fVal += arrNoise2DTmp[(x-1)&255][(y  )&255]*0.6f;

					fVal += arrNoise2DTmp[(x  )&255][(y  )&255];
					arrNoise2D[x][y] = fVal/(0.6f*4.f + 1.f);
				}
			}
		}

		nBuildNoiseMap = false;
	}

	return arrNoise2D[X&255][Y&255];
}

bool CGameExporter::_ExportSurfaceTextureRecursive( SProgressHelper &phelper, const int iParentIndex, SRecursionHelperQuarter &rDestPiece,
																									 const float fMinX, const float fMinY, const uint32 dwRecursionDepth )
{
	// to jump over one mip level
	uint32 dwMipSectorCount = 1<<(dwRecursionDepth+1);

	SSectorInfo sectorInfo;
	CVegetationMap *pVegetation = GetIEditor()->GetVegetationMap();
	CHeightmap *pHeightmap = GetIEditor()->GetHeightmap();
	GetIEditor()->GetHeightmap()->GetSectorsInfo( sectorInfo );
	
//	char str[256];

//	sprintf(str,"_ExportSurfaceTextureRecursive iParentIndex=%d dwRecursionDepth=%d\n",iParentIndex,dwRecursionDepth);
//	OutputDebugString(str);


	//SRecursionHelper &rHelper = phelper.m_TempMem[dwRecursionDepth+1];

	bool bLeafNode = true;

	// split in 4 parts, recursively proceed
  if(iParentIndex!=-1)
	for(uint32 dwI=0;dwI<4;++dwI)
	{	
		int iIndexBlockValue = phelper.m_rIndexBlock[phelper.m_IndexBlockPos+dwI];

		if(iIndexBlockValue>=0)
		{
			bLeafNode=false;
			break;
		}
	}

	float fInvSectorCnt = 1.0f/(float)(1<<dwRecursionDepth);

	if(bLeafNode)
	{
//		OutputDebugString("Leaf\n");

		// layer 1 -----------------
		{
			CImage &ref = rDestPiece.m_Layer[1];

			if(!ref.IsValid())
				ref.Allocate(phelper.m_dwLayerExtends[1],phelper.m_dwLayerExtends[1]);

			phelper.m_rLightGen.GetSubImageStretched(fMinX,fMinY,fMinX+fInvSectorCnt,fMinY+fInvSectorCnt,ref,
				ETTG_QUIET|ETTG_LIGHTING|ETTG_STATOBJ_SHADOWS|ETTG_STATOBJ_PAINTBRIGHTNESS|ETTG_ABGR);
		}

		// layer 0 -----------------
		{
			CImage &ref = rDestPiece.m_Layer[0];

			if(!ref.IsValid())
				ref.Allocate(phelper.m_dwLayerExtends[0],phelper.m_dwLayerExtends[0]);

			CRGBLayer &TerrainRGBLayer = GetIEditor()->GetDocument()->m_cHeightmap.m_TerrainRGBTexture;

			TerrainRGBLayer.GetSubImageStretched(fMinX,fMinY,fMinX+fInvSectorCnt,fMinY+fInvSectorCnt,ref,true);
		}

		// convert RGB colour into format that has less compression artefacts for brightness variations
		{
			uint32 dwWidth = rDestPiece.m_Layer[0].GetWidth();
			uint32 dwHeight = rDestPiece.m_Layer[0].GetHeight();

			uint32 *pMem = &rDestPiece.m_Layer[0].ValueAt(0,0);
			uint32 *pMemAO = &rDestPiece.m_Layer[1].ValueAt(0,0);

			float fWSMinX = fMinX * m_I3DEngine->GetTerrainSize();
			float fWSMinY = fMinY * m_I3DEngine->GetTerrainSize();
			float fWSSectorSizeMeters = m_I3DEngine->GetTerrainSize()*fInvSectorCnt;

			// find terrain min/max
			float fTerrainMinZ=1000000;
			float fTerrainMaxZ=0;
			for(uint32 dwY=0;dwY<dwHeight;++dwY)
			{
				for(uint32 dwX=0;dwX<dwWidth;++dwX)
				{
					float fWSDX = ((((float)dwX))*(1.f+1.f/256.f)-0.5f)/(float)dwWidth*(float)fWSSectorSizeMeters;
					float fWSDY = ((((float)dwY))*(1.f+1.f/256.f)-0.5f)/(float)dwHeight*(float)fWSSectorSizeMeters;

					Vec3 vWSPos; // pos on terrain
					vWSPos.x = (fWSMinY+fWSDY);
					vWSPos.y = (fWSMinX+fWSDX);
					vWSPos.z = m_I3DEngine->GetTerrainElevation(int(vWSPos.x),int(vWSPos.y));

					fTerrainMaxZ = max(fTerrainMaxZ,vWSPos.z);
					fTerrainMinZ = min(fTerrainMinZ,vWSPos.z);
				}
			}

			// build sector AO data
			int nAODataSize = 256;
			SAOInfo * pAOData = NULL;
			if(m_bUpdateIndirectLighting)
			{
				pAOData = new SAOInfo[nAODataSize*nAODataSize];
				BuildSectorAOData(fWSMinX, fWSMinY, fWSSectorSizeMeters, pAOData, nAODataSize);
			}


			for(uint32 dwY=0;dwY<dwHeight;++dwY)
			{
				for(uint32 dwX=0;dwX<dwWidth;++dwX)
				{
					// tex0
					uint32 dwR,dwG,dwB;

#ifdef TERRAIN_USE_CIE_COLORSPACE
					float fR = ((*pMem>>16)&0xff)*(1.0f/255.0f);
					float fG = ((*pMem>>8)&0xff)*(1.0f/255.0f);
					float fB = ((*pMem)&0xff)*(1.0f/255.0f);

					ColorF cCIE = ColorF(fR,fG,fB).RGB2mCIE();

					dwR = (uint32)(cCIE.r*255.0f+0.5f);
					dwG = (uint32)(cCIE.g*255.0f+0.5f);
					dwB = (uint32)(cCIE.b*255.0f+0.5f);
#else
					dwR = (*pMem)&0xff;
					dwG = (*pMem>>8)&0xff;
					dwB = (*pMem>>16)&0xff;
#endif

					// get world pos
					float fWSDX = ((((float)dwX))*(1.f+1.f/256.f)-0.5f)/(float)dwWidth*(float)fWSSectorSizeMeters;
					float fWSDY = ((((float)dwY))*(1.f+1.f/256.f)-0.5f)/(float)dwHeight*(float)fWSSectorSizeMeters;
					Vec3 vWSPos; // pos on terrain
					vWSPos.x = (fWSMinY+fWSDY);
					vWSPos.y = (fWSMinX+fWSDX);
					vWSPos.z = m_I3DEngine->GetTerrainElevation(int(vWSPos.x),int(vWSPos.y));

					// store ground level sky access
					float fSkyAmount = (((*pMemAO)>>8)&0xff)/255.f;
					// add some filtered noise
					fSkyAmount += GetFilteredNoiseVal(vWSPos.x*4.f,vWSPos.y*4.f)*SATURATE(1.f-fSkyAmount)*0.5f; 
					uint32 dwA = SATURATEB(fSkyAmount*255.5f);

					*pMem = (dwA<<24) | (dwR<<16) | (dwG<<8) | (dwB);

          dwR = dwG = dwB = dwA = 255;

					if(m_bUpdateIndirectLighting)
					{ // tex1
						// store objects elevation z
						if(dwY>=0 && dwY<nAODataSize && dwX>=0 && dwX<nAODataSize)
						{
							float fObjZ = pAOData[dwY+dwX*nAODataSize].GetMax();
							dwG = SATURATEB((fObjZ-vWSPos.z)/gAOObjectsZRange*255.f);
						}

						// store terrain z
						int nH = SATURATE((vWSPos.z-fTerrainMinZ)/(fTerrainMaxZ-fTerrainMinZ))*256.f*256.f; // from 0 to 255
						dwA = SATURATEB(nH/256);
          }

          // store normal in R and B
          Vec3 vNormal = m_I3DEngine->GetTerrainSurfaceNormal(vWSPos);
          dwR = SATURATEB(vNormal.x*127.5f+127.5f);
          dwB = SATURATEB(vNormal.y*127.5f+127.5f);

					*pMemAO = (dwA<<24) | (dwB<<16) | (dwG<<8) | (dwR);					

					pMemAO++;				
					pMem++;
				}
			}

			delete [] pAOData; pAOData = NULL;

			rDestPiece.fTerrainMinZ = fTerrainMinZ;
			rDestPiece.fTerrainMaxZ = fTerrainMaxZ;
		}

    if(iParentIndex!=-1)
  		phelper.m_IndexBlockPos+=4;
	}
	else
	{
//		OutputDebugString("Nodes:\n");


		// split in 4 parts, recursively proceed
    for(uint32 dwX=0;dwX<2;++dwX)
  		for(uint32 dwY=0;dwY<2;++dwY)
			{	
				uint32 dwPart=dwX+dwY*2; // 0=lefttop,1=righttop,2=leftbottom,3=rightbottom

				int iIndexBlockValue = phelper.m_rIndexBlock[phelper.m_IndexBlockPos++];

//				char str[256];

//				sprintf(str,"   Node iIndexBlockValue = m_rIndexBlock[%d/%d] = %d\n",phelper.m_IndexBlockPos-1,(uint32)phelper.m_rIndexBlock.size(),iIndexBlockValue);
//				OutputDebugString(str);

				float fInvHalfSectorCnt=fInvSectorCnt*0.5f;

				// recursion
				if(!_ExportSurfaceTextureRecursive(phelper,iIndexBlockValue,phelper.m_TempMem[dwRecursionDepth+1].m_Quarter[dwPart],
					fMinX+dwX*fInvHalfSectorCnt,fMinY+dwY*fInvHalfSectorCnt,dwRecursionDepth+1))
					return false;

				assert(phelper.m_TempMem[dwRecursionDepth+1].m_Quarter[dwPart].m_Layer[0].IsValid());
				assert(phelper.m_TempMem[dwRecursionDepth+1].m_Quarter[dwPart].m_Layer[1].IsValid());
			}

		//SRecursionHelper &rHelper = phelper.m_TempMem[dwRecursionDepth];

		for(uint32 dwLayer=0;dwLayer<2;++dwLayer)
		{
			CImage InputQuarters[4];
			float arrTerrainMinZ[4];
			float arrTerrainMaxZ[4];

			// Attach only copies pointers, not the data itself
			for(uint32 dwPart=0;dwPart<4;++dwPart)
			{
				InputQuarters[dwPart].Attach(phelper.m_TempMem[dwRecursionDepth+1].m_Quarter[dwPart].m_Layer[dwLayer]);

				arrTerrainMinZ[dwPart] = phelper.m_TempMem[dwRecursionDepth+1].m_Quarter[dwPart].fTerrainMinZ;
				arrTerrainMaxZ[dwPart] = phelper.m_TempMem[dwRecursionDepth+1].m_Quarter[dwPart].fTerrainMaxZ;
/*
				if(m_bDebugTexture)
				if(dwLayer==0)
				{
					char str[80];
					sprintf(str,"C:\\temp\\input%d.bmp",dwPart);
					CImageUtil::SaveBitmap(str,InputQuarters[dwPart]);
				}
*/
			}

			if(!rDestPiece.m_Layer[dwLayer].IsValid())
				rDestPiece.m_Layer[dwLayer].Allocate(phelper.m_dwLayerExtends[dwLayer],phelper.m_dwLayerExtends[dwLayer]);

			if(dwLayer==0)
				DownSampleWithBordersPreserved(InputQuarters,rDestPiece.m_Layer[dwLayer]);
			else
				DownSampleWithBordersPreservedAO(InputQuarters,arrTerrainMinZ,arrTerrainMaxZ,rDestPiece.m_Layer[dwLayer]);
		}
	}


	if(iParentIndex!=-1)
	{
		assert(iParentIndex>=0);

//		char str[256];
//		sprintf(str,"Store %d\n",iParentIndex);
//		OutputDebugString(str);

//		if(iParentIndex==0)
//		{
//			OutputDebugString("Root\n");
//		}

		if(!SeekToSectorAndStore(phelper,iParentIndex,dwRecursionDepth,rDestPiece))
			return false;
	}

	return true;
}



bool CGameExporter::SeekToSectorAndStore( SProgressHelper &phelper, const int iParentIndex,
	const uint32 dwLevel, SRecursionHelperQuarter &rQuarter )
{
	assert(iParentIndex>=0);


	// debug border diffuse (black edges)
	/*{
		for(uint32 dwY=0;dwY<rQuarter.m_Layer[0].GetHeight();++dwY)
		for(uint32 dwX=0;dwX<rQuarter.m_Layer[0].GetWidth();++dwX)
		{
			bool bBorder = dwX==0 || dwX==rQuarter.m_Layer[0].GetWidth()-1 || dwY==0 || dwY==rQuarter.m_Layer[0].GetHeight()-1;

			if(bBorder)
				rQuarter.m_Layer[0].ValueAt(dwX,dwY) = 0xffffffff;
		}
	}

	// debug border occ (black edges)
	{
		for(uint32 dwY=0;dwY<rQuarter.m_Layer[1].GetHeight();++dwY)
			for(uint32 dwX=0;dwX<rQuarter.m_Layer[1].GetWidth();++dwX)
			{
				bool bBorder = dwX==0 || dwX==rQuarter.m_Layer[1].GetWidth()-1 || dwY==0 || dwY==rQuarter.m_Layer[1].GetHeight()-1;

				if(bBorder)
					rQuarter.m_Layer[1].ValueAt(dwX,dwY) = 0xffffffff;
			}	
	}*/


	if(m_bDebugTexture)
	{
		char str[80];
		sprintf(str,"C:\\temp\\seekstore_%d.bmp",iParentIndex);
		CImageUtil::SaveBitmap(str,rQuarter.m_Layer[0]);
			//CImageUtil::SaveBitmap(str,rQuarter.m_Layer[1]);
	}


//	phelper.m_toFile.Seek(phelper.m_FileTileOffset + phelper.m_ElementFileSize*iParentIndex,CFile::begin);

	CTextureCompression texcomp(m_bHiQualityCompression);

//	char str[256];
//	sprintf(str,"CGameExporter::SeekToSectorAndStore (%dx%d) parent:%d level:%d [%d+%d*%d]\n",rQuarter.m_Layer[0].GetWidth(),rQuarter.m_Layer[0].GetHeight(),iParentIndex,dwLevel,(uint32)phelper.m_FileTileOffset,(uint32)phelper.m_ElementFileSize,iParentIndex);
//	OutputDebugString(str);

	CCryMemFile fileTemp;		// this could be improved - local allocation could be avoided

	// layer 0 -----------------
	ETEX_Format fmtLayer0 = m_bDebugTexture ? eTF_A4R4G4B4 : eTerrainPrimaryTextureFormat;

	Vec3 vLumWeight(0,0,0);

	// convert RGB colour into format that has less compression artefacts for brightness variations
	{
		vLumWeight = Vec3(0.3f,0.3f,1.0f);
		vLumWeight /= vLumWeight.x+vLumWeight.y+vLumWeight.z;
	}

	texcomp.WriteDDSToFile(fileTemp,(uint8 *)rQuarter.m_Layer[0].GetData(),rQuarter.m_Layer[0].GetWidth(),rQuarter.m_Layer[0].GetHeight(),rQuarter.m_Layer[0].GetSize(),eTF_A8R8G8B8,fmtLayer0,0,false,vLumWeight,false);

	// layer 1 -----------------
	// no dithering, convert from green channel 32 bit to 1 channel
	texcomp.WriteDDSToFile(fileTemp,(uint8 *)rQuarter.m_Layer[1].GetData(),rQuarter.m_Layer[1].GetWidth(),rQuarter.m_Layer[1].GetHeight(),rQuarter.m_Layer[1].GetSize(),eTF_A8R8G8B8,eTerrainSecondatyTextureFormat,0,false,Vec3(0,0,0),false);

	// seek to destination and update file there
	if(0!=m_levelPakFile.GetArchive()->UpdateFileContinuousSegment(phelper.m_szFilename,phelper.m_FileTileOffset+ phelper.m_ElementFileSize*phelper.m_Max,
		fileTemp.GetMemPtr(),fileTemp.GetLength(),phelper.m_FileTileOffset + phelper.m_ElementFileSize*iParentIndex))
	{
		// error
		assert(0);
	}

	return phelper.Increase();
}








void CGameExporter::DownSampleWithBordersPreserved( const CImage rIn[4], CImage &rOut )
{
	uint32 dwOutputWidth=rOut.GetWidth();
	uint32 dwOutputHeight=rOut.GetHeight();

	assert(rIn[0].GetData()!=rOut.GetData());
	assert(rIn[1].GetData()!=rOut.GetData());
	assert(rIn[2].GetData()!=rOut.GetData());
	assert(rIn[3].GetData()!=rOut.GetData());

	// if needed this can be optimized a lot

	// lefttop
	{
		for(uint32 dwY=0;dwY<dwOutputHeight;++dwY)
		for(uint32 dwX=0;dwX<dwOutputWidth;++dwX)
		{
			uint32 dwPart=0;
			uint32 dwLocalInX=dwX*2, dwLocalInY=dwY*2;

			if(dwLocalInX>=dwOutputWidth)
			{
				dwPart=1;dwLocalInX-=dwOutputWidth;
			}
			
			if(dwLocalInY>=dwOutputHeight)
			{
				dwPart+=2;dwLocalInY-=dwOutputHeight;
			}

			if(dwX==0)
			{
				if(dwY==0)
					rOut.ValueAt(dwX,dwY) = rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY);					// left top corner
				else if(dwY==dwOutputHeight-1)
					rOut.ValueAt(dwX,dwY) = rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY+1);				// left bottom corner
				else
				{																																							// left side
					rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY),rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY+1) );
				}
			}
			else if(dwX==dwOutputWidth-1)
			{
				if(dwY==0)
					rOut.ValueAt(dwX,dwY) = rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY);					// right top corner
				else if(dwY==dwOutputHeight-1)
					rOut.ValueAt(dwX,dwY) = rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY+1);				// right bottom corner
				else
				{																																								// right side
					rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY),rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY+1) );
				}
			}
			else
			{
				if(dwY==0)
				{
					rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY),rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY) );
				}
				else if(dwY==dwOutputHeight-1)
				{
					rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY+1),rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY+1) );
				}
				else
				{																																							// inner
					uint32 dwC[2];

					dwC[0] = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY),rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY) );
					dwC[1] = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY+1),rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY+1) );

					rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( dwC[0],dwC[1] );
				}
			}
		}
	}
}

static uint32 ComputeAvgCol_AO( const uint32 dwCol0, const uint32 dwCol1, 
															 float fTMZ_InMin, float fTMZ_InMax, 
															 float fTMZ_OutMin, float fTMZ_OutMax)
{
	uint32 arrZOut[4];

	for(int i=0; i<4; i++)
	{
		if(i==3) // terrain elevation needs to be range converted
		{
			float fZIn0 = ((dwCol0>>(8*i))&0xff)/255.f*(fTMZ_InMax-fTMZ_InMin) + fTMZ_InMin;
			float fZIn1 = ((dwCol1>>(8*i))&0xff)/255.f*(fTMZ_InMax-fTMZ_InMin) + fTMZ_InMin;
			float fZOut = (fZIn0+fZIn1)*0.5f;
			arrZOut[i] = SATURATEB((fZOut - fTMZ_OutMin)/(fTMZ_OutMax - fTMZ_OutMin)*255.f + 0.5f);
		}
		else
		{
			float fZIn0 = ((dwCol0>>(8*i))&0xff);
			float fZIn1 = ((dwCol1>>(8*i))&0xff);
			float fZOut = (fZIn0+fZIn1)*0.5f;
			arrZOut[i] = SATURATEB(fZOut);
		}
	}

	uint32 dwRes = (arrZOut[3]<<24) | (arrZOut[2]<<16) | (arrZOut[1]<<8) | (arrZOut[0]);

	return dwRes;
}

void CGameExporter::DownSampleWithBordersPreservedAO( const CImage rIn[4], 
	const float rInTerrainMinZ[4], const float rInTerrainMaxZ[4], CImage &rOut )
{
	uint32 dwOutputWidth=rOut.GetWidth();
	uint32 dwOutputHeight=rOut.GetHeight();

	assert(rIn[0].GetData()!=rOut.GetData());
	assert(rIn[1].GetData()!=rOut.GetData());
	assert(rIn[2].GetData()!=rOut.GetData());
	assert(rIn[3].GetData()!=rOut.GetData());

	// if needed this can be optimized a lot

	float fNewZMin = rInTerrainMinZ[0];
	float fNewZMax = rInTerrainMaxZ[0];
	for(int i=0; i<4; i++)
	{
		fNewZMin = min(fNewZMin,rInTerrainMinZ[i]);
		fNewZMax = max(fNewZMax,rInTerrainMaxZ[i]);
	}

	// lefttop
	{
		for(uint32 dwY=0;dwY<dwOutputHeight;++dwY)
			for(uint32 dwX=0;dwX<dwOutputWidth;++dwX)
			{
				uint32 dwPart=0;
				uint32 dwLocalInX=dwX*2, dwLocalInY=dwY*2;

				if(dwLocalInX>=dwOutputWidth)
				{
					dwPart=1;dwLocalInX-=dwOutputWidth;
				}

				if(dwLocalInY>=dwOutputHeight)
				{
					dwPart+=2;dwLocalInY-=dwOutputHeight;
				}

/*				if(dwX==0)
				{
					if(dwY==0)
						rOut.ValueAt(dwX,dwY) = rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY);					// left top corner
					else if(dwY==dwOutputHeight-1)
						rOut.ValueAt(dwX,dwY) = rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY+1);				// left bottom corner
					else
					{																																							// left side
						rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY),rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY+1) );
					}
				}
				else if(dwX==dwOutputWidth-1)
				{
					if(dwY==0)
						rOut.ValueAt(dwX,dwY) = rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY);					// right top corner
					else if(dwY==dwOutputHeight-1)
						rOut.ValueAt(dwX,dwY) = rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY+1);				// right bottom corner
					else
					{																																								// right side
						rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY),rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY+1) );
					}
				}
				else*/
				{
/*					if(dwY==0)
					{
						rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY),rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY) );
					}
					else if(dwY==dwOutputHeight-1)
					{
						rOut.ValueAt(dwX,dwY) = ColorB::ComputeAvgCol_Fast( rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY+1),rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY+1) );
					}
					else*/
					{																																							// inner
						uint32 dwC[2];

						dwC[0] = ComputeAvgCol_AO( 
							rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY), 
							rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY), 
							rInTerrainMinZ[dwPart], rInTerrainMaxZ[dwPart], fNewZMin, fNewZMax );
						dwC[1] = ComputeAvgCol_AO( 
							rIn[dwPart].ValueAt(dwLocalInX,dwLocalInY+1), rIn[dwPart].ValueAt(dwLocalInX+1,dwLocalInY+1), 
							rInTerrainMinZ[dwPart], rInTerrainMaxZ[dwPart], fNewZMin, fNewZMax );

						rOut.ValueAt(dwX,dwY) = ComputeAvgCol_AO( dwC[0], dwC[1], fNewZMin, fNewZMax, fNewZMin, fNewZMax );
					}
				}
			}
	}
}


//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportAIGraph( const CString &path )
{ 
	if ( !m_IEditor->GetSystem()->GetAISystem() )
		return;

	// AI file will be generated individually
//	GetIEditor()->GetGameEngine()->GenerateAiTriangulation();

	GetIEditor()->SetStatusText( _T("Exporting AI Graph...") );

	char szLevel[1024];
	char szMission[1024];
	char fileNameNav[1024];
	char fileNameVerts[1024];
	char fileNameHide[1024];
	char fileNameVolume[1024];
	char fileNameAreas[1024];
	char fileNameFlight[1024];
	char fileNameRoads[1024];
	char fileNameWaypoint3DSurface[1024];
	strcpy( szLevel,path );
	strcpy( szMission, m_IEditor->GetDocument()->GetCurrentMission()->GetName() );
	PathRemoveBackslash(szLevel);
	sprintf(fileNameNav,"%s\\net%s.bai",szLevel,szMission);
	sprintf(fileNameVerts,"%s\\verts%s.bai",szLevel,szMission);
	sprintf(fileNameHide,"%s\\hide%s.bai",szLevel,szMission);
	sprintf(fileNameVolume, "%s\\v3d%s.bai",szLevel,szMission);
	sprintf(fileNameAreas, "%s\\areas%s.bai",szLevel,szMission);
	sprintf(fileNameFlight, "%s\\fnav%s.bai",szLevel,szMission);
	sprintf(fileNameRoads, "%s\\roadnav%s.bai",szLevel,szMission);
	sprintf(fileNameWaypoint3DSurface, "%s\\waypt3Dsfc%s.bai",szLevel,szMission);
	
  if (m_IEditor->GetSystem()->GetAISystem()->GetNodeGraph())
	{
		m_IEditor->GetSystem()->GetAISystem()->ExportData(fileNameNav, fileNameHide, fileNameAreas, fileNameRoads, fileNameWaypoint3DSurface);
		m_IEditor->GetSystem()->GetAISystem()->GetNodeGraph()->Validate("Before updating pak", true);

		// Read theose files back and put them to Pak file.
		CFile file;
		if (file.Open( fileNameNav,CFile::modeRead ))
		{
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileNameNav,mem ))
				Warning( "Failed to update pak file with %s", fileNameNav);
			else
				DeleteFile( fileNameNav );
		}
		if (file.Open( fileNameVerts,CFile::modeRead ))
		{
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileNameVerts,mem ))
				Warning( "Failed to update pak file with %s", fileNameVerts);
			else
				DeleteFile( fileNameVerts );
		}
		if (file.Open( fileNameHide,CFile::modeRead ))
		{
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileNameHide,mem ))
				Warning( "Failed to update pak file with %s", fileNameHide);
			else
				DeleteFile( fileNameHide );
		}
		if (file.Open( fileNameVolume,CFile::modeRead ))
		{
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileNameVolume,mem ))
				Warning( "Failed to update pak file with %s", fileNameVolume);
			else
				DeleteFile( fileNameVolume );
		}
		if (file.Open( fileNameAreas,CFile::modeRead ))
		{
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileNameAreas,mem ))
				Warning( "Failed to update pak file with %s", fileNameAreas);
			else
				DeleteFile( fileNameAreas );
		}
		if (file.Open( fileNameFlight,CFile::modeRead ))
		{
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileNameFlight,mem ))
				Warning( "Failed to update pak file with %s", fileNameFlight);
			else
				DeleteFile( fileNameFlight );
		}
		if (file.Open( fileNameRoads,CFile::modeRead ))
		{
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileNameRoads,mem ))
				Warning( "Failed to update pak file with %s", fileNameRoads);
			else
				DeleteFile( fileNameRoads );
		}
		if (file.Open( fileNameWaypoint3DSurface,CFile::modeRead ))
		{
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileNameWaypoint3DSurface,mem ))
				Warning( "Failed to update pak file with %s", fileNameWaypoint3DSurface);
			else
				DeleteFile( fileNameWaypoint3DSurface );
		}
	} 

  const std::vector<string> & fileNames = m_IEditor->GetSystem()->GetAISystem()->GetVolumeRegionFiles(szLevel, szMission);

  for (unsigned i = 0 ; i < fileNames.size(); ++i)
  {
    const string& fileName = fileNames[i];

    CFile file;
	  if (file.Open( fileName, CFile::modeRead ))
	  {
			CMemoryBlock mem;
			mem.Allocate( file.GetLength() );
			file.Read( mem.GetBuffer(),file.GetLength() );
			file.Close();
			if (false == m_levelPakFile.UpdateFile( fileName,mem ))
				Warning( "Failed to update pak file with %s", fileName);
			else
				DeleteFile( fileName );
	  }
  }

	CLogFile::WriteLine( "Exporting AI Graph done." );
}

/*
//////////////////////////////////////////////////////////////////////////
void CGameExporter::FlattenHeightmap( uint16 *pSaveHeightmapData,int width,int height )
{
	float fDist,fMaxDist;
	int iX, iY;
	float fAttenuation;
	int i,j;
	int iStartPosX, iStartPosY;
	Vec3 op;

	std::vector<CBaseObject*> objects;
	m_IEditor->GetObjectManager()->GetObjects( objects );

		// Mark holes and used flag.
	for (j=0; j<height; j++)
	{
		for (i=0; i<width; i++)
		{
			// Clear inuse and hole bit.
			pSaveHeightmapData[i + j * width] &= ~TERRAIN_BITMASK;
		}
	}

	CHeightmap *pHeightmap = m_IEditor->GetHeightmap();
	int unitSize = pHeightmap->GetUnitSize();

	float fPrecisionScale = pHeightmap->GetShortPrecisionScale();

	// Encoding reserved bit and flatten the ground in the marked area
	// of map objects
	for (i=0; i < objects.size(); i++)
	{  
		CBaseObject *obj = objects[i];

		int iEmptyRadius = ftoi(obj->GetArea()/2.0f);

		// Skip if the object has no empty radius
		if (iEmptyRadius == 0)
			continue;

		// Swap X/Y
		op = obj->GetWorldPos();
		float fZ = m_IEditor->GetTerrainElevation(op.x,op.y);
		int objX = ftoi( op.y/unitSize );
		int objY = ftoi( op.x/unitSize );

		//m_vegetationMap->ClearObjects( CPoint(objX*UNIT_SIZE,objY*UNIT_SIZE),2*iEmptyRadius );

		if (!obj->CheckFlags(OBJFLAG_FLATTEN))
			continue;
 
		// Calculate the maximum distance to be able to calculate attenuation
		// inside the loop
		fMaxDist = (float) sqrtf(iEmptyRadius*iEmptyRadius + iEmptyRadius*iEmptyRadius);

		// Mark the area for this object
		for (iStartPosX= -(iEmptyRadius + 15); iStartPosX<iEmptyRadius+15; iStartPosX++)
		{
			for (iStartPosY= -(iEmptyRadius + 15); iStartPosY<iEmptyRadius + 15; iStartPosY++)
			{
				// Calculate current position
				iX = objX + iStartPosX;
				iY = objY + iStartPosY;

				// Skip invalid positions
				if (iX < 0 || iY < 0 || iX >= width || iY >= height)
				{
					continue;
				}

				// Calclate the attenuation for the current distance
				fDist = sqrtf((float) (abs(iStartPosX) * abs(iStartPosX) + abs(iStartPosY) * abs(iStartPosY)));
				fDist = __max(0.0f, fDist - 15);
				fAttenuation = 1.0f - __min(1.0f, fDist / fMaxDist);
				
				// Make the area around the building flat
				uint16 *src = &pSaveHeightmapData[iX+iY*width];
				unsigned int h = ftoi( fPrecisionScale*fAttenuation*fZ + (1.0f - fAttenuation) * (*src));
				
				*src &= TERRAIN_BITMASK; // Leave only bitsflags.
				*src |= h & ~(TERRAIN_BITMASK); // Combine height width flags.

				if (fAttenuation < 0.3)
				 continue;

				// Set the third bit
				pHeightmap->InfoAt(iX,iY) &= ~HEIGHTMAP_INFO_SFTYPE_MASK;
			}
		}
	}

	// Mark holes and used flag.
	for (j=0; j<height; j++)
	{
		for (i=0; i<width; i++)
		{
			if(pHeightmap->IsHoleAt(i,j))
				pSaveHeightmapData[i + j*width] |= SURFACE_TYPE_MASK;
			 else
				pSaveHeightmapData[i + j*width] |= GetDetailLayerIdAt(i,j);
		}
	}
}
*/


//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportBrushes( const CString &path )
{
	GetIEditor()->SetStatusText( _T("Exporting Brushes...") );

	CBrushExporter brushExport;
	brushExport.ExportBrushes( path,m_levelPath,m_levelPakFile );
}

void CGameExporter::ForceDeleteFile( const char *filename )
{
	SetFileAttributes( filename,FILE_ATTRIBUTE_NORMAL );
	DeleteFile( filename );
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::DeleteOldFiles(  const CString &levelDir,bool bSurfaceTexture )
{
	ForceDeleteFile( levelDir + "brush.lst" );
	ForceDeleteFile( levelDir + "particles.lst" );
	ForceDeleteFile( levelDir + "LevelData.xml" );
	ForceDeleteFile( levelDir + "MovieData.xml" );
	ForceDeleteFile( levelDir + "objects.lst" );
	ForceDeleteFile( levelDir + "objects.idx" );
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportMusic( XmlNodeRef &levelDataNode,const CString &path )
{
	// Export music manager.
	CMusicManager *pMusicManager = GetIEditor()->GetMusicManager();
	pMusicManager->Export( levelDataNode );

	CString musicPath = Path::AddBackslash(path) + "Music";
	CString filename = Path::Make( musicPath,MUSIC_LEVEL_LIBRARY_FILE );

	bool bEmptyLevelLib = true;
	XmlNodeRef nodeLib = CreateXmlNode( "MusicThemeLibrary" );
	// Export Music local level library.
	for (int i = 0; i < pMusicManager->GetLibraryCount(); i++)
	{
		IDataBaseLibrary *pLib = pMusicManager->GetLibrary(i);
		if (pLib->IsLevelLibrary())
		{
			if (pLib->GetItemCount() > 0)
			{
				bEmptyLevelLib = false;
				// Export this library.
				pLib->Serialize( nodeLib,false );
			}
		}
		else
		{
			if (pLib->GetItemCount() > 0)
			{
				// Export this library to pak file.
				XmlNodeRef musicNodeLib = CreateXmlNode( "MusicThemeLibrary" );
				pLib->Serialize( musicNodeLib,false );
				CCryMemFile file;
				XmlString xmlData = musicNodeLib->getXML();
				file.Write( xmlData.c_str(),xmlData.length() );
				CString music_filename = Path::Make( musicPath,Path::GetFile(pLib->GetFilename()) );
				m_levelPakFile.UpdateFile( music_filename,file );
			}
		}
	}
	if (!bEmptyLevelLib)
	{
		XmlString xmlData = nodeLib->getXML();

		CCryMemFile file;
		file.Write( xmlData.c_str(),xmlData.length() );
		m_levelPakFile.UpdateFile( filename,file );
	}
	else
	{
		m_levelPakFile.RemoveFile( filename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportMaterials( XmlNodeRef &levelDataNode,const CString &path )
{
	//////////////////////////////////////////////////////////////////////////
	// Export materials manager.
	CMaterialManager *pManager = GetIEditor()->GetMaterialManager();
	pManager->Export( levelDataNode );

	CString filename = Path::Make( path,MATERIAL_LEVEL_LIBRARY_FILE );

	bool bHaveItems = true;

	int numMtls = 0;

	XmlNodeRef nodeMaterials = CreateXmlNode( "MaterialsLibrary" );
	// Export Materials local level library.
	for (int i = 0; i < pManager->GetLibraryCount(); i++)
	{
		XmlNodeRef nodeLib = nodeMaterials->newChild( "Library" );
		CMaterialLibrary *pLib = (CMaterialLibrary*)pManager->GetLibrary(i);
		if (pLib->GetItemCount() > 0)
		{
			bHaveItems = false;
			// Export this library.
			numMtls += pManager->ExportLib( pLib,nodeLib );
		}
	}
	if (!bHaveItems)
	{
		XmlString xmlData = nodeMaterials->getXML();

		CCryMemFile file;
		file.Write( xmlData.c_str(),xmlData.length() );
		m_levelPakFile.UpdateFile( filename,file );
	}
	else
	{
		m_levelPakFile.RemoveFile( filename );
	}
	m_numExportedMaterials = numMtls;
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportLevelResourceList( const CString &path )
{
	IResourceList *pResList = gEnv->pCryPak->GetRecorderdResourceList(ICryPak::RFOM_Level);

	// Write resource list to file.
	CCryMemFile memFile;
	for (const char *filename = pResList->GetFirst(); filename; filename = pResList->GetNext())
	{
		memFile.Write( filename,strlen(filename) );
		memFile.Write( "\n",1 );
	}

	CString resFile = Path::Make( path,RESOURCE_LIST_FILE );

	m_levelPakFile.UpdateFile( resFile,memFile,true );
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportLevelShaderCache( const CString &path )
{
	CString buf;
	GetIEditor()->GetDocument()->GetShaderCache()->SaveBuffer( buf );
	CCryMemFile memFile;
	memFile.Write( (const char*)buf,buf.GetLength() );
	
	CString filename = Path::Make( path,SHADER_LIST_FILE );
	m_levelPakFile.UpdateFile( filename,memFile,true );
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportGameTokens( XmlNodeRef &levelDataNode,const CString &path )
{
	// Export game tokens
	CGameTokenManager *pGTM = GetIEditor()->GetGameTokenManager();
	// write only needed libs for this levels
	pGTM->Export( levelDataNode );

	CString gtPath = Path::AddBackslash(path) + "GameTokens";
	CString filename = Path::Make( gtPath, GAMETOKENS_LEVEL_LIBRARY_FILE);

	bool bEmptyLevelLib = true;
	XmlNodeRef nodeLib = CreateXmlNode( "GameTokensLibrary" );
	// Export GameTokens local level library.
	for (int i = 0; i < pGTM->GetLibraryCount(); i++)
	{
		IDataBaseLibrary *pLib = pGTM->GetLibrary(i);
		if (pLib->IsLevelLibrary())
		{
			if (pLib->GetItemCount() > 0)
			{
				bEmptyLevelLib = false;
				// Export this library.
				pLib->Serialize( nodeLib,false );
				nodeLib->setAttr("LevelName", m_levelName); // we set the Name from "Level" to the realname
			}
		}
		else
		{
			// AlexL: Not sure if 
			// (1) we should store all libs into the PAK file or
			// (2) just use references to the game global Game/Libs directory.
			// Currently we use (1)
			if (pLib->GetItemCount() > 0)
			{
				// Export this library to pak file.
				XmlNodeRef gtNodeLib = CreateXmlNode( "GameTokensLibrary" );
				pLib->Serialize( gtNodeLib,false );
				CCryMemFile file;
				XmlString xmlData = gtNodeLib->getXML();
				file.Write( xmlData.c_str(),xmlData.length() );
				CString gtfilename = Path::Make( gtPath,Path::GetFile(pLib->GetFilename()) );
				m_levelPakFile.UpdateFile( gtfilename,file );
			}
		}
	}
	if (!bEmptyLevelLib)
	{
		XmlString xmlData = nodeLib->getXML();

		CCryMemFile file;
		file.Write( xmlData.c_str(),xmlData.length() );
		m_levelPakFile.UpdateFile( filename,file );
	}
	else
	{
		m_levelPakFile.RemoveFile( filename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportFileList(const CString& path)
{
	// process the folder of the specified map name, producing a filelist.xml file
	//	that can later be used for map downloads
	struct _finddata_t fileinfo;
	intptr_t handle;
	string newpath;
	
	CString filename = Path::GetFileName(Path::RemoveBackslash(path));
	string mapname = filename + ".dds";
	string metaname = filename + ".xml";

	XmlNodeRef rootNode = gEnv->pSystem->CreateXmlNode("download");
	rootNode->setAttr("name", filename);
	rootNode->setAttr("type", "Map");
	XmlNodeRef indexNode = rootNode->newChild("index");
	if(indexNode)
	{
		indexNode->setAttr("src", "filelist.xml");
		indexNode->setAttr("dest", "filelist.xml");
	}
	XmlNodeRef filesNode = rootNode->newChild("files");
	if(filesNode)
	{
		newpath = m_levelName;
		newpath += "/*.*";
		handle = gEnv->pCryPak->FindFirst (newpath.c_str(), &fileinfo);
		if (handle == -1)
			return;
		do
		{
			// ignore "." and ".."
			if (fileinfo.name[0] == '.')
				continue;
			// do we need any files from sub directories?
			if (fileinfo.attrib & _A_SUBDIR)
			{
				continue;
			}

			// only need the following files for multiplayer downloads
			if(!stricmp(fileinfo.name, "level.pak")
				|| !stricmp(fileinfo.name, "levelLM.pak")
				|| !stricmp(fileinfo.name, LEVELVS_PAK_NAME)
				|| !stricmp(fileinfo.name, mapname.c_str())
				|| !stricmp(fileinfo.name, metaname.c_str()) )
			{
				XmlNodeRef newFileNode = filesNode->newChild("file");
				if(newFileNode)
				{
					// TEMP: this is just for testing. src probably needs to be blank.
			//		string src = "http://server41/updater/";
			//		src += m_levelName;
			//		src += "/";
			//		src += fileinfo.name;
					newFileNode->setAttr("src", fileinfo.name);
					newFileNode->setAttr("dest", fileinfo.name);
					newFileNode->setAttr("size", fileinfo.size);

					unsigned char md5[16];
					string filename = m_levelName;
					filename += "/";
					filename += fileinfo.name;
					if(gEnv->pCryPak->ComputeMD5(filename, md5))
					{
						char md5string[33];
						sprintf(md5string, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
							md5[0], md5[1], md5[2], md5[3], 
							md5[4], md5[5], md5[6], md5[7],
							md5[8], md5[9], md5[10], md5[11],
							md5[12], md5[13], md5[14], md5[15]);
						newFileNode->setAttr("md5", md5string);
					}
					else
						newFileNode->setAttr("md5", "");
				}
			}
		} while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);

		gEnv->pCryPak->FindClose (handle);
	}

	// save filelist.xml 
	newpath = m_levelName;
	newpath += "/filelist.xml";
	rootNode->saveToFile(newpath.c_str());
}


void CGameExporter::BuildSectorAOData(int nAreaY, int nAreaX, int nAreaSize, SAOInfo * pAOData, int nAODataSize)
{
  double dStartTime = m_ISystem->GetITimer()->GetAsyncCurTime();

	m_ISystem->GetILog()->Log("Building AO data for sector ( %d, %d ) x %d m ... ", nAreaX, nAreaY, nAreaSize);

	// TODO: include borders

	float fRatio = (float)nAreaSize/nAODataSize;

	bool bObjectsFound = false;

	// generate
	for(int x=0; x<nAODataSize; x++)
	{
		for(int y=0; y<nAODataSize; y++)
		{
			Vec3 vWSPos(float(x*fRatio+nAreaX),float(y*fRatio+nAreaY),0);
			vWSPos.z = m_I3DEngine->GetTerrainElevation(vWSPos.x,vWSPos.y);

			Vec3 vRayBot = vWSPos + Vec3(0.f,0.f,4.f);
			Vec3 vRayTop = vWSPos + Vec3(0.f,0.f,64.f);

			pAOData[x+y*nAODataSize].SetMax(vWSPos.z);

			// store objects elevation relative to to sector min z
			Vec3 vHitPoint(0,0,0);
			if(m_I3DEngine->RayObjectsIntersection(vRayTop, vRayBot, vHitPoint, eERType_Vegetation))
			{
				pAOData[x+y*nAODataSize].SetMax(max(vWSPos.z,vHitPoint.z));
				bObjectsFound = true;
			}
		}
	}

	// blur
	if(bObjectsFound)
	if(int nPassesNum = 8/fRatio)
	{
		SAOInfo * pAODataTmp = new SAOInfo[nAODataSize*nAODataSize];

		for(int nPass=0; nPass<nPassesNum; nPass++)
		{
			memcpy(pAODataTmp,pAOData,nAODataSize*nAODataSize*sizeof(pAOData[0]));

			for(int x=1; x<nAODataSize-1; x++)
			{
				for(int y=1; y<nAODataSize-1; y++)
				{
		//			float fRefMax = max(pAOData[((x)+(y)*nAODataSize)].GetMax(),
			//			m_I3DEngine->GetTerrainElevation(x,y));

					float fMax = 0;
					float fCount = 0;

					for(int _x=x-1; _x<=x+1; _x++)
					{
						for(int _y=y-1; _y<=y+1; _y++)
						{
//              fMax += max(pAOData[((_x)+(_y)*nAODataSize)].GetMax(), fRefMax);
              fMax += pAOData[((_x)+(_y)*nAODataSize)].GetMax();
							fCount++;
						}
					}

					pAODataTmp[((x)+(y)*nAODataSize)].SetMax(fMax/fCount);
				}
			}

			memcpy(pAOData,pAODataTmp,nAODataSize*nAODataSize*sizeof(pAOData[0]));
		}
		delete [] pAODataTmp;
	}

	m_ISystem->GetILog()->LogPlus(" done", nAreaX, nAreaY, nAreaSize, nAODataSize);

  gBuildSectorAODataTime += m_ISystem->GetITimer()->GetAsyncCurTime() - dStartTime;
}