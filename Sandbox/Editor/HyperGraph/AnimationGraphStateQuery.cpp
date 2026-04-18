#include "stdafx.h"
#include "AnimationGraphStateQuery.h"
#include "AnimationGraphDialog.h"
#include "AnimationGraphUnitTester.h"
#include "IEntitySystem.h"
#include "IScriptSystem.h"

#define GROUP_ID_LOADING 1
#define GROUP_ID_CONDITIONS 2
#define GROUP_ID_STATES 3
#define GROUP_ID_PRESELECTION 4

#define ID_LOADANDCOMPILEGRAPH 61
#define ID_LOADGRAPH 62
#define ID_DOQUERY 63
#define ID_HIDEUPPERBODY 64
#define ID_SHOWUPPERBODY 65
#define ID_HIDERANKINGS 66
#define ID_SHOWRANKINGS 67
#define ID_RESETSELECTION 68
#define ID_RELOAD 69
#define ID_GRAPHLOADEDSTATUS 70
#define ID_UNITTESTS 71

#define ANIM_GRAPH_NAME_SUFFIX "-StateQuery"

#define XML_TAG_PRESELECTIONLISTS	"PreselectionLists"
#define XML_TAG_PRESELECTION		"Preselection"
#define XML_TAG_SELECTION			"Selection"
#define XML_ATTR_NAME				"Name"
#define XML_ATTR_TYPE				"Type"
#define XML_ATTR_INPUT				"Input"
#define XML_ATTR_VALUE				"Value"
#define PRESELECTION_TYPE_BASICS	"Basics"
#define PRESELECTION_TYPE_OTHER		"Other"


namespace
{

	int ShowQuickPopup( const std::vector<CString>& elems, CWnd * pWnd )
	{
		CMenu menu;
		menu.CreatePopupMenu();
		for (std::vector<CString>::const_iterator iter = elems.begin(); iter != elems.end(); ++iter)
			menu.AppendMenu( MF_STRING, iter - elems.begin() + 1, *iter );

		CPoint p;
		::GetCursorPos(&p);
		return menu.TrackPopupMenuEx( TPM_RETURNCMD|TPM_LEFTBUTTON|TPM_LEFTALIGN, p.x, p.y, pWnd, NULL ) - 1;
	}

	template <class H>
	class CPreselectionCallback : public Functor1<IVariable*>
	{
	public:
		CPreselectionCallback( H * pHost, void (H::*notification)() ) : Functor1<IVariable*>(thunk, pHost, 0, &notification, sizeof(notification)) {}

		static void thunk( const FunctorBase& ftor, IVariable * )
		{
			typedef void (H::*Notification)();
			Notification notification = *(Notification*)(void*)ftor.getMemFunc();
			H * callee = (H*)ftor.getCallee();
			(callee->*notification)();
		}
	};

	template <class T>
	class CSetCallback : public Functor1<IVariable*>
	{
	public:
		CSetCallback( T& value ) : Functor1<IVariable*>(thunk, &value, 0, 0, 0) {}

		static void thunk( const FunctorBase& ftor, IVariable * pVar )
		{
			pVar->Get( *(T*)ftor.getCallee() );
		}
	};

	template <class H>
	class CNotifyCallback : public Functor1<IVariable*>
	{
	public:
		CNotifyCallback( H * pHost, void (H::*notification)() ) : Functor1<IVariable*>(thunk, pHost, 0, &notification, sizeof(notification)) {}

		static void thunk( const FunctorBase& ftor, IVariable * )
		{
			typedef void (H::*Notification)();
			Notification notification = *(Notification*)(void*)ftor.getMemFunc();
			H * callee = (H*)ftor.getCallee();
			(callee->*notification)();
		}
	};

	template <class T>
	IVariablePtr VariableHelper( CVarBlockPtr varBlock, const CString& humanName, T& value )
	{
		IVariablePtr var = new CVariable<T>();
		var->Set(value);
		var->AddOnSetCallback( CSetCallback<T>(value) );
		var->SetHumanName(humanName);
		var->SetFlags(0);
		varBlock->AddVariable(var);
		return var;
	}

	class CCriteriaModeVariable : public CVariableEnum<int>
	{
	public:
		CCriteriaModeVariable( CVarBlock * pVB, CAGInputPtr pInput, CAnimationGraphStateQuery * pSQ ) : m_pVB(pVB), m_pInput(pInput), m_pSQ(pSQ)
		{
			AddEnumItem( "Any Value", eCT_AnyValue );
			AddEnumItem( "Specified Value", eCT_ExactValue );
			Set( pSQ->GetSelection( pInput ) );
			AddOnSetCallback( CNotifyCallback<CCriteriaModeVariable>(this, &CCriteriaModeVariable::Change) );		
		}

		void Init()
		{
			CVarBlockPtr pVB = m_pSQ->CreateCriteriaVariables( m_pInput );

			for (int i=0; i<pVB->GetVarsCount(); i++)
				m_pVB->AddVariable( pVB->GetVariable(i) );
		}

	private:
		void Change()
		{
			int temp;
			Get(temp);
			ECriteriaType critType = (ECriteriaType) temp;

			// now set the criteria
			m_pSQ->SetSelection( m_pInput, critType );

			// and rebuild the panel
			m_pSQ->OnCommand( ID_RELOAD );
		}

		CVarBlock * m_pVB;
		CAGInputPtr m_pInput;
		CAnimationGraphStateQuery * m_pSQ;
	};

	void VariableHelper_Criteria( CVarBlockPtr varBlock, const CString& humanName, CAGInputPtr pInput, CAnimationGraphStateQuery * pSQ )
	{
		_smart_ptr<CCriteriaModeVariable> var = new CCriteriaModeVariable(varBlock, pInput, pSQ);
		var->SetHumanName(humanName);
		varBlock->AddVariable(var);
		var->Init();
	}
}

/******************
CAnimationGraphStateQuery::CPropGroup
******************/

CAnimationGraphStateQuery::CPropGroup::CPropGroup( CAnimationGraphStateQuery * pParent, const CString& name, CXTPTaskPanelGroup *group, const TAGPropMap& props )
{
	m_pParent = pParent;
	m_pItem = 0;
	m_pProps = 0;
	m_sName = name;

	// create the property editor
	CRect rc(0,0,220,500);
	m_pProps = new CPropertyCtrl();
	m_pProps->Create( WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, rc, m_pParent );
	m_pProps->ModifyStyle( 0, WS_CLIPCHILDREN|WS_CLIPSIBLINGS );
	m_pProps->SetItemHeight(16);
	m_pProps->ExpandAll();
	
	Reset( props, group );
}

void CAnimationGraphStateQuery::CPropGroup::Reset( const TAGPropMap& props, CXTPTaskPanelGroup *group )
{
	m_pGroup = group;
	if ( !m_pItem )
	{
		m_pItem = m_pGroup->AddControlItem( m_pProps->GetSafeHwnd() );
		m_pItem->SetCaption( m_sName );
	}

	//UpdateLock updateLock(m_pParent);

	m_pProps->RemoveAllItems();

	for (TAGPropMap::const_iterator iter = props.begin(); iter != props.end(); ++iter)
	{
		if (iter->first == "")
			m_pProps->AddVarBlock( iter->second );
		else
			m_pProps->AddVarBlock( iter->second, iter->first );
	}
	m_pProps->ExpandAll();

	// get things positioned correctly
	CRect rc;
	m_pParent->ExpandGroup( m_pGroup, FALSE );
	m_pParent->GetClientRect(rc);
	int h = m_pProps->GetVisibleHeight();
	m_pProps->SetWindowPos( NULL, 0, 0, rc.right, h+1, SWP_NOMOVE );
	m_pItem->SetControlHandle( m_pProps->GetSafeHwnd() );
	m_pParent->Reposition(FALSE);
	m_pParent->ExpandGroup( m_pGroup, TRUE );
}

CAnimationGraphStateQuery::CPropGroup::~CPropGroup()
{
	if ( m_pItem )
	{
		m_pItem->Remove();
	}
	SAFE_DELETE( m_pProps );
}


/******************
CAnimationGraphStateQuery::UpdateLock
******************/

CAnimationGraphStateQuery::UpdateLock::UpdateLock( CAnimationGraphStateQuery * pSQ ) : m_pSQ(pSQ), m_pos(0)
{
	if (1 == ++m_pSQ->m_iUpdateLock)
	{
		if (m_pSQ->GetSafeHwnd())
		{
			m_pSQ->GetParent()->LockWindowUpdate();

			SCROLLINFO si;
			si.cbSize = sizeof( SCROLLINFO );
			m_pSQ->GetScrollInfo( SB_VERT, &si, SIF_POS );
			m_pos = si.nPos;
		}
	}
}

CAnimationGraphStateQuery::UpdateLock::~UpdateLock()
{
	if (0 == --m_pSQ->m_iUpdateLock)
	{
		if (m_pSQ->GetSafeHwnd())
		{
			m_pSQ->GetParent()->UnlockWindowUpdate();

			SCROLLINFO si;
			si.cbSize = sizeof( SCROLLINFO );
			si.fMask = SIF_POS;
			si.nPos = m_pos;
			m_pSQ->SetScrollInfo( SB_VERT, &si );

			// hack to hide buggy behavior of the panel
			m_pSQ->SendMessage( WM_VSCROLL, (m_pos<<16)|SB_THUMBTRACK, NULL );
		}
	}
}


/******************
CAnimationGraphStateQuery
******************/


CAnimationGraphStateQuery::CAnimationGraphStateQuery() : 
	m_pParent( NULL ), 
	m_pGroupLoading( NULL ), 
	m_pGroupConditions( NULL ), 
	m_pGroupStates( NULL ), 
	m_pGroupPreselection( NULL ),
	m_pPreselectionLists( NULL ),
	m_pCriteria( NULL ),
	m_pPreselections( NULL ),
	m_preselections( NULL ),
	m_iUpdateLock( 0 ),
	m_bGraphLoaded( false ),
	m_bGraphChangedSinceSaved( false ),
	m_bHideUpperBodyResults( true ),
	m_bHideRankings( false ),
	m_pResults( NULL )
{
}

CAnimationGraphStateQuery::~CAnimationGraphStateQuery()
{
	if ( m_pResults )
	{
		m_pResults = NULL;
	}
}

void CAnimationGraphStateQuery::Init( CAnimationGraphDialog * pParent )
{
	m_pParent = pParent;
	
	CRect rc(0,0,350,500);
	Create(WS_CHILD|/*WS_VISIBLE|*/WS_CLIPSIBLINGS|WS_CLIPCHILDREN, rc, pParent, 0);
	SetBehaviour(xtpTaskPanelBehaviourExplorer);
	SetTheme(xtpTaskPanelThemeNativeWinXP);
	SetSelectItemOnFocus(TRUE);
	AllowDrag(FALSE);
	SetAnimation( xtpTaskPanelAnimationNo );
	GetPaintManager()->m_rcGroupOuterMargins.SetRect(0,0,0,0);
	GetPaintManager()->m_rcGroupInnerMargins.SetRect(0,0,0,0);
	GetPaintManager()->m_rcItemOuterMargins.SetRect(0,0,0,0);
	GetPaintManager()->m_rcItemInnerMargins.SetRect(1,1,1,1);
	GetPaintManager()->m_rcControlMargins.SetRect(2,0,2,0);
	GetPaintManager()->m_nGroupSpacing = 0;
}

void CAnimationGraphStateQuery::Reload( bool bGraphChangedSinceSaved )
{
	UpdateLock updateLock(this);

	CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();
	if ( pGraph )
	{
		m_bGraphChangedSinceSaved = bGraphChangedSinceSaved;

		CString sWorkingName = GetGraphWorkingName();
		m_bGraphLoaded = ( GetISystem()->GetIAnimationGraphSystem()->IsAnimationGraphLoaded( sWorkingName ) &&
			0==m_sCurrentGraphWorkingName.Compare( sWorkingName ) );
		m_sCurrentGraphWorkingName = sWorkingName;

		ReloadGroupLoading();
		ReloadGroupPreselection();
		ReloadGroupConditions();
		ReloadGroupStates();
	}
}

void CAnimationGraphStateQuery::Reload()
{
	Reload( m_bGraphChangedSinceSaved );
}

void CAnimationGraphStateQuery::ReloadGroupLoading()
{
	if ( !m_pGroupLoading )
	{
		m_pGroupLoading = AddGroup(GROUP_ID_LOADING);
		m_pGroupLoading->SetCaption( "Loading Into Engine" );
		m_pGroupLoading->SetItemLayout(xtpTaskItemLayoutDefault);

		AddVerb( m_pGroupLoading, ID_LOADANDCOMPILEGRAPH, "Compile and load graph" );
		AddVerb( m_pGroupLoading, ID_LOADGRAPH, "Load last saved version" );
		AddVerb( m_pGroupLoading, ID_UNITTESTS, "Run unit tests" );
	}

	AddGroupLoadingText();
}

void CAnimationGraphStateQuery::ReloadGroupConditions()
{
	if ( !m_pGroupConditions )
	{
		m_pGroupConditions = AddGroup(GROUP_ID_CONDITIONS);
		m_pGroupConditions->SetCaption( "Selection Criteria" );
		m_pGroupConditions->SetItemLayout(xtpTaskItemLayoutDefault);
	}
		
	if ( m_bGraphLoaded )
	{		
		AddVerb( m_pGroupConditions, ID_RESETSELECTION, "Reset all criteria\n" );

		BuildPropMap();

		if ( m_pCriteria.get() )
		{
			m_pCriteria->Reset( m_pMap, m_pGroupConditions );	
		}
		else
		{
			m_pCriteria = new CPropGroup( this, "Inputs", m_pGroupConditions, m_pMap );
		}
	}
	else
	{
		RemoveVerb( m_pGroupConditions, ID_RESETSELECTION );
		m_pCriteria = NULL;
		m_SelectionMap.clear();
	}
}

void CAnimationGraphStateQuery::ReloadGroupStates()
{
	// Always clear this group and start from scratch
	if ( m_pGroupStates )
	{
		m_pGroupStates->Remove();
	}

	m_pGroupStates = AddGroup(GROUP_ID_STATES);
	m_pGroupStates->SetCaption( "Search Results" );
	m_pGroupStates->SetItemLayout(xtpTaskItemLayoutDefault);
	
	if ( !m_bGraphLoaded )
	{
		if ( m_pResults )
		{
			m_pResults = NULL;
		}
	}

	int iNumResults = m_pResults ? m_pResults->GetNumResults() : 0;
	if ( m_bGraphLoaded )
	{
		AddVerb( m_pGroupStates, ID_DOQUERY, "Perform search\n" );

		if ( iNumResults == 0 )
		{
			AddText( m_pGroupStates, "\nNo matching states", true );
		}
		else
		{
			AddVerb( m_pGroupStates, ID_HIDEUPPERBODY, "Hide upper body variations" );
			AddVerb( m_pGroupStates, ID_SHOWUPPERBODY, "Show upper body variations" );
			SetVerbEnabled( m_pGroupStates, m_bHideUpperBodyResults ? ID_HIDEUPPERBODY : ID_SHOWUPPERBODY, false);
			AddVerb( m_pGroupStates, ID_HIDERANKINGS, "Hide rankings" );
			AddVerb( m_pGroupStates, ID_SHOWRANKINGS, "Show rankings" );
			SetVerbEnabled( m_pGroupStates, m_bHideRankings ? ID_HIDERANKINGS : ID_SHOWRANKINGS, false);

			CString sStateList;
			CString sWorking;
			for ( int i=0; i<iNumResults; ++i )
			{
				const IAnimationGraphQueryResults::IResult *pResult = m_pResults->GetResult( i );

				if ( m_bHideUpperBodyResults && 
						 string::npos != string( pResult->GetValue() ).find( "+" ) && 
						 string::npos == string( pResult->GetValue() ).find( "+Nothing" ) )
				{
					//This is an upper body variation, so skip it and decrement total results number
					--iNumResults;
				}
				else
				{
					string sState = pResult->GetValue();
					if ( m_bHideUpperBodyResults )
					{
						sState = pResult->GetValueNoMixIns();
					}

					if ( m_bHideRankings)
					{
						sWorking.Format( "%s\n", sState );
					}
					else
					{
						sWorking.Format( "%s : %d\n", sState, pResult->GetRanking() );
					}
					sStateList.Append( sWorking );
				}
			}
			sWorking.Format( "\n%d matching states:\n", iNumResults );
			AddText( m_pGroupStates, sWorking, true );
			AddText( m_pGroupStates, sStateList, true );
		}				
	}
}

void CAnimationGraphStateQuery::ReloadGroupPreselection()
{
	if ( !m_pGroupPreselection )
	{
		m_pGroupPreselection = AddGroup(GROUP_ID_PRESELECTION);
		m_pGroupPreselection->SetCaption( "Criteria Preselection" );
		m_pGroupPreselection->SetItemLayout(xtpTaskItemLayoutDefault);
	}

	if ( m_bGraphLoaded )
	{
		if (m_pPreselections.get())
		{
			// basically do nothing
		}
		else
		{
			// check if the lists need to be loaded (again - in case something changed)
			if ( !m_pPreselectionLists )
			{
				BuildPreselectionLists();
			}

			m_pPreselectionMap["Preselection Lists"] = m_pPreselectionLists;
			m_pPreselections = new CPropGroup( this, "Preselections", m_pGroupPreselection, m_pPreselectionMap );
		}
	}
	else
	{
		if (m_pPreselectionLists)
		{
			m_pPreselectionLists->Clear();
			m_pPreselectionLists = NULL;
		}
		m_pPreselections = NULL;
	}
}

void CAnimationGraphStateQuery::AddVerb( CXTPTaskPanelGroup *pGroup, int id, const CString& name )
{
	CXTPTaskPanelGroupItem * pGI = pGroup->FindItem( id );
	if ( !pGI )
	{
		CXTPTaskPanelGroupItem * pGI = pGroup->AddLinkItem( id );
		pGI->SetType( xtpTaskItemTypeLink );
		pGI->SetCaption( name );
	}
}

void CAnimationGraphStateQuery::RemoveVerb( CXTPTaskPanelGroup *pGroup, int id )
{
	CXTPTaskPanelGroupItem * pGI = pGroup->FindItem( id );
	if ( pGI )
	{
		pGI->Remove();
	}
}
void CAnimationGraphStateQuery::SetVerbEnabled( CXTPTaskPanelGroup *pGroup, int id, bool bEnabled )
{
	CXTPTaskPanelGroupItem * pGI = pGroup->FindItem( id );
	if ( pGI )
	{
		pGI->SetEnabled( bEnabled );
	}
}

void CAnimationGraphStateQuery::AddText( CXTPTaskPanelGroup *pGroup, const CString& name, bool bBold /*=false */ )
{
	CXTPTaskPanelGroupItem * pGI = pGroup->AddTextItem( name );
	pGI->SetBold( bBold );
	pGI->SetSize( CSize( pGI->GetSize().cx, pGI->GetSize().cy * 2 ) );
}

void CAnimationGraphStateQuery::AddModifiableText( CXTPTaskPanelGroup *pGroup, int id, const CString& name, bool bBold /*=false */ )
{
	CXTPTaskPanelGroupItem * pGI = pGroup->FindItem( id );
	if ( !pGI )
	{
		CXTPTaskPanelGroupItem * pGI = pGroup->AddLinkItem( id );
		pGI->SetType( xtpTaskItemTypeText );
		pGI->SetCaption( name );
		pGI->SetBold( bBold );
		pGI->SetSize( CSize( pGI->GetSize().cx, pGI->GetSize().cy * 2 ) );
	}	
}

void CAnimationGraphStateQuery::UpdateModifiableText( CXTPTaskPanelGroup *pGroup, int id, const CString& name, bool bBold /*=false */ )
{
	CXTPTaskPanelGroupItem * pGI = pGroup->FindItem( id );
	if ( pGI )
	{
		pGI->SetCaption( name );
		pGI->SetBold( bBold );
	}
	else
	{
		AddModifiableText( pGroup, id, name, bBold );
	}
}

void CAnimationGraphStateQuery::AddGroupLoadingText()
{
	CString sText = "\nStatus: Graph not loaded";
	CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();

	if (pGraph)
	{
		if ( m_bGraphLoaded )
		{
			if ( !m_bGraphChangedSinceSaved )
			{
				sText = "\nStatus: Graph loaded and current";
			}
			else
			{
				sText = "\nStatus: Graph loaded but not current";
			}
		}		
	}
	
	UpdateModifiableText( m_pGroupLoading, ID_GRAPHLOADEDSTATUS, sText, true );
}

void CAnimationGraphStateQuery::OnCommand( int cmd, bool bSkipConfirmations /*=false*/ )
{
	switch (cmd)
	{
	case ID_LOADANDCOMPILEGRAPH:
		{
			CString errorString;
			CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();

			if ( pGraph && ( bSkipConfirmations || IDYES == AfxMessageBox( "Compile and load current graph into the game engine?\n(may take a couple of minutes)\n" , MB_YESNO ) ) )
			{
				m_bGraphLoaded = false;

				XmlNodeRef graph = pGraph->ToXml();
				if (graph)
				{
					if ( GetISystem()->GetIAnimationGraphSystem()->LoadAnimationGraph( GetGraphWorkingName(), GetGraphFileName(), graph ) )
					{
						m_bGraphLoaded = true;
					}
					else
					{
						errorString = "Unable to load animation graph into game engine";
					}
				}
				else
				{
					errorString = "Unable to convert graph to XML";
				}
			}

			if (!errorString.IsEmpty())
				AfxMessageBox( errorString );

			Reload( false );
		}
		break;

	case ID_LOADGRAPH:
		{
			CString errorString;
			CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();

			if ( pGraph && ( bSkipConfirmations || IDYES == AfxMessageBox( "Load the last saved version of this graph into the game engine?\n(only takes a couple of seconds)\n" , MB_YESNO ) ) )
			{
				m_bGraphLoaded = false;

				if ( GetISystem()->GetIAnimationGraphSystem()->LoadAnimationGraph( GetGraphWorkingName(), GetGraphFileName(), NULL, true ) )
				{
					m_bGraphLoaded = true;
				}
				else
				{
					errorString = "Unable to load animation graph into game engine";
				}
			}

			if (!errorString.IsEmpty())
				AfxMessageBox( errorString );

			Reload();
		}
		break;
	
	case ID_DOQUERY:
		{
			DoQuery();
		}
		break;
	
	case ID_HIDEUPPERBODY:
		{
			if ( m_bHideUpperBodyResults == false )
			{
				m_bHideUpperBodyResults = true;
				Reload();
			}		
		}
		break;

	case ID_SHOWUPPERBODY:
		{
			if ( m_bHideUpperBodyResults == true )
			{
				m_bHideUpperBodyResults = false;
				Reload();
			}		
		}
		break;

	case ID_HIDERANKINGS:
		{
			if ( m_bHideRankings == false )
			{
				m_bHideRankings = true;
				Reload();
			}		
		}
		break;

	case ID_SHOWRANKINGS:
		{
			if ( m_bHideRankings == true )
			{
				m_bHideRankings = false;
				Reload();
			}		
		}
		break;

	case ID_RESETSELECTION:
		{
			m_SelectionMap.clear();
			ResetPreselections();
			Reload();
		}
		break;

	case ID_RELOAD:
		{
			Reload();
		}
		break;
	case ID_UNITTESTS:
		{
			RunUnitTests();
		}
		break;
	}
}

void CAnimationGraphStateQuery::DoQuery()
{
	CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();

	if ( pGraph && m_bGraphLoaded )
	{
		// Set inputs
		SAnimationGraphQueryInputs inputs;
		for ( TSelectionMap::iterator iter = m_SelectionMap.begin(); iter != m_SelectionMap.end(); ++iter )
		{
			SAnimationGraphQueryInputs::SInput sInput = SAnimationGraphQueryInputs::SInput( iter->first->GetName() );

			EAnimationGraphInputType eType = iter->first->GetType();
			if ( eType == eAGIT_Integer )
			{
				sInput.eType = SAnimationGraphQueryInputs::eAGQIT_Integer;
				sInput.iValue = iter->second.intValue;
			}
			else if ( eType == eAGIT_Float )
			{
				sInput.eType = SAnimationGraphQueryInputs::eAGQIT_Float;
				sInput.fValue = iter->second.floatValue;
			}
			else if ( eType == eAGIT_String )
			{
				sInput.eType = SAnimationGraphQueryInputs::eAGQIT_String;
				strcpy_s( sInput.sValue, iter->second.strValue );
			}
			
			sInput.bAnyValue = ( iter->second.type == eCT_AnyValue );

			inputs.AddInput( sInput );
		}
		
		// Do query
		m_pResults = GetISystem()->GetIAnimationGraphSystem()->QueryAnimationGraph( GetGraphWorkingName(), &inputs );

		// Display results
		Reload();
	}
}

void CAnimationGraphStateQuery::BuildPropMap()
{
	m_pMap.clear();
	CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();
	for ( CAnimationGraph::input_iterator iter = pGraph->InputBegin(); iter != pGraph->InputEnd(); ++iter )
	{
		CAGInputPtr pInput = *iter;

		CVarBlockPtr pVB = new CVarBlock();
		m_pMap[ pInput->GetName() ] = pVB;
		VariableHelper_Criteria( pVB, "Selection mode", pInput, this );
	}
}

void CAnimationGraphStateQuery::SetSelection( CAGInputPtr pInput, ECriteriaType eType )
{
	TSelectionMap::iterator iter;
	iter = m_SelectionMap.find(pInput);

	if ( iter != m_SelectionMap.end() )
	{
		m_SelectionMap[pInput].type = eType;
	}	
}

ECriteriaType CAnimationGraphStateQuery::GetSelection( CAGInputPtr pInput )
{
	TSelectionMap::iterator iter;
	iter = m_SelectionMap.find(pInput);

	return ( iter != m_SelectionMap.end() ) ? m_SelectionMap[pInput].type : eCT_AnyValue;
}

CVarBlockPtr CAnimationGraphStateQuery::CreateCriteriaVariables( CAGInputPtr pInput )
{
	CVarBlockPtr pVB = new CVarBlock();
	TSelectionMap::iterator iter;
	iter = m_SelectionMap.find(pInput);

	if ( iter == m_SelectionMap.end() )
	{
		m_SelectionMap[pInput] = SCriteria();
	}
	else
	{
		if ( eCT_ExactValue == iter->second.type )
		{
			if ( pInput->GetType() == eAGIT_Integer )
			{
				VariableHelper( pVB, "Value", iter->second.intValue );
			}
			else if ( pInput->GetType() == eAGIT_Float )
			{
				VariableHelper( pVB, "Value", iter->second.floatValue );
			}
			else if ( pInput->GetType() == eAGIT_String )
			{
				_smart_ptr<CVariableEnum<CString> > var = new CVariableEnum<CString>();
				for (CAGInput::key_iterator it = pInput->KeyBegin(); it != pInput->KeyEnd(); ++it)
					var->AddEnumItem( *it, *it );

				CString temp;
				temp.Format( "[%s]", pInput->GetName() );
				temp.MakeLower();
				var->AddEnumItem( temp, temp );

				var->Set( iter->second.strValue );
				var->AddOnSetCallback( CSetCallback<CString>( iter->second.strValue ) );
				var->SetHumanName("Value");
				var->SetFlags(0);
				pVB->AddVariable(var);
			}		
		}
	}

	return pVB;
}

CString CAnimationGraphStateQuery::GetGraphFileName()
{
	CString sName;

	CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();
	if ( pGraph )
	{
		sName = pGraph->GetFilename();
		sName = PathUtil::GetFile( (const char*)sName );
	}

	return sName;
}

CString CAnimationGraphStateQuery::GetGraphWorkingName()
{
	return GetGraphFileName() + ANIM_GRAPH_NAME_SUFFIX;
}

void CAnimationGraphStateQuery::RunUnitTests( bool bMessageOnSuccess /*=true*/, bool bMessageOnLoadFail /*=true*/ )
{
	CString sMessage;
	CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();

	if ( pGraph )
	{
		if ( !m_bGraphLoaded )
		{
			// Load last saved graph into engine automatically
			OnCommand( ID_LOADGRAPH, true ); 
			if ( !m_bGraphLoaded )
			{
				AfxMessageBox( "Unable to load graph into engine, cannot perform unit tests", MB_OK | MB_ICONERROR );
				return;
			}
		}
		
		CAnimationGraphUnitTester tester;
		if ( tester.Load( pGraph, GetGraphFileName(), GetGraphWorkingName(), &sMessage ) )
		{
			std::vector<CString> sErrors;
			int iNumTestsRun = 0;
			int iNumTestsPasssed = 0;
			bool bSuccess = tester.RunAllTests( &sErrors, &iNumTestsRun, &iNumTestsPasssed );
			if ( !bSuccess )
			{
				sMessage.Format( "Failure!\nPassed %d/%d tests.\n\nErrors:\n", iNumTestsPasssed, iNumTestsRun );
				for ( int i=0; i<sErrors.size(); ++i )
				{
					sMessage += sErrors[i] + "\n";
				}
				AfxMessageBox( sMessage, MB_OK | MB_ICONERROR );
			}
			else if ( bMessageOnSuccess )
			{
				sMessage.Format( "Success!\nPassed %d/%d tests.", iNumTestsPasssed, iNumTestsRun );
				AfxMessageBox( sMessage, MB_OK | MB_ICONINFORMATION );
			}
		}
		else if ( bMessageOnLoadFail )
		{
			AfxMessageBox( sMessage, MB_OK | MB_ICONERROR );
		}
	}	
}

void CAnimationGraphStateQuery::ChangePreselection()
{
	// read out all preselection values and set them
	// for that to work, the lists need to be members

	// reset all values
	CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();
	for ( CAnimationGraph::input_iterator iter = pGraph->InputBegin(); iter != pGraph->InputEnd(); ++iter )
	{
		SetSelection(*iter, eCT_AnyValue);
	}	
	Reload();


	// go over all lists and set their preselected values
	int lCount = m_pPreselectionLists->GetVarsCount();
	CVariableEnum<int>* tempList = NULL;
	int listNr = 0;
	int noneCount = 0;
	for (int i = 0; i < lCount; ++i)
	{
		// get the chosen selection list nr
		tempList = static_cast<CVariableEnum<int>*>(m_pPreselectionLists->GetVariable(i));
		tempList->Get(listNr);

		// get the list's name to read the selections from the xml file
		CString listName = tempList->GetDisplayValue();

		if (listName.Compare("<none>") == 0)
		{
			++noneCount;
			continue;
		}

		// find the list in the xml file
		int listCount = m_preselections->getChildCount();
		XmlNodeRef currentChild = NULL;
		for (int i = 0; i < listCount; ++i)
		{
			currentChild = m_preselections->getChild(i);
			if ( currentChild->isTag( XML_TAG_PRESELECTION ) )
			{
				const char *sName = currentChild->getAttr(XML_ATTR_NAME);
				if ( sName[0] && listName.Compare( sName ) == 0 )
				{
					// found the list, so break
					break;
				}
			}

		}

		// go through all the lists entries and set them
		int selectionCount = currentChild->getChildCount();
		for (int j = 0; j < selectionCount; ++j)
		{
			XmlNodeRef entry = currentChild->getChild(j);
			if ( !entry->isTag(XML_TAG_SELECTION) )
			{
				// unknown tag
				continue;
			}

			// get the input name
			CString sInputName = entry->getAttr(XML_ATTR_INPUT);

			// set the value
			CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();
			CAnimationGraph::input_iterator iter;
			for ( iter = pGraph->InputBegin(); iter != pGraph->InputEnd(); ++iter )
			{
				// find the one we're looking for
				CAGInputPtr pInput = *iter;
				if ( pInput->GetName().Compare(sInputName) == 0)
				{
					// check if it exists in the selection map as well (it should)
					TSelectionMap::iterator iter;
					iter = m_SelectionMap.find(pInput);
					if ( iter != m_SelectionMap.end() )
					{
						// set the value to eCT_ExactValue (=Specified Value)
						SetSelection(pInput, eCT_ExactValue);
						Reload();

						// if the value type is string, check whether the value
						// that is about to be set even exists
						if (pInput->GetType() == eAGIT_String)
						{
							// get the (string) value 
							CString sValue = entry->getAttr(XML_ATTR_VALUE);

							bool bExists = false;
							CVarBlockPtr pVB = m_pMap[ pInput->GetName() ];
							for (CAGInput::key_iterator it = pInput->KeyBegin(); it != pInput->KeyEnd(); ++it)
							{
								if (*it == sValue)
								{
									// it is available, so it can be set
									iter->second.strValue = sValue;
									Reload();
									bExists = true;
									break;
								}
							}
							// if it doesn't exist, switch back
							if (!bExists)
							{
								SetSelection(pInput, eCT_AnyValue);
								Reload();
								// make some kind of error notification
								Warning("Animationgraph Preselection attempted to set the illegal value '%s' for the Input '%s'.", sValue, pInput->GetName());
							}
						}
						else // int, float and ranges
						{	
							// get the correct values
							// cast value into the correct data type and then set it

							// has a single value
							if ( pInput->GetType() == eAGIT_Integer )
								entry->getAttr(XML_ATTR_VALUE, iter->second.intValue);
							else if ( pInput->GetType() == eAGIT_Float )
								entry->getAttr(XML_ATTR_VALUE, iter->second.floatValue);
							else
								Warning("Preselection failed to determine type of numerical value. Type id is %i", pInput->GetType());

							Reload();
						}

					}
					else
					{
						// the value cannot be set because the input exists,
						// but cannot be found in the selection list
						// this shouldn't happen
					}

					// now end the for loop prematurely
					break;
				}
			}
			if ( iter == pGraph->InputEnd())
			{
				// input could not be found
				Warning("Preselection List tries to set the Input '%s' which doesn't exist in this graph. Will be ignored.", sInputName);
			}


		}

			
		// set the list's selections
	}

	// if lCount == noneCount reset all selected criteria
	if (lCount == noneCount)
	{
		OnCommand( ID_RESETSELECTION );
	}
	


}

void CAnimationGraphStateQuery::BuildPreselectionLists()
{
	// Load preselection lists from xml file
	// and add <none> as an option on top.
	if ( !m_pPreselectionLists )
	{
		m_pPreselectionLists = new CVarBlock();
	}

	// there are two kind of lists
	CString sBasics(PRESELECTION_TYPE_BASICS);
	CString sOther(PRESELECTION_TYPE_OTHER);
	CVariableEnum<int>* basicLists = new CVariableEnum<int>();
	CVariableEnum<int>* othersLists = new CVariableEnum<int>();
	// add the <none> option at the top
	basicLists->AddEnumItem("<none>", 0);
	othersLists->AddEnumItem("<none>", 0);
	
	// get the file name
	CString sFileName( "Animations/graphs/" + GetGraphFileName() );
	int pos = sFileName.Find( ".xml" );
	if( pos != string::npos )
	{
		sFileName.Insert( pos, "_preselections" );
	}
	else
	{
		Warning("Cannot create PreselectionLists Filename from %s", GetGraphFileName());
		return;
	}

	// check if file exists and get the top node
	XmlNodeRef pNode = GetISystem()->LoadXmlFile( sFileName );
	if ( pNode != NULL)
	{
		m_preselections = pNode;	
		m_preselections = m_preselections->isTag( XML_TAG_PRESELECTIONLISTS ) ? m_preselections : m_preselections->findChild( XML_TAG_PRESELECTIONLISTS );

		// load in the list names
		int listCount = m_preselections->getChildCount();
		XmlNodeRef currentChild = NULL;
		int bCount = 0;
		int oCount = 0;
		for (int i = 0; i < listCount; ++i)
		{
			currentChild = m_preselections->getChild(i);
			if ( currentChild->isTag( XML_TAG_PRESELECTION ) )
			{
				// this child is a preselection list so get it's name and type
				const char *sType = currentChild->getAttr(XML_ATTR_TYPE);
				const char *sName = currentChild->getAttr(XML_ATTR_NAME);
				if ( sType[0] && sBasics.Compare( sType ) == 0 )
				{
					++bCount;
					basicLists->AddEnumItem(sName, bCount);
				}
				else if ( sType[0] && sOther.Compare( sType ) == 0 )
				{
					++oCount;
					othersLists->AddEnumItem(sName, oCount);
				}
			}
		}
	}
	else
	{
		// there are no preselections for this graph
		// return;
	}

	// add the two entries to the display group
	// only the first list is activated at the moment
	// and the additional list is implemented for future use
	m_pPreselectionLists->AddVariable(basicLists, "Basics");
//	m_pPreselectionLists->AddVariable(othersLists, "Other");	

	// add a callback routine to be called on change
	m_pPreselectionLists->AddOnSetCallback(CNotifyCallback<CAnimationGraphStateQuery>(this, &CAnimationGraphStateQuery::ChangePreselection));
}

void CAnimationGraphStateQuery::ResetPreselections()
{
	// reset everything to <none>
	CVariableEnum<int>* tempList = NULL;
	int vCount = m_pPreselectionLists->GetVarsCount();
	for (int i = 0; i < vCount; ++i)
	{
		tempList = static_cast<CVariableEnum<int>*>(m_pPreselectionLists->GetVariable(i));
		tempList->Set(0);
	}
}