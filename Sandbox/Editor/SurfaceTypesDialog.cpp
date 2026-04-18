// SurfaceTypesDialog.cpp : implementation file
//

#include "stdafx.h"
#include "SurfaceTypesDialog.h"
#include "SurfaceType.h"
#include "CryEditDoc.h"
#include "StringDlg.h"
#include "GameEngine.h"
#include "Material\MaterialManager.h"
#include "MatEditMainDlg.h"

#define MAX_SURFACE_TYPES 256

/////////////////////////////////////////////////////////////////////////////
// CSurfaceTypesDialog dialog
CSurfaceTypesDialog::CSurfaceTypesDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CSurfaceTypesDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSurfaceTypesDialog)
	m_currSurfaceTypeName = _T("");
	//}}AFX_DATA_INIT
	m_firstSelected = "";
}


void CSurfaceTypesDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_MATERIAL, m_materialEdit);
	DDX_Control(pDX, IDC_SURFACE_TYPES, m_types);
	DDX_LBString(pDX, IDC_SURFACE_TYPES, m_currSurfaceTypeName);
	DDX_Control(pDX, IDC_PROJECTION, m_projCombo);

	DDX_Control(pDX, IDC_ADD_SFTYPE, m_btnSTAdd);
	DDX_Control(pDX, IDC_REMOVE_SFTYPE, m_btnSTRemove);
	DDX_Control(pDX, IDC_RENAME_SFTYPE, m_btnSTRename);
	DDX_Control(pDX, IDC_CLONE_SFTYPE, m_btnSTClone);
}


BEGIN_MESSAGE_MAP(CSurfaceTypesDialog, CDialog)
	ON_BN_CLICKED(IDC_ADD_SFTYPE, OnAddType)
	ON_BN_CLICKED(IDC_REMOVE_SFTYPE, OnRemoveType)
	ON_BN_CLICKED(IDC_RENAME_SFTYPE, OnRenameType)
	ON_LBN_SELCHANGE(IDC_SURFACE_TYPES, OnSelectSurfaceType)
	ON_BN_CLICKED(ID_IMPORT, OnImport)
	ON_BN_CLICKED(ID_EXPORT, OnExport)
	ON_LBN_DBLCLK(IDC_SURFACE_TYPES, OnDblclkSurfaceTypes)
	ON_BN_CLICKED(IDC_CLONE_SFTYPE, OnCloneSftype)
	ON_EN_UPDATE(IDC_SCALEX, OnDetailScaleUpdate )
	ON_EN_UPDATE(IDC_SCALEY, OnDetailScaleUpdate )
	ON_CBN_SELENDOK(IDC_PROJECTION, OnCbnSelendokProjection)
	ON_BN_CLICKED(IDC_MATERIAL_BROWSE, OnBnClickedMaterialEditor)
	ON_BN_CLICKED(IDC_PICK, OnBnClickedMaterialPick)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CSurfaceTypesDialog::ReloadSurfaceTypes()
{
	int sel = m_types.GetCurSel();
	m_types.ResetContent();
	for (int i = 0; i < m_srfTypes.size(); i++)
	{
		m_types.InsertString( i,m_srfTypes[i]->GetName() );
	}
	if (m_types.GetCount() > 0)
	{
		m_types.SetSel(0);
		m_currSurfaceType = m_srfTypes[0];
	} else 
		m_currSurfaceType = 0;

	if (sel != LB_ERR && sel < m_types.GetCount())
	{
		m_types.SetCurSel(sel);
		SetCurrentSurfaceType( m_srfTypes[sel] );
	}
	else if (m_types.GetCount() > 0)
	{
		// Select first surface type.
		m_types.SetSel(0);
		SetCurrentSurfaceType( m_srfTypes[0] );
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSurfaceTypesDialog message handlers

void CSurfaceTypesDialog::OnAddType() 
{
	// Surface types count limited to MAX_SURFACE_TYPES.
	if (m_srfTypes.size() >= MAX_SURFACE_TYPES)
		return;
	
	CSurfaceType *sf = new CSurfaceType;
	char sfName[128];
	sprintf( sfName,"SurfaceType%d",m_srfTypes.size() );
	sf->SetName( sfName );
	m_srfTypes.push_back( sf );
	m_newSurfaceTypes.insert( sf );
	ReloadSurfaceTypes();
	if (m_types.GetCount() > 0)
	{
		m_types.SetSel(m_types.GetCount()-1);
		m_currSurfaceType = sf;
	}
}

void CSurfaceTypesDialog::OnRemoveType() 
{
	// TODO: Add your control notification handler code here
	int sel = m_types.GetCurSel();
	if (sel != LB_ERR)
	{
		if (m_newSurfaceTypes.find(m_srfTypes[sel]) != m_newSurfaceTypes.end())
		{
			m_newSurfaceTypes.erase( m_srfTypes[sel] );
			delete m_srfTypes[sel];
		}
		m_srfTypes.erase( m_srfTypes.begin()+sel );
		ReloadSurfaceTypes();
	}
}

void CSurfaceTypesDialog::OnRenameType() 
{
	// Change name of selected type.
	if (!m_currSurfaceType)
		return;

	CStringDlg cDialog;
	cDialog.m_strString = m_currSurfaceType->GetName();
	cDialog.DoModal();
	m_currSurfaceType->SetName(cDialog.m_strString);

	ReloadSurfaceTypes();

	// We modified the document
	GetIEditor()->SetModifiedFlag();
}

BOOL CSurfaceTypesDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_scaleX.Create( this,IDC_SCALEX );
	m_scaleY.Create( this,IDC_SCALEY );
	m_projCombo.AddString( "X-Axis" );
	m_projCombo.AddString( "Y-Axis" );
	m_projCombo.AddString( "Z-Axis" );
	
	m_srfTypes.clear();
	m_srfTypes.reserve( 10+GetDoc()->GetSurfaceTypeCount()*2 );

	for (int i = 0; i < GetDoc()->GetSurfaceTypeCount(); i++)
		m_srfTypes.push_back( GetDoc()->GetSurfaceTypePtr(i) );

	m_currSurfaceType = 0;
	ReloadSurfaceTypes();

	m_types.SelectString( -1,m_firstSelected );
	OnSelectSurfaceType();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

CCryEditDoc* CSurfaceTypesDialog::GetDoc()
{
	return GetIEditor()->GetDocument();
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceTypesDialog::SetSelectedSurfaceType( const CString &srfTypeName )
{
	m_firstSelected = srfTypeName;
}

void CSurfaceTypesDialog::SetCurrentSurfaceType( CSurfaceType *sf )
{
	m_currSurfaceType = sf;
	if (m_currSurfaceType)
	{
		m_materialEdit.SetWindowText( m_currSurfaceType->GetMaterial() );
		m_scaleX.SetValue( sf->GetDetailTextureScale().x );
		m_scaleY.SetValue( sf->GetDetailTextureScale().y );
		m_projCombo.SetCurSel( sf->GetProjAxis() );
	}
}

void CSurfaceTypesDialog::OnSelectSurfaceType() 
{
	int sel = m_types.GetCurSel();
	if (sel != LB_ERR)
	{
		SetCurrentSurfaceType( m_srfTypes[sel] );
	}
	else
		SetCurrentSurfaceType( 0 );
}

void CSurfaceTypesDialog::OnOK() 
{
	// On ok.	
	GetDoc()->RemoveAllSurfaceTypes();
	for (int i = 0; i < m_srfTypes.size(); i++)
	{
		GetDoc()->AddSurfaceType( m_srfTypes[i] );
	}

	GetIEditor()->GetGameEngine()->ReloadSurfaceTypes();

	GetDoc()->SetModifiedFlag();
	CDialog::OnOK();
}

void CSurfaceTypesDialog::OnCancel() 
{
	DeleteNew();
	CDialog::OnCancel();
}

void CSurfaceTypesDialog::DeleteNew()
{
	// Release memory for newly created surface types.
	for (std::set<CSurfaceType*>::iterator it = m_newSurfaceTypes.begin(); it != m_newSurfaceTypes.end(); ++it)
	{
		delete *it;
	}
}

void CSurfaceTypesDialog::OnImport() 
{
	char szFilters[] = "Surface Types Files (*.sft)|*.sft||";
	CAutoDirectoryRestoreFileDialog dlg(TRUE, NULL,"*.sft", OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR, szFilters);

	if (dlg.DoModal() == IDOK) 
	{
		CXmlArchive ar;
		ar.Load( dlg.GetPathName() );
		SerializeSurfaceTypes(ar);

		ReloadSurfaceTypes();
	}
}

void CSurfaceTypesDialog::OnExport() 
{
	char szFilters[] = "Surface Types Files (*.sft)|*.sft||";
	CAutoDirectoryRestoreFileDialog dlg(FALSE, "sft",NULL, OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR, szFilters);

	if (dlg.DoModal() == IDOK) 
	{
		CXmlArchive ar( "SurfaceTypesSettings" );
		SerializeSurfaceTypes(ar);
		ar.Save( dlg.GetPathName() );
	}
}

void CSurfaceTypesDialog::SerializeSurfaceTypes( CXmlArchive &xmlAr )
{
	if (xmlAr.bLoading)
	{
		// Loading
		CLogFile::WriteLine("Loading surface types...");

		// Clear old layers
		DeleteNew();
		m_srfTypes.clear();

		// Load the layer count
		XmlNodeRef node = xmlAr.root->findChild( "SurfaceTypes" );
		if (!node)
			return;

		// Read all node
		m_srfTypes.resize( node->getChildCount() ); 
		for (int i=0; i < node->getChildCount(); i++)
		{
			CXmlArchive ar( xmlAr );
			ar.root = node->getChild(i);
			// Fill the layer with the data
			m_srfTypes[i] = new CSurfaceType;
			m_srfTypes[i]->Serialize( ar );
		}
	}
	else
	{
		// Storing
		CLogFile::WriteLine("Storing surface types...");

		// Save the layer count

		XmlNodeRef node = xmlAr.root->newChild( "SurfaceTypes" );

		// Write all surface types.
		for (int i = 0; i < m_srfTypes.size(); i++)
		{
			CXmlArchive ar( xmlAr );
			ar.root = CreateXmlNode( "SurfaceType" );
			node->addChild( ar.root );
			m_srfTypes[i]->Serialize( ar );
		}
	}
}

void CSurfaceTypesDialog::OnDblclkSurfaceTypes() 
{
	OnRenameType();	
}

void CSurfaceTypesDialog::OnCloneSftype() 
{
	if (!m_currSurfaceType)
		return;

	// Surface types count limited to MAX_SURFACE_TYPES.
	if (m_srfTypes.size() >= MAX_SURFACE_TYPES)
		return;

	CSurfaceType *sf = new CSurfaceType;
	*sf = *m_currSurfaceType;

	char sfName[128];
	sprintf( sfName,"%s%d",(const char*)m_currSurfaceType->GetName(),m_srfTypes.size() );
	sf->SetName( sfName );
	m_srfTypes.push_back( sf );
	m_newSurfaceTypes.insert( sf );
	
	ReloadSurfaceTypes();
	if (m_types.GetCount() > 0)
	{
		m_types.SetSel(m_types.GetCount()-1);
		m_currSurfaceType = sf;
	}
}

void CSurfaceTypesDialog::OnDetailScaleUpdate()
{
	if (m_currSurfaceType)
	{
		float x = m_scaleX.GetValue();
		float y = m_scaleY.GetValue();
		m_currSurfaceType->SetDetailTextureScale( Vec3(x,y,0) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceTypesDialog::OnCbnSelendokProjection()
{
	if (m_currSurfaceType)
	{
		int sel = m_projCombo.GetCurSel();
		if (sel != LB_ERR)
		{
			m_currSurfaceType->SetProjAxis(sel);
		}
	}	
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceTypesDialog::OnBnClickedMaterialEditor()
{
	CString name;
	m_materialEdit.GetWindowText(name);

	CMatEditMainDlg dlg;
	dlg.DoModal();
	//GetIEditor()->OpenDataBaseLibrary( EDB_MATERIAL_LIBRARY,GetIEditor()->GetMaterialManager()->FindItemByName(name) );
}

//////////////////////////////////////////////////////////////////////////
void CSurfaceTypesDialog::OnBnClickedMaterialPick()
{
	if (!m_currSurfaceType)
		return;
	CString name;
	CMaterial *pMaterial = GetIEditor()->GetMaterialManager()->GetCurrentMaterial();
	if (pMaterial)
		name = pMaterial->GetName();

  m_materialEdit.SetWindowText(name);
	m_currSurfaceType->SetMaterial(name);
}
