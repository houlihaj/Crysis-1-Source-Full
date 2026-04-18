// TerrainVoxelPanel.cpp : implementation file
//

#include "stdafx.h"
#include "TerrainVoxelPanel.h"
#include "TerrainVoxelTool.h"
#include "SurfaceType.h"

#include "CryEditDoc.h"

#include <I3DEngine.h>
#include <IPhysics.h>
#include ".\terrainvoxelpanel.h"

/////////////////////////////////////////////////////////////////////////////
// CTerrainVoxelPanel dialog


CTerrainVoxelPanel::CTerrainVoxelPanel(CTerrainVoxelTool *tool,CWnd* pParent /*=NULL*/)
	: CDialog(CTerrainVoxelPanel::IDD, pParent)
{
	//{{AFX_DATA_INIT(CTerrainVoxelPanel)
	//}}AFX_DATA_INIT

	m_tool = tool;
	Create( IDD,pParent );

	assert( tool != 0 );
}

void CTerrainVoxelPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTerrainVoxelPanel)
	DDX_Control(pDX, IDC_BRUSH_TYPE, m_brushType);
	DDX_Control(pDX, IDC_BRUSH_SHAPE, m_brushShape);
	DDX_Control(pDX, IDC_BRUSH_RADIUS_SLIDER, m_radiusSlider);
	DDX_Control(pDX, IDC_SURFACE_TYPES, m_types);
	DDX_Control(pDX, IDC_SETUPPOSITION, m_SetupPosition);
	DDX_Control(pDX, IDC_INITPOSITION, m_InitPosition);
	DDX_Control(pDX, IDC_ALIGN, m_Align);
	DDX_Control(pDX, IDC_COLOR, m_SolidColor);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CTerrainVoxelPanel, CDialog)
	//{{AFX_MSG_MAP(CTerrainVoxelPanel)
	ON_WM_HSCROLL()
	ON_EN_UPDATE(IDC_BRUSH_RADIUS, OnUpdateNumbers)
	ON_EN_UPDATE(IDC_BRUSH_DEPTH, OnUpdateNumbers)
	ON_EN_UPDATE(IDC_PLANE_SIZE, OnUpdateNumbers)
	ON_CBN_SELENDOK(IDC_BRUSH_TYPE, OnSelendokBrushType)
	ON_CBN_SELENDOK(IDC_BRUSH_SHAPE, OnSelendokBrushShape)
	ON_BN_CLICKED(IDC_VOXEL_PHYZ, OnFlagButton)
	ON_BN_CLICKED(IDC_VOXEL_SIMP, OnFlagButton)
	ON_BN_CLICKED(IDC_VOXEL_SHAD, OnFlagButton)
	ON_LBN_SELCHANGE(IDC_SURFACE_TYPES, OnSurfTypeSelchange)
	ON_CPN_XT_SELENDOK(IDC_COLOR, OnColorChanged)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_VOXEL_OBJECT, OnBnClickedVoxelObject)
	ON_BN_CLICKED(IDC_VOXEL_TERRAIN, OnBnClickedVoxelTerrain)
	ON_BN_CLICKED(IDC_SETUPPOSITION, OnSetupPosition)
	ON_BN_CLICKED(IDC_INITPOSITION, OnInitPosition)
	ON_BN_CLICKED(IDC_ALIGN, OnAlign)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTerrainVoxelPanel message handlers

BOOL CTerrainVoxelPanel::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_brushRadius.Create( this,IDC_BRUSH_RADIUS );
	m_brushDepth.Create( this,IDC_BRUSH_DEPTH );
	m_PlaneSize.Create( this,IDC_PLANE_SIZE );

	float fMin=0.05f;
	float fMax=200.0f;
	m_brushRadius.SetRange( fMin,fMax);								// number crtl
	m_radiusSlider.SetRange( RadiusToSliderPosition(fMin),RadiusToSliderPosition(fMax) );	// slider
	m_brushDepth.SetRange(0, 100);
	m_PlaneSize.SetInteger(true);
	m_PlaneSize.SetRange(10, 1000);
	m_PlaneSize.SetValue(m_tool->GetPlaneSize());
	
	m_brushType.AddString( "Subtract" );
	m_brushType.AddString( "Create" );
	m_brushType.AddString( "Material" );
	m_brushType.AddString( "Blur" );
	m_brushType.AddString( "CopyTerrain" );
	m_brushType.SetCurSel(1);
	
	m_brushShape.AddString( "Sphere" );
	m_brushShape.AddString( "Box" );
	m_brushShape.SetCurSel(0);

	CWnd * rb = GetDlgItem(IDC_VOXEL_OBJECT);
	if(rb)
		((CButton *)rb)->SetCheck(true);

	((CButton *) GetDlgItem(IDC_VOXEL_PHYZ))->SetCheck(TRUE);
	((CButton *) GetDlgItem(IDC_VOXEL_SHAD))->SetCheck(TRUE);

	SelectVoxelObjectType();	
	OnFlagButton();

	m_SolidColor.SetColor( m_tool->m_vpParams.m_Color );

	ReloadSurfaceTypes();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void CTerrainVoxelPanel::SelectVoxelObjectType()
{
	if(!m_tool)
		return;

	CButton * rb1 = (CButton *)GetDlgItem(IDC_VOXEL_OBJECT);
	CButton * rb2 = (CButton *)GetDlgItem(IDC_VOXEL_TERRAIN);
	if(rb1)
		rb1->SetCheck(m_tool->m_vpParams.m_voxelType==1);
	if(rb2)
		rb2->SetCheck(m_tool->m_vpParams.m_voxelType==2);

	((CButton *) GetDlgItem(IDC_VOXEL_PHYZ))->SetCheck(m_tool->m_vpParams.m_isUpdatePhysics);
	((CButton *) GetDlgItem(IDC_VOXEL_SIMP))->SetCheck(m_tool->m_vpParams.m_isSimplifyMesh);
	((CButton *) GetDlgItem(IDC_VOXEL_SHAD))->SetCheck(m_tool->m_vpParams.m_isComputeShadows);

	for(int i=0; i<m_types.GetCount(); i++)
	{
		CString selStr;
		m_types.GetText(i, selStr);
		if(!strcmp(selStr, m_tool->m_vpParams.m_SurfType))
			m_types.SetCurSel(i);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::ReloadSurfaceTypes()
{
	m_types.ResetContent();
	for (int i = 0; i < GetIEditor()->GetDocument()->GetSurfaceTypeCount(); i++)
	{
		m_types.InsertString( i, GetIEditor()->GetDocument()->GetSurfaceTypePtr(i)->GetName() );
	}

  if(GetIEditor()->GetDocument()->GetSurfaceTypeCount())
    m_types.SetCurSel(0);

  for(int i=0; i<m_types.GetCount(); i++)
  {
    CString selStr;
    m_types.GetText(i, selStr);
    if(!strcmp(selStr, m_tool->m_vpParams.m_SurfType))
      m_types.SetCurSel(i);
  }

  OnSurfTypeSelchange();
}


//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::OnColorChanged()
{
	m_tool->m_vpParams.m_Color = m_SolidColor.GetColor();
}


//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::SetBrush( CTerrainVoxelBrush &br, int voxelObjectType )
{
	if (br.type == eVoxelBrushSubtract)
	{
		m_brushType.SetCurSel(0);
	}
	if (br.type == eVoxelBrushCreate)
	{
		m_brushType.SetCurSel(1);
	}
	if (br.type == eVoxelBrushMaterial)
	{
		m_brushType.SetCurSel(2);
	}
	if (br.type == eVoxelBrushBlur)
	{
		m_brushType.SetCurSel(3);
	}
	if (br.type == eVoxelBrushCopyTerrain)
	{
		m_brushType.SetCurSel(4);
	}

	m_brushRadius.SetValue( br.radius );
	m_radiusSlider.SetPos( RadiusToSliderPosition(br.radius) );
	m_brushDepth.SetValue( br.depth );
	
	if(br.shape==eVoxelBrushShapeSphere)
	{
		m_brushShape.SetCurSel(0);
	}
	if(br.shape==eVoxelBrushShapeBox)
	{
		m_brushShape.SetCurSel(1);
	}

	SelectVoxelObjectType();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::OnUpdateNumbers() 
{
	CTerrainVoxelBrush br;
	m_tool->GetBrush(br);
	float prevRadius = br.radius;
	br.radius = m_brushRadius.GetValue();
	br.depth = m_brushDepth.GetValue();

	SetBrush( br, -1 );
	m_tool->SetBrush(br);
	m_tool->SetPlaneSize(m_PlaneSize.GetValue());
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::OnFlagButton() 
{
	int f1 = ((CButton *) GetDlgItem(IDC_VOXEL_PHYZ))->GetCheck();
	int f2 = ((CButton *) GetDlgItem(IDC_VOXEL_SIMP))->GetCheck();
	int f3 = ((CButton *) GetDlgItem(IDC_VOXEL_SHAD))->GetCheck();

	m_tool->m_vpParams.m_isUpdatePhysics = f1;
	m_tool->m_vpParams.m_isSimplifyMesh = f2;
	m_tool->m_vpParams.m_isComputeShadows = f3;

	gEnv->p3DEngine->Voxel_SetFlags(f1, f2, f3, false);
}

//////////////////////////////////////////////////////////////////////////
float CTerrainVoxelPanel::SliderPositionToRadius(int sliderPosition)
{
	return pow(10.0f, sliderPosition / 100.0f);
}

int CTerrainVoxelPanel::RadiusToSliderPosition(float radius)
{
	return 100 * log10(radius);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::OnHScroll( UINT nSBCode,UINT nPos,CScrollBar* pScrollBar )
{
	CSliderCtrl *pSliderCtrl = (CSliderCtrl*)pScrollBar;
	CTerrainVoxelBrush br;
	m_tool->GetBrush(br);
	if (pSliderCtrl == &m_radiusSlider)
	{
		br.radius = SliderPositionToRadius(m_radiusSlider.GetPos());;
	}
	SetBrush( br, -1 );
	m_tool->SetBrush(br);
}

void CTerrainVoxelPanel::OnSelendokBrushType() 
{
	int sel = m_brushType.GetCurSel();
	if (sel != LB_ERR)
	{
		switch (sel)
		{
		case 0:
			m_tool->SetActiveBrushType(eVoxelBrushSubtract);
			break;
		case 1:
			m_tool->SetActiveBrushType(eVoxelBrushCreate);
			break;
		case 2:
			m_tool->SetActiveBrushType(eVoxelBrushMaterial);
			break;
		case 3:
			m_tool->SetActiveBrushType(eVoxelBrushBlur);
			break;
		case 4:
			m_tool->SetActiveBrushType(eVoxelBrushCopyTerrain);
			break;
		}
	}
	CTerrainVoxelBrush br;
	m_tool->GetBrush(br);
	SetBrush( br, -1 );
}


void CTerrainVoxelPanel::OnSelendokBrushShape() 
{
	int sel = m_brushShape.GetCurSel();
	if (sel != LB_ERR)
	{
		switch (sel)
		{
		case 0:
			m_tool->SetActiveBrushShape(eVoxelBrushShapeSphere);
			break;
		case 1:
			m_tool->SetActiveBrushShape(eVoxelBrushShapeBox);
			break;
		}
	}
	CTerrainVoxelBrush br;
	m_tool->GetBrush(br);
	SetBrush( br, -1 );
}

void CTerrainVoxelPanel::OnBnClickedVoxelObject()
{
	m_tool->SetActiveVoxelObjectType(1);
}

void CTerrainVoxelPanel::OnBnClickedVoxelTerrain()
{
	m_tool->SetActiveVoxelObjectType(2);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::OnSurfTypeSelchange()
{
	int curSel = m_types.GetCurSel();
	if (curSel < 0)
		return;
	m_types.GetText(curSel, m_tool->m_vpParams.m_SurfType );
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::OnSetupPosition() 
{
	bool isCheck = m_SetupPosition.GetCheck();
	m_SetupPosition.SetCheck(!isCheck);
	m_tool->SetupPosition(!isCheck);
	m_Align.SetCheck(0);
	m_tool->PlaneAlign(false);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::OnAlign()
{
	bool isCheck = m_Align.GetCheck();
	m_Align.SetCheck(!isCheck);
	m_tool->SetupPosition(false);
	m_SetupPosition.SetCheck(0);
	m_tool->PlaneAlign(!isCheck);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainVoxelPanel::OnInitPosition()
{
	m_SetupPosition.SetCheck(false);
	m_tool->InitPosition();
	OnSetupPosition();
}