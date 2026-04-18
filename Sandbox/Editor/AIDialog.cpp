// AIDialog.cpp : implementation file
//

#include "stdafx.h"
#include "AIDialog.h"

#include "Controls\PropertyCtrl.h"

#include "AI\AIManager.h"
#include "AI\AIGoalLibrary.h"
#include "AI\AIBehaviorLibrary.h"

// CAIDialog dialog

IMPLEMENT_DYNAMIC(CAIDialog, CDialog)
CAIDialog::CAIDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CAIDialog::IDD, pParent)
{
	//m_propWnd = 0;
}

CAIDialog::~CAIDialog()
{
}

void CAIDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ANCHORS, m_behaviors);
	DDX_Control(pDX, IDC_DESCRIPTION, m_description);
}


BEGIN_MESSAGE_MAP(CAIDialog, CDialog)
	ON_BN_CLICKED(IDEDIT, OnBnClickedEdit)
	ON_BN_CLICKED(IDREFRESH, OnBnClickedReload)
	ON_LBN_DBLCLK(IDC_ANCHORS, OnLbnDblClk)
	ON_LBN_SELCHANGE(IDC_ANCHORS, OnLbnSelchange)
END_MESSAGE_MAP()


// CAIDialog message handlers

void CAIDialog::OnLbnDblClk()
{
	if ( m_behaviors.GetCurSel() >= 0 )
		EndDialog( IDOK );
}

BOOL CAIDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText( "AI Behaviors" );
	GetDlgItem( IDC_NEW )->EnableWindow( FALSE );
	GetDlgItem( IDDELETE )->EnableWindow( FALSE );
	SetDlgItemText( IDC_LISTCAPTION, "&Choose AI behavior:" );

	ReloadBehaviors();
	/*
	XmlNodeRef root("Root");
	CAIGoalLibrary *lib = GetIEditor()->GetAI()->GetGoalLibrary();
	std::vector<CAIGoalPtr> goals;
	lib->GetGoals( goals );
	for (int i = 0; i < goals.size(); i++)
	{
		CAIGoal *goal = goals[i];
		XmlNodeRef goalNode = root->newChild(goal->GetName());
		goalNode->setAttr("type","List");
		
		// Copy Goal params.
		if (goal->GetParamsTemplate())
		{
			XmlNodeRef goalParams = goal->GetParamsTemplate()->clone();
			for (int k = 0; k < goalParams->getChildCount(); k++)
			{
				goalNode->addChild(goalParams->getChild(k));
			}
		}
		for (int j = 0; j < goal->GetStageCount(); j++)
		{
			CAIGoalStage &stage = goal->GetStage(j);
			CAIGoal *stageGoal = lib->FindGoal(stage.name);
			if (stageGoal)
			{
				XmlNodeRef stageNode = goalNode->newChild(stage.name);
				stageNode->setAttr("type","List");
				//if (stage.params)
				{
					XmlNodeRef templ = stageGoal->GetParamsTemplate()->clone();
					//CXmlTemplate::GetValues( templ,stage.params );
					for (int k = 0; k < templ->getChildCount(); k++)
					{
						stageNode->addChild(templ->getChild(k));
					}
				}
			}
		}
	}

	XmlParser parser;
	root = parser.parse( "C:\\MasterCD\\ai.xml" );

	CRect rc( 10,10,300,500 );
	m_propWnd = new CPropertiesWindow( root,"Props","Props" );
	m_propWnd->Resizable = false;
	m_propWnd->OpenChildWindow( GetSafeHwnd(),rc,true );
	m_propWnd->ForceRefresh();
	m_propWnd->Show(1);
	*/

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CAIDialog::ReloadBehaviors()
{
	CAIBehaviorLibrary *lib = GetIEditor()->GetAI()->GetBehaviorLibrary();
	std::vector<CAIBehaviorPtr> behaviors;
	lib->GetBehaviors( behaviors );
	for (int i = 0; i < behaviors.size(); i++)
	{
		m_behaviors.AddString( behaviors[i]->GetName() );
	}
	m_behaviors.SelectString( -1,m_aiBehavior );

	CAIBehavior *behavior = GetIEditor()->GetAI()->GetBehaviorLibrary()->FindBehavior( m_aiBehavior );
	if (behavior)
	{
		m_description.SetWindowText( behavior->GetDescription() );
	}
	else
	{
		m_description.SetWindowText( "" );
	}
}

void CAIDialog::SetAIBehavior( const CString &behavior )
{
	m_aiBehavior = behavior;
}

void CAIDialog::OnBnClickedEdit()
{
	CAIBehavior *behavior = GetIEditor()->GetAI()->GetBehaviorLibrary()->FindBehavior( m_aiBehavior );
	if (!behavior)
		return;

	behavior->Edit();
}

void CAIDialog::OnBnClickedReload()
{
	CAIBehavior *behavior = GetIEditor()->GetAI()->GetBehaviorLibrary()->FindBehavior( m_aiBehavior );
	if (!behavior)
		return;
	behavior->ReloadScript();
}

void CAIDialog::OnLbnSelchange()
{
	SetDlgItemText( IDCANCEL, "Cancel" );
	GetDlgItem( IDOK )->EnableWindow( TRUE );

	int sel = m_behaviors.GetCurSel();
	if (sel == LB_ERR)
		return;
	m_behaviors.GetText( sel,m_aiBehavior );
	
	CAIBehavior *behavior = GetIEditor()->GetAI()->GetBehaviorLibrary()->FindBehavior( m_aiBehavior );
	if (behavior)
	{
		m_description.SetWindowText( behavior->GetDescription() );
	}
	else
	{
		m_description.SetWindowText( "" );
	}
}
