// AIPointPanel.cpp : implementation file
//
//		09/12/04 Kirill - added removable flag

#include "stdafx.h"
#include "AIPointPanel.h"

#include "Objects\AIPoint.h"
#include "IAgent.h"
#include ".\aipointpanel.h"

// CAIPointPanel dialog

IMPLEMENT_DYNCREATE(CAIPointPanel, CDialog)

CAIPointPanel::CAIPointPanel(CWnd* pParent /*=NULL*/)
	: CDialog(CAIPointPanel::IDD, pParent)
	, m_type(0)
  , m_navigationType(0)
	, m_removable(0)
{
	m_pickCallback.Init(this, false);
	m_pickImpassCallback.Init(this, true);
	m_object = 0;
	Create( IDD,pParent );
}

CAIPointPanel::~CAIPointPanel()
{
}

void CAIPointPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NODES, m_links);
	DDX_Control(pDX, IDC_REGENLINKS, m_regenLinksBtn);
	DDX_Control(pDX, IDC_PICK, m_pickBtn);
	DDX_Control(pDX, IDC_PICKIMPASS, m_pickImpassBtn);
	DDX_Control(pDX, IDC_SELECT, m_selectBtn);
	DDX_Control(pDX, IDC_REMOVE, m_removeBtn);
	
	DDX_Radio(pDX, IDC_WAYPOINT, m_type);
	DDX_Radio(pDX, IDC_HUMAN, m_navigationType);
	DDX_Check(pDX, IDC_REMOVABLE, m_removable);

}

BOOL CAIPointPanel::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_links.SetBkColor( RGB(0xE0,0xE0,0xE0) );
	
	m_pickBtn.SetPickCallback( &m_pickCallback, "Pick AIPoint to Link",0 );
	m_pickImpassBtn.SetPickCallback( &m_pickImpassCallback, "Pick AIPoint to Link Impass",0 );

	return TRUE;  // return TRUE  unless you set the focus to a control
}

BEGIN_MESSAGE_MAP(CAIPointPanel, CDialog)
	ON_BN_CLICKED(IDC_REGENLINKS, OnBnClickedRegenLinks)
	ON_BN_CLICKED(IDC_SELECT, OnBnClickedSelect)
	ON_BN_CLICKED(IDC_REMOVE, OnBnClickedRemove)
	ON_BN_CLICKED(IDC_REMOVEALL, OnBnClickedRemoveAll)
	ON_BN_CLICKED(IDC_REMOVEALLINAREA, OnBnClickedRemoveAllInArea)
	ON_LBN_DBLCLK(IDC_NODES, OnLbnDblclkLinks)
	ON_LBN_SELCHANGE(IDC_NODES, OnLbnLinksSelChange)
	ON_BN_CLICKED(IDC_WAYPOINT, OnBnClickedWaypoint)
	ON_BN_CLICKED(IDC_HIDEPOINT, OnBnClickedHidepoint)
	ON_BN_CLICKED(IDC_HIDESECONDARY, OnBnClickedHidepointSecondary)
	ON_BN_CLICKED(IDC_ENTRYPOINT, OnBnClickedEntrypoint)
	ON_BN_CLICKED(IDC_EXITPOINT, OnBnClickedExitpoint)
	ON_BN_CLICKED(IDC_REMOVABLE, OnBnClickedRemovable)
  ON_BN_CLICKED(IDC_HUMAN, OnBnClickedHuman)
  ON_BN_CLICKED(IDC_3DSURFACE, OnBnClicked3dsurface)
END_MESSAGE_MAP()


void CAIPointPanel::SetObject( CAIPoint *object )
{
	if (m_object)
	{
		for (int i = 0; i < m_object->GetLinkCount(); i++)
			m_object->SelectLink(i,false);
	}

	assert( object );
	m_object = object;
	switch (object->GetAIType())
	{
	case EAIPOINT_WAYPOINT:
		m_type = 0;
		break;
	case EAIPOINT_HIDE:
		m_type = 1;
		break;
	case EAIPOINT_HIDESECONDARY:
		m_type = 2;
		break;
	case EAIPOINT_ENTRYEXIT:
		m_type = 3;
		break;
	case EAIPOINT_EXITONLY:
		m_type = 4;
		break;
	}

	switch (object->GetAINavigationType())
	{
  case EAINAVIGATION_HUMAN:
		m_navigationType = 0;
		break;
  case EAINAVIGATION_3DSURFACE:
		m_navigationType = 1;
		break;
	}

  m_removable = int(m_object->GetRemovable());

	UpdateData(FALSE);
	ReloadLinks();
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::StartPick()
{
	// Simulate click on pick button.
	m_pickBtn.OnClicked();
}

void CAIPointPanel::StartPickImpass()
{
	// Simulate click on pick button.
	m_pickImpassBtn.OnClicked();
}

void CAIPointPanel::ReloadLinks()
{
	m_links.ResetContent();
	for (int i = 0; i < m_object->GetLinkCount(); i++)
	{
		CAIPoint *obj = m_object->GetLink(i);
		if (obj)
			m_links.AddString( obj->GetName() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedSelect()
{
	assert( m_object );
	int sel = m_links.GetCurSel();
	if (sel != LB_ERR)
	{
		CBaseObject *obj = m_object->GetLink(sel);
		if (obj)
		{
			GetIEditor()->ClearSelection();
			GetIEditor()->SelectObject( obj );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedRegenLinks()
{
	assert( m_object );
	m_object->RegenLinks();
}


//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedRemove()
{
	assert( m_object );
	int sel = m_links.GetCurSel();
	if (sel != LB_ERR)
	{
		CUndo undo( "Unlink AIPoint" );
		CAIPoint *obj = m_object->GetLink(sel);
		if (obj)
			m_object->RemoveLink( obj );
//		m_object->RegenLinks();
		ReloadLinks();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedRemoveAll()
{
	assert( m_object );
	m_object->RemoveAllLinks();
//	m_object->RegenLinks();
	CUndo undo( "Unlink AIPoint (all)" );
	ReloadLinks();
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedRemoveAllInArea()
{
  IGraph *aiGraph = GetIEditor()->GetSystem()->GetAISystem() ? GetIEditor()->GetSystem()->GetAISystem()->GetNodeGraph() : NULL;
  if (!aiGraph)
    return;

	assert( m_object );

	if (!m_object->GetGraphNode())
		return;
  if (!(aiGraph->GetNavType(m_object->GetGraphNode()) & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE)))
		return;
	int nBuildingID = aiGraph->GetBuildingIDFromWaypointNode(m_object->GetGraphNode());
	if (nBuildingID < 0)
		return;

	CBaseObjectsArray allObjects;
	GetIEditor()->GetObjectManager()->GetObjects(allObjects);
	std::vector<CAIPoint*> objectsInBuilding;

	unsigned nObjects = allObjects.size();
	for (unsigned i = 0 ; i < nObjects ; ++i)
	{
		CBaseObject* baseObj = allObjects[i];
		if (baseObj && baseObj->IsKindOf( RUNTIME_CLASS(CAIPoint) ))
		{
			CAIPoint* AIPoint = (CAIPoint*) baseObj;
      const GraphNode *pNode = AIPoint->GetGraphNode();
			if (!pNode)
				continue;
			if (!(aiGraph->GetNavType(pNode) & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE)))
				continue;
			if (aiGraph->GetBuildingIDFromWaypointNode(pNode) != nBuildingID)
				continue;

			objectsInBuilding.push_back(AIPoint);
		}
	}

	unsigned nObjectsInBuilding = objectsInBuilding.size();
	for (unsigned i = 0 ; i < nObjectsInBuilding ; ++i)
	{
		CUndo undo( "Unlink AIPoint (all in area)" );
		CAIPoint* obj = objectsInBuilding[i];
		obj->RemoveAllLinks();
	}
	if (nObjectsInBuilding > 0)
		objectsInBuilding[0]->RegenLinks();
	ReloadLinks();
}


//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnLbnDblclkLinks()
{
	// Select current entity.
	OnBnClickedSelect();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnPick( bool impass, CBaseObject *picked )
{
	assert( m_object );
	CUndo undo( "Link AIPoint" );
	m_object->AddLink( (CAIPoint*)picked, !impass );
//	m_object->RegenLinks();
	ReloadLinks();

	// 
//	m_entityName.SetWindowText( picked->GetName() );
}

//////////////////////////////////////////////////////////////////////////
bool CAIPointPanel::OnPickFilter( bool impass, CBaseObject *picked )
{
	assert( picked != 0 );
	return picked != m_object && picked->IsKindOf( RUNTIME_CLASS(CAIPoint) );
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnCancelPick( bool impass )
{
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedWaypoint()
{
	assert( m_object );
	m_object->SetAIType( EAIPOINT_WAYPOINT );
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedHidepoint()
{
	assert( m_object );
	m_object->SetAIType( EAIPOINT_HIDE );
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedHidepointSecondary()
{
	assert( m_object );
	m_object->SetAIType( EAIPOINT_HIDESECONDARY );
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedEntrypoint()
{
	assert( m_object );
	m_object->SetAIType( EAIPOINT_ENTRYEXIT );
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedExitpoint()
{
	assert( m_object );
	m_object->SetAIType( EAIPOINT_EXITONLY );
}


//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnLbnLinksSelChange()
{
	assert( m_object );
	int sel = m_links.GetCurSel();
	if (sel != LB_ERR)
	{
		// Unselect all others.
		for (int i = 0; i < m_object->GetLinkCount(); i++)
		{
			if (sel == i)
				m_object->SelectLink(i,true);
			else
				m_object->SelectLink(i,false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedRemovable()
{
	m_object->MakeRemovable( !m_object->GetRemovable() );
}


//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClickedHuman()
{
	assert( m_object );
  m_object->SetAINavigationType( EAINAVIGATION_HUMAN );
}

//////////////////////////////////////////////////////////////////////////
void CAIPointPanel::OnBnClicked3dsurface()
{
	assert( m_object );
	m_object->SetAINavigationType( EAINAVIGATION_3DSURFACE );
}
