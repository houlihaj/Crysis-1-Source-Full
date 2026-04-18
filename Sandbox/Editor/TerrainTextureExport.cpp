// TerrainLighting.cpp : implementation file
//

#include "stdafx.h"
#include "TerrainTextureExport.h"
#include "TerrainTexture.h"
#include "TerrainTexGen.h"
#include "Heightmap.h"
#include "WaitProgress.h"
#include "CryEditDoc.h"
#include "resource.h"

#define TERRAIN_PREVIEW_RESOLUTION 256


/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureExport dialog

//CDC CTerrainTextureExport::m_dcLightmap;


CTerrainTextureExport::CTerrainTextureExport(CWnd* pParent /*=NULL*/)
	: CDialog(CTerrainTextureExport::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTerrainTextureExport)
	//}}AFX_DATA_INIT

	m_cx = 19;
	m_cy = 24;

	m_bSelectMode = false;
	rcSel.left = rcSel.top = rcSel.right = rcSel.bottom = -1;

	m_pTexGen = new CTerrainTexGen;
	m_pTexGen->Init(TERRAIN_PREVIEW_RESOLUTION,true);
}

CTerrainTextureExport::~CTerrainTextureExport()
{
	delete m_pTexGen;
}

void CTerrainTextureExport::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainTextureExport)
	DDX_Control(pDX, IDC_IMPORT, m_importBtn );
	DDX_Control(pDX, IDC_EXPORT, m_exportBtn );
	DDX_Control(pDX, IDC_CLOSE, m_closeBtn );
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTerrainTextureExport, CDialog)
	//{{AFX_MSG_MAP(CTerrainTextureExport)
	ON_WM_PAINT()
	ON_COMMAND(IDC_EXPORT, OnExport)
	ON_COMMAND(IDC_IMPORT, OnImport)
	ON_COMMAND(IDC_CLOSE, OnCloseBtn)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainTextureExport message handlers


//////////////////////////////////////////////////////////////////////////
BOOL CTerrainTextureExport::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_terrain.Allocate( TERRAIN_PREVIEW_RESOLUTION,TERRAIN_PREVIEW_RESOLUTION );

	GetDlgItem(IDC_FILE)->SetWindowText( gSettings.terrainTextureExport);

	CString maskInfoText="()";
	if(GetIEditor()->GetDocument()->IsNewTerranTextureSystem())
		maskInfoText.Format("RGB(%dx%d)",GetIEditor()->GetDocument()->GetHeightmap().m_TerrainRGBTexture.CalcMinRequiredTextureExtend(),GetIEditor()->GetDocument()->GetHeightmap().m_TerrainRGBTexture.CalcMinRequiredTextureExtend() );
	GetDlgItem(IDC_INFO_TEXT)->SetWindowText( maskInfoText );


	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CTerrainTextureExport::OnPaint()
{
	CPaintDC dc(this);
	DrawPreview(dc);
}


//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::DrawPreview(CDC & dc) 
{
	CHeightmap * heightmap = GetIEditor()->GetHeightmap();

	//CPaintDC dc(this); // device context for painting
	CPen cGrayPen (PS_SOLID, 1, 0x007F7F7F);
	CPen cRedPen  (PS_SOLID, 1, 0x004040ff);
	CPen cGreenPen(PS_SOLID, 1, 0x0000ff00);
	CPen cWhitePen(PS_SOLID, 1, 0x00FFFFFF);
	CBrush brush(0x00808080);

	// Generate a preview if we don't have one
	if (!m_dcLightmap.m_hDC)
		OnGenerate();

	uint32 dwTileCountX = heightmap->m_TerrainRGBTexture.GetTileCountX();
	uint32 dwTileCountY = heightmap->m_TerrainRGBTexture.GetTileCountY();
	
	// Draw the preview
	dc.BitBlt(m_cx, m_cy, TERRAIN_PREVIEW_RESOLUTION, TERRAIN_PREVIEW_RESOLUTION, &m_dcLightmap, 0, 0, SRCCOPY);

	CPen *prevPen = dc.SelectObject(&cGrayPen);
	{
		dc.SetBkMode(TRANSPARENT);
		for(int x=0; x<dwTileCountX; x++)
			for(int y=0; y<dwTileCountY; y++)
			{
				RECT rc = {m_cx + x*TERRAIN_PREVIEW_RESOLUTION/dwTileCountX, m_cy + y*TERRAIN_PREVIEW_RESOLUTION/dwTileCountY,
					m_cx + (x+1)*TERRAIN_PREVIEW_RESOLUTION/dwTileCountX, m_cy + (y+1)*TERRAIN_PREVIEW_RESOLUTION/dwTileCountY};

        uint32 dwLocalSize = heightmap->m_TerrainRGBTexture.GetTileResolution(x,y);
        dc.SetTextColor(RGB(SATURATEB(dwLocalSize/8),0,0));

				if(rcSel.left<=x && rcSel.right>=x && rcSel.top<=y && rcSel.bottom>=y)
				{
					RECT rc = {m_cx + x*TERRAIN_PREVIEW_RESOLUTION/dwTileCountX+4, m_cy + y*TERRAIN_PREVIEW_RESOLUTION/dwTileCountY+4,
						m_cx + (x+1)*TERRAIN_PREVIEW_RESOLUTION/dwTileCountX - 4, m_cy + (y+1)*TERRAIN_PREVIEW_RESOLUTION/dwTileCountY-4};
					dc.FillRect(&rc, &brush);
				}

        char str[32];
				if(dwLocalSize == 1024)
					sprintf( str,"1k");
				else if(dwLocalSize == 2048)
					sprintf( str,"2k");
				else if(dwLocalSize == 4096)
					sprintf( str,"4k");
				else
					sprintf( str,"%d", dwLocalSize );
				dc.DrawText( str, &rc, DT_SINGLELINE|DT_CENTER|DT_VCENTER );
			}
	}

	dc.SelectObject(&cGreenPen);
	for(int x=0; x<=dwTileCountX; x++)
	{
		dc.MoveTo( m_cx + x*TERRAIN_PREVIEW_RESOLUTION/dwTileCountX, m_cy );
		dc.LineTo( m_cx + x*TERRAIN_PREVIEW_RESOLUTION/dwTileCountX, m_cy+TERRAIN_PREVIEW_RESOLUTION );
	}
	for(int y=0; y<=dwTileCountY; y++)
	{
		dc.MoveTo( m_cx , m_cy + y*TERRAIN_PREVIEW_RESOLUTION/dwTileCountY );
		dc.LineTo( m_cx + TERRAIN_PREVIEW_RESOLUTION, m_cy + y*TERRAIN_PREVIEW_RESOLUTION/dwTileCountY );
	}

	if(rcSel.top>=0)
	{
		RECT rc = {m_cx + rcSel.left*TERRAIN_PREVIEW_RESOLUTION/dwTileCountX+1, m_cy + rcSel.top*TERRAIN_PREVIEW_RESOLUTION/dwTileCountY+1,
			m_cx + (rcSel.right+1)*TERRAIN_PREVIEW_RESOLUTION/dwTileCountX-1, m_cy + (rcSel.bottom+1)*TERRAIN_PREVIEW_RESOLUTION/dwTileCountY-1};

		dc.SelectObject(&cRedPen);

		dc.MoveTo( rc.left, rc.top );
		dc.LineTo( rc.right, rc.top );
		dc.LineTo( rc.right, rc.bottom );
		dc.LineTo( rc.left, rc.bottom );
		dc.LineTo( rc.left, rc.top );
	}

	dc.SelectObject(prevPen);
}

void CTerrainTextureExport::OnGenerate() 
{
	RECT rcUpdate;

	BeginWaitCursor();

	LightingSettings *ls = GetIEditor()->GetDocument()->GetLighting();

	int skyQuality = ls->iHemiSamplQuality;
	ls->iHemiSamplQuality = 0;
			
	// Create a DC and a bitmap
	if (!m_dcLightmap.m_hDC)
		VERIFY(m_dcLightmap.CreateCompatibleDC(NULL));
	if (!m_bmpLightmap.m_hObject)
		VERIFY(m_bmpLightmap.CreateBitmap(TERRAIN_PREVIEW_RESOLUTION, TERRAIN_PREVIEW_RESOLUTION, 1, 32, NULL));
	m_dcLightmap.SelectObject(&m_bmpLightmap);

	// Calculate the lighting.
	m_pTexGen->InvalidateLighting();
	m_pTexGen->GenerateSurfaceTexture(ETTG_LIGHTING|ETTG_QUIET|ETTG_ABGR|ETTG_BAKELIGHTING|ETTG_NOTEXTURE|ETTG_SHOW_WATER, m_terrain);

	// put m_terrain into m_bmpLightmap
	{
		BITMAPINFO bi;

		ZeroStruct(bi);
		bi.bmiHeader.biSize = sizeof(bi);
		bi.bmiHeader.biWidth = m_terrain.GetWidth();
		bi.bmiHeader.biHeight = -m_terrain.GetHeight();
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biCompression = BI_RGB;

		SetDIBits(m_dcLightmap.m_hDC,(HBITMAP)m_bmpLightmap.GetSafeHandle(),0,m_terrain.GetHeight(),m_terrain.GetData(),&bi,false);
	}

	// Update the preview
	::SetRect(&rcUpdate, 10, 10, 20 + TERRAIN_PREVIEW_RESOLUTION, 30 + TERRAIN_PREVIEW_RESOLUTION);
	InvalidateRect(&rcUpdate, FALSE);
	UpdateWindow();
	ls->iHemiSamplQuality = skyQuality;

	EndWaitCursor();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::ImportExport(const CPoint & minp, const CPoint & maxp, bool bIsImport, bool bIsClipboard)
{
	CHeightmap * heightmap = GetIEditor()->GetHeightmap();

	if(!bIsClipboard)
	{
		//if(!strlen(gSettings.terrainTextureExport))
		if(!gSettings.BrowseTerrainTexture(!bIsImport))
			return;

		GetDlgItem(IDC_FILE)->SetWindowText( gSettings.terrainTextureExport);

		if(!strlen(gSettings.terrainTextureExport))
		{
			AfxMessageBox("Error: Need to specify file name.");
			return;
		}
	}

	uint32 square;

	heightmap->m_TerrainRGBTexture.ImportExportBlock( bIsClipboard ? 0 : gSettings.terrainTextureExport,
		(float)minp.x/heightmap->GetWidth(), (float)minp.y/heightmap->GetHeight(), 
		(float)maxp.x/heightmap->GetWidth(), (float)maxp.y/heightmap->GetHeight(), &square, bIsImport);

	if(bIsImport)
	{
		RECT rc = {minp.x, minp.y, maxp.x, maxp.y};
		if(square > 2048*2048)
			AfxMessageBox("Save a level and generate surface texture to see the result.");
		else
			heightmap->UpdateLayerTexture(rc);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnExport() 
{
	if(rcSel.top<0)
		return;

	CHeightmap * heightmap = GetIEditor()->GetHeightmap();

	uint32 dwTileCountX = heightmap->m_TerrainRGBTexture.GetTileCountX();
	uint32 dwTileCountY = heightmap->m_TerrainRGBTexture.GetTileCountY();

	ImportExport(CPoint(rcSel.left * heightmap->GetWidth() / dwTileCountX, rcSel.top * heightmap->GetHeight() / dwTileCountY),
		CPoint((rcSel.right+1) * heightmap->GetWidth() / dwTileCountX, (rcSel.bottom+1) * heightmap->GetHeight() / dwTileCountY)
		);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnImport() 
{
	if(rcSel.top<0)
		return;

	CHeightmap * heightmap = GetIEditor()->GetHeightmap();

	uint32 dwTileCountX = heightmap->m_TerrainRGBTexture.GetTileCountX();
	uint32 dwTileCountY = heightmap->m_TerrainRGBTexture.GetTileCountY();

	ImportExport(CPoint(rcSel.left * heightmap->GetWidth() / dwTileCountX, rcSel.top * heightmap->GetHeight() / dwTileCountY),
		CPoint((rcSel.right+1) * heightmap->GetWidth() / dwTileCountX, (rcSel.bottom+1) * heightmap->GetHeight() / dwTileCountY)
		, true
		);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnLButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonDown(nFlags, point);

	CHeightmap * heightmap = GetIEditor()->GetHeightmap();

	uint32 dwTileCountX = heightmap->m_TerrainRGBTexture.GetTileCountX();
	uint32 dwTileCountY = heightmap->m_TerrainRGBTexture.GetTileCountY();

	int x = (point.x-m_cx)*dwTileCountX/TERRAIN_PREVIEW_RESOLUTION;
	int y = (point.y-m_cy)*dwTileCountY/TERRAIN_PREVIEW_RESOLUTION;

	if(x>=0 && y>=0 && x<dwTileCountX && y< dwTileCountY)
	{
		rcSel.left = x;
		rcSel.top = y;
		rcSel.right = x;
		rcSel.bottom = y;
		m_bSelectMode = true;
		SetCapture();
	}
	else
		rcSel.left = rcSel.top = rcSel.right = rcSel.bottom = -1;

	DrawPreview(*(GetDC()));
}


//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnLButtonUp(UINT nFlags, CPoint point)
{
	if(m_bSelectMode)
	{
		ReleaseCapture();
		m_bSelectMode = false;
	}
	CDialog::OnLButtonUp(nFlags, point);
}


//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnMouseMove(UINT nFlags, CPoint point)
{
	CDialog::OnMouseMove(nFlags, point);

	if(!m_bSelectMode)
		return;

	if(nFlags & MK_LBUTTON)
	{
		CHeightmap * heightmap = GetIEditor()->GetHeightmap();

		uint32 dwTileCountX = heightmap->m_TerrainRGBTexture.GetTileCountX();
		uint32 dwTileCountY = heightmap->m_TerrainRGBTexture.GetTileCountY();

		int x = (point.x-m_cx)*dwTileCountX/TERRAIN_PREVIEW_RESOLUTION;
		int y = (point.y-m_cy)*dwTileCountY/TERRAIN_PREVIEW_RESOLUTION;

		if(	x>=0 && y>=0 && x>=rcSel.left && y>=rcSel.top && x<dwTileCountX && y<dwTileCountY 
				&& (x!=rcSel.right || y!=rcSel.bottom) )
		{
			rcSel.right = x;
			rcSel.bottom = y;
			DrawPreview(*(GetDC()));
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CTerrainTextureExport::OnCloseBtn()
{
	OnCancel();
}