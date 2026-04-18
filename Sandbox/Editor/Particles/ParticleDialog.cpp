////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   particledialog.cpp
//  Version:     v1.00
//  Created:     17/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleDialog.h"

#include "StringDlg.h"
#include "NumberDlg.h"

#include "ParticleManager.h"
#include "ParticleLibrary.h"
#include "ParticleItem.h"

#include "Objects\BrushObject.h"
#include "Objects\Entity.h"
#include "Objects\ObjectManager.h"
#include "ViewManager.h"
#include "Clipboard.h"

#include <I3DEngine.h>
#include <ParticleParams.h>
#include <CryTypeInfo.h>
#include <IEntitySystem.h>

#define CURVE_TYPE
#include "Util\VariableTypeInfo.h"

#include "ParticleParamsTypeInfo.cpp"

#define IDC_PARTICLES_TREE AFX_IDW_PANE_FIRST

#define EDITOR_OBJECTS_PATH CString("Editor/Objects/")

IMPLEMENT_DYNAMIC(CParticleDialog,CBaseLibraryDialog);
//////////////////////////////////////////////////////////////////////////
// Particle UI structures.
//////////////////////////////////////////////////////////////////////////

/** User Interface definition of particle system.
*/
class CParticleUIDefinition
{
public:
	ParticleParams m_localParams;
	CVarBlockPtr m_vars;
	IVariable::OnSetCallback m_onSetCallback;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		m_vars = new CVarBlock;

		// Create UI vars, using ParticleParams TypeInfo.
		CVariableArray* pVarTable = AddGroup("Emitter");

		const CTypeInfo& partInfo = TypeInfo((ParticleParams*)0);
		for AllSubVars( pVarInfo, partInfo )
		{
			if (*pVarInfo->Name == '_')
				continue;

			cstr sGroup;
			if (pVarInfo->GetAttr("Group", sGroup))
				pVarTable = AddGroup(sGroup);

			IVariable* pVar = CVariableTypeInfo::Create(*pVarInfo, &m_localParams);

			// Add to group.
			pVar->AddRef();								// Variables are local and must not be released by CVarBlock.
			pVarTable->AddChildVar(pVar);
		}

		return m_vars;
	}

	//////////////////////////////////////////////////////////////////////////
	void SetFromParticles( CParticleItem *pParticles )
	{
		IParticleEffect *pEffect = pParticles->GetEffect();
		if (!pEffect)
			return;

		// Copy to local params, then run update on all vars.
		m_localParams = pEffect->GetParticleParams();
		m_vars->OnSetValues();
	}

	//////////////////////////////////////////////////////////////////////////
	void SetToParticles( CParticleItem *pParticles )
	{
		IParticleEffect *pEffect = pParticles->GetEffect();
		if (!pEffect)
			return;

		pEffect->SetParticleParams(m_localParams);

		// Update particles.
		pParticles->Update();
	}

private:

	//////////////////////////////////////////////////////////////////////////
  CVariableArray* AddGroup( const char *sName )
	{
		CVariableArray* pArray = new CVariableArray;
		pArray->AddRef();
		pArray->SetFlags(IVariable::UI_BOLD);
		if (sName)
			pArray->SetName(sName);
		m_vars->AddVariable(pArray);
		return pArray;
	}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CParticlePickCallback : public IPickObjectCallback
{
public:
	CParticlePickCallback() { m_bActive = true; };
	//! Called when object picked.
	virtual void OnPick( CBaseObject *picked )
	{
		/*
		m_bActive = false;
		CParticleItem *pParticles = picked->GetParticle();
		if (pParticles)
			GetIEditor()->OpenParticleLibrary( pParticles );
			*/
		delete this;
	}
	//! Called when pick mode cancelled.
	virtual void OnCancelPick()
	{
		m_bActive = false;
		delete this;
	}
	//! Return true if specified object is pickable.
	virtual bool OnPickFilter( CBaseObject *filterObject )
	{
		/*
		// Check if object have material.
		if (filterObject->GetParticle())
			return true;
		*/
		return false;
	}
	static bool IsActive() { return m_bActive; };
private:
	static bool m_bActive;
};
bool CParticlePickCallback::m_bActive = false;
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// CParticleDialog implementation.
//////////////////////////////////////////////////////////////////////////
CParticleDialog::CParticleDialog( CWnd *pParent )
	: CBaseLibraryDialog(IDD_DB_ENTITY, pParent)
{
	m_pPartManager = GetIEditor()->GetParticleManager();
	m_pItemManager = m_pPartManager;

	m_bRealtimePreviewUpdate = true;
	m_pGeometry = 0;
	m_pRenderNode = 0;
	
	m_drawType = DRAW_BOX;
	m_geometryFile = Path::MakeFullPath(EDITOR_OBJECTS_PATH + "MtlBox.cgf");
	m_bOwnGeometry = true;

	m_dragImage = 0;
	m_hDropItem = 0;

	m_hCursorDefault = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	m_hCursorCreate = AfxGetApp()->LoadCursor( IDC_HIT_CURSOR );
	m_hCursorReplace = AfxGetApp()->LoadCursor(IDC_HAND_INTERNAL);

	pParticleUI = new CParticleUIDefinition;

	// Immidiatly create dialog.
	Create( IDD_DATABASE,pParent );
}

CParticleDialog::~CParticleDialog()
{
	delete pParticleUI;	
}

void CParticleDialog::DoDataExchange(CDataExchange* pDX)
{
	CBaseLibraryDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CParticleDialog, CBaseLibraryDialog)
	ON_COMMAND( ID_DB_ADD,OnAddItem )
	ON_COMMAND( ID_DB_PLAY,OnPlay )

	ON_COMMAND( ID_DB_MTL_DRAWSELECTED,OnDrawSelection )
	ON_COMMAND( ID_DB_MTL_DRAWBOX,OnDrawBox )
	ON_COMMAND( ID_DB_MTL_DRAWSPHERE,OnDrawSphere )
	ON_COMMAND( ID_DB_MTL_DRAWTEAPOT,OnDrawTeapot )
	ON_UPDATE_COMMAND_UI( ID_DB_PLAY,OnUpdatePlay )

	ON_COMMAND( ID_DB_SELECTASSIGNEDOBJECTS,OnSelectAssignedObjects )
	ON_COMMAND( ID_DB_MTL_ACTIVATE_ALL,OnActivateAll )
	ON_COMMAND( ID_DB_ASSIGNTOSELECTION,OnAssignParticleToSelection )
	ON_COMMAND( ID_DB_MTL_GETFROMSELECTION,OnGetParticleFromSelection )
	ON_COMMAND( ID_DB_MTL_RESETMATERIAL,OnResetParticleOnSelection )
	ON_COMMAND( ID_DB_PLAY,OnEntityStart)
	ON_COMMAND( ID_DB_STOP,OnEntityStop)
	ON_UPDATE_COMMAND_UI( ID_DB_ASSIGNTOSELECTION,OnUpdateAssignMtlToSelection )
	ON_UPDATE_COMMAND_UI( ID_DB_SELECTASSIGNEDOBJECTS,OnUpdateSelected )
	ON_UPDATE_COMMAND_UI( ID_DB_MTL_GETFROMSELECTION,OnUpdateObjectSelected )
	ON_UPDATE_COMMAND_UI( ID_DB_MTL_RESETMATERIAL,OnUpdateObjectSelected )

	ON_COMMAND( ID_DB_MTL_ADDSUBMTL,OnAddSubItem )
	ON_COMMAND( ID_DB_MTL_DELSUBMTL,OnDelSubItem )
	ON_UPDATE_COMMAND_UI( ID_DB_MTL_ADDSUBMTL,OnUpdateSelected )
	ON_UPDATE_COMMAND_UI( ID_DB_MTL_DELSUBMTL,OnUpdateSelected )

	ON_COMMAND( ID_DB_MTL_PICK,OnPickMtl )
	ON_UPDATE_COMMAND_UI( ID_DB_MTL_PICK,OnUpdatePickMtl )

	ON_NOTIFY(TVN_BEGINDRAG, IDC_PARTICLES_TREE, OnBeginDrag)
	ON_NOTIFY(NM_RCLICK , IDC_PARTICLES_TREE, OnNotifyTreeRClick)
	ON_NOTIFY(NM_CLICK, IDC_PARTICLES_TREE, OnNotifyTreeLClick)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnDestroy()
{
	int temp;
	int HSplitter,VSplitter;
	m_wndHSplitter.GetRowInfo( 0,HSplitter,temp );
	m_wndVSplitter.GetColumnInfo( 0,VSplitter,temp );
	AfxGetApp()->WriteProfileInt("Dialogs\\Particles","HSplitter",HSplitter );
	AfxGetApp()->WriteProfileInt("Dialogs\\Particles","VSplitter",VSplitter );

	//ReleaseGeometry();
	CBaseLibraryDialog::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CParticleDialog::OnInitDialog()
{
	CBaseLibraryDialog::OnInitDialog();

	InitToolbar(IDR_DB_PARTICLES_BAR);

	CRect rc;
	GetClientRect(rc);

	// Create left panel tree control.
	m_treeCtrl.Create( WS_VISIBLE|WS_CHILD|WS_TABSTOP|WS_BORDER|TVS_HASBUTTONS|TVS_SHOWSELALWAYS|TVS_LINESATROOT|TVS_HASLINES|
		TVS_FULLROWSELECT|TVS_NOHSCROLL|TVS_INFOTIP|TVS_TRACKSELECT,rc,this,IDC_LIBRARY_ITEMS_TREE );
	if (!gSettings.gui.bWindowsVista)
		m_treeCtrl.SetItemHeight(18);

	//int h2 = rc.Height()/2;
	int h2 = 200;

	int HSplitter = AfxGetApp()->GetProfileInt("Dialogs\\Particles","HSplitter",200 );
	int VSplitter = AfxGetApp()->GetProfileInt("Dialogs\\Particles","VSplitter",200 );
	//int HSplitter = 300;

	m_wndVSplitter.CreateStatic( this,1,2,WS_CHILD|WS_VISIBLE );
	m_wndHSplitter.CreateStatic( &m_wndVSplitter,2,1,WS_CHILD|WS_VISIBLE );

	//m_imageList.Create(IDB_PARTICLES_TREE, 16, 1, RGB (255, 0, 255));
	CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_PARTICLES_TREE,16,RGB(255,0,255) );

	// TreeCtrl must be already created.
	m_treeCtrl.SetParent( &m_wndHSplitter );
	m_treeCtrl.SetImageList(&m_imageList,TVSIL_NORMAL);

	m_previewCtrl.Create( &m_wndHSplitter,rc,WS_CHILD|WS_VISIBLE );
	m_previewCtrl.SetGrid(true);
	m_previewCtrl.SetAxis(true);
	m_previewCtrl.EnableUpdate( true );

	m_propsCtrl.Create( WS_VISIBLE|WS_CHILD|WS_BORDER,rc,&m_wndVSplitter,2 );
	m_vars = pParticleUI->CreateVars();
	m_propsCtrl.AddVarBlock( m_vars );
	m_propsCtrl.ExpandAllChilds( m_propsCtrl.GetRootItem(),false );
	m_propsCtrl.EnableWindow( FALSE );

	//m_wndHSplitter.SetPane( 0,0,&m_previewCtrl,CSize(100,HSplitter) );
	//m_wndHSplitter.SetPane( 1,0,&m_propsCtrl,CSize(100,HSplitter) );

	//m_wndVSplitter.SetPane( 0,0,&m_treeCtrl,CSize(VSplitter,100) );
	//m_wndVSplitter.SetPane( 0,1,&m_wndHSplitter,CSize(VSplitter,100) );

	m_wndHSplitter.SetPane( 0,0,&m_treeCtrl,CSize(100,HSplitter) );
	m_wndHSplitter.SetPane( 1,0,&m_previewCtrl,CSize(100,HSplitter) );

	m_wndVSplitter.SetPane( 0,0,&m_wndHSplitter,CSize(VSplitter,100) );
	m_wndVSplitter.SetPane( 0,1,&m_propsCtrl,CSize(VSplitter,100) );

	RecalcLayout();

	ReloadLibs();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
UINT CParticleDialog::GetDialogMenuID()
{
	return IDR_DB_ENTITY;
};

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CParticleDialog::InitToolbar( UINT nToolbarResID )
{
	InitLibraryToolbar();

	CXTPToolBar *pParticleToolBar = GetCommandBars()->Add( _T("ParticlesToolBar"),xtpBarTop );
	pParticleToolBar->EnableCustomization(FALSE);
	VERIFY(pParticleToolBar->LoadToolBar( nToolbarResID ));

	CXTPToolBar *pItemBar = GetCommandBars()->GetToolBar(IDR_DB_LIBRARY_ITEM_BAR);
	if (pItemBar)
		DockRightOf(pParticleToolBar,pItemBar);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType,cx,cy);
	// resize splitter window.
	if (m_wndVSplitter.m_hWnd)
	{
		/*
		int cxCur,cxMin;
		m_wndVSplitter.GetColumnInfo(0,cxCur,cxMin);
		int nSize = max(cx-cxCur,0);
		m_wndHSplitter.SetRowInfo( 0,nSize,100 );
		*/

		CRect rc;
		GetClientRect(rc);
		m_wndVSplitter.MoveWindow(rc,FALSE);
	}
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CParticleDialog::InsertItemToTree( CBaseLibraryItem *pItem,HTREEITEM hParent )
{
	CParticleItem *pParticles = (CParticleItem*)pItem;

	if (pParticles->GetParent())
	{
		if (!hParent || hParent == TVI_ROOT || m_treeCtrl.GetItemData(hParent) == 0)
			return 0;
	}

	HTREEITEM hMtlItem = CBaseLibraryDialog::InsertItemToTree( pItem,hParent );
	UpdateItemState(pParticles);

	for (int i = 0; i < pParticles->GetChildCount(); i++)
	{
		CParticleItem *pSubItem = pParticles->GetChild(i);
		InsertItemToTree( pSubItem,hMtlItem );
	}
	return hMtlItem;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnAddItem()
{
	if (!m_pLibrary)
		return;

	CStringGroupDlg dlg( _T("New Particle Name"),this );
	dlg.SetGroup( m_selectedGroup );
	//dlg.SetString( entityClass );
	if (dlg.DoModal() != IDOK || dlg.GetString().IsEmpty())
	{
		return;
	}

	CString fullName = m_pItemManager->MakeFullItemName( m_pLibrary,dlg.GetGroup(),dlg.GetString() );
	if (m_pItemManager->FindItemByName( fullName ))
	{
		Warning( "Item with name %s already exist",(const char*)fullName );
		return;
	}

	CParticleItem *pParticles = (CParticleItem*)m_pItemManager->CreateItem( m_pLibrary );

	pParticles->SetDefaults();
	
	// Make prototype name.
	SetItemName( pParticles,dlg.GetGroup(),dlg.GetString() );
	pParticles->Update();

	ReloadItems();
	SelectItem( pParticles );
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::SetParticleVars( CParticleItem *pParticles )
{
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::SelectItem( CBaseLibraryItem *item,bool bForceReload )
{
	bool bChanged = item != m_pCurrentItem || bForceReload;
	m_propsCtrl.SelectItem(0);
	CBaseLibraryDialog::SelectItem( item,bForceReload );
	
	if (!item)
	{
		m_propsCtrl.EnableWindow(FALSE);
		return;
	}

	if (!bChanged)
		return;

	// Empty preview control.
	m_previewCtrl.LoadParticleEffect( 0 );
	
	//m_pPartManager->SetCurrentParticle( (CParticleItem*)item );

	if (!m_pRenderNode)
	{
		//LoadGeometry( m_geometryFile );
	}
	//if (m_pRenderNode)
		//m_previewCtrl.SetEntity( m_pRenderNode );
	
	if (m_propsCtrl.IsWindowEnabled() != TRUE)
		m_propsCtrl.EnableWindow(TRUE);
	m_propsCtrl.EnableUpdateCallback(false);

	// Render preview geometry with current material
	CParticleItem *pParticles = GetSelectedParticle();

	if (pParticles)
	{
		m_previewCtrl.LoadParticleEffect( pParticles->GetEffect() );
	}
	//AssignMtlToGeometry();

	// Update variables.
	m_propsCtrl.EnableUpdateCallback(false);
	pParticleUI->SetFromParticles( pParticles );
	m_propsCtrl.EnableUpdateCallback(true);

	m_propsCtrl.SetUpdateCallback( functor(*this,&CParticleDialog::OnUpdateProperties) );
	m_propsCtrl.EnableUpdateCallback(true);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::Update()
{
	if (!m_bRealtimePreviewUpdate)
		return;

	// Update preview control.
	if (m_pRenderNode)
	{
		m_previewCtrl.Update();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdateProperties( IVariable *var )
{
	CParticleItem *pParticles = GetSelectedParticle();
	if (!pParticles)
		return;

	pParticleUI->SetToParticles( pParticles );

	// Update visual cues of item and parents.
	for (; pParticles; pParticles = pParticles->GetParent())
		UpdateItemState(pParticles);

	//AssignMtlToGeometry();

	GetIEditor()->SetModifiedFlag();
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnPlay()
{
	m_bRealtimePreviewUpdate = !m_bRealtimePreviewUpdate;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdatePlay( CCmdUI* pCmdUI )
{
	if (m_bRealtimePreviewUpdate)
		pCmdUI->SetCheck(TRUE);
	else
		pCmdUI->SetCheck(FALSE);
}

//////////////////////////////////////////////////////////////////////////
CParticleItem* CParticleDialog::GetSelectedParticle()
{
	CBaseLibraryItem *pItem = m_pCurrentItem;
	return (CParticleItem*)pItem;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnDrawSelection()
{
	//if (m_drawType == DRAW_SELECTION)
		//return;
	m_drawType = DRAW_SELECTION;
	
	m_geometryFile = "";
	//ReleaseGeometry();

	m_bOwnGeometry = false;
	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	if (!pSel->IsEmpty())
	{

		//AssignMtlToGeometry();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnDrawBox()
{
	if (m_drawType == DRAW_BOX)
		return;
	m_drawType = DRAW_BOX;
	//LoadGeometry( Path::MakeFullPath(EDITOR_OBJECTS_PATH+"MtlBox.cgf") );
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnDrawSphere()
{
	if (m_drawType == DRAW_SPHERE)
		return;
	m_drawType = DRAW_SPHERE;
	//LoadGeometry( Path::MakeFullPath(EDITOR_OBJECTS_PATH+"MtlSphere.cgf") );
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnDrawTeapot()
{
	if (m_drawType == DRAW_TEAPOT)
		return;
	m_drawType = DRAW_TEAPOT;
	//LoadGeometry( Path::MakeFullPath(EDITOR_OBJECTS_PATH+"MtlTeapot.cgf") );
}

//@FIXME
/*
//////////////////////////////////////////////////////////////////////////
void CParticleDialog::LoadGeometry( const CString &filename )
{
	m_geometryFile = filename;
	ReleaseGeometry();
	m_bOwnGeometry = true;
	m_pGeometry = GetIEditor()->Get3DEngine()->LoadStatObj( m_geometryFile );
	if (m_pGeometry)
	{
		m_pRenderNode = GetIEditor()->Get3DEngine()->CreateRenderNode();
		m_pRenderNode->SetEntityStatObj( 0,m_pGeometry );
		m_previewCtrl.SetEntity( m_pRenderNode );
		AssignMtlToGeometry();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::ReleaseGeometry()
{
	//@FIXME
	m_previewCtrl.SetEntity(0);
	if (m_pRenderNode)
	{
		GetIEditor()->Get3DEngine()->DeleteRenderNode(m_pRenderNode);
		m_pRenderNode = 0;
	}
	if (m_pGeometry)
	{
		// Load test geometry.
		GetIEditor()->Get3DEngine()->ReleaseObject( m_pGeometry );
	}
	m_pGeometry = 0;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::AssignMtlToGeometry()
{
	if (!m_pRenderNode)
		return;

	CParticleItem *pParticles = GetSelectedParticle();
	if (!pParticles)
		return;

	pParticles->AssignToEntity( m_pRenderNode );
}
	*/

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnAssignParticleToSelection()
{
	CParticleItem *pParticles = GetSelectedParticle();
	if (!pParticles)
		return;

	CUndo undo( "Assign ParticleEffect" );

	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	if (!pSel->IsEmpty())
	{
		for (int i = 0; i < pSel->GetCount(); i++)
		{
			AssignParticleToEntity( pParticles,pSel->GetObject(i) );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnActivateAll()
{
	CParticleItem *pParticles = GetSelectedParticle();
	if (!pParticles)
		return;

	CUndo undo( "Activate All Particle Tree" );

	pParticles->DebugEnable();

	// Update variables.
	m_propsCtrl.EnableUpdateCallback(false);
	pParticleUI->SetFromParticles( pParticles );
	m_propsCtrl.EnableUpdateCallback(true);

	m_propsCtrl.SetUpdateCallback( functor(*this,&CParticleDialog::OnUpdateProperties) );
	m_propsCtrl.EnableUpdateCallback(true);

	UpdateItemState(pParticles, true);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnSelectAssignedObjects()
{
	//@FIXME
	/*
	CParticleItem *pParticles = GetSelectedParticle();
	if (!pParticles)
		return;

	CBaseObjectsArray objects;
	GetIEditor()->GetObjectManager()->GetObjects( objects );
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject *pObject = objects[i];
		if (pObject->GetParticle() != pParticles)
			continue;
		if (pObject->IsHidden() || pObject->IsFrozen())
			continue;
		GetIEditor()->GetObjectManager()->SelectObject( pObject );
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnResetParticleOnSelection()
{
	CUndo undo( "Reset Particle" );

	//@FIXME
	/*
	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	if (!pSel->IsEmpty())
	{
		for (int i = 0; i < pSel->GetCount(); i++)
		{
			pSel->GetObject(i)->SetParticle( 0 );
		}
	}
	*/
}

CEntity* CParticleDialog::GetItemFromEntity()
{
	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject* pObject = pSel->GetObject(i);
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			CEntity *pEntity = (CEntity*)pObject;
			if (pEntity->GetProperties())
			{
				IVariable *pVar = pEntity->GetProperties()->FindVariable( "ParticleEffect" );
				if (pVar)
				{
					CString effect;
					pVar->Get(effect);
					{
						CBaseLibraryItem* pItem = (CBaseLibraryItem*)m_pItemManager->LoadItemByName(effect);
						if (pItem)
						{
							SelectItem(pItem);
							return pEntity;
						}
					}
				}
			}
		}
	}
	return NULL;
}

void CParticleDialog::OnGetParticleFromSelection()
{
	GetItemFromEntity();
}

void CParticleDialog::OnEntityStart()
{
}

void CParticleDialog::OnEntityStop()
{
}

//////////////////////////////////////////////////////////////////////////
bool CParticleDialog::AssignParticleToEntity( CParticleItem *pItem, CBaseObject *pObject, Vec3 const* pPos )
{
	// Assign ParticleEffect field if it has one.
	// Otherwise, spawn/attach an emitter to the entity
	assert(pItem);
	assert(pObject);
	if (pObject->IsKindOf(RUNTIME_CLASS(CEntity)))
	{
		CEntity *pEntity = (CEntity*)pObject;
		IVariable *pVar = 0;
		if (pEntity->GetProperties())
		{
			pVar = pEntity->GetProperties()->FindVariable( "ParticleEffect" );
			if (pVar && pVar->GetType() == IVariable::STRING)
				// Set selected entity's ParticleEffect field.
				pVar->Set( pItem->GetFullName() );
			else
			{
				// Create a new ParticleEffect entity on top of selected entity, attach to it.
				Vec3 pos;
				if (pPos)
					pos = *pPos;
				else
				{
					pos = pObject->GetPos();
					BBox box;
					pObject->GetLocalBounds(box);
					pos.z += box.max.z;
				}

				CreateParticleEntity( pItem, pos, pObject );
			}
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CParticleDialog::CreateParticleEntity( CParticleItem *pItem, Vec3 const& pos, CBaseObject* pParent )
{
	GetIEditor()->ClearSelection();
	CBaseObject* pObject = GetIEditor()->NewObject( "Entity", "ParticleEffect" );
	if (pObject)
	{
		// Set pos, offset by object size.
		BBox box;
		pObject->GetLocalBounds(box);
		pObject->SetPos(pos - Vec3(0,0,box.min.z));
		pObject->SetRotation( Quat::CreateRotationXYZ(Ang3(DEG2RAD(90),0,0)) );

		if (pParent)
			pParent->AttachChild( pObject );
		AssignParticleToEntity( pItem, pObject );
		GetIEditor()->SelectObject( pObject );
	}
	return pObject;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnAddSubItem()
{
	CParticleItem *pParticles = GetSelectedParticle();
	if (!pParticles)
		return;

	//if (pParticles->GetParent())
		//pParticles = pParticles->GetParent();

	CUndo undo( "Add Sub Particle" );

	CParticleItem *pSubItem = (CParticleItem*)m_pItemManager->CreateItem( m_pLibrary );
	pSubItem->SetDefaults();

	pParticles->AddChild( pSubItem );
	pSubItem->SetName( m_pItemManager->MakeUniqItemName(pParticles->GetName()) );
	pSubItem->Update();

	ReloadItems();
	SelectItem( pSubItem );
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnDelSubItem()
{
	CParticleItem *pSubItem = GetSelectedParticle();
	if (!pSubItem)
		return;

	CUndo undo( "Remove Sub Particle" );
	CParticleItem *pParticles = pSubItem->GetParent();
	if (pParticles)
	{
		pParticles->RemoveChild(pSubItem);
		m_pItemManager->DeleteItem( pSubItem );

		ReloadItems();
		SelectItem( pParticles );
	}
}


void CParticleDialog::UpdateItemState( CParticleItem* pItem, bool bRecursive )
{
	HTREEITEM hItem = stl::find_in_map( m_itemsToTree,pItem,(HTREEITEM)0 );
	if (hItem)
	{
		//m_treeCtrl.SetItemState(hItem, pItem->GetEnabledState() ? TVIS_BOLD : 0, TVIS_BOLD);

		// Swap icon set, depending on self & child activation.
		int nEnabled = pItem->GetEnabledState();
		if (nEnabled & 1)
			m_treeCtrl.SetItemImage(hItem, 2, 3);
		else if (nEnabled & 2)
			m_treeCtrl.SetItemImage(hItem, 4, 5);
		else
			m_treeCtrl.SetItemImage(hItem, 6, 7);
	}
	if (bRecursive)
		for (int i = 0; i < pItem->GetChildCount(); i++)
			UpdateItemState(pItem->GetChild(i), true);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdateAssignMtlToSelection( CCmdUI* pCmdUI )
{
	if (GetSelectedParticle() && !GetIEditor()->GetSelection()->IsEmpty())
	{
		pCmdUI->Enable( TRUE );
	}
	else
	{
		pCmdUI->Enable( FALSE );
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdateObjectSelected( CCmdUI* pCmdUI )
{
	if (!GetIEditor()->GetSelection()->IsEmpty())
	{
		pCmdUI->Enable( TRUE );
	}
	else
	{
		pCmdUI->Enable( FALSE );
	}
}

void CParticleDialog::OnUpdateConvertFromEntity( CCmdUI* pCmdUI )
{
	if (!GetIEditor()->GetSelection()->IsEmpty())
	{
		pCmdUI->Enable( TRUE );
	}
	else
	{
		pCmdUI->Enable( FALSE );
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;

	CParticleItem* pParticles = (CParticleItem*)m_treeCtrl.GetItemData(hItem);
	if (!pParticles)
		return;

	m_pDraggedMtl = pParticles;

	m_treeCtrl.Select( hItem,TVGN_CARET );

	m_hDropItem = 0;
	m_dragImage = m_treeCtrl.CreateDragImage( hItem );
	if (m_dragImage)
	{
		m_hDraggedItem = hItem;
		m_hDropItem = hItem;
		m_dragImage->BeginDrag(0, CPoint(-10, -10));

		CRect rc;
		AfxGetMainWnd()->GetWindowRect( rc );
		
		CPoint p = pNMTreeView->ptDrag;
		ClientToScreen( &p );
		p.x -= rc.left;
		p.y -= rc.top;
		
		m_dragImage->DragEnter( AfxGetMainWnd(),p );
		SetCapture();
		GetIEditor()->EnableUpdate( false );
	}
	
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragImage)
	{
		CPoint p;

		p = point;
		ClientToScreen( &p );
		m_treeCtrl.ScreenToClient( &p );

		TVHITTESTINFO hit;
		ZeroStruct(hit);
		hit.pt = p;
		HTREEITEM hHitItem = m_treeCtrl.HitTest( &hit );
		if (hHitItem)
		{
			if (m_hDropItem != hHitItem)
			{
				if (m_hDropItem)
					m_treeCtrl.SetItem( m_hDropItem,TVIF_STATE,0,0,0,0,TVIS_DROPHILITED,0 );
				// Set state of this item to drop target.
				m_treeCtrl.SetItem( hHitItem,TVIF_STATE,0,0,0,TVIS_DROPHILITED,TVIS_DROPHILITED,0 );
				m_hDropItem = hHitItem;
				m_treeCtrl.Invalidate();
			}
		}

		CRect rc;
		AfxGetMainWnd()->GetWindowRect( rc );
		p = point;
		ClientToScreen( &p );
		p.x -= rc.left;
		p.y -= rc.top;
		m_dragImage->DragMove( p );

		SetCursor( m_hCursorDefault );
		// Check if can drop here.
		{
			CPoint p;
			GetCursorPos( &p );
			CViewport* viewport = GetIEditor()->GetViewManager()->GetViewportAtPoint( p );
			if (viewport)
			{
				CPoint vp = p;
				viewport->ScreenToClient(&vp);
				HitContext hit;
				if (viewport->HitTest( vp,hit ))
				{
					if (hit.object && hit.object->IsKindOf(RUNTIME_CLASS(CEntity)))
					{
						SetCursor( m_hCursorReplace );
					}
				}
			}
		}
	}

	CBaseLibraryDialog::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	//CXTResizeDialog::OnLButtonUp(nFlags, point);

	if (m_hDropItem)
	{
		m_treeCtrl.SetItem( m_hDropItem,TVIF_STATE,0,0,0,0,TVIS_DROPHILITED,0 );
		m_hDropItem = 0;
	}

	if (m_dragImage)
	{
		CPoint p;
		GetCursorPos( &p );

		GetIEditor()->EnableUpdate( true );

		m_dragImage->DragLeave( AfxGetMainWnd() );
		m_dragImage->EndDrag();
		delete m_dragImage;
		m_dragImage = 0;
		ReleaseCapture();

		CPoint treepoint = p;
		m_treeCtrl.ScreenToClient( &treepoint );

		CRect treeRc;
		m_treeCtrl.GetClientRect(treeRc);

		if (treeRc.PtInRect(treepoint))
		{
			// Droped inside tree.
			TVHITTESTINFO hit;
			ZeroStruct(hit);
			hit.pt = treepoint;
			HTREEITEM hHitItem = m_treeCtrl.HitTest( &hit );
			if (hHitItem)
			{
				DropToItem( hHitItem,m_hDraggedItem,m_pDraggedMtl );
				m_hDraggedItem = 0;
				m_pDraggedMtl = 0;
				return;
			}
			DropToItem( 0,m_hDraggedItem,m_pDraggedMtl );
		}
		else
		{
			// Not droped inside tree.

			CWnd *wnd = WindowFromPoint( p );

			CUndo undo( "Assign ParticleEffect" );

			CViewport* viewport = GetIEditor()->GetViewManager()->GetViewportAtPoint( p );
			if (viewport)
			{
				CPoint vp = p;
				viewport->ScreenToClient(&vp);
				CParticleItem *pParticles = m_pDraggedMtl;

				// Drag and drop into one of views.
				HitContext  hit;
				if (viewport->HitTest( vp,hit ))
				{
					Vec3 hitpos = hit.raySrc + hit.rayDir * hit.dist;
					if (hit.object && AssignParticleToEntity( pParticles, hit.object, &hitpos ))
						; // done
					else
					{
						// Place emitter at hit location.
						hitpos = viewport->SnapToGrid(hitpos);
						CreateParticleEntity(pParticles, hitpos);
					}
				}
				else
				{
					// Snap to terrain.
					bool hitTerrain;
					Vec3 pos = viewport->ViewToWorld( vp,&hitTerrain );
					if (hitTerrain)
					{
						pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y);
					}
					pos = viewport->SnapToGrid(pos);
					CreateParticleEntity(pParticles, pos);
				}
			}
		}
		m_pDraggedMtl = 0;
	}
	m_pDraggedMtl = 0;
	m_hDraggedItem = 0;

	CBaseLibraryDialog::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Show helper menu.
	CPoint point;

	CParticleItem *pParticles = 0;

	// Find node under mouse.
	GetCursorPos( &point );
	m_treeCtrl.ScreenToClient( &point );
	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(point,&uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pParticles = (CParticleItem*)m_treeCtrl.GetItemData(hItem);
	}

	if (!pParticles)
		return;

	SelectItem( pParticles );

	// Create pop up menu.
	CMenu menu;
	menu.CreatePopupMenu();
	
	if (pParticles)
	{
		CClipboard clipboard;
		bool bNoPaste = clipboard.IsEmpty();
		int pasteFlags = 0;
		if (bNoPaste)
			pasteFlags |= MF_GRAYED;

		menu.AppendMenu( MF_STRING,ID_DB_CUT,"Cut" );
		menu.AppendMenu( MF_STRING,ID_DB_COPY,"Copy" );
		menu.AppendMenu( MF_STRING|pasteFlags,ID_DB_PASTE,"Paste" );
		menu.AppendMenu( MF_STRING,ID_DB_CLONE,"Clone" ); 
		menu.AppendMenu( MF_SEPARATOR,0,"" );
		menu.AppendMenu( MF_STRING,ID_DB_RENAME,"Rename" );
		menu.AppendMenu( MF_STRING,ID_DB_REMOVE,"Delete" );
		menu.AppendMenu( MF_SEPARATOR,0,"" );
		menu.AppendMenu( MF_STRING,ID_DB_MTL_ACTIVATE_ALL,"Enable/Disable All" );
		menu.AppendMenu( MF_STRING,ID_DB_ASSIGNTOSELECTION,"Assign to Selected Objects" );
		/*
		menu.AppendMenu( MF_SEPARATOR,0,"" );
		menu.AppendMenu( MF_STRING,ID_DB_PLAY, "Start on Selected Entity" );
		menu.AppendMenu( MF_STRING,ID_DB_STOP, "Stop on Selected Entity" );
		*/
	}

	GetCursorPos( &point );
	menu.TrackPopupMenu( TPM_LEFTALIGN|TPM_LEFTBUTTON,point.x,point.y,this );
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnNotifyTreeLClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = FALSE;
	
	// Show helper menu.
	CPoint point;

	CParticleItem *pParticles = 0;

	// Find node under mouse.
	GetCursorPos( &point );
	m_treeCtrl.ScreenToClient( &point );
	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(point,&uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pParticles = (CParticleItem*)m_treeCtrl.GetItemData(hItem);
	}

	if (pParticles)
		m_previewCtrl.LoadParticleEffect( pParticles->GetEffect() );
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnPickMtl()
{
	if (!CParticlePickCallback::IsActive())
		GetIEditor()->PickObject( new CParticlePickCallback,0,"Pick Object to Select Particle" );
	else
		GetIEditor()->CancelPick();
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnUpdatePickMtl( CCmdUI* pCmdUI )
{
	if (CParticlePickCallback::IsActive())
	{
		pCmdUI->SetCheck(1);
	}
	else
	{
		pCmdUI->SetCheck(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnCopy()
{
	CParticleItem *pParticles = GetSelectedParticle();
	if (pParticles)
	{
		XmlNodeRef node = CreateXmlNode( "Particles" );
		CBaseLibraryItem::SerializeContext ctx(node,false);
		ctx.bCopyPaste = true;

		CClipboard clipboard;
		pParticles->Serialize( ctx );
		clipboard.Put( node );
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::OnPaste()
{
	if (!m_pLibrary)
		return;

	CParticleItem *pItem = GetSelectedParticle();
	if (!pItem)
		return;

	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return;

	if (strcmp(node->getTag(),"Particles") == 0)
	{
		node->delAttr( "Name" );

		m_pPartManager->PasteToParticleItem( pItem,node,true );
		ReloadItems();
		SelectItem(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleDialog::DropToItem( HTREEITEM hItem,HTREEITEM hSrcItem,CParticleItem *pParticles )
{
	pParticles->GetLibrary()->SetModified();

	TSmartPtr<CParticleItem> pHolder = pParticles; // Make usre its not release while we remove and add it back.

	if (!hItem)
	{
		// Detach from parent.
		if (pParticles->GetParent())
			pParticles->GetParent()->RemoveChild( pParticles );

		ReloadItems();
		SelectItem( pParticles );
		return;
	}

	CParticleItem* pTargetItem = (CParticleItem*)m_treeCtrl.GetItemData(hItem);
	if (!pTargetItem)
	{
		// This is group.
		
		// Detach from parent.
		if (pParticles->GetParent())
			pParticles->GetParent()->RemoveChild( pParticles );

		// Move item to different group.
		CString groupName = m_treeCtrl.GetItemText(hItem);
		SetItemName( pParticles,groupName,pParticles->GetShortName() );

		m_treeCtrl.DeleteItem( hSrcItem );
		InsertItemToTree( pParticles,hItem );
		return;
	}
	// Ignore itself or immidiate target.
	if (pTargetItem == pParticles || pTargetItem == pParticles->GetParent())
		return;



	// Detach from parent.
	if (pParticles->GetParent())
		pParticles->GetParent()->RemoveChild( pParticles );

	// Attach to target.
	pTargetItem->AddChild( pParticles );

	ReloadItems();
	SelectItem( pParticles );
}
