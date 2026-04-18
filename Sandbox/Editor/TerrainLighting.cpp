// TerrainLighting.cpp : implementation file
//

#include "stdafx.h"
#include "TerrainLighting.h"
#include "CryEditDoc.h"
#include "TerrainTexture.h"
#include "Heightmap.h"
#include "WaitProgress.h"
#include "GameEngine.h"
#include "resource.h"
#include "TerrainTexGen.h"

#define LIGHTING_PREVIEW_RESOLUTION 256

const int MOON_SUN_TRANS_WIDTH = 400;
const int MOON_SUN_TRANS_HEIGHT = 10;

/////////////////////////////////////////////////////////////////////////////
// CTerrainLighting dialog


CTerrainLighting::CTerrainLighting(CWnd* pParent /*=NULL*/)
	: CDialog(CTerrainLighting::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTerrainLighting)
	m_optLightingAlgo = -1;
	m_sldSunDirection = 240;
	m_sldLongitude = 90;		// equator
//	m_bTerrainShadows = FALSE;
	m_sldILApplySS = 1;
	m_sldSkyQuality=0;
//	m_sldSkyBrightening=0;
	m_sldDawnTime = 355;
	m_sldDawnDuration = 10;
	m_sldDuskTime = 365;
	m_sldDuskDuration = 10;
	//}}AFX_DATA_INIT
	m_pTexGen = new CTerrainTexGen;
	m_pTexGen->Init(LIGHTING_PREVIEW_RESOLUTION,true);
}

CTerrainLighting::~CTerrainLighting()
{
	delete m_pTexGen;
}

void CTerrainLighting::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainLighting)
	DDX_Radio(pDX, IDC_PRECISELIGHTING2, m_optLightingAlgo);
	DDX_Slider(pDX, IDC_SUN_DIRECTION, m_sldSunDirection);
	DDX_Slider(pDX, IDC_SUNMAPLONGITUDE, m_sldLongitude);
//	DDX_Check(pDX, IDC_TERRAINSHADOWS, m_bTerrainShadows);
	DDX_Check(pDX, IDC_OBJECTTERRAINOCCL, m_sldILApplySS);
	DDX_Slider(pDX, IDC_HEMISAMPLEQ, m_sldSkyQuality);
//	DDX_Slider(pDX, IDC_SKYBRIGHTENING, m_sldSkyBrightening);	
	DDX_Slider(pDX, IDC_DAWN_SLIDER, m_sldDawnTime);
	DDX_Slider(pDX, IDC_DAWN_DUR_SLIDER, m_sldDawnDuration);
	DDX_Slider(pDX, IDC_DUSK_SLIDER, m_sldDuskTime);
	DDX_Slider(pDX, IDC_DUSK_DUR_SLIDER, m_sldDuskDuration);	
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTerrainLighting, CDialog)
	//{{AFX_MSG_MAP(CTerrainLighting)
	ON_WM_PAINT()
	ON_WM_HSCROLL()
//	ON_BN_CLICKED(IDC_TERRAINSHADOWS, OnTerrainShadows)
	ON_BN_CLICKED(IDC_UPDATEIL, OnUpdateIL)
	ON_BN_CLICKED(IDC_OBJECTTERRAINOCCL, OnApplyILSS)
	ON_COMMAND(ID_FILE_EXPORT, OnFileExport)
	ON_COMMAND(ID_FILE_IMPORT, OnFileImport)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_BN_CLICKED(IDC_PRECISELIGHTING2, OnBnClickedPrecise)
	ON_BN_CLICKED(IDC_DYNAMICSUN, OnDynamicSun)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainLighting message handlers

void CTerrainLighting::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	RECT rcClient;
	CPen cGrayPen(PS_SOLID, 1, 0x007F7F7F);
	CPen cWhitePen(PS_SOLID, 1, 0x00FFFFFF);
	CPen cRedPen(PS_SOLID, 1, 0x000000FF);

	// Generate a preview if we don't have one
	if (!m_dcLightmap.m_hDC)
		GenerateLightmap(false);	
	// Draw the preview of the lightmap
	dc.BitBlt(19, 24, LIGHTING_PREVIEW_RESOLUTION, LIGHTING_PREVIEW_RESOLUTION, &m_dcLightmap, 0, 0, SRCCOPY);
	
	if (!m_dcMoonSunTrans.m_hDC)
		GenerateMoonSunTransition(false);	
	dc.BitBlt(315, 245, MOON_SUN_TRANS_WIDTH, MOON_SUN_TRANS_HEIGHT, &m_dcMoonSunTrans, 0, 0, SRCCOPY);

	CPen *prevPen = dc.SelectObject(&cRedPen);
	{
		for (int i(0); i<5; ++i)
		{
			int x = 315 + i * (MOON_SUN_TRANS_WIDTH / 4);
			dc.MoveTo(x, 245 - 3);
			dc.LineTo(x, 245 + MOON_SUN_TRANS_HEIGHT + 3);
		}
	}

	CRect rc(19,24,19+LIGHTING_PREVIEW_RESOLUTION,24+LIGHTING_PREVIEW_RESOLUTION);

	// Draw Sun direction.

	CPoint center = rc.CenterPoint();
//	Vec3 sunVector = GetIEditor()->GetDocument()->GetLighting()->GetSunVector();
/*	sunVector.Normalize();				// should never fail
	sunVector.z = 0;
	sunVector.NormalizeSafe();		// to avoid (0,0,0).Normalize
	CPoint source = center + CPoint(-sunVector.x*120,-sunVector.y*120);
*/
	float fSunRot = 2*gf_PI * (90-m_sldSunDirection)/360.0f;
	float fLongitude = 0.5f*gf_PI-gf_PI * m_sldLongitude/180.0f;

	uint32 dwI=0;
	for(double fAngle=0; fAngle<=2*gf_PI; fAngle+=2*gf_PI/64.0,++dwI)
	{
		const float fR=120;

		Matrix33 a,b,c,m;
		
		a.SetRotationZ(fAngle);
		b.SetRotationX(fLongitude);
		c.SetRotationY(fSunRot);

		m = a*b*c;

		Vec3 vLocalSunDir = Vec3(0,fR,0)*m;
		Vec3 vLocalSunDirLeft = Vec3(-fR*0.05f,fR,fR*0.05f)*m;
		Vec3 vLocalSunDirRight = Vec3(-fR*0.05f,fR,-fR*0.05f)*m;

		CPoint point = center + CPoint((int)vLocalSunDir.x,(int)vLocalSunDir.z);
		CPoint pointLeft = center + CPoint((int)vLocalSunDirLeft.x,(int)vLocalSunDirLeft.z);
		CPoint pointRight = center + CPoint((int)vLocalSunDirRight.x,(int)vLocalSunDirRight.z);

		if(dwI==0)
			dc.MoveTo( point );
		else
		{
			if(vLocalSunDir.y>-0.1f)		// beginning or below surface
			{
				dc.MoveTo( pointLeft );
				dc.LineTo( point );
				dc.LineTo( pointRight );
				dc.MoveTo( point );
			}
		}
	}

//	dc.MoveTo( center );dc.LineTo( source );		// north line


	// Draw a menu bar separator
	GetClientRect(&rcClient);
	dc.SelectObject(&cGrayPen);

	dc.MoveTo(0, 0);
	dc.LineTo(rcClient.right, 0);
	dc.SelectObject(&cWhitePen);
	dc.MoveTo(0, 1);
	dc.LineTo(rcClient.right, 1);

	dc.SelectObject(prevPen);
		
	// Do not call CDialog::OnPaint() for painting messages
}

void CTerrainLighting::GenerateLightmap(bool drawOntoDialog)
{
	BeginWaitCursor();

	// Generate a preview of the light map

	// Create a DC and a bitmap
	if (!m_dcLightmap.m_hDC)
		VERIFY(m_dcLightmap.CreateCompatibleDC(NULL));
	if (!m_bmpLightmap.m_hObject)
		VERIFY(m_bmpLightmap.CreateBitmap(LIGHTING_PREVIEW_RESOLUTION, LIGHTING_PREVIEW_RESOLUTION, 1, 32, NULL));
	m_dcLightmap.SelectObject(&m_bmpLightmap);

	// Calculate the lighting.
	m_pTexGen->InvalidateLighting();

	int flags=ETTG_LIGHTING|ETTG_QUIET|ETTG_ABGR|ETTG_INVALIDATE_LAYERS|ETTG_BAKELIGHTING;

	// no texture needed
	flags|=ETTG_NOTEXTURE;
	flags|=ETTG_SHOW_WATER;

	m_pTexGen->GenerateSurfaceTexture(flags,m_lightmap);

	// put m_lightmap into m_bmpLightmap
	{
		BITMAPINFO bi;

		ZeroStruct(bi);
		bi.bmiHeader.biSize = sizeof(bi);
		bi.bmiHeader.biWidth = m_lightmap.GetWidth();
		bi.bmiHeader.biHeight = -m_lightmap.GetHeight();
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biCompression = BI_RGB;

		SetDIBits(m_dcLightmap.m_hDC,(HBITMAP)m_bmpLightmap.GetSafeHandle(),0,m_lightmap.GetHeight(),m_lightmap.GetData(),&bi,false);
	}

	// Update the preview
	if (drawOntoDialog)
	{
		RECT rcUpdate;
		::SetRect(&rcUpdate, 10, 10, 20 + LIGHTING_PREVIEW_RESOLUTION, 30 + LIGHTING_PREVIEW_RESOLUTION);
		InvalidateRect(&rcUpdate, FALSE);
		UpdateWindow();
	}

	EndWaitCursor();
}

void CTerrainLighting::GenerateMoonSunTransition(bool drawOntoDialog)
{
	BeginWaitCursor();

	// Generate a preview of the moon sun transitions

	// Create a DC and a bitmap
	if (!m_dcMoonSunTrans.m_hDC)
		VERIFY(m_dcMoonSunTrans.CreateCompatibleDC(NULL));
	if (!m_bmpMoonSunTrans.m_hObject)
		VERIFY(m_bmpMoonSunTrans.CreateBitmap(MOON_SUN_TRANS_WIDTH, MOON_SUN_TRANS_HEIGHT, 1, 32, NULL));
	m_dcMoonSunTrans.SelectObject(&m_bmpMoonSunTrans);

	unsigned int *pData(&m_moonSunTrans.GetData()[0]);
	for (int x(0); x<MOON_SUN_TRANS_WIDTH; ++x)
	{
		pData[x] = 0;
		pData[x + MOON_SUN_TRANS_WIDTH * (MOON_SUN_TRANS_HEIGHT - 1)] = 0;
	}

	// render sun/moon transition as overlay into DIB
	for (int x(0); x<MOON_SUN_TRANS_WIDTH; ++x)
	{
		// render gradient
		{
			ColorF sun(1, 0.9f, 0.4f, 1);
			ColorF moon(0.4f, 0.4f, 1, 1);
			ColorF black(0, 0, 0, 1);

			ColorF col;

			float f(2.0f * x / (float) MOON_SUN_TRANS_WIDTH);
			if (f >= 1.0f)
			{
				f -= 1.0f;

				float t((float) m_sldDuskTime / (float) (12 * 60));
				float d((float) m_sldDuskDuration / (float) (12 * 60));

				float m(t + d * 0.5f);
				float s(t - d * 0.5f);

				if (f < s)
					col = sun;
				else if (f >= m)
					col = moon;
				else
				{
					assert(s < m);
					float b(0.5f * (s + m));
					if (f < b)
						col.lerpFloat(sun, black, (f-s) / (b-s));
					else
						col.lerpFloat(black, moon, (f-b) / (m-b));
				}
			}
			else
			{
				float t((float) m_sldDawnTime / (float) (12 * 60));
				float d((float) m_sldDawnDuration / (float) (12 * 60));

				float s(t + d * 0.5f);
				float m(t - d * 0.5f);

				if (f < m)
					col = moon;
				else if (f >= s)
					col = sun;
				else
				{
					assert(s > m);
					float b(0.5f * (s + m));
					if (f < b)
						col.lerpFloat(moon, black, (f-m) / (b-m));
					else
						col.lerpFloat(black, sun, (f-b) / (s-b));
				}
			}

			ColorB _col(col);
			unsigned int finalCol((_col.r << 16) | (_col.g << 8) | _col.b);

			unsigned int *pData(&m_moonSunTrans.GetData()[x + MOON_SUN_TRANS_WIDTH]);
			for (int y(1); y<MOON_SUN_TRANS_HEIGHT-1; ++y)
			{
				*pData = finalCol;
				pData += MOON_SUN_TRANS_WIDTH;
			}
		}
	}

	// put m_moonSunTrans into m_bmpMoonSunTrans
	{
		BITMAPINFO bi;

		ZeroStruct(bi);
		bi.bmiHeader.biSize = sizeof(bi);
		bi.bmiHeader.biWidth = m_moonSunTrans.GetWidth();
		bi.bmiHeader.biHeight = -m_moonSunTrans.GetHeight();
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biCompression = BI_RGB;

		SetDIBits(m_dcMoonSunTrans.m_hDC, (HBITMAP) m_bmpMoonSunTrans.GetSafeHandle(), 0, m_moonSunTrans.GetHeight(), m_moonSunTrans.GetData(), &bi, false);
	}

	// Update the preview
	if (drawOntoDialog)
	{
		RECT rcUpdate;
		::SetRect(&rcUpdate, 315, 245-3, 315 + MOON_SUN_TRANS_WIDTH, 245+3 + MOON_SUN_TRANS_HEIGHT);
		InvalidateRect(&rcUpdate, FALSE);
		UpdateWindow();
	}

	EndWaitCursor();
}

BOOL CTerrainLighting::OnInitDialog() 
{
	////////////////////////////////////////////////////////////////////////
	// Initialize the dialog with the settings from the document
	////////////////////////////////////////////////////////////////////////
	m_originalLightSettings = *GetIEditor()->GetDocument()->GetLighting();

	CSliderCtrl ctrlSlider;

	CLogFile::WriteLine("Loading lighting dialog...");

	CDialog::OnInitDialog();
	
	// Set the ranges for the slider controls
	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_SUN_DIRECTION)->m_hWnd));
	ctrlSlider.SetRange(0, 360, TRUE);
	ctrlSlider.Detach();
	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_SUNMAPLONGITUDE)->m_hWnd));
	ctrlSlider.SetRange(0, 180, TRUE);
	ctrlSlider.Detach();
	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_HEMISAMPLEQ)->m_hWnd));
	ctrlSlider.SetRange(0, 10, TRUE);
	ctrlSlider.Detach();
//	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_SKYBRIGHTENING)->m_hWnd));
//	ctrlSlider.SetRange(0, 100, TRUE);
//	ctrlSlider.Detach();
	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_DAWN_SLIDER)->m_hWnd));
	ctrlSlider.SetRange(0, 12 * 60, TRUE);
	ctrlSlider.Detach();
	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_DAWN_DUR_SLIDER)->m_hWnd));
	ctrlSlider.SetRange(0, 60, TRUE);
	ctrlSlider.Detach();
	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_DUSK_SLIDER)->m_hWnd));
	ctrlSlider.SetRange(0, 12 * 60, TRUE);
	ctrlSlider.Detach();
	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_DUSK_DUR_SLIDER)->m_hWnd));
	ctrlSlider.SetRange(0, 60, TRUE);
	ctrlSlider.Detach();

	m_lightmap.Allocate( LIGHTING_PREVIEW_RESOLUTION,LIGHTING_PREVIEW_RESOLUTION );
	m_moonSunTrans.Allocate(MOON_SUN_TRANS_WIDTH, MOON_SUN_TRANS_HEIGHT);

	// Synchronize the controls with the values from the document
	UpdateControls();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CTerrainLighting::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	////////////////////////////////////////////////////////////////////////
	// Update the document with the values from the sliders
	////////////////////////////////////////////////////////////////////////

	UpdateData(TRUE);

	LightingSettings *ls = GetIEditor()->GetDocument()->GetLighting();

	const uint32 scrollID = pScrollBar->GetDlgCtrlID();

	ls->iSunRotation = m_sldSunDirection;
	ls->iILApplySS = m_sldILApplySS;
	ls->iHemiSamplQuality = m_sldSkyQuality;
//	ls->iSkyBrightening = m_sldSkyBrightening;
	ls->iLongitude = m_sldLongitude;

	ls->iDawnTime = m_sldDawnTime;
	ls->iDawnDuration = m_sldDawnDuration;
	ls->iDuskTime = m_sldDuskTime;
	ls->iDuskDuration = m_sldDuskDuration;

	// We modified the document
	GetIEditor()->SetModifiedFlag();

	// Update the preview
	switch(scrollID)
	{
	case IDC_DAWN_SLIDER:
	case IDC_DAWN_DUR_SLIDER:
	case IDC_DUSK_SLIDER:
	case IDC_DUSK_DUR_SLIDER:
		{
			GenerateMoonSunTransition();
			UpdateMoonSunTransitionLabels();
			break;
		}
	default:
		{
			GenerateLightmap();
			break;
		}
	}

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}



void CTerrainLighting::OnUpdateIL() 
{
	////////////////////////////////////////////////////////////////////////
	// Synchronize value in the document
	////////////////////////////////////////////////////////////////////////

	UpdateData(TRUE);

	// We modified the document
	GetIEditor()->SetModifiedFlag();

	CScrollBar *pScrollBar = (CScrollBar*)GetDlgItem(IDC_HEMISAMPLEQ);
	pScrollBar->EnableWindow(true);
}

void CTerrainLighting::OnApplyILSS()
{
	////////////////////////////////////////////////////////////////////////
	// Synchronize value in the document
	////////////////////////////////////////////////////////////////////////

	UpdateData(TRUE);

	GetIEditor()->GetDocument()->GetLighting()->iILApplySS = m_sldILApplySS;

	// We modified the document
	GetIEditor()->SetModifiedFlag();
}

void CTerrainLighting::OnFileExport() 
{
	////////////////////////////////////////////////////////////////////////
	// Export the lighting settings
	////////////////////////////////////////////////////////////////////////
	
	char szFilters[] = "Light Settings (*.lgt)|*.lgt||";
	CString fileName;
	if (CFileUtil::SelectSaveFile( szFilters,"lgt",GetIEditor()->GetLevelFolder(),fileName ))
	{
		CLogFile::WriteLine("Exporting light settings...");

		// Write the light settings into the archive
		XmlNodeRef node = CreateXmlNode( "LightSettings" );
		GetIEditor()->GetDocument()->GetLighting()->Serialize( node,false );
		SaveXmlNode( node,fileName );
	}
}

void CTerrainLighting::OnFileImport() 
{
	////////////////////////////////////////////////////////////////////////
	// Import the lighting settings
	////////////////////////////////////////////////////////////////////////
	
	char szFilters[] = "Light Settings (*.lgt)|*.lgt||";
	CString fileName;

	if (CFileUtil::SelectFile( szFilters,GetIEditor()->GetLevelFolder(),fileName ))
	{
		CLogFile::WriteLine("Importing light settings...");

		XmlParser parser;
		XmlNodeRef node = parser.parse( fileName );
		GetIEditor()->GetDocument()->GetLighting()->Serialize( node,true );

		// We modified the document
		GetIEditor()->SetModifiedFlag();

		// Update the controls with the settings from the document
		UpdateControls();

		// Update the preview
		GenerateLightmap();
		GenerateMoonSunTransition();
	}
}

void CTerrainLighting::UpdateControls()
{
	////////////////////////////////////////////////////////////////////////	
	// Update the controls with the settings from the document
	////////////////////////////////////////////////////////////////////////

	LightingSettings *ls = GetIEditor()->GetDocument()->GetLighting();
//	m_bTerrainShadows = ls->bTerrainShadows;
	m_sldSunDirection = ls->iSunRotation;
	m_sldILApplySS = ls->iILApplySS;
	m_sldSkyQuality = ls->iHemiSamplQuality;
//	m_sldSkyBrightening = ls->iSkyBrightening;
	m_sldLongitude = ls->iLongitude;

	m_sldDawnTime = clamp_tpl(ls->iDawnTime, 0, 12 * 60);
	m_sldDawnDuration = clamp_tpl(ls->iDawnDuration, 0, 60);
	m_sldDuskTime = clamp_tpl(ls->iDuskTime, 0, 12 * 60);
	m_sldDuskDuration = clamp_tpl(ls->iDuskDuration, 0, 60);	

	switch (ls->eAlgo)
	{
		case ePrecise:
			m_optLightingAlgo = 0;
			break;

		case eDynamicSun:
			m_optLightingAlgo = 1;
			break;

		default: assert(0);
	}
	
	UpdateData(FALSE);

	UpdateMoonSunTransitionLabels();
}


void CTerrainLighting::OnDynamicSun() 
{
	GetIEditor()->GetDocument()->GetLighting()->eAlgo = eDynamicSun;
	GetIEditor()->SetModifiedFlag();
	UpdateControls();
	GenerateLightmap();
}


void CTerrainLighting::OnBnClickedPrecise()
{
	GetIEditor()->GetDocument()->GetLighting()->eAlgo = ePrecise;
	GetIEditor()->SetModifiedFlag();
	UpdateControls();
	GenerateLightmap();
}


BOOL CTerrainLighting::DestroyWindow() 
{
	return CDialog::DestroyWindow();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLighting::OnMouseMove(UINT nFlags, CPoint point)
{
	CDialog::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLighting::OnLButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainLighting::OnLButtonUp(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonUp(nFlags, point);
}


void CTerrainLighting::OnOK()
{
	CDialog::OnOK();

	CWaitCursor wait;
	// When exiting lighting dialog.
	GetIEditor()->GetGameEngine()->ReloadEnvironment();
}

void CTerrainLighting::OnCancel()
{
	LightingSettings *ls = GetIEditor()->GetDocument()->GetLighting();
	*ls = m_originalLightSettings;
	CDialog::OnCancel();
}

void CTerrainLighting::UpdateMoonSunTransitionLabels()
{
	char text[32]; text[sizeof(text)-1] = '\0';	
	{
		_snprintf(text, sizeof(text) - 1,"%.2d:%.2d", m_sldDawnTime / 60, m_sldDawnTime % 60);
		GetDlgItem(IDC_DAWN_INFO)->SetWindowText(text);
	}
	{
		_snprintf(text, sizeof(text) - 1,"%.2d", m_sldDawnDuration);
		GetDlgItem(IDC_DAWN_DUR_INFO)->SetWindowText(text);
	}
	{
		_snprintf(text, sizeof(text) - 1,"%.2d:%.2d", 12 + (m_sldDuskTime / 60), m_sldDuskTime % 60);
		GetDlgItem(IDC_DUSK_INFO)->SetWindowText(text);
	}
	{
		_snprintf(text, sizeof(text) - 1,"%.2d", m_sldDuskDuration);
		GetDlgItem(IDC_DUSK_DUR_INFO)->SetWindowText(text);
	}
}