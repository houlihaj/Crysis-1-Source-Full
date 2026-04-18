////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   aimanager.h
//  Version:     v1.00
//  Created:     11/9/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __aimanager_h__
#define __aimanager_h__

#if _MSC_VER > 1000
#pragma once
#endif

// forward declarations.
class CAIGoalLibrary;
class CAIBehaviorLibrary;


class CSOParamBase
{
public:
	CSOParamBase* pNext;
	IVariable* pVariable;
	bool bIsGroup;

	CSOParamBase( bool isGroup ) : pNext(0), pVariable(0), bIsGroup(isGroup) {}
	virtual ~CSOParamBase() { if (pNext) delete pNext; }

	void PushBack( CSOParamBase* pParam )
	{
		if ( pNext )
			pNext->PushBack( pParam );
		else
			pNext = pParam;
	}
};

class CSOParam : public CSOParamBase
{
public:
	CSOParam() : CSOParamBase(false), bVisible(true), bEditable(true) {}

	CString sName;		// the internal name
	CString sCaption;	// property name as it is shown in the editor
	bool bVisible;		// is the property visible
	bool bEditable;		// should the property be enabled
	CString sValue;		// default value
	CString sHelp;		// description of the property
};

class CSOParamGroup : public CSOParamBase
{
public:
	CSOParamGroup() : CSOParamBase(true), pChildren(0), bExpand(true) {}
	virtual ~CSOParamGroup() { if (pChildren) delete pChildren; if (pVariable) pVariable->Release(); }

	CString sName;		// property group name
	bool bExpand;		// is the group expanded by default
	CString sHelp;		// description
	CSOParamBase* pChildren;
};

class CSOTemplate
{
public:
	CSOTemplate() : params(0) {}
	~CSOTemplate() { if (params) delete params; }

	int id;					// id used in the rules to reference this template
	CString	name;			// name of the template
	CString description;	// description
	CSOParamBase* params;	// pointer to the first param
};


// map of all known smart object templates mapped by template id
typedef std::map< int, CSOTemplate* > MapTemplates;



//////////////////////////////////////////////////////////////////////////
class CAIManager
{
public:
	CAIManager();
	~CAIManager();
	void Init( ISystem *system );

	IAISystem*	GetAISystem();

	CAIGoalLibrary*	GetGoalLibrary() { return m_goalLibrary; };
	CAIBehaviorLibrary*	GetBehaviorLibrary() { return m_behaviorLibrary; };

	//////////////////////////////////////////////////////////////////////////
	//! AI Anchor Actions enumeration.
	void GetAnchorActions( std::vector<CString> &actions ) const;
	int AnchorActionToId( const char *sAction ) const;

	//////////////////////////////////////////////////////////////////////////
	//! Smart Objects States and Actions enumeration
	void GetSmartObjectStates( std::vector<CString> &values ) const;
	void GetSmartObjectActions( std::vector<CString> &values ) const;
	void AddSmartObjectState( const char* sState );

	// Enumerate all AI characters.

	//////////////////////////////////////////////////////////////////////////
	void ReloadScripts();
	void ReloadActionGraphs();
	void SaveActionGraphs();
	void SaveAndReloadActionGraphs();
	bool NewAction( CString& filename );

	const MapTemplates& GetMapTemplates() const { return m_mapTemplates; }
	const char* GetSmartObjectTemplateName( int index ) const {}

private:
	void EnumAnchorActions();
	void RandomizeAIVariations();

	void LoadActionGraphs();
	void FreeActionGraphs();

	void FreeTemplates();
	bool ReloadTemplates();
	CSOParamBase* LoadTemplateParams( XmlNodeRef root ) const;

	MapTemplates m_mapTemplates;

	CAIGoalLibrary* m_goalLibrary;
	CAIBehaviorLibrary* m_behaviorLibrary;
	IAISystem*	m_aiSystem;

	//! AI Anchor Actions.
	friend struct CAIAnchorDump;
	typedef std::map<CString,int> AnchorActions;
	AnchorActions m_anchorActions;
};

#endif // __aimanager_h__