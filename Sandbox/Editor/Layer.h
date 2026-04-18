// Layer.h: interface for the CLayer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LAYER_H__92D64D54_E1C2_4D48_9664_8BD50F7324F8__INCLUDED_)
#define AFX_LAYER_H__92D64D54_E1C2_4D48_9664_8BD50F7324F8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//! Single texture layer
class CLayer : public CObject
{
public:
	// constructor
	CLayer();
	// destructor
	virtual ~CLayer();

	// Name
	CString GetLayerName() { return m_strLayerName; }
	void SetLayerName(CString strName) { m_strLayerName = strName; }

	// Slope Angle 0=flat..90=max steep
	float GetLayerMinSlopeAngle() { return m_minSlopeAngle; }
	float GetLayerMaxSlopeAngle() { return m_maxSlopeAngle; }
	void SetLayerMinSlopeAngle( float min ) { m_minSlopeAngle = min; InvalidateMask(); }
	void SetLayerMaxSlopeAngle( float max ) { m_maxSlopeAngle = max; InvalidateMask(); }

	// Altitude
	float GetLayerStart() { return m_LayerStart; }
	float GetLayerEnd() { return m_LayerEnd; }
	void SetLayerStart( float iStart ) { m_LayerStart = iStart; InvalidateMask(); }
	void SetLayerEnd( float iEnd ) { m_LayerEnd = iEnd; InvalidateMask(); }

	// Calculate memory size allocated for this layer
	// Return:
	//   in bytes
	int GetSize() const;

	//////////////////////////////////////////////////////////////////////////
	// Layer Mask
	//////////////////////////////////////////////////////////////////////////
	unsigned char GetLayerMaskPoint( uint x,uint y ) { return m_layerMask.ValueAt(x,y); }
//	void SetLayerMaskPoint( uint x,uint y,unsigned char c ) { m_layerMask.ValueAt(x,y) = c; }

	//////////////////////////////////////////////////////////////////////////
	// Update current mask.
	//////////////////////////////////////////////////////////////////////////

	// Update current mask for this sector and put it into target mask data.
	// Argument:s
	//   mask - Mask image returned from function, may not be not initialized when calling function.
	// Return:
	//   true is mask for this sector exist, false if not.
	bool UpdateMaskForSector( CPoint sector, const CRect &sectorRect, const int resolution, const CPoint vTexOffset,
		const CFloatImage &hmap,CByteImage& mask );
	//
	bool UpdateMask( const CFloatImage &hmap,CByteImage& mask );
	//
	CByteImage& GetMask();

	//////////////////////////////////////////////////////////////////////////

	//
	void GenerateWaterLayer16(float *pHeightmapPixels, UINT iHeightmapWidth,UINT iHeightmapHeight,float waterLevel );

	//////////////////////////////////////////////////////////////////////////
	int GetMaskResolution() const { return m_maskResolution; }
	
	// Texture
	int GetTextureWidth() { return m_cTextureDimensions.cx; }
	int GetTextureHeight() { return m_cTextureDimensions.cy; }
	CSize GetTextureDimensions() { return m_cTextureDimensions; }
	// filename without path
	CString GetTextureFilename();
	// including path
	CString GetTextureFilenameWithPath() const;
	void DrawLayerTexturePreview(LPRECT rcPos, CDC *pDC);
	bool LoadTexture(CString strFileName);
	// used to fille with water color
	void FillWithColor( COLORREF col,int width,int height );
	bool LoadTexture(LPCTSTR lpBitmapName, UINT iWidth, UINT iHeight);
	bool LoadTexture(DWORD *pBitmapData, UINT iWidth, UINT iHeight);
	void ExportTexture(CString strFileName);
	// represents the editor feature
	void ExportMask( const CString &strFileName );
	bool HasTexture() {	return ((GetTextureWidth() != 0) && (m_texture.GetData() != NULL)); }
	void Update3dengineInfo();
	
	uint& GetTexturePixel( int x,int y ) { return m_texture.ValueAt(x,y); }


	// represents the editor feature
	// Load a BMP texture into the layer mask.
	bool LoadMask( const CString &strFileName );
	void GenerateLayerMask( CByteImage &mask,int width,int height );

	// Release allocated mask
	void ReleaseMask();
	
	// Release temporar allocated resources
	void ReleaseTempResources();

	// Serialisation
	void Serialize( CXmlArchive &xmlAr );

	// Call if mask was Modified.
	void InvalidateMask();
	// Invalidate one sector of the layer mask.
	void InvalidateMaskSector( CPoint sector );

	// Check if layer is valid.
	bool IsValid() { return m_layerMask.IsValid(); }

	// In use
	bool IsInUse() { return m_bLayerInUse; }
	void SetInUse(bool bNewState) { m_bLayerInUse = bNewState; }

	bool IsAutoGen() const { return m_bAutoGen; }
	void SetAutoGen( bool bAutoGen );

	//////////////////////////////////////////////////////////////////////////
	// True if layer is currently selected.
	bool IsSelected() const { return m_bSelected; }
	//! Mark layer as selected or not.
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }
	
	//////////////////////////////////////////////////////////////////////////
	// -1 if not assigned
	void SetSurfaceTypeId( const int iId ) { m_iSurfaceTypeId = iId; }
	// -1 if not assigned
	int GetSurfaceTypeId() { return m_iSurfaceTypeId; }

	void AssignMaterial( const CString &materialName, float detailTextureScale, int detailTextureProjection );

	// usage currently deactivated (SMOOTH)
//	void SetSmooth( bool bSmooth ) { m_bSmooth = bSmooth; InvalidateMask(); }
	// usage currently deactivated (SMOOTH)
//	bool IsSmooth() const { return m_bSmooth; }

	//! Load texture if it was unloaded.
	void PrecacheTexture();
	//! Load mask if it was unloaded.
	void PrecacheMask();

	//////////////////////////////////////////////////////////////////////////
	//! Mark all layer mask sectors as invalid.
	void InvalidateAllSectors();
	void SetAllSectorsValid();

	//! Compress mask.
	void CompressMask();

	//! Export layer block.
	void ExportBlock( const CRect &rect,CXmlArchive &ar );
	//! Import layer block.
	void ImportBlock( CXmlArchive &ar,CPoint offset );

	//! Allocate mask grid for layer.
	void AllocateMaskGrid();

	void GetMemoryUsage( ICrySizer *pSizer );

	//! should always return a valid LayerId
	uint32 GetOrRequestLayerId();

	//! might return 0xffffffff
	uint32 GetCurrentLayerId();

	// CHeightmap::m_LayerIdBitmap
	void SetLayerId( const uint32 dwLayerId );

	CBitmap& GetTexturePreviewBitmap();
	CImage& GetTexturePreviewImage();

protected: // -----------------------------------------------------------------------

	enum SectorMaskFlags
	{
		SECTOR_MASK_VALID = 1,
	};

	// Convert the layer from BGR to RGB
	void BGRToRGB();

	uchar& GetSector( CPoint sector );

	// Autogenerate layer mask.
	// Arguments:
	//   vHTexOffset - offset within hmap
	void AutogenLayerMask( const CRect &rect, const int resolution, const CPoint vHTexOffset, const CFloatImage &hmap,CByteImage& mask );

	// Get native resolution for layer mask (For not autogen levels).
	int GetNativeMaskResolution() const;

private: // -----------------------------------------------------------------------

	CXTPMenuBar m_wndMenuBar;
	CXTPToolBar m_wndToolBar;

	CString					m_strLayerName;			// Name (might not be unique)

	// Layer texture
	CBitmap					m_bmpLayerTexPrev;
	CDC							m_dcLayerTexPrev;
	CString					m_strLayerTexPath;
	CSize						m_cTextureDimensions;

public: // test
	CImage					m_texture;
	CImage          m_previewImage;

public:

	ColorF					m_cLayerFilterColor;		//
	float						m_fLayerBrightness;			// used together with m_cLayerFilterColor

private:

	// Layer parameters
	float						m_LayerStart;
	float						m_LayerEnd;

	float						m_minSlopeAngle;		// min slope 0=flat..90=max steep
	float						m_maxSlopeAngle;		// max slope 0=flat..90=max steep

	CString         m_materialName;

	//////////////////////////////////////////////////////////////////////////
	// Mask.
	//////////////////////////////////////////////////////////////////////////
	// Layer mask data
	CString					m_maskFile;							//
	CByteImage			m_layerMask;						//
	CByteImage			m_scaledMask;						// Mask used when scaled mask version is needed.

	int							m_maskResolution;				//
	
	bool						m_bNeedUpdate;					//

	bool						m_bCompressedMaskValid;	//
	CMemoryBlock		m_compressedMask;				//

	bool						m_bLayerInUse;					// Should this layer be used during terrain generation ?
	bool						m_bAutoGen;							//
	
	bool						m_bSelected;						// True if layer is selected as current.
	int							m_iSurfaceTypeId;				// -1 if not assigned yet, 
	uint32					m_dwLayerId;						// used in CHeightmap::m_LayerIdBitmap, usually in the range 0..0xff, 0xffffffff means not set

	static UINT			m_iInstanceCount;				// Internal instance count, used for Uniq name assignment.

	//////////////////////////////////////////////////////////////////////////
	// Layer Sectors.
	//////////////////////////////////////////////////////////////////////////
	std::vector<unsigned char> m_maskGrid;	// elements can be 0 or SECTOR_MASK_VALID
	int							m_numSectors;
};

#endif // !defined(AFX_LAYER_H__92D64D54_E1C2_4D48_9664_8BD50F7324F8__INCLUDED_)
