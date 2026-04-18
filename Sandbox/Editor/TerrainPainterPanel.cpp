////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terrainpainterpanel.cpp
//  Version:     v1.00
//  Created:     25/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "TerrainPainterPanel.h"
#include "TerrainTexturePainter.h"

#include "CryEditDoc.h"
#include "Layer.h"
#include ".\terrainpainterpanel.h"

/////////////////////////////////////////////////////////////////////////////
// CTerrainPainterPanel dialog


CTerrainPainterPanel::CTerrainPainterPanel(CTerrainTexturePainter *tool,CWnd* pParent /*=NULL*/)
	: CDialog(CTerrainPainterPanel::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTerrainPainterPanel)
	//}}AFX_DATA_INIT

	Create( IDD,pParent );

	assert( tool != 0 );
	m_tool = tool;
}

void CTerrainPainterPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainPainterPanel)
	DDX_Control(pDX, IDC_BRUSH_MASKLAYERID, m_MaskLayerId);
	DDX_Control(pDX, IDC_BRUSH_HARDNESS_SLIDER, m_hardnessSlider);
	DDX_Control(pDX, IDC_BRUSH_RADIUS_SLIDER, m_radiusSlider);
	DDX_Control(pDX, IDC_LAYERS, m_layers);
	DDX_Control(pDX, IDC_PAINT_DETAILLAYER, m_optDetailLayer);
	DDX_Control(pDX, IDC_PAINT_MASKBYLAYERSETTINGS, m_MaskLayerSettings);
	DDX_Control(pDX, IDC_TERRAINPAINTER_COLOR, m_SolidColor);
	DDX_Control(pDX, IDC_BRUSH_BRIGHTNESS_SLIDER, m_BrightnessSlider);
	DDX_Control(pDX, IDC_CHANGE, m_resizeResolution);
	//DDX_Control(pDX, IDC_EXPORT, m_export);
	//DDX_Control(pDX, IDC_IMPORT, m_import);
	//DDX_Control(pDX, IDC_FILE_BROWSE, m_fileBrowse);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTerrainPainterPanel, CDialog)
	//{{AFX_MSG_MAP(CTerrainPainterPanel)
	ON_EN_UPDATE(IDC_BRUSH_RADIUS, UpdateTextureBrushSettings)

	ON_LBN_SELCHANGE(IDC_LAYERS, SetLayerMaskSettingsToLayer)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_BRUSH_HARDNESS_SLIDER, OnSliderChange)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_BRUSH_BRIGHTNESS_SLIDER, OnSliderChange)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_BRUSH_RADIUS_SLIDER, OnSliderChange)
	ON_CBN_SELENDOK(IDC_BRUSH_MASKLAYERID, UpdateTextureBrushSettings)
	ON_EN_UPDATE(IDC_BRUSH_HARDNESS, UpdateTextureBrushSettings)
	ON_EN_UPDATE(IDC_NOISE_SCALE, UpdateTextureBrushSettings)
	ON_EN_UPDATE(IDC_NOISE_FREQ, UpdateTextureBrushSettings)
	ON_CPN_XT_SELENDOK(IDC_TERRAINPAINTER_COLOR, UpdateTextureBrushSettings)

	ON_BN_CLICKED(IDC_PAINT_DETAILLAYER, UpdateTextureBrushSettings)
	ON_BN_CLICKED(IDC_PAINT_MASKBYLAYERSETTINGS, UpdateTextureBrushSettings)
	ON_BN_CLICKED(IDC_TERRAINPAINTER_RESETBRIGHTNESS,OnBrushResetBrightness)

	ON_EN_UPDATE(IDC_BRUSH_LAYER_ALTMIN, GetLayerMaskSettingsFromLayer)
	ON_EN_UPDATE(IDC_BRUSH_LAYER_ALTMAX, GetLayerMaskSettingsFromLayer)
	ON_EN_UPDATE(IDC_BRUSH_LAYER_SLOPEMIN, GetLayerMaskSettingsFromLayer)
	ON_EN_UPDATE(IDC_BRUSH_LAYER_SLOPEMAX, GetLayerMaskSettingsFromLayer)

	//}}AFX_MSG_MAP

	ON_BN_CLICKED(IDC_BRUSH_SETTOLAYER, &CTerrainPainterPanel::OnBnClickedBrushSettolayer)
	ON_BN_CLICKED(IDC_CHANGE, OnChange)
	//ON_BN_CLICKED(IDC_EXPORT, OnExport)
	//ON_BN_CLICKED(IDC_IMPORT, OnImport)
	//ON_BN_CLICKED(IDC_FILE_BROWSE, OnFileBrowse)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainPainterPanel message handlers

BOOL CTerrainPainterPanel::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_brushRadius.Create( this,IDC_BRUSH_RADIUS );
	m_brushRadius.SetRange( 0,32 );
	
	m_brushHardness.Create( this,IDC_BRUSH_HARDNESS );
	m_brushHardness.SetRange( 0,1 );

	m_layerAltMin.Create( this,IDC_BRUSH_LAYER_ALTMIN );
	m_layerAltMin.SetRange( 0,1024 );
	m_layerAltMax.Create( this,IDC_BRUSH_LAYER_ALTMAX );
	m_layerAltMax.SetRange( 0,1024 );
	m_layerSlopeMin.Create( this,IDC_BRUSH_LAYER_SLOPEMIN );
	m_layerSlopeMin.SetRange( 0,90 );
	m_layerSlopeMax.Create( this,IDC_BRUSH_LAYER_SLOPEMAX );
	m_layerSlopeMax.SetRange( 0,90 );

	float fMax=1000;
	m_radiusSlider.SetRange( 1,100 * log10(fMax) );	

	m_hardnessSlider.SetRange( 0,100 );
	m_BrightnessSlider.SetRange( 0,512 );

	//m_SolidColor.ModifyCPStyle( 0,CPS_XT_EXTENDED|CPS_XT_MORECOLORS );
	//m_SolidColor.SetDefaultColor(RGB(255,255,255));
	m_SolidColor.SetColor(RGB(255,255,255));
	m_SolidColor.SetWindowText("");

	//GetDlgItem(IDC_FILE)->SetWindowText( gSettings.terrainTextureExport);

	// Fill layers.
	ReloadLayers();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::SetBrush( CTextureBrush &br )
{
	m_brushRadius.SetRange( br.minRadius,br.maxRadius );
	m_brushRadius.SetValue( br.radius );
	m_brushHardness.SetValue( br.hardness );
	m_optDetailLayer.SetCheck( (br.bDetailLayer)?BST_CHECKED:BST_UNCHECKED );
	m_MaskLayerSettings.SetCheck( (br.bMaskByLayerSettings)?BST_CHECKED:BST_UNCHECKED );
	m_SolidColor.SetColor(br.m_cFilterColor.pack_abgr8888()&0xffffff);

	m_radiusSlider.SetPos( log10(br.radius)*100.0f );
	m_BrightnessSlider.SetPos( br.m_fBrightness*255.0f );
	m_hardnessSlider.SetPos( br.hardness*100.0f );

	m_optDetailLayer.EnableWindow(br.m_dwMaskLayerId==0xffffffff);
	if(br.m_dwMaskLayerId!=0xffffffff)
		m_optDetailLayer.SetCheck(false);
/*
	CTextureBrush br;
	m_tool->GetBrush(br);
	br.m_cFilterColor=pLayer->m_cLayerFilterColor;
	br.m_fBrightness=pLayer->m_fLayerBrightness;
	m_tool->SetBrush(br);
*/
}


//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::OnSliderChange(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CTextureBrush br;
	m_tool->GetBrush(br);
	br.hardness = (float)m_hardnessSlider.GetPos()/100.0f;
	br.m_fBrightness = m_BrightnessSlider.GetPos()/255.0f;
	br.radius =  pow(10.0f,m_radiusSlider.GetPos()/100.0f);
	SetBrush( br );
	m_tool->SetBrush(br);

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::SelectLayer( const CLayer *pLayer )
{
	CCryEditDoc *pDoc = GetIEditor()->GetDocument();

	for (int i = 0; i < pDoc->GetLayerCount(); i++)
	{
		CLayer *pLayerIt = pDoc->GetLayer(i);

		pLayerIt->SetSelected(pLayerIt==pLayer);
	}

	ReloadLayers();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::ReloadLayers()
{
	CLayer *pSelectedLayer=0;
	m_layers.ResetContent();
	m_MaskLayerId.ResetContent();
	m_MaskLayerId.AddString("<none>");
	CCryEditDoc *pDoc = GetIEditor()->GetDocument();
	for (int i = 0; i < pDoc->GetLayerCount(); i++)
	{
		CLayer *pLayer = pDoc->GetLayer(i);

		if (pLayer->IsSelected())
			pSelectedLayer=pLayer;

		m_layers.AddString( pLayer->GetLayerName() );
		m_MaskLayerId.AddString( pLayer->GetLayerName() );
	}
	m_MaskLayerId.SelectString( -1,"<none>" );

	CTextureBrush br;
	m_tool->GetBrush(br);
	br.m_dwMaskLayerId=0xffffffff;
	m_tool->SetBrush(br);
	m_layers.SelectString( -1,pSelectedLayer?pSelectedLayer->GetLayerName():"" );

	SetLayerMaskSettingsToLayer();
}

//////////////////////////////////////////////////////////////////////////
CString CTerrainPainterPanel::GetSelectedLayer()
{
	int curSel = m_layers.GetCurSel();
	if (curSel < 0)
		return "";
	CString selStr;
	m_layers.GetText(curSel,selStr );
	return selStr;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::OnBrushResetBrightness()
{
	m_BrightnessSlider.SetPos(255);

	CTextureBrush br;
	m_tool->GetBrush(br);
	br.m_fBrightness = m_BrightnessSlider.GetPos()/255.0f;
	SetBrush( br );
	m_tool->SetBrush(br);
}


//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::UpdateTextureBrushSettings()
{
	CTextureBrush br;
	m_tool->GetBrush(br);

	br.bDetailLayer = m_optDetailLayer.GetCheck();
	br.bMaskByLayerSettings = m_MaskLayerSettings.GetCheck();
	br.radius = m_brushRadius.GetValue();
	br.hardness = m_brushHardness.GetValue();
	br.m_cFilterColor = ColorF((unsigned int)m_SolidColor.GetColor());

	br.m_dwMaskLayerId=0xffffffff;

	{
		int sel = m_MaskLayerId.GetCurSel();
		if (sel != LB_ERR)
		{
			CString selStr;
			m_MaskLayerId.GetLBText(sel,selStr);
			
			CLayer *pLayer = GetIEditor()->GetDocument()->FindLayer(selStr);

			if(pLayer)
			{
				br.m_dwMaskLayerId=pLayer->GetOrRequestLayerId();
//				pLayer->m_cLayerFilterColor=br.m_cFilterColor;
//				pLayer->m_fLayerBrightness=br.m_fBrightness;
			}
		}
	}
	
	SetBrush( br );

	m_tool->SetBrush(br);
}



void CTerrainPainterPanel::SetLayerMaskSettingsToLayer()
{
	CLayer *pLayer = GetIEditor()->GetDocument()->FindLayer(GetSelectedLayer());

	m_layerAltMin.EnableWindow(pLayer!=0);
	m_layerAltMax.EnableWindow(pLayer!=0);
	m_layerSlopeMin.EnableWindow(pLayer!=0);
	m_layerSlopeMax.EnableWindow(pLayer!=0);

	if(pLayer)
	{
		m_layerAltMin.SetValue( pLayer->GetLayerStart() );
		m_layerAltMax.SetValue( pLayer->GetLayerEnd() );
		m_layerSlopeMin.SetValue( pLayer->GetLayerMinSlopeAngle() );
		m_layerSlopeMax.SetValue( pLayer->GetLayerMaxSlopeAngle() );

		CTextureBrush br;
		m_tool->GetBrush(br);
		br.m_cFilterColor=pLayer->m_cLayerFilterColor;
		br.m_fBrightness=pLayer->m_fLayerBrightness;
		m_tool->SetBrush(br);

		m_SolidColor.SetColor(br.m_cFilterColor.pack_abgr8888()&0xffffff);
		m_BrightnessSlider.SetPos(br.m_fBrightness*255.0f);
	}
	else
	{
		m_layerAltMin.SetValue(0);
		m_layerAltMax.SetValue(255);
		m_layerSlopeMin.SetValue(0);
		m_layerSlopeMax.SetValue(90);
	}
}



void CTerrainPainterPanel::GetLayerMaskSettingsFromLayer()
{
	CLayer *pLayer = GetIEditor()->GetDocument()->FindLayer(GetSelectedLayer());

	if(pLayer)
	{
		pLayer->SetLayerStart(m_layerAltMin.GetValue());
		pLayer->SetLayerEnd(m_layerAltMax.GetValue());
		pLayer->SetLayerMinSlopeAngle(m_layerSlopeMin.GetValue());
		pLayer->SetLayerMaxSlopeAngle(m_layerSlopeMax.GetValue());
		GetIEditor()->GetDocument()->SetModifiedFlag();
	}
}




void CTerrainPainterPanel::OnBnClickedBrushSettolayer()
{
	CLayer *pLayer = GetIEditor()->GetDocument()->FindLayer(GetSelectedLayer());

	if(!pLayer)
		return;

	CTextureBrush br;
	m_tool->GetBrush(br);

	pLayer->m_cLayerFilterColor=br.m_cFilterColor;
	pLayer->m_fLayerBrightness=br.m_fBrightness;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::OnChange()
{
	bool isChecked = 1-m_resizeResolution.GetCheck();
	m_resizeResolution.SetCheck(isChecked);
	
	CTextureBrush br;
	m_tool->GetBrush(br);
	if(!isChecked)
		br.type = ET_BRUSH_PAINT;
	else
		br.type = ET_BRUSH_CHANGERESOLUTION;
	SetBrush( br );
	m_tool->SetBrush(br);
}

/*
//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::OnExport()
{
	m_tool->ImportExport(false, false);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::OnImport()
{
	m_tool->ImportExport(true, false);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainPainterPanel::OnFileBrowse()
{
	gSettings.BrowseTerrainTexture();
	GetDlgItem(IDC_FILE)->SetWindowText( gSettings.terrainTextureExport);
}
*/