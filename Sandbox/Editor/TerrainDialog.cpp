// TerrainDialog.cpp : implementation file
//

#include "stdafx.h"
#include "TerrainDialog.h"
#include "DimensionsDialog.h"
#include "CryEditDoc.h"
#include "NumberDlg.h"
#include "GenerationParam.h"
#include "Noise.h"

#include "SizeDialog.h"
#include "TerrainLighting.h"
#include "TerrainTexture.h"
#include "ViewManager.h"

#include "TerrainModifyTool.h"
#include "TerrainModifyPanel.h"
#include "VegetationMap.h"


#define IDW_ROLLUP_PANE      AFX_IDW_CONTROLBAR_FIRST+10

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainDialog,CBaseFrameWnd)

//////////////////////////////////////////////////////////////////////////
class CTerrainEditorViewClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {CB15B296-6829-459d-BA26-A5C0BFE0B8F5}
		static const GUID guid = { 0xcb15b296, 0x6829, 0x459d, { 0xba, 0x26, 0xa5, 0xc0, 0xbf, 0xe0, 0xb8, 0xf5 } };
		return guid;
	}
	virtual const char* ClassName() { return _T("Terrain Editor"); };
	virtual const char* Category() { return _T("Terrain Editor"); };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CTerrainDialog); };
	virtual const char* GetPaneTitle() { return _T("Terrain Editor"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(100,100,1000,800); };
	virtual bool SinglePane() { return false; };
	virtual bool WantIdleUpdate() { return true; };
};
REGISTER_CLASS_DESC(CTerrainEditorViewClass)

/////////////////////////////////////////////////////////////////////////////
// CTerrainDialog dialog
CTerrainDialog::CTerrainDialog()
{
	// We don't have valid recent terrain generation parameters yet
	m_sLastParam = new SNoiseParams;
	m_sLastParam->bValid = false;
	m_pViewport = 0;

	m_heightmap = GetIEditor()->GetHeightmap();

	CRect rc(0,0,0,0);
	Create( WS_CHILD|WS_VISIBLE,rc,AfxGetMainWnd() );
}

CTerrainDialog::~CTerrainDialog()
{
	GetIEditor()->SetEditTool( 0 );
	delete m_sLastParam;
}

BEGIN_MESSAGE_MAP(CTerrainDialog, CBaseFrameWnd)
	ON_MESSAGE(WM_KICKIDLE,OnKickIdle)
	ON_COMMAND(ID_TERRAIN_LOAD, OnTerrainLoad)
	ON_COMMAND(ID_TERRAIN_ERASE, OnTerrainErase)
	ON_COMMAND(ID_BRUSH_1, OnBrush1)
	ON_COMMAND(ID_BRUSH_2, OnBrush2)
	ON_COMMAND(ID_BRUSH_3, OnBrush3)
	ON_COMMAND(ID_BRUSH_4, OnBrush4)
	ON_COMMAND(ID_BRUSH_5, OnBrush5)
	ON_COMMAND(ID_TERRAIN_RESIZE, OnTerrainResize)
	ON_COMMAND(ID_TERRAIN_LIGHT, OnTerrainLight)
	ON_COMMAND(ID_TERRAIN_SURFACE, OnTerrainSurface)
	ON_COMMAND(ID_TERRAIN_GENERATE, OnTerrainGenerate)
	ON_COMMAND(ID_TERRAIN_INVERT, OnTerrainInvert)
	ON_COMMAND(ID_FILE_EXPORTHEIGHTMAP, OnExportHeightmap)
	ON_COMMAND(ID_MODIFY_MAKEISLE, OnModifyMakeisle)
	ON_COMMAND(ID_MODIFY_FLATTEN_LIGHT, OnModifyFlattenLight)
	ON_COMMAND(ID_MODIFY_FLATTEN_HEAVY, OnModifyFlattenHeavy)
	ON_COMMAND(ID_MODIFY_SMOOTH, OnModifySmooth)
	ON_COMMAND(ID_MODIFY_REMOVEWATER, OnModifyRemovewater)
	ON_COMMAND(ID_MODIFY_SMOOTHSLOPE, OnModifySmoothSlope)
	ON_COMMAND(ID_HEIGHTMAP_SHOWLARGEPREVIEW, OnHeightmapShowLargePreview)
	ON_COMMAND(ID_MODIFY_SMOOTHBEACHESCOAST, OnModifySmoothBeachesOrCoast)
	ON_COMMAND(ID_MODIFY_NOISE, OnModifyNoise)
	ON_COMMAND(ID_MODIFY_NORMALIZE, OnModifyNormalize)
	ON_COMMAND(ID_MODIFY_REDUCERANGE, OnModifyReduceRange)
	ON_COMMAND(ID_MODIFY_REDUCERANGELIGHT, OnModifyReduceRangeLight)
	ON_COMMAND(ID_MODIFY_RANDOMIZE, OnModifyRandomize)
	ON_COMMAND(ID_LOW_OPACITY, OnLowOpacity)
	ON_COMMAND(ID_MEDIUM_OPACITY, OnMediumOpacity)
	ON_COMMAND(ID_HIGH_OPACITY, OnHighOpacity)
	ON_WM_MOUSEWHEEL()
	ON_COMMAND(ID_HOLD, OnHold)
	ON_COMMAND(ID_FETCH, OnFetch)
	ON_COMMAND(ID_OPTIONS_SHOWMAPOBJECTS, OnOptionsShowMapObjects)
	ON_COMMAND(ID_OPTIONS_SHOWWATER, OnOptionsShowWater)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWWATER,OnShowWaterUpdateUI)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS_SHOWMAPOBJECTS,OnShowMapObjectsUpdateUI)
	ON_COMMAND(ID_TOOLS_EXPORTTERRAINASGEOMETRIE, OnExportTerrainAsGeometrie)
	ON_COMMAND(ID_SETWATERLEVEL, OnSetWaterLevel)
	ON_COMMAND(ID_MODIFY_SETMAXHEIGHT, OnSetMaxHeight)
	ON_COMMAND(ID_MODIFY_SETUNITSIZE, OnSetUnitSize)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainDialog message handlers

BOOL CTerrainDialog::OnInitDialog() 
{
	// Create and setup the heightmap edit viewport and the toolbars
	GetCommandBars()->GetCommandBarsOptions()->bShowExpandButtonAlways = FALSE;
	GetCommandBars()->EnableCustomization(FALSE);

	CRect rcClient;
	GetClientRect(rcClient);

	CXTPCommandBar* pMenuBar = GetCommandBars()->SetMenu( _T("Menu Bar"),IDR_TERRAIN );	
	pMenuBar->SetFlags(xtpFlagStretched);
	pMenuBar->EnableCustomization(FALSE);

	// Create Library toolbar.
	CXTPToolBar *pToolBar1 = GetCommandBars()->Add( _T("ToolBar1"),xtpBarTop );
	pToolBar1->EnableCustomization(FALSE);
	VERIFY(pToolBar1->LoadToolBar( IDR_TERRAIN ));

	CXTPToolBar *pToolBar2 = GetCommandBars()->Add( _T("ToolBar2"),xtpBarTop );
	pToolBar2->EnableCustomization(FALSE);
	VERIFY(pToolBar2->LoadToolBar( IDR_BRUSHES ));

	DockRightOf(pToolBar2,pToolBar1);

	// Create the status bar.
	{
		UINT indicators[] =
		{
			ID_SEPARATOR,           // status line indicator
		};
		VERIFY( m_wndStatusBar.Create( this, WS_CHILD|WS_VISIBLE|CBRS_BOTTOM) );
		VERIFY( m_wndStatusBar.SetIndicators( indicators,sizeof(indicators)/sizeof(UINT) ) );
	}


	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for TaskPanel
	m_pDockPane_Rollup = GetDockingPaneManager()->CreatePane( IDW_ROLLUP_PANE, CRect(0,0,300,500), dockRightOf );
	//m_pDockPane_Rollup->SetOptions(xtpPaneNoCloseable|xtpPaneNoFloatable);


	m_rollupCtrl.Create( WS_CHILD|WS_VISIBLE,CRect(0,0,100,100),this,2 );

	m_pViewport = new CTopRendererWnd;
	m_pViewport->SetDlgCtrlID(AFX_IDW_PANE_FIRST);
	m_pViewport->MoveWindow( CRect(20,50,500,500) );
	m_pViewport->ModifyStyle( WS_POPUP,WS_CHILD,0 );
	m_pViewport->SetParent( this );
	m_pViewport->SetOwner( this );
	m_pViewport->m_bShowHeightmap = true;
	m_pViewport->ShowWindow( SW_SHOW );
	m_pViewport->SetShowWater( true );
	m_pViewport->SetShowViewMarker( false );

	m_pTerainTool = new CTerrainModifyTool;
	CTerrainModifyPanel *pToolPanel = new CTerrainModifyPanel(m_pTerainTool,&m_rollupCtrl);
	m_pTerainTool->SetExternalUIPanel( pToolPanel );

	m_pViewport->SetEditTool( m_pTerainTool,true );
	GetIEditor()->SetEditTool( m_pTerainTool );

	CTerrainBrush br;
	m_pTerainTool->GetBrush(br);
	br.height = 100;
	br.radiusInside = 10;
	br.radius = 10;
	m_pTerainTool->SetBrush(br,true);

	m_rollupCtrl.InsertPage( "Terrain Brush",pToolPanel );

	//////////////////////////////////////////////////////////////////////////
	char szCaption[128];
	sprintf(szCaption, "Heightmap %ix%i",	m_heightmap->GetWidth(), m_heightmap->GetHeight());
	m_wndStatusBar.SetPaneText(0, szCaption);

	AutoLoadFrameLayout( "TerrainEditor" );

	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
LRESULT CTerrainDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;    
		if (!pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_ROLLUP_PANE:
				pwndDockWindow->Attach(&m_rollupCtrl);
				break;
			default:
				return FALSE;
			}
		}
		return TRUE;
	}
	else if (wParam == XTP_DPN_CLOSEPANE)
	{
		// get a pointer to the docking pane being closed.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (pwndDockWindow->IsValid())
		{
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
CTerrainModifyTool* CTerrainDialog::GetTerrainTool()
{
	return m_pTerainTool;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CTerrainDialog::OnKickIdle(WPARAM wParam, LPARAM lParam)
{
	if (m_pViewport)
		m_pViewport->Update();
	
	return FALSE;
}

void CTerrainDialog::OnTerrainLoad() 
{
	////////////////////////////////////////////////////////////////////////
	// Load a heightmap from a BMP file
	////////////////////////////////////////////////////////////////////////
	
	char szFilters[] = "All Images Files|*.bmp;*.pgm;*.raw|8-bit Bitmap Files (*.bmp)|*.bmp|16-bit PGM Files (*.pgm)|*.pgm|16-bit RAW Files (*.raw)|*.raw|All files (*.*)|*.*||";
	CAutoDirectoryRestoreFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR, szFilters);

	if (dlg.DoModal() == IDOK) 
	{
		char ext[_MAX_EXT];
		_splitpath( dlg.GetPathName(),NULL,NULL,NULL,ext );
		
		CWaitCursor wait;

		if (stricmp(ext,".pgm") == 0)
		{
			m_heightmap->LoadPGM( dlg.GetPathName() );
		}
		else if (stricmp(ext,".raw") == 0)
		{
			m_heightmap->LoadRAW( dlg.GetPathName() );
		}
		else
		{
			// Load the heightmap
			m_heightmap->LoadBMP(dlg.GetPathName());
		}

		InvalidateTerrain();
	}
}

void CTerrainDialog::OnTerrainErase() 
{
	////////////////////////////////////////////////////////////////////////
	// Erase the heightmap
	////////////////////////////////////////////////////////////////////////

	// Ask first
	if (AfxMessageBox("Really erase the heightmap ?", MB_ICONQUESTION | MB_YESNO, NULL) != IDYES)
		return;

	// Erase it
	m_heightmap->Clear();

	InvalidateTerrain();

	// All layers need to be generated from scratch
	GetIEditor()->GetDocument()->InvalidateLayers();
}

void CTerrainDialog::OnTerrainResize() 
{
	////////////////////////////////////////////////////////////////////////
	// Query a new terrain size from the user and set it
	////////////////////////////////////////////////////////////////////////

	CDimensionsDialog cDialog;
	
	// Set the current size
	cDialog.SetDimensions(m_heightmap->GetWidth());
	
	// Show the dialog
	if (cDialog.DoModal() != IDOK)
		return;

	CWaitCursor wait;

	// Set the new size
	m_heightmap->Resize(cDialog.GetDimensions(), cDialog.GetDimensions(),m_heightmap->GetUnitSize() );

	InvalidateTerrain();
}

void CTerrainDialog::OnTerrainInvert() 
{
	////////////////////////////////////////////////////////////////////////
	// Invert the heightmap
	////////////////////////////////////////////////////////////////////////
	
	CWaitCursor wait;

	m_heightmap->Invert();

	InvalidateTerrain();
}

void CTerrainDialog::OnTerrainGenerate() 
{
	////////////////////////////////////////////////////////////////////////
	// Generate a terrain
	////////////////////////////////////////////////////////////////////////

	SNoiseParams sParam;
	CGenerationParam cDialog;
	
	if (GetLastParam()->bValid)
	{
		// Use last parameters
		cDialog.LoadParam(GetLastParam());
	}
	else
	{
		// Set default parameters for the dialog
		cDialog.m_sldCover = 0;
		cDialog.m_sldFade = (int) (0.46f * 10);
		cDialog.m_sldFrequency = (int) (7.0f * 10);
		cDialog.m_sldFrequencyStep = (int) (2.0f * 10);
		cDialog.m_sldPasses = 8;
		cDialog.m_sldRandomBase = 1;
		cDialog.m_sldSharpness = (int) (0.999f * 1000);
		cDialog.m_sldBlur = 0;
	}
				
	// Show the generation parameter dialog
	if (cDialog.DoModal() == IDCANCEL)
		return;

	CLogFile::WriteLine("Generating new terrain...");

	// Fill the parameter structure for the terrain generation
	cDialog.FillParam(&sParam);
	sParam.iWidth = m_heightmap->GetWidth();
	sParam.iHeight = m_heightmap->GetHeight();
	sParam.bBlueSky = false;

	// Save the paramters
	ZeroStruct( *m_sLastParam );

	CWaitCursor wait;
	// Generate
	m_heightmap->GenerateTerrain(sParam);

	InvalidateTerrain();
}

void CTerrainDialog::OnExportHeightmap()
{
	////////////////////////////////////////////////////////////////////////
	// Export the heightmap to BMP
	////////////////////////////////////////////////////////////////////////

	char szFilters[] = "8-bit Bitmap (*.bmp)|*.bmp|16-bit PGM (*.pgm)|*.pgm|16-bit RAW (*.raw)|*.raw|32-bit RAW (*.fraw)|*.fraw||";
	CAutoDirectoryRestoreFileDialog dlg(FALSE, "bmp", NULL, OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR, szFilters);
	
	// Show the dialog
	if (dlg.DoModal() == IDOK) 
	{
		BeginWaitCursor();

		CLogFile::WriteLine("Exporting heightmap...");


		char ext[_MAX_EXT];
		_splitpath( dlg.GetPathName(),NULL,NULL,NULL,ext );
		if (stricmp(ext,".pgm") == 0)
		{
			// PGM
			m_heightmap->SavePGM( dlg.GetPathName() );
		}
		else if (stricmp(ext,".raw") == 0)
		{
			// PGM
			m_heightmap->SaveRAW( dlg.GetPathName() );
		}
		else if (stricmp(ext,".fraw") == 0)
		{
			// PGM
			m_heightmap->SaveFRAW( dlg.GetPathName() );
		}
		else
		{
			// BMP or others
			m_heightmap->SaveImage( dlg.GetPathName() );
		}
		
		EndWaitCursor();
	}
}

void CTerrainDialog::OnModifySmoothBeachesOrCoast() 
{
	////////////////////////////////////////////////////////////////////////
	// Make smooth beaches or a smooth coast
	////////////////////////////////////////////////////////////////////////

	CWaitCursor wait;

	// Call the smooth beaches function of the heightmap class
	m_heightmap->MakeBeaches();
	InvalidateTerrain();
}

void CTerrainDialog::OnModifyMakeisle() 
{
	////////////////////////////////////////////////////////////////////////
	// Convert the heightmap to an island
	////////////////////////////////////////////////////////////////////////
	CWaitCursor wait;

	// Call the make isle fucntion of the heightmap class
	m_heightmap->MakeIsle();

	InvalidateTerrain();
}

void CTerrainDialog::Flatten(float fFactor)
{
	////////////////////////////////////////////////////////////////////////
	// Increase the number of flat areas on the heightmap
	////////////////////////////////////////////////////////////////////////

	CWaitCursor wait;

	// Call the flatten function of the heigtmap class
	m_heightmap->Flatten(fFactor);

	InvalidateTerrain();
}

void CTerrainDialog::OnModifyFlattenLight() 
{
	Flatten(0.75f);
}

void CTerrainDialog::OnModifyFlattenHeavy() 
{
	Flatten(0.5f);
}

void CTerrainDialog::OnModifyRemovewater() 
{
	//////////////////////////////////////////////////////////////////////
	// Remove all water areas from the heightmap
	//////////////////////////////////////////////////////////////////////

	CLogFile::WriteLine("Removing water areas from heightmap...");

	CWaitCursor wait;

	// Remove the water
	m_heightmap->RemoveWater();

	InvalidateTerrain();
}

void CTerrainDialog::OnModifySmoothSlope() 
{
	//////////////////////////////////////////////////////////////////////
	// Remove areas with high slope from the heightmap
	//////////////////////////////////////////////////////////////////////

	CWaitCursor wait;

	// Call the smooth slope function of the heightmap class
	m_heightmap->SmoothSlope();

	InvalidateTerrain();
}

void CTerrainDialog::OnModifySmooth() 
{
	//////////////////////////////////////////////////////////////////////
	// Smooth the heightmap
	//////////////////////////////////////////////////////////////////////

	CWaitCursor wait;

	m_heightmap->Smooth();
	InvalidateTerrain();
}

void CTerrainDialog::OnModifyNoise() 
{
	////////////////////////////////////////////////////////////////////////
	// Noise the heightmap
	////////////////////////////////////////////////////////////////////////
	
	CWaitCursor wait;
	m_heightmap->Noise();
	InvalidateTerrain();
}

void CTerrainDialog::OnModifyNormalize() 
{
	////////////////////////////////////////////////////////////////////////
	// Normalize the heightmap
	////////////////////////////////////////////////////////////////////////
	
	CWaitCursor wait;
	m_heightmap->Normalize();

	InvalidateTerrain();
}

void CTerrainDialog::OnModifyReduceRange() 
{
	////////////////////////////////////////////////////////////////////////
	// Reduce the value range of the heightmap (Heavy)
	////////////////////////////////////////////////////////////////////////
	
	CWaitCursor wait;
	m_heightmap->LowerRange(0.8f);

	InvalidateTerrain();
}

void CTerrainDialog::OnModifyReduceRangeLight() 
{
	////////////////////////////////////////////////////////////////////////
	// Reduce the value range of the heightmap (Light)
	////////////////////////////////////////////////////////////////////////
	
	CWaitCursor wait;
	m_heightmap->LowerRange(0.95f);

	InvalidateTerrain();
}

void CTerrainDialog::OnModifyRandomize() 
{
	////////////////////////////////////////////////////////////////////////
	// Add a small amount of random noise
	////////////////////////////////////////////////////////////////////////
	
	CWaitCursor wait;
	m_heightmap->Randomize();

	InvalidateTerrain();
}


void CTerrainDialog::OnHeightmapShowLargePreview() 
{
	////////////////////////////////////////////////////////////////////////
	// Show a full-size version of the heightmap
	////////////////////////////////////////////////////////////////////////

	DWORD *pImageData = NULL;
	unsigned int i, j;
	uint8 iColor;
	t_hmap *pHeightmap = NULL;

	BeginWaitCursor();

	CLogFile::WriteLine("Exporting heightmap...");

	CHeightmap *heightmap = GetIEditor()->GetHeightmap();
	UINT iWidth = heightmap->GetWidth();
	UINT iHeight = heightmap->GetHeight();

	CImage image;
	image.Allocate( heightmap->GetWidth(),heightmap->GetHeight() );
	// Allocate memory to export the heightmap
	pImageData = (DWORD*)image.GetData();

	// Get a pointer to the heightmap data
	pHeightmap = heightmap->GetData();

	// Write heightmap into the image data array
	for (j=0; j<iHeight; j++)
		for (i=0; i<iWidth; i++)
		{
			// Get a normalized grayscale value from the heigthmap
			iColor = (uint8)__min(pHeightmap[i + j * iWidth], 255.0f);

			// Create a BGR grayscale value and store it in the image
			// data array
			pImageData[i + j * iWidth] =
				(iColor << 16) | (iColor << 8) | iColor;
		}

	// Save the heightmap into the bitmap
	CFileUtil::CreateDirectory( "Temp" );
	bool bOk = CImageUtil::SaveImage( "Temp\\HeightmapPreview.bmp",image );

	EndWaitCursor();
	
	if (bOk)
	{
		CString dir = "Temp\\";
		// Show the heightmap
		::ShellExecute(::GetActiveWindow(), "open", "HeightmapPreview.bmp", 
			"", dir, SW_SHOWMAXIMIZED);
	}
}

BOOL CTerrainDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	////////////////////////////////////////////////////////////////////////
	// Forward mouse wheel messages to the drawing window
	////////////////////////////////////////////////////////////////////////

	return __super::OnMouseWheel(nFlags, zDelta, pt);
}

void CTerrainDialog::OnTerrainLight() 
{
	////////////////////////////////////////////////////////////////////////
	// Show the terrain lighting dialog
	////////////////////////////////////////////////////////////////////////

	CTerrainLighting cDialog;

	cDialog.DoModal();
}

void CTerrainDialog::OnTerrainSurface() 
{
	GetIEditor()->SetEditTool( 0 );

	////////////////////////////////////////////////////////////////////////
	// Show the terrain texture dialog
	////////////////////////////////////////////////////////////////////////
	
	GetIEditor()->OpenView( "Terrain Texture Layers" );
}

void CTerrainDialog::OnHold()
{
	// Hold the current heightmap state
	m_heightmap->Hold();
}

void CTerrainDialog::OnFetch()
{
	int iResult;

	// Did we modify the heigthmap ?
	if (GetIEditor()->IsModified())
	{
		// Ask first
		iResult = MessageBox("Do you really want to restore the previous heightmap state ?", 
			"Fetch", MB_YESNO | MB_ICONQUESTION);

		// Abort
		if (iResult == IDNO)
			return;
	}

	// Restore the old heightmap state
	m_heightmap->Fetch();

	// We modified the document
	GetIEditor()->SetModifiedFlag();

	// All layers need to be generated from scratch
	m_heightmap->InvalidateLayers();

	InvalidateTerrain();
}

/////////////////////////////////////////////////////////////////////////////
// Options
void CTerrainDialog::OnShowWaterUpdateUI( CCmdUI* pCmdUI )
{
	pCmdUI->SetCheck( m_pViewport->GetShowWater() ? 1 : 0 );
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnShowMapObjectsUpdateUI( CCmdUI* pCmdUI )
{
	pCmdUI->SetCheck( m_pViewport->m_bShowStatObjects ? 1 : 0 );
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnOptionsShowMapObjects() 
{
	m_pViewport->m_bShowStatObjects = !m_pViewport->m_bShowStatObjects;

	// Update the draw window
	InvalidateViewport();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::OnOptionsShowWater() 
{
	m_pViewport->SetShowWater( !m_pViewport->GetShowWater() );
	// Update the draw window
	InvalidateViewport();
}

/////////////////////////////////////////////////////////////////////////////
// Brushes

void CTerrainDialog::OnBrush1() 
{
	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		br.radiusInside = br.radius = 2;
		GetTerrainTool()->SetBrush(br,true);
	}
}

void CTerrainDialog::OnBrush2() 
{
	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		br.radiusInside = br.radius = 10;
		GetTerrainTool()->SetBrush(br,true);
	}
}

void CTerrainDialog::OnBrush3() 
{
	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		br.radiusInside = br.radius = 25;
		GetTerrainTool()->SetBrush(br,true);
	}
}

void CTerrainDialog::OnBrush4() 
{
	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		br.radiusInside = br.radius = 50;
		GetTerrainTool()->SetBrush(br,true);
	}
}

void CTerrainDialog::OnBrush5() 
{
	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		br.radiusInside = br.radius = 100;
		GetTerrainTool()->SetBrush(br,true);
	}
}

void CTerrainDialog::OnLowOpacity()
{
	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		br.hardness = 0.2f;
		GetTerrainTool()->SetBrush(br,true);
	}
}

void CTerrainDialog::OnMediumOpacity()
{
	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		br.hardness = 0.5f;
		GetTerrainTool()->SetBrush(br,true);
	}
}

void CTerrainDialog::OnHighOpacity()
{
	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		br.hardness = 1.0;
		GetTerrainTool()->SetBrush(br,true);
	}
}

void CTerrainDialog::OnExportTerrainAsGeometrie()
{
}

void CTerrainDialog::OnSetWaterLevel() 
{
	////////////////////////////////////////////////////////////////////////
	// Let the user change the current water level
	////////////////////////////////////////////////////////////////////////
	// Get the water level from the document and set it as default into
	// the dialog
	float waterLevel = GetIEditor()->GetHeightmap()->GetWaterLevel();
	CNumberDlg cDialog( this,waterLevel,"Set Water Height" );

	// Show the dialog
	if (cDialog.DoModal() == IDOK)
	{
		// Retrive the new water level from the dialog and save it in the document
		waterLevel = cDialog.GetValue();
		GetIEditor()->GetHeightmap()->SetWaterLevel(waterLevel);
		InvalidateTerrain();
	}
}

void CTerrainDialog::OnSetMaxHeight() 
{
	////////////////////////////////////////////////////////////////////////
	// Let the user change the current water level
	////////////////////////////////////////////////////////////////////////
	// Get the water level from the document and set it as default into
	// the dialog
	float fValue = GetIEditor()->GetHeightmap()->GetMaxHeight();
	CNumberDlg cDialog( this,fValue,"Set Max Terrain Height" );

	// Show the dialog
	if (cDialog.DoModal() == IDOK)
	{
		// Retrive the new water level from the dialog and save it in the document
		fValue = cDialog.GetValue();
		GetIEditor()->GetHeightmap()->SetMaxHeight(fValue);

		InvalidateTerrain();
	}
}

void CTerrainDialog::OnSetUnitSize() 
{
	////////////////////////////////////////////////////////////////////////
	// Let the user change the current water level
	////////////////////////////////////////////////////////////////////////
	// Get the water level from the document and set it as default into
	// the dialog
	float fValue = GetIEditor()->GetHeightmap()->GetUnitSize();
	CNumberDlg cDialog( this,fValue,"Set Unit Size (Meters per unit)" );
	cDialog.SetInteger(true);

	// Show the dialog
	if (cDialog.DoModal() == IDOK)
	{
		// Retrive the new water level from the dialog and save it in the document
		fValue = cDialog.GetValue();
		GetIEditor()->GetHeightmap()->SetUnitSize(fValue);

		InvalidateTerrain();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::InvalidateTerrain()
{
	GetIEditor()->SetModifiedFlag();
	GetIEditor()->GetHeightmap()->UpdateEngineTerrain( true );

	// All layers need to be generated from scratch
	GetIEditor()->GetHeightmap()->InvalidateLayers();

	if (GetTerrainTool())
	{
		CTerrainBrush br;
		GetTerrainTool()->GetBrush(br);
		if (br.bRepositionObjects)
		{
			BBox box;
			box.min = -Vec3(100000,100000,100000);
			box.max = Vec3(100000,100000,100000);
			if (GetIEditor()->GetVegetationMap())
				GetIEditor()->GetVegetationMap()->RepositionArea( box );
			// Make sure objects preserve height.
			GetIEditor()->GetObjectManager()->SendEvent( EVENT_KEEP_HEIGHT,box );
		}
	}

	InvalidateViewport();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainDialog::InvalidateViewport()
{
	m_pViewport->Invalidate();
	GetIEditor()->UpdateViews(eUpdateHeightmap);
}
