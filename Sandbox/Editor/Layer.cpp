// Layer.cpp: implementation of the CLayer class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Layer.h"

#include "Heightmap.h"
#include "TerrainGrid.h"
#include "CryEditDoc.h"						// will be removed
#include "CrySizer.h"							// ICrySizer
#include "SurfaceType.h"					// CSurfaceType

//! Size of the texture preview
#define LAYER_TEX_PREVIEW_CX 128
//! Size of the texture preview
#define LAYER_TEX_PREVIEW_CY 128

#define MAX_TEXTURE_SIZE (1024*1024*2)

#define DEFAULT_MASK_RESOLUTION 4096

// Static member variables
UINT CLayer::m_iInstanceCount = 0;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLayer::CLayer() :m_cLayerFilterColor(1,1,1), m_fLayerBrightness(1)
{
	////////////////////////////////////////////////////////////////////////
	// Initialize member variables
	////////////////////////////////////////////////////////////////////////
	// One more instance
	m_iInstanceCount++;

	m_bAutoGen = true;
	m_dwLayerId=0xffffffff;		// not set yet

	m_iSurfaceTypeId = -1;

	// Create a layer name based on the instance count
	m_strLayerName.Format("Layer %i", m_iInstanceCount);

	// Init member variables
	m_LayerStart = 0;
	m_LayerEnd = 1024;
	m_strLayerTexPath = "";
	m_cTextureDimensions.cx = 0;
	m_cTextureDimensions.cy = 0;

	m_minSlopeAngle = 0;
	m_maxSlopeAngle = 90;
	m_bNeedUpdate = true;
	m_bCompressedMaskValid = false;

	// Create the DCs
	m_dcLayerTexPrev.CreateCompatibleDC(NULL);

	// Create the bitmap
	VERIFY(m_bmpLayerTexPrev.CreateBitmap(LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CX, 1, 32, NULL));
	m_dcLayerTexPrev.SelectObject(&m_bmpLayerTexPrev);

	// Layer is used
	m_bLayerInUse = true;
	m_bSelected = false;

	m_numSectors = 0;

	AllocateMaskGrid();
}

uint32 CLayer::GetCurrentLayerId()
{
	return m_dwLayerId;
}

uint32 CLayer::GetOrRequestLayerId()
{
	if(m_dwLayerId==0xffffffff)		// not set yet
	{
		bool bFree[256];

		for(uint32 dwI=0;dwI<256;++dwI)
			bFree[dwI]=true;

		GetIEditor()->GetDocument()->MarkUsedLayerIds(bFree);
		GetIEditor()->GetHeightmap()->MarkUsedLayerIds(bFree);

		for(uint32 dwI=0;dwI<256;++dwI)
			if(bFree[dwI])
			{
				m_dwLayerId=dwI;
				CryLog("GetOrRequestLayerId() '%s' m_dwLayerId=%d",GetLayerName(),m_dwLayerId);
				GetIEditor()->GetDocument()->SetModifiedFlag();
				break;
			}
	}

	assert(m_dwLayerId<=(0xff>>HEIGHTMAP_INFO_SFTYPE_SHIFT));

	return m_dwLayerId;
}


CLayer::~CLayer()
{
	////////////////////////////////////////////////////////////////////////
	// Clean up on exit
	////////////////////////////////////////////////////////////////////////

	m_iInstanceCount--;

	// Make sure the DCs are freed correctly
	m_dcLayerTexPrev.SelectObject((CBitmap *) NULL);

	// Free layer mask data
	m_layerMask.Release();
}

CString CLayer::GetTextureFilename() 
{
	return CString(PathFindFileName(LPCTSTR(m_strLayerTexPath)));
}

CString CLayer::GetTextureFilenameWithPath() const
{
	return m_strLayerTexPath;
}


//////////////////////////////////////////////////////////////////////////
void CLayer::DrawLayerTexturePreview(LPRECT rcPos, CDC *pDC)
{
	////////////////////////////////////////////////////////////////////////
	// Draw the preview of the layer texture
	////////////////////////////////////////////////////////////////////////

	ASSERT(rcPos);
	ASSERT(pDC);
	CBrush brshGray;

	if (m_bmpLayerTexPrev.m_hObject)
	{
		pDC->SetStretchBltMode(HALFTONE);
		pDC->StretchBlt(rcPos->left, rcPos->top, rcPos->right - rcPos->left, 
			rcPos->bottom - rcPos->top, &m_dcLayerTexPrev, 0, 0,
			LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CY, SRCCOPY);
	}
	else
	{
		brshGray.CreateSysColorBrush(COLOR_BTNFACE);
		pDC->FillRect(rcPos, &brshGray);
	}
}

void CLayer::Serialize( CXmlArchive &xmlAr )
{
	////////////////////////////////////////////////////////////////////////
	// Save or restore the class
	////////////////////////////////////////////////////////////////////////
  if (xmlAr.bLoading)
	{
		// We need an update
		InvalidateMask();

		////////////////////////////////////////////////////////////////////////
		// Loading
		////////////////////////////////////////////////////////////////////////

		XmlNodeRef layer = xmlAr.root;

		layer->getAttr( "Name",m_strLayerName );
		
		// Texture
		layer->getAttr( "Texture",m_strLayerTexPath );
/*
		// only for testing - can slow down loading time
		{
			bool bQualityLoss;

			CImage img;

			if(CImageUtil::LoadImage( m_strLayerTexPath,img,&bQualityLoss))
			{
				if(bQualityLoss)
					GetISystem()->Warning(VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,VALIDATOR_FLAG_TEXTURE,m_strLayerTexPath,"Layer texture format can introduce quality loss - use lossless format or quality can suffer a lot");
			}
		}
*/
		layer->getAttr( "TextureWidth",m_cTextureDimensions.cx );
		layer->getAttr( "TextureHeight",m_cTextureDimensions.cy );

		layer->getAttr( "Material",m_materialName );

		// Parameters (Altitude, Slope...)
		layer->getAttr( "AltStart",m_LayerStart );
		layer->getAttr( "AltEnd",m_LayerEnd );
		layer->getAttr( "MinSlopeAngle",m_minSlopeAngle );
		layer->getAttr( "MaxSlopeAngle",m_maxSlopeAngle );

		// In use flag
		layer->getAttr( "InUse",m_bLayerInUse );
		layer->getAttr( "AutoGenMask",m_bAutoGen );
		

		if(m_dwLayerId!=0xffffffff && m_dwLayerId>0xff)
		{
			CLogFile::WriteLine("ERROR: LayerId is out of range - value was clamped");
			m_dwLayerId=0xffffffff;
		}

		{
			CString sSurfaceType;

			layer->getAttr( "SurfaceType",sSurfaceType );

			m_iSurfaceTypeId = GetIEditor()->GetDocument()->FindSurfaceType(sSurfaceType);
		}

		if(!layer->getAttr( "LayerId",m_dwLayerId))
		{
			m_dwLayerId = m_iSurfaceTypeId;

			char str[256];
			sprintf(str,"LayerId missing - old level format - generate value from detail layer %d",m_dwLayerId);
			CLogFile::WriteLine(str);
		}
		{
			Vec3 vFilterColor(1,1,1);

			layer->getAttr( "FilterColor",vFilterColor );

			m_cLayerFilterColor=vFilterColor;
		}

		{
			m_fLayerBrightness=1;
			layer->getAttr( "LayerBrightness",m_fLayerBrightness );
		}

		void *pData;
		int nSize;
		if (xmlAr.pNamedData && xmlAr.pNamedData->GetDataBlock( CString("Layer_")+m_strLayerName,pData,nSize ))
		{
			// Load it
			if (!LoadTexture( (DWORD*)pData,m_cTextureDimensions.cx, m_cTextureDimensions.cy))
			{
				Warning( "Failed to load texture for layer %s",(const char*)m_strLayerName );
			}
		}
		else if (xmlAr.pNamedData)
		{
			// Try loading texture from external file,
			if (!m_strLayerTexPath.IsEmpty() && !LoadTexture( m_strLayerTexPath))
			{
				GetISystem()->Warning(VALIDATOR_MODULE_EDITOR,VALIDATOR_ERROR,VALIDATOR_FLAG_FILE,(const char*)m_strLayerTexPath,"Failed to load texture for layer %s",(const char*)m_strLayerName );
			}
		}

		if (!m_bAutoGen)
		{
			int maskWidth=0,maskHeight=0;
			layer->getAttr( "MaskWidth",maskWidth );
			layer->getAttr( "MaskHeight",maskHeight );
			
			m_maskResolution = maskWidth;
			if (m_maskResolution == 0)
				m_maskResolution = GetNativeMaskResolution();

			if (xmlAr.pNamedData)
			{
				bool bCompressed = true;
				CMemoryBlock *memBlock = xmlAr.pNamedData->GetDataBlock( CString("LayerMask_")+m_strLayerName,bCompressed );
				if (memBlock)
				{
					m_compressedMask = *memBlock;
					m_bCompressedMaskValid = true;
				}
				else
				{
					// No compressed block, fallback to back-compatability mode.
					if (xmlAr.pNamedData->GetDataBlock( CString("LayerMask_")+m_strLayerName,pData,nSize ))
					{
						CByteImage mask;
						mask.Attach( (unsigned char*)pData,maskWidth,maskHeight );
						if (maskWidth == DEFAULT_MASK_RESOLUTION)
						{
							m_layerMask.Allocate(m_maskResolution,m_maskResolution);
							m_layerMask.Copy(mask);
						}
						else
						{
							GenerateLayerMask( mask,m_maskResolution,m_maskResolution );
						}
					}
				}
			}
		}
	}
	else
	{
		////////////////////////////////////////////////////////////////////////
		// Storing
		////////////////////////////////////////////////////////////////////////

		XmlNodeRef layer = xmlAr.root;

		// Name
		layer->setAttr( "Name",m_strLayerName );
		
		// Texture
		layer->setAttr( "Texture",m_strLayerTexPath );
		layer->setAttr( "TextureWidth",m_cTextureDimensions.cx );
		layer->setAttr( "TextureHeight",m_cTextureDimensions.cy );

		layer->setAttr( "Material",m_materialName );

		// Parameters (Altitude, Slope...)
		layer->setAttr( "AltStart",m_LayerStart );
		layer->setAttr( "AltEnd",m_LayerEnd );
		layer->setAttr( "MinSlopeAngle",m_minSlopeAngle );
		layer->setAttr( "MaxSlopeAngle",m_maxSlopeAngle );

		// In use flag
		layer->setAttr( "InUse",m_bLayerInUse );

		// Auto mask or explicit mask.
		layer->setAttr( "AutoGenMask",m_bAutoGen );
		layer->setAttr( "LayerId",m_dwLayerId);

		{
			CString sSurfaceType = "";

			CSurfaceType *pSurfaceType = GetIEditor()->GetDocument()->GetSurfaceTypePtr(m_iSurfaceTypeId);

			if(pSurfaceType)
				sSurfaceType = pSurfaceType->GetName();

			layer->setAttr( "SurfaceType",sSurfaceType );
		}

		{
			Vec3 vFilterColor = Vec3(m_cLayerFilterColor.r,m_cLayerFilterColor.g,m_cLayerFilterColor.b);

			layer->setAttr( "FilterColor",vFilterColor );
		}

		{
			layer->setAttr( "LayerBrightness",m_fLayerBrightness );
		}

		int layerTexureSize = m_cTextureDimensions.cx*m_cTextureDimensions.cy*sizeof(DWORD);

/*		if (layerTexureSize <= MAX_TEXTURE_SIZE)
		{
			PrecacheTexture();
      xmlAr.pNamedData->AddDataBlock( CString("Layer_")+m_strLayerName,m_texture.GetData(),m_texture.GetSize() );
		}
*/

		if (!m_bAutoGen)
		{
			//////////////////////////////////////////////////////////////////////////
			// Save old stuff...
			//////////////////////////////////////////////////////////////////////////
			layer->setAttr( "MaskWidth",m_maskResolution );
			layer->setAttr( "MaskHeight",m_maskResolution );
			if (m_maskFile.IsEmpty())
				layer->setAttr( "MaskFileName",m_maskFile );

			if (!m_bCompressedMaskValid && m_layerMask.IsValid())
			{
				CompressMask();
			}
			if (m_bCompressedMaskValid)
			{
				// Store uncompressed block of data.
				if (xmlAr.pNamedData)
					xmlAr.pNamedData->AddDataBlock( CString("LayerMask_")+m_strLayerName, m_compressedMask );
			}
			else
			{
				// no mask.
			}

			//////////////////////////////////////////////////////////////////////////
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CLayer::SetAutoGen( bool bAutoGen )
{
	bool prev = m_bAutoGen;
	m_bAutoGen = bAutoGen;
	
	if (prev != m_bAutoGen)
	{
		InvalidateMask();
		if (!m_bAutoGen)
		{
			m_maskResolution = GetNativeMaskResolution();
			// Not autogenerated layer must keep its mask.
			m_layerMask.Allocate( m_maskResolution,m_maskResolution );
			m_layerMask.Clear();
			SetAllSectorsValid();
		}
		else
		{
			// Release layer mask.
			m_layerMask.Release();
			InvalidateAllSectors();
		}
	}
};

void CLayer::FillWithColor( COLORREF col,int width,int height )
{
	m_cTextureDimensions = CSize(width, height);

	// Allocate new memory to hold the bitmap data
	m_cTextureDimensions = CSize(width,height);
	m_texture.Allocate( width,height );
	uint *pTex = m_texture.GetData();
	for (int i = 0; i < width * height; i++)
	{
		*pTex++ = col;
	}
}

bool CLayer::LoadTexture(LPCTSTR lpBitmapName, UINT iWidth, UINT iHeight)
{
	////////////////////////////////////////////////////////////////////////
	// Load a layer texture out of a ressource
	////////////////////////////////////////////////////////////////////////

	CBitmap bmpLoad;
	BOOL bReturn;
	
	ASSERT(lpBitmapName);
	ASSERT(iWidth);
	ASSERT(iHeight);

	// Load the bitmap
	bReturn = bmpLoad.Attach(::LoadBitmap( AfxGetInstanceHandle(),lpBitmapName));

	if (!bReturn)
	{
		ASSERT(bReturn);
		return false;
	}

	// Save the bitmap's dimensions
	m_cTextureDimensions = CSize(iWidth, iHeight);

	// Free old tetxure data

	// Allocate new memory to hold the bitmap data
	m_cTextureDimensions = CSize(iWidth,iHeight);
	m_texture.Allocate(iWidth,iHeight);

	// Retrieve the bits from the bitmap
	VERIFY(bmpLoad.GetBitmapBits(m_texture.GetSize(),m_texture.GetData() ));	

	// Convert from BGR tp RGB
	BGRToRGB();

  Update3dengineInfo();

	return true;
}

inline bool IsPower2( int n )
{
	for (int i = 0; i < 30; i++)
	{
		if (n == (1<<i))
			return true;
	}
	return false;
}

bool CLayer::LoadTexture(CString strFileName)
{
	////////////////////////////////////////////////////////////////////////
	// Load a BMP texture into the layer
	////////////////////////////////////////////////////////////////////////
	CLogFile::FormatLine("Loading layer texture (%s)...", (const char*)strFileName );

	// Save the filename
	m_strLayerTexPath = Path::GetRelativePath(strFileName);
	if (m_strLayerTexPath.IsEmpty())
		m_strLayerTexPath = strFileName;

	bool bQualityLoss;

	bool bError=false;

	if (!CImageUtil::LoadImage( m_strLayerTexPath,m_texture,&bQualityLoss))
	{
		CLogFile::FormatLine("Error loading layer texture (%s)...", (const char*)m_strLayerTexPath );
		bError=true;
	}
	else
	if(bQualityLoss)
	{
		Warning( "Layer texture format introduces quality loss - use lossless format or quality can suffer a lot" );
		bError=true;
	}
	else
	if (!IsPower2(m_texture.GetWidth()) && !IsPower2(m_texture.GetHeight()))
	{
		Warning( "Selected Layer Texture must have power of 2 size." );
		bError=true;
	}

	if(bError)
	{
		m_strLayerTexPath = "";
		m_texture.Allocate( 4,4 );
		m_texture.Fill(0xff);
		m_cTextureDimensions = CSize(m_texture.GetWidth(),m_texture.GetHeight());
		return false;
	}
	
	// Store the size
	m_cTextureDimensions = CSize(m_texture.GetWidth(),m_texture.GetHeight());

	CBitmap bmpLoad;
	CDC dcLoad;
	// Create the DC for blitting from the loaded bitmap
	VERIFY(dcLoad.CreateCompatibleDC(NULL));

	CImage inverted;
	inverted.Allocate( m_texture.GetWidth(),m_texture.GetHeight() );
	for (int y = 0; y < m_texture.GetHeight(); y++)
	{
		for (int x = 0; x < m_texture.GetWidth(); x++)
		{
			uint val = m_texture.ValueAt(x,y);
			inverted.ValueAt(x,y) = 0xFF000000 | RGB( GetBValue(val),GetGValue(val),GetRValue(val) );
		}
	}

	bmpLoad.CreateBitmap(m_texture.GetWidth(),m_texture.GetHeight(), 1, 32, inverted.GetData() );

	// Select it into the DC
	dcLoad.SelectObject(&bmpLoad);

	// Copy it to the preview bitmap
	m_dcLayerTexPrev.SetStretchBltMode(COLORONCOLOR);
	m_dcLayerTexPrev.StretchBlt(0, 0, LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CY, &dcLoad, 
		0, 0, m_texture.GetWidth(),m_texture.GetHeight(), SRCCOPY);
	dcLoad.DeleteDC();

	m_previewImage.Allocate(LAYER_TEX_PREVIEW_CX,LAYER_TEX_PREVIEW_CY);
	CImageUtil::ScaleToFit( m_texture,m_previewImage );

	Update3dengineInfo();

	return true;
}

void CLayer::Update3dengineInfo()
{

}

 
bool CLayer::LoadTexture(DWORD *pBitmapData, UINT iWidth, UINT iHeight)
{
	////////////////////////////////////////////////////////////////////////
	// Load a texture from an array into the layer
	////////////////////////////////////////////////////////////////////////

	CDC dcLoad;
	CBitmap bmpLoad;
	DWORD *pPixData = NULL, *pPixDataEnd = NULL;

	if (iWidth == 0 || iHeight == 0)
	{
		return false;
	}

	// Allocate new memory to hold the bitmap data
	m_cTextureDimensions = CSize(iWidth,iHeight);
	m_texture.Allocate(iWidth,iHeight);

	// Copy the image data into the layer
	memcpy( m_texture.GetData(), pBitmapData, m_texture.GetSize() );

	////////////////////////////////////////////////////////////////////////
	// Generate the texture preview
	////////////////////////////////////////////////////////////////////////

	// Set the loop pointers
	pPixData = pBitmapData;
	pPixDataEnd = &pBitmapData[GetTextureWidth() * GetTextureHeight()];

	// Switch R and B
	while (pPixData != pPixDataEnd)
	{
		// Extract the bits, shift them, put them back and advance to the next pixel
		*pPixData++ = ((* pPixData & 0x00FF0000) >> 16) | 
			(* pPixData & 0x0000FF00) | ((* pPixData & 0x000000FF) << 16);
	}

	// Create the DC for blitting from the loaded bitmap
	VERIFY(dcLoad.CreateCompatibleDC(NULL));

	// Create the matching bitmap
	if (!bmpLoad.CreateBitmap(iWidth, iHeight, 1, 32, pBitmapData))
	{
		ASSERT(FALSE);
		return false;
	}

	// Select it into the DC
	dcLoad.SelectObject(&bmpLoad);

	// Copy it to the preview bitmap
	m_dcLayerTexPrev.SetStretchBltMode(COLORONCOLOR);
	m_dcLayerTexPrev.StretchBlt(0, 0, LAYER_TEX_PREVIEW_CX, LAYER_TEX_PREVIEW_CY, &dcLoad, 
		0, 0, iWidth, iHeight, SRCCOPY);

  Update3dengineInfo();

	return true;
}

bool CLayer::LoadMask( const CString &strFileName )
{
	assert( m_layerMask.IsValid() );
	if (!m_layerMask.IsValid())
		return false;
	////////////////////////////////////////////////////////////////////////
	// Load a BMP texture into the layer mask.
	////////////////////////////////////////////////////////////////////////
	CLogFile::FormatLine("Loading mask texture (%s)...", (const char*)strFileName );

	m_maskFile = Path::GetRelativePath(strFileName);
	if (m_maskFile.IsEmpty())
		m_maskFile = strFileName;

	// Load the bitmap
	CImage maskRGBA;
	if (!CImageUtil::LoadImage( m_maskFile,maskRGBA ))
	{
		CLogFile::FormatLine("Error loading layer mask (%s)...", (const char*)m_maskFile );
		return false;
	}

	CByteImage mask;
	mask.Allocate( maskRGBA.GetWidth(),maskRGBA.GetHeight() );

	unsigned char *dest = mask.GetData();
	unsigned int *src = maskRGBA.GetData();
	int size = maskRGBA.GetWidth()*maskRGBA.GetHeight();
	for (int i = 0; i < size; i++)
	{
		*dest = *src & 0xFF;
		src++;
		dest++;
	}
	GenerateLayerMask( mask,m_layerMask.GetWidth(),m_layerMask.GetHeight() );
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLayer::GenerateLayerMask( CByteImage &mask,int width,int height )
{
	// Mask is not valid anymore.
	InvalidateMask();

	// Check if mask image is same size.
	if (width == mask.GetWidth() && height == mask.GetHeight())
	{
		m_layerMask.Copy( mask );
		return;
	}

	if (m_layerMask.GetWidth() != width || m_layerMask.GetHeight() != height)
	{
		// Allocate new memory for the layer mask
		m_layerMask.Allocate(width,height);
	}

	// Scale mask image to the size of destination layer mask.
	CImageUtil::ScaleToFit( mask,m_layerMask );

//	m_currWidth = width;
//	m_currHeight = height;

//SMOOTH	if (m_layerMask.GetWidth() != mask.GetWidth() || m_layerMask.GetHeight() != mask.GetHeight())
//SMOOTH	{
//SMOOTH		if (m_bSmooth)
//SMOOTH			CImageUtil::SmoothImage( m_layerMask,2 );
//SMOOTH	}

	// All sectors of mask are valid.
	SetAllSectorsValid();
	
	// Free compressed mask.
	m_compressedMask.Free();
	m_bCompressedMaskValid = false;
}


void CLayer::GenerateWaterLayer16(float *pHeightmapPixels, UINT iHeightmapWidth,UINT iHeightmapHeight,float waterLevel )
{
	////////////////////////////////////////////////////////////////////////
	// Generate a layer that is used to render the water in the preview
	////////////////////////////////////////////////////////////////////////

	uchar *pLayerEnd = NULL;
	uchar *pLayer = NULL;

	m_bAutoGen = false;

	ASSERT(!IsBadWritePtr(pHeightmapPixels, iHeightmapWidth * 
		iHeightmapHeight * sizeof(float)));

	CLogFile::WriteLine("Generating the water layer...");

	// Allocate new memory for the layer mask
	m_layerMask.Allocate( iHeightmapWidth,iHeightmapHeight );

	uchar *pLayerMask = m_layerMask.GetData();

	// Set the loop pointers
	pLayerEnd = &pLayerMask[iHeightmapWidth * iHeightmapHeight];
	pLayer = pLayerMask;

	// Generate the layer
	while (pLayer != pLayerEnd)
		*pLayer++ = ((* pHeightmapPixels++) <= waterLevel) ? 127 : 0;

//SMOOTH
/*
	if (m_bSmooth)
	{
		// Smooth the layer
		for (j=1; j<iHeightmapHeight - 1; j++)
		{
			// Precalculate for better speed
			iCurPos = j * iHeightmapWidth + 1;
			
			for (i=1; i<iHeightmapWidth - 1; i++)
			{
				// Next pixel
				iCurPos++;
				
				// Smooth it out
				pLayerMask[iCurPos] = ftoi(
					(pLayerMask[iCurPos] +
					pLayerMask[iCurPos + 1]+
					pLayerMask[iCurPos + iHeightmapWidth] +
					pLayerMask[iCurPos + iHeightmapWidth + 1] +
					pLayerMask[iCurPos - 1]+
					pLayerMask[iCurPos - iHeightmapWidth] +
					pLayerMask[iCurPos - iHeightmapWidth - 1] +
					pLayerMask[iCurPos - iHeightmapWidth + 1] +
					pLayerMask[iCurPos + iHeightmapWidth - 1])
					* 0.11111111111f);
			}
		}
	}
*/

//	m_currWidth = iHeightmapWidth;
//	m_currHeight = iHeightmapHeight;

	m_bNeedUpdate = false;
}

void CLayer::BGRToRGB()
{
	////////////////////////////////////////////////////////////////////////
	// Convert the layer data from BGR to RGB
	////////////////////////////////////////////////////////////////////////
	PrecacheTexture();

	DWORD *pPixData = NULL, *pPixDataEnd = NULL;

	// Set the loop pointers
	pPixData = (DWORD*)m_texture.GetData();
	pPixDataEnd = pPixData + (m_texture.GetWidth()*m_texture.GetHeight());

	// Switch R and B
	while (pPixData != pPixDataEnd)
	{
		// Extract the bits, shift them, put them back and advance to the next pixel
		*pPixData++ = ((* pPixData & 0x00FF0000) >> 16) | 
			(* pPixData & 0x0000FF00) | ((* pPixData & 0x000000FF) << 16);
	}
}

void CLayer::ExportTexture(CString strFileName)
{
	////////////////////////////////////////////////////////////////////////
	// Save the texture data of this layer into a BMP file
	////////////////////////////////////////////////////////////////////////

	DWORD *pTempBGR = NULL;
	DWORD *pPixData = NULL, *pPixDataEnd = NULL;

	CLogFile::WriteLine("Exporting texture from layer data to BMP...");

	PrecacheTexture();

	if (!m_texture.IsValid())
		return;

	// Make a copy of the layer data
	CImage image;
	image.Allocate(GetTextureWidth(),GetTextureHeight());
	pTempBGR = (DWORD*)image.GetData();
	memcpy(pTempBGR, m_texture.GetData(),m_texture.GetSize());

	// Set the loop pointers
	pPixData = pTempBGR;
	pPixDataEnd = &pTempBGR[GetTextureWidth() * GetTextureHeight()];

	/*
	// Switch R and B
	while (pPixData != pPixDataEnd)
	{
		// Extract the bits, shift them, put them back and advance to the next pixel
		*pPixData++ = ((* pPixData & 0x00FF0000) >> 16) | 
			(* pPixData & 0x0000FF00) | ((* pPixData & 0x000000FF) << 16);
	}
	*/
	
	// Write the bitmap
	CImageUtil::SaveImage( strFileName,image );
}

void CLayer::ExportMask( const CString &strFileName )
{
	////////////////////////////////////////////////////////////////////////
	// Save the texture data of this layer into a BMP file
	////////////////////////////////////////////////////////////////////////
	DWORD *pTempBGR = NULL;

	CLogFile::WriteLine("Exporting layer mask to BMP...");

	CByteImage layerMask;
	uchar *pLayerMask = NULL;

	int w,h;

	if (m_bAutoGen)
	{
		int nativeResolution = GetNativeMaskResolution();
		w = h = nativeResolution;
		// Create mask at native texture resolution.
		CRect rect( 0,0,nativeResolution,nativeResolution );
		CFloatImage hmap;
		hmap.Allocate( nativeResolution,nativeResolution );
		layerMask.Allocate( nativeResolution,nativeResolution );
		// Get hmap at native resolution.
		GetIEditor()->GetHeightmap()->GetData( rect,hmap.GetWidth(),CPoint(0,0),hmap,true,true );
		AutogenLayerMask( rect,nativeResolution,CPoint(0,0),hmap,layerMask );

		pLayerMask = layerMask.GetData();
	}
	else
	{
		PrecacheMask();
		pLayerMask = m_layerMask.GetData();
		w = m_layerMask.GetWidth();
		h = m_layerMask.GetHeight();
	}

	if (w == 0 || h == 0 || pLayerMask == 0)
	{
		AfxMessageBox( "Cannot export empty mask" );
		return;
	}

	// Make a copy of the layer data
	CImage image;
	image.Allocate(w,h);
	pTempBGR = (DWORD*)image.GetData();
	for (int i = 0; i < w*h; i++)
	{
		uint col = pLayerMask[i];
		pTempBGR[i] = col | col << 8 | col << 16;
	}

	// Write the mask
	CImageUtil::SaveImage( strFileName,image );
}


void CLayer::ReleaseMask()
{
	m_layerMask.Release();
}



void CLayer::ReleaseTempResources()
{
	// Free layer mask data
	if (IsAutoGen())
		m_layerMask.Release();
	else
	{
		//CompressMask();
	}
	//	m_currWidth = 0;
	//	m_currHeight = 0;

	// If texture image is too big, release it.
	if (m_texture.GetSize() > MAX_TEXTURE_SIZE)
		m_texture.Release();
}

void CLayer::PrecacheTexture()
{
	if (m_texture.IsValid())
		return;

	if (m_strLayerTexPath.IsEmpty())
	{
		m_cTextureDimensions = CSize(4,4);
		m_texture.Allocate(m_cTextureDimensions.cx,m_cTextureDimensions.cy);
		m_texture.Fill(0xff);
	}
	else if (!CImageUtil::LoadImage( m_strLayerTexPath,m_texture ))
	{
		Error( "Error loading layer texture (%s)...", (const char*)m_strLayerTexPath );
		m_cTextureDimensions = CSize(4,4);
		m_texture.Allocate(m_cTextureDimensions.cx,m_cTextureDimensions.cy);
		m_texture.Fill(0xff);
		return;
	}
	m_cTextureDimensions.cx = m_texture.GetWidth();
	m_cTextureDimensions.cx = m_texture.GetHeight();
	// Convert from BGR tp RGB
	//BGRToRGB();
}

//////////////////////////////////////////////////////////////////////////
void CLayer::InvalidateAllSectors()
{
	// Fill all sectors with 0, (clears all flags).
	memset( &m_maskGrid[0],0,m_maskGrid.size()*sizeof(m_maskGrid[0]) );
}

//////////////////////////////////////////////////////////////////////////
void CLayer::SetAllSectorsValid()
{
	// Fill all sectors with 0xFF, (Set all flags).
	memset( &m_maskGrid[0],0xFF,m_maskGrid.size()*sizeof(m_maskGrid[0]) );
}

//////////////////////////////////////////////////////////////////////////
void CLayer::InvalidateMaskSector( CPoint sector )
{
	GetSector(sector) = 0;
	m_bCompressedMaskValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CLayer::InvalidateMask()
{
	m_bNeedUpdate = true;

	InvalidateAllSectors();
	if (m_scaledMask.IsValid())
		m_scaledMask.Release();
	/*
	if (m_layerMask.IsValid
	if (m_bCompressedMaskValid)
		m_compressedMask.Free();
	m_bCompressedMaskValid = false;
	m_compressedMask.Free();
	*/
};

//////////////////////////////////////////////////////////////////////////
void CLayer::PrecacheMask()
{
	if(GetIEditor()->GetDocument()->IsNewTerranTextureSystem() && !m_bCompressedMaskValid)
		return;

	//if (IsAutoGen())
		//return;
	if (!m_layerMask.IsValid())
	{
		m_layerMask.Allocate( m_maskResolution,m_maskResolution );
		if (!m_layerMask.IsValid())
		{
			Warning( "Layer %s compressed mask have invalid resolution %d.",(const char*)m_strLayerName,m_maskResolution );
			m_layerMask.Allocate( GetNativeMaskResolution(),GetNativeMaskResolution() );
			return;
		}
		if (m_bCompressedMaskValid)
		{
			bool bUncompressOk = m_layerMask.Uncompress( m_compressedMask );
			m_compressedMask.Free();
			m_bCompressedMaskValid = false;
			if (!bUncompressOk)
			{
				Warning( "Layer %s compressed mask have invalid resolution.",(const char*)m_strLayerName );
			}
		}
		else
		{
			// No compressed layer data.
			m_layerMask.Clear();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CByteImage& CLayer::GetMask()
{
	PrecacheMask();
	return m_layerMask;
}

//////////////////////////////////////////////////////////////////////////
bool CLayer::UpdateMaskForSector( CPoint sector, const CRect &sectorRect, const int resolution, const CPoint vTexOffset,
	const CFloatImage &hmap, CByteImage& mask )
{
	////////////////////////////////////////////////////////////////////////
	// Update the layer mask. The heightmap bits are supplied for speed
	// reasons, repeated memory allocations during batch generations of
	// layers are to slow
	////////////////////////////////////////////////////////////////////////
	PrecacheTexture();
	PrecacheMask();

	uchar &sectorFlags = GetSector(sector);
	if (sectorFlags & SECTOR_MASK_VALID && !m_bNeedUpdate)
	{
		if (m_layerMask.GetWidth() == resolution)
		{
			mask.Attach(m_layerMask);
			return true;
		}
	}
	m_bNeedUpdate = false;
	
	if (!IsAutoGen())
	{
		if (!m_layerMask.IsValid())
			return false;

		if (resolution == m_layerMask.GetWidth())
		{
			mask.Attach(m_layerMask);
		}
		else if (resolution == m_scaledMask.GetWidth())
		{
			mask.Attach(m_scaledMask);
		}
		else
		{
			m_scaledMask.Allocate( resolution,resolution );
			CImageUtil::ScaleToFit( m_layerMask,m_scaledMask );
//SMOOTH			if (m_bSmooth)
//SMOOTH				CImageUtil::SmoothImage( m_scaledMask,2 );
			mask.Attach(m_scaledMask);
		}

		// Mark this sector as valid.
		sectorFlags |= SECTOR_MASK_VALID;
		// All valid.
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Auto generate mask.
	//////////////////////////////////////////////////////////////////////////

	// If layer mask differ in size, invalidate all sectors.
	if (resolution != m_layerMask.GetWidth())
	{
		m_layerMask.Allocate( resolution,resolution );
		m_layerMask.Clear();
		InvalidateAllSectors();
	}

	// Mark this sector as valid.
	sectorFlags |= SECTOR_MASK_VALID;

	mask.Attach(m_layerMask);

	float hVal = 0.0f;

	AutogenLayerMask( sectorRect,resolution,vTexOffset,hmap,m_layerMask );
	
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLayer::UpdateMask( const CFloatImage &hmap,CByteImage& mask )
{
	PrecacheTexture();
	PrecacheMask();

	int resolution = hmap.GetWidth();

	if (!m_bNeedUpdate && m_layerMask.GetWidth() == resolution)
	{
		mask.Attach( m_layerMask );
		return true;
	}
	m_bNeedUpdate = false;
	
	if (!IsAutoGen())
	{
		if (!m_layerMask.IsValid())
			return false;

		if (resolution == m_layerMask.GetWidth())
		{
			mask.Attach(m_layerMask);
		}
		else if (resolution == m_scaledMask.GetWidth())
		{
			mask.Attach(m_scaledMask);
		}
		else
		{
			m_scaledMask.Allocate( resolution,resolution );
			CImageUtil::ScaleToFit( m_layerMask,m_scaledMask );
//SMOOTH			if (m_bSmooth)
//SMOOTH				CImageUtil::SmoothImage( m_scaledMask,2 );
			mask.Attach(m_scaledMask);
		}
		// All valid.
		SetAllSectorsValid();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Auto generate mask.
	//////////////////////////////////////////////////////////////////////////

	// If layer mask differ in size, invalidate all sectors.
	if (resolution != m_layerMask.GetWidth())
	{
		m_layerMask.Allocate( resolution,resolution );
		m_layerMask.Clear();
	}

	mask.Attach(m_layerMask);

	CRect rect(0,0,resolution,resolution);
	AutogenLayerMask( rect,hmap.GetWidth(),CPoint(0,0),hmap,m_layerMask );
	SetAllSectorsValid();

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLayer::AutogenLayerMask( const CRect &rc, const int resolution, const CPoint vHTexOffset, const CFloatImage &hmap, CByteImage& mask )
{
	CRect rect = rc;

	assert( hmap.IsValid() );
	assert( mask.IsValid() );
//	assert( hmap.GetWidth() == mask.GetWidth() );

	// Inflate rectangle.
	rect.InflateRect(1,1,1,1);

	// clip within mask - 1 pixel
	rect.left = max((int)rect.left,1);
	rect.top = max((int)rect.top,1);
	rect.right = min((int)rect.right,resolution-1);
	rect.bottom = min((int)rect.bottom,resolution-1);

	// clip within heightmap - 1 pixel
	rect.left = max((int)rect.left,1+(int)vHTexOffset.x);
	rect.top = max((int)rect.top,1+(int)vHTexOffset.y);
	rect.right = min((int)rect.right,hmap.GetWidth()-1+(int)vHTexOffset.x);
	rect.bottom = min((int)rect.bottom,hmap.GetHeight()-1+(int)vHTexOffset.y);

	// Set the loop pointers
	uchar *pLayerMask = mask.GetData();
	float *pHmap = hmap.GetData();

	// We need constant random numbers
	srand(0);

	float MinAltitude = m_LayerStart;
	float MaxAltitude = m_LayerEnd;

	float fMinSlope = tan(m_minSlopeAngle/90.1f*g_PI/2.0f);	// 0..90 -> 0..~1/0
	float fMaxSlope = tan(m_maxSlopeAngle/90.1f*g_PI/2.0f);	// 0..90 -> 0..~1/0

	// Scan the heightmap for pixels that belong to this layer
	unsigned int x,y;
	unsigned int dwMaskWidth = resolution;
	int iHeightmapWidth = hmap.GetWidth();
	float hVal = 0.0f;

	// /8 because of the way we calculate the slope	
	// /256 because the terrain height ranges from 
	float fInvHeightScale = (float)resolution 
												 /(float)GetIEditor()->GetHeightmap()->GetWidth()
												 /(float)GetIEditor()->GetHeightmap()->GetUnitSize() / 8.0f;

	for (y = rect.top; y < rect.bottom; y++)
	{
		for (x = rect.left; x < rect.right; x++)
		{
			unsigned int hpos = (x-vHTexOffset.x) + (y-vHTexOffset.y)*iHeightmapWidth;
			unsigned int lpos = x + y*dwMaskWidth;

			// Get the height value from the heightmap
			float *h = &pHmap[hpos];
			hVal = *h;

			if (hVal < MinAltitude || hVal > MaxAltitude)
			{
				pLayerMask[lpos] = 0;
				continue;
			}

			// Calculate the slope for this point
			float fs = (
				fabs((*(h + 1)) - hVal) +
				fabs((*(h - 1)) - hVal) +
				fabs((*(h + iHeightmapWidth)) - hVal) +
				fabs((*(h - iHeightmapWidth)) - hVal) +
				fabs((*(h + iHeightmapWidth + 1)) - hVal) +
				fabs((*(h - iHeightmapWidth - 1)) - hVal) +
				fabs((*(h + iHeightmapWidth - 1)) - hVal) +
				fabs((*(h - iHeightmapWidth + 1)) - hVal));

			// Compensate the smaller slope for bigger heightfields
			float fSlope = fs*fInvHeightScale;

			// Check if the current point belongs to the layer
			if (fSlope >= fMinSlope && fSlope <= fMaxSlope)
				pLayerMask[lpos] = 0xFF;
			 else
				pLayerMask[lpos] = 0;
		}
	}
	rect.DeflateRect(1,1,1,1);
	
//SMOOTH
/*	if (m_bSmooth)
	{
		// Smooth the layer
		for (y = rect.top; y < rect.bottom; y++)
		{
			unsigned int lpos = y*dwMaskWidth + rect.left;

			for (x = rect.left; x < rect.right; x++,lpos++)
			{
				// Smooth it out
				pLayerMask[lpos] =(
					(uint)pLayerMask[lpos] +
					pLayerMask[lpos + 1] +
					pLayerMask[lpos + dwMaskWidth] +
					pLayerMask[lpos + dwMaskWidth + 1] +
					pLayerMask[lpos - 1] +
					pLayerMask[lpos - dwMaskWidth] + 
					pLayerMask[lpos - dwMaskWidth - 1] +
					pLayerMask[lpos - dwMaskWidth + 1] +
					pLayerMask[lpos + dwMaskWidth - 1]) / 9;
			}
		}
	}
*/
}

//////////////////////////////////////////////////////////////////////////
void CLayer::AllocateMaskGrid()
{
	CHeightmap *pHeightmap = GetIEditor()->GetHeightmap();
	SSectorInfo si;
	pHeightmap->GetSectorsInfo( si );
	m_numSectors = si.numSectors;
	m_maskGrid.resize( si.numSectors*si.numSectors );
	memset( &m_maskGrid[0],0,m_maskGrid.size()*sizeof(m_maskGrid[0]) );

	m_maskResolution = si.surfaceTextureSize;
}

//////////////////////////////////////////////////////////////////////////
uchar& CLayer::GetSector( CPoint sector )
{
	int p = sector.x + sector.y*m_numSectors;
	assert( p >= 0 && p < m_maskGrid.size() );
	return m_maskGrid[p];
}


//////////////////////////////////////////////////////////////////////////
void CLayer::SetLayerId( const uint32 dwLayerId )
{
	assert(dwLayerId<=(0xff>>HEIGHTMAP_INFO_SFTYPE_SHIFT));

	m_dwLayerId=dwLayerId;
	assert(m_dwLayerId==0xffffffff || m_dwLayerId<=0xff);
	
//	CryLog("SetLayerId() '%s' m_dwLayerId=%d",GetLayerName(),m_dwLayerId);
}

//////////////////////////////////////////////////////////////////////////
void CLayer::CompressMask()
{
	if (m_bCompressedMaskValid)
		return;

	m_bCompressedMaskValid = false;
	m_compressedMask.Free();

	// Compress mask.
	if (m_layerMask.IsValid())
	{
		m_layerMask.Compress( m_compressedMask );
		m_bCompressedMaskValid = true;
	}
	m_layerMask.Release();
	m_scaledMask.Release();
}

//////////////////////////////////////////////////////////////////////////
int CLayer::GetSize() const
{
	int size = sizeof(*this);
	size += m_texture.GetSize();
	size += m_layerMask.GetSize();
	size += m_scaledMask.GetSize();
	size += m_compressedMask.GetSize();
	size += m_maskGrid.size()*sizeof(unsigned char);
	return size;
}

//////////////////////////////////////////////////////////////////////////
//! Export layer block.
void CLayer::ExportBlock( const CRect &rect,CXmlArchive &xmlAr )
{
	// ignore autogenerated layers.
	//if (m_bAutoGen)
		//return;

	XmlNodeRef node = xmlAr.root;

	PrecacheMask();
	if (!m_layerMask.IsValid())
		return;

	node->setAttr( "Name",m_strLayerName );
	node->setAttr( "MaskWidth",m_layerMask.GetWidth() );
	node->setAttr( "MaskHeight",m_layerMask.GetHeight() );

	CRect subRc( 0,0,m_layerMask.GetWidth(),m_layerMask.GetHeight() );
	subRc &= rect;

	node->setAttr( "X1",subRc.left );
	node->setAttr( "Y1",subRc.top );
	node->setAttr( "X2",subRc.right );
	node->setAttr( "Y2",subRc.bottom );

	if (!subRc.IsRectEmpty())
	{
		CByteImage subImage;
		subImage.Allocate( subRc.Width(),subRc.Height() );
		m_layerMask.GetSubImage( subRc.left,subRc.top,subRc.Width(),subRc.Height(),subImage );

		xmlAr.pNamedData->AddDataBlock( CString("LayerMask_")+m_strLayerName,subImage.GetData(),subImage.GetSize() );
	}
}

//! Import layer block.
void CLayer::ImportBlock( CXmlArchive &xmlAr,CPoint offset )
{
	// ignore autogenerated layers.
	//if (m_bAutoGen)
		//return;

	XmlNodeRef node = xmlAr.root;

	PrecacheMask();
	if (!m_layerMask.IsValid())
		return;

	CRect subRc;

	node->getAttr( "X1",subRc.left );
	node->getAttr( "Y1",subRc.top );
	node->getAttr( "X2",subRc.right );
	node->getAttr( "Y2",subRc.bottom );

	void *pData;
	int nSize;
	if (xmlAr.pNamedData->GetDataBlock( CString("LayerMask_")+m_strLayerName, pData,nSize ))
	{
		CByteImage subImage;
		subImage.Attach( (unsigned char*)pData,subRc.Width(),subRc.Height() );
		m_layerMask.SetSubImage( subRc.left+offset.x,subRc.top+offset.y,subImage );
	}
}

//////////////////////////////////////////////////////////////////////////
int CLayer::GetNativeMaskResolution() const
{
	CHeightmap *pHeightmap = GetIEditor()->GetHeightmap();
	SSectorInfo si;
	pHeightmap->GetSectorsInfo( si );
	return si.surfaceTextureSize;
}

//////////////////////////////////////////////////////////////////////////
CBitmap& CLayer::GetTexturePreviewBitmap()
{
	return m_bmpLayerTexPrev;
}

//////////////////////////////////////////////////////////////////////////
CImage& CLayer::GetTexturePreviewImage()
{
	return m_previewImage;
}

void CLayer::GetMemoryUsage( ICrySizer *pSizer )
{
	pSizer->Add(*this);

	pSizer->Add((char *)m_compressedMask.GetBuffer(),m_compressedMask.GetSize());
	pSizer->Add(m_maskGrid);
	pSizer->Add(m_layerMask);
	pSizer->Add(m_scaledMask);
}

//////////////////////////////////////////////////////////////////////////
void CLayer::AssignMaterial( const CString &materialName, float detailTextureScale, int detailTextureProjection )
{
	int iSurfaceTypeId = -1;

	CCryEditDoc *doc = GetIEditor()->GetDocument();
	for (int i = 0; i < doc->GetSurfaceTypeCount(); i++)
	{
		CSurfaceType *pSrfType = doc->GetSurfaceTypePtr(i);
		if (stricmp(pSrfType->GetMaterial(),materialName) == 0)
		{
			// if all details match a preexisting surface type, use that one
			if(detailTextureScale == pSrfType->GetDetailTextureScale().x && detailTextureProjection == pSrfType->GetProjAxis())
			{
				iSurfaceTypeId = i;
				break;
			}
		}
	}
	if (iSurfaceTypeId >= 0)
	{
		// If surface type with same material found.
		m_iSurfaceTypeId = iSurfaceTypeId;
	}
	else
	{
		int nNumSrfTypeReferences = 0;
		// Check if we reference unique surface type.
		for (int i = 0; i < doc->GetLayerCount(); i++)
		{
			if (doc->GetLayer(i)->GetSurfaceTypeId() == m_iSurfaceTypeId)
				nNumSrfTypeReferences++;
		}
		if (nNumSrfTypeReferences == 1 && m_iSurfaceTypeId >= 0)
		{
			// yes, unique surface type. Just set the properties
			CSurfaceType *pSrfType = doc->GetSurfaceTypePtr(m_iSurfaceTypeId);
			pSrfType->SetMaterial(materialName);
			pSrfType->SetDetailTextureScale(Vec3(detailTextureScale, detailTextureScale, 0.0f));
			pSrfType->SetProjAxis(detailTextureProjection);
		}
		else
		{
			// Create a new surface type.
			CSurfaceType *pSrfType = new CSurfaceType;
			pSrfType->SetMaterial(materialName);
			pSrfType->SetName(materialName);
			pSrfType->SetDetailTextureScale(Vec3(detailTextureScale, detailTextureScale, 0.0f));
			pSrfType->SetProjAxis(detailTextureProjection);
			m_iSurfaceTypeId = doc->AddSurfaceType( pSrfType );
		}
	}
}
