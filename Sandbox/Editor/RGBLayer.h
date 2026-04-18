#ifndef _RGBLAYER_HDR_
#define _RGBLAYER_HDR_

#include "Util/ImagePainter.h"		// CImagePainter, SEditorPaintBrush
#include "TimeValue.h"						// CTimeValue

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000



// RGB terrain texture layer
// internal structure (tiled based, loading tiles on demand) is hidden from outside
// texture does not have to be square
// cache uses "last recently used" strategy to remain within set memory bounds (during surface texture generation)
class CRGBLayer : public CObject
{
public:
	// constructor
	CRGBLayer( const char *szFilename );
	// destructor
	virtual ~CRGBLayer();


	// Serialization
	void Serialize( CXmlArchive &xmlAr );

	// might throw an exception if memory is low but we only need very few bytes
	// Arguments:
	//   dwTileCountX - must not be 0
	//   dwTileCountY - must not be 0
	//   dwTileResolution must be power of two
	void AllocateTiles( const uint32 dwTileCountX, const uint32 dwTileCountY, const uint32 dwTileResolution );

	//
	void FreeData();

	// calculated the texture resolution needed to capture all details in the texture
	uint32 CalcMinRequiredTextureExtend();

	uint32 GetTileCountX() const { return m_dwTileCountX; }
	uint32 GetTileCountY() const { return m_dwTileCountY; }

	// might need to load the tile once
	// Return:
	//   might be 0 if no tile exists at this point
	uint32 GetTileResolution( const uint32 dwTileX, const uint32 dwTileY );

	// Arguments:
	//   dwTileX -  0..GetTileCountX()-1
	//   dwTileY -  0..GetTileCountY()-1
	//   bRaise - true=raise, flase=lower
	bool ChangeTileResolution( const uint32 dwTileX, const uint32 dwTileY, uint32 dwNewSize );

	// 1:1
	void SetSubImageRGBLayer( const uint32 dwDstX, const uint32 dwDstY, const CImage &rTileImage );

	void SetSubImageStretched( const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, CImage &rInImage, const bool bFiltered=false );

	// stretched (dwSrcWidth,dwSrcHeight)->rOutImage
	// Arguments:
	//   fSrcLeft - 0..1
	//   fSrcTop - 0..1
	//   fSrcRight - 0..1, <fSrcLeft
	//   fSrcBottom - 0..1, <fSrcTop
	//	Notice: is build as follows: blue | green << 8 | red << 16, alpha unused
	void GetSubImageStretched( const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, CImage &rOutImage, const bool bFiltered=false );

	// Paint spot with pattern (to an RGB image)
	// Arguments:
	//   fpx - 0..1 in the texture
	//   fpy - 0..1 in the texture
	void PaintBrushWithPatternTiled( const float fpx, const float fpy, SEditorPaintBrush &brush, const CImage &imgPattern );

	// needed for Layer Painting
	// Arguments:
	//   fSrcLeft - usual range: [0..1] but full range is valid 
	//   fSrcTop - usual range: [0..1] but full range is valid 
	//   fSrcRight - usual range: [0..1] but full range is valid 
	//   fSrcBottom - usual range: [0..1] but full range is valid 
	// Return:
	//   might be 0 if no tile was touched
	uint32 CalcMaxLocalResolution( const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom );

	bool ImportExportBlock(const char * pFileName, const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, uint32 * outSquare, bool bIsImport=false);

	//
	bool IsAllocated() const;

	void GetMemoryUsage( ICrySizer *pSizer );

	// useful to detect problems before saving (e.g. file is read-only)
	bool WouldSaveSucceed();

	// can be called from outside to save memory
	void CleanupCache();

	// writes out a debug images to the current directory
	void Debug();

	// rarely needed editor operation - n terrain texture tiles become 4*n
	// slow
	// Returns:
	//   success
	bool RefineTiles();

private: // ---------------------------------------------------------

	class CTerrainTextureTiles
	{
	public:
		// default constructor
		CTerrainTextureTiles() :m_pTileImage(0), m_bDirty(false), m_dwSize(0)
		{
		}

		CImage *			m_pTileImage;				// 0 if not loaded, otherwise pointer to the image (m_dwTileResolution,m_dwTileResolution)
		CTimeValue		m_timeLastUsed;			// needed for "last recently used" strategy
		bool					m_bDirty;						// true=tile needs to be written to disk, needed for "last recently used" strategy
		uint32				m_dwSize;						// only valid if m_dwSize!=0, if not valid you need to call LoadTileIfNeeded()
	};

	// -----------------------------------------------------------------

	std::vector<CTerrainTextureTiles>		m_TerrainTextureTiles;					// [x+y*m_dwTileCountX]

	uint32															m_dwTileCountX;									// 
	uint32															m_dwTileCountY;									// 
	uint32															m_dwTileResolution;							// must be power of two, tiles are square in size
	uint32															m_dwCurrentTileMemory;					// to detect if GarbageCollect is needed
	bool																m_bPakOpened;										// to optimize redundant OpenPak an ClosePak
	bool																m_bInfoDirty;										// true=save needed e.g. internal properties changed
	bool																m_bNextSerializeForceSizeSave;	//

	static const uint32									m_dwMaxTileMemory=100*1024*1024;	// max 100 MB
	string															m_TerrainRGBFileName;

	// -----------------------------------------------------------------

	//
	bool OpenPakForLoading();

	//
	void ClosePakForLoading();

	//
	bool SaveAndFreeMemory( const bool bForceFileCreation=false );

	//
//	bool IsSaveNeeded() const;

	// Arguments:
	//   dwTileX - 0..m_dwTileCountX
	//   dwTileY - 0..m_dwTileCountY
	// Return:
	//   might be 0 if no tile exists at this point
	CTerrainTextureTiles *LoadTileIfNeeded( const uint32 dwTileX, const uint32 dwTileY );

	// Arguments:
	//   dwTileX - 0..m_dwTileCountX
	//   dwTileY - 0..m_dwTileCountY
	// Return:
	//   might be 0 if no tile exists at this point
	CTerrainTextureTiles *GetTilePtr( const uint32 dwTileX, const uint32 dwTileY );

	// slow but convenient (will be filtered one day)
	// Arguments:
	//   fpx - 0..1
	//   fpx - 0..1
	uint32 GetValueAt( const float fpx, const float fpy );

	// slow but convenient (will be filtered one day)
	// Arguments:
	//   fpx - 0..1
	//   fpx - 0..1
	uint32 GetFilteredValueAt( const float fpx, const float fpy );

	// Arguments:
	//   dwX - [0..GetTextureWidth()[
	//   dwY - [0..GetTextureHeight()[
	void SetValueAt( const uint32 dwX, const uint32 dwY, const uint32 dwValue );

	void FreeTile( CTerrainTextureTiles &rTile );

	// Return:
	//   might be 0
	CTerrainTextureTiles *FindOldestTileToFree();

	// removed tiles till we reach the limit
	void ConsiderGarbageCollection();

	void ClipTileRect( CRect &inoutRect ) const;

	// Return:
	//   true = save needed
	bool IsDirty() const;

	CString GetFullFileName();
};

#endif // _RGBLAYER_HDR_