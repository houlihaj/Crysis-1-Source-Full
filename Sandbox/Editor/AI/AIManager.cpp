////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   aimanager.cpp
//  Version:     v1.00
//  Created:     11/9/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#include "StdAfx.h"
#include "AIManager.h"

#include "AIGoalLibrary.h"
#include "AiGoal.h"
#include "AiBehaviorLibrary.h"

#include "IAISystem.h"
#include <IAIAction.h>

#include <IScriptSystem.h>
#include <IEntitySystem.h>

#include "../HyperGraph/FlowGraph.h"
#include "../HyperGraph/FlowGraphManager.h"
#include "../HyperGraph/HyperGraphDialog.h"
#include "StartupLogoDialog.h"
#include "Objects\Entity.h"

extern void ReloadSmartObjects( IConsoleCmdArgs* /* pArgs */ );

#define GRAPH_FILE_FILTER "Graph XML Files (*.xml)|*.xml"

#define SOED_SOTEMPLATES_FILENAME "/Libs/SmartObjects/Templates/SOTemplates.xml"

//////////////////////////////////////////////////////////////////////////
// Function created on designers request to automatically randomize
// model index variations per instance of the AI character.
//////////////////////////////////////////////////////////////////////////
void ed_randomize_variations( IConsoleCmdArgs* pArgs )
{
	CBaseObjectsArray objects;
	GetIEditor()->GetObjectManager()->GetObjects( objects );

	for (int i = 0; i < (int)objects.size(); i++)
	{
		CBaseObject *pObject = objects[i];
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			CEntity *pEntityObject = (CEntity*)pObject;
			if (pEntityObject->GetProperties2())
			{
				CVarBlock *pEntityProperties = pEntityObject->GetProperties();
				if (pEntityObject->GetPrototype())
					pEntityProperties = pEntityObject->GetPrototype()->GetProperties();
				if (!pEntityProperties)
					continue;
				// This is some kind of AI entity.
				// Check if it have AI variations
				IVariable *p_nModelVariations = pEntityProperties->FindVariable( "nModelVariations",true );
				IVariable *p_nVariation = pEntityObject->GetProperties2()->FindVariable( "nVariation",true );
				if (p_nModelVariations && p_nVariation)
				{
					int nModelVariations = 0;
					p_nModelVariations->Get(nModelVariations);
					if (nModelVariations > 0)
					{
						int nVariation = 1 + (int)(cry_frand()*nModelVariations);
						IVariable *p_nVariation = pEntityObject->GetProperties2()->FindVariable( "nVariation",true );
						if (p_nVariation)
						{
							p_nVariation->Set( nVariation );
							pEntityObject->Reload();
							continue;
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CAI Manager.
//////////////////////////////////////////////////////////////////////////
CAIManager::CAIManager()
{
	m_aiSystem = NULL;
	m_goalLibrary = new CAIGoalLibrary;
	m_behaviorLibrary = new CAIBehaviorLibrary;
}

CAIManager::~CAIManager()
{
	FreeActionGraphs();

	delete m_behaviorLibrary;
	m_behaviorLibrary = 0;
	delete m_goalLibrary;
	m_goalLibrary = 0;
}

void CAIManager::Init( ISystem *system )
{
	m_aiSystem = system->GetAISystem();
	if (!m_aiSystem)
		return;

//	m_aiSystem->Init( );
	
	//m_goalLibrary->InitAtomicGoals();
	CStartupLogoDialog::SetText( "Loading AI Behaviors..." );
	m_behaviorLibrary->LoadBehaviors( "Scripts\\AI\\Behaviors\\" );

	CStartupLogoDialog::SetText( "Initializing Smart Objects..." );
	//enumerate Anchor actions.
	EnumAnchorActions();

	//load smart object templates
	ReloadTemplates();

	//enumerate AI Actions and Smart Objects
	m_aiSystem->InitSmartObjects();

	CStartupLogoDialog::SetText( "Initializing Action Flow Graphs..." );
	LoadActionGraphs();

	gEnv->pConsole->AddCommand( "so_reload", ReloadSmartObjects );
	gEnv->pConsole->AddCommand( "ed_randomize_variations", ed_randomize_variations );
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::ReloadActionGraphs()
{
	if (!m_aiSystem)
		return;
//	FreeActionGraphs();
	GetAISystem()->ReloadActions();
	LoadActionGraphs();
}

void CAIManager::LoadActionGraphs()
{
	if (!m_aiSystem)
		return;

	int i = 0;
	IAIAction* pAction;
	while ( pAction = m_aiSystem->GetAIAction(i++) )
	{
		CFlowGraph* m_pFlowGraph = GetIEditor()->GetFlowGraphManager()->FindGraphForAction( pAction );
		if ( !m_pFlowGraph )
		{
			m_pFlowGraph = GetIEditor()->GetFlowGraphManager()->CreateGraphForAction( pAction );
			m_pFlowGraph->AddRef();
			CString filename( AI_ACTIONS_PATH );
			filename += '/';
			filename += pAction->GetName();
			filename += ".xml";
			m_pFlowGraph->SetName( "" );
			m_pFlowGraph->Load( filename );
		}
	}
}

void CAIManager::SaveAndReloadActionGraphs()
{
	if (!m_aiSystem)
		return;

	CString actionName;
	CWnd* pWnd = GetIEditor()->FindView( "Flow Graph" );
	if ( pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)) )
	{
		CHyperGraphDialog* pHGDlg = (CHyperGraphDialog*) pWnd;
		CHyperGraph* pGraph = pHGDlg->GetGraphView()->GetGraph();
		if ( pGraph )
		{
			IAIAction* pAction = pGraph->GetAIAction();
			if ( pAction )
			{
				actionName = pAction->GetName();
				pHGDlg->GetGraphView()->SetGraph( NULL );
			}
		}
	}

	SaveActionGraphs();
	ReloadActionGraphs();

	if ( !actionName.IsEmpty() )
	{
		IAIAction* pAction = GetAISystem()->GetAIAction( actionName );
		if ( pAction )
		{
			CFlowGraphManager* pManager = GetIEditor()->GetFlowGraphManager();
			CFlowGraph* pFlowGraph = pManager->FindGraphForAction( pAction );
			assert( pFlowGraph );
			if ( pFlowGraph )
				pManager->OpenView( pFlowGraph );
		}
	}
}

void CAIManager::SaveActionGraphs()
{
	if(!m_aiSystem)
		return;

	CWaitCursor waitCursor;

	int i = 0;
	IAIAction* pAction;
	while ( pAction = m_aiSystem->GetAIAction(i++) )
	{
		CFlowGraph* m_pFlowGraph = GetIEditor()->GetFlowGraphManager()->FindGraphForAction( pAction );
		if ( m_pFlowGraph->IsModified() )
		{
			m_pFlowGraph->Save( m_pFlowGraph->GetName() + CString(".xml") );
			pAction->Invalidate();
		}
	}
}

void CAIManager::FreeActionGraphs()
{
	CFlowGraphManager* pFGMgr = GetIEditor()->GetFlowGraphManager();
	if (pFGMgr)
	{
		pFGMgr->FreeGraphsForActions();
	}
}

IAISystem*	CAIManager::GetAISystem()
{
	return GetIEditor()->GetSystem()->GetAISystem();
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::ReloadScripts()
{
	GetBehaviorLibrary()->ReloadScripts();
	EnumAnchorActions();
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::GetAnchorActions( std::vector<CString> &actions ) const
{
	actions.clear();
	for (AnchorActions::const_iterator it = m_anchorActions.begin(); it != m_anchorActions.end(); it++)
	{
		actions.push_back( it->first );
	}
}

//////////////////////////////////////////////////////////////////////////
int CAIManager::AnchorActionToId( const char *sAction ) const
{
	AnchorActions::const_iterator it = m_anchorActions.find(sAction);
	if (it != m_anchorActions.end())
		return it->second;
	return -1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct CAIAnchorDump : public IScriptTableDumpSink
{
	CAIManager::AnchorActions actions;

	CAIAnchorDump( IScriptTable *obj )
	{
		m_pScriptObject = obj;
	}

	virtual void OnElementFound(int nIdx,ScriptVarType type) {}
	virtual void OnElementFound(const char *sName,ScriptVarType type)
	{
		if (type == svtNumber)
		{
			// New behavior.
			int val;
			if (m_pScriptObject->GetValue(sName,val))
			{
				actions[sName] = val;
			}
		}
	}
private:
	IScriptTable *m_pScriptObject;
};

//////////////////////////////////////////////////////////////////////////
void CAIManager::EnumAnchorActions()
{
	IScriptSystem *pScriptSystem = GetIEditor()->GetSystem()->GetIScriptSystem();
	
	SmartScriptTable pAIAnchorTable( pScriptSystem,true );
	if (pScriptSystem->GetGlobalValue( "AIAnchorTable",pAIAnchorTable ))
	{
		CAIAnchorDump anchorDump(pAIAnchorTable);
		pAIAnchorTable->Dump( &anchorDump );
		m_anchorActions = anchorDump.actions;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::GetSmartObjectStates( std::vector<CString> &values ) const
{
	if (!m_aiSystem)
		return;

	const char* sStateName;
	for ( int i = 0; sStateName = m_aiSystem->GetSmartObjectStateName(i); ++i )
		values.push_back( sStateName );
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::GetSmartObjectActions( std::vector<CString> &values ) const
{
	if (!m_aiSystem)
		return;

	const char* sActionName;
	for ( int i = 0; sActionName = m_aiSystem->GetAIActionName(i); ++i )
		values.push_back( sActionName );
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::AddSmartObjectState( const char* sState )
{
	if (!m_aiSystem)
		return;
	m_aiSystem->RegisterSmartObjectState( sState );
}

//////////////////////////////////////////////////////////////////////////
bool CAIManager::NewAction( CString& filename )
{	
	CFileUtil::CreateDirectory( Path::GetGameFolder() + CString("/") + AI_ACTIONS_PATH );

	if ( !CFileUtil::SelectSaveFile( GRAPH_FILE_FILTER, "xml", Path::GetGameFolder() + CString("/") + AI_ACTIONS_PATH, filename ) )
		return false;

	filename.MakeLower();

	// check if file exists.
	FILE *file = fopen( filename, "rb" );
	if ( file )
	{
		fclose(file);
		AfxMessageBox( "Can't create AI Action because another AI Action with this name already exists!\n\nCreation canceled..." );
		return false;
	}

	// Make a new graph.
	CFlowGraphManager* pManager = GetIEditor()->GetFlowGraphManager();
	CHyperGraph* pGraph = pManager->CreateGraph();

	CHyperNode* pStartNode = (CHyperNode*) pGraph->CreateNode( "AI:ActionStart" );
	pStartNode->SetPos( Gdiplus::PointF(80,10) );
	CHyperNode* pEndNode = (CHyperNode*) pGraph->CreateNode( "AI:ActionEnd" );
	pEndNode->SetPos( Gdiplus::PointF(400,10) );
	CHyperNode* pPosNode = (CHyperNode*) pGraph->CreateNode( "Entity:EntityPos" );
	pPosNode->SetPos( Gdiplus::PointF(20,70) );

	pGraph->UnselectAll();
	pGraph->ConnectPorts( pStartNode, &pStartNode->GetOutputs()->at(1), pPosNode, &pPosNode->GetInputs()->at(0), false );

	bool r = pGraph->Save( filename );

	ReloadActionGraphs();

	return r;
}

//////////////////////////////////////////////////////////////////////////
void CAIManager::FreeTemplates()
{
	MapTemplates::iterator it, itEnd = m_mapTemplates.end();
	for ( it = m_mapTemplates.begin(); it != itEnd; ++it )
		delete it->second;
	m_mapTemplates.clear();
}

bool CAIManager::ReloadTemplates()
{
	XmlNodeRef root = GetISystem()->LoadXmlFile( PathUtil::GetGameFolder() + SOED_SOTEMPLATES_FILENAME );
	if ( !root || !root->isTag("SOTemplates") )
		return false;

	FreeTemplates();

	int count = root->getChildCount();
	for ( int i = 0; i < count; ++i )
	{
		XmlNodeRef node = root->getChild(i);
		if ( node->isTag("Template") )
		{
			CSOTemplate* pTemplate = new CSOTemplate;

			node->getAttr( "id", pTemplate->id );
			pTemplate->name = node->getAttr( "name" );
			pTemplate->description = node->getAttr( "description" );

			// check is the id unique
			MapTemplates::iterator find = m_mapTemplates.find( pTemplate->id );
			if ( find != m_mapTemplates.end() )
			{
				AfxMessageBox( CString("WARNING:\nSmart object template named ") + pTemplate->name + " will be ignored! The id is not unique." );
				delete pTemplate;
				continue;
			}

			// load params
			pTemplate->params = LoadTemplateParams( node );
			if ( !pTemplate->params )
			{
				AfxMessageBox( CString("WARNING:\nSmart object template named ") + pTemplate->name + " will be ignored! Can't load params." );
				delete pTemplate;
				continue;
			}

			// insert in map
			m_mapTemplates[ pTemplate->id ] = pTemplate;
		}
	}
	return true;
}

CSOParamBase* CAIManager::LoadTemplateParams( XmlNodeRef root ) const
{
	CSOParamBase* pFirst = NULL;

	int count = root->getChildCount();
	for ( int i = 0; i < count; ++i )
	{
		XmlNodeRef node = root->getChild(i);
		if ( node->isTag("Param") )
		{
			CSOParam* pParam = new CSOParam;

			pParam->sName = node->getAttr( "name" );
			pParam->sCaption = node->getAttr( "caption" );
			node->getAttr( "visible", pParam->bVisible );
			node->getAttr( "editable", pParam->bEditable );
			pParam->sValue = node->getAttr( "value" );
			pParam->sHelp = node->getAttr( "help" );

			IVariable* pVar = NULL;
/*
			if ( pParam->sName == "bNavigationRule" )
				pVar = &gSmartObjectsUI.bNavigationRule;
			else if ( pParam->sName == "sEvent" )
				pVar = &gSmartObjectsUI.sEvent;
			else if ( pParam->sName == "userClass" )
				pVar = &gSmartObjectsUI.userClass;
			else if ( pParam->sName == "userState" )
				pVar = &gSmartObjectsUI.userState;
			else if ( pParam->sName == "userHelper" )
				pVar = &gSmartObjectsUI.userHelper;
			else if ( pParam->sName == "iMaxAlertness" )
				pVar = &gSmartObjectsUI.iMaxAlertness;
			else if ( pParam->sName == "objectClass" )
				pVar = &gSmartObjectsUI.objectClass;
			else if ( pParam->sName == "objectState" )
				pVar = &gSmartObjectsUI.objectState;
			else if ( pParam->sName == "objectHelper" )
				pVar = &gSmartObjectsUI.objectHelper;
			else if ( pParam->sName == "entranceHelper" )
				pVar = &gSmartObjectsUI.entranceHelper;
			else if ( pParam->sName == "exitHelper" )
				pVar = &gSmartObjectsUI.exitHelper;
			else if ( pParam->sName == "limitsDistanceFrom" )
				pVar = &gSmartObjectsUI.limitsDistanceFrom;
			else if ( pParam->sName == "limitsDistanceTo" )
				pVar = &gSmartObjectsUI.limitsDistanceTo;
			else if ( pParam->sName == "limitsOrientation" )
				pVar = &gSmartObjectsUI.limitsOrientation;
			else if ( pParam->sName == "delayMinimum" )
				pVar = &gSmartObjectsUI.delayMinimum;
			else if ( pParam->sName == "delayMaximum" )
				pVar = &gSmartObjectsUI.delayMaximum;
			else if ( pParam->sName == "delayMemory" )
				pVar = &gSmartObjectsUI.delayMemory;
			else if ( pParam->sName == "multipliersProximity" )
				pVar = &gSmartObjectsUI.multipliersProximity;
			else if ( pParam->sName == "multipliersOrientation" )
				pVar = &gSmartObjectsUI.multipliersOrientation;
			else if ( pParam->sName == "multipliersVisibility" )
				pVar = &gSmartObjectsUI.multipliersVisibility;
			else if ( pParam->sName == "multipliersRandomness" )
				pVar = &gSmartObjectsUI.multipliersRandomness;
			else if ( pParam->sName == "fLookAtOnPerc" )
				pVar = &gSmartObjectsUI.fLookAtOnPerc;
			else if ( pParam->sName == "userPreActionState" )
				pVar = &gSmartObjectsUI.userPreActionState;
			else if ( pParam->sName == "objectPreActionState" )
				pVar = &gSmartObjectsUI.objectPreActionState;
			else if ( pParam->sName == "highPriority" )
				pVar = &gSmartObjectsUI.highPriority;
			else if ( pParam->sName == "actionName" )
				pVar = &gSmartObjectsUI.actionName;
			else if ( pParam->sName == "userPostActionState" )
				pVar = &gSmartObjectsUI.userPostActionState;
			else if ( pParam->sName == "objectPostActionState" )
				pVar = &gSmartObjectsUI.objectPostActionState;

			if ( !pVar )
			{
				AfxMessageBox( CString("WARNING:\nSmart object template has a Param tag named ") + pParam->sName + " which is not recognized as valid name!\nThe Param will be ignored..." );
				delete pParam;
				continue;
			}
*/
			pParam->pVariable = pVar;

			if ( pFirst )
				pFirst->PushBack( pParam );
			else
				pFirst = pParam;
		}
		else if ( node->isTag("ParamGroup") )
		{
			CSOParamGroup* pGroup = new CSOParamGroup;

			pGroup->sName = node->getAttr( "name" );
			node->getAttr( "expand", pGroup->bExpand );
			pGroup->sHelp = node->getAttr( "help" );

			// load children
			pGroup->pChildren = LoadTemplateParams( node );
			if ( !pGroup->pChildren )
			{
				AfxMessageBox( CString("WARNING:\nSmart object template has a ParamGroup tag named ") + pGroup->sName + " without any children nodes!\nThe ParamGroup will be ignored..." );
				delete pGroup;
				continue;
			}

			CVariableArray* pVar = new CVariableArray();
			pVar->AddRef();
			pGroup->pVariable = pVar;

			if ( pFirst )
				pFirst->PushBack( pGroup );
			else
				pFirst = pGroup;
		}
	}

	return pFirst;
}
