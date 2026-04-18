// Heightmap.cpp: implementation of the CHeightmap class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Heightmap.h"
#include "Noise.h"
#include "Layer.h"
#include "CryEditDoc.h"
#include "TerrainFormulaDlg.h"
#include "VegetationMap.h"
#include "TerrainGrid.h"
#include "Util\DynamicArray2D.h"
#include "GameEngine.h"

#include <I3DEngine.h>

#define DEFAULT_TEXTURE_SIZE 4096

//! Size of terrain sector in meters.
#define SECTOR_SIZE_IN_METERS 64

//! Size of noise array.
#define NOISE_ARRAY_SIZE 512

//! Filename used when Holding/Fetching heightmap.
#define HEIGHTMAP_HOLD_FETCH_FILE "Heightmap.hld"

#define OVVERIDE_LAYER_SURFACETYPE_FROM 128

//////////////////////////////////////////////////////////////////////////
//! Undo object for heightmap modifications.
class CUndoHeightmapModify : public IUndoObject
{
public:
	CUndoHeightmapModify( int x1,int y1,int width,int height,CHeightmap *heightmap )
	{
		m_hmap.Attach( heightmap->GetData(),heightmap->GetWidth(),heightmap->GetHeight() );
		// Store heightmap block.
		m_rc = CRect( x1,y1,x1+width,y1+height );
		m_rc &= CRect( 0,0,m_hmap.GetWidth(),m_hmap.GetHeight() );
		m_hmap.GetSubImage( m_rc.left,m_rc.top,m_rc.Width(),m_rc.Height(),m_undo );
	}
protected:
	virtual void Release() { delete this; };
	virtual int GetSize() {	return sizeof(*this) + m_undo.GetSize() + m_redo.GetSize(); };
	virtual const char* GetDescription() { return "Heightmap Modify"; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			// Store for redo.
			m_hmap.GetSubImage( m_rc.left,m_rc.top,m_rc.Width(),m_rc.Height(),m_redo );
		}
		// Restore image.
		m_hmap.SetSubImage( m_rc.left,m_rc.top,m_undo );

		int w = m_rc.Width();
		if(w<m_rc.Height()) w =m_rc.Height();

		GetIEditor()->GetHeightmap()->UpdateEngineTerrain( m_rc.left,m_rc.top,w,w,true,false );
		//GetIEditor()->GetHeightmap()->UpdateEngineTerrain( m_rc.left,m_rc.top,m_rc.Width(),m_rc.Height(),true,false );

		if (bUndo)
		{
			BBox box;
			box.min = GetIEditor()->GetHeightmap()->HmapToWorld( CPoint(m_rc.left,m_rc.top) );
			box.max = GetIEditor()->GetHeightmap()->HmapToWorld( CPoint(m_rc.left+w,m_rc.top+w) );
			box.min.z -= 10000;
			box.max.z += 10000;
			GetIEditor()->UpdateViews(eUpdateHeightmap,&box);
		}
	}
	virtual void Redo()
	{
		if (m_redo.IsValid())
		{
			// Restore image.
			m_hmap.SetSubImage( m_rc.left,m_rc.top,m_redo );
			GetIEditor()->GetHeightmap()->UpdateEngineTerrain( m_rc.left,m_rc.top,m_rc.Width(),m_rc.Height(),true,false );
			
			{
				BBox box;
				box.min = GetIEditor()->GetHeightmap()->HmapToWorld( CPoint(m_rc.left,m_rc.top) );
				box.max = GetIEditor()->GetHeightmap()->HmapToWorld( CPoint(m_rc.left+m_rc.Width(),m_rc.top+m_rc.Height()) );
				box.min.z -= 10000;
				box.max.z += 10000;
				GetIEditor()->UpdateViews(eUpdateHeightmap,&box);
			}
		}
	}

private:
	CRect m_rc;
	TImage<float> m_undo;
	TImage<float> m_redo;

	TImage<float> m_hmap;		// memory data is shared
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for heightmap modifications.
class CUndoHeightmapInfo : public IUndoObject
{
public:
	CUndoHeightmapInfo( int x1,int y1,int width,int height,CHeightmap *heightmap )
	{
		m_hmap.Attach( heightmap->m_LayerIdBitmap.GetData(),heightmap->GetWidth(),heightmap->GetHeight() );
		// Store heightmap block.
		m_hmap.GetSubImage( x1,y1,width,height,m_undo );
		m_rc = CRect(x1,y1,x1+width,y1+height);
	}
protected:
	virtual void Release() { delete this; };
	virtual int GetSize()	{	return sizeof(*this) + m_undo.GetSize() + m_redo.GetSize(); };
	virtual const char* GetDescription() { return "Heightmap Hole"; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			// Store for redo.
			m_hmap.GetSubImage( m_rc.left,m_rc.top,m_rc.Width(),m_rc.Height(),m_redo );
		}
		// Restore image.
		m_hmap.SetSubImage( m_rc.left,m_rc.top,m_undo );
		GetIEditor()->GetHeightmap()->UpdateEngineHole( m_rc.left,m_rc.top,m_rc.Width(),m_rc.Height() );
	}
	virtual void Redo()
	{
		if (m_redo.IsValid())
		{
			// Restore image.
			m_hmap.SetSubImage( m_rc.left,m_rc.top,m_redo );
			GetIEditor()->GetHeightmap()->UpdateEngineHole( m_rc.left,m_rc.top,m_rc.Width(),m_rc.Height() );
		}
	}

private:
	CRect m_rc;
	TImage<unsigned char> m_undo;
	TImage<unsigned char> m_redo;
	TImage<unsigned char> m_hmap;
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void ReduceTerrainPrecisionTo12Bit( IConsoleCmdArgs*pArgs )
{
	if(!GetIEditor()->GetHeightmap())
		return;

	float * pHeightmap = GetIEditor()->GetHeightmap()->GetData();
	int nDataSize = GetIEditor()->GetHeightmap()->GetWidth()*GetIEditor()->GetHeightmap()->GetHeight();
	float fPrecisionScale = GetIEditor()->GetHeightmap()->GetShortPrecisionScale();

	for(int i=0; i<nDataSize; i++)
	{
		unsigned int h = ftoi( pHeightmap[i] * fPrecisionScale+0.5f );
		h &= ~SURFACE_TYPE_MASK;
		
		if(fabs(double(pHeightmap[i] - (0.01f + h/fPrecisionScale)>0.001f)))
			pHeightmap[i] = 0.01f + h/fPrecisionScale;
	}

	GetIEditor()->GetHeightmap()->UpdateEngineTerrain(true);
}

CHeightmap::CHeightmap() :m_TerrainRGBTexture("TerrainTexture.pak")
{
	m_fMaxHeight = 256;
	m_bUpdateLayerIdBitmap=false;

	m_pNoise = NULL;
	// Init member variables
	m_pHeightmap = NULL;
	m_iWidth = 0;
	m_iHeight = 0;
	m_pNoise = NULL;
	m_fWaterLevel = 16;
	m_unitSize = 2;
//	m_cachedResolution = 0;
	m_numSectors = 0;

	m_vegetationMap = new CVegetationMap;

	m_textureSize = DEFAULT_TEXTURE_SIZE;

	m_terrainGrid = new CTerrainGrid(this);

	InitNoise();

	//////////////////////////////////////////////////////////////////////////
	// Load water texture.
	m_waterImage.Allocate(128,128);

	// Retrieve the bits from the bitmap
	CBitmap bmpLoad;
	VERIFY( bmpLoad.Attach(::LoadBitmap(AfxGetApp()->m_hInstance,MAKEINTRESOURCE(IDB_WATER))) );
	VERIFY( bmpLoad.GetBitmapBits(128 * 128 * sizeof(DWORD), m_waterImage.GetData()) );	
	//////////////////////////////////////////////////////////////////////////
	if (gEnv)
		gEnv->pConsole->AddCommand("ReduceTerrainPrecisionTo12Bit", ReduceTerrainPrecisionTo12Bit);
}

CHeightmap::~CHeightmap()
{
	// Reset the heightmap
	CleanUp();

	delete m_terrainGrid;

	// Remove the noise array
	if (m_pNoise)
	{
		delete m_pNoise;
		m_pNoise = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::CleanUp()
{
	////////////////////////////////////////////////////////////////////////
	// Free the data
	////////////////////////////////////////////////////////////////////////

	if (m_pHeightmap)
	{
		delete [] m_pHeightmap;
		m_pHeightmap = NULL;
	}

	m_TerrainRGBTexture.FreeData();

	if (m_vegetationMap)
	{
		delete m_vegetationMap;
		m_vegetationMap = 0;
	}

	m_iWidth = 0;
	m_iHeight = 0;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::Resize( int iWidth, int iHeight,int unitSize,bool bCleanOld )
{
	////////////////////////////////////////////////////////////////////////
	// Resize the heightmap
	////////////////////////////////////////////////////////////////////////

	ASSERT(iWidth && iHeight);

	int prevWidth,prevHeight,prevUnitSize;
	prevWidth = m_iWidth;
	prevHeight = m_iHeight;
	prevUnitSize = m_unitSize;

	t_hmap * prevHeightmap = 0;
	TImage<unsigned char> prevLayerIdBitmap;

	if (bCleanOld)
	{
		// Free old heightmap
		CleanUp();
	}
	else
	{
		if (m_pHeightmap)
		{
			// Remember old state.
			//prevHeightmap.Allocate( m_iWidth,m_iHeight );
			//memcpy( prevHeightmap.GetData(),m_pHeightmap,prevHeightmap.GetSize() );
			prevHeightmap = m_pHeightmap;
			m_pHeightmap = 0;
		}
		if (m_LayerIdBitmap.IsValid())
		{
			prevLayerIdBitmap.Allocate( m_iWidth,m_iHeight );
			memcpy( prevLayerIdBitmap.GetData(),m_LayerIdBitmap.GetData(),prevLayerIdBitmap.GetSize() );
		}
	}

	// Save width and height
	m_iWidth = iWidth;
	m_iHeight = iHeight;
	m_unitSize = unitSize;


	int sectorSize = SECTOR_SIZE_IN_METERS;
	m_numSectors = (m_iWidth*m_unitSize) / sectorSize;

	if(GetIEditor()->GetDocument()->IsNewTerranTextureSystem())
	{
		uint32 dwTerrainSizeInMeters = m_iWidth*unitSize;
		uint32 dwTerrainTextureResolution = dwTerrainSizeInMeters/2;			// terrain texture resolution scaled with heightmap - bad: hard coded factor 2m=1texel

		uint32 dwTileResolution = 512;			// to avoid having too many tiles we try to get big ones

		if(dwTerrainTextureResolution<=dwTileResolution)
			dwTileResolution=128;

		uint32 dwTileCount = dwTerrainTextureResolution/dwTileResolution;

		m_TerrainRGBTexture.AllocateTiles(dwTileCount,dwTileCount,dwTileResolution);
	}

	// Allocate new data
	if (m_pHeightmap)
		delete [] m_pHeightmap;
	m_pHeightmap = new t_hmap[iWidth * iHeight];

	CryLog("allocating editor height map (%dx%d)*4",iWidth,iHeight);

	Verify();

	m_LayerIdBitmap.Allocate( iWidth,iHeight );

	if(	prevWidth < m_iWidth || prevHeight < m_iHeight)
		Clear();

	if (bCleanOld)
	{
		// Set to zero
		Clear();
	}
	else
	{
		// Copy from previous data.
		if (prevHeightmap && prevLayerIdBitmap.IsValid())
		{
			CWaitCursor wait;
			if(prevUnitSize == m_unitSize)
				CopyFrom( prevHeightmap,prevLayerIdBitmap.GetData(),prevWidth );
			else
				CopyFromInterpolate( prevHeightmap,prevLayerIdBitmap.GetData(),prevWidth,  float(prevUnitSize) / m_unitSize);
		}
	}

	if(prevHeightmap)
		delete [] prevHeightmap;

	m_terrainGrid->InitSectorGrid( m_numSectors );
	m_terrainGrid->SetResolution( m_textureSize );

	if (bCleanOld)
	{
    m_vegetationMap = new CVegetationMap;
		m_vegetationMap->Allocate( this );
	}
	else
	{
		CWaitCursor wait;
		bool bVegSaved = false;
		CXmlArchive ar("Temp");
		if (m_vegetationMap)
		{
			m_vegetationMap->Serialize( ar );
			bVegSaved = true;
		}

		if (m_vegetationMap)
			delete m_vegetationMap;
		m_vegetationMap = new CVegetationMap;
		m_vegetationMap->Allocate( this );

		if (bVegSaved)
		{
			// Load back from archive.
			ar.bLoading = true;
			m_vegetationMap->Serialize(ar);
		}
	}

	if (!bCleanOld)
	{
		int numLayers = GetIEditor()->GetDocument()->GetLayerCount();
		for (int i = 0; i < numLayers; i++)
		{
			CLayer *pLayer = GetIEditor()->GetDocument()->GetLayer(i);
			pLayer->AllocateMaskGrid();
		}
	}

	if(	prevWidth != m_iWidth || prevHeight != m_iHeight)
	{
		ITerrain *pTerrain = GetIEditor()->Get3DEngine()->GetITerrain();
		if (pTerrain)
		{
			GetIEditor()->Get3DEngine()->DeleteTerrain();
			GetIEditor()->GetGameEngine()->InitTerrain();
		}
	}

	// We modified the heightmap.
	SetModified();
}


//////////////////////////////////////////////////////////////////////////
void CHeightmap::PaintLayerId( const float fpx, const float fpy, const SEditorPaintBrush &brush, const uint32 dwLayerId )
{
	assert(dwLayerId<=(0xff>>HEIGHTMAP_INFO_SFTYPE_SHIFT));
	uint8 ucLayerInfoData = (dwLayerId<<HEIGHTMAP_INFO_SFTYPE_SHIFT)&HEIGHTMAP_INFO_SFTYPE_MASK;

	SEditorPaintBrush cpyBrush=brush;

	cpyBrush.bBlended=false;
	cpyBrush.color=ucLayerInfoData;

	CImagePainter painter;

	painter.PaintBrush(fpx,fpy,m_LayerIdBitmap,cpyBrush);
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::MarkUsedLayerIds( bool bFree[256] ) const
{
	for(uint32 dwY=0;dwY<m_iHeight;++dwY)
	for(uint32 dwX=0;dwX<m_iWidth;++dwX)
	{
		uint32 ucId = GetLayerIdAt(dwX,dwY);

		assert(ucId<=(0xff>>HEIGHTMAP_INFO_SFTYPE_SHIFT));

		bFree[ucId]=false;
	}
}


//////////////////////////////////////////////////////////////////////////
CPoint CHeightmap::WorldToHmap( const Vec3 &wp )
{
	//swap x/y.
	return CPoint(wp.y/m_unitSize,wp.x/m_unitSize);
}
	
//////////////////////////////////////////////////////////////////////////
Vec3 CHeightmap::HmapToWorld( CPoint hpos )
{
	return Vec3(hpos.y*m_unitSize,hpos.x*m_unitSize,0);
}

//////////////////////////////////////////////////////////////////////////
CRect CHeightmap::WorldBoundsToRect( const BBox &worldBounds )
{
	CPoint p1 = WorldToHmap(worldBounds.min);
	CPoint p2 = WorldToHmap(worldBounds.max);
	if (p1.x > p2.x)
		std::swap(p1.x,p2.x);
	if (p1.y > p2.y)
		std::swap(p1.y,p2.y);
	CRect rc;
	rc.SetRect(p1,p2);
	rc.IntersectRect(rc,CRect(0,0,m_iWidth,m_iHeight));
	return rc;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::InvalidateLayers()
{
	GetIEditor()->GetDocument()->InvalidateLayers();
}

void CHeightmap::Clear()
{
	if (m_iWidth && m_iHeight)
	{
		memset( m_pHeightmap,0,sizeof(t_hmap)*m_iWidth*m_iHeight );
		m_LayerIdBitmap.Clear();
	}
};

void CHeightmap::SetMaxHeight( float fMaxHeight )
{
	m_fMaxHeight = fMaxHeight;

	// Clamp heightmap to the max height.
	int nSize = GetWidth()*GetHeight();
	for (int i = 0; i < nSize; i++)
	{
		if (m_pHeightmap[i] > m_fMaxHeight)
			m_pHeightmap[i] = m_fMaxHeight;
	}
}

void CHeightmap::LoadBMP(LPCSTR pszFileName, bool bNoiseAndFilter)
{
	////////////////////////////////////////////////////////////////////////
	// Load a BMP file as current heightmap
	////////////////////////////////////////////////////////////////////////
	CImage image;
	CImage hmap;

	if (!CImageUtil::LoadImage( pszFileName,image ))
	{
		AfxMessageBox( _T("Load image failed."),MB_OK|MB_ICONERROR );
		return;
	}

	if (image.GetWidth() != m_iWidth || image.GetHeight() != m_iHeight)
	{
		hmap.Allocate( m_iWidth,m_iHeight );
		CImageUtil::ScaleToFit( image,hmap );
	}
	else
	{
		hmap.Attach( image );
	}

	float fPrecisionScale = 1.0f / GetBytePrecisionScale();

	uint *pData = hmap.GetData();
	int size = hmap.GetWidth()*hmap.GetHeight();
	for (int i = 0; i < size; i++)
	{
		// Extract a color channel and rescale the value
		// before putting it into the heightmap
		int col = pData[i] & 0x000000FF;
		m_pHeightmap[i] = col / fPrecisionScale;
	}
	/*
	// Noise and filter ?
	if (bNoiseAndFilter)
	{
		Smooth();
		Noise();
	}
	*/

	// We modified the heightmap.
	SetModified();

	CLogFile::FormatLine("Heightmap loaded from file %s", pszFileName);
}

void CHeightmap::SaveImage(LPCSTR pszFileName, bool bNoiseAndFilter)
{
	uint *pImageData = NULL;
	unsigned int i, j;
	uint8 iColor;
	t_hmap *pHeightmap = NULL;
	UINT iWidth = GetWidth();
	UINT iHeight = GetHeight();

	// Allocate memory to export the heightmap
	CImage image;
	image.Allocate(iWidth,iHeight);
	pImageData = image.GetData();

	// Get a pointer to the heightmap data
	pHeightmap = GetData();

	float fPrecisionScale = GetBytePrecisionScale();

	// BMP
	// Write heightmap into the image data array
	for (j=0; j<iHeight; j++)
	{
		for (i=0; i<iWidth; i++)
		{
			// Get a normalized grayscale value from the heigthmap
			iColor = (uint8)__min((pHeightmap[i + j * iWidth]*fPrecisionScale), 255.0f);
					
			// Create a BGR grayscale value and store it in the image
			// data array
			pImageData[i + j * iWidth] = (iColor << 16) | (iColor << 8) | iColor;
		}
	}
	
	// Save the heightmap into the bitmap	
	CImageUtil::SaveImage( pszFileName,image );
}

void CHeightmap::SavePGM( const CString &pgmFile )
{
	CImage image;
	image.Allocate(m_iWidth,m_iHeight);

	float fPrecisionScale = GetShortPrecisionScale();
	for (int j=0; j<m_iHeight; j++)
	{
		for (int i=0; i<m_iWidth; i++)
		{
			unsigned int h = ftoi( GetXY(i,j)*fPrecisionScale+0.5f );
			if (h > 0xFFFF) h = 0xFFFF;
			image.ValueAt(i,j) = ftoi( h );
		}
	}
	CImageUtil::SaveImage( pgmFile,image );
}

void CHeightmap::LoadPGM( const CString &pgmFile )
{
	CImage image;
	CImageUtil::LoadImage( pgmFile,image );
	if (image.GetWidth() != m_iWidth || image.GetHeight() != m_iHeight)
	{
		MessageBox( NULL,"PGM dimensions do not match dimensions of heighmap","Warning",MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	float fPrecisionScale = GetShortPrecisionScale();

	for (int j=0; j<m_iHeight; j++)
	{
		for (int i=0; i<m_iWidth; i++)
		{
			GetXY(i,j) = image.ValueAt(i,j) / fPrecisionScale;
		}
	}
}

//! Save heightmap in RAW format.
void CHeightmap::SaveRAW(  const CString &rawFile )
{
	CString str;
	FILE *file = fopen( rawFile,"wb" );
	if (!file)
	{
		str.Format( "Error saving file %s",(const char*)rawFile );
		MessageBox( NULL,str,"Warning",MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	CWordImage image;
	image.Allocate( m_iWidth,m_iHeight );

	float fPrecisionScale = GetShortPrecisionScale();

	for (int j=0; j<m_iHeight; j++)
	{
		for (int i=0; i<m_iWidth; i++)
		{
			unsigned int h = ftoi( GetXY(i,j)*fPrecisionScale+0.5f );
			if (h > 0xFFFF) h = 0xFFFF;
			image.ValueAt(i,j) = h;
		}
	}

	fwrite( image.GetData(),image.GetSize(),1,file );

	fclose(file);
}

//! Save heightmap in RAW format.
void CHeightmap::SaveFRAW(  const CString &rawFile )
{
	CString str;
	FILE *file = fopen( rawFile,"wb" );
	if (!file)
	{
		str.Format( "Error saving file %s",(const char*)rawFile );
		MessageBox( NULL,str,"Warning",MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	TImage<float> image;
	image.Allocate( m_iWidth,m_iHeight );

	float fPrecisionScale = GetShortPrecisionScale();

	for (int j=0; j<m_iHeight; j++)
	{
		for (int i=0; i<m_iWidth; i++)
		{
			image.ValueAt(i,j) = GetXY(i,j);
		}
	}

	fwrite( image.GetData(),image.GetSize(),1,file );

	fclose(file);
}

//! Load heightmap from RAW format.
void	CHeightmap::LoadRAW(  const CString &rawFile )
{
	CString str;
	FILE *file = fopen( rawFile,"rb" );
	if (!file)
	{
		str.Format( "Error loading file %s",(const char*)rawFile );
		MessageBox( NULL,str,"Warning",MB_OK|MB_ICONEXCLAMATION );
		return;
	}
	fseek( file,0,SEEK_END );
	int fileSize = ftell( file );
	fseek( file,0,SEEK_SET );

	float fPrecisionScale = GetShortPrecisionScale();
	if (fileSize != m_iWidth*m_iHeight*2)
	{
		str.Format( "Bad RAW file, RAW file must be %dxd 16bit image",m_iWidth,m_iHeight );
		MessageBox( NULL,str,"Warning",MB_OK|MB_ICONEXCLAMATION );
		fclose(file);
		return;
	}
	CWordImage image;
	image.Allocate( m_iWidth,m_iHeight );
	fread( image.GetData(),image.GetSize(),1,file );

	for (int j=0; j<m_iHeight; j++)
	{
		for (int i=0; i<m_iWidth; i++)
		{
			SetXY( i,j,image.ValueAt(i,j)/fPrecisionScale );
		}
	}

	fclose(file);
}

void CHeightmap::Noise()
{
	////////////////////////////////////////////////////////////////////////
	// Add noise to the heightmap
	////////////////////////////////////////////////////////////////////////
	InitNoise();

	Verify();

	unsigned int i, j;
	long iCurPos;
	UINT iNoiseSwapMode, iNoiseX, iNoiseY;
	UINT iNoiseOffsetX, iNoiseOffsetY;
	
	assert(m_pNoise);

	// Calculate the way we have to swap the noise. We do this to avoid
	// singularities when a noise array is aplied more than once
	srand(clock());
	iNoiseSwapMode = rand() % 5;

	// Calculate a noise offset for the same reasons
	iNoiseOffsetX = rand() % NOISE_ARRAY_SIZE;
	iNoiseOffsetY = rand() % NOISE_ARRAY_SIZE;

	iCurPos = 0;
	for (j=1; j<m_iHeight - 1; j++)
	{
		// Precalculate for better speed
		iCurPos = j * m_iWidth  + 1;

		for (i=1; i<m_iWidth - 1; i++)
		{
			// Next pixel
			iCurPos++;

			// Apply only to places that don't have zero height
			if (m_pHeightmap[iCurPos])
			{
				// Skip amnything below the water level
				if (m_pHeightmap[iCurPos] < m_fWaterLevel)
					continue;

				// Swap the noise
				switch (iNoiseSwapMode)
				{
				case 0:
					iNoiseX = i;
					iNoiseY = j;
					break;
				case 1:
					iNoiseX = j;
					iNoiseY = i;
					break;
				case 2:
					iNoiseX = m_iWidth - i;
					iNoiseY = j;
					break;
				case 3:
					iNoiseX = i;
					iNoiseY = m_iHeight - j;
					break;
				case 4:
					iNoiseX = m_iWidth - i;
					iNoiseY = m_iHeight - j;
					break;
				}

				// Add the random noise offset
				iNoiseX += iNoiseOffsetX;
				iNoiseY += iNoiseOffsetY;

				// Add the signed noise
				m_pHeightmap[iCurPos] = __min(m_fMaxHeight, 
					__max(m_fWaterLevel,
								m_pHeightmap[iCurPos] + m_pNoise->m_Array[iNoiseX % NOISE_ARRAY_SIZE][iNoiseY % NOISE_ARRAY_SIZE]));
			}
		}
	}

	// We modified the heightmap.
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::Smooth( CFloatImage &hmap,const CRect &rect )
{
	int w = hmap.GetWidth();
	int h = hmap.GetHeight();

	int x1 = max((int)rect.left+2,1);
	int y1 = max((int)rect.top+2,1);
	int x2 = min((int)rect.right-2,w-1);
	int y2 = min((int)rect.bottom-2,h-1);

	t_hmap* pData = hmap.GetData();

	int i,j,pos;
	// Smooth it
	for (j=y1; j < y2; j++)
	{
		pos = j*w;
		for (i=x1; i < x2; i++)
		{
			pData[i + pos] = 
				(pData[i + pos] +
				pData[i + 1 + pos] +
				pData[i - 1 + pos] +
				pData[i + pos+w] +
				pData[i + pos-w] + 
				pData[(i - 1) + pos-w] +
				pData[(i + 1) + pos-w] +
				pData[(i - 1) + pos+w] +
				pData[(i + 1) + pos+w])
				* (1.0f/9.0f);
		}
	}
	/*
	for (j=y2-1; j > y1; j--)
	{
		pos = j*w;
		for (i=x2-1; i > x1; i--)
		{
			pData[i + pos] = 
				(pData[i + pos] +
				pData[i + 1 + pos] +
				pData[i - 1 + pos] +
				pData[i + pos+w] +
				pData[i + pos-w] + 
				pData[(i - 1) + pos-w] +
				pData[(i + 1) + pos-w] +
				pData[(i - 1) + pos+w] +
				pData[(i + 1) + pos+w]) 
				* (1.0f/9.0f);
		}
	}
	*/
}

void CHeightmap::Smooth()
{
	////////////////////////////////////////////////////////////////////////
	// Smooth the heightmap
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;

	Verify();

	// Smooth it
	for (i=1; i<m_iWidth-1; i++)
		for (j=1; j<m_iHeight-1; j++)
		{
			m_pHeightmap[i + j * m_iWidth] = 
				(m_pHeightmap[i + j * m_iWidth] +
				 m_pHeightmap[(i + 1) + j * m_iWidth] +
				 m_pHeightmap[i + (j + 1) * m_iWidth] +
				 m_pHeightmap[(i + 1) + (j + 1) * m_iWidth] + 
				 m_pHeightmap[(i - 1) + j * m_iWidth] +
				 m_pHeightmap[i + (j - 1) * m_iWidth] + 
				 m_pHeightmap[(i - 1) + (j - 1) * m_iWidth] +
				 m_pHeightmap[(i + 1) + (j - 1) * m_iWidth] +
				 m_pHeightmap[(i - 1) + (j + 1) * m_iWidth])
				 / 9.0f;
		}

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::Invert()
{
	////////////////////////////////////////////////////////////////////////
	// Invert the heightmap
	////////////////////////////////////////////////////////////////////////

	unsigned int i;

	Verify();

	for (i=0; i<m_iWidth * m_iHeight; i++)
	{
		m_pHeightmap[i] = m_fMaxHeight - m_pHeightmap[i];
		if (m_pHeightmap[i] > m_fMaxHeight)
			m_pHeightmap[i] = m_fMaxHeight;
		if (m_pHeightmap[i] < 0)
			m_pHeightmap[i] = 0;
	}

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::RemoveWater()
{
	////////////////////////////////////////////////////////////////////////
	// Remove any water from the heightmap
	////////////////////////////////////////////////////////////////////////

	unsigned int i;

	Verify();

	for (i=0; i<m_iWidth * m_iHeight; i++)
		m_pHeightmap[i] = __max(m_pHeightmap[i], m_fWaterLevel);

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::Normalize()
{
	////////////////////////////////////////////////////////////////////////
	// Normalize the heightmap to a 0 - m_fMaxHeight range
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;
	float fLowestPoint = 512000.0f, fHighestPoint = -512000.0f;
	float fValueRange;
	float fHeight;

	Verify();

	// Find the value range
	for (i=0; i<m_iWidth; i++)
		for (j=0; j<m_iHeight; j++)
		{
			fLowestPoint = __min(fLowestPoint, GetXY(i, j));
			fHighestPoint = __max(fHighestPoint, GetXY(i, j));
		}

	// Storing the value range in this way saves us a division and a multiplication
	fValueRange = (1.0f / (fHighestPoint - (float) fLowestPoint)) * m_fMaxHeight;

	// Normalize the heightmap
	for (i=0; i<m_iWidth; i++)
		for (j=0; j<m_iHeight; j++)
		{
			fHeight = GetXY(i, j);

//			fHeight += (float) fabs(fLowestPoint);
			fHeight -= fLowestPoint;
			fHeight *= fValueRange;

//			fHeight=128.0f;

			SetXY(i, j, fHeight);
		}

	fLowestPoint = 512000.0f, fHighestPoint = -512000.0f;

	// Find the value range
/*	for (i=0; i<m_iWidth; i++)
		for (j=0; j<m_iHeight; j++)
		{
			fLowestPoint = __min(fLowestPoint, GetXY(i, j));
			fHighestPoint = __max(fHighestPoint, GetXY(i, j));
		}*/

	// We modified the heightmap.
	SetModified();
}

bool CHeightmap::GetDataEx(t_hmap *pData, UINT iDestWidth, bool bSmooth, bool bNoise)
{
	////////////////////////////////////////////////////////////////////////
	// Retrieve heightmap data. Scaling and noising is optional
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;
	long iXSrcFl, iXSrcCe, iYSrcFl, iYSrcCe;
	float fXSrc, fYSrc;
	float fHeight[4];
	float fHeightWeight[4];
	float fHeightBottom;
	float fHeightTop;
	UINT dwHeightmapWidth = GetWidth();
	t_hmap *pDataStart = pData;

	Verify();

	bool bProgress = iDestWidth > 1024;
	// Only log significant allocations. This also prevents us from cluttering the
	// log file during the lightmap preview generation
	if (iDestWidth > 1024)
		CLogFile::FormatLine("Retrieving heightmap data (Width: %i)...", iDestWidth);

	CWaitProgress wait( "Scaling Heightmap",bProgress );
	
	// Loop trough each field of the new image and interpolate the value
	// from the source heightmap
	for (j=0; j<iDestWidth; j++)
	{
		if (bProgress)
		{
			if (!wait.Step( j*100/iDestWidth ))
				return false;
		}

		// Calculate the average source array position
		fYSrc = ((float) j / (float) iDestWidth) * dwHeightmapWidth;
		assert(fYSrc >= 0.0f && fYSrc <= dwHeightmapWidth);
		
		// Precalculate floor and ceiling values. Use fast asm integer floor and
		// fast asm float / integer conversion
		iYSrcFl = ifloor(fYSrc);
		iYSrcCe = iYSrcFl+1;

		// Clamp the ceiling coordinates to a save range
		if (iYSrcCe >= (int) dwHeightmapWidth)
			iYSrcCe = dwHeightmapWidth - 1;

		// Distribution between top and bottom height values
		fHeightWeight[3] = fYSrc-(float)iYSrcFl;
		fHeightWeight[2] = 1.0f-fHeightWeight[3];

		for (i=0; i<iDestWidth; i++)
		{
			// Calculate the average source array position
			fXSrc = ((float) i / (float) iDestWidth) * dwHeightmapWidth;
			assert(fXSrc >= 0.0f && fXSrc <= dwHeightmapWidth);

			// Precalculate floor and ceiling values. Use fast asm integer floor and
			// fast asm float / integer conversion
			iXSrcFl = ifloor(fXSrc);
			iXSrcCe = iXSrcFl+1;
			
			// Distribution between left and right height values
			fHeightWeight[1] = fXSrc-(float)iXSrcFl;
			fHeightWeight[0] = 1.0f-fHeightWeight[1];
/*
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
			*/


			if (iXSrcCe >= (int) dwHeightmapWidth)
				iXSrcCe = dwHeightmapWidth - 1;

			// Get the four nearest height values
			fHeight[0] = (float) m_pHeightmap[iXSrcFl + iYSrcFl * dwHeightmapWidth];
			fHeight[1] = (float) m_pHeightmap[iXSrcCe + iYSrcFl * dwHeightmapWidth];
			fHeight[2] = (float) m_pHeightmap[iXSrcFl + iYSrcCe * dwHeightmapWidth];
			fHeight[3] = (float) m_pHeightmap[iXSrcCe + iYSrcCe * dwHeightmapWidth];

			// Interpolate between the four nearest height values
	
			// Get the height for the given X position trough interpolation between
			// the left and the right height
			fHeightBottom = (fHeight[0] * fHeightWeight[0] + fHeight[1] * fHeightWeight[1]);
			fHeightTop    = (fHeight[2] * fHeightWeight[0] + fHeight[3] * fHeightWeight[1]);

			// Set the new value in the destination heightmap
			*pData++ = (t_hmap) (fHeightBottom * fHeightWeight[2] + fHeightTop * fHeightWeight[3]);
		}
	}

	if (bNoise)
	{
		InitNoise();

		pData = pDataStart;
		// Smooth it
		for (i=1; i < iDestWidth-1; i++)
			for (j=1; j <  iDestWidth-1; j++)
			{
				*pData++ += (((float)rand())/RAND_MAX) * 1.0f/16.0f;
			}
	}

	if (bSmooth)
	{
		CFloatImage img;
		img.Attach( pDataStart,iDestWidth,iDestWidth );
		Smooth( img,CRect(0,0,iDestWidth,iDestWidth) );
	}

	// Cache rescaled data.
//	m_cachedResolution = iDestWidth;

	/*
	int nSize = m_cachedResolution*m_cachedResolution*sizeof(float);
	CMemoryBlock temp;
	temp.Allocate( nSize );
	temp.Copy( pDataStart,nSize );
	temp.Compress( m_cachedHeightmap );
	*/
	
	return true;
}



//////////////////////////////////////////////////////////////////////////
bool CHeightmap::GetData( CRect &srcRect, const int resolution, const CPoint vTexOffset, CFloatImage &hmap, bool bSmooth,bool bNoise )
{
	////////////////////////////////////////////////////////////////////////
	// Retrieve heightmap data. Scaling and noising is optional
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;
	int iXSrcFl, iXSrcCe, iYSrcFl, iYSrcCe;
	float fXSrc, fYSrc;
	float fHeight[4];
	float fHeightWeight[4];
	float fHeightBottom;
	float fHeightTop;
	UINT dwHeightmapWidth = GetWidth();
	
	int width = hmap.GetWidth();

	// clip within source
	int x1 = max((int)srcRect.left,0);
	int y1 = max((int)srcRect.top,0);
	int x2 = min((int)srcRect.right,resolution);
	int y2 = min((int)srcRect.bottom,resolution);

	// clip within dest hmap
	x1 = max(x1,(int)vTexOffset.x);
	y1 = max(y1,(int)vTexOffset.y);
	x2 = min(x2,hmap.GetWidth()+(int)vTexOffset.x);
	y2 = min(y2,hmap.GetHeight()+(int)vTexOffset.y);

	float fScaleX = m_iWidth/(float)hmap.GetWidth();
	float fScaleY = m_iHeight/(float)hmap.GetHeight();

	t_hmap *pDataStart = hmap.GetData();

	int trgW = x2 - x1;

	bool bProgress = trgW > 1024;
	CWaitProgress wait( "Scaling Heightmap",bProgress );
	
	// Loop trough each field of the new image and interpolate the value
	// from the source heightmap
	for (j = y1; j < y2; j++)
	{
		if (bProgress)
		{
			if (!wait.Step( (j-y1)*100/(y2-y1) ))
				return false;
		}

		t_hmap *pData = &pDataStart[(j-vTexOffset.y)*width + x1-vTexOffset.x];

		// Calculate the average source array position
		fYSrc = j*fScaleY;
		assert(fYSrc >= 0.0f && fYSrc <= dwHeightmapWidth);
		
		// Precalculate floor and ceiling values. Use fast asm integer floor and
		// fast asm float / integer conversion
		iYSrcFl = ifloor(fYSrc);
		iYSrcCe = iYSrcFl+1;

		// Clamp the ceiling coordinates to a save range
		if (iYSrcCe >= (int) dwHeightmapWidth)
			iYSrcCe = dwHeightmapWidth - 1;


		// Distribution between top and bottom height values
		fHeightWeight[3] = fYSrc - (float)iYSrcFl;
		fHeightWeight[2] =1.0f - fHeightWeight[3];

		for (i = x1; i < x2; i++)
		{
			// Calculate the average source array position
			fXSrc = i * fScaleX;
			assert(fXSrc >= 0.0f && fXSrc <= dwHeightmapWidth);

			// Precalculate floor and ceiling values. Use fast asm integer floor and
			// fast asm float / integer conversion
			iXSrcFl = ifloor(fXSrc);
			iXSrcCe = iXSrcFl+1;
			
			if (iXSrcCe >= (int) dwHeightmapWidth)
				iXSrcCe = dwHeightmapWidth - 1;

			// Distribution between left and right height values
			fHeightWeight[1] = fXSrc - (float)iXSrcFl;
			fHeightWeight[0] = 1.0f - fHeightWeight[1];

			// Get the four nearest height values
			fHeight[0] = (float) m_pHeightmap[iXSrcFl + iYSrcFl * dwHeightmapWidth];
			fHeight[1] = (float) m_pHeightmap[iXSrcCe + iYSrcFl * dwHeightmapWidth];
			fHeight[2] = (float) m_pHeightmap[iXSrcFl + iYSrcCe * dwHeightmapWidth];
			fHeight[3] = (float) m_pHeightmap[iXSrcCe + iYSrcCe * dwHeightmapWidth];

			// Interpolate between the four nearest height values
	
			// Get the height for the given X position trough interpolation between
			// the left and the right height
			fHeightBottom = (fHeight[0] * fHeightWeight[0] + fHeight[1] * fHeightWeight[1]);
			fHeightTop    = (fHeight[2] * fHeightWeight[0] + fHeight[3] * fHeightWeight[1]);

			// Set the new value in the destination heightmap
			*pData++ = (t_hmap) (fHeightBottom * fHeightWeight[2] + fHeightTop * fHeightWeight[3]);
		}
	}


	// Only if requested resolution, higher then current resolution.
	if(resolution > m_iWidth)
	if(bNoise)
	{
		InitNoise();

		int ye=hmap.GetHeight();
		int xe=hmap.GetWidth();

		t_hmap *pData = pDataStart;

		// add noise
		for (int y = 0; y < ye; y++)
		{
			for (int x = 0; x < xe; x++)
			{
				*pData++ += (((float)rand())/RAND_MAX) * 1.0f/16.0f;
				//*pData++ += (((float)rand())/RAND_MAX) * 1.0f/8.0f;
			}
		}
	}

	if(bSmooth)
		Smooth( hmap,srcRect-vTexOffset );

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CHeightmap::GetPreviewBitmap( DWORD *pBitmapData,int width,bool bSmooth, bool bNoise,CRect *pUpdateRect,bool bShowWater )
{
	bool res = false;
	CFloatImage hmap;

	hmap.Allocate(width,width);
	t_hmap *pHeightmap = hmap.GetData();

	CRect destUpdateRect = CRect(0,0,width,width);

	if (pUpdateRect)
	{
		CRect destUpdateRect = *pUpdateRect;
		float fScale = (float)width / m_iWidth;
		destUpdateRect.left *= fScale;
		destUpdateRect.right *= fScale;
		destUpdateRect.top *= fScale;
		destUpdateRect.bottom *= fScale;

		res = GetData( destUpdateRect,width,CPoint(0,0),hmap,bSmooth,bNoise );
	}
	else
	{
		res = GetDataEx( pHeightmap,width,bSmooth,bNoise );
	}

	float fByteScale = GetBytePrecisionScale();
	if (res)
	{
		t_hmap *pH = pHeightmap;

		uint32 *pWaterTexData = m_waterImage.GetData();

		CRect bounds(0,0,width,width);
		if (pUpdateRect)
		{
			CRect destUpdateRect = *pUpdateRect;
			float fScale = (float)width / m_iWidth;
			destUpdateRect.left *= fScale;
			destUpdateRect.right *= fScale;
			destUpdateRect.top *= fScale;
			destUpdateRect.bottom *= fScale;

			bounds.IntersectRect(bounds,destUpdateRect);
		}

		if (bShowWater)
		{
			uint32 nWaterLevel = ftoi(m_fWaterLevel * fByteScale);
			// Fill the preview with the heightmap image
			for (int iY=bounds.top; iY < bounds.bottom; iY++)
				for (int iX=bounds.left; iX < bounds.right; iX++)
				{
					// Use the height value as grayscale if the current heightmap point is above the water
					// level. Use a texel from the tiled water texture when it is below the water level
					uint32 val = ftoi(pH[iX + iY*width] * fByteScale);
					pBitmapData[iX + iY*width] = (val >= nWaterLevel) ? RGB(val,val,val) : pWaterTexData[(iX%128)+(iY%128)*128];
				}
		}
		else
		{
			// Fill the preview with the heightmap image
			for (int iY=bounds.top; iY < bounds.bottom; iY++)
				for (int iX=bounds.left; iX < bounds.right; iX++)
				{
					// Use the height value as grayscale
					uint32 val = ftoi(pH[iX + iY*width] * fByteScale);
					pBitmapData[iX + iY*width] = RGB(val,val,val);
				}
		}
	}

	return res;
}

void CHeightmap::GenerateTerrain(const SNoiseParams &noiseParam)
{
	////////////////////////////////////////////////////////////////////////
	// Generate a new terrain with the parameters stored in sParam
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;
	CDynamicArray2D cHeightmap(GetWidth(), GetHeight());
	CNoise cNoise;
	float fYScale = 255.0f;
	float *pArrayShortcut = NULL;
	DWORD *pImageData = NULL;
	DWORD *pImageDataStart = NULL;
	
	SNoiseParams sParam = noiseParam;

	ASSERT(sParam.iWidth == m_iWidth && sParam.iHeight == m_iHeight);

	//////////////////////////////////////////////////////////////////////
	// Generate the noise array
	//////////////////////////////////////////////////////////////////////

 	AfxGetMainWnd()->BeginWaitCursor();

	CLogFile::WriteLine("Noise...");

	// Set the random value
	srand(sParam.iRandom);

	// Process layers
	for (i=0; i<sParam.iPasses; i++)
	{
		// Apply the fractal noise function to the array
		cNoise.FracSynthPass(&cHeightmap, sParam.fFrequency, fYScale, 
			sParam.iWidth, sParam.iHeight, FALSE);

		// Modify noise generation parameters
		sParam.fFrequency *= sParam.fFrequencyStep;
		fYScale *= sParam.fFade;	
	}

	//////////////////////////////////////////////////////////////////////
	// Store the generated terrain in the heightmap
	//////////////////////////////////////////////////////////////////////

	for (j=0; j<m_iHeight; j++)
		for (i=0; i<m_iWidth; i++)
			SetXY(i, j, cHeightmap.m_Array[i][j]);

	//////////////////////////////////////////////////////////////////////
	// Perform some modifications on the heightmap
	//////////////////////////////////////////////////////////////////////

	// Smooth the heightmap and normalize it
	for (i=0; i<sParam.iSmoothness; i++)
		Smooth();

	Normalize();
	Noise();
	MakeIsle();
			
	//////////////////////////////////////////////////////////////////////
	// Finished
	//////////////////////////////////////////////////////////////////////

	// We modified the heightmap.
	SetModified();

	// All layers need to be generated from scratch
	GetIEditor()->GetDocument()->InvalidateLayers();

	AfxGetMainWnd()->EndWaitCursor();
}


float CHeightmap::CalcHeightScale() const
{
	return 1.0f / (float)(GetWidth() * GetUnitSize());
}

void CHeightmap::SmoothSlope()
{
	//////////////////////////////////////////////////////////////////////
	// Remove areas with high slope from the heightmap
	//////////////////////////////////////////////////////////////////////

	UINT iCurPos;
	float fAverage;
	unsigned int i, j;

	CLogFile::WriteLine("Smoothing the slope of the heightmap...");

	// Remove the high slope areas (horizontal)
	for (j=1; j<m_iHeight - 1; j++)
	{
		// Precalculate for better speed
		iCurPos = j * m_iWidth + 1;

		for (i=1; i<m_iWidth - 1; i++)
		{
			// Next pixel
			iCurPos++;

			// Get the average value for this area
			fAverage = 
				(m_pHeightmap[iCurPos]                + m_pHeightmap[iCurPos + 1]            + m_pHeightmap[iCurPos + m_iWidth] +
				 m_pHeightmap[iCurPos + m_iWidth + 1] + m_pHeightmap[iCurPos - 1]            + m_pHeightmap[iCurPos - m_iWidth] + 
				 m_pHeightmap[iCurPos - m_iWidth - 1] + m_pHeightmap[iCurPos - m_iWidth + 1] + m_pHeightmap[iCurPos + m_iWidth - 1])
				 * 0.11111111111f;

			// Clamp the surrounding values to the given level
			ClampToAverage(&m_pHeightmap[iCurPos], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth], fAverage);
			// TODO: ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth - 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth - 1], fAverage);
		}
	}

	// Remove the high slope areas (vertical)
	for (i=1; i<m_iWidth - 1; i++)
	{
		// Precalculate for better speed
		iCurPos = i;

		for (j=1; j<m_iHeight - 1; j++)
		{
			// Next pixel
			iCurPos += m_iWidth;

			// Get the average value for this area
			fAverage = 
				(m_pHeightmap[iCurPos]                + m_pHeightmap[iCurPos + 1]            + m_pHeightmap[iCurPos + m_iWidth] +
				 m_pHeightmap[iCurPos + m_iWidth + 1] + m_pHeightmap[iCurPos - 1]            + m_pHeightmap[iCurPos - m_iWidth] + 
				 m_pHeightmap[iCurPos - m_iWidth - 1] + m_pHeightmap[iCurPos - m_iWidth + 1] + m_pHeightmap[iCurPos + m_iWidth - 1])
				 * 0.11111111111f;

			// Clamp the surrounding values to the given level
			ClampToAverage(&m_pHeightmap[iCurPos], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth - 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth - 1], fAverage);
		}
	}

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::ClampToAverage(t_hmap *pValue, float fAverage)
{
	//////////////////////////////////////////////////////////////////////
	// Used during slope removal to clamp height values into a normalized
	// rage
	//////////////////////////////////////////////////////////////////////

	float fClampedVal;

	// Does the heightvalue differ heavily from the average value ?
	if (fabs(*pValue - fAverage) > fAverage * 0.001f)
	{
		// Negativ / Positiv ?
		if (*pValue < fAverage)
			fClampedVal = fAverage - (fAverage * 0.001f);
		else
			fClampedVal = fAverage + (fAverage * 0.001f);
		
		// Renormalize it
		ClampHeight(fClampedVal);

		*pValue = fClampedVal;
	}
}

void CHeightmap::MakeIsle()
{
	//////////////////////////////////////////////////////////////////////
	// Convert any terrain into an isle
	//////////////////////////////////////////////////////////////////////

	int i, j;
	t_hmap *pHeightmapData = m_pHeightmap;
//	UINT m_iHeightmapDiag;
	float fDeltaX, fDeltaY;
	float fDistance;
	float fCurHeight, fFade;
	
	CLogFile::WriteLine("Modifying heightmap to an isle...");

	// Calculate the length of the diagonale trough the heightmap
//	m_iHeightmapDiag = (UINT) sqrt(GetWidth() * GetWidth() +
//		GetHeight() * GetHeight());
	float fMaxDistance = sqrtf((GetWidth()/2) * (GetWidth()/2) +	(GetHeight()/2) * (GetHeight()/2));

	for (j=0; j<m_iHeight; j++)
	{
		// Calculate the distance delta
		fDeltaY = (float)abs( (int)(j - m_iHeight/2) );

		for (i=0; i<m_iWidth; i++)
		{
			// Calculate the distance delta
			fDeltaX = (float)abs( (int)(i-m_iWidth/2) );
			
			// Calculate the distance
			fDistance = (float) sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);

			// Calculate the fade-off
//			fFade = __min(0.5f, __min(sinf((float) i / ((float) m_iWidth / 3.12f)),
//				sinf((float) j / ((float) m_iHeight / 3.12f))));
//			float fCos=cosf(__clamp(fDistance/fMaxDistance*3.1416f, 0.0f, 3.1415f));
//			fFade = __clamp(fCos*0.5f+0.5f, 0.0f, 1.0f);
//			float fCosX=cosf(__clamp(sinf((float)i/(float)m_iWidth*3.1416f)*3.1416f, 0.0f, 3.1415f));
//			float fCosY=cosf(__clamp(sinf((float)j/(float)m_iHeight*3.1416f)*3.1416f, 0.0f, 3.1415f));
			float fCosX=sinf((float)i/(float)m_iWidth*3.1416f);
			float fCosY=sinf((float)j/(float)m_iHeight*3.1416f);
			fFade=fCosX*fCosY;
//			fFade = __clamp((fCosX*0.5f+0.5f)*(fCosY*0.5f+0.5f), 0.0f, 1.0f);

			// Only apply heavy modification to the borders of the island
//			fFade = fFade * fFade + (1.0f - fFade) * (fFade * 2);
			fFade=1.0-((1.0f-fFade)*(1.0f-fFade));

			// Modify the value
			fCurHeight = *pHeightmapData;
//			fCurHeight += fFade * 10.0f;
//	    fCurHeight *= sqrtf(fFade);
			fCurHeight *= fFade;
			
			// Clamp
			ClampHeight(fCurHeight);

			// Write the value back and andvance
			*pHeightmapData++ = fCurHeight;
		}
	}

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::Flatten(float fFactor)
{
	////////////////////////////////////////////////////////////////////////
	// Increase the number of flat areas on the heightmap (TODO: Fix !)
	////////////////////////////////////////////////////////////////////////

	t_hmap *pHeightmapData = m_pHeightmap;
	t_hmap *pHeightmapDataEnd = &m_pHeightmap[m_iWidth * m_iHeight];
	float fRes;

	CLogFile::WriteLine("Flattening heightmap...");

	// Perform the conversion
	while (pHeightmapData != pHeightmapDataEnd)
	{
		// Get the exponential value for this height value
		fRes = ExpCurve(*pHeightmapData, 128.0f, 0.985f);

		// Is this part of the landscape a potential flat area ?
		// Only modify parts of the landscape that are above the water level
		if (fRes < 100 && *pHeightmapData > m_fWaterLevel)
		{
			// Yes, apply the factor to it
			*pHeightmapData = (t_hmap) (*pHeightmapData * fFactor);

			// When we use a factor greater than 0.5, we don't want to drop below
			// the water level
			*pHeightmapData++ = __max(m_fWaterLevel, *pHeightmapData);
		}
		else
		{
			// No, use the exponential function to make smooth transitions
			*pHeightmapData++ = (t_hmap) fRes;
		}
	}

	// We modified the heightmap.
	SetModified();
}

float CHeightmap::ExpCurve(float v, float fCover, float fSharpness)
{
	//////////////////////////////////////////////////////////////////////
	// Exponential function
	//////////////////////////////////////////////////////////////////////

	float c;

	c = v - fCover;

	if (c < 0)
		c = 0;

	return m_fMaxHeight - (float) ((pow(fSharpness, c)) * m_fMaxHeight);
}

void CHeightmap::MakeBeaches()
{
	//////////////////////////////////////////////////////////////////////
	// Create flat areas around beaches
	//////////////////////////////////////////////////////////////////////

  CTerrainFormulaDlg TerrainFormulaDlg;  
  TerrainFormulaDlg.m_dParam1 = 5;
  TerrainFormulaDlg.m_dParam2 = 1000;
  TerrainFormulaDlg.m_dParam3 = 1;  
  if(TerrainFormulaDlg.DoModal()!=IDOK)
    return;

	unsigned int i, j;
	t_hmap *pHeightmapData = NULL;
	t_hmap *pHeightmapDataStart = NULL;
	double dCurHeight;
	
	CLogFile::WriteLine("Applaing formula ...");

	// Get the water level
	double dWaterLevel = m_fWaterLevel;

	// Make the beaches
	for (j=0; j<m_iHeight; j++)
	{
		for (i=0; i<m_iWidth; i++)
		{
			dCurHeight = m_pHeightmap[i + j * m_iWidth];


			// Center water level at zero
			dCurHeight -= dWaterLevel;
    
      // do nothing with small values but increase big values
      dCurHeight = dCurHeight * (1.0 + fabs(dCurHeight)*TerrainFormulaDlg.m_dParam1);    

      // scale back (can be automated)
      dCurHeight /= TerrainFormulaDlg.m_dParam2;
      
			// Convert the coordinates back to the old range
			dCurHeight += dWaterLevel;

      // move flat area up out of water
      dCurHeight += TerrainFormulaDlg.m_dParam3;
  
      // check range
			if(dCurHeight > m_fMaxHeight)
        dCurHeight = m_fMaxHeight;
      else if(dCurHeight<0)
        dCurHeight=0;
	
			// Write the value back and andvance to the next pixel
			m_pHeightmap[i + j * m_iWidth] = dCurHeight;
		}
	}

  m_pHeightmap[0]=0;
  m_pHeightmap[1]=255;

	// Normalize because the beach creation distorted the value range
///	Normalize();

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::LowerRange(float fFactor)
{
	//////////////////////////////////////////////////////////////////////
	// Lower the value range of the heightmap, effectively making it
	// more flat
	//////////////////////////////////////////////////////////////////////

	unsigned int i;
	float fWaterHeight = m_fWaterLevel;

	CLogFile::WriteLine("Lowering range...");

	// Lower the range, make sure we don't put anything below the water level
	for (i=0; i<m_iWidth * m_iHeight; i++)
		m_pHeightmap[i] = ((m_pHeightmap[i] - fWaterHeight) * fFactor) + fWaterHeight;

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::Randomize()
{
	////////////////////////////////////////////////////////////////////////
	// Add a small amount of random noise
	////////////////////////////////////////////////////////////////////////

	unsigned int i;

	CLogFile::WriteLine("Lowering range...");

	// Add the noise
	for (i=0; i<m_iWidth * m_iHeight; i++)
		m_pHeightmap[i] += (float) rand() / RAND_MAX*8.0f - 4.0f;

	// Normalize because we might have valid the valid range
	Normalize();

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::DrawSpot(unsigned long iX, unsigned long iY,
						              uint8 iWidth, float fAddVal, 
						              float fSetToHeight, bool bAddNoise)
{
	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////

	long i, j;
	long iPosX, iPosY, iIndex;
	float fMaxDist, fAttenuation, fJSquared;
	float fCurHeight;
	
	assert(m_pNoise);

	if (bAddNoise)
		InitNoise();

	// Calculate the maximum distance
	fMaxDist = sqrtf((float) ((iWidth / 2) * (iWidth / 2) + (iWidth / 2) * (iWidth / 2)));

	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->RecordUndo( new CUndoHeightmapModify(iX-iWidth,iY-iWidth,iWidth*2,iWidth*2,this) );

	for (j=(long) -iWidth; j<iWidth; j++)
	{
		// Precalculate
		iPosY = iY + j;
		fJSquared = (float) (j * j);
		
		for (i=(long) -iWidth; i<iWidth; i++)
		{
			// Calculate the position
			iPosX = iX + i;
			
			// Skip invalid locations
			if (iPosX < 0 || iPosY < 0 || 
				iPosX > (long) m_iWidth - 1 || iPosY > (long) m_iHeight - 1)
			{
				continue;
			}

			// Calculate the array index
			iIndex = iPosX + iPosY * m_iWidth;

			// Calculate attenuation factor
			fAttenuation = 1.0f - __min(1.0f, sqrtf((float) (i * i + fJSquared)) / fMaxDist);

			// Which drawing mode are we in ?
			if (fSetToHeight >= 0.0f)
			{
				// Set to height mode, modify the location towards the specified height
				fCurHeight = m_pHeightmap[iIndex];
				m_pHeightmap[iIndex] *= 4.0f;
				m_pHeightmap[iIndex] += (1.0f - fAttenuation) * fCurHeight + fAttenuation * fSetToHeight;
				m_pHeightmap[iIndex] /= 5.0f;
			}
			else if (bAddNoise)
			{
				// Noise brush
				if (fAddVal > 0.0f)
				{
					m_pHeightmap[iIndex] += fAddVal/100 * 
						((float) fabs(m_pNoise->m_Array[iPosX % NOISE_ARRAY_SIZE][iPosY % NOISE_ARRAY_SIZE]))
						* fAttenuation;
				}
				else
				{
					m_pHeightmap[iIndex] += fAddVal/100 *
						(float) (-fabs(m_pNoise->m_Array[iPosX % NOISE_ARRAY_SIZE][iPosY % NOISE_ARRAY_SIZE]))
						* fAttenuation;
				}
			}
			else
			{
				// No, modify the location with a normal brush
				m_pHeightmap[iIndex] += fAddVal * fAttenuation;
			}

			// Clamp
			ClampHeight(m_pHeightmap[iIndex]);
		}
	}

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::DrawSpot2( int iX, int iY, int radius,float insideRadius,float fHeight,float fHardness,bool bAddNoise,float noiseFreq,float noiseScale )
{
	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	int i, j;
	int iPosX, iPosY, iIndex;
	float fMaxDist, fAttenuation, fYSquared;
	float fCurHeight;

	if (bAddNoise)
		InitNoise();

	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->RecordUndo( new CUndoHeightmapModify(iX-radius,iY-radius,radius*2,radius*2,this) );

	// Calculate the maximum distance
	fMaxDist = radius;

	for (j=(long) -radius; j<radius; j++)
	{
		// Precalculate
		iPosY = iY + j;
		fYSquared = (float) (j * j);
		
		for (i=(long) -radius; i<radius; i++)
		{
			// Calculate the position
			iPosX = iX + i;
			
			// Skip invalid locations
			if (iPosX < 0 || iPosY < 0 ||	iPosX > m_iWidth - 1 || iPosY > m_iHeight - 1)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + i*i);
			if (dist > fMaxDist)
				continue;

			// Calculate the array index
			iIndex = iPosX + iPosY * m_iWidth;

			// Calculate attenuation factor
			if (dist <= insideRadius)
				fAttenuation = 1.0f;
			else
				fAttenuation = 1.0f - __min(1.0f, (dist-insideRadius)/fMaxDist);

			// Set to height mode, modify the location towards the specified height
			fCurHeight = m_pHeightmap[iIndex];
			float dh = fHeight - fCurHeight;

			float h = fCurHeight + (fAttenuation)*dh*fHardness;
			

			if (bAddNoise)
			{
				int nx = ftoi(iPosX*noiseFreq) % NOISE_ARRAY_SIZE;
				int ny = ftoi(iPosY*noiseFreq) % NOISE_ARRAY_SIZE;
				float noise = m_pNoise->m_Array[nx][ny];
				h += (float)(noise)*fAttenuation*noiseScale;
			}
			

			// Clamp
			ClampHeight(h);
			m_pHeightmap[iIndex] = h;
		}
	}

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::RiseLowerSpot( int iX, int iY, int radius,float insideRadius,float fHeight,float fHardness,bool bAddNoise,float noiseFreq,float noiseScale )
{
	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	int i, j;
	int iPosX, iPosY, iIndex;
	float fMaxDist, fAttenuation, fYSquared;
	float fCurHeight;

	if (bAddNoise)
		InitNoise();

	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->RecordUndo( new CUndoHeightmapModify(iX-radius,iY-radius,radius*2,radius*2,this) );

	// Calculate the maximum distance
	fMaxDist = radius;

	for (j=(long) -radius; j<radius; j++)
	{
		// Precalculate
		iPosY = iY + j;
		fYSquared = (float) (j * j);

		for (i=(long) -radius; i<radius; i++)
		{
			// Calculate the position
			iPosX = iX + i;

			// Skip invalid locations
			if (iPosX < 0 || iPosY < 0 ||	iPosX > m_iWidth - 1 || iPosY > m_iHeight - 1)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + i*i);
			if (dist > fMaxDist)
				continue;

			// Calculate the array index
			iIndex = iPosX + iPosY * m_iWidth;

			// Calculate attenuation factor
			if (dist <= insideRadius)
				fAttenuation = 1.0f;
			else
				fAttenuation = 1.0f - __min(1.0f, (dist-insideRadius)/fMaxDist);

			// Set to height mode, modify the location towards the specified height
			fCurHeight = m_pHeightmap[iIndex];
			float dh = fHeight;

			float h = fCurHeight + (fAttenuation)*dh*fHardness;


			if (bAddNoise)
			{
				int nx = ftoi(iPosX*noiseFreq) % NOISE_ARRAY_SIZE;
				int ny = ftoi(iPosY*noiseFreq) % NOISE_ARRAY_SIZE;
				float noise = m_pNoise->m_Array[nx][ny];
				h += (float)(noise)*fAttenuation*noiseScale;
			}


			// Clamp
			ClampHeight(h);

			m_pHeightmap[iIndex] = h;
		}
	}

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::SmoothSpot( int iX, int iY, int radius, float fHeight,float fHardness )
{
	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	int i, j;
	int iPosX, iPosY;
	float fMaxDist, fYSquared;

	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->RecordUndo( new CUndoHeightmapModify(iX-radius,iY-radius,radius*2,radius*2,this) );

	// Calculate the maximum distance
	fMaxDist = radius;

	for (j=(long) -radius; j<radius; j++)
	{
		// Precalculate
		iPosY = iY + j;
		fYSquared = (float) (j * j);

		// Skip invalid locations
		if (iPosY < 1 || iPosY > m_iHeight-2)
				continue;
		
		for (i=(long) -radius; i<radius; i++)
		{
			// Calculate the position
			iPosX = iX + i;
			
			// Skip invalid locations
			if (iPosX < 1 || iPosX > m_iWidth-2)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + i*i);
			if (dist > fMaxDist)
				continue;

			int pos = iPosX + iPosY*m_iWidth;
			float h;
			h =	(m_pHeightmap[pos] +
					 m_pHeightmap[pos+1] +
					 m_pHeightmap[pos-1] +
					 m_pHeightmap[pos+m_iWidth] +
					 m_pHeightmap[pos-m_iWidth] + 
					 m_pHeightmap[pos+1+m_iWidth] +
					 m_pHeightmap[pos+1-m_iWidth] +
					 m_pHeightmap[pos-1+m_iWidth] +
					 m_pHeightmap[pos-1-m_iWidth])
					/ 9.0f;

			float currH = m_pHeightmap[pos];
			m_pHeightmap[pos] = currH + (h-currH)*fHardness;
		}
	}

	// We modified the heightmap.
	SetModified();
}

void CHeightmap::Hold()
{
	////////////////////////////////////////////////////////////////////////
	// Save a backup copy of the heightmap
	////////////////////////////////////////////////////////////////////////

	FILE *hFile = NULL;

	CLogFile::WriteLine("Saving temporary copy of the heightmap");

	AfxGetMainWnd()->BeginWaitCursor();

	// Open the hold / fetch file
	hFile = fopen(HEIGHTMAP_HOLD_FETCH_FILE, "wb");
	ASSERT(hFile);
	if (hFile)
	{
		// Write the dimensions
		VERIFY(fwrite(&m_iWidth, sizeof(m_iWidth), 1, hFile));
		VERIFY(fwrite(&m_iHeight, sizeof(m_iHeight), 1, hFile));

		// Write the data
		VERIFY(fwrite(m_pHeightmap, sizeof(t_hmap), m_iWidth * m_iHeight, hFile));

		//! Write the info.
		VERIFY(fwrite( m_LayerIdBitmap.GetData(),sizeof(unsigned char), m_LayerIdBitmap.GetSize(), hFile));

		fclose(hFile);
	}

	AfxGetMainWnd()->EndWaitCursor();
}

void CHeightmap::Fetch()
{
	////////////////////////////////////////////////////////////////////////
	// Read a backup copy of the heightmap
	////////////////////////////////////////////////////////////////////////

	CLogFile::WriteLine("Loading temporary copy of the heightmap");

	AfxGetMainWnd()->BeginWaitCursor();

	if (!Read(HEIGHTMAP_HOLD_FETCH_FILE))
	{
		AfxMessageBox("You need to use 'Hold' before 'Fetch' !");
		return;
	}

	AfxGetMainWnd()->EndWaitCursor();
}

bool CHeightmap::Read(CString strFileName)
{
	////////////////////////////////////////////////////////////////////////
	// Load a heightmap from a file
	////////////////////////////////////////////////////////////////////////

	FILE *hFile = NULL;
	UINT iWidth, iHeight;

	if (strFileName.IsEmpty())
		return false;

	// Open the hold / fetch file
	hFile = fopen(strFileName.GetBuffer(0), "rb");
	
	if (!hFile)
		return false;

	// Read the dimensions
	VERIFY(fread(&iWidth, sizeof(iWidth), 1, hFile));
	VERIFY(fread(&iHeight, sizeof(iHeight), 1, hFile));

	// Resize the heightmap
	Resize(iWidth, iHeight,m_unitSize);

	// Load the data
	VERIFY(fread(m_pHeightmap, sizeof(t_hmap), m_iWidth * m_iHeight, hFile));

	//! Write the info.
	m_LayerIdBitmap.Allocate( m_iWidth,m_iHeight );
	VERIFY(fread( m_LayerIdBitmap.GetData(),sizeof(unsigned char), m_LayerIdBitmap.GetSize(), hFile));

	fclose(hFile);

	return true;
}

void CHeightmap::InitNoise()
{
	////////////////////////////////////////////////////////////////////////
	// Initialize the noise array
	////////////////////////////////////////////////////////////////////////
	if (m_pNoise)
		return;


	CNoise cNoise;
	static bool bFirstQuery = true;
	float fFrequency = 6.0f;
	float fFrequencyStep = 2.0f;
	float fYScale = 1.0f;
	float fFade = 0.46f;
	float fLowestPoint = 256000.0f, fHighestPoint = -256000.0f;
	float fValueRange;
	unsigned int i, j;

	assert(!m_pNoise);

	// Allocate a new array class to 
	m_pNoise = new CDynamicArray2D(NOISE_ARRAY_SIZE, NOISE_ARRAY_SIZE);
	
	// Process layers
	for (i=0; i<8; i++)
	{
		// Apply the fractal noise function to the array
		cNoise.FracSynthPass(m_pNoise, fFrequency, fYScale, NOISE_ARRAY_SIZE, NOISE_ARRAY_SIZE, TRUE);

		// Modify noise generation parameters
		fFrequency *= fFrequencyStep;
		fYScale *= fFade;	
	}

	// Find the value range
	for (i=0; i<NOISE_ARRAY_SIZE; i++)
		for (j=0; j<NOISE_ARRAY_SIZE; j++)
		{
			fLowestPoint = __min(fLowestPoint, m_pNoise->m_Array[i][j]);
			fHighestPoint = __max(fHighestPoint, m_pNoise->m_Array[i][j]);
		}

	// Storing the value range in this way saves us a division and a multiplication
	fValueRange = 1.0f / (fHighestPoint + (float) fabs(fLowestPoint)) * 255.0f;

	// Normalize the heightmap
	for (i=0; i<NOISE_ARRAY_SIZE; i++)
		for (j=0; j<NOISE_ARRAY_SIZE; j++)
		{
			m_pNoise->m_Array[i][j] += (float) (fLowestPoint);
			m_pNoise->m_Array[i][j] *= fValueRange;

			// Keep signed / unsigned balance
			//m_pNoise->m_Array[i][j] += 2500.0f/255.0f;
		}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::CalcSurfaceTypes( const CRect *rect )
{
	int i;
	bool first = true;

	CRect rc;

	CFloatImage hmap;
	hmap.Attach( m_pHeightmap,m_iWidth,m_iHeight );

	if (rect)
		rc = *rect;
	else
		rc.SetRect( 0,0,m_iWidth,m_iHeight );

	// Generate the masks
	CCryEditDoc *doc = GetIEditor()->GetDocument();
	int numLayers = doc->GetLayerCount();
	for (i = 0; i < numLayers; i++)
	{
		CLayer *pLayer = doc->GetLayer(i);
		
		if (!pLayer->IsInUse())
			continue;

		uint32 dwLayerId = pLayer->GetOrRequestLayerId();

		if(first)
		{
			first = false;

			// For every pixel of layer update surface type.
			for (uint y = rc.top; y < rc.bottom; y++)
			{
				int yp = y*m_iWidth;
				for (uint x = rc.left; x < rc.right; x++)
					SetLayerIdAt(x,y,dwLayerId);
			}
		}
		else
		{
			// Assume that layer mask is at size of full resolution texture.
			CByteImage &layerMask = pLayer->GetMask();
			if (!layerMask.IsValid())
				continue;
			
			int layerWidth = layerMask.GetWidth();
			int layerHeight = layerMask.GetHeight();
			float xScale = (float)layerWidth / m_iWidth;
			float yScale = (float)layerHeight / m_iHeight;

			uchar *pLayerMask = layerMask.GetData();
			
			// For every pixel of layer update surface type.
			for (uint y = rc.top; y < rc.bottom; y++)
			{
				for (uint x = rc.left; x < rc.right; x++)
				{
					uchar a = pLayerMask[ftoi(x*xScale + layerWidth*y*yScale)];
					if (a > OVVERIDE_LAYER_SURFACETYPE_FROM)
					{					
						SetLayerIdAt(x,y,dwLayerId);
					}
				}
			}
		}
	}
}


void CHeightmap::UpdateEngineTerrain( bool bOnlyElevation )
{
	UpdateEngineTerrain( 0,0,m_iWidth,m_iHeight,true,!bOnlyElevation );
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::UpdateEngineTerrain( int x1, int y1, int areaSize, int _height, bool bElevation, bool bInfoBits )
{
	// update terrain by square blocks aligned to terrain sector size

	int nSecSize = GetIEditor()->Get3DEngine()->GetTerrainSectorSize()/GetIEditor()->Get3DEngine()->GetHeightMapUnitSize();
	int nTerrSize = m_iWidth;

	areaSize = ceil(areaSize/(float)nSecSize+1)*nSecSize;
	areaSize = CLAMP(areaSize, 0, nTerrSize);

	x1 = floor(x1/(float)nSecSize)*nSecSize;
	y1 = floor(y1/(float)nSecSize)*nSecSize;
	x1 = CLAMP(x1, 0, (nTerrSize-areaSize));
	y1 = CLAMP(y1, 0, (nTerrSize-areaSize));

	int x2 = x1+areaSize;
	int y2 = y1+areaSize;
	x2 = CLAMP(x2, areaSize, nTerrSize);
	y2 = CLAMP(y2, areaSize, nTerrSize);

	int w, h;
	w = x2-x1;
	h = y2-y1;

	if(w<=0 || h<=0)
		return;

	TImage<unsigned char> image;
	image.Allocate( w, h );
	unsigned char *terrainBlock = image.GetData();

	// fill image with surface type info
	for (int y = y1; y < y2; y++)
	{
		for (int x = x1; x < x2; x++)
		{
			int ncell = (y-y1)*w + (x-x1);
			assert(ncell>=0 && ncell<w*h);
			unsigned char *p = &terrainBlock[ncell];

			if(IsHoleAt(x,y))
				*p |= SURFACE_TYPE_MASK;
			 else
				*p = GetDetailLayerIdAt(x,y) % SURFACE_TYPE_MASK;
		}
	}
	
	if (bElevation || bInfoBits)
	{
    CHeightmap * pHM = GetIEditor()->GetHeightmap();
    uint32 nSizeX = pHM->m_TerrainRGBTexture.GetTileCountX();
    uint32 nSizeY = pHM->m_TerrainRGBTexture.GetTileCountY();
    uint32 * pResolutions = new uint32[nSizeX*nSizeY];
    for(int x=0; x<nSizeX; x++) for(int y=0; y<nSizeY; y++)
      pResolutions[x+y*nSizeX] = pHM->m_TerrainRGBTexture.GetTileResolution(x, y);

		GetIEditor()->Get3DEngine()->SetHeightMapMaxHeight(m_fMaxHeight);
    GetIEditor()->Get3DEngine()->GetITerrain()->SetTerrainElevation( y1, x1, w, h, m_pHeightmap, terrainBlock, 
      nSizeX ? pResolutions : NULL, nSizeX, nSizeY );

    delete [] pResolutions;
	}
}

void CHeightmap::Serialize( CXmlArchive &xmlAr,bool bSerializeVegetation,bool bUpdateTerrain )
{
	if (xmlAr.bLoading)
	{
		// Loading
		XmlNodeRef heightmap = xmlAr.root->findChild( "Heightmap" );
		if (!heightmap)
			return;

		heightmap->getAttr( "Width",m_iWidth );
		heightmap->getAttr( "Height",m_iHeight );
		heightmap->getAttr( "WaterLevel",m_fWaterLevel );
		heightmap->getAttr( "UnitSize",m_unitSize );
		heightmap->getAttr( "MaxHeight",m_fMaxHeight );

		int textureSize;
		if (heightmap->getAttr( "TextureSize",textureSize ))
		{
			SetSurfaceTextureSize(textureSize,textureSize);
		}

		void *pData;
		int size1,size2;
		
		// Allocate new memory
		Resize(m_iWidth, m_iHeight,m_unitSize);

		// Load heightmap data.
		if (xmlAr.pNamedData->GetDataBlock( "HeightmapDataW",pData,size1 ))
		{
			float fInvPrecision = 1.0f / GetShortPrecisionScale();
			ushort *pSrc = (ushort*)pData;
			for (int i = 0; i < m_iWidth*m_iHeight; i++)
			{
				m_pHeightmap[i] = (float)pSrc[i] * fInvPrecision;
			}
		}
		else if (xmlAr.pNamedData->GetDataBlock( "HeightmapData",pData,size1 ))
		{
			// Backward compatability for float heigthmap data.
			memcpy( m_pHeightmap,pData,size1 );
		}

		if(xmlAr.pNamedData->GetDataBlock("HeightmapLayerIdBitmap",pData,size2 ))
		{
			assert(!m_bUpdateLayerIdBitmap);

			// new version
			memcpy( m_LayerIdBitmap.GetData(),pData,MAX(size2,m_LayerIdBitmap.GetSize()) );
		}
		else
		{
			m_bUpdateLayerIdBitmap=true;

			// old version - to be backward compatible
			if(xmlAr.pNamedData->GetDataBlock( "HeightmapInfo",pData,size2 ))
				memcpy( m_LayerIdBitmap.GetData(),pData,MAX(size2,m_LayerIdBitmap.GetSize()) );
		}

		// After heightmap serialization, update terrain in 3D Engine.
		if (bUpdateTerrain)
			UpdateEngineTerrain(false);

		if (m_vegetationMap && bSerializeVegetation)
		{
			m_vegetationMap->Serialize( xmlAr );
		}
	}
	else
	{
		// Storing
		XmlNodeRef heightmap = xmlAr.root->newChild( "Heightmap" );

		heightmap->setAttr( "Width",m_iWidth );
		heightmap->setAttr( "Height",m_iHeight );
		heightmap->setAttr( "WaterLevel",m_fWaterLevel );
		heightmap->setAttr( "UnitSize",m_unitSize );
		heightmap->setAttr( "TextureSize",m_textureSize );
		heightmap->setAttr( "MaxHeight",m_fMaxHeight );

		// Save heightmap data as words.
		//xmlAr.pNamedData->AddDataBlock( "HeightmapData",m_pHeightmap,m_iWidth * m_iHeight * sizeof(t_hmap) );
		{
			CWordImage hdata;
			hdata.Allocate( m_iWidth,m_iHeight );
			ushort *pTrg = hdata.GetData();
			float fPrecisionScale = GetShortPrecisionScale();
			for (int i = 0; i < m_iWidth*m_iHeight; i++)
			{
				float val = m_pHeightmap[i];
				unsigned int h = ftoi(val*fPrecisionScale+0.5f);
				if (h > 0xFFFF) h = 0xFFFF;
				pTrg[i] = h;
			}
			xmlAr.pNamedData->AddDataBlock( "HeightmapDataW",hdata.GetData(),hdata.GetSize() );
		}

		// old version
		// xmlAr.pNamedData->AddDataBlock( "HeightmapInfo",m_info.GetData(),m_info.GetSize() );
	
		// new version
		xmlAr.pNamedData->AddDataBlock( "HeightmapLayerIdBitmap",m_LayerIdBitmap.GetData(),m_LayerIdBitmap.GetSize() );

		if (m_vegetationMap && bSerializeVegetation)
		{
			m_vegetationMap->Serialize( xmlAr );
		}
	}

	m_TerrainRGBTexture.Serialize(xmlAr);
}


void CHeightmap::ProcessAfterLoading()
{
	if(m_bUpdateLayerIdBitmap)
	{
		CLogFile::WriteLine("Converting heightmap DetailID to LayerID (new format) ...");
		CCryEditDoc *pDoc = GetIEditor()->GetDocument();

		for(uint32 dwY=0;dwY<m_iHeight;++dwY)
		for(uint32 dwX=0;dwX<m_iWidth;++dwX)
		{
			unsigned char ucVal = m_LayerIdBitmap.ValueAt(dwX,dwY);

			bool bHole = (ucVal & 0x1)!=0;
			uint32 dwDetailLayerId = (ucVal>>2) & 0xf; 

			SetHoleAt(dwX,dwY,bHole);

			uint32 dwLayerId = pDoc->GetLayerIdFromDetailLayerId(dwDetailLayerId);		assert(dwLayerId!=0xffffffff);

			if(dwLayerId!=0xffffffff)
				SetLayerIdAt(dwX,dwY,dwLayerId);
		}

		m_bUpdateLayerIdBitmap=false;
	}
}

void CHeightmap::SerializeTerrain( CXmlArchive &xmlAr, bool & )
{
	if (xmlAr.bLoading)
	{ // Loading
		void * pData = NULL;
		int nSize = 0;
		if(xmlAr.pNamedData->GetDataBlock( "TerrainCompiledData", pData, nSize ))
		{
			STerrainChunkHeader * pHeader = (STerrainChunkHeader *)pData;
			if(pHeader->nChunkVersion == TERRAIN_CHUNK_VERSION)
			{
				ITerrain * pTerrain = GetIEditor()->Get3DEngine()->CreateTerrain(pHeader->TerrainInfo);
				if(!pTerrain->SetCompiledData((uchar*)pData,nSize,NULL,NULL))
					GetIEditor()->Get3DEngine()->DeleteTerrain();
			}
		}
		GetIEditor()->GetGameEngine()->InitTerrain();
	}
	else 
	{
		ITerrain *pTerrain = GetIEditor()->Get3DEngine()->GetITerrain();
		if (pTerrain)
		{
			int nSize = pTerrain->GetCompiledDataSize();
			if (nSize > 0)
			{ // Storing
				uchar * pData = new uchar[nSize];
				GetIEditor()->Get3DEngine()->GetITerrain()->GetCompiledData(pData,nSize,NULL,NULL);
				xmlAr.pNamedData->AddDataBlock( "TerrainCompiledData", pData, nSize, true );
				delete [] pData;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::SerializeVegetation( CXmlArchive &xmlAr )
{
	if (m_vegetationMap)
	{
		m_vegetationMap->Serialize( xmlAr );
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::SetWaterLevel( float waterLevel )
{
	// We modified the heightmap.
	SetModified();

	m_fWaterLevel = waterLevel;

	if(GetIEditor()->GetSystem()->GetI3DEngine())
	if(GetIEditor()->GetSystem()->GetI3DEngine()->GetITerrain())
		GetIEditor()->GetSystem()->GetI3DEngine()->GetITerrain()->SetOceanWaterLevel(waterLevel);
};

//////////////////////////////////////////////////////////////////////////
void CHeightmap::SetModified()
{
	GetIEditor()->SetModifiedFlag();

//	m_cachedResolution = 0;
//	m_cachedHeightmap.Free();
}


void CHeightmap::SetHoleAt( const int x, const int y, const bool bHole )
{
	if(bHole)
		m_LayerIdBitmap.ValueAt(x,y) |= HEIGHTMAP_INFO_HOLE;
	 else
		m_LayerIdBitmap.ValueAt(x,y) &= ~HEIGHTMAP_INFO_HOLE;
}

//////////////////////////////////////////////////////////////////////////
// Make hole.
void CHeightmap::MakeHole( int x1,int y1,int width,int height,bool bMake )
{
	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->RecordUndo( new CUndoHeightmapInfo(x1,y1,width+1,height+1,this) );

	I3DEngine *engine = GetIEditor()->Get3DEngine();
	int x2 = x1 + width;
	int y2 = y1 + height;
	for (int x = x1; x <= x2; x++)
	{
		for (int y = y1; y <= y2; y++)
		{
			SetHoleAt(x,y,bMake);
		}
	}

	UpdateEngineTerrain( x1, y1, x2-x1, y2-y1, false, true );
}

//////////////////////////////////////////////////////////////////////////
bool CHeightmap::IsHoleAt( const int x, const int y ) const
{ 
	return (m_LayerIdBitmap.ValueAt(x,y)&HEIGHTMAP_INFO_HOLE)!=0; 
}


//////////////////////////////////////////////////////////////////////////
void CHeightmap::SetLayerIdAt( const int x, const int y, const uint32 dwLayerId )
{
	uint8 &ucRef = m_LayerIdBitmap.ValueAt(x,y);

	assert(dwLayerId<(HEIGHTMAP_INFO_SFTYPE_MASK>>HEIGHTMAP_INFO_SFTYPE_SHIFT));

	ucRef = (ucRef & (~HEIGHTMAP_INFO_SFTYPE_MASK)) | (dwLayerId<<HEIGHTMAP_INFO_SFTYPE_SHIFT);
}

//////////////////////////////////////////////////////////////////////////
uint32 CHeightmap::GetDetailLayerIdAt( const int x, const int y ) const
{ 
	uint32 dwLayerId = (m_LayerIdBitmap.ValueAt(x,y)&HEIGHTMAP_INFO_SFTYPE_MASK) >> HEIGHTMAP_INFO_SFTYPE_SHIFT;

	uint32 dwDetailLayerId = GetIEditor()->GetDocument()->GetDetailIdLayerFromLayerId(dwLayerId);

	return dwDetailLayerId;
}


//////////////////////////////////////////////////////////////////////////
uint32 CHeightmap::GetLayerIdAt( const int x, const int y) const
{ 
	uint8 ucValue=0;

	if(x>=0 && y>=0 && x<m_LayerIdBitmap.GetWidth() && y<m_LayerIdBitmap.GetHeight())
		ucValue=m_LayerIdBitmap.ValueAt(x,y);

	uint32 dwLayerId = (ucValue&HEIGHTMAP_INFO_SFTYPE_MASK) >> HEIGHTMAP_INFO_SFTYPE_SHIFT;

	return dwLayerId;
}

//////////////////////////////////////////////////////////////////////////
float CHeightmap::GetZInterpolated( const float x, const float y )
{
	if (x <= 0 || y <= 0 || x >= m_iWidth-1 || y >= m_iHeight-1)
		return 0;

	int nX = fastftol_positive(x);
	int nY = fastftol_positive(y);

	float dx1 = x - nX;
	float dy1 = y - nY;

	float dDownLandZ0 = 
		(1.f-dx1) * (m_pHeightmap[ nX		+	nY*m_iWidth  ] ) + 
		(    dx1) * (m_pHeightmap[(nX+1)+	nY*m_iWidth  ] );

	float dDownLandZ1 = 
		(1.f-dx1) * (m_pHeightmap[ nX		+	(nY+1)*m_iWidth] ) + 
		(    dx1) * (m_pHeightmap[(nX+1)+	(nY+1)*m_iWidth] );

	float dDownLandZ = (1-dy1) * dDownLandZ0 + (  dy1) * dDownLandZ1;

	if(dDownLandZ < 0)
		dDownLandZ = 0;

	return dDownLandZ;
}

//////////////////////////////////////////////////////////////////////////
float CHeightmap::GetAccurateSlope( const float x, const float y )
{
	uint32 iHeightmapWidth = GetWidth();

	if (x <= 0 || y <= 0 || x >= m_iWidth-1 || y >= m_iHeight-1)
		return 0;

	// Calculate the slope for this point

	float arrEvelvations[8];
	int nId = 0;

	float d = 0.7f;
	arrEvelvations[nId++] = GetZInterpolated( x+1, y   );
	arrEvelvations[nId++] = GetZInterpolated( x-1, y   );
	arrEvelvations[nId++] = GetZInterpolated( x  , y+1 );
	arrEvelvations[nId++] = GetZInterpolated( x  , y-1 );
	arrEvelvations[nId++] = GetZInterpolated( x+d, y+d );
	arrEvelvations[nId++] = GetZInterpolated( x-d, y-d );
	arrEvelvations[nId++] = GetZInterpolated( x+d, y-d );
	arrEvelvations[nId++] = GetZInterpolated( x-d, y+d );

	float fMin = arrEvelvations[0];
	float fMax = arrEvelvations[0];

	for(int i=0; i<8; i++)
	{
		if(arrEvelvations[i]>fMax)
			fMax = arrEvelvations[i];
		if(arrEvelvations[i]<fMin)
			fMin = arrEvelvations[i];
	}

	// Compensate the smaller slope for bigger height fields
	return (fMax-fMin)*0.5f/GetUnitSize();
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::UpdateEngineHole( int x1,int y1,int width,int height )
{
	UpdateEngineTerrain( x1, y1, width, height, false, true );
}

//////////////////////////////////////////////////////////////////////////
CVegetationMap* CHeightmap::GetVegetationMap()
{
	return m_vegetationMap;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::GetSectorsInfo( SSectorInfo &si )
{
	ZeroStruct(si);
	si.unitSize = m_unitSize;
	si.sectorSize = SECTOR_SIZE_IN_METERS;
	si.numSectors = m_numSectors;
	si.sectorTexSize = m_textureSize / si.numSectors;
	si.surfaceTextureSize = m_textureSize;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::SetSurfaceTextureSize( int width,int height )
{
	assert( width == height );

	if (width != 0)
	{
    m_textureSize = width;
	}
	m_terrainGrid->SetResolution( m_textureSize );
}

/*
//////////////////////////////////////////////////////////////////////////
void CHeightmap::MoveBlock( const CRect &rc,CPoint offset )
{
	CRect hrc(0,0,m_iWidth,m_iHeight );
	CRect subRc = rc & hrc;
	CRect trgRc = rc;
	trgRc.OffsetRect(offset);
	trgRc &= hrc;

	if (subRc.IsRectEmpty() || trgRc.IsRectEmpty())
		return;

	if (CUndo::IsRecording())
	{
		// Must be square.
		int size = (trgRc.Width() > trgRc.Height()) ? trgRc.Width() : trgRc.Height();
		CUndo::Record( new CUndoHeightmapModify(trgRc.left,trgRc.top,size,size,this) );
	}

	CFloatImage hmap;
	CFloatImage hmapSubImage;
	hmap.Attach( m_pHeightmap,m_iWidth,m_iHeight );
	hmapSubImage.Allocate( subRc.Width(),subRc.Height() );

	hmap.GetSubImage( subRc.left,subRc.top,subRc.Width(),subRc.Height(),hmapSubImage );
	hmap.SetSubImage( trgRc.left,trgRc.top,hmapSubImage );

	CByteImage infoMap;
	CByteImage infoSubImage;
	infoMap.Attach( &m_info[0],m_iWidth,m_iHeight );
	infoSubImage.Allocate( subRc.Width(),subRc.Height() );

	infoMap.GetSubImage( subRc.left,subRc.top,subRc.Width(),subRc.Height(),infoSubImage );
	infoMap.SetSubImage( trgRc.left,trgRc.top,infoSubImage );

	// Move Vegetation.
	if (m_vegetationMap)
	{
		Vec3 p1 = HmapToWorld(CPoint(subRc.left,subRc.top));
		Vec3 p2 = HmapToWorld(CPoint(subRc.right,subRc.bottom));
		Vec3 ofs = HmapToWorld(offset);
		CRect worldRC( p1.x,p1.y,p2.x,p2.y );
		// Export and import to block.
		CXmlArchive ar("Root");
		ar.bLoading = false;
		m_vegetationMap->ExportBlock( worldRC,ar );
		ar.bLoading = true;
		m_vegetationMap->ImportBlock( ar,CPoint(ofs.x,ofs.y) );
	}
}
*/

//////////////////////////////////////////////////////////////////////////
int CHeightmap::LogLayerSizes()
{
	int totalSize = 0;
	CCryEditDoc *doc = GetIEditor()->GetDocument();
	int numLayers = doc->GetLayerCount();
	for (int i = 0; i < numLayers; i++)
	{
		CLayer *pLayer = doc->GetLayer(i);
		int layerSize = pLayer->GetSize();
		totalSize += layerSize;
		CLogFile::FormatLine( "Layer %s: %dM",(const char*)pLayer->GetLayerName(),layerSize/(1024*1024) );
	}
	CLogFile::FormatLine( "Total Layers Size: %dM",totalSize/(1024*1024) );
	return totalSize;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::ExportBlock( const CRect &inrect,CXmlArchive &xmlAr, bool bIsExportVegetation )
{
	// Storing
	CLogFile::WriteLine("Exporting Heightmap settings...");

	XmlNodeRef heightmap = xmlAr.root->newChild( "Heightmap" );

	CRect subRc( 0,0,m_iWidth,m_iHeight );
	subRc &= inrect;

	heightmap->setAttr( "Width",m_iWidth );
	heightmap->setAttr( "Height",m_iHeight );

	// Save rectangle dimensions to root of terrain block.
	xmlAr.root->setAttr( "X1",subRc.left );
	xmlAr.root->setAttr( "Y1",subRc.top );
	xmlAr.root->setAttr( "X2",subRc.right );
	xmlAr.root->setAttr( "Y2",subRc.bottom );

	// Rectangle.
	heightmap->setAttr( "X1",subRc.left );
	heightmap->setAttr( "Y1",subRc.top );
	heightmap->setAttr( "X2",subRc.right );
	heightmap->setAttr( "Y2",subRc.bottom );

	heightmap->setAttr( "UnitSize",m_unitSize );

	CFloatImage hmap;
	CFloatImage hmapSubImage;
	hmap.Attach( m_pHeightmap,m_iWidth,m_iHeight );
	hmapSubImage.Allocate( subRc.Width(),subRc.Height() );

	hmap.GetSubImage( subRc.left,subRc.top,subRc.Width(),subRc.Height(),hmapSubImage );

	CByteImage LayerIdBitmapImage;
	LayerIdBitmapImage.Allocate( subRc.Width(),subRc.Height() );

	m_LayerIdBitmap.GetSubImage( subRc.left,subRc.top,subRc.Width(),subRc.Height(),LayerIdBitmapImage );

	// Save heightmap.
	xmlAr.pNamedData->AddDataBlock( "HeightmapData",hmapSubImage.GetData(),hmapSubImage.GetSize() );
	xmlAr.pNamedData->AddDataBlock( "HeightmapLayerIdBitmap",LayerIdBitmapImage.GetData(),LayerIdBitmapImage.GetSize() );


	//return;
	if(bIsExportVegetation)
	{
		Vec3 p1 = HmapToWorld(CPoint(subRc.left,subRc.top));
		Vec3 p2 = HmapToWorld(CPoint(subRc.right,subRc.bottom));
		if (m_vegetationMap)
		{
			CRect worldRC( p1.x,p1.y,p2.x,p2.y );
			m_vegetationMap->ExportBlock( worldRC,xmlAr );
		}
	}

	//@TODO.
	//Timur, TODO remove it
	return;

	SSectorInfo si;
	GetSectorsInfo(si);
	float pixelsPerMeter = ((float)si.sectorTexSize) / si.sectorSize;

	CRect layerRc;
	layerRc.left = subRc.left*si.unitSize * pixelsPerMeter;
	layerRc.top = subRc.top*si.unitSize * pixelsPerMeter;
	layerRc.right = subRc.right*si.unitSize * pixelsPerMeter;
	layerRc.bottom = subRc.bottom*si.unitSize * pixelsPerMeter;

	XmlNodeRef layers = xmlAr.root->newChild( "Layers" );
	// Export layers settings.
	CCryEditDoc *doc = GetIEditor()->GetDocument();
	for (int i = 0; i < doc->GetLayerCount(); i++)
	{
		CLayer *layer = doc->GetLayer(i);
		//if (layer->IsAutoGen())
			//continue;
		CXmlArchive ar( xmlAr );
		ar.root = layers->newChild( "Layer" );
		layer->ExportBlock( layerRc,ar );
	}
}
	
//////////////////////////////////////////////////////////////////////////
CPoint CHeightmap::ImportBlock( CXmlArchive &xmlAr,CPoint newPos,bool bUseNewPos, float heightOffset, bool bOnlyVegetation)
{
	CLogFile::WriteLine("Importing Heightmap settings...");

	XmlNodeRef heightmap = xmlAr.root->findChild( "Heightmap" );
	if (!heightmap)
		return CPoint(0,0);

	UINT width,height;

	heightmap->getAttr( "Width",width );
	heightmap->getAttr( "Height",height );

	CPoint offset(0,0);

	if (width != m_iWidth || height != m_iHeight)
	{
		MessageBox( AfxGetMainWnd()->GetSafeHwnd(),_T("Terrain Block dimensions differ from current terrain."),_T("Warning"),MB_OK|MB_ICONEXCLAMATION );
		return CPoint(0,0);
	}

	CRect subRc;

	// Rectangle.
	heightmap->getAttr( "X1",subRc.left );
	heightmap->getAttr( "Y1",subRc.top );
	heightmap->getAttr( "X2",subRc.right );
	heightmap->getAttr( "Y2",subRc.bottom );

	if (bUseNewPos)
	{
		offset = CPoint( newPos.x-subRc.left,newPos.y-subRc.top );
		subRc.OffsetRect( offset );
	}

	if(!bOnlyVegetation)
	{
		void *pData;
		int size;

		// Load heightmap data.
		if (xmlAr.pNamedData->GetDataBlock( "HeightmapData",pData,size ))
		{
			// Backward compatability for float heigthmap data.
			CFloatImage hmap;
			CFloatImage hmapSubImage;
			hmap.Attach( m_pHeightmap,m_iWidth,m_iHeight );
			hmapSubImage.Attach( (float*)pData,subRc.Width(),subRc.Height() );

			hmap.SetSubImage( subRc.left,subRc.top,hmapSubImage, heightOffset );
		}

		if (xmlAr.pNamedData->GetDataBlock( "HeightmapLayerIdBitmap",pData,size ))
		{
			CByteImage LayerIdBitmapImage;
			LayerIdBitmapImage.Attach( (unsigned char*)pData,subRc.Width(),subRc.Height() );

			m_LayerIdBitmap.SetSubImage( subRc.left,subRc.top,LayerIdBitmapImage );
		}

		// After heightmap serialization, update terrain in 3D Engine.
		int wid = subRc.Width()+2;
		if(wid<subRc.Height()+2)
			wid=subRc.Height()+2;
		UpdateEngineTerrain( subRc.left-1,subRc.top-1,wid,wid,true,true );
	}

	if (m_vegetationMap)
	{
		Vec3 ofs = HmapToWorld(offset);
		m_vegetationMap->ImportBlock( xmlAr,CPoint(ofs.x,ofs.y) );
	}

	if(!bOnlyVegetation)
	{
		// Import layers.
		XmlNodeRef layers = xmlAr.root->findChild( "Layers" );
		if (layers)
		{
			SSectorInfo si;
			GetSectorsInfo(si);
			float pixelsPerMeter = ((float)si.sectorTexSize) / si.sectorSize;

			CPoint layerOffset;
			layerOffset.x = offset.x*si.unitSize * pixelsPerMeter;
			layerOffset.y = offset.y*si.unitSize * pixelsPerMeter;

			// Export layers settings.
			CCryEditDoc *doc = GetIEditor()->GetDocument();
			for (int i = 0; i < layers->getChildCount(); i++)
			{
				CXmlArchive ar( xmlAr );
				ar.root = layers->getChild(i);
				CString layerName;
				if (!ar.root->getAttr( "Name",layerName ))
					continue;
				CLayer *layer = doc->FindLayer( layerName );
				if (!layer)
					continue;
			
				layer->ImportBlock( ar,layerOffset );
			}
		}
	}

	return offset;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::CopyFrom( t_hmap *pHmap,unsigned char *pLayerIdBitmap,int resolution )
{
	int x,y;
	int res = min(resolution,(int)m_iWidth);
	for (y = 0; y < res; y++)
	{
		for (x = 0; x < res; x++)
		{
			SetXY( x,y, pHmap[x + y*resolution] );
			m_LayerIdBitmap.ValueAt( x,y ) = pLayerIdBitmap[x + y*resolution];
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::CopyFromInterpolate( t_hmap *pHmap,unsigned char *pLayerIdBitmap,int resolution, float fKof )
{
	if(fKof>1)
	{
		int kof = (int) fKof;
		for (int y=0, y2=0; y < m_iHeight; y+=kof, y2++)
			for (int x=0, x2=0; x < m_iWidth; x+=kof, x2++)
			{
				for(int y1=0; y1<kof; y1++)
					for(int x1=0; x1<kof; x1++)
					{
						if(x + x1<m_iWidth && y + y1<m_iHeight && x2<resolution && y2<resolution)
						{
							float kofx = (float) x1/kof;
							float kofy = (float) y1/kof;

							int x3 = x2+1;
							int y3 = y2+1;
	/*
							if(x2>0 && y2>0 && x3+1<resolution && y3+1<resolution)
							{
								t_hmap p1xy2 = pHmap[x2 + y2*resolution] + (pHmap[x2 + y2*resolution] - pHmap[x2-1 + y2*resolution])/3;
								t_hmap p2xy2 = pHmap[x3 + y2*resolution] + (pHmap[x3 + y2*resolution] - pHmap[x3+1 + y2*resolution])/3;

								t_hmap p1xy3 = pHmap[x2 + y3*resolution] + (pHmap[x2 + y3*resolution] - pHmap[x2-1 + y3*resolution])/3;
								t_hmap p2xy3 = pHmap[x3 + y3*resolution] + (pHmap[x3 + y3*resolution] - pHmap[x3+1 + y3*resolution])/3;


								t_hmap pxy2 = p1xy2 
							}
							else
							{
	*/
								if(x3>=resolution)
									x3 = x2;
								if(y3>=resolution)
									y3 = y2;

								SetXY( x + x1,y + y1, 
								(1.0f-kofy) * ((1.0f-kofx)*pHmap[x2 + y2*resolution] + kofx*pHmap[x3 + y2*resolution])+
								kofy *((1.0f-kofx)*pHmap[x2 + y3*resolution] + kofx*pHmap[x3 + y3*resolution])
								);
	//						}

							m_LayerIdBitmap.ValueAt( x + x1,y + y1 ) = pLayerIdBitmap[x2 + y2*resolution];
						}
					}
				unsigned char val = pLayerIdBitmap[x2 + y2*resolution];

				if(y2<resolution-1)
				{
					unsigned char val1 = pLayerIdBitmap[x2 + (y2+1)*resolution];
					if(val1>val)
						m_LayerIdBitmap.ValueAt( x,y+kof-1 ) = val1;
				}

				if(x2<resolution-1)
				{
					unsigned char val1 = pLayerIdBitmap[x2+1 + y2*resolution];
					if(val1>val)
						m_LayerIdBitmap.ValueAt( x+kof-1, y ) = val1;
				}

				if(x2<resolution-1 && y2<resolution-1)
				{
					// choose max occured value or max value between several max occured.
					unsigned char valu[4];
					int bal[4];
					valu[0] = val;
					valu[1] = pLayerIdBitmap[x2+1 + y2*resolution];
					valu[2] = pLayerIdBitmap[x2 + (y2+1)*resolution];
					valu[3] = pLayerIdBitmap[x2+1 + (y2+1)*resolution];

					int max=0;

					int k=0;
					for(k=0; k<4; k++)
					{
						bal[k]=1000+valu[k];
						if(bal[k]>max)
						{
							val = valu[k];
							max=bal[k];
						}
						if(k>0)
						{
							for(int kj=0; kj<k; kj++)
							{
								if(valu[kj]==valu[k])
								{
									valu[k] = 0;
									bal[kj]+=bal[k];
									bal[k] = 0;
									if(bal[kj]>max)
									{
										val = valu[kj];
										max=bal[kj];
									}
									break;
								}
							}
						}
					}
					
					m_LayerIdBitmap.ValueAt( x+kof-1, y+kof-1 ) = val;
				}
			}
	}
	else if(0.1f<fKof && fKof<1.0f)
	{
		int kof = int(1.0f / fKof + 0.5f);
		for (int y=0; y < m_iHeight; y++)
			for (int x=0; x < m_iWidth; x++)
			{
				int x2 = x*kof;
				int y2 = y*kof;
				if(x2<resolution && y2<resolution)
				{
					SetXY( x,y, pHmap[x2 + y2*resolution]);
					m_LayerIdBitmap.ValueAt( x, y ) = pLayerIdBitmap[x2 + y2*resolution];
				}
			}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::InitSectorGrid()
{
	SSectorInfo si;
	GetSectorsInfo(si);
	GetTerrainGrid()->InitSectorGrid( si.numSectors );
}



void CHeightmap::GetMemoryUsage( ICrySizer *pSizer )
{
	pSizer->Add(*this);

	if(m_pHeightmap)
	{
		SIZER_COMPONENT_NAME(pSizer,"Heightmap 2D Array");
		pSizer->Add(m_pHeightmap,m_iWidth*m_iHeight);
	}

	if(m_pNoise)
		m_pNoise->GetMemoryUsage(pSizer);

	{
		SIZER_COMPONENT_NAME(pSizer,"LayerId 2D Array");
		pSizer->Add((char *)m_LayerIdBitmap.GetData(),m_LayerIdBitmap.GetSize());
	}

	if(m_vegetationMap)
		m_vegetationMap->GetMemoryUsage(pSizer);

	if(m_terrainGrid)
		m_terrainGrid->GetMemoryUsage(pSizer);

	{
		SIZER_COMPONENT_NAME(pSizer,"RGBLayer");

		m_TerrainRGBTexture.GetMemoryUsage(pSizer);
	}
}


t_hmap CHeightmap::GetSafeXY( const uint32 dwX, const uint32 dwY ) const
{
	if(dwX<=0 || dwY<=0 || dwX>=(uint32)m_iWidth || dwY>=(uint32)m_iHeight)
		return 0;

	return m_pHeightmap[dwX + dwY*m_iWidth]; 
}

void CHeightmap::RecordUndo( int x1,int y1,int width,int height)
{
	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->RecordUndo( new CUndoHeightmapModify( x1, y1, width, height,this) );
}

void CHeightmap::UpdateLayerTexture( const CRect &rect )
{
	float x1 = HmapToWorld(CPoint(rect.left, rect.top)).x;
	float y1 = HmapToWorld(CPoint(rect.left, rect.top)).y;
	float x2 = HmapToWorld(CPoint(rect.right, rect.bottom)).x;
	float y2 = HmapToWorld(CPoint(rect.right, rect.bottom)).y;

	int iTexSectorSize = GetIEditor()->Get3DEngine()->GetTerrainTextureNodeSizeMeters();
	int iTerrainSize = GetIEditor()->Get3DEngine()->GetTerrainSize();
	int iTexSectorsNum = iTerrainSize /iTexSectorSize;

	uint32 dwFullResolution = m_TerrainRGBTexture.CalcMaxLocalResolution((float)rect.left/GetWidth(), (float)rect.top/GetHeight(), (float)rect.right/GetWidth(), (float)rect.bottom/GetHeight());
	uint32 dwNeededResolution = dwFullResolution/iTexSectorsNum;

	int iSectMinX = (int)floor(y1/iTexSectorSize);
	int iSectMinY = (int)floor(x1/iTexSectorSize);
	int iSectMaxX = (int)floor(y2/iTexSectorSize);
	int iSectMaxY = (int)floor(x2/iTexSectorSize);

	CWaitProgress progress( "Updating Terrain Layers" );

	int nTotalSectors = (iSectMaxX-iSectMinX+1) * (iSectMaxY-iSectMinY+1);
	int nCurrSectorNum = 0;
	for(int iSectX=iSectMinX; iSectX<=iSectMaxX; iSectX++)
		for(int iSectY=iSectMinY; iSectY<=iSectMaxY; iSectY++)
		{
			progress.Step(  (100*nCurrSectorNum)/nTotalSectors );
			nCurrSectorNum++;
			int iLocalOutMinX = y1*dwFullResolution/iTerrainSize - iSectX*dwNeededResolution;
			int iLocalOutMinY = x1*dwFullResolution/iTerrainSize - iSectY*dwNeededResolution;
			int iLocalOutMaxX = y2*dwFullResolution/iTerrainSize - iSectX*dwNeededResolution;
			int iLocalOutMaxY = x2*dwFullResolution/iTerrainSize - iSectY*dwNeededResolution;

			if(iLocalOutMinX < 0) iLocalOutMinX = 0;
			if(iLocalOutMinY < 0) iLocalOutMinY = 0;
			if(iLocalOutMaxX > dwNeededResolution) iLocalOutMaxX = dwNeededResolution;
			if(iLocalOutMaxY > dwNeededResolution) iLocalOutMaxY = dwNeededResolution;

			if(iLocalOutMinX!=iLocalOutMaxX && iLocalOutMinY!=iLocalOutMaxY)
			{
				bool bFullRefreshRequired = GetTerrainGrid()->GetSector(CPoint(iSectX, iSectY))->textureId==0;
				bool bRecreated;
				int texId = GetTerrainGrid()->LockSectorTexture( CPoint(iSectX, iSectY),dwFullResolution/iTexSectorsNum,bRecreated);
				if(bFullRefreshRequired || bRecreated)
				{
					iLocalOutMinX = 0;
					iLocalOutMinY = 0;
					iLocalOutMaxX = dwNeededResolution;
					iLocalOutMaxY = dwNeededResolution;
				}

				CImage image;
				image.Allocate(iLocalOutMaxX-iLocalOutMinX,iLocalOutMaxY-iLocalOutMinY);

				m_TerrainRGBTexture.GetSubImageStretched(
					(iSectX+(float)iLocalOutMinX/dwNeededResolution)/iTexSectorsNum,
					(iSectY+(float)iLocalOutMinY/dwNeededResolution)/iTexSectorsNum,
					(iSectX+(float)iLocalOutMaxX/dwNeededResolution)/iTexSectorsNum,
					(iSectY+(float)iLocalOutMaxY/dwNeededResolution)/iTexSectorsNum,
					image);

				// convert RGB colour into format that has less compression artefacts for brightness variations
				{
					bool bDX10 = GetIEditor()->GetRenderer()->GetRenderType()==eRT_DX10;			// i would be cleaner if renderer would ensure the behave the same but that can be slower 

					uint32 dwWidth = image.GetWidth();
					uint32 dwHeight = image.GetHeight();

					uint32 *pMem = &image.ValueAt(0,0);

					for(uint32 dwY=0;dwY<dwHeight;++dwY)
						for(uint32 dwX=0;dwX<dwWidth;++dwX)
						{
#ifdef TERRAIN_USE_CIE_COLORSPACE							
							float fR = ((*pMem>>16)&0xff)*(1.0f/255.0f);
							float fG = ((*pMem>>8)&0xff)*(1.0f/255.0f);
							float fB = ((*pMem)&0xff)*(1.0f/255.0f);

							ColorF cCIE = ColorF(fR,fG,fB).RGB2mCIE();

							uint32 dwR = (uint32)(cCIE.r*255.0f+0.5f);
							uint32 dwG = (uint32)(cCIE.g*255.0f+0.5f);
							uint32 dwB = (uint32)(cCIE.b*255.0f+0.5f);
#else
							uint32 dwR = (*pMem>>16)&0xff;
							uint32 dwG = (*pMem>>8)&0xff;
							uint32 dwB = (*pMem)&0xff;
#endif

							if(bDX10)
								*pMem++ = 0xff000000 | (dwB<<16) | (dwG<<8) | (dwR);		// shader requires alpha channel = 1
							 else
								 *pMem++ = 0xff000000 | (dwR<<16) | (dwG<<8) | (dwB);		// shader requires alpha channel = 1
						}
				}

				GetIEditor()->GetRenderer()->UpdateTextureInVideoMemory( texId,(unsigned char*)image.GetData(),
					iLocalOutMinX,iLocalOutMinY, image.GetWidth(), image.GetHeight(), eTF_A8R8G8B8 );
			}
		}
}

/////////////////////////////////////////////////////////
void CHeightmap::GetLayerIdBlock(int x, int y, int width, int height, CByteImage & map)
{
	if (m_LayerIdBitmap.IsValid())
		m_LayerIdBitmap.GetSubImage( x, y, width, height, map);
}

/////////////////////////////////////////////////////////
void CHeightmap::SetLayerIdBlock(int x, int y, const CByteImage & map)
{
	if (m_LayerIdBitmap.IsValid())
		m_LayerIdBitmap.SetSubImage( x, y, map);
}
