// Heightmap.h: interface for the CHeightmap class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_HEIGHTMAP_H__F92AE690_FC38_4249_BEA9_51384504DF9E__INCLUDED_)
#define AFX_HEIGHTMAP_H__F92AE690_FC38_4249_BEA9_51384504DF9E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "RGBLayer.h"								// CRGBLayer

// Heightmap data type
typedef float t_hmap;

#define NUM_SECTORS 32 //!< Sector table dimensions (32*32)

// From Engine.
//! Surface type mask, masks bits resereved to store surface id.
#define SURFACE_TYPE_MASK (1|2|4|8)
//! Heigtmap bitmask, masks all bits out of heightmap data reserved for special use.
#define TERRAIN_BITMASK (SURFACE_TYPE_MASK)

enum EHeighmapInfo
{
	//! Hole flag in heighmap info.
	HEIGHTMAP_INFO_HOLE		= 0x01,
};

//! Surface id mask in heighmap info.
#define HEIGHTMAP_INFO_SFTYPE_MASK 0xfe
//! Surface id shift in heighmap info.
#define HEIGHTMAP_INFO_SFTYPE_SHIFT	1

class CXmlArchive;
class CDynamicArray2D;
struct SNoiseParams;
class CVegetationMap;
class CTerrainGrid;
struct SEditorPaintBrush;

struct SSectorInfo
{
	int unitSize;						// Size of terrain unit.
	int sectorSize;					// Sector size in meters.
	int sectorTexSize;			// Size of texture for one sector in pixels.
	int numSectors;					// Number of sectors on one side of terrain.
	int surfaceTextureSize;	// Size of whole terrain surface texture.
};

// Editor data structure to keep the heights, detail layer information/holes, terrain texture
class CHeightmap  
{
public:
	// constructor
	CHeightmap();
	// destructor
	virtual ~CHeightmap();

	// Member data access
	uint GetWidth() const { return m_iWidth; };
	uint GetHeight() const { return m_iHeight; };
	float GetMaxHeight() const { return m_fMaxHeight; }

	//! Get size of every heightmap unit in meters.
	int GetUnitSize() const { return m_unitSize; };

	void SetMaxHeight( float fMaxHeight );
	void SetUnitSize( int nUnitSize ) { m_unitSize = nUnitSize; }

	//! Convert from world coordinates to heightmap coordinates.
	CPoint WorldToHmap( const Vec3 &wp );
	//! Convert from heightmap coordinates to world coordinates.
	Vec3 HmapToWorld( CPoint hpos );

	// Maps world bounding box to the heightmap space rectangle.
	CRect WorldBoundsToRect( const BBox &worldBounds );

	//! Get access to vegetation map.
	CVegetationMap* GetVegetationMap();

	//! Set size of current surface texture.
	void SetSurfaceTextureSize( int width,int height );

	//! Returns information about sectors on terrain.
	//! @param si Structure filled with quered data.
	void GetSectorsInfo( SSectorInfo &si );
	
	t_hmap* GetData() { return m_pHeightmap; };
	bool GetDataEx(t_hmap *pData, UINT iDestWidth, bool bSmooth = true, bool bNoise = true);

	// Fill image data
	// Arguments:
	//   resolution Resolution of needed heightmap.
	//   vTexOffset offst within hmap
	//   trgRect Target rectangle in scaled heightmap.
	bool GetData( CRect &trgRect, const int resolution, const CPoint vTexOffset, CFloatImage &hmap, bool bSmooth=true,bool bNoise=true );

	//////////////////////////////////////////////////////////////////////////
	// Terrain Grid functions.
	//////////////////////////////////////////////////////////////////////////
	void InitSectorGrid();
	CTerrainGrid* GetTerrainGrid() const { return m_terrainGrid; };
	
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	void SetXY( uint x,uint y,t_hmap iVal ) { m_pHeightmap[x + y*m_iWidth] = iVal; };
	t_hmap& GetXY( uint x,uint y ) { return m_pHeightmap[x + y*m_iWidth]; };
	t_hmap GetXY( uint x,uint y ) const { return m_pHeightmap[x + y*m_iWidth]; };
	t_hmap GetSafeXY( const uint32 dwX, const uint32 dwY ) const;

	//! Calculate heightmap slope at given point.
	//! @return clamped to 0-255 range slope value.
	float GetSlope( int x,int y );

	// Returns
	//   slope in height meters per meter
	float GetAccurateSlope( const float x, const float y );

	float GetZInterpolated( const float x1, const float y1 );

	float CalcHeightScale() const;
	
	bool GetPreviewBitmap( DWORD *pBitmapData,int width,bool bSmooth = true, bool bNoise = true,CRect *pUpdateRect=NULL,bool bShowWater=false );

	void GetLayerIdBlock(int x, int y, int width, int height, CByteImage & map);
	void SetLayerIdBlock(int x, int y, const CByteImage & map);

	// Arguments:
	//   fpx - 0..1 in the whole terrain
	//   fpy - 0..1 in the whole terrain
	//   dwLayerId - id from CLayer::GetLayerId()
	void PaintLayerId( const float fpx, const float fpy, const SEditorPaintBrush &brush, const uint32 dwLayerId );

	//
	bool IsHoleAt( const int x, const int y ) const;
	
	// Arguments
	//   bHole - true=hole, false=no hole
	void SetHoleAt( const int x, const int y, const bool bHole );
	// Arguments:
	//   x - 0..GetWidth()-1
	//   y - 0..GetHeight()-1
	// Returns:
	//   0xffffffff means there is not valid layer
	uint32 GetLayerIdAt( const int x, const int y ) const;
	// Arguments:
	//   x - 0..GetWidth()-1
	//   y - 0..GetHeight()-1
	// Returns:
	//   0xffffffff means there is not valid detail layer
	uint32 GetDetailLayerIdAt( const int x, const int y ) const;

	// Arguments
	//   dwLayerId - from CLayer::GetLayerId()
	void SetLayerIdAt( const int x, const int y, const uint32 dwLayerId );

	void MarkUsedLayerIds( bool bFree[256] ) const;

	// Hold / fetch
	void Fetch();
	void Hold();
	bool Read(CString strFileName);

	// (Re)Allocate / deallocate
	void Resize( int iWidth, int iHeight,int unitSize,bool bCleanOld=true );
	void CleanUp();

	void InvalidateLayers();

	// Importing / exporting
	void Serialize( CXmlArchive &xmlAr,bool bSerializeVegetation,bool bUpdateTerrain );
	void SerializeVegetation( CXmlArchive &xmlAr  );
	void SerializeTerrain( CXmlArchive &xmlAr, bool &bTerrainRefreshRequested );
	void SaveImage( LPCSTR pszFileName, bool bNoiseAndFilter = true);
	void LoadBMP( LPCSTR pszFileName, bool bNoiseAndFilter = true);

	//! Save heightmap to PGM file format.
	void SavePGM( const CString &pgmFile );
	//! Load heightmap from PGM file format.
	void LoadPGM( const CString &pgmFile );

	//! Save heightmap in RAW format.
	void	SaveRAW(  const CString &rawFile );
	//! Load heightmap from RAW format.
	void	LoadRAW(  const CString &rawFile );
	
	//! Save heightmap in RAW format.
	void	SaveFRAW(  const CString &rawFile );

	// Actions
	void Smooth();
	void Smooth( CFloatImage &hmap,const CRect &rect );

	void Noise();
	void Normalize();
	void RemoveWater();
	void Invert();
	void MakeIsle();
	void SmoothSlope();
	void Randomize();
	void MakeBeaches();
	void LowerRange(float fFactor);
	void Flatten(float fFactor);
	void GenerateTerrain(const SNoiseParams &sParam);
	void Clear();

	//! Calculate surface type in rectangle.
	//! @param rect if Rectangle is null surface type calculated for whole heightmap.
	void CalcSurfaceTypes( const CRect *rect = NULL );

	// Drawing
	void DrawSpot(unsigned long iX, unsigned long iY, 
		uint8 iWidth, float fAddVal, float fSetToHeight = -1.0f,
		bool bAddNoise = false);

	void DrawSpot2( int iX, int iY, int radius,float insideRadius, float fHeigh,float fHardness=1.0f,bool bAddNoise = false,float noiseFreq=1,float noiseScale=1 );
	void SmoothSpot( int iX, int iY, int radius, float fHeigh,float fHardness );
	void RiseLowerSpot( int iX, int iY, int radius,float insideRadius, float fHeigh,float fHardness=1.0f,bool bAddNoise = false,float noiseFreq=1,float noiseScale=1 );

	//! Make hole in the terrain.
	void MakeHole( int x1,int y1,int width,int height,bool bMake );

	//! Export terrain block.
	void ExportBlock( const CRect &rect,CXmlArchive &ar, bool bIsExportVegetation = true);
	//! Import terrain block, return offset of block to new position.
	CPoint ImportBlock( CXmlArchive &ar, CPoint newPos, bool bUseNewPos=true, float heightOffset = 0.0f, bool bOnlyVegetation = false );

	//! Update terrain block in engine terrain.
	void UpdateEngineTerrain( int x1,int y1,int width,int height,bool bElevation,bool bInfoBits );
	//! Update all engine terrain.
	void UpdateEngineTerrain( bool bOnlyElevation=true );

	//! Update layer textures in video memory.
	void UpdateLayerTexture( const CRect &rect );

	//! Synchronize engine hole bit with bit stored in editor heightmap.
	void UpdateEngineHole( int x1,int y1,int width,int height );

	void SetWaterLevel( float waterLevel );
	float GetWaterLevel() const { return m_fWaterLevel; };

	void CopyData(t_hmap *pDataOut)
	{
		memcpy(pDataOut, m_pHeightmap, sizeof(t_hmap) * m_iWidth * m_iHeight);
	}

	//! Copy from different heightmap data.
	void CopyFrom( t_hmap *pHmap,unsigned char *pLayerIdBitmap,int resolution );
	void CopyFromInterpolate( t_hmap *pHmap,unsigned char *pLayerIdBitmap,int resolution, float kof );

	//! Dump to log sizes of all layers.
	//! @return Total size allocated for layers.
	int LogLayerSizes();

	float GetShortPrecisionScale() const { return (double)(256*256-1) / m_fMaxHeight; }
	float GetBytePrecisionScale() const { return (double)(255) / m_fMaxHeight; }

	void GetMemoryUsage( ICrySizer *pSizer );

	// to update internal structures
	void ProcessAfterLoading();

	void RecordUndo( int x1,int y1,int width,int height);

	// -----------------------------------------------------------------

	CRGBLayer								m_TerrainRGBTexture;					// Terrain RGB texture

protected: // -----------------------------------------------------------------

	void ClampHeight( float &h ) { h = min(m_fMaxHeight,max(0.0f,h)); }
	
	// Helper functionss
	__inline void ClampToAverage(t_hmap *pValue, float fAverage);
	__inline float ExpCurve(float v, float fCover, float fSharpness);

	void SetModified();

	// Verify internal class state
	__inline void Verify() 
	{ 
		ASSERT(m_iWidth && m_iHeight); 
		ASSERT(!IsBadWritePtr(m_pHeightmap, sizeof(t_hmap) * m_iWidth * m_iHeight));
	};

	// Initialization
	void InitNoise();

private: // --------------------------------------------------------------------

	float							m_fWaterLevel;					//
	float							m_fMaxHeight;						//
	bool							m_bUpdateLayerIdBitmap;	// m_info was loaded, need to be converted to new format m_LayerIdBitmap

	t_hmap *					m_pHeightmap;						//
	CDynamicArray2D *	m_pNoise;								//
	
	CByteImage				m_LayerIdBitmap;				// (m_iWidth,m_iHeight) LayerId and EHeighmapInfo (was m_info, Detail LayerId per heightmap element)

	UINT							m_iWidth;								//
	UINT							m_iHeight;							//

	int								m_textureSize;					// Size of surface texture.
	int								m_numSectors;						// Number of sectors per grid side.
	int								m_unitSize;							// 

	CVegetationMap *	m_vegetationMap;				// Vegetation map for this heightmap.
	CTerrainGrid *		m_terrainGrid;					// Terrain grid.

	CImage            m_waterImage;

	friend class CUndoHeightmapInfo;
};


//////////////////////////////////////////////////////////////////////////
// Inlined implementation of get slope.
inline float CHeightmap::GetSlope( int x,int y )
{
	//assert( x >= 0 && x < m_iWidth && y >= 0 && y < m_iHeight );
	if (x <= 0 || y <= 0 || x >= m_iWidth-1 || y >= m_iHeight-1)
		return 0;

	t_hmap *p = &m_pHeightmap[x+y*m_iWidth];
	float h = *p;
	int w = m_iWidth;
	float fs = (fabs(*(p+1)		- h) +
		          fabs(*(p-1)		- h) +
		          fabs(*(p+w)		- h) +
		          fabs(*(p-w)		- h) +
		          fabs(*(p+w+1)	- h) +
		          fabs(*(p-w-1) - h) +
       			  fabs(*(p+w-1) - h) +
	        	  fabs(*(p-w+1) - h));
	fs = fs*8;
	if (fs > 255.0f) fs = 255.0f;
	return fs;
};

#endif // !defined(AFX_HEIGHTMAP_H__F92AE690_FC38_4249_BEA9_51384504DF9E__INCLUDED_)