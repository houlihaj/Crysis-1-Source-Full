////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   gameexporter.h
//  Version:     v1.00
//  Created:     30/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __gameexporter_h__
#define __gameexporter_h__
#pragma once

#include "Util\PakFile.h"

class CTerrainLightGen;
class CWaitProgress;


/*!	CGameExporter implements exporting of data fom Editor to Game format.
		It will produce LevelData.cpk file in current level folder, with nececcary exported files.
 */
class CGameExporter
{
public:
	// constructor
	CGameExporter( ISystem *system );
	// destructor
	virtual ~CGameExporter();

	// In auto exporting mode, highest possible settings will be choosen and no UI dialogs will be shown.
	void SetAutoExportMode( bool bAuto );

	// Export level info to game.
	virtual void Export( bool bSurfaceTexture, bool bReloadTerrain, const bool bAI=true );
	virtual void ExportLevelData( const CString &path,bool bExportMission=true );
	virtual void ExportLevelInfo( const CString &path );

//	void FlattenHeightmap( uint16 *pSaveHeightmapData,int width,int height );

	virtual void ExportMap( const char *pszGamePath, bool bSurfaceTexture);
	virtual void ExportHeightMap(const char *pszGamePath);
	virtual void ExportAnimations( const CString &path );
//	virtual void ExportStaticObjects( const CString &path );
	virtual void ExportSurfaceTypes( const CString &iniFile );
	virtual void ExportMapIni( const CString &path );
	virtual void ExportMapInfo( XmlNodeRef &node );
	virtual bool ExportSurfaceTexture( const char *szFilePathName );
	virtual void ExportAIGraph( const CString &path );
	
	virtual void ExportBrushes( const CString &path );
	virtual void ExportLevelResourceList( const CString &path );
	virtual void ExportLevelShaderCache( const CString &path );
	virtual void ExportMusic( XmlNodeRef &levelDataNode,const CString &path );
	virtual void ExportMaterials( XmlNodeRef &levelDataNode,const CString &path );
	virtual void ExportGameTokens( XmlNodeRef &levelDataNode,const CString &path );

	// create a filelist.xml, with 
	virtual void ExportFileList(const CString& path);

private: // ----------------------------------------------------------------------

	struct SRecursionHelperQuarter
	{
		CImage										m_Layer[2];				// layer 0,1
		float fTerrainMinZ, fTerrainMaxZ;
	};

	struct SRecursionHelper
	{
		SRecursionHelperQuarter		m_Quarter[4];			// 4 textures in one (0=lefttop,1=righttop,2=leftbottom,3=rightbottom)
	};



	// "locally higher texture resolution"
	
	struct SProgressHelper
	{
		SProgressHelper( CWaitProgress &ref, const uint32 dwMax, CTerrainLightGen &rLightGen, CFile &toFile,
			std::vector<int16> &rIndexBlock, const ULONGLONG FileTileOffset, const ULONGLONG ElementFileSize, const char *szFilename ) 
			:m_Progress(ref), m_Current(0), m_Max(dwMax), m_rLightGen(rLightGen), m_toFile(toFile),
			m_rIndexBlock(rIndexBlock), m_IndexBlockPos(0), m_FileTileOffset(FileTileOffset), m_ElementFileSize(ElementFileSize),
			m_szFilename(szFilename)
		{
			m_dwLayerExtends[0]=m_dwLayerExtends[1]=0;
		}

		// Return
		//   true to continue, false to abort lengthy operation.
		bool Increase()
		{
			++m_Current;

			return m_Progress.Step( (m_Current*100)/m_Max );
		}

		uint32											m_dwLayerExtends[2];// layer 0 and 1 width and height

		uint32											m_Current;					// 
		uint32											m_Max;							// used for progress indicator and to compute file size = dwUsedTextureIds
		CWaitProgress &							m_Progress;					//

		CTerrainLightGen &					m_rLightGen;				//
		CFile &											m_toFile;						// 
		ULONGLONG										m_FileTileOffset;		//
		ULONGLONG										m_ElementFileSize;	//

		std::vector<int16> &				m_rIndexBlock;			//
		uint32											m_IndexBlockPos;		//
		const char *								m_szFilename;				// points to the filenname

		SRecursionHelper 						m_TempMem[32];			//
	};


	// --------------------------------------------------------------------------

	CString						m_levelName;								//
	CString						m_missionName;							//
	CString						m_levelPath;								//

	ISystem *					m_ISystem;									//
	I3DEngine *				m_I3DEngine;								//
	IAISystem *				m_IAISystem;								//
	IEntitySystem *		m_IEntitySystem;						//
	IEditor *					m_IEditor;									//

	CPakFile					m_levelPakFile;							//
	bool							m_bHiQualityCompression;		//
	bool							m_bDebugTexture;						//
	bool							m_bUpdateIndirectLighting;
	bool              m_bAutoExportMode;
	int								m_ApplySS;
//	float							m_fTerrainGIClamp;
	int								m_iExportTexWidth;					//
	int								m_numExportedMaterials;			//


	// "locally higher texture resolution"
	
	//
	void DeleteOldFiles( const CString &levelDir,bool bSurfaceTexture );
	//
	void ForceDeleteFile( const char *filename );

	// Quality is good (2x2) downsampling but not the best possible (3x3) because we need to limit access
	// withing the texture to create texture efficiently
	// the border colors remain the same to avoid discontinuities with LODs
	// Arguments:
	//   rInAndOut - 4 sub images (0=lefttop,1=righttop,2=leftbottom,3=rightbottom), all of the same size
	static void DownSampleWithBordersPreserved( const CImage rIn[4], CImage &rOut );
	static void DownSampleWithBordersPreservedAO( const CImage rIn[4], 
		const float rInTerrainMinZ[4], const float rInTerrainMaxZ[4], CImage &rOut );

	// Arguments:
	//   dwMinX - [0..dwSectorCount-1]
	//   dwMinY - [0..dwSectorCount-1]
	// Return
	//   true to continue, false to abort lengthy operation.
	bool _ExportSurfaceTextureRecursive( SProgressHelper &phelper, const int iParentIndex, SRecursionHelperQuarter &rDestPiece,
		const float fMinX=0, const float fMinY=0, const uint32 dwRecursionDepth=0 );

	bool _BuildIndexBlockRecursive( const uint32 dwTileSize, std::vector<int16> &rIndexBlock, uint32 &dwUsedTextureIds,
		const float fMinX=0, const float fMinY=0, const uint32 dwRecursionDepth=0 );

	// Return
	//   true to continue, false to abort lengthy operation.
	bool SeekToSectorAndStore( SProgressHelper &phelper, const int iParentIndex, const uint32 dwLevel,
		SRecursionHelperQuarter &rQuarter );

	void BuildSectorAOData(int nAreaX, int nAreaY, int nAreaSize, struct SAOInfo * pData, int nDataSize);
};

#endif // __gameexporter_h__
