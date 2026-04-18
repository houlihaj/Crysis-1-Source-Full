// TopRendererWnd.cpp: implementation of the CTopRendererWnd class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TopRendererWnd.h"
#include "Heightmap.h"
#include "VegetationMap.h"
#include "DisplaySettings.h"
#include "EditTool.h"
#include "ViewManager.h"
#include "Objects\ObjectManager.h"

#include "TerrainTexGen.h"
#include "TerrainModifyTool.h"
//#include "Image.h"

// Size of the surface texture
#define SURFACE_TEXTURE_WIDTH 512

#define MARKER_SIZE 6.0f
#define MARKER_DIR_SIZE 10.0f
#define SELECTION_RADIUS 30.0f

#define GL_RGBA 0x1908
#define GL_BGRA 0x80E1


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTopRendererWnd,C2DViewport)

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTopRendererWnd, C2DViewport)
	//{{AFX_MSG_MAP(CTopRendererWnd)
	//}}AFX_MSG_MAP
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// Used to give each static object type a different color
static uint sVegetationColors[16] =
{
	0xFFFF0000,
	0xFF00FF00,
	0xFF0000FF,
	0xFFFFFFFF,
	0xFFFF00FF,
	0xFFFFFF00,
	0xFF00FFFF,
	0xFF7F00FF,
	0xFF7FFF7F,
	0xFFFF7F00,
	0xFF00FF7F,
	0xFF7F7F7F,
	0xFFFF0000,
	0xFF00FF00,
	0xFF0000FF,
	0xFFFFFFFF,
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTopRendererWnd::CTopRendererWnd()
{
	////////////////////////////////////////////////////////////////////////
	// Set the window type member of the base class to the correct type and
	// create the initial surface texture
	////////////////////////////////////////////////////////////////////////

	if (gSettings.viewports.bTopMapSwapXY)
		SetAxis( VPA_YX );
	else
		SetAxis( VPA_XY );

	m_bShowHeightmap = false;
	m_bShowStatObjects = false;
	m_bLastShowHeightmapState = false;

	////////////////////////////////////////////////////////////////////////
	// Surface texture
	////////////////////////////////////////////////////////////////////////

	m_textureSize.cx = gSettings.viewports.nTopMapTextureResolution;
	m_textureSize.cy = gSettings.viewports.nTopMapTextureResolution;

	m_heightmapSize = CSize(1,1);

	m_terrainTextureId = 0;

	m_vegetationTextureId = 0;
	m_bFirstTerrainUpdate = true;
	m_bShowWater = false;
	
	// Create a new surface texture image
	ResetSurfaceTexture();

	m_bContentsUpdated = false;

	m_gridAlpha = 0.3f;
	m_colorGridText = RGB(255,255,255);
	m_colorAxisText = RGB(255,255,255);
	m_colorBackground = RGB(128,128,128);

	CHeightmap *heightmap = GetIEditor()->GetHeightmap();
	if (heightmap)
	{
		//SSectorInfo si;
		//heightmap->GetSectorsInfo(si);
		//float sizeMeters = si.numSectors*si.sectorSize;
		//float mid = sizeMeters/2;
		//SetScrollOffset( 0,-sizeMeters );

	}
}

//////////////////////////////////////////////////////////////////////////
CTopRendererWnd::~CTopRendererWnd()
{
	////////////////////////////////////////////////////////////////////////
	// Destroy the attached render and free the surface texture
	////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::SetType( EViewportType type )
{
	m_viewType = type;
	m_axis = VPA_YX;
	SetAxis( m_axis );
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::ResetContent()
{
	C2DViewport::ResetContent();
	ResetSurfaceTexture();

	// Reset texture ids.
	m_terrainTextureId = 0;
	m_vegetationTextureId = 0;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::UpdateContent( int flags )
{
	if (gSettings.viewports.bTopMapSwapXY)
		SetAxis( VPA_YX );
	else
		SetAxis( VPA_XY );

	C2DViewport::UpdateContent(flags);
	if (!GetIEditor()->GetDocument())
		return;

	CHeightmap *heightmap = GetIEditor()->GetHeightmap();
	if (!heightmap)
		return;

	if (!m_hWnd)
		return;

	if (!IsWindowVisible())
		return;

	m_heightmapSize.cx = heightmap->GetWidth() * heightmap->GetUnitSize();
	m_heightmapSize.cy = heightmap->GetHeight() * heightmap->GetUnitSize();

	UpdateSurfaceTexture( flags );
	m_bContentsUpdated = true;

	// if first update.
	if (m_bFirstTerrainUpdate)
	{
		CHeightmap *heightmap = GetIEditor()->GetHeightmap();
		if (heightmap)
		{
			SSectorInfo si;
			heightmap->GetSectorsInfo(si);
			float sizeMeters = si.numSectors*si.sectorSize;
			float mid = sizeMeters/2;
			//SetScrollOffset( 0,-sizeMeters );

			SetZoom( 0.95f*m_rcClient.Width()/sizeMeters,CPoint(m_rcClient.Width()/2,m_rcClient.Height()/2) );
			SetScrollOffset(-10,-10);
		}
	}
	m_bFirstTerrainUpdate = false;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::UpdateSurfaceTexture( int flags )
{
	////////////////////////////////////////////////////////////////////////
	// Generate a new surface texture
	////////////////////////////////////////////////////////////////////////
	if (flags & eUpdateHeightmap)
	{
		bool bShowHeightmap = m_bShowHeightmap;

		CEditTool *pTool = GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CTerrainModifyTool)))
		{
			bShowHeightmap = true;
		}

		bool bRedrawFullTexture = m_bLastShowHeightmapState != bShowHeightmap;

		m_bLastShowHeightmapState = bShowHeightmap;
		if (!bShowHeightmap)
		{
			m_textureSize.cx = gSettings.viewports.nTopMapTextureResolution;
			m_textureSize.cy = gSettings.viewports.nTopMapTextureResolution;
			
			m_terrainTexture.Allocate( m_textureSize.cx,m_textureSize.cy );
			
			int flags = ETTG_LIGHTING|ETTG_FAST_LLIGHTING|ETTG_ABGR|ETTG_BAKELIGHTING;
			if (m_bShowWater)
				flags |= ETTG_SHOW_WATER;
			// Fill in the surface data into the array. Apply lighting and water, use
			// the settings from the document
			CTerrainTexGen texGen;
			texGen.GenerateSurfaceTexture( flags,m_terrainTexture );
		}
		else
		{
			int cx = gSettings.viewports.nTopMapTextureResolution;
			int cy = gSettings.viewports.nTopMapTextureResolution;
			if (cx != m_textureSize.cx || cy != m_textureSize.cy || !m_terrainTexture.GetData())
			{
				m_textureSize.cx = cx;
				m_textureSize.cy = cy;
				m_terrainTexture.Allocate( m_textureSize.cx,m_textureSize.cy );
			}
			
			BBox box = GetIEditor()->GetViewManager()->GetUpdateRegion();
			CRect updateRect = GetIEditor()->GetHeightmap()->WorldBoundsToRect( box );
			CRect *pUpdateRect = &updateRect;
			if (bRedrawFullTexture)
				pUpdateRect = 0;
			GetIEditor()->GetHeightmap()->GetPreviewBitmap( (DWORD*)m_terrainTexture.GetData(),m_textureSize.cx,false,false,pUpdateRect,m_bShowWater );
		}
	}

	if (flags == eUpdateStatObj)
	{
		// If The only update flag is Update of static objects, display them.
		m_bShowStatObjects = true;
	}

	if (flags & eUpdateStatObj)
	{
		if (m_bShowStatObjects)
		{
			DrawStaticObjects();
		}
	}
	m_bContentValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::DrawStaticObjects()
{
	if (!m_bShowStatObjects)
		return;

	CVegetationMap *vegetationMap = GetIEditor()->GetVegetationMap();
	int srcW = vegetationMap->GetNumSectors();
	int srcH = vegetationMap->GetNumSectors();

	CRect rc = CRect(0,0,srcW,srcH);
	BBox updateRegion = GetIEditor()->GetViewManager()->GetUpdateRegion();
	if (updateRegion.min.x > -10000)
	{
		// Update region valid.
		CRect urc;
		CPoint p1 = CPoint( vegetationMap->WorldToSector(updateRegion.min.y),vegetationMap->WorldToSector(updateRegion.min.x) );
		CPoint p2 = CPoint( vegetationMap->WorldToSector(updateRegion.max.y),vegetationMap->WorldToSector(updateRegion.max.x) );
		urc = CRect( p1.x-1,p1.y-1,p2.x+1,p2.y+1 );
		rc &= urc;
	}

	int trgW = rc.right - rc.left;
	int trgH = rc.bottom - rc.top;

	if (trgW <= 0 || trgH <= 0)
		return;

	m_vegetationTexturePos = CPoint( rc.left,rc.top );
	m_vegetationTextureSize = CSize( srcW,srcH );
	m_vegetationTexture.Allocate( trgW,trgH );

	uint *trg = m_vegetationTexture.GetData();
	vegetationMap->DrawToTexture( trg,trgW,trgH,rc.left,rc.top );
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::ResetSurfaceTexture()
{
	////////////////////////////////////////////////////////////////////////
	// Create a surface texture that consists entirely of water
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;
	DWORD *pSurfaceTextureData = NULL;
	DWORD *pPixData = NULL, *pPixDataEnd = NULL;
	CBitmap bmpLoad;
	BOOL bReturn;

	// Load the water texture out of the ressource
	bReturn = bmpLoad.Attach(::LoadBitmap(AfxGetApp()->m_hInstance,	MAKEINTRESOURCE(IDB_WATER)));
	ASSERT(bReturn);

	// Allocate new memory to hold the bitmap data
	CImage waterImage;
	waterImage.Allocate( 128,128 );

	// Retrieve the bits from the bitmap
	bmpLoad.GetBitmapBits( 128*128*sizeof(DWORD), waterImage.GetData() );
 
	// Allocate memory for the surface texture
	m_terrainTexture.Allocate( m_textureSize.cx,m_textureSize.cy );

	// Fill the surface texture with the water texture, tile as needed
	for (j=0; j<m_textureSize.cy; j++)
		for (i=0; i<m_textureSize.cx; i++)
		{
			m_terrainTexture.ValueAt(i,j) = waterImage.ValueAt( i&127,j&127 );
		}

	m_bContentValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::Draw( DisplayContext &dc )
{
	FUNCTION_PROFILER( GetIEditor()->GetSystem(),PROFILE_EDITOR );

	////////////////////////////////////////////////////////////////////////
	// Perform the rendering for this window
	////////////////////////////////////////////////////////////////////////
	if (!m_bContentsUpdated)
		UpdateContent( 0xFFFFFFFF );

	////////////////////////////////////////////////////////////////////////
	// Render the 2D map
	////////////////////////////////////////////////////////////////////////
	if (!m_terrainTextureId)
	{
		//GL_BGRA_EXT
		if (m_terrainTexture.IsValid())
		{
			m_terrainTextureId = m_renderer->DownLoadToVideoMemory( (unsigned char*)m_terrainTexture.GetData(),m_textureSize.cx,m_textureSize.cy,eTF_A8R8G8B8,eTF_A8R8G8B8,0,0,0 );
			//m_terrainTexture.Release();
		}
	}

	if (m_terrainTextureId && m_terrainTexture.IsValid())
	{
		m_renderer->UpdateTextureInVideoMemory( m_terrainTextureId,(unsigned char*)m_terrainTexture.GetData(),0,0,m_textureSize.cx,m_textureSize.cy,eTF_A8R8G8B8 );
		//m_renderer->RemoveTexture( m_terrainTextureId );
		//m_terrainTextureId = m_renderer->DownLoadToVideoMemory( (unsigned char*)m_terrainTexture.GetData(),m_textureSize.cx,m_textureSize.cy,eTF_8888,eTF_8888,0,0,0 );
		//m_terrainTexture.Release();
	}


	if (m_bShowStatObjects)
	{
		if (m_vegetationTexture.IsValid())
		{
			int w = m_vegetationTexture.GetWidth();
			int h = m_vegetationTexture.GetHeight();
			uint *tex = m_vegetationTexture.GetData();
			if (!m_vegetationTextureId)
			{
				m_vegetationTextureId = m_renderer->DownLoadToVideoMemory( (unsigned char*)tex,w,h,eTF_A8R8G8B8,eTF_A8R8G8B8,0,0,FILTER_NONE );
			}
			else
			{
				int px = m_vegetationTexturePos.x;
				int py = m_vegetationTexturePos.y;
				m_renderer->UpdateTextureInVideoMemory( m_vegetationTextureId,(unsigned char*)tex,px,py,w,h,eTF_A8R8G8B8 );
			}
			m_vegetationTexture.Release();
		}
	}
	

	// Reset states
	m_renderer->ResetToDefault();
	// Disable depth test
	//m_renderer->EnableDepthTest(false);
	//m_renderer->EnableDepthWrites(false);

	dc.DepthTestOff();

	//m_renderer->EnableBlend(false);
	// Draw the map into the view
	
	//m_renderer->Draw2dImage(iX, iY+iHeight, iWidth, -iHeight, m_terrainTextureId);
	Matrix34 tm = GetScreenTM();
//	Matrix34 rot;
//	rot.SetIdentity();
//	rot.SetFromVectors( Vec3(0,1,0),Vec3(1,0,0),Vec3(0,0,1),Vec3(0,0,0) );

//	dc.PushMatrix( tm*rot );

//	m_renderer->LoadMatrix(&(tm*rot));
//	m_renderer->DrawImage( 0,0,m_heightmapSize.cx,m_heightmapSize.cy, m_terrainTextureId,0,1,1,0,1,1,1,1 );

	if (m_axis == VPA_YX)
	{
		float s[4],t[4];

		s[0]=0;	t[0]=0;
		s[1]=1;	t[1]=0;
		s[2]=1;	t[2]=1;
		s[3]=0;	t[3]=1;
		m_renderer->DrawImageWithUV( tm.m03,tm.m13,0,tm.m01*m_heightmapSize.cx,tm.m10*m_heightmapSize.cy, m_terrainTextureId, s,t);
	}
	else
	{
		float s[4],t[4];
		s[0]=0;	t[0]=0;
		s[1]=0;	t[1]=1;
		s[2]=1;	t[2]=1;
		s[3]=1;	t[3]=0;
		m_renderer->DrawImageWithUV( tm.m03,tm.m13,0,tm.m00*m_heightmapSize.cx,tm.m11*m_heightmapSize.cy, m_terrainTextureId, s,t);
	}

		//m_renderer->DrawImageWithUV( tm.m03,tm.m13,0,tm.m00*m_heightmapSize.cx,tm.m11*m_heightmapSize.cy, m_terrainTextureId, s,t);

//	m_renderer->PopMatrix();
//	dc.PopMatrix();

	//m_renderer->EnableDepthTest(true);
	//m_renderer->EnableDepthWrites(true);

	//m_renderer->EnableBlend(true);

/*
	// Draw the static objects into the view
	if (m_bShowStatObjects && m_vegetationTextureId)
	{
		m_renderer->SetBlendMode();
		m_renderer->EnableBlend(true);

		GetMapRect( m_textureSize,rcMap );

		iWidth = (m_vegetationTextureSize.cx * m_fZoomFactor) * ((float)m_textureSize.cx/m_vegetationTextureSize.cx);
		iHeight = (m_vegetationTextureSize.cy * m_fZoomFactor) * ((float)m_textureSize.cy/m_vegetationTextureSize.cy);

		// Convert the window coordinates into coordinates for the renderer
		iX =  ((float)rcMap.left / rcWnd.right * 800.0f);
		iY =  ((float)rcMap.top / rcWnd.bottom * 600.0f);
		iWidth = ((float)iWidth / rcWnd.right * 800.0f);
		iHeight = ((float)iHeight / rcWnd.bottom * 600.0f);
		m_renderer->Draw2dImage(iX, iY+iHeight, iWidth, -iHeight, m_vegetationTextureId);
		m_renderer->EnableBlend(false);
	}
*/

	dc.DepthTestOn();

	C2DViewport::Draw( dc );
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::OnTitleMenu( CMenu &menu )
{
	bool labels = GetIEditor()->GetDisplaySettings()->IsDisplayLabels();
	menu.AppendMenu( MF_STRING|((labels)?MF_CHECKED:MF_UNCHECKED),1,"Labels" );
	menu.AppendMenu( MF_STRING|((m_bShowHeightmap)?MF_CHECKED:MF_UNCHECKED),2,"Show Heightmap" );
	menu.AppendMenu( MF_STRING|((m_bShowStatObjects)?MF_CHECKED:MF_UNCHECKED),3,"Show Static Objects" );
	menu.AppendMenu( MF_STRING|((m_bShowWater)?MF_CHECKED:MF_UNCHECKED),4,"Show Water" );
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::OnTitleMenuCommand( int id )
{
	switch (id)
	{
		case 1:
			GetIEditor()->GetDisplaySettings()->DisplayLabels( !GetIEditor()->GetDisplaySettings()->IsDisplayLabels() );
			break;
		case 2:
			m_bShowHeightmap = !m_bShowHeightmap;
			UpdateContent( 0xFFFFFFFF );
			break;
		case 3:
			m_bShowStatObjects = !m_bShowStatObjects;
			if (m_bShowStatObjects)
				UpdateContent( eUpdateStatObj );
			break;
		case 4:
			m_bShowWater = !m_bShowWater;
			UpdateContent( eUpdateStatObj );
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3	CTopRendererWnd::ViewToWorld( CPoint vp,bool *collideWithTerrain,bool onlyTerrain )
{
	Vec3 wp = C2DViewport::ViewToWorld( vp,collideWithTerrain,onlyTerrain );
	wp.z = GetIEditor()->GetTerrainElevation( wp.x,wp.y );
	return wp;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::SetShowWater( bool bEnable )
{
	m_bShowWater = bEnable;
}
