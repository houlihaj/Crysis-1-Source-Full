// GridSettingsDialog.cpp : implementation file
//

#include "stdafx.h"

#include "GridSettingsDialog.h"
#include "ViewManager.h"


// CGridSettingsDialog dialog

IMPLEMENT_DYNAMIC(CGridSettingsDialog, CDialog)
CGridSettingsDialog::CGridSettingsDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CGridSettingsDialog::IDD, pParent)
{
}

CGridSettingsDialog::~CGridSettingsDialog()
{
}

void CGridSettingsDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SNAP, m_snapToGrid);
	DDX_Control(pDX, IDC_ANGLESNAP, m_angleSnap);
	DDX_Control(pDX, IDC_CONSTRUCTION_PLANE_SIZE, m_angleSnap);
	DDX_Control(pDX, IDC_DISPLAY_CONSTRUCTION_PLANE, m_displayCP);
	DDX_Control(pDX, IDC_SNAP_MARKER_DISPLAY, m_displaySnapMarker);
	DDX_Control(pDX, IDC_SNAP_MARKER_COLOR, m_snapMarkerColor);
	DDX_Control(pDX, IDC_GRID_USERDEFINED, m_userDefined);
	DDX_Control(pDX, IDC_GETFROMOBJECT, m_getFromObject);
	DDX_Control(pDX, IDC_GRID_GETANGLES, m_getAnglesFromObject);
	DDX_Control(pDX, IDC_GRID_GETTRANSLATION, m_getTranslationFromObject);
}


BEGIN_MESSAGE_MAP(CGridSettingsDialog, CDialog)
	ON_BN_CLICKED(IDC_GRID_USERDEFINED,OnBnUserDefined)
	ON_BN_CLICKED(IDC_GETFROMOBJECT,OnBnGetFromObject)
	ON_BN_CLICKED(IDC_GRID_GETANGLES,OnBnGetAngles)
	ON_BN_CLICKED(IDC_GRID_GETTRANSLATION,OnBnGetTranslation)
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////
BOOL CGridSettingsDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	CGrid *pGrid = GetIEditor()->GetViewManager()->GetGrid();

	m_gridSize.Create( this,IDC_GRID_SIZE,CNumberCtrl::LEFTALIGN );
	m_gridScale.Create( this,IDC_GRID_SCALE,CNumberCtrl::LEFTALIGN );

	m_userDefined.SetCheck( pGrid->bUserDefined?BST_CHECKED:BST_UNCHECKED );
	m_getFromObject.SetCheck( pGrid->bGetFromObject?BST_CHECKED:BST_UNCHECKED );

	m_angleX.Create( this,IDC_GRID_ROTX,CNumberCtrl::LEFTALIGN );
	m_angleX.SetRange( -180.0f, 180.0f);
	m_angleX.SetValue( pGrid->rotationAngles.x );

	m_angleY.Create( this,IDC_GRID_ROTY,CNumberCtrl::LEFTALIGN );
	m_angleY.SetRange( -180.0f, 180.0f);
	m_angleY.SetValue( pGrid->rotationAngles.y );

	m_angleZ.Create( this,IDC_GRID_ROTZ,CNumberCtrl::LEFTALIGN );
	m_angleZ.SetRange( -180.0f, 180.0f);
	m_angleZ.SetValue( pGrid->rotationAngles.z );

	m_translationX.Create( this,IDC_GRID_POSX,CNumberCtrl::LEFTALIGN );
	m_translationX.SetValue( pGrid->translation.x );

	m_translationY.Create( this,IDC_GRID_POSY,CNumberCtrl::LEFTALIGN );
	m_translationY.SetValue( pGrid->translation.y );

	m_translationZ.Create( this,IDC_GRID_POSZ,CNumberCtrl::LEFTALIGN );
	m_translationZ.SetValue( pGrid->translation.z );

	m_angleSnapScale.Create( this,IDC_ANGLESNAP_SIZE,CNumberCtrl::LEFTALIGN );
	m_angleSnapScale.SetInteger(true);

//	m_gridSize.SetInteger(true);
	m_gridSize.SetRange( 0.01,1024 );
	m_gridSize.SetValue( pGrid->size );
	m_gridScale.SetValue( pGrid->scale );
	m_snapToGrid.SetCheck( (pGrid->IsEnabled())?BST_CHECKED:BST_UNCHECKED );
	
	m_angleSnap.SetCheck( (pGrid->IsAngleSnapEnabled())?BST_CHECKED:BST_UNCHECKED );
	m_angleSnapScale.SetValue( pGrid->GetAngleSnap() );

	m_displayCP.SetCheck( (gSettings.snap.constructPlaneDisplay)?BST_CHECKED:BST_UNCHECKED );
	m_CPSize.Create( this,IDC_CONSTRUCTION_PLANE_SIZE,CNumberCtrl::LEFTALIGN );
	m_CPSize.SetValue( gSettings.snap.constructPlaneSize );

	m_displaySnapMarker.SetCheck( (gSettings.snap.markerDisplay)?BST_CHECKED:BST_UNCHECKED );
	m_snapMarkerSize.Create( this,IDC_SNAP_MARKER_SIZE,CNumberCtrl::LEFTALIGN );
	m_snapMarkerSize.SetValue( gSettings.snap.markerSize );

	m_snapMarkerColor.SetColor(gSettings.snap.markerColor);
	m_snapMarkerColor.SetDefaultColor(RGB(0,200,200));
	//m_snapMarkerColor.ShowText(FALSE);
	m_snapMarkerColor.ModifyCPStyle( 0,CPS_XT_EXTENDED|CPS_XT_MORECOLORS );

	EnableGridPropertyControls(pGrid->bUserDefined,pGrid->bGetFromObject);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CGridSettingsDialog::OnOK()
{
	CGrid *pGrid = GetIEditor()->GetViewManager()->GetGrid();

	pGrid->Enable( m_snapToGrid.GetCheck() == BST_CHECKED );
	pGrid->size = m_gridSize.GetValue();
	pGrid->scale = m_gridScale.GetValue();

	pGrid->bUserDefined = m_userDefined.GetCheck() == BST_CHECKED;
	pGrid->bGetFromObject = m_getFromObject.GetCheck() == BST_CHECKED;
	pGrid->rotationAngles.x = m_angleX.GetValue();
	pGrid->rotationAngles.y = m_angleY.GetValue();
	pGrid->rotationAngles.z = m_angleZ.GetValue();
	pGrid->translation.x = m_translationX.GetValue();
	pGrid->translation.y = m_translationY.GetValue();
	pGrid->translation.z = m_translationZ.GetValue();

	pGrid->bAngleSnapEnabled = m_snapToGrid.GetCheck() == BST_CHECKED;
	pGrid->angleSnap = m_angleSnapScale.GetValue();

	gSettings.snap.constructPlaneDisplay = m_displayCP.GetCheck() == BST_CHECKED;
	gSettings.snap.constructPlaneSize = m_CPSize.GetValue();
	
	gSettings.snap.markerDisplay = m_displaySnapMarker.GetCheck() == BST_CHECKED;
	gSettings.snap.markerSize = m_snapMarkerSize.GetValue();
	gSettings.snap.markerColor = m_snapMarkerColor.GetColor();
	
	CDialog::OnOK();

	gSettings.Save();
}

void CGridSettingsDialog::OnBnUserDefined()
{
	EnableGridPropertyControls(m_userDefined.GetCheck() == BST_CHECKED,m_getFromObject.GetCheck() == BST_CHECKED);
}

void CGridSettingsDialog::OnBnGetFromObject()
{
	EnableGridPropertyControls(m_userDefined.GetCheck() == BST_CHECKED,m_getFromObject.GetCheck() == BST_CHECKED);
}

void CGridSettingsDialog::OnBnGetAngles()
{
		CSelectionGroup *sel = GetIEditor()->GetSelection();
		if(sel->GetCount()>0)
		{
			CBaseObject *obj = sel->GetObject(0);
			Matrix34 tm = obj->GetWorldTM();
			AffineParts ap;
			ap.SpectralDecompose(tm);

			Vec3 rotation = Vec3(RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(ap.rot))));

			m_angleX.SetValue(rotation.x);
			m_angleY.SetValue(rotation.y);
			m_angleZ.SetValue(rotation.z);
		}
}

void CGridSettingsDialog::OnBnGetTranslation()
{
		CSelectionGroup *sel = GetIEditor()->GetSelection();
		if(sel->GetCount()>0)
		{
			CBaseObject *obj = sel->GetObject(0);
			Matrix34 tm = obj->GetWorldTM();
			Vec3 translation = tm.GetTranslation();

			m_translationX.SetValue(translation.x);
			m_translationY.SetValue(translation.y);
			m_translationZ.SetValue(translation.z);
		}
}

void CGridSettingsDialog::EnableGridPropertyControls(const bool isUserDefined,const bool isGetFromObject)
{
	m_getFromObject.EnableWindow(isUserDefined == true);

	m_angleX.EnableWindow(isUserDefined == true && isGetFromObject == false);
	m_angleY.EnableWindow(isUserDefined == true && isGetFromObject == false);
	m_angleZ.EnableWindow(isUserDefined == true && isGetFromObject == false);
	m_translationX.EnableWindow(isUserDefined == true && isGetFromObject == false);
	m_translationY.EnableWindow(isUserDefined == true && isGetFromObject == false);
	m_translationZ.EnableWindow(isUserDefined == true && isGetFromObject == false);
	m_getAnglesFromObject.EnableWindow(isUserDefined == true && isGetFromObject == false);
	m_getTranslationFromObject.EnableWindow(isUserDefined == true && isGetFromObject == false);
}