#include "StdAfx.h"
#include "AnimationGraph.h"
#include "AnimationGraphStateEditor.h"
#include <queue>
#include "Controls\PropertyCtrl.h"
#include "IAnimationGraphSystem.h"
#include "HyperGraphNode.h"
#include "HyperNodePainter_MultiIOBox.h"
#include "SpeedReport.h"
#include "StringUtils.h"
#include "StringDlg.h"
#include "..\Util\FileUtil.h"

#define NO_VALUE_STRING "<none>"
#define ADD_KEY_STRING "<Add Key...>"
#define DIFFERENT_VALUES_STRING "Different Values Selected"


bool CParamsDeclaration::GuessParamId( const TParameterizationId& hint, TParameterizationId& returnId ) const
{
	returnId.clear();
	for ( const_iterator it = begin(); it != end(); ++it )
	{
		CString paramName( it->first );
		paramName.MakeLower();
		TParameterizationId::const_iterator itHint = hint.find( paramName );
		if ( itHint == hint.end() )
		{
			// there's no hint for this parameter
			returnId.clear();
			return false;
		}
		if ( it->second.find(itHint->second) == it->second.end() )
		{
			// invalid hint
			returnId.clear();
			return false;
		}
		returnId.insert( std::make_pair(it->first, itHint->second) );
	}
	return true;
}


namespace
{

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

	IVariablePtr SetGraph( IVariablePtr var, CAnimationGraph * pGraph )
	{
		var->AddOnSetCallback( CNotifyCallback<CAnimationGraph>(pGraph, &CAnimationGraph::Modified) );
		return var;
	}

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

	template <class T>
	IVariablePtr ConstVariableHelper( CVarBlockPtr varBlock, const CString& humanName, const T& value )
	{
		IVariablePtr var = new CVariable<T>();
		var->Set(value);
		var->SetHumanName(humanName);
		var->SetFlags( IVariable::UI_DISABLED | IVariable::UI_BOLD );
		varBlock->AddVariable(var);
		return var;
	}

	template <class T, class H>
	IVariablePtr VariableHelper( CVarBlockPtr varBlock, const CString& humanName, T& value, H * pHost, void (H::*notification)() )
	{
		IVariablePtr pVar = VariableHelper( varBlock, humanName, value );
		pVar->AddOnSetCallback( CNotifyCallback<H>(pHost, notification) );
		return pVar;
	}

	class CStateVariable : public CVariableEnum<CString>, public IAnimationGraphListener
	{
	public:
		CStateVariable(CAnimationGraph * pGraph, CAGStatePtr& ptr, CAGStatePtr selectedState) : m_pGraph(pGraph), m_ptr(ptr), m_selectedState(selectedState)
		{
			m_pGraph->AddListener(this);
		}
		~CStateVariable()
		{
			m_pGraph->RemoveListener(this);
		}

		void Init()
		{
			Reload();
			AddOnSetCallback( CNotifyCallback<CStateVariable>(this, &CStateVariable::Change) );
		}

		void Reload()
		{
			_smart_ptr<CVarEnumList<CString> > pEnum = new CVarEnumList<CString>();
			pEnum->AddItem(NO_VALUE_STRING, NO_VALUE_STRING);

			std::vector<CString> listItems;
			for (CAnimationGraph::state_iterator iter = m_pGraph->StateBegin(); iter != m_pGraph->StateEnd(); ++iter)
			{
				CAGStatePtr state = ( *iter );
				if ( ValidParentForSelectedNode( state ) )
				{
					listItems.push_back( state->GetName() );
				}
			}
			std::sort( listItems.begin(), listItems.end() );

			for (std::vector<CString>::const_iterator iter = listItems.begin(); iter != listItems.end(); ++iter)
				pEnum->AddItem( *iter, *iter );

			SetEnumList(pEnum);
		}

		void OnStateEvent( EAGStateEvent evt, CAGStatePtr pState )
		{
			if (evt == eAGSE_ChangeName)
				Reload();
		}

	private:
		void Change()
		{
			CString temp;
			Get(temp);
			if (temp != NO_VALUE_STRING)
				m_ptr = m_pGraph->FindState( temp );
			else
				m_ptr = CAGStatePtr();
		}

		bool ValidParentForSelectedNode( CAGStatePtr state )
		{
			if ( ! state )
			{
				return true;
			}

			bool foundLoop = ( state == m_selectedState );
			if ( foundLoop )
			{
				return false;
			}

			return ValidParentForSelectedNode( state->GetParent() );
		}

		CAGStatePtr& m_ptr;
		CAnimationGraph * m_pGraph;
		CAGStatePtr m_selectedState;
	};

	class CStateGroupVariable : public CVariableEnum<CString>, public IAnimationGraphListener
	{
	public:
		CStateGroupVariable(CAnimationGraph * pGraph, CString& ptr) : m_pGraph(pGraph), m_ptr(ptr)
		{
			m_pGraph->AddListener(this);
		}
		~CStateGroupVariable()
		{
			m_pGraph->RemoveListener(this);
		}

		void Init()
		{
			Reload();
			AddOnSetCallback( CNotifyCallback<CStateGroupVariable>(this, &CStateGroupVariable::Change) );
		}

		void Reload()
		{
			_smart_ptr<CVarEnumList<CString> > pEnum = new CVarEnumList<CString>();
			pEnum->AddItem(NO_VALUE_STRING, NO_VALUE_STRING);

			std::vector<CString> listItems;
			for (CAnimationGraph::state_iterator iter = m_pGraph->StateBegin(); iter != m_pGraph->StateEnd(); ++iter)
				listItems.push_back((*iter)->GetName());
			std::sort( listItems.begin(), listItems.end() );

			for (std::vector<CString>::const_iterator iter = listItems.begin(); iter != listItems.end(); ++iter)
				pEnum->AddItem( *iter, *iter );

			SetEnumList(pEnum);
		}

		void OnStateEvent( EAGStateEvent evt, CAGStatePtr pState )
		{
			if (evt == eAGSE_ChangeName)
				Reload();
		}

	private:
		void Change()
		{
			CString temp;
			Get(temp);
			if (temp != NO_VALUE_STRING)
				m_ptr = temp;
			else
				m_ptr = CString();
		}

		CString& m_ptr;
		CAnimationGraph * m_pGraph;
	};

	class CCriteriaModeVariable : public CVariableEnum<int>
	{
	public:
		CCriteriaModeVariable( CVarBlock * pVB, CAGInputPtr pInput, CAGStatePtr pState, CAnimationGraphStateEditor * pSE ) : m_pVB(pVB), m_pInput(pInput), m_pState(pState), m_pSE(pSE)
		{
			ECriteriaType value = pState->GetCriteriaType(pInput);
			if ( value == eCT_DefinedInTemplate )
			{
				AddEnumItem( "Defined in template", eCT_DefinedInTemplate );
				Set( int(eCT_DefinedInTemplate) );
			}
			else
			{
				if ( value != eCT_UseParent && m_pState->GetActiveParameterization() != NULL )
				{
					AddEnumItem( "Use Default", eCT_UseParent );
					if ( value == eCT_UseDefault )
						value = eCT_UseParent;
				}
				else
				{
					AddEnumItem( "Use Parent State", eCT_UseParent );
				}
				AddEnumItem( "Any Value", eCT_AnyValue );
				AddEnumItem( "Specified Value", eCT_ExactValue );
				if (pInput->CanUseMinMaxCriteria())
					AddEnumItem( "Value Range (min/max)", eCT_MinMax );
				if (value == eCT_DifferentValues)
					AddEnumItem( DIFFERENT_VALUES_STRING, eCT_DifferentValues );
				Set( int(value) );
				AddOnSetCallback( CNotifyCallback<CCriteriaModeVariable>(this, &CCriteriaModeVariable::Change) );
			}
		}

		void Init()
		{
			int value;
			Get(value);
			if ( value != eCT_DefinedInTemplate )
			{
				CVarBlockPtr pVB = m_pState->CreateCriteriaVariables( m_pInput, true, m_pState->GetActiveParameterization() );
				for (int i=0; i<pVB->GetVarsCount(); i++)
					m_pVB->AddVariable( pVB->GetVariable(i) );
			}
		}

	private:
		void Change()
		{
			int temp;
			Get(temp);
			ECriteriaType critType = (ECriteriaType) temp;

			// now set the criteria
			m_pState->SetCriteriaType( m_pInput, critType );

			// and rebuild the panel
			if (m_pSE)
				m_pSE->StateEvent( eSEE_RebuildCriteria );
		}

		CVarBlock * m_pVB;
		CAGInputPtr m_pInput;
		CAGStatePtr m_pState;
		CAnimationGraphStateEditor * m_pSE;
	};

	class CCriteriaKeyValueVariable : public CVariableEnum<CString>
	{
	public:
		CCriteriaKeyValueVariable( CVarBlock * pVB, CAGInputPtr pInput, CString& value ) : m_pVB(pVB), m_pInput(pInput), m_valueRef(value)
		{
			AddEnumItem( ADD_KEY_STRING, ADD_KEY_STRING );

			for ( CAGInput::key_iterator it = pInput->KeyBegin(); it != pInput->KeyEnd(); ++it )
				AddEnumItem( *it, *it );

			CString temp;
			temp.Format( "[%s]", pInput->GetName() );
			temp.MakeLower();
			AddEnumItem( temp, temp );

			Set( m_valueRef );
			AddOnSetCallback( CNotifyCallback< CCriteriaKeyValueVariable >(this, &CCriteriaKeyValueVariable::Change) );
		}

	private:
		void Change()
		{
			CString temp;
			Get( temp );

			if ( temp != ADD_KEY_STRING )
			{
				m_valueRef = temp;
				return;
			}

			CString name;
			for ( int i=0; ; i++ )
			{
				name.Format( "input%d", i );
				if ( !m_pInput->HasKey(name) )
					break;
			}
			CStringDlg dlg( "Add Key" );
			dlg.m_strString = name;
			if ( dlg.DoModal() == IDOK && !dlg.m_strString.IsEmpty() )
			{
				if ( m_pInput->HasKey(dlg.m_strString) )
					AfxMessageBox("WARNING: Key already exists!");
				else
					m_pInput->AddKey( dlg.m_strString );
				m_valueRef = dlg.m_strString;
				AddEnumItem( m_valueRef, m_valueRef );
				Set( m_valueRef );
			}
			else
				Set( m_valueRef );
		}

		CVarBlock * m_pVB;
		CAGInputPtr m_pInput;
		CString& m_valueRef;
	};

	class COverridableModeVariable : public CVariableEnum<CString>
	{
	public:
		COverridableModeVariable( CVarBlock * pVB, CAGCategoryPtr pCategory, CAGStatePtr pState, CAnimationGraphStateEditor * pSE ) : m_pCategory(pCategory), m_pState(pState), m_pSE(pSE)
		{
			AddEnumItem( "Use parent implementation", "_parent" );
			AddEnumItem( "Use no implementation", "_none" );

			CAnimationGraph * pGraph = pState->GetGraph();
			for (CAnimationGraph::state_factory_iterator iter = pGraph->StateFactoryBegin(); iter != pGraph->StateFactoryEnd(); ++iter)
			{
				if ((*iter)->GetCategory() == pCategory)
					AddEnumItem((*iter)->GetHumanName(), (*iter)->GetName());
			}

			Set( pState->GetStateFactoryMode(pCategory) );

			AddOnSetCallback( CNotifyCallback<COverridableModeVariable>( this, &COverridableModeVariable::OnChange ) );
		}

		void Init( CVarBlockPtr pVBDisplay )
		{
			CAGStatePtr pState = m_pState;
			while (pState)
			{
				CString mode = pState->GetStateFactoryMode( m_pCategory );
				if (mode == "_parent")
				{
					pState = pState->GetParent();
					continue;
				}
				else if (mode == "_none")
				{
				}
				else
				{
					CVarBlockPtr pVBEdit = pState->GetStateFactoryVarBlock( m_pCategory );
					int nVars = pVBEdit->GetVarsCount();
					int flags = 0;
					if (pState != m_pState)
						flags |= IVariable::UI_DISABLED;
					for (int i=0; i<nVars; i++)
					{
						IVariablePtr pVar = pVBEdit->GetVariable(i);
						pVar->SetFlags( flags );
						pVBDisplay->AddVariable( pVar );
					}
				}

				break;
			}
		}

	private:
		CAGCategoryPtr m_pCategory;
		CAGStatePtr m_pState;
		CAnimationGraphStateEditor * m_pSE;

		void OnChange()
		{
			CString newMode;
			Get(newMode);
			m_pState->SetStateFactoryMode( m_pCategory, newMode );

			m_pSE->StateEvent(eSEE_RebuildOverridable);
		}
	};

	class CTemplateVariable : public CVariableEnum<CString>
	{
	public:
		CTemplateVariable( CVarBlock * pVB, CAGStateTemplatePtr pStateTemplate, CAGStatePtr pState, CAnimationGraphStateEditor * pSE ) : m_pStateTemplate(pStateTemplate), m_pState(pState), m_pSE(pSE)
		{
			CAnimationGraph * pGraph = pState->GetGraph();

			std::vector<CString> templates;
			for (CAnimationGraph::state_template_iterator iter = pGraph->StateTemplateBegin(); iter != pGraph->StateTemplateEnd(); ++iter)
				templates.push_back((*iter)->GetName());
			std::sort(templates.begin(), templates.end());
			for (std::vector<CString>::const_iterator iter = templates.begin(); iter != templates.end(); ++iter)
				AddEnumItem( *iter, *iter );

			Set( pStateTemplate->GetName() );

			if ( pState->GetActiveParameterization() )
				SetFlags( GetFlags() | IVariable::UI_DISABLED );

			AddOnSetCallback( CNotifyCallback<CTemplateVariable>( this, &CTemplateVariable::OnChange ) );
		}

		void Init( CVarBlockPtr pVBDisplay )
		{
			const std::map<CString, CString>& tplParams = m_pStateTemplate->GetParams();
			std::map<CString, CString>& stParams = m_pState->GetTemplateParameters();
			for (std::map<CString, CString>::const_iterator iter = tplParams.begin(); iter != tplParams.end(); ++iter)
				VariableHelper( pVBDisplay, iter->first, stParams[iter->first] );
		}

	private:
		CAGStateTemplatePtr m_pStateTemplate;
		CAGStatePtr m_pState;
		CAnimationGraphStateEditor * m_pSE;

		void OnChange()
		{
			CString newMode;
			Get(newMode);
			m_pState->SetTemplateType( newMode );
			m_pSE->StateEvent(eSEE_RebuildTemplate);
		}
	};

	class CSetInclusionVariable : public CVariable<bool>
	{
	public:
		CSetInclusionVariable( CString key, std::set<CString>& values ) : m_key(key), m_values(values), m_initialized(false)
		{
		}

		void Init()
		{
			bool included = m_values.find(m_key) != m_values.end();
			SetValue(included);			
			m_initialized = true;
		}

		void OnSetValue(bool)
		{
			if (!m_initialized)
				return;
			bool included = false;
			GetValue(included);
			if (included)
				m_values.insert(m_key);
			else
				m_values.erase(m_key);
		}

	private:
		CString m_key;
		std::set<CString>& m_values;
		bool m_initialized;
	};

	class CMovementControlMethodVariable : public CVariableEnum<int>
	{
	public:

		CMovementControlMethodVariable(EMovementControlMethod* pMCM)
		{
			m_pMCM = pMCM;

			int temp = (int)*m_pMCM;
			Set(temp);

			AddEnumItem("Entity", eMCM_Entity);
			AddEnumItem("Animation", eMCM_Animation);
			AddEnumItem("DecoupledCatchUp", eMCM_DecoupledCatchUp);

			AddOnSetCallback(CNotifyCallback<CMovementControlMethodVariable>(this, &CMovementControlMethodVariable::Change));
		}

	private:

		void Change()
		{
			int temp;
			Get(temp);
			*m_pMCM = (EMovementControlMethod)temp;
		}

		EMovementControlMethod* m_pMCM;

	};

	class CLinkParamsVariable : public CVariableArray
	{
		class CCallback : public Functor1<IVariable*>
		{
		public:
			typedef CLinkParamsVariable H;

			CCallback( H* pHost, void (H::*notification)(IVariable*) ) : Functor1<IVariable*>(thunk, pHost, 0, &notification, sizeof(notification)) {}

			static void thunk( const FunctorBase& ftor, IVariable* pVar )
			{
				typedef void (H::*Notification)(IVariable*);
				Notification notification = *(Notification*)(void*)ftor.getMemFunc();
				H * callee = (H*)ftor.getCallee();
				(callee->*notification)(pVar);
			}
		};

	public:
		CLinkParamsVariable( CAGLinkPtr pLink, const CAGStatePtr pOwnerState, const CAGStatePtr pRelatedState, bool bIsSource )
			: m_pLink( pLink )
			, m_pOwnerState( pOwnerState )
			, m_pRelatedState( pRelatedState )
			, m_bIsSource( bIsSource )
		{
			SetHumanName( m_pOwnerState->GetName() );
		}

		void Init()
		{
			DeleteAllChilds();
			const CParamsDeclaration* pParams = m_pOwnerState->GetParamsDeclaration();
			if ( !pParams || pParams->empty() )
			{
				_smart_ptr< CVariableEnum<CString> > pVar = new CVariableEnum< CString >;
				pVar->SetFlags( IVariable::UI_DISABLED );
				pVar->AddEnumItem( "", "" );
				AddChildVar( pVar );
				return;
			}

			CParamsDeclaration::const_iterator it, itEnd = pParams->end();
			for ( it = pParams->begin(); it != itEnd; ++it )
			{
				CString name;
				name = '[';
				name += it->first;
				name += ']';

				_smart_ptr< CVariableEnum<CString> > pVar = new CVariableEnum< CString >;
				pVar->SetHumanName( it->first );
				pVar->AddEnumItem( "", "" );

				const CParamsDeclaration* pRelatedParams = m_pRelatedState->GetParamsDeclaration();
				CParamsDeclaration::const_iterator itRelated, itRelatedEnd = pRelatedParams->end();
				for ( itRelated = pRelatedParams->begin(); itRelated != itRelatedEnd; ++itRelated )
				{
					CString temp;
					temp = '[';
					temp += itRelated->first;
					temp += ']';
					CString lower( temp );
					lower.MakeLower();
					pVar->AddEnumItem( temp, lower );
				}

				TSetOfCString::const_iterator itValues, itValuesEnd = it->second.end();
				for ( itValues = it->second.begin(); itValues != itValuesEnd; ++itValues )
					pVar->AddEnumItem( *itValues, *itValues );

				name.MakeLower();
				CAGLink::TListMappedParams::iterator itMapped, itMappedEnd = m_pLink->m_listMappedParams.end();
				for ( itMapped = m_pLink->m_listMappedParams.begin(); itMapped != itMappedEnd; ++itMapped )
					if ( name == (m_bIsSource ? itMapped->first : itMapped->second) )
						pVar->Set( m_bIsSource ? itMapped->second : itMapped->first );

				pVar->AddOnSetCallback(CCallback( this, &CLinkParamsVariable::OnChange ));
				AddChildVar( pVar );
			}
		}

		CLinkParamsVariable* m_pCrossLinked;

	private:
		void OnChange( IVariable* pVar )
		{
			CString name;
			name = '[';
			name += pVar->GetHumanName();
			name += ']';
			name.MakeLower();
			CString value;
			pVar->Get( value );

			CString paramValue;
			if ( !value.IsEmpty() && value.GetAt(0) == '[' && value.GetAt(value.GetLength()-1) == ']' )
				paramValue = value;
			paramValue.MakeLower();

			bool bFound = false;
			CAGLink::TListMappedParams::iterator it = m_pLink->m_listMappedParams.begin();
			CAGLink::TListMappedParams::iterator itEnd = m_pLink->m_listMappedParams.end();
			if ( m_bIsSource )
			{
				while ( it != itEnd )
				{
					if ( it->first == name )
					{
						bFound = true;
						if ( value.IsEmpty() )
						{
							m_pLink->m_listMappedParams.erase( it++ );
						}
						else
						{
							it->second = value;
							++it;
						}
					}
					else if ( it->second == paramValue )
						m_pLink->m_listMappedParams.erase( it++ );
					else
						++it;
				}
				if ( !bFound && !value.IsEmpty() )
					m_pLink->m_listMappedParams.push_back( std::make_pair(name,value) );
			}
			else
			{
				while ( it != itEnd )
				{
					if ( it->second == name )
					{
						bFound = true;
						if ( value.IsEmpty() )
						{
							m_pLink->m_listMappedParams.erase( it++ );
						}
						else
						{
							it->first = value;
							++it;
						}
					}
					else if ( it->first == paramValue )
						m_pLink->m_listMappedParams.erase( it++ );
					else
						++it;
				}
				if ( !bFound && !value.IsEmpty() )
					m_pLink->m_listMappedParams.push_back( std::make_pair(value,name) );
			}
			m_pLink->MappingChanged();
		}

		CAGLinkPtr m_pLink;
		const CAGStatePtr m_pOwnerState;
		const CAGStatePtr m_pRelatedState;
		bool m_bIsSource;
	};

	IVariablePtr VariableHelper_StateSelector( CVarBlockPtr varBlock, const CString& humanName, CAGStatePtr& extendVar, CAnimationGraph * pGraph, CAGStatePtr selectedState )
	{
		CAGStatePtr origValue = extendVar;
		_smart_ptr<CStateVariable> var = new CStateVariable(pGraph, extendVar, selectedState);
		var->Init();
		if (origValue)
			var->Set(origValue->GetName());
		else
			var->Set(NO_VALUE_STRING);
		var->SetHumanName(humanName);
		varBlock->AddVariable(var);
		return &*var;
	}

	IVariablePtr VariableHelper_SetInclusion( CVarBlockPtr varBlock, const CString& humanName, const CString& key, std::set<CString>& values, CAnimationGraph * pGraph )
	{
		_smart_ptr<CSetInclusionVariable> var = new CSetInclusionVariable(key, values);
		var->Init();
		var->SetHumanName(humanName);
		varBlock->AddVariable(var);
		return &*var;
	}

	IVariablePtr VariableHelper_StateGroupSelector( CVarBlockPtr varBlock, const CString& humanName, CString& groupVar, CAnimationGraph * pGraph )
	{
		_smart_ptr<CStateGroupVariable> var = new CStateGroupVariable(pGraph, groupVar);
		var->Init();
		if (groupVar.IsEmpty())
			var->Set(NO_VALUE_STRING);
		else
			var->Set(groupVar);
		var->SetHumanName(humanName);
		varBlock->AddVariable(var);
		return &*var;
	}

	IVariablePtr VariableHelper_MCM(CVarBlockPtr varBlock, const CString& humanName, EMovementControlMethod* pMCM)
	{
		_smart_ptr<CMovementControlMethodVariable> var = new CMovementControlMethodVariable(pMCM);
		var->SetHumanName(humanName);
		varBlock->AddVariable(var);
		return &*var;
	}

	IVariablePtr VariableHelper_Criteria( CVarBlockPtr varBlock, const CString& humanName, CAGInputPtr pInput, CAGStatePtr pState, CAnimationGraphStateEditor * pSE )
	{
		_smart_ptr<CCriteriaModeVariable> var = new CCriteriaModeVariable(varBlock, pInput, pState, pSE);
		var->SetHumanName(humanName);
		varBlock->AddVariable(var);
		var->Init();
		return &*var;
	}

	IVariablePtr VariableHelper_Overridable( CVarBlockPtr varBlock, const CString& humanName, CAGCategoryPtr pCategory, CAGStatePtr pState, CAnimationGraphStateEditor * pSE )
	{
		_smart_ptr<COverridableModeVariable> var = new COverridableModeVariable(varBlock, pCategory, pState, pSE);
		var->SetHumanName(humanName);
		if ( pState->GetActiveParameterization() )
			var->SetFlags( var->GetFlags() | IVariable::UI_DISABLED );
		varBlock->AddVariable(var);
		var->Init(varBlock);
		return &*var;
	}

	IVariablePtr VariableHelper_Template( CVarBlockPtr pVB, const CString& humanName, CAGStateTemplatePtr pStateTemplate, CAGStatePtr pState, CAnimationGraphStateEditor * pSE )
	{
		_smart_ptr<CTemplateVariable> var = new CTemplateVariable(pVB, pStateTemplate, pState, pSE);
		var->SetHumanName(humanName);
		pVB->AddVariable(var);
		var->Init(pVB);
		return &*var;
	}

	_smart_ptr< CLinkParamsVariable > VariableHelper_ParamMapping( CVarBlockPtr pVB, CAGLinkPtr pLink, CAGStatePtr pStateFrom, CAGStatePtr pStateTo, bool bIsSource )
	{
		_smart_ptr< CLinkParamsVariable > pVar = new CLinkParamsVariable( pLink, pStateFrom, pStateTo, bIsSource );
		pVB->AddVariable( pVar );
		pVar->Init();
		return pVar;
	}
}

/*
 * CAGCompositeOperation
 */

CString CAGCompositeOperation::GetExplanation()
{
	CString out;
	for (std::vector<IAGCheckedOperationPtr>::const_iterator iter = m_ops.begin(); iter != m_ops.end(); ++iter)
		out += (*iter)->GetExplanation() + ", ";
	return out;
}

void CAGCompositeOperation::Perform()
{
	for (std::vector<IAGCheckedOperationPtr>::const_iterator iter = m_ops.begin(); iter != m_ops.end(); ++iter)
		(*iter)->Perform();
}

/*
 * CAGCategory
 */

CAGCategory::CAGCategory( const IAnimationGraphCategoryIterator::SCategory& cat )
{
	m_name = cat.name;
	m_bOverridable = cat.overridable;
  m_bUsableWithTemplate = cat.usableWithTemplate;
}

/*
 * CAGStateFactoryParam
 */

CAGStateFactoryParamPtr CAGStateFactoryParam::Create(const IAnimationGraphStateFactoryIterator::SStateFactoryParameter& p)
{
#define DEF_CREATE_TYPE(hname, cname) if (0 == strcmp(hname, p.type)) return new CAGStateFactoryParamImpl<cname>(p);
	DEF_CREATE_TYPE("int", int);
	DEF_CREATE_TYPE("float", float);
	DEF_CREATE_TYPE("vec3", Vec3);
	DEF_CREATE_TYPE("string", CString);
	DEF_CREATE_TYPE("bool", bool);
#undef DEF_CREATE_TYPE
	CryError("Unknown parameter type: %s", p.type);
	return CAGStateFactoryParamPtr();
}

CAGStateFactoryParam::CAGStateFactoryParam( const IAnimationGraphStateFactoryIterator::SStateFactoryParameter& p )
{
	m_bRequired = p.required;
	m_name = p.name;
	m_humanName = p.humanName;
	m_defaultValue = p.defaultValue;
	m_upgrade = p.upgradeFunction;
}

void CAGStateFactoryParam::AddToVarBlock( CVarBlockPtr pVB )
{
	IVariablePtr pVar;

	if (!m_bRequired)
	{
		pVar = new CVariable<bool>();
		pVar->SetHumanName( "Use " + m_name );
		pVar->SetName( "_use_" + m_name );
		pVar->Set(false);
		pVB->AddVariable( pVar );
	}

	pVar = CreateVariable();
	pVar->SetHumanName( m_humanName );
	pVar->SetName( m_name );
	if (!m_defaultValue.IsEmpty())
		pVar->Set( m_defaultValue );
	pVB->AddVariable( pVar );
}

void CAGStateFactoryParam::Load( XmlNodeRef node, CVarBlockPtr pVB )
{
	if (!m_bRequired)
	{
		if (node->haveAttr(m_name))
		{
			pVB->FindVariable("_use_" + m_name)->Set(true);
			pVB->FindVariable(m_name)->Set( node->getAttr(m_name) );
		}
		else
		{
			pVB->FindVariable("_use_" + m_name)->Set(false);
		}
	}
	else
	{
		if (!node->haveAttr(m_name) && m_upgrade)
			m_upgrade(node);
		pVB->FindVariable(m_name)->Set( node->getAttr(m_name) );
	}
}

void CAGStateFactoryParam::Save( XmlNodeRef node, CVarBlockPtr pVB )
{
	bool have = true;
	if (!m_bRequired)
	{
		have = false;
		pVB->FindVariable("_use_" + m_name)->Get(have);
	}
	if (have)
	{
		CString value;
		pVB->FindVariable(m_name)->Get(value);
		node->setAttr(m_name, value);
	}
}

/*
 * CAGStateFactory
 */

CAGStateFactory::CAGStateFactory( CAnimationGraph * pGraph, const IAnimationGraphStateFactoryIterator::SStateFactory& p )
{
	m_pCategory = pGraph->FindCategory( p.category );
	assert(m_pCategory);
	m_name = p.name;
	m_humanName = p.humanName;

	typedef IAnimationGraphStateFactoryIterator::SStateFactoryParameter Param;
	for (const Param * pParam = p.pParams; pParam && pParam->type; ++pParam)
		m_params.push_back( CAGStateFactoryParam::Create(*pParam) );
}

void CAGStateFactory::FillVarBlock( CVarBlockPtr pVB )
{
	for (std::vector<CAGStateFactoryParamPtr>::const_iterator iter = m_params.begin(); iter != m_params.end(); ++iter)
	{
		(*iter)->AddToVarBlock( pVB );
	}
}

void CAGStateFactory::Load( XmlNodeRef node, CVarBlockPtr pVB )
{
	for (std::vector<CAGStateFactoryParamPtr>::const_iterator iter = m_params.begin(); iter != m_params.end(); ++iter)
	{
		(*iter)->Load( node, pVB );
	}
}

XmlNodeRef CAGStateFactory::ToXml( CVarBlockPtr pVB )
{
	XmlNodeRef node = CreateXmlNode( m_name );

	for (std::vector<CAGStateFactoryParamPtr>::const_iterator iter = m_params.begin(); iter != m_params.end(); ++iter)
	{
		(*iter)->Save( node, pVB );
	}

	return node;
}

/*
 * CAGInput
 */

CAGInput::CAGInput( CAnimationGraph * pGraph ) : m_pGraph(pGraph), m_type(eAGIT_String), m_floatMin(0), m_floatMax(0), m_intMin(0), m_intMax(0), m_signalled(false), m_priority(1), m_attachToBlendWeight("-1"), m_blendWeightMoveSpeed(0.0f), m_forceGuard(false), m_mixinFilter(false)
{
}

CAGInput::CAGInput( CAnimationGraph * pGraph, const CString& name ) : m_pGraph(pGraph), m_type(eAGIT_String), m_floatMin(0), m_floatMax(0), m_intMin(0), m_intMax(0), m_name(name), m_priority(1), m_signalled(false), m_attachToBlendWeight("-1"), m_blendWeightMoveSpeed(0.0f), m_forceGuard(false), m_mixinFilter(false)
{
}

bool CAGInput::Load( XmlNodeRef node )
{
	const char * tag = node->getTag();
	m_name = node->getAttr("name");
	node->getAttr("signalled", m_signalled);
	node->getAttr("priority", m_priority);
	m_forceGuard = false;
	node->getAttr("forceGuard", m_forceGuard);
	m_mixinFilter = false;
	node->getAttr("mixinFilter", m_mixinFilter);

	m_defaultValue = node->getAttr("defaultValue");
	m_attachToBlendWeight = "-1";
	node->getAttr("attachToBlendWeight", m_attachToBlendWeight);
	m_blendWeightMoveSpeed = 0.0f;
	node->getAttr("blendWeightMoveSpeed", m_blendWeightMoveSpeed);

	if (0 == strcmp(tag, "IntegerState"))
	{
		m_type = eAGIT_Integer;
		if (!node->getAttr("min", m_intMin) || !node->getAttr("max", m_intMax))
			CryLogAlways("Integer state %s without min/max", (const char *)m_name);
	}
	else if (0 == strcmp(tag, "FloatState"))
	{
		m_type = eAGIT_Float;
		if (!node->getAttr("min", m_floatMin) || !node->getAttr("max", m_floatMax))
			CryLogAlways("Float state %s without min/max", (const char *)m_name);
	}
	else if (0 == strcmp(tag, "KeyState"))
	{
		m_type = eAGIT_String;
		const int nChildren = node->getChildCount();
		for (int i=0; i<nChildren; i++)
		{
			m_keyValues.insert( node->getChild(i)->getAttr("value") );
		}
	}
	else
	{
		CryLogAlways("Unknown state type %s for %s ", tag, (const char *)m_name);
		return false;
	}
	return true;
}

XmlNodeRef CAGInput::ToXml()
{
	XmlNodeRef node = CreateXmlNode("");

	node->setAttr("name", m_name);
	node->setAttr("signalled", m_signalled);
	node->setAttr("priority", m_priority);
	node->setAttr("attachToBlendWeight", m_attachToBlendWeight);
	node->setAttr("blendWeightMoveSpeed", m_blendWeightMoveSpeed);
	node->setAttr("forceGuard", m_forceGuard);
	node->setAttr("mixinFilter", m_mixinFilter);
	if (!m_defaultValue.IsEmpty())
		node->setAttr("defaultValue", m_defaultValue);

	const char * nodeName = 0;
	switch (m_type)
	{
	case eAGIT_Float:
		node->setTag("FloatState");
		node->setAttr("min", m_floatMin);
		node->setAttr("max", m_floatMax);
		break;
	case eAGIT_Integer:
		node->setTag("IntegerState");
		node->setAttr("min", m_intMin);
		node->setAttr("max", m_intMax);
		break;
	case eAGIT_String:
		node->setTag("KeyState");
		for (std::set<CString>::const_iterator iter = m_keyValues.begin(); iter != m_keyValues.end(); ++iter)
		{
			XmlNodeRef value = node->createNode("Key");
			value->setAttr("value", *iter);
			node->addChild(value);
		}
		break;
	default:
		assert(false);
		return XmlNodeRef();
	}
	return node;
}

TAGPropMap CAGInput::GetGeneralProperties()
{
	TAGPropMap pm;
	CVarBlockPtr pVB = new CVarBlock();
	pm[""] = pVB;
	VariableHelper( pVB, "Name", m_name )->AddOnSetCallback( CNotifyCallback<CAGInput>(this, &CAGInput::SetName) );
	VariableHelper( pVB, "Signalled", m_signalled );
	VariableHelper( pVB, "DefaultValue", m_defaultValue );
	VariableHelper( pVB, "Guarded", m_forceGuard );
	VariableHelper( pVB, "Mixin Filter", m_mixinFilter );
	VariableHelper( pVB, "Priority", m_priority );
	VariableHelper( pVB, "AttachToBlendWeight", m_attachToBlendWeight );
	VariableHelper( pVB, "BlendWeightMoveSpeed", m_blendWeightMoveSpeed );

	_smart_ptr<CVariableEnum<int> > pType = new CVariableEnum<int>();
	pType->SetHumanName( "Type" );
	pType->AddEnumItem( "Key", eAGIT_String );
	pType->AddEnumItem( "Integer", eAGIT_Integer );
	pType->AddEnumItem( "Float", eAGIT_Float );
	pType->Set( int(m_type) );
	pType->AddOnSetCallback( functor(*this, &CAGInput::SetType) );
	pVB->AddVariable(pType);

	return pm;
}

TAGPropMap CAGInput::GetFloatProperties()
{
	TAGPropMap pm;
	CVarBlockPtr pVB = new CVarBlock();
	pm[""] = pVB;

	VariableHelper( pVB, "Minimum", m_floatMin );
	VariableHelper( pVB, "Maximum", m_floatMax );

	return pm;
}

TAGPropMap CAGInput::GetIntegerProperties()
{
	TAGPropMap pm;
	CVarBlockPtr pVB = new CVarBlock();
	pm[""] = pVB;

	VariableHelper( pVB, "Minimum", m_intMin );
	VariableHelper( pVB, "Maximum", m_intMax );

	return pm;
}

std::vector<CString> CAGInput::GetKeyProperties()
{
	std::vector<CString> output;
	std::copy( m_keyValues.begin(), m_keyValues.end(), back_inserter(output) );
	return output;
}

void CAGInput::SetName()
{
	m_name = m_pGraph->OnChangedInputName( this );
}

void CAGInput::SetType( IVariable * pVar )
{
	int value = 0xc64;
	pVar->Get(value);
	switch (value)
	{
	case eAGIT_String:
	case eAGIT_Float:
	case eAGIT_Integer:
		m_type = static_cast<EAnimationGraphInputType>(value);
		m_pGraph->SendInputEvent( eAGIE_ChangeType, this );
		break;
	default:
		assert(false);
		pVar->Set( int(m_type) );
	}
}

/*
 * CAGStateTemplate
 */

bool CAGStateTemplate::Load( XmlNodeRef node )
{
	int n = node->getChildCount();
	m_name = node->getAttr("name");
	m_extend = node->getAttr("extend");
	for (int i=0; i<n; i++)
	{
		XmlNodeRef child = node->getChild(i);
		CString tag = child->getTag();
		if (tag == "Param")
		{
			m_params.insert( std::make_pair( child->getAttr("name"), child->getAttr("type") ) );
		}
		else if (tag == "SelectWhen")
		{
			for (int j=0; j<child->getChildCount(); j++)
				m_selected.insert( child->getChild(j)->getTag() );
		}
	}
	return true;
}

bool CAGStateTemplate::ProcessParents( const CAnimationGraph* pGraph )
{
	int pos = 0;
	CString single = m_extend.Tokenize( ",", pos );
	while ( !single.IsEmpty() )
	{
		CAGStateTemplate* pParent = pGraph->GetTemplate( single );
		if ( pParent && pParent->ProcessParents(pGraph) )
		{
			// inherit params
			std::map<CString, CString>::iterator itParam, itParamEnd = pParent->m_params.end();
			for ( itParam = pParent->m_params.begin(); itParam != itParamEnd; ++itParam )
				m_params.insert( *itParam );

			// inherit select when
			std::set<CString>::iterator itSelect, itSelectEnd = pParent->m_selected.end();
			for ( itSelect = pParent->m_selected.begin(); itSelect != itSelectEnd; ++itSelect )
				m_selected.insert( *itSelect );
		}
		single = m_extend.Tokenize( ",", pos );
	}
	m_extend.Empty();
	return true;
}

/*
 * CAGState
 */

CAGState::CAGState( CAnimationGraph * pGraph ) : 
	m_pGraph(pGraph), 
	m_allowSelect(true), 
	m_canMix(false), 
	m_movementControlMethodH(eMCM_Entity),
	m_movementControlMethodV(eMCM_Entity),
	m_bAnimationControlledView(false),
	m_bHurryable(false),
	m_bNoCollider(false),
	m_cost(100),
	m_includeInGame(false),
	m_turnMul(3.0f),
	m_bSkipFP(false),
	m_iconSnapshotTime(0.5f),
	m_pActiveParameterization(NULL),
	m_bMixedExcludeFromGraph(false)
{
	m_pTemplate = m_pGraph->GetDefaultTemplate();
}

CAGState::CAGState( CAnimationGraph * pGraph, const CString& name ) : 
	m_pGraph(pGraph), 
	m_allowSelect(true), 
	m_name(name), 
	m_canMix(false), 
	m_movementControlMethodH(eMCM_Entity),
	m_movementControlMethodV(eMCM_Entity),
	m_bAnimationControlledView(false),
	m_bHurryable(false),
	m_bNoCollider(false),
	m_cost(100),
	m_includeInGame(false),
	m_turnMul(3.0f),
	m_bSkipFP(false),
	m_iconSnapshotTime(0.5f),
	m_pActiveParameterization(NULL),
	m_bMixedExcludeFromGraph(false)
{
	m_pTemplate = m_pGraph->GetDefaultTemplate();
}

bool CAGState::IsNullState() const
{
	return (m_pTemplate == m_pGraph->GetDefaultTemplate()) && m_extraStateFactories.empty();
}

bool CAGState::HasAnimation() const
{
	// figure out if this state possibly has an animation
	if (IsNullState())
		return false;

//	if (m_name[0] == '+')
//		return true;

	const char * animationParamNames[] = {
		"animation",
		"animation1",
		"animation2",
		"anim",
		"play"
	};
	for (int i=0; i<sizeof(animationParamNames)/sizeof(*animationParamNames); i++)
	{
		if (m_pTemplate->HasParam(animationParamNames[i]))
		{
			const std::map<CString, CString>& m = m_pTemplate->GetParams();
			std::map<CString, CString>::const_iterator iter = m.find(animationParamNames[i]);
			if (iter != m.end())
				if (iter->second[0])
					return true;
		}
	}

	return false;
}

IAGCheckedOperationPtr CAGState::CreateDeleteOp()
{
	_smart_ptr<CAGCompositeOperation> pOp = new CAGCompositeOperation();
	for (CAnimationGraph::state_iterator iter = m_pGraph->StateBegin(); iter != m_pGraph->StateEnd(); ++iter)
	{
		CAGStatePtr pState = *iter;
		if (pState == this)
			continue;
		if (pState->m_pExtend == this)
			pOp->AddOperation( new CAGCheckedOperation<CAGState>(pState, &CAGState::NullifyParent, "Parent state for " + pState->GetName()) );
	}
	for (CAnimationGraph::view_iterator iter = m_pGraph->ViewBegin(); iter != m_pGraph->ViewEnd(); ++iter)
	{
		CAGViewPtr pView = *iter;
		if (!pView->CanAddState(this))
			pOp->AddOperation( new CAGCheckedOperation1<CAGView, CAGStatePtr>(pView, &CAGView::RemoveState, this, "Exists in view " + pView->GetName()) );
	}
	pOp->AddOperation( new CAGCheckedOperation1<CAnimationGraph, CAGStatePtr>(m_pGraph, &CAnimationGraph::RemoveState, this, "") );
	return &*pOp;
}

void CAGState::GetParameterizations( std::vector< TParameterizationId >& paramIds )
{
	paramIds.clear();

	CAGState::CartesianProductHelper productHelper;
	CParamsDeclaration *pDecl = GetParamsDeclaration();
	CParamsDeclaration::const_iterator itDecl;
	for ( itDecl = pDecl->begin(); itDecl != pDecl->end(); ++itDecl )
	{
		productHelper.sets.push_back( std::pair< CString, const TSetOfCString* >(itDecl->first, &itDecl->second) );
	}

	if ( !productHelper.sets.empty() )
	{
		productHelper.Make( paramIds, productHelper.sets.begin() );
	}
}

bool CAGState::IsParameterizationExcluded( const TParameterizationId* pParameterizationId ) 
{
	bool bRet = false;

	if ( pParameterizationId )
	{
		TParameterizationMap::iterator itParamsMap = m_paramsMap.find( *pParameterizationId );
		if ( itParamsMap != m_paramsMap.end() && itParamsMap->second.m_bExcludeFromGraph ) 
		{
			bRet = true;
		}
	}

	return bRet;
}

int CAGState::GetExcludeFromGraph() const
{
	if ( m_bMixedExcludeFromGraph || !m_pActiveParameterization )
		return -1;
	return m_pActiveParameterization->m_bExcludeFromGraph ? 1 : 0;
}

void CAGState::SetExcludeFromGraph( bool bExclude )
{
	assert( m_pActiveParameterization );
	m_bMixedExcludeFromGraph = false;
	m_pActiveParameterization->m_bExcludeFromGraph = bExclude;
}

void CAGState::OptimizeParameterization()
{
	TParameterizationMap::iterator it = m_paramsMap.begin();
	while ( it != m_paramsMap.end() )
	{
		CParamStateOverride& paramOverride = it->second;

		TSelectWhenMap::iterator itSW = paramOverride.m_selectWhen.begin();
		while ( itSW != paramOverride.m_selectWhen.end() )
			if ( itSW->second.type == eCT_UseParent )
				paramOverride.m_selectWhen.erase( itSW++ );
			else
				++itSW;

		std::map< CString, CString >::iterator itTemplates = paramOverride.m_templateParams.begin();
		while ( itTemplates != paramOverride.m_templateParams.end() )
			if ( itTemplates->second == m_templateParams[ itTemplates->first ] )
				paramOverride.m_templateParams.erase( itTemplates++ );
			else
				++itTemplates;

		if ( !paramOverride.m_bExcludeFromGraph && paramOverride.m_selectWhen.empty() && paramOverride.m_templateParams.empty() )
			m_paramsMap.erase( it++ );
		else
			++it;
	}
}

void CAGState::ActivateParameterization( const TParameterizationId* pParameterizationId, const CString& viewName )
{
	if ( !m_vParameterizationIds.empty() )
	{
		// there is a parameterization activated currently
		// make sure all changes are reflected in m_paramsMap

		bool bExcludeFromGraph = m_MultipleStateOverride.m_bExcludeFromGraph;

		for ( TVectorParamIds::iterator itIds = m_vParameterizationIds.begin(); itIds != m_vParameterizationIds.end(); ++itIds )
		{
			TParameterizationMap::iterator itParamsMap = m_paramsMap.find( *itIds );
			if ( itParamsMap != m_paramsMap.end() )
			{
				// that id is already in the map
				CParamStateOverride& paramOverride = itParamsMap->second;

				if ( !m_bMixedExcludeFromGraph )
					paramOverride.m_bExcludeFromGraph = bExcludeFromGraph;

				std::map< CString, CString >::iterator itTemplates = m_MultipleStateOverride.m_templateParams.begin();
				for ( ; itTemplates != m_MultipleStateOverride.m_templateParams.end(); ++itTemplates )
					if ( itTemplates->second != DIFFERENT_VALUES_STRING )
						paramOverride.m_templateParams[ itTemplates->first ] = itTemplates->second;

				for ( TSelectWhenMap::iterator itSelectWhen = paramOverride.m_selectWhen.begin();
					itSelectWhen != paramOverride.m_selectWhen.end(); ++itSelectWhen )
				{
					ECriteriaType currentType = itSelectWhen->second.type;
					ECriteriaType multipleType = eCT_UseParent;
					TSelectWhenMap::iterator swFind = m_MultipleStateOverride.m_selectWhen.find( itSelectWhen->first );
					if ( swFind != m_MultipleStateOverride.m_selectWhen.end() )
						multipleType = swFind->second.type;
					if ( multipleType != eCT_DifferentValues )
					{
						if ( swFind != m_MultipleStateOverride.m_selectWhen.end() )
							itSelectWhen->second = swFind->second;
						else
							itSelectWhen->second.type = eCT_UseParent;
					}
				}

				for ( TSelectWhenMap::iterator itSelectWhen = m_MultipleStateOverride.m_selectWhen.begin();
					itSelectWhen != m_MultipleStateOverride.m_selectWhen.end(); ++itSelectWhen )
				{
					if ( itSelectWhen->second.type != eCT_DifferentValues )
						paramOverride.m_selectWhen[ itSelectWhen->first ] = itSelectWhen->second;
				}
			}
			else
			{
				// that id isn't yet in the map so it will be added here
				CParamStateOverride& paramOverride = m_paramsMap[ *itIds ];

				paramOverride.m_bExcludeFromGraph = m_bMixedExcludeFromGraph ? false : bExcludeFromGraph;

				std::map< CString, CString >::iterator itTemplates = m_MultipleStateOverride.m_templateParams.begin();
				for ( ; itTemplates != m_MultipleStateOverride.m_templateParams.end(); ++itTemplates )
					if ( itTemplates->second != DIFFERENT_VALUES_STRING )
						paramOverride.m_templateParams[ itTemplates->first ] = itTemplates->second;

				for ( TSelectWhenMap::iterator itSelectWhen = m_MultipleStateOverride.m_selectWhen.begin();
					itSelectWhen != m_MultipleStateOverride.m_selectWhen.end(); ++itSelectWhen )
				{
					if ( itSelectWhen->second.type != eCT_DifferentValues )
						paramOverride.m_selectWhen[ itSelectWhen->first ] = itSelectWhen->second;
				}
			}
		}
		m_vParameterizationIds.clear();
	}

	OptimizeParameterization();

	m_activeViewName = viewName;
	m_bMixedExcludeFromGraph = false;

	m_pActiveParameterization = NULL;
	if ( pParameterizationId )
	{
		m_ActiveParameterizationId = *pParameterizationId;

		CartesianProductHelper productHelper;
		productHelper.defaults = *pParameterizationId;

		typedef std::vector< CParamsDeclaration::const_iterator > TVectorOfParamIters;
		TVectorOfParamIters vParamIters;
		typedef std::vector< TSetOfCString::const_iterator > TVectorOfValueIters;
		TVectorOfValueIters vValueIters;
		int iCurrentParam = 0;
		CParamsDeclaration::const_iterator itCurrentParam = m_paramsDeclaration.begin();
		for( TParameterizationId::const_iterator it = pParameterizationId->begin(); it != pParameterizationId->end(); ++it )
		{
			if ( it->second.IsEmpty() )
			{
				CParamsDeclaration::const_iterator itCurrentParam = m_paramsDeclaration.find( it->first );
				if ( itCurrentParam != m_paramsDeclaration.end() )
					productHelper.sets.push_back( std::pair< CString, const TSetOfCString* >(it->first, &itCurrentParam->second) );
			}
		}

		if ( !productHelper.sets.empty() )
		{
			productHelper.Make( m_vParameterizationIds, productHelper.sets.begin() );

			m_MultipleStateOverride.m_bExcludeFromGraph = false;
			m_MultipleStateOverride.m_selectWhen.clear();
			m_MultipleStateOverride.m_templateParams = m_templateParams;

			for ( TVectorParamIds::const_iterator itIds = m_vParameterizationIds.begin(); itIds != m_vParameterizationIds.end(); ++itIds )
			{
				TParameterizationMap::iterator itParamMap = m_paramsMap.find( *itIds );
				if ( itParamMap != m_paramsMap.end() )
				{
					CParamStateOverride& paramOverride = itParamMap->second;
					if ( itIds == m_vParameterizationIds.begin() )
					{
						m_MultipleStateOverride.m_bExcludeFromGraph = paramOverride.m_bExcludeFromGraph;
						m_MultipleStateOverride.m_selectWhen = paramOverride.m_selectWhen;

						std::map< CString, CString >::iterator itTemplates = paramOverride.m_templateParams.begin();
						for ( ; itTemplates != paramOverride.m_templateParams.end(); ++itTemplates )
							m_MultipleStateOverride.m_templateParams[ itTemplates->first ] = itTemplates->second;

						continue;
					}

					if ( !m_bMixedExcludeFromGraph )
						if ( m_MultipleStateOverride.m_bExcludeFromGraph != paramOverride.m_bExcludeFromGraph )
							m_bMixedExcludeFromGraph = true;

					std::map< CString, CString >::iterator itTemplates = paramOverride.m_templateParams.begin();
					for ( ; itTemplates != paramOverride.m_templateParams.end(); ++itTemplates )
					{
						CString& value = m_MultipleStateOverride.m_templateParams[ itTemplates->first ];
						if ( value != DIFFERENT_VALUES_STRING )
							if ( itTemplates->second != value )
								value = DIFFERENT_VALUES_STRING;
					}

					itTemplates = m_MultipleStateOverride.m_templateParams.begin();
					for ( ; itTemplates != m_MultipleStateOverride.m_templateParams.end(); ++itTemplates )
						if ( paramOverride.m_templateParams.find( itTemplates->first ) == paramOverride.m_templateParams.end() &&
							m_templateParams[ itTemplates->first ] != itTemplates->second )
								itTemplates->second = DIFFERENT_VALUES_STRING;

					TSelectWhenMap::iterator itSelectWhen = paramOverride.m_selectWhen.begin();
					for ( ; itSelectWhen != paramOverride.m_selectWhen.end(); ++itSelectWhen )
					{
						ECriteriaType currentType = itSelectWhen->second.type;
						ECriteriaType multipleType = eCT_UseParent;
						TSelectWhenMap::iterator swFind = m_MultipleStateOverride.m_selectWhen.find( itSelectWhen->first );
						if ( swFind != m_MultipleStateOverride.m_selectWhen.end() )
							multipleType = swFind->second.type;
						if ( currentType != multipleType )
							m_MultipleStateOverride.m_selectWhen[ itSelectWhen->first ].type = eCT_DifferentValues;
						else
						{
							switch ( multipleType )
							{
							case eCT_UseParent:
							case eCT_AnyValue:
								break;
							case eCT_ExactValue:
								switch ( itSelectWhen->first->GetType() )
								{
								case eAGIT_Float:
									if ( itSelectWhen->second.floatValue != swFind->second.floatValue )
										m_MultipleStateOverride.m_selectWhen[ itSelectWhen->first ].type = eCT_DifferentValues;
									break;
								case eAGIT_Integer:
									if ( itSelectWhen->second.intValue != swFind->second.intValue )
										m_MultipleStateOverride.m_selectWhen[ itSelectWhen->first ].type = eCT_DifferentValues;
									break;
								case eAGIT_String:
									if ( itSelectWhen->second.strValue != swFind->second.strValue )
										m_MultipleStateOverride.m_selectWhen[ itSelectWhen->first ].type = eCT_DifferentValues;
									break;
								default:
									assert(false);
								}
								break;
							case eCT_MinMax:
								switch ( itSelectWhen->first->GetType() )
								{
								case eAGIT_Float:
									if ( itSelectWhen->second.floatRange.minimum != swFind->second.floatRange.minimum ||
										itSelectWhen->second.floatRange.maximum != swFind->second.floatRange.maximum )
											m_MultipleStateOverride.m_selectWhen[ itSelectWhen->first ].type = eCT_DifferentValues;
									break;
								case eAGIT_Integer:
									if ( itSelectWhen->second.intRange.minimum != swFind->second.intRange.minimum ||
										itSelectWhen->second.intRange.maximum != swFind->second.intRange.maximum )
											m_MultipleStateOverride.m_selectWhen[ itSelectWhen->first ].type = eCT_DifferentValues;
									break;
								case eAGIT_String:
									break;
								default:
									assert(false);
								}
								break;
							default:
								assert(!"add an entry here!");
							}
						}
					}

					itSelectWhen = m_MultipleStateOverride.m_selectWhen.begin();
					for ( ; itSelectWhen != m_MultipleStateOverride.m_selectWhen.end(); ++itSelectWhen )
					{
						ECriteriaType multipleType = itSelectWhen->second.type;
						ECriteriaType currentType = eCT_UseParent;
						TSelectWhenMap::iterator swFind = paramOverride.m_selectWhen.find( itSelectWhen->first );
						if ( swFind != paramOverride.m_selectWhen.end() )
							currentType = swFind->second.type;
						if ( currentType != multipleType )
							itSelectWhen->second.type = eCT_DifferentValues;
						else
						{
							switch ( multipleType )
							{
							case eCT_UseParent:
							case eCT_AnyValue:
								break;
							case eCT_ExactValue:
								switch ( itSelectWhen->first->GetType() )
								{
								case eAGIT_Float:
									if ( itSelectWhen->second.floatValue != swFind->second.floatValue )
										itSelectWhen->second.type = eCT_DifferentValues;
									break;
								case eAGIT_Integer:
									if ( itSelectWhen->second.intValue != swFind->second.intValue )
										itSelectWhen->second.type = eCT_DifferentValues;
									break;
								case eAGIT_String:
									if ( itSelectWhen->second.strValue != swFind->second.strValue )
										itSelectWhen->second.type = eCT_DifferentValues;
									break;
								default:
									assert(false);
								}
								break;
							case eCT_MinMax:
								switch ( itSelectWhen->first->GetType() )
								{
								case eAGIT_Float:
									if ( itSelectWhen->second.floatRange.minimum != swFind->second.floatRange.minimum ||
										itSelectWhen->second.floatRange.maximum != swFind->second.floatRange.maximum )
											itSelectWhen->second.type = eCT_DifferentValues;
									break;
								case eAGIT_Integer:
									if ( itSelectWhen->second.intRange.minimum != swFind->second.intRange.minimum ||
										itSelectWhen->second.intRange.maximum != swFind->second.intRange.maximum )
											itSelectWhen->second.type = eCT_DifferentValues;
									break;
								case eAGIT_String:
									break;
								default:
									assert(false);
								}
								break;
							default:
								assert(!"add an entry here!");
							}
						}
					}
				}
				else
				{
					if ( !m_bMixedExcludeFromGraph )
						if ( m_MultipleStateOverride.m_bExcludeFromGraph )
							m_bMixedExcludeFromGraph = true;

					std::map< CString, CString >::iterator itTemplates = m_templateParams.begin();
					for ( ; itTemplates != m_templateParams.end(); ++itTemplates )
					{
						CString& value = m_MultipleStateOverride.m_templateParams[ itTemplates->first ];
						if ( value != DIFFERENT_VALUES_STRING )
							if ( itTemplates->second != value )
								value = DIFFERENT_VALUES_STRING;
					}

					TSelectWhenMap::iterator itSelectWhen = m_MultipleStateOverride.m_selectWhen.begin();
					for ( ; itSelectWhen != m_MultipleStateOverride.m_selectWhen.end(); ++itSelectWhen )
					{
						ECriteriaType multipleType = itSelectWhen->second.type;
						if ( multipleType != eCT_UseParent )
							itSelectWhen->second.type = eCT_DifferentValues;
					}
				}
			}

			m_pActiveParameterization = m_vParameterizationIds.empty() ? NULL : &m_MultipleStateOverride;
		}
		else
		{
			// one single state is activated (no wildcard parameters)
			m_pActiveParameterization = &m_paramsMap[ *pParameterizationId ];

			// copy non-overridden template params (later they will be removed in OptimizeParameterization if not changed)
			std::map< CString, CString >::iterator itTemplates = m_templateParams.begin();
			for ( ; itTemplates != m_templateParams.end(); ++itTemplates )
				if ( m_pActiveParameterization->m_templateParams.find( itTemplates->first ) == m_pActiveParameterization->m_templateParams.end() )
					m_pActiveParameterization->m_templateParams[ itTemplates->first ] = itTemplates->second;
		}
	}
	else
		m_ActiveParameterizationId.clear();
}

bool CAGState::Load_SelectWhen( TSelectWhenMap& selectWhenMap, XmlNodeRef node ) const
{
	const int nCriteria = node->getChildCount();
	for (int j=0; j<nCriteria; j++)
	{
		XmlNodeRef criteria = node->getChild(j);
		CAGInputPtr pInput;
		SCriteria c;
		if (!(pInput = m_pGraph->FindInput(criteria->getTag())))
		{
			CryLogAlways("Unable to find input %s", criteria->getTag());
			continue;
		}
		if (criteria->haveAttr("min") && criteria->haveAttr("max"))
		{
			c.type = eCT_MinMax;
			switch (pInput->GetType())
			{
			case eAGIT_Float:
				if (!criteria->getAttr("min", c.floatRange.minimum) || !criteria->getAttr("max", c.floatRange.maximum))
					return false;
				break;
			case eAGIT_Integer:
				if (!criteria->getAttr("min", c.intRange.minimum) || !criteria->getAttr("max", c.intRange.maximum))
					return false;
				break;
			case eAGIT_String:
				return false;
			default:
				assert(false);
				return false;
			}
		}
		else if (criteria->haveAttr("value"))
		{
			c.type = eCT_ExactValue;
			switch (pInput->GetType())
			{
			case eAGIT_Float:
				if (!criteria->getAttr("value", c.floatValue))
					return false;
				break;
			case eAGIT_Integer:
				if (!criteria->getAttr("value", c.intValue))
					return false;
				break;
			case eAGIT_String:
				c.strValue = criteria->getAttr("value");
				break;
			default:
				assert(false);
				return false;
			}
		}
		else
		{
			c.type = eCT_AnyValue;
		}
		selectWhenMap.insert( std::make_pair(pInput, c) );
	}
	return true;
}

bool CAGState::Load( XmlNodeRef node )
{
	int iTemp;
	bool bTemp;

	if (node->haveAttr("extend"))
	{
		m_pExtend = m_pGraph->FindState(node->getAttr("extend"));
		if (!m_pExtend)
			CryLogAlways( "Unable to find extend node: %s", node->getAttr("extend") );
	}
	m_allowSelect = true;
		node->getAttr("allowSelect", m_allowSelect);

	m_includeInGame = false;
		node->getAttr("includeInGame", m_includeInGame);

	m_canMix = false;
		node->getAttr("canMix", m_canMix);

	m_movementControlMethodH = eMCM_Entity;
	if (node->getAttr("MovementControlMethodH", iTemp))
	{
		m_movementControlMethodH = (EMovementControlMethod)iTemp;
	}
	else
	{
		if (node->getAttr("animationControlledMovement", bTemp) && bTemp)
		{
			m_movementControlMethodH = eMCM_Animation;
		}
		else
		{
			if (node->getAttr("entityControlledMovement", bTemp) && bTemp)
			{
				m_movementControlMethodH = eMCM_Entity;
			}
		}
	}

	m_movementControlMethodV = eMCM_Entity;
	if (node->getAttr("MovementControlMethodV", iTemp))
		m_movementControlMethodV = (EMovementControlMethod)iTemp;

	m_bAnimationControlledView = false;
		node->getAttr("animationControlledView", m_bAnimationControlledView);

	m_iconSnapshotTime = 0.5f;
		node->getAttr("snapshotTime", m_iconSnapshotTime);

	m_bHurryable = false;
	node->getAttr("hurryable", m_bHurryable);

	m_bNoCollider = false;
	if (!node->getAttr("NoCollider", m_bNoCollider))
			node->getAttr("noCylinder", m_bNoCollider);

	m_turnMul = 3.0f;
	node->getAttr("additionalTurnMultiplier", m_turnMul);

	m_cost = 100;
	node->getAttr("cost", m_cost);
	m_name = node->getAttr("id");
	m_pTemplate = m_pGraph->GetDefaultTemplate();

	m_bSkipFP = false;
	node->getAttr("skipFirstPerson", m_bSkipFP);

	m_mixins.clear();

	m_pTemplate = m_pGraph->GetDefaultTemplate();
	if (XmlNodeRef templateNode = node->findChild("Template"))
	{
		m_pTemplate = m_pGraph->GetTemplate( templateNode->getAttr("name") );
		for (int i=0; i<templateNode->getNumAttributes(); i++)
		{
			const char * key, * value;
			templateNode->getAttributeByIndex(i, &key, &value);
			if ( strcmp("name",key) )
				m_templateParams[key] = value;
		}
	}

	const int nChildren = node->getChildCount();
	for (int i=0; i<nChildren; i++)
	{
		XmlNodeRef child = node->getChild(i);
		const char * tag = child->getTag();
		if (0 == strcmp(tag, "MixIn"))
		{
			m_mixins.insert( child->getAttr("id") );
			m_canMix = true;
		}
		else if (0 == strcmp(tag, "SelectWhen"))
		{
			if ( !Load_SelectWhen(m_selectWhen, child) )
				return false;
		}
		else
		{
			CAGStateFactoryPtr pStateFactory = m_pGraph->FindStateFactory( tag );
			if (!pStateFactory)
			{
				// skip
			}
			else if (!pStateFactory->GetCategory()->IsOverridable())
			{
				m_extraStateFactories.push_back(pStateFactory);
				m_extraStateFactories.back().Load(child);
			}
			/*
			else if (!m_bUseTemplate)
			{
				m_overridableStateFactories[pStateFactory->GetCategory()] = pStateFactory;
				m_overridableStateFactories[pStateFactory->GetCategory()].Load(child);
			}
			*/
		}
	}

	m_paramsDeclaration.clear();
	if (XmlNodeRef parameterizationNode = node->findChild( "Parameterization" ))
	{
		for ( int j = 0; j < parameterizationNode->getChildCount(); ++j )
		{
			XmlNodeRef child = parameterizationNode->getChild(j);
			const char * tag = child->getTag();
			if (0 == strcmp(tag, "Parameter"))
			{
				std::pair< CParamsDeclaration::iterator, bool > result =
					m_paramsDeclaration.insert( CParamsDeclaration::value_type(child->getAttr("id"),TSetOfCString()) );
				TSetOfCString& values = result.first->second;

				int valuesCount = child->getChildCount();
				while ( valuesCount-- )
				{
					XmlNodeRef valueNode = child->getChild(valuesCount);
					const char * tag = valueNode->getTag();
					if (0 == strcmp(tag, "Value"))
						values.insert( valueNode->getAttr("id") );
				}
			}
			else if (0 == strcmp(tag, "Override"))
			{
				TParameterizationId id;
				id.InitEmptyFromDeclaration( m_paramsDeclaration );
				if ( id.FromString( child->getAttr("id") ) == false )
				{
					CString msg;
					msg.Format( "Invalid id \"%s\" in Override node while loading state %s.\n\nIgnoring the node...", child->getAttr("id"), (const char*)m_name );
					AfxMessageBox( msg );
				}
				else
				{
					std::pair< TParameterizationMap::iterator, bool > result =
						m_paramsMap.insert( TParameterizationMap::value_type(id,CParamStateOverride()) );
					CParamStateOverride& paramOverride = result.first->second;

					for ( int i = 0; i < child->getChildCount(); ++i )
					{
						XmlNodeRef childOfOverride = child->getChild(i);
						const char * tag = childOfOverride->getTag();
						if (0 == strcmp(tag, "ExcludeFromGraph"))
						{
							paramOverride.m_bExcludeFromGraph = true;
						}
						else if (0 == strcmp(tag, "SelectWhen"))
						{
							Load_SelectWhen( paramOverride.m_selectWhen, childOfOverride );
						}
						else if (0 == strcmp(tag, "Template"))
						{
							for (int i=0; i<childOfOverride->getNumAttributes(); i++)
							{
								const char * key, * value;
								childOfOverride->getAttributeByIndex(i, &key, &value);
								paramOverride.m_templateParams[key] = value;
							}
						}
					}
				}
			}
		}
	}

	return true;
}

struct CompareGetFirstName
{
	template <class T>
	bool operator()( const T& a, const T& b ) const
	{
		return a.first->GetName() < b.first->GetName();
	}
};

bool CAGState::SelectWhen_ToXml( TSelectWhenMap& selectWhenMap, XmlNodeRef parentNode ) const
{
	if (!selectWhenMap.empty())
	{
		XmlNodeRef selectWhen = parentNode->createNode("SelectWhen");

		std::vector< std::pair<CAGInputPtr, SCriteria> > inputs;
		for (std::map<CAGInputPtr, SCriteria>::const_iterator iter = selectWhenMap.begin(); iter != selectWhenMap.end(); ++iter)
			inputs.push_back(*iter);

		std::sort( inputs.begin(), inputs.end(), CompareGetFirstName() );

		for (std::vector< std::pair<CAGInputPtr, SCriteria> >::const_iterator iter = inputs.begin(); iter != inputs.end(); ++iter)
		{
			XmlNodeRef crit = selectWhen->createNode( iter->first->GetName() );
			bool add = true;
			switch (iter->second.type)
			{
			case eCT_UseParent:
				add = false;
				break;
			case eCT_MinMax:
				switch (iter->first->GetType())
				{
				case eAGIT_Float:
					crit->setAttr("min", iter->second.floatRange.minimum);
					crit->setAttr("max", iter->second.floatRange.maximum);
					break;
				case eAGIT_Integer:
					crit->setAttr("min", iter->second.intRange.minimum);
					crit->setAttr("max", iter->second.intRange.maximum);
					break;
				case eAGIT_String:
					assert(false);
					return false;
					break;
				default:
					assert(false);
				}
				break;
			case eCT_ExactValue:
				switch (iter->first->GetType())
				{
				case eAGIT_Float:
					crit->setAttr("value", iter->second.floatValue);
					break;
				case eAGIT_Integer:
					crit->setAttr("value", iter->second.intValue);
					break;
				case eAGIT_String:
					crit->setAttr("value", iter->second.strValue);
					break;
				default:
					assert(false);
					return false;
				}
				break;
			case eCT_AnyValue:
				break;
			default:
				assert(!"Add criteria type here");
			}
			if (add)
				selectWhen->addChild(crit);
		}
		parentNode->addChild(selectWhen);
	}
	return true;
}

XmlNodeRef CAGState::ToXml()
{
	XmlNodeRef node = CreateXmlNode("State");
	node->setAttr("id", m_name);
	node->setAttr("allowSelect", m_allowSelect);
	node->setAttr("includeInGame", m_includeInGame);
	node->setAttr("canMix", m_canMix);
	node->setAttr("cost", m_cost);
	node->setAttr("MovementControlMethodH", (int)m_movementControlMethodH);
	node->setAttr("MovementControlMethodV", (int)m_movementControlMethodV);
	node->setAttr("animationControlledView", m_bAnimationControlledView);
	node->setAttr("hurryable", m_bHurryable);
	node->setAttr("NoCollider", m_bNoCollider);
	node->setAttr("additionalTurnMultiplier", m_turnMul);
	node->setAttr("skipFirstPerson", m_bSkipFP);
	node->setAttr("snapshotTime", m_iconSnapshotTime);

	if (m_pExtend)
		node->setAttr("extend", m_pExtend->GetName());

	if ( !SelectWhen_ToXml(m_selectWhen, node) )
		return false;

	XmlNodeRef templateNode = node->createNode("Template");
	for (std::map<CString, CString>::const_iterator iter = m_templateParams.begin(); iter != m_templateParams.end(); ++iter)
		templateNode->setAttr(iter->first, iter->second);
	templateNode->setAttr("name", m_pTemplate->GetName());
	node->addChild(templateNode);

	for (std::vector<SStateFactoryInfo>::const_iterator iter = m_extraStateFactories.begin(); iter != m_extraStateFactories.end(); ++iter)
	{
		XmlNodeRef child = iter->pFactory->ToXml( iter->pVarBlock );
		if (!child)
			return XmlNodeRef();
		else
			node->addChild(child);
	}

	for (std::set<CString>::iterator iter = m_mixins.begin(); iter != m_mixins.end(); ++iter)
	{
		XmlNodeRef mixin = node->createNode("MixIn");
		mixin->setAttr("id", *iter);
		node->addChild(mixin);
	}

	if ( !m_paramsDeclaration.empty() )
	{
		OptimizeParameterization();

		XmlNodeRef parameterizationNode = node->createNode( "Parameterization" );
		
		for (CParamsDeclaration::iterator it = m_paramsDeclaration.begin(); it != m_paramsDeclaration.end(); ++it )
		{
			XmlNodeRef parameterNode = parameterizationNode->createNode( "Parameter" );
			parameterNode->setAttr( "id", it->first );
			
			TSetOfCString& values = it->second;
			for (TSetOfCString::iterator itVals = values.begin(); itVals != values.end(); ++itVals )
			{
				XmlNodeRef valueNode = parameterNode->createNode( "Value" );
				valueNode->setAttr( "id", *itVals );
				parameterNode->addChild( valueNode );
			}
			parameterizationNode->addChild( parameterNode );
		}

		for ( TParameterizationMap::iterator it = m_paramsMap.begin(); it != m_paramsMap.end(); ++it )
		{
			CParamStateOverride& paramOverride = it->second;
			XmlNodeRef overrideNode = parameterizationNode->createNode( "Override" );
			overrideNode->setAttr( "id", it->first.AsString() );

			if ( paramOverride.m_bExcludeFromGraph )
			{
				XmlNodeRef excludeNode = overrideNode->createNode( "ExcludeFromGraph" );
				overrideNode->addChild( excludeNode );
			}

			SelectWhen_ToXml( paramOverride.m_selectWhen, overrideNode );

			if ( !paramOverride.m_templateParams.empty() )
			{
				XmlNodeRef templateNode = overrideNode->createNode( "Template" );
				std::map< CString, CString >::const_iterator iter = paramOverride.m_templateParams.begin();
				for ( ; iter != paramOverride.m_templateParams.end(); ++iter )
					templateNode->setAttr( iter->first, iter->second );
				overrideNode->addChild( templateNode );
			}

			parameterizationNode->addChild( overrideNode );
		}

		node->addChild( parameterizationNode );
	}

	return node;
}

TAGPropMap CAGState::GetGeneralProperties()
{
	CVarBlockPtr pVB = new CVarBlock();
	SetGraph( VariableHelper( pVB, "State name", m_name, this, &CAGState::OnChangeName ), m_pGraph );
	SetGraph( VariableHelper_StateSelector( pVB, "Parent state", m_pExtend, m_pGraph, this ), m_pGraph )->AddOnSetCallback( CNotifyCallback<CAGState>( this, &CAGState::OnChangeParent ) );
	SetGraph( VariableHelper( pVB, "Pathfind cost", m_cost ), m_pGraph );
//	SetGraph( VariableHelper( pVB, "Turn Speed Multiplier", m_turnMul ), m_pGraph );
	SetGraph( VariableHelper( pVB, "Allow selection", m_allowSelect ), m_pGraph )->AddOnSetCallback( CNotifyCallback<CAGState>( this, &CAGState::OnChangeParent ) );
	SetGraph( VariableHelper( pVB, "Include in game", m_includeInGame ), m_pGraph );
//	SetGraph( VariableHelper( pVB, "Use template", m_bUseTemplate ), m_pGraph )->AddOnSetCallback( CNotifyCallback<CAGState>( this, &CAGState::OnChangeUseTemplate ) );
	SetGraph( VariableHelper( pVB, "Can mix", m_canMix ), m_pGraph );
//	SetGraph(VariableHelper_MCM(pVB, "Hor. Mvmt. Ctrl.", &m_movementControlMethodH), m_pGraph);
//	SetGraph(VariableHelper_MCM(pVB, "Ver. Mvmt. Ctrl.", &m_movementControlMethodV), m_pGraph);
	SetGraph( VariableHelper( pVB, "Animation controlled view", m_bAnimationControlledView ), m_pGraph );
	SetGraph( VariableHelper( pVB, "Can be hurried?", m_bHurryable ), m_pGraph );
	SetGraph( VariableHelper( pVB, "Skip for players?", m_bSkipFP), m_pGraph );

	for (CAnimationGraph::view_iterator iter = m_pGraph->ViewBegin(); iter != m_pGraph->ViewEnd(); ++iter)
	{
		CString name = (*iter)->GetName();
		if (name.IsEmpty() || name[0] != '+')
			continue;
		VariableHelper_SetInclusion( pVB, "Mix " + name, name, m_mixins, m_pGraph );
	}
	
	/*
	IVariablePtr pIconSnapshotTime = SetGraph( VariableHelper( pVB, "Icon Snapshot Time", m_iconSnapshotTime), m_pGraph);
	pIconSnapshotTime->SetLimits(0.0f, 1.0f);
	pIconSnapshotTime->AddOnSetCallback(CNotifyCallback<CAGState>(this, &CAGState::OnChangeIconSnapshotTime));
	*/

	TAGPropMap pm;
	pm[""] = pVB;
	return pm;
}

TAGPropMap CAGState::GetCriteriaProperties( CAnimationGraphStateEditor * pSE )
{
	TAGPropMap pm;

	CVarBlockPtr pVBMain, pVBInherited, pVBUnused, pVBTemplate;

	for (CAnimationGraph::input_iterator iterInputs = m_pGraph->InputBegin(); /*AllowSelect() &&*/ iterInputs != m_pGraph->InputEnd(); ++iterInputs)
	{
		CAGInputPtr pInput = *iterInputs;

		CVarBlockPtr pVB = new CVarBlock();
		SetGraph( VariableHelper_Criteria( pVB, pInput->GetName(), pInput, this, pSE ), m_pGraph );

		switch ( GetCriteriaType(pInput) )
		{
		case eCT_UseParent:
			CAGState* pParent;
			pParent = this;
			while ( pParent = pParent->GetParent() )
			{
				if ( pParent->GetCriteriaType( pInput ) != eCT_UseParent )
				{
					if ( !pVBInherited )
						pm["Inherited"] = pVBInherited = new CVarBlock();
					pm[ CString("Inherited/") + pInput->GetName() ] = pVB;
					break;
				}
			}
			if ( !pParent )
			{
				if ( !pVBUnused )
					pm["Unused"] = pVBUnused = new CVarBlock();
				pm[ CString("Unused/") + pInput->GetName() ] = pVB;
			}
			break;

		case eCT_DefinedInTemplate:
			if ( !pVBTemplate )
				pm[" Defined in template"] = pVBTemplate = new CVarBlock();
			pm[ CString(" Defined in template/") + pInput->GetName() ] = pVB;
			break;

		default:
			if ( !pVBMain )
				pm[" "] = pVBMain = new CVarBlock();
			pm[ CString(" /") + pInput->GetName() ] = pVB;
		}
	}

	return pm;
}

TAGPropMap CAGState::GetOverridableProperties( CAnimationGraphStateEditor * pSE )
{
	TAGPropMap pm;
	for (CAnimationGraph::category_iterator iterCats = m_pGraph->CategoryBegin(); iterCats != m_pGraph->CategoryEnd(); ++iterCats)
	{
		CAGCategoryPtr pCat = *iterCats;
		if (!pCat->IsOverridable())
			continue;

		CVarBlockPtr pVB = new CVarBlock();
		pm[pCat->GetName()] = pVB;

		SetGraph( VariableHelper_Overridable( pVB, "Implementation", pCat, this, pSE ), m_pGraph );
	}

	return pm;
}

TAGPropMap CAGState::GetExtraProperties()
{
	CVarBlockPtr pVB = new CVarBlock();
	if (m_pExtend)
	{
		CVarBlockPtr pVBFrom = m_pExtend->GetExtraProperties()[""];
		const int n = pVBFrom->GetVarsCount();
		for (int i=0; i<n; i++)
		{
			IVariablePtr pVar = pVBFrom->GetVariable(i);
			if ((pVar->GetFlags() && IVariable::UI_DISABLED) == 0)
			{
				IVariablePtr pVar2 = new CVariable<CString>();
				pVar2->SetDataType( pVar->GetDataType() );
				pVar2->SetFlags( IVariable::UI_DISABLED );
				pVar2->SetHumanName( pVar->GetHumanName() );
				CString temp;
				pVar->Get( temp );
				pVar2->Set( temp );
				pVar = pVar2;
			}
			pVB->AddVariable( pVar );
		}
	}
	for (std::vector<SStateFactoryInfo>::const_iterator iter = m_extraStateFactories.begin(); iter != m_extraStateFactories.end(); ++iter)
	{
		ConstVariableHelper( pVB, "Type", iter->pFactory->GetName() );
		CVarBlockPtr pVBFrom = iter->pVarBlock;
		const int n = pVBFrom->GetVarsCount();
		for (int i=0; i<n; i++)
		{
			pVB->AddVariable( pVBFrom->GetVariable(i) );
		}
	}

	TAGPropMap pm;
	pm[""] = pVB;
	return pm;
}

TAGPropMap CAGState::GetTemplateProperties( CAnimationGraphStateEditor * pSE )
{
	CVarBlockPtr pVB = new CVarBlock();

	SetGraph( VariableHelper_Template( pVB, "Template", m_pTemplate, this, pSE ), m_pGraph );

	TAGPropMap pm;
	pm[""] = pVB;
	return pm;
}

ECriteriaType CAGState::GetCriteriaType( CAGInputPtr pInput ) const
{
	if ( IsSelectionDefinedInTemplate(pInput) )
		return eCT_DefinedInTemplate;

	const TSelectWhenMap& selectWhen = m_pActiveParameterization ? m_pActiveParameterization->m_selectWhen : m_selectWhen;
	std::map<CAGInputPtr, SCriteria>::const_iterator iter = selectWhen.find(pInput);
	if ( iter == selectWhen.end() || iter->second.type == eCT_UseParent )
	{
		if ( m_pActiveParameterization == NULL )
			return eCT_UseParent;
		iter = m_selectWhen.find(pInput);
		if ( iter == m_selectWhen.end() )
			return eCT_UseParent;
		else if ( iter->second.type == eCT_UseParent )
			return eCT_UseParent;
		else
			return eCT_UseDefault;
	}
	else
		return iter->second.type;
}

void CAGState::SetCriteriaType( CAGInputPtr pInput, ECriteriaType type )
{
	TSelectWhenMap& selectWhen = m_pActiveParameterization ? m_pActiveParameterization->m_selectWhen : m_selectWhen;
	selectWhen[pInput].type = type;
}

CVarBlockPtr CAGState::CreateCriteriaVariables( CAGInputPtr pInput, bool enable, CAGState::CParamStateOverride* pParameterization )
{
	TSelectWhenMap& selectWhen = pParameterization ? pParameterization->m_selectWhen : m_selectWhen;

	std::map<CAGInputPtr, SCriteria>::iterator iter;
	iter = selectWhen.find(pInput);
	if (iter == selectWhen.end() || iter->second.type == eCT_UseParent)
	{
		if (pParameterization)
			return CreateCriteriaVariables( pInput, false );
		else if (m_pExtend)
			return m_pExtend->CreateCriteriaVariables( pInput, false );
		else
			return new CVarBlock();
	}
	else
	{
		CVarBlockPtr ret = new CVarBlock();
		switch (iter->second.type)
		{
		case eCT_AnyValue:
		case eCT_DifferentValues:
			break;
		case eCT_ExactValue:
			switch (pInput->GetType())
			{
			case eAGIT_Float:
				VariableHelper( ret, "Value", iter->second.floatValue );
				break;
			case eAGIT_Integer:
				VariableHelper( ret, "Value", iter->second.intValue );
				break;
			case eAGIT_String:
				{
					CCriteriaKeyValueVariable* var = new CCriteriaKeyValueVariable( ret, pInput, iter->second.strValue );
					var->SetHumanName("Value");
					var->SetFlags(0);
					ret->AddVariable(var);
				}
				break;
			default:
				assert(false);
				return CVarBlockPtr();
			}
			break;
		case eCT_MinMax:
			switch (pInput->GetType())
			{
			case eAGIT_Float:
				VariableHelper( ret, "Minimum", iter->second.floatRange.minimum );
				VariableHelper( ret, "Maximum", iter->second.floatRange.maximum );
				break;
			case eAGIT_Integer:
				VariableHelper( ret, "Minimum", iter->second.intRange.minimum );
				VariableHelper( ret, "Maximum", iter->second.intRange.maximum );
				break;
			default:
				assert(false);
				return CVarBlockPtr();
			}
			break;
		default:
			assert(false);
			return CVarBlockPtr();
		}

		for (int i=0; i<ret->GetVarsCount(); i++)
		{
			if (!enable)
			{
				ret->GetVariable(i)->SetFlags( IVariable::UI_DISABLED );
				SetGraph( ret->GetVariable(i), m_pGraph );
			}
		}

		return ret;
	}
}

void CAGState::SetTemplateType( const CString& type )
{
	CAGStateTemplatePtr newType = m_pGraph->GetTemplate(type);
	if (newType == m_pTemplate)
		return;

	m_pGraph->Modified();
	m_pTemplate = newType;
}

void CAGState::SetStateFactoryMode( CAGCategoryPtr pCategory, const CString& mode )
{
	if (mode == GetStateFactoryMode(pCategory))
		return;

	m_pGraph->Modified();

	m_overridableStateFactories.erase(pCategory);
	if (mode == "_parent")
		return;
	if (mode == "_none")
		m_overridableStateFactories[pCategory] = SStateFactoryInfo();
	else
		m_overridableStateFactories[pCategory] = m_pGraph->FindStateFactory( mode );
}

CString CAGState::GetStateFactoryMode( CAGCategoryPtr pCategory ) const
{
	std::map<CAGCategoryPtr, SStateFactoryInfo>::const_iterator iter = m_overridableStateFactories.find(pCategory);
	if (iter == m_overridableStateFactories.end())
		return "_parent";
	else if (iter->second.pFactory)
		return iter->second.pFactory->GetName();
	else
		return "_none";
}

CVarBlockPtr CAGState::GetStateFactoryVarBlock( CAGCategoryPtr pCategory ) const
{
	std::map<CAGCategoryPtr, SStateFactoryInfo>::const_iterator iter = m_overridableStateFactories.find(pCategory);
	assert( iter != m_overridableStateFactories.end() );
	assert( !!iter->second.pVarBlock );
	return iter->second.pVarBlock;
}

void CAGState::OnChangeName()
{
	static bool ignore = false;
	if (ignore)
		return;
	ignore = true;

	CString temp = m_pGraph->OnChangedStateName(this);
	if ( m_name != temp )
	{
		AfxMessageBox( "There is already a state named '" + m_name + "'!\n\nTry again...", MB_OK|MB_ICONERROR );
		m_name = temp;
		m_pGraph->SendStateEvent( eAGSE_CanNotChangeName, this );
	}

	ignore = false;
}

void CAGState::OnChangeParent()
{
	m_pGraph->SendStateEvent( eAGSE_ChangeParent, this );
}

void CAGState::OnChangeUseTemplate()
{
	m_pGraph->SendStateEvent( eAGSE_ChangeUseTemplate, this );
}

void CAGState::OnChangeIconSnapshotTime()
{
	m_pGraph->SendStateEvent( eAGSE_ChangeIconSnapshotTime, this );
}

void CAGState::AddExtraState( CAGStateFactoryPtr pSF )
{
	m_pGraph->Modified();
	m_extraStateFactories.push_back(pSF);
}

void CAGState::RemoveExtraState( int i )
{
	m_pGraph->Modified();
	m_extraStateFactories.erase( m_extraStateFactories.begin()+i );
}

void CAGState::GetExtraStateStrings( std::vector<CString>& items ) const
{
	for (std::vector<SStateFactoryInfo>::const_iterator iter = m_extraStateFactories.begin(); iter != m_extraStateFactories.end(); ++iter)
	{
		CString index;
		index.Format( "%d %s", (iter - m_extraStateFactories.begin()), (const char*)iter->pFactory->GetHumanName() );
		items.push_back( index );
	}
}

bool CAGState::AddParamValue( const char* param, const char* value )
{
	CParamsDeclaration::iterator declaration = m_paramsDeclaration.find( param );
	if ( declaration == m_paramsDeclaration.end() )
		return false;
	TSetOfCString& values = declaration->second;

	ActivateParameterization( NULL );
	if ( values.insert( value ).second == false )
		return false;

	if ( values.size() == 1 )
	{
		// for the first parameter value just keep all the existing entries in m_paramsMap (with modified id's)

		TParameterizationMap old;
		old.swap( m_paramsMap );
		for ( TParameterizationMap::iterator it = old.begin(), itEnd = old.end(); it != itEnd; ++it )
		{
			TParameterizationId paramId( it->first );

			TParameterizationId::iterator itParam = paramId.find( param );
			assert( itParam != paramId.end() );
			assert( itParam->second.IsEmpty() );
			if ( itParam->second.IsEmpty() )
				itParam->second = value;
			m_paramsMap.insert( TParameterizationMap::value_type(paramId, it->second) );
		}
		return true;
	}

	if ( values.size() == 2 )
	{
		// for the second parameter value just duplicate all the existing entries in m_paramsMap (with modified id's)

		TParameterizationMap old;
		old.swap( m_paramsMap );
		for ( TParameterizationMap::iterator it = old.begin(), itEnd = old.end(); it != itEnd; ++it )
		{
			TParameterizationId paramId( it->first );
			m_paramsMap.insert( TParameterizationMap::value_type(paramId, it->second) );

			TParameterizationId::iterator itParam = paramId.find( param );
			assert( itParam != paramId.end() );
			itParam->second = value;
			m_paramsMap.insert( TParameterizationMap::value_type(paramId, it->second) );
		}
		return true;
	}

	// there were two or more parameter values before adding the new one - do the complex merging here

	TSetOfCString::const_iterator itValues = values.begin(), itValuesEnd = values.end();

	TParameterizationMap old;
	old.swap( m_paramsMap );
	TParameterizationMap::iterator itParamMap;
	while ( (itParamMap = old.begin()) != old.end() )
	{
		TParameterizationId paramId( itParamMap->first );
		CParamStateOverride overrides( itParamMap->second );
		old.erase(itParamMap);

		// keep existing entries unmodified
		m_paramsMap.insert( TParameterizationMap::value_type(paramId, overrides) );

		TParameterizationId::iterator itParam = paramId.find( param );
		CString firstValue( itParam->second );
		assert( itParam != paramId.end() );

		for ( itValues = values.begin(); itValues != itValuesEnd; ++itValues )
		{
			if ( *itValues != value && *itValues != firstValue )
			{
				CParamStateOverride currentOverrides;
				{
					// this block prevents accessing itOverride after erase()
					itParam->second = *itValues;
					TParameterizationMap::iterator itOverride = old.find( paramId );
					if ( itOverride == old.end() )
					{
						overrides.m_bExcludeFromGraph = false;
						overrides.m_selectWhen.clear();
						overrides.m_templateParams.clear();
						continue;
					}
					currentOverrides = itOverride->second;

					// keep existing entries unmodified
					m_paramsMap.insert( *itOverride );
					old.erase( itOverride );
				}

				// merge 'Exclude from graph' flag
				if ( overrides.m_bExcludeFromGraph != currentOverrides.m_bExcludeFromGraph )
					overrides.m_bExcludeFromGraph = false;

				// merge selection criteria
				TSelectWhenMap::iterator itSelectWhen = overrides.m_selectWhen.begin();
				while ( itSelectWhen != overrides.m_selectWhen.end() )
				{
					TSelectWhenMap::const_iterator findSW = currentOverrides.m_selectWhen.find( itSelectWhen->first );
					if ( findSW == currentOverrides.m_selectWhen.end() || itSelectWhen->second != findSW->second )
						overrides.m_selectWhen.erase( itSelectWhen++ );
					else
						++itSelectWhen;
				}

				// merge template parameters
				std::map< CString, CString >::iterator itTemplateParam = overrides.m_templateParams.begin();
				while ( itTemplateParam != overrides.m_templateParams.end() )
				{
					std::map< CString, CString >::const_iterator findTP = currentOverrides.m_templateParams.find( itTemplateParam->first );
					if ( findTP == currentOverrides.m_templateParams.end() || itTemplateParam->second != findTP->second )
						overrides.m_templateParams.erase( itTemplateParam++ );
					else
						++itTemplateParam;
				}
			}
		}

		// add the entry if needed
		if ( overrides.m_bExcludeFromGraph || !overrides.m_selectWhen.empty() || !overrides.m_templateParams.empty() )
		{
			itParam->second = value;
			m_paramsMap.insert( TParameterizationMap::value_type(paramId, overrides) );
		}
	}

	return true;
}

bool CAGState::DeleteParamValue( const char* param, const char* value )
{
	CParamsDeclaration::iterator declaration = m_paramsDeclaration.find( param );
	if ( declaration == m_paramsDeclaration.end() )
		return false;
	TSetOfCString& values = declaration->second;

	ActivateParameterization( NULL );
	TSetOfCString::iterator itValue = values.find( value );
	if ( itValue == values.end() )
		return false;
	values.erase( itValue );

	// Update link constraints

	CString paramLinkName('[');
	paramLinkName += param;
	paramLinkName += ']';
	paramLinkName.MakeLower();

	std::vector< CAGLinkPtr > links;
	m_pGraph->StateLinks( links, this, true, true );
	for ( std::vector<CAGLinkPtr>::iterator itLinks = links.begin(), itLinksEnd = links.end(); itLinks != itLinksEnd; ++itLinks )
	{
		CAGLink* pLink = *itLinks;
		CAGLink::TListMappedParams& mapping = pLink->m_listMappedParams;
		CAGLink::TListMappedParams::iterator itMapping = mapping.begin();

		CAGStatePtr from, to;
		pLink->GetLinkedStates( from, to );
		if ( this == from )
		{
			while ( itMapping != mapping.end() )
			{
				if ( itMapping->first == paramLinkName && itMapping->second == param )
				{
					// TODO: Ask for removing the link here, and remove the link instead of removing the link constrain
					mapping.erase( itMapping++ );
				}
				else
					++itMapping;
			}
		}
		else
		{
			while ( itMapping != mapping.end() )
			{
				if ( itMapping->second == paramLinkName && itMapping->first == param )
				{
					// TODO: Ask for removing the link here, and remove the link instead of removing the link constrain
					mapping.erase( itMapping++ );
				}
				else
					++itMapping;
			}
		}
	}

	if ( values.size() == 0 )
	{
		// for the last parameter value just keep all the existing entries in m_paramsMap (with modified id's)

		TParameterizationMap old;
		old.swap( m_paramsMap );
		for ( TParameterizationMap::iterator it = old.begin(), itEnd = old.end(); it != itEnd; ++it )
		{
			TParameterizationId paramId( it->first );
			TParameterizationId::iterator itParam = paramId.find( param );
			assert( itParam != paramId.end() );
			itParam->second.Empty();
			m_paramsMap.insert( TParameterizationMap::value_type(paramId, it->second) );
		}
		return true;
	}

	// if this is not the last parameter value just remove the entries in m_paramsMap if id refers to the removed value

	TParameterizationMap::iterator it = m_paramsMap.begin();
	while ( it != m_paramsMap.end() )
	{
		TParameterizationId paramId( it->first );
		TParameterizationId::iterator itParam = paramId.find( param );
		assert( itParam != paramId.end() );
		if ( itParam->second == value )
			m_paramsMap.erase( it++ );
		else
			++it;
	}
	return true;
}

bool CAGState::RenameParamValue( const char* param, const char* oldValue, const char* newValue )
{
	if ( strcmp(oldValue, newValue) == 0 )
		return false;

	ActivateParameterization( NULL );
	CParamsDeclaration::iterator itDeclaration = m_paramsDeclaration.find( param );
	if ( itDeclaration == m_paramsDeclaration.end() )
		return false;

	TSetOfCString& values = itDeclaration->second;
	{
		TSetOfCString::iterator itValue = values.find( oldValue );
		if ( itValue == values.end() )
			return false;
		if ( values.insert(newValue).second == false )
			return false;
		values.erase( itValue );
	}

	TParameterizationMap old;
	old.swap( m_paramsMap );
	for ( TParameterizationMap::iterator it = old.begin(), itEnd = old.end(); it != itEnd; ++it )
	{
		TParameterizationId paramId( it->first );
		TParameterizationId::iterator itParam = paramId.find( param );
		assert( itParam != paramId.end() );
		if ( itParam->second == oldValue )
			itParam->second = newValue;
		m_paramsMap.insert( TParameterizationMap::value_type(paramId, it->second) );
	}

	// Update link constraints

	CString paramLinkName('[');
	paramLinkName += param;
	paramLinkName += ']';
	paramLinkName.MakeLower();

	std::vector< CAGLinkPtr > links;
	m_pGraph->StateLinks( links, this, true, true );
	for ( std::vector<CAGLinkPtr>::iterator itLinks = links.begin(), itLinksEnd = links.end(); itLinks != itLinksEnd; ++itLinks )
	{
		CAGLink* pLink = *itLinks;
		CAGLink::TListMappedParams& mapping = pLink->m_listMappedParams;

		CAGStatePtr from, to;
		pLink->GetLinkedStates( from, to );
		if ( this == from )
		{
			for ( CAGLink::TListMappedParams::iterator itMapping = mapping.begin(); itMapping != mapping.end(); ++itMapping )
				if ( itMapping->first == paramLinkName && itMapping->second == oldValue )
					itMapping->second = newValue;
		}
		else
		{
			for ( CAGLink::TListMappedParams::iterator itMapping = mapping.begin(); itMapping != mapping.end(); ++itMapping )
				if ( itMapping->second == paramLinkName && itMapping->first == oldValue )
					itMapping->first = newValue;
		}
	}

	return true;
}

bool CAGState::AddParameter( const CString& name )
{
	ActivateParameterization( NULL );
	if ( m_paramsDeclaration.insert( TMapParams::value_type(name, TSetOfCString()) ).second == false )
		return false;

	TParameterizationMap old;
	old.swap( m_paramsMap );
	for ( TParameterizationMap::iterator it = old.begin(), itEnd = old.end(); it != itEnd; ++it )
	{
		TParameterizationId paramId( it->first );
		paramId.insert( TParameterizationId::value_type(name, CString()) );
		m_paramsMap.insert( TParameterizationMap::value_type(paramId, it->second) );
	}
	return true;
}

bool CAGState::DeleteParameter( const CString& name )
{
	ActivateParameterization( NULL );
	CParamsDeclaration::iterator itDeclaration = m_paramsDeclaration.find( name );
	if ( itDeclaration == m_paramsDeclaration.end() )
		return false;
	TSetOfCString& values = itDeclaration->second;

	if ( values.size() <= 1 )
	{
		// simpler case - only remove the parameter from all id's

		TParameterizationMap old;
		old.swap( m_paramsMap );
		for ( TParameterizationMap::iterator it = old.begin(), itEnd = old.end(); it != itEnd; ++it )
		{
			TParameterizationId paramId( it->first );
			TParameterizationId::iterator itParam = paramId.find( name );
			assert( itParam != paramId.end() );
			paramId.erase( itParam );
			m_paramsMap.insert( TParameterizationMap::value_type(paramId, it->second) );
		}

		// finally delete the parameter declaration
		m_paramsDeclaration.erase( itDeclaration );
		return true;
	}

	// there are two or more parameter values - do the complex merging here

	TSetOfCString::const_iterator itValues = values.begin(), itValuesEnd = values.end();

	TParameterizationMap old;
	old.swap( m_paramsMap );
	TParameterizationMap::iterator itParamMap;
	while ( (itParamMap = old.begin()) != old.end() )
	{
		TParameterizationId paramId( itParamMap->first );
		CParamStateOverride overrides( itParamMap->second );
		old.erase(itParamMap);

		TParameterizationId::iterator itParam = paramId.find( name );
		CString firstValue( itParam->second );
		assert( itParam != paramId.end() );

		for ( itValues = values.begin(); itValues != itValuesEnd; ++itValues )
		{
			if ( *itValues != firstValue )
			{
				CParamStateOverride currentOverrides;
				{
					// this block prevents accessing itOverride after erase()
					itParam->second = *itValues;
					TParameterizationMap::iterator itOverride = old.find( paramId );
					if ( itOverride == old.end() )
					{
						overrides.m_bExcludeFromGraph = false;
						overrides.m_selectWhen.clear();
						overrides.m_templateParams.clear();
						continue;
					}
					currentOverrides = itOverride->second;
					old.erase( itOverride );
				}

				// merge 'Exclude from graph' flag
				if ( overrides.m_bExcludeFromGraph != currentOverrides.m_bExcludeFromGraph )
					overrides.m_bExcludeFromGraph = false;

				// merge selection criteria
				TSelectWhenMap::iterator itSelectWhen = overrides.m_selectWhen.begin();
				while ( itSelectWhen != overrides.m_selectWhen.end() )
				{
					TSelectWhenMap::const_iterator findSW = currentOverrides.m_selectWhen.find( itSelectWhen->first );
					if ( findSW == currentOverrides.m_selectWhen.end() || itSelectWhen->second != findSW->second )
						overrides.m_selectWhen.erase( itSelectWhen++ );
					else
						++itSelectWhen;
				}

				// merge template parameters
				std::map< CString, CString >::iterator itTemplateParam = overrides.m_templateParams.begin();
				while ( itTemplateParam != overrides.m_templateParams.end() )
				{
					std::map< CString, CString >::const_iterator findTP = currentOverrides.m_templateParams.find( itTemplateParam->first );
					if ( findTP == currentOverrides.m_templateParams.end() || itTemplateParam->second != findTP->second )
						overrides.m_templateParams.erase( itTemplateParam++ );
					else
						++itTemplateParam;
				}
			}
		}

		// add the entry if needed
		if ( overrides.m_bExcludeFromGraph || !overrides.m_selectWhen.empty() || !overrides.m_templateParams.empty() )
		{
			paramId.erase( itParam );
			m_paramsMap.insert( TParameterizationMap::value_type(paramId, overrides) );
		}
	}

	// finally delete the parameter declaration
	m_paramsDeclaration.erase( itDeclaration );

	// Update link constraints

	CString paramLinkName('[');
	paramLinkName += name;
	paramLinkName += ']';
	paramLinkName.MakeLower();

	std::vector< CAGLinkPtr > links;
	m_pGraph->StateLinks( links, this, true, true );
	for ( std::vector<CAGLinkPtr>::iterator itLinks = links.begin(), itLinksEnd = links.end(); itLinks != itLinksEnd; ++itLinks )
	{
		CAGLink* pLink = *itLinks;
		CAGLink::TListMappedParams& mapping = pLink->m_listMappedParams;
		CAGLink::TListMappedParams::iterator itMapping = mapping.begin(); 

		CAGStatePtr from, to;
		pLink->GetLinkedStates( from, to );
		if ( this == from )
		{
			while ( itMapping != mapping.end() )
			{
				if ( itMapping->first == paramLinkName )
					mapping.erase( itMapping++ );
				else
					++itMapping;
			}
		}
		else
		{
			while ( itMapping != mapping.end() )
			{
				if ( itMapping->second == paramLinkName )
					mapping.erase( itMapping++ );
				else
					++itMapping;
			}
		}
	}

	return true;
}

bool CAGState::RenameParameter( const CString& oldName, const CString& newName )
{
	if ( oldName.CompareNoCase(newName) == 0 )
		return false;

	ActivateParameterization( NULL );
	CParamsDeclaration::iterator itDeclaration = m_paramsDeclaration.find( oldName );
	if ( itDeclaration == m_paramsDeclaration.end() )
		return false;
	if ( m_paramsDeclaration.insert( CParamsDeclaration::value_type(newName, itDeclaration->second) ).second == false )
		return false;
	m_paramsDeclaration.erase( itDeclaration );

	TParameterizationMap old;
	old.swap( m_paramsMap );
	for ( TParameterizationMap::iterator it = old.begin(), itEnd = old.end(); it != itEnd; ++it )
	{
		TParameterizationId paramId( it->first );
		TParameterizationId::iterator itParam = paramId.find( oldName );
		assert( itParam != paramId.end() );
		paramId.insert( TParameterizationId::value_type(newName, itParam->second) );
		paramId.erase( itParam );
		m_paramsMap.insert( TParameterizationMap::value_type(paramId, it->second) );
	}

	// Update link constraints

	CString paramLinkOldName('[');
	paramLinkOldName += oldName;
	paramLinkOldName += ']';
	paramLinkOldName.MakeLower();

	CString paramLinkNewName('[');
	paramLinkNewName += newName;
	paramLinkNewName += ']';
	paramLinkNewName.MakeLower();

	std::vector< CAGLinkPtr > links;
	m_pGraph->StateLinks( links, this, true, true );
	for ( std::vector<CAGLinkPtr>::iterator itLinks = links.begin(), itLinksEnd = links.end(); itLinks != itLinksEnd; ++itLinks )
	{
		CAGLink* pLink = *itLinks;
		CAGLink::TListMappedParams& mapping = pLink->m_listMappedParams;

		CAGStatePtr from, to;
		pLink->GetLinkedStates( from, to );
		if ( this == from )
		{
			for ( CAGLink::TListMappedParams::iterator itMapping = mapping.begin(); itMapping != mapping.end(); ++itMapping )
				if ( itMapping->first == paramLinkOldName )
					itMapping->first = paramLinkNewName;
		}
		else
		{
			for ( CAGLink::TListMappedParams::iterator itMapping = mapping.begin(); itMapping != mapping.end(); ++itMapping )
				if ( itMapping->second == paramLinkOldName )
					itMapping->second = paramLinkNewName;
		}
	}

	return true;
}


/*
 * CAGLink
 */

CAGLink::CAGLink( CAnimationGraph * pGraph ) : m_pGraph(pGraph), m_forceFollowChance(0), m_cost(100), m_overrideTransitionTime(0.0f)
{
}

bool CAGLink::Load( XmlNodeRef node )
{
	if (!node->getAttr("forceFollowChance", m_forceFollowChance))
		m_forceFollowChance = 0;
	m_cost = 100;
	node->getAttr("cost", m_cost);
	m_overrideTransitionTime = 0.0f;
	node->getAttr("transitionTime", m_overrideTransitionTime);

	m_listMappedParams.clear();
	for ( int i = 0; i < node->getChildCount(); ++i )
	{
		XmlNodeRef child = node->getChild(i);
		if ( !strcmp("Mapping",child->getTag()) )
			m_listMappedParams.push_back(std::make_pair( CString(child->getAttr("from")), CString(child->getAttr("to")) ));
	}
	return true;
}

XmlNodeRef CAGLink::ToXml()
{
	XmlNodeRef pNode = CreateXmlNode("Link");
	pNode->setAttr("forceFollowChance", m_forceFollowChance);
	pNode->setAttr("cost", m_cost);
	pNode->setAttr("transitionTime", m_overrideTransitionTime);

	TListMappedParams::iterator it, itEnd = m_listMappedParams.end();
	for ( it = m_listMappedParams.begin(); it != itEnd; ++it )
	{
		XmlNodeRef pChild = CreateXmlNode("Mapping");
		pChild->setAttr("from", it->first);
		pChild->setAttr("to", it->second);
		pNode->addChild(pChild);
	}
	return pNode;
}

TAGPropMap CAGLink::GetGeneralProperties()
{
	TAGPropMap pm;
	CVarBlockPtr pVB = new CVarBlock();
	pm[""] = pVB;

	VariableHelper( pVB, "Force follow chance", m_forceFollowChance );
	VariableHelper( pVB, "Pathfind cost", m_cost );
	VariableHelper( pVB, "Override transition time", m_overrideTransitionTime );

	return pm;
}

TAGPropMap CAGLink::GetMappingProperties()
{
	TAGPropMap pm;
	CVarBlockPtr pVB = new CVarBlock();
	pm[""] = pVB;

	CAGStatePtr pFromState, pToState;
	GetLinkedStates( pFromState, pToState );
	CLinkParamsVariable* pFromVar = VariableHelper_ParamMapping( pVB, this, pFromState, pToState, true );
	CLinkParamsVariable* pToVar = VariableHelper_ParamMapping( pVB, this, pToState, pFromState, false );

	return pm;
}

void CAGLink::GetLinkedStates( CAGStatePtr& fromState, CAGStatePtr& toState ) const
{
	m_pGraph->FindLinkedStates( this, fromState, toState );
}

void CAGLink::MappingChanged()
{
	m_pGraph->SendLinkEvent( eAGLE_ChangeMapping, this );
}

/*
 * CAGView
 */

CAGView::CAGView( CAnimationGraph * pGraph ) : m_pGraph(pGraph)
{
}

CAGView::CAGView( CAnimationGraph * pGraph, const CString& name ) : m_pGraph(pGraph), m_name(name)
{
}

CAGView::~CAGView()
{
	if (m_pLastCreatedGraph != 0)
		m_pLastCreatedGraph->RemoveListener(this);
}

void CAGView::OnHyperGraphEvent( IHyperNode * pNode, EHyperGraphEvent event )
{
	CAGNodeBase* pAGNode = (CAGNode*)pNode;
	bool updatePos = false;
	bool makeConnections = false;

	if (!pAGNode || pAGNode->GetAGNodeType() == AG_NODE_MAIN)
	{
		switch (event)
		{
		case EHG_NODE_ADD:
			if (m_states.find(pAGNode->GetState()) == m_states.end())
				m_pGraph->Modified();
			m_states[pAGNode->GetState()].hid = pAGNode->GetId();
			CreateNodesForState(pAGNode->GetState(), static_cast<CAGNode*>(pAGNode), pAGNode->IsSelected());
			makeConnections = true;
			updatePos = true;
			break;
		case EHG_NODE_CHANGE:
			{
				updatePos = true;
				PositionNodesForState(pAGNode->GetState(), static_cast<CAGNode*>(pAGNode));
			}
			break;
		case EHG_NODE_DELETE:
			m_pGraph->Modified();
			m_states.erase( pAGNode->GetState() );
			break;
		}
	}

	if (updatePos)
	{
		Gdiplus::PointF& pos = m_states[pAGNode->GetState()].pos;
		if (!pos.Equals(pAGNode->GetPos()))
		{
			pos = pAGNode->GetPos();
			m_pGraph->Modified();
		}
	}

	if (makeConnections)
	{
		CAGStatePtr pLookFor = pAGNode->GetState();
		CHyperGraph * pGraph = (CHyperGraph*) pAGNode->GetGraph();

		typedef std::vector<CAGStatePtr> States;
		States states;
		for (StateMap::const_iterator iter = m_states.begin(); iter != m_states.end(); ++iter)
		{
			if (iter->second.hid == HyperNodeID(-1))
				continue;

			for (StateMap::const_iterator iter2 = m_states.begin(); iter2 != m_states.end(); ++iter2)
			{
				if (iter2->second.hid == HyperNodeID(-1))
					continue;

				if (iter->first != pLookFor && iter2->first != pLookFor)
					continue;

				if (m_pGraph->IsLinked(iter->first, iter2->first))
				{
					CHyperNode * from = (CHyperNode*) pGraph->FindNode( iter->second.hid );
					CHyperNode * to = (CHyperNode*) pGraph->FindNode( iter2->second.hid );
					pGraph->ConnectPorts( from, from->FindPort("out", false), to, to->FindPort("in", true), false );
				}
			}
		}
	}
}

void CAGView::RemoveState( CAGStatePtr pState )
{
	m_states.erase(pState);
}

bool CAGView::Load( XmlNodeRef node )
{
	m_name = node->getAttr("name");
	int nChildren = node->getChildCount();
	for (int i=0; i<nChildren; i++)
	{
		XmlNodeRef state = node->getChild(i);
		if (0 != strcmp("State", state->getTag()))
			return false;
		CAGStatePtr pState = m_pGraph->FindState( state->getAttr("id") );
		if (!pState)
			continue;
		SStateInfo stateInfo;
		if (!state->getAttr("x", stateInfo.pos.X) || !state->getAttr("y", stateInfo.pos.Y))
		{
			CryLogAlways("Unable to get coordinates for view state %s in %s", pState->GetName(), m_name );
			return false;
		}
		m_states[pState] = stateInfo;
	}
	return true;
}

struct PairCompareFirst
{
	template <class T>
	bool operator()( const T& a, const T& b ) const
	{
		return a.first < b.first;
	}
};

XmlNodeRef CAGView::ToXml()
{
	XmlNodeRef node = CreateXmlNode("View");
	node->setAttr("name", m_name);
	std::vector< std::pair<CString, SStateInfo> > stateInfo;
	for (StateMap::const_iterator iter = m_states.begin(); iter != m_states.end(); ++iter)
	{
		stateInfo.push_back( std::make_pair(iter->first->GetName(), iter->second) );
	}

	std::sort(stateInfo.begin(), stateInfo.end(), PairCompareFirst());

	for (std::vector< std::pair<CString, SStateInfo> >::const_iterator iter = stateInfo.begin(); iter != stateInfo.end(); ++iter)
	{
		XmlNodeRef child = CreateXmlNode("State");
		child->setAttr("id", iter->first);
		child->setAttr("x", iter->second.pos.X);
		child->setAttr("y", iter->second.pos.Y);
		node->addChild(child);
	}
	return node;
}

_smart_ptr<CHyperGraph> CAGView::GetHyperGraph()
{
	for (StateMap::iterator iter = m_states.begin(); iter != m_states.end(); ++iter)
	{
		iter->second.hid = HyperNodeID(-1);
	}

	if (m_pLastCreatedGraph)
		m_pLastCreatedGraph->RemoveListener(this);
	m_pLastCreatedGraph = m_pGraph->CreateGraph();
	m_pLastCreatedGraph->AddListener(this);
	CryLogAlways("CAGView::GetHyperGraph 0x%p new pGraph=0x%p m_pGraph=0x%p", this, (CHyperGraph*)m_pLastCreatedGraph, m_pGraph);

	for (StateMap::iterator iter = m_states.begin(); iter != m_states.end(); ++iter)
	{
		Gdiplus::PointF pos = iter->second.pos;

		CAGStatePtr pState = iter->first;
		CAGNode * pNode = (CAGNode*) m_pLastCreatedGraph->CreateNode( pState->GetName(), pos );
		if (pNode)
			m_states[pState].hid = pNode->GetId();
		//pNode->SetPos(pos);

		//CreateNodesForState(iter->first, pNode);
	}

	return m_pLastCreatedGraph;
}

void CAGView::CreateNodesForState(CAGStatePtr pState, CAGNode * pNode, bool bSelected)
{
	if (m_pGraph->IsShowingIcons())
	{
		CAnimationImagePtr image = m_pGraph->GetImageManager()->GetImage(m_pGraph, pState);
		if (image)
		{
			CAGImageNode* pImageNode = (CAGImageNode*) m_pLastCreatedGraph->CreateNode( CString("$image ") + pState->GetName() );
			pImageNode->SetImage(image);
			if (bSelected)
				pImageNode->SetSelected(true);

			pNode->AttachImageNode(pImageNode);
		}

		PositionNodesForState(pState, pNode);
	}
}

void CAGView::PositionNodesForState(CAGStatePtr pState, CAGNode * pNode)
{
	IHyperGraphEnumerator* pEnum = static_cast<CAGNode*>(pNode)->GetChildrenEnumerator();
	for (IHyperNode* pChild = pEnum->GetFirst(); pChild; pChild = pEnum->GetNext())
	{
		CAnimationImagePtr image = m_pGraph->GetImageManager()->GetImage(m_pGraph, pState);
		float width = 0.0f;
		if (image)
			width = image->GetImage()->GetWidth();
		static_cast<CHyperNode*>(pChild)->SetPos(pNode->GetPos() - Gdiplus::PointF(width + 8.0f, 0.0f));
	}
	pEnum->Release();
}

bool CAGView::CanAddState( CAGStatePtr ptr )
{
	return !!ptr && m_states.find(ptr) == m_states.end();
}

bool CAGView::HasState( CAGStatePtr ptr )
{
	return m_states.find(ptr) != m_states.end();
}

CAGNode* CAGView::GetNodeForState(CAGStatePtr pState)
{
	CAGNode* pNode = 0;
	StateMap::iterator itStateEntry = m_states.find(pState);
	HyperNodeID hid = HyperNodeID(-1);
	if (itStateEntry != m_states.end())
		hid = (*itStateEntry).second.hid;
	if (hid != -1)
		pNode = (CAGNode*)m_pLastCreatedGraph->FindNode(hid);
	return pNode;
}

TAGPropMap CAGView::GetGeneralProperties()
{
	TAGPropMap pm;
	CVarBlockPtr pVB = new CVarBlock();
	pm[""] = pVB;

	VariableHelper( pVB, "Name", m_name, this, &CAGView::OnChangeName );

	return pm;
}

void CAGView::OnChangeName()
{
	m_name = m_pGraph->OnChangedViewName( this );
}

/*
 * CAGNode
 */

static CHyperNodePainter_MultiIOBox animGraphPainter;
static CHyperNodePainter_Image animGraphImagePainter;

CAGNodeBase::CAGNodeBase(CAGStatePtr pState)
:	m_pState(pState)
{
}

CAGNode::CAGNode( CHyperGraph * pGraph, HyperNodeID id, CAGStatePtr pState ) : CAGNodeBase(pState)
{
	assert (pState != 0);

	m_pGraph = pGraph;
	m_id = id;


	IVariablePtr pVar = new CVariableVoid();
	pVar->SetHumanName("in");
	pVar->SetName("in");
	CHyperNodePort portIn( pVar, true );
	portIn.bAllowMulti = true;
	AddPort(portIn);

	pVar = new CVariableVoid();
	pVar->SetHumanName("out");
	pVar->SetName("out");
	CHyperNodePort portOut( pVar, false );
	portOut.bAllowMulti = true;
	AddPort(portOut);

	//SetName( pState->GetName() );
	SetClass( pState->GetName() );

	SetPainter(&animGraphPainter);
}

void CAGNode::AttachImageNode(CAGImageNode* pImageNode)
{
	pImageNode->SetParent(this);
	this->m_imageNodes.push_back(pImageNode);
}

/*
 * CAGImageNode
 */
CAGImageNode::CAGImageNode( CHyperGraph * pGraph, HyperNodeID id, CAGStatePtr pState ) : CAGNodeBase(pState)
{
	m_pParent = 0;

	assert (pState != 0);

	m_pGraph = pGraph;
	m_id = id;

	//SetName( pState->GetName() );
	SetClass( pState->GetName() );

	SetPainter(&animGraphImagePainter);
}

/*
 * CAGEdge
 */

void CAGEdge::DrawSpecial( Gdiplus::Graphics * pGr, Gdiplus::PointF where )
{
	if(!pGr) return;
	if (!pLink)
		return;
	int forceFollow = pLink->GetForceFollowChance();
	int	cost = pLink->GetCost();

	if (forceFollow || cost != 100)
	{
		Gdiplus::Font font(L"Tahoma", 9.0f);

		Gdiplus::PointF	pos( where );

		wchar_t buf[32];
		if( forceFollow )
		{
			swprintf( buf, L"%d", forceFollow );
			Gdiplus::SolidBrush brush( Gdiplus::Color(255,255,255) );
			pGr->DrawString( buf, -1, &font, pos, &brush );
		}

// DEJAN:
// The following block crashes without any obvious reason!
// However, currently it isn't really needed and might even be confusing,
// so I commented it out until we decide what to do with it.
/*
		pos.Y -= font.GetHeight( pGr );

		if (cost != 100)
		{
			swprintf( buf, L"%d", cost );
			Gdiplus::SolidBrush brush( Gdiplus::Color(230,230,230) );
			pGr->DrawString( buf, -1, &font, pos, &brush );
		}
*/
	}
}

/*
 * CAnimationGraph
 */

CAnimationGraph::CAnimationGraph(CAnimationImageManagerPtr pImageManager)
:	m_pImageManager(pImageManager)
{
	m_bShowIcons = false;
	m_modified = false;

	IAnimationGraphCategoryIteratorPtr pCatIter = GetISystem()->GetIAnimationGraphSystem()->CreateCategoryIterator();
	IAnimationGraphCategoryIterator::SCategory cat;
	while (pCatIter->Next(cat))
	{
		CAGCategoryPtr pCat = new CAGCategory(cat);
		m_categories[pCat->GetName()] = pCat;
		m_categoryVec.push_back(pCat);
	}

	IAnimationGraphStateFactoryIteratorPtr pSFIter = GetISystem()->GetIAnimationGraphSystem()->CreateStateFactoryIterator();
	IAnimationGraphStateFactoryIterator::SStateFactory sf;
	while (pSFIter->Next(sf))
	{
		CAGStateFactoryPtr pSF = new CAGStateFactory(this, sf);
		m_stateFactories[pSF->GetName()] = pSF;
		m_stateFactoryVec.push_back(pSF);
	}

	LoadTemplates();
}

CAnimationGraph::~CAnimationGraph()
{
	if (m_modified && IDOK == AfxMessageBox( "Save changes to " + m_filename + "?", MB_YESNO|MB_ICONQUESTION ))
		Save();
}

bool CAnimationGraph::Load( const CString& filename )
{
	m_filename = Path::GamePathToFullPath(filename);
	m_filename = Path::GetRelativePath(m_filename);

	m_sCharacterName.Empty();

	XmlParser parser;
	XmlNodeRef root = parser.parse(filename);
	if (!root)
		return false;

	// load character file name
	{
		XmlNodeRef character = root->findChild("Character");
		if ( character )
			character->getAttr( "filename", m_sCharacterName );
	}

	// load inputs
	{
		XmlNodeRef inputs = root->findChild("Inputs");
		if (!inputs)
		{
			CryLogAlways("No inputs");
			return false;
		}
		const int nChildren = inputs->getChildCount();
		for (int i=0; i<nChildren; i++)
		{
			CAGInputPtr pInput = new CAGInput(this);
			if (!pInput->Load(inputs->getChild(i)))
			{
				CryLogAlways("Failed loading input %d", i);
				return false;
			}
			if (!AddInput(pInput))
			{
				CryLogAlways("Failed adding input %d", i);
				return false;
			}
		}
	}
	// load states
	{
		XmlNodeRef states = root->findChild("States");
		if (!states)
			return false;
		std::queue<XmlNodeRef> statesToLoad;
		const int nChildren = states->getChildCount();
		for (int i=0; i<nChildren; i++)
			statesToLoad.push( states->getChild(i) );
		int skipCount = 0;
		bool allLoadedOk = true;
		while (!statesToLoad.empty() && skipCount < statesToLoad.size())
		{
			XmlNodeRef state = statesToLoad.front();
			statesToLoad.pop();
			if (state->haveAttr("extend") && !FindState(state->getAttr("extend")))
			{
				statesToLoad.push(state);
				skipCount ++;
			}
			else
			{
				skipCount = 0;
				CAGStatePtr pState = new CAGState(this);
				if (!pState->Load(state))
				{
					CryLogAlways("Failed loading state %s", state->getAttr("id"));
					allLoadedOk = false;
				}
				if (!AddState(pState))
				{
					CryLogAlways("Failed adding state %s", (const char *)pState->GetName());
					allLoadedOk = false;
				}
			}
		}
		if (skipCount)
		{
			CryLogAlways("Couldn't load all states");

			std::vector<CString> statesThatCouldntBeLoaded;
			while (!statesToLoad.empty())
			{
				statesThatCouldntBeLoaded.push_back( statesToLoad.front()->getAttr("id") );
				statesToLoad.pop();
			}
			std::sort( statesThatCouldntBeLoaded.begin(), statesThatCouldntBeLoaded.end() );
			for (std::vector<CString>::const_iterator iter = statesThatCouldntBeLoaded.begin(); iter != statesThatCouldntBeLoaded.end(); ++iter)
			{
				CryLogAlways("  %d: %s", (iter - statesThatCouldntBeLoaded.begin()), (const char *)*iter);
			}

			return false;
		}
		if (!allLoadedOk)
			return false;
	}
	// load links
	{
		XmlNodeRef links = root->findChild("Transitions");
		if (!links)
			return false;
		const int nChildren = links->getChildCount();
		for (int i=0; i<nChildren; i++)
		{
			XmlNodeRef child = links->getChild(i);
			CAGLinkPtr pLink = new CAGLink(this);
			if (!pLink->Load(child))
				return false;
			CString fromStr = child->getAttr("from");
			CString toStr = child->getAttr("to");
			CAGStatePtr from = FindState( fromStr );
			CAGStatePtr to = FindState( toStr );
			if (!from || !to)
			{
				CryLogAlways("Failed loading link %s->%s", (const char *)fromStr, (const char *)toStr);
				if (!from)
					CryLogAlways("%s does not exist", (const char *)fromStr);
				if (!to)
					CryLogAlways("%s does not exist", (const char *)toStr);
				continue;
			}
			bool ok = AddLink( from, to, pLink );
			if (ok && 0 == strcmp("BiLink", child->getTag()))
				ok &= AddLink( to, from, pLink );
			if (!ok)
			{
				CryLogAlways("Failed adding link %s->%s", (const char *)fromStr, (const char *)toStr);
				continue;
			}
		}
	}
	// load views
	{
		XmlNodeRef views = root->findChild("Views");
		if (!!views)
		{
			const int nChildren = views->getChildCount();
			for (int i=0; i<nChildren; i++)
			{
				XmlNodeRef child = views->getChild(i);
				CAGViewPtr pView = new CAGView(this);
				if (!pView->Load(child))
					return false;
				if (!AddView(pView))
					return false;
			}
		}
	}

	m_modified = false;

	return true;
}

struct CompareGetName
{
	template <class T>
	bool operator()( const T& a, const T& b ) const
	{
		return a->GetName() < b->GetName();
	}
};

struct CompareGetNamePair
{
	template <class T>
	bool operator()( const std::pair<T,T>& a, const std::pair<T,T>& b )
	{
		return a.first->GetName() < b.first->GetName() || (a.first->GetName() == b.first->GetName() && a.second->GetName() < b.second->GetName());
	}
};

template <class T>
static bool BlockToXml( XmlNodeRef parent, const CString& name, const std::set<T>& values )
{
	XmlNodeRef block = parent->createNode(name);

	std::vector<T> saveValues;
	for (std::set<T>::const_iterator iter = values.begin(); iter != values.end(); ++iter)
		saveValues.push_back(*iter);
	std::sort( saveValues.begin(), saveValues.end(), CompareGetName() );

	for (std::vector<T>::const_iterator iter = saveValues.begin(); iter != saveValues.end(); ++iter)
	{
		XmlNodeRef node = (*iter)->ToXml();
		if (!node)
			return false;
		block->addChild(node);
	}

	parent->addChild(block);
	return true;
}

XmlNodeRef CAnimationGraph::ToXml()
{
	XmlNodeRef root = CreateXmlNode("AnimationSetup");
	root->setAttr("version", "1");

	// save character file name
	if ( !m_sCharacterName.IsEmpty() )
	{
		XmlNodeRef character = root->createNode( "Character" );
		character->setAttr( "filename", m_sCharacterName );
		root->addChild( character );
	}

	// save inputs
	if (!BlockToXml( root, "Inputs", m_inputs ))
		return XmlNodeRef();

	// save states
	if (!BlockToXml( root, "States", m_states ))
		return XmlNodeRef();

	// save links
	XmlNodeRef links = root->createNode("Transitions");
	std::vector<TLink> saveTrans;
	for (std::map<TLink, CAGLinkPtr>::const_iterator iter = m_links.begin(); iter != m_links.end(); ++iter)
		saveTrans.push_back(iter->first);
	std::sort( saveTrans.begin(), saveTrans.end(), CompareGetNamePair() );

	for (std::vector<TLink>::const_iterator iter = saveTrans.begin(); iter != saveTrans.end(); ++iter)
	{
		// [Dejan] removed this check - views should not affect the functionality of the graph
		/*
		// both nodes must be contained within one view (otherwise we're keeping information that is not apparent to an animator)
		bool containedInASingleView = false;
		for (view_iterator iterView = ViewBegin(); iterView != ViewEnd(); ++iterView)
		{
			if ((*iterView)->HasState(iter->first) && (*iterView)->HasState(iter->second))
			{
				containedInASingleView = true;
				break;
			}
		}
		if (!containedInASingleView)
		{
			CryLogAlways("Discarded link from %s to %s as it's not contained in any view", iter->first->GetName(), iter->second->GetName());
			continue;
		}
		*/

		// ok, we can save this
		XmlNodeRef node = m_links[*iter]->ToXml();
		if (!node)
			return XmlNodeRef();
		node->setAttr("from", iter->first->GetName());
		node->setAttr("to", iter->second->GetName());
		links->addChild(node);
	}
	root->addChild(links);

	// save views
	if (!BlockToXml( root, "Views", m_views))
		return XmlNodeRef();

	return root;
}

void CAnimationGraph::Save()
{
	if (m_filename.IsEmpty())
		return;

	ICharacterInstance * pInstance = NULL;

	CErrorsRecorder errRecorder;

	if (pInstance)
	{
		GenerateBadCALReport( pInstance->GetIAnimationSet(), this );
		pInstance->Release();
	}
	GenerateOrphanNodesReport(this);
	GenerateNullNodesWithNoForceLeaveReport(this);

	XmlNodeRef xml = ToXml();
	if (!xml)
	{
		AfxMessageBox("Saving Failed");
		return;
	}
	SaveXmlNode( xml,m_filename );

	GetISystem()->GetIAnimationGraphSystem()->ExportXMLToBinary( m_filename, xml );

	m_modified = false;
}

bool CAnimationGraph::AddInput( CAGInputPtr pInput )
{
	const CString& name = pInput->GetName();
	if (m_inputsByName.find(name) != m_inputsByName.end())
		return false;
	if (m_inputs.find(pInput) != m_inputs.end())
		return false;
	m_inputs.insert( pInput );
	m_inputsByName.insert( std::make_pair(name, pInput) );
	Modified();
	return true;
}

CAGInputPtr CAnimationGraph::FindInput( const CString& name ) const
{
	std::map<CString, CAGInputPtr>::const_iterator iter = m_inputsByName.find(name);
	if (iter == m_inputsByName.end())
		return CAGInputPtr();
	else
		return iter->second;
}

bool CAnimationGraph::AddState( CAGStatePtr pState )
{
	const CString& name = pState->GetName();
	if (m_statesByName.find(name) != m_statesByName.end())
		return false;
	if (m_states.find(pState) != m_states.end())
		return false;
	m_states.insert( pState );
	m_statesByName.insert( std::make_pair(name, pState) );
	Modified();
	return true;
}

bool CAnimationGraph::CloneState( CAGStatePtr pState )
{
	// Create unique name
	CString sName;
	int i = 0;
	do 
	{
		++i;
		sName.Format( "%s_%d", pState->GetName(), i);
	} while( m_statesByName.find( sName ) != m_statesByName.end() );

	// Create new node and load params into it
	CAGStatePtr pNewState = new CAGState(this);
	XmlNodeRef pRef = pState->ToXml();	
	pRef->setAttr( "id", sName );
	pNewState->Load( pRef );

	m_states.insert( pNewState );
	m_statesByName.insert( std::make_pair(sName, pNewState) );
	Modified();
	SendStateEvent(eAGSE_Cloned, pNewState); 
	return true;
}

void CAnimationGraph::RemoveState( CAGStatePtr pState )
{
	m_states.erase( pState );
	m_statesByName.erase( pState->GetName() );
	Modified();
	SendStateEvent(eAGSE_Removed, pState);
}

CAGStatePtr CAnimationGraph::FindState( const CString& name ) const
{
	std::map<CString, CAGStatePtr>::const_iterator iter = m_statesByName.find(name);
	if (iter == m_statesByName.end())
		return CAGStatePtr();
	else
		return iter->second;
}

bool CAnimationGraph::AddView( CAGViewPtr pView )
{
	const CString& name = pView->GetName();
	if (m_viewsByName.find(name) != m_viewsByName.end())
		return false;
	if (m_views.find(pView) != m_views.end())
		return false;
	m_views.insert( pView );
	m_viewsByName.insert( std::make_pair(name, pView) );
	Modified();
	return true;
}

CAGViewPtr CAnimationGraph::FindView( const CString& name ) const
{
	std::map<CString, CAGViewPtr>::const_iterator iter = m_viewsByName.find(name);
	if (iter == m_viewsByName.end())
		return CAGViewPtr();
	else
		return iter->second;
}

bool CAnimationGraph::RemoveView( CAGViewPtr pView )
{
	std::set<CAGViewPtr>::iterator it = m_views.find( pView );
	std::map<CString, CAGViewPtr>::iterator itByName = m_viewsByName.find( pView->GetName() );
	if ( it == m_views.end() )
		return false;
	m_views.erase( it );
	m_viewsByName.erase( itByName );
	SendViewEvent( eAGVE_Removed, pView );
	Modified();
	return true;
}

CAGCategoryPtr CAnimationGraph::FindCategory( const CString& name ) const
{
	std::map<CString, CAGCategoryPtr>::const_iterator iter = m_categories.find(name);
	if (iter == m_categories.end())
		return CAGCategoryPtr();
	else
		return iter->second;
}

CAGStateFactoryPtr CAnimationGraph::FindStateFactory( const CString& name ) const
{
	std::map<CString, CAGStateFactoryPtr>::const_iterator iter = m_stateFactories.find(name);
	if (iter == m_stateFactories.end())
		return CAGStateFactoryPtr();
	else
		return iter->second;
}

CAGLinkPtr CAnimationGraph::FindLink(CAGStatePtr from, CAGStatePtr to) const
{
	TLink link(from, to);
	std::map<TLink, CAGLinkPtr>::const_iterator iter = m_links.find(link);
	if (iter == m_links.end())
		return CAGLinkPtr();
	else
		return iter->second;
}

void CAnimationGraph::FindLinkedStates( const CAGLink* link, CAGStatePtr& from, CAGStatePtr& to ) const
{
	std::map<TLink, CAGLinkPtr>::const_iterator it, itEnd = m_links.end();
	for ( it = m_links.begin(); it != itEnd; ++it )
		if ( it->second == link )
		{
			from = it->first.first;
			to = it->first.second;
			return;
		}
	from = to = NULL;
}

bool CAnimationGraph::AddLink( CAGStatePtr from, CAGStatePtr to, CAGLinkPtr pLink )
{
	TLink link(from, to);
	if (m_links.find(link) != m_links.end())
		return false;
	m_links.insert(std::make_pair(link, pLink));
	Modified();
	return true;
}

bool CAnimationGraph::RemoveLink( CAGStatePtr from, CAGStatePtr to )
{
	TLink link(from, to);
	Modified();
	return m_links.erase( link );
}

bool CAnimationGraph::IsLinked( CAGStatePtr from, CAGStatePtr to ) const
{
	return m_links.find( TLink(from, to) ) != m_links.end();
}

void CAnimationGraph::AddListener( IAnimationGraphListener * pListener )
{
	stl::push_back_unique( m_listeners, pListener );
}

void CAnimationGraph::RemoveListener( IAnimationGraphListener * pListener )
{
	stl::find_and_erase( m_listeners, pListener );
}

void CAnimationGraph::SendStateEvent( EAGStateEvent evt, CAGStatePtr pState )
{
	for (std::vector<IAnimationGraphListener*>::const_iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->OnStateEvent( evt, pState );
	}
}

void CAnimationGraph::SendViewEvent( EAGViewEvent evt, CAGViewPtr pView )
{
	for (std::vector<IAnimationGraphListener*>::const_iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->OnViewEvent( evt, pView );
	}
}

void CAnimationGraph::SendLinkEvent( EAGLinkEvent evt, CAGLinkPtr pLink )
{
	for (std::vector<IAnimationGraphListener*>::const_iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->OnLinkEvent( evt, pLink );
	}
	Modified();
}

void CAnimationGraph::SendInputEvent( EAGInputEvent evt, CAGInputPtr pInput )
{
	for (std::vector<IAnimationGraphListener*>::const_iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->OnInputEvent( evt, pInput );
	}
}

template <class T>
CString CAnimationGraph::OnChangedName( T ptr, std::map<CString, T>& byName )
{
	bool isAllowed = (byName.find(ptr->GetName()) == byName.end());

	for (std::map<CString, T>::iterator iter = byName.begin(); iter != byName.end(); ++iter)
	{
		if (iter->second == ptr)
		{
			if (isAllowed)
			{
				byName.erase( iter );
				byName.insert( std::make_pair(ptr->GetName(), ptr) );
				Modified();
			}
			else
				return iter->first;
			break;
		}
	}
	return ptr->GetName();
}

CString CAnimationGraph::OnChangedStateName( CAGStatePtr pState )
{
	CString temp = OnChangedName( pState, m_statesByName );
	if (temp == pState->GetName())
		SendStateEvent( eAGSE_ChangeName, pState );
	return temp;
}

CString CAnimationGraph::OnChangedViewName( CAGViewPtr ptr )
{
	CString temp = OnChangedName( ptr, m_viewsByName );
	if (temp == ptr->GetName())
		SendViewEvent( eAGVE_ChangeName, ptr );
	return temp;
}

CString CAnimationGraph::OnChangedInputName( CAGInputPtr ptr )
{
	CString temp = OnChangedName( ptr, m_inputsByName );
	if (temp == ptr->GetName())
		SendInputEvent( eAGIE_ChangeName, ptr );
	return temp;
}

void CAnimationGraph::ReloadClasses()
{
}

CHyperGraph * CAnimationGraph::CreateGraph()
{
	class CAGHyperGraph : public CHyperGraph
	{
	public:
		CAGHyperGraph( CAnimationGraph * pGraph ) : CHyperGraph(pGraph), m_pGraph(pGraph)
		{
		}

		void RemoveEdge( CHyperEdge *pEdge )
		{
			CAGNode * pAGOut = (CAGNode*) FindNode(pEdge->nodeOut);
			CAGNode * pAGIn = (CAGNode*) FindNode(pEdge->nodeIn);
			m_pGraph->RemoveLink( pAGOut->GetState(), pAGIn->GetState() );
			CHyperGraph::RemoveEdge(pEdge);
		}

	private:
		CHyperEdge * CreateEdge()
		{
			return new CAGEdge();
		}
		void RegisterEdge(CHyperEdge *pEdge, bool fromSpecialDrag)
		{
			CAGNode * pAGOut = (CAGNode*) FindNode(pEdge->nodeOut);
			CAGNode * pAGIn = (CAGNode*) FindNode(pEdge->nodeIn);
			CAGLinkPtr pLink = m_pGraph->FindLink(pAGOut->GetState(), pAGIn->GetState());
			if (!pLink)
			{
				pLink =	new CAGLink(m_pGraph);
				if (fromSpecialDrag)
					pLink->SetForceFollowChance(1);

				// find state parameters named same and automap them
				CAGStatePtr from = pAGOut->GetState();
				CAGStatePtr to = pAGIn->GetState();
				if ( from && to )
				{
					CParamsDeclaration* pFromParams = from->GetParamsDeclaration();
					CParamsDeclaration::iterator it1, it1End = pFromParams->end();
					CParamsDeclaration* pToParams = to->GetParamsDeclaration();
					CParamsDeclaration::iterator it2End = pToParams->end();
					for ( it1 = pFromParams->begin(); it1 != it1End; ++ it1 )
					{
						if ( pToParams->find( it1->first ) != it2End )
						{
							CString paramName;
							paramName = '[';
							paramName += it1->first;
							paramName += ']';
							paramName.MakeLower();
							pLink->m_listMappedParams.push_back( std::make_pair(paramName,paramName) );
						}
					}
				}

				m_pGraph->AddLink( pAGOut->GetState(), pAGIn->GetState(), pLink );
			}
			((CAGEdge*)pEdge)->pLink = pLink;
			CHyperGraph::RegisterEdge(pEdge,fromSpecialDrag);
		}
    
		CAnimationGraph * m_pGraph;
	};

	return new CAGHyperGraph( this );
}

CHyperNode * CAnimationGraph::CreateNode( CHyperGraph* pGraph,const char *sNodeClass,HyperNodeID nodeId, const Gdiplus::PointF& pos, CBaseObject* pObj)
{
	// [Michael S] Normally in HyperGraphManager implementations sNodeClass would indicate the class of node
	// to create. However in this case it is being used to indicate the name of the state being represented.
	// In order to specify that the node should be an image node, sNodeClass should be of the form
	// "$image <state>".

	// Check whether the node is to be an image node.
	static const char imageClassPrefix[] = "$image ";
	int nameLength = strlen(sNodeClass);
	int prefixLength = sizeof(imageClassPrefix) - 1;
	bool isImage = false;
	if (nameLength > prefixLength && memcmp(sNodeClass, imageClassPrefix, prefixLength) == 0)
	{
		isImage = true;
		sNodeClass += prefixLength;
	}

	CHyperNode* pNode = 0;
	CAGStatePtr pState = FindState(sNodeClass);
	if (pState != 0)
	{
		if (isImage)
				pNode = new CAGImageNode( pGraph, nodeId, pState);
		else
				pNode = new CAGNode( pGraph, nodeId, pState);
		pNode->SetPos(pos);
	}

	return pNode;
}

void CAnimationGraph::ChildStates( std::vector<CAGStatePtr>& states, CAGStatePtr parent )
{
	states.resize(0);
	for (CAnimationGraph::state_iterator iter = m_states.begin(); iter != m_states.end(); ++iter)
	{
		CAGState* pParent = (*iter)->GetParent();
		while ( pParent != NULL && pParent != parent )
			pParent = pParent->GetParent();

		if ( pParent == parent )
			states.push_back(*iter);
	}
}

void CAnimationGraph::StatesLinkedTo( std::vector<CAGStatePtr>& states, CAGStatePtr to )
{
	states.resize(0);
	for (std::map<TLink, CAGLinkPtr>::const_iterator iter = m_links.begin(); iter != m_links.end(); ++iter)
	{
		if (iter->first.second == to)
			states.push_back(iter->first.first);
	}
}

void CAnimationGraph::StatesLinkedFrom( std::vector<CAGStatePtr>& states, CAGStatePtr from )
{
	states.resize(0);
	for (std::map<TLink, CAGLinkPtr>::const_iterator iter = m_links.lower_bound( TLink(from, NULL) ); iter != m_links.end() && iter->first.first == from; ++iter)
	{
		states.push_back(iter->first.second);
	}
}

void CAnimationGraph::StateLinks( std::vector<CAGLinkPtr>& links, CAGStatePtr state, bool includeLinksTo, bool includeLinksFrom )
{
	links.resize(0);
	for (std::map<TLink, CAGLinkPtr>::const_iterator iter = m_links.begin(); iter != m_links.end(); ++iter)
	{
		if (includeLinksTo && iter->first.second == state)
			links.push_back(iter->second);
		if (includeLinksFrom && iter->first.first == state)
			links.push_back(iter->second);
	}
	std::sort( links.begin(), links.end() );
	links.resize( std::unique( links.begin(), links.end() ) - links.begin() );
}

void CAnimationGraph::Modified()
{
	if (!m_modified)
	{
		m_modified = true;
		for (std::vector<IAnimationGraphListener*>::const_iterator iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
		{
			(*iter)->OnGraphModified();
		}
	}
}

void CAnimationGraph::GetLinks( TLinkVec& links )
{
	for (std::map<TLink, CAGLinkPtr>::const_iterator iter = m_links.begin(); iter != m_links.end(); ++iter)
		links.push_back(iter->first);
}

void CAnimationGraph::ShowIcons(bool showIcons)
{
	m_bShowIcons = showIcons;
}

void CAnimationGraph::LoadTemplates()
{
	CAGStateTemplatePtr pTemplate = new CAGStateTemplate;
	m_templates.insert(pTemplate);
	m_templatesByName.insert( std::make_pair(pTemplate->GetName(), pTemplate) );

	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	char filename[_MAX_PATH];

	CString path = "Libs/AnimationGraphTemplates";
	CString search = path + "/*.xml";
	intptr_t handle = pCryPak->FindFirst( search, &fd );
	if (handle != -1)
	{
		int res = 0;

		do 
		{
			strcpy( filename, path );
			strcat( filename, "/" );
			strcat( filename, fd.name );

			if (filename[strlen(filename)-1] != '~')
			{
				XmlNodeRef root = GetISystem()->LoadXmlFile( filename );
				if (root)
				{
					pTemplate = new CAGStateTemplate;
					if (!pTemplate->Load( root ))
					{
						CryLogAlways("Failed loading animation graph template %s", filename);
					}
					else
					{
						m_templates.insert(pTemplate);
						m_templatesByName.insert( std::make_pair(pTemplate->GetName(), pTemplate) );
					}
				}
				else
				{
					CryLogAlways("Failed parsing animation graph template %s", filename);
				}
			}

			res = pCryPak->FindNext( handle, &fd );
		}
		while (res >= 0);

		pCryPak->FindClose( handle );
	}

	// process inheritance
	std::set<CAGStateTemplatePtr>::iterator itTemplates, itTemplatesEnd = m_templates.end();
	for ( itTemplates = m_templates.begin(); itTemplates != itTemplatesEnd; ++itTemplates )
	{
		CAGStateTemplate* pTemplate = *itTemplates;
		pTemplate->ProcessParents( this );
	}
}

CAGStateTemplatePtr CAnimationGraph::GetDefaultTemplate() const
{
	std::map<CString, CAGStateTemplatePtr>::const_iterator it = m_templatesByName.find("Default");
	return it != m_templatesByName.end() ? it->second : NULL;
}

CAGStateTemplatePtr CAnimationGraph::GetTemplate( const CString& name ) const
{
	std::map<CString, CAGStateTemplatePtr>::const_iterator iter = m_templatesByName.find(name);
	if (iter == m_templatesByName.end())
		return GetDefaultTemplate();
	return iter->second;
}

void CAnimationGraph::SetCharacterName( const char* szCharacterName )
{
	if ( m_sCharacterName != szCharacterName )
	{
		m_sCharacterName = szCharacterName;
		Modified();
	}
}

const char* CAGStateAnimationEnumerator::s_animationParameterNames[] = {
	"animation",
	"animation1",
	"animation2",
	"idle_animation"
};
const int CAGStateAnimationEnumerator::s_numAnimationParameterNames = sizeof(CAGStateAnimationEnumerator::s_animationParameterNames) / sizeof(CAGStateAnimationEnumerator::s_animationParameterNames[0]);

CAGStateAnimationEnumerator::CAGStateAnimationEnumerator(CAGState* pState, const TParameterizationId& hint)
:	m_pState(pState),
	m_index(0),
	m_hint(hint)
{
	if ( m_pState->IsParameterized() )
		m_pState->GetParamsDeclaration()->GuessParamId( m_hint, m_guessId );
	AdvanceToNextAnimation();
}

void CAGStateAnimationEnumerator::Get( string& animationName ) const
{
	if (!IsDone())
		animationName = m_pState->GetTemplateParameter( s_animationParameterNames[m_index], m_guessId.empty() ? NULL : &m_guessId ).GetString();
}

bool CAGStateAnimationEnumerator::IsDone() const
{
	return m_index >= s_numAnimationParameterNames;
}

void CAGStateAnimationEnumerator::AdvanceToNextAnimation()
{
	while (!IsDone() && !m_pState->HasTemplateParam(s_animationParameterNames[m_index], false))
		++m_index;
}

void CAGStateAnimationEnumerator::Step()
{
	++m_index;
	AdvanceToNextAnimation();
}
