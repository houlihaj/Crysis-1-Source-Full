#ifndef __ANIMATIONGRAPH_H__
#define __ANIMATIONGRAPH_H__

#pragma once

#include <set>
#include <map>
#include "IAnimationGraph.h"
#include "IAnimationGraphSystem.h"
#include "HyperGraph.h"
#include "HyperGraphManager.h"
#include "AnimationImageManager.h"

class CAnimationGraph;
class CAnimationGraphStateEditor;
class CPropertyCtrl;

typedef std::set< CString > TSetOfCString;
typedef std::map< CString, TSetOfCString, stl::less_stricmp<CString> > TMapParams;


class TParameterizationId;


class CParamsDeclaration : public TMapParams
{
public:
	bool GuessParamId( const TParameterizationId& hint, TParameterizationId& returnId ) const;
};


class TParameterizationId : public std::map< CString, CString >
{
public:
	bool operator < ( const TParameterizationId& p ) const
	{
		assert( size() == p.size() );
		std::pair< const_iterator, const_iterator > it2 = std::mismatch( begin(), end(), p.begin() );
		if ( it2.first == end() )
			return false;
		return *it2.first < *it2.second;
	}
	bool operator == ( const TParameterizationId& p ) const
	{
		assert( size() == p.size() );
		std::pair< const_iterator, const_iterator > it2 = std::mismatch( begin(), end(), p.begin() );
		return it2.first == end();
	}
	CString AsString() const
	{
		CString result;
		const_iterator it = begin();
		while ( it != end() )
		{
			result += it->second;
			if ( ++it == end() )
				break;
			result += ',';
		}
		return result;
	}
	bool FromString( const CString& value )
	{
		int i = 0;
		int len = value.GetLength();
		CString token;
		iterator it = begin();
		while ( i <= len )
		{
			int comma = value.Find( ',', i );
			if ( comma == -1 )
				comma = len;
			assert( it != end() );
			if ( it == end() )
				return false;
			it->second = value.Mid( i, comma-i );
			i = comma+1;
			++it;
		}
		assert( it == end() );
		return it == end();
	}
	void InitEmptyFromDeclaration( const CParamsDeclaration& params )
	{
		CParamsDeclaration::const_iterator it, itEnd = params.end();
		for ( it = params.begin(); it != itEnd; ++it )
			insert(value_type( it->first, CString() ));
	}
};

enum ECriteriaType
{
	eCT_AnyValue,
	eCT_MinMax,
	eCT_ExactValue,
	eCT_UseParent,
	eCT_UseDefault,
	eCT_DifferentValues,
	eCT_DefinedInTemplate,
};

struct SCriteria
{
	SCriteria()
	{
		type = eCT_AnyValue;
		floatRange.minimum = 0;
		floatRange.maximum = 0;
		intRange.minimum = 0;
		intRange.maximum = 0;
		floatValue = 0;
		intValue = 0;
	}

	ECriteriaType type;
	struct 
	{
		float minimum;
		float maximum;
	} floatRange;
	struct 
	{
		int minimum;
		int maximum;
	} intRange;
	float floatValue;
	int intValue;
	CString strValue;

	bool operator == ( const SCriteria& rhs ) const
	{
		if ( type != rhs.type )
			return false;
		switch ( type )
		{
		case eCT_AnyValue:
			return true;
		case eCT_MinMax:
			return floatRange.minimum == rhs.floatRange.minimum
				&& floatRange.maximum == rhs.floatRange.maximum
				&& intRange.minimum == rhs.intRange.minimum
				&& intRange.maximum == rhs.intRange.maximum;
		case eCT_ExactValue:
			return floatValue == rhs.floatValue
				&& intValue == rhs.intValue
				&& strValue == rhs.strValue;
		//case eCT_UseParent:
		//case eCT_UseDefault:
		//case eCT_DifferentValues:
		//case eCT_DefinedInTemplate:
		default:
			assert( !"Non-handled switch case in SCriteria operator ==" );
		}
		return false;
	}
	bool operator != ( const SCriteria& rhs ) const { return !(operator==(rhs));}
};


typedef std::map<CString, CVarBlockPtr> TAGPropMap;

// used for telling the user about doing something, before actually doing it!
struct IAGCheckedOperation : public _reference_target_t
{
	virtual CString GetExplanation() = 0;
	virtual void Perform() = 0;
};

typedef _smart_ptr<IAGCheckedOperation> IAGCheckedOperationPtr;

template <class T>
class CAGCheckedOperation : public IAGCheckedOperation
{
public:
	typedef void (T::*OpFunc)();
	CAGCheckedOperation( _smart_ptr<T> ptr, OpFunc func, CString explanation ) : m_ptr(ptr), m_func(func), m_explanation(explanation)
	{
	}

	virtual CString GetExplanation() { return m_explanation; }
	virtual void Perform() { ((&*m_ptr)->*m_func)(); }

private:
	_smart_ptr<T> m_ptr;
	OpFunc m_func;
	CString m_explanation;
};

template <class T, class Param>
class CAGCheckedOperation1 : public IAGCheckedOperation
{
public:
	typedef void (T::*OpFunc)(Param);
	CAGCheckedOperation1( _smart_ptr<T> ptr, OpFunc func, Param param, CString explanation ) : m_ptr(ptr), m_func(func), m_explanation(explanation), m_param(param)
	{
	}

	virtual CString GetExplanation() { return m_explanation; }
	virtual void Perform() { ((&*m_ptr)->*m_func)(m_param); }

private:
	_smart_ptr<T> m_ptr;
	OpFunc m_func;
	CString m_explanation;
	Param m_param;
};

class CAGCompositeOperation : public IAGCheckedOperation
{
public:
	void AddOperation( IAGCheckedOperationPtr op ) { m_ops.push_back(op); }
	virtual CString GetExplanation();
	virtual void Perform();

private:
	std::vector<IAGCheckedOperationPtr> m_ops;
};

class CAGCategory : public _reference_target_t
{
public:
	CAGCategory( const IAnimationGraphCategoryIterator::SCategory& );

	const CString& GetName() const { return m_name; }
	bool IsOverridable() const { return m_bOverridable; }
  bool IsUsableWithTemplate() const { return m_bUsableWithTemplate; }

private:
	CString m_name;
	bool m_bOverridable;
  bool m_bUsableWithTemplate;
};
typedef _smart_ptr<CAGCategory> CAGCategoryPtr;

class CAGStateFactoryParam;
typedef _smart_ptr<CAGStateFactoryParam> CAGStateFactoryParamPtr;
class CAGStateFactoryParam : public _reference_target_t
{
public:
	static CAGStateFactoryParamPtr Create(const IAnimationGraphStateFactoryIterator::SStateFactoryParameter&);

	const CString& GetName() const { return m_name; }
	void AddToVarBlock( CVarBlockPtr pVB );
	void Load( XmlNodeRef node, CVarBlockPtr pVB );
	void Save( XmlNodeRef node, CVarBlockPtr pVB );

protected:
	CAGStateFactoryParam( const IAnimationGraphStateFactoryIterator::SStateFactoryParameter& );

private:
	virtual IVariablePtr CreateVariable() = 0;

	bool m_bRequired;
	CString m_name;
	CString m_humanName;
	CString m_defaultValue;
	AnimationGraphUpgradeFunction m_upgrade;
};

template <class T>
class CAGStateFactoryParamImpl : public CAGStateFactoryParam
{
public:
	CAGStateFactoryParamImpl( const IAnimationGraphStateFactoryIterator::SStateFactoryParameter& p ) : CAGStateFactoryParam(p) {}

private:
	virtual IVariablePtr CreateVariable()
	{
		return new CVariable<T>();
	}
};

class CAGStateFactory;
typedef _smart_ptr<CAGStateFactory> CAGStateFactoryPtr;
class CAGStateFactory : public _reference_target_t
{
public:
	CAGStateFactory( CAnimationGraph * pGraph, const IAnimationGraphStateFactoryIterator::SStateFactory& p );

	const CString& GetName() const { return m_name; }
	const CString& GetHumanName() const { return m_humanName; }
	CAGCategoryPtr GetCategory() const { return m_pCategory; }

	void FillVarBlock( CVarBlockPtr pVB );
	void Load( XmlNodeRef node, CVarBlockPtr pVB );
	XmlNodeRef ToXml( CVarBlockPtr pVB );

private:
	CAGCategoryPtr m_pCategory;
	CString m_name;
	CString m_humanName;
	std::vector<CAGStateFactoryParamPtr> m_params;
};

/*
enum EAGInputType
{
	eAGIT_Integereger,
	eAGIT_Float,
	eAGIT_String
};
*/

class CAGInput : public _reference_target_t
{
public:
	CAGInput( CAnimationGraph * pGraph );
	CAGInput( CAnimationGraph * pGraph, const CString& name );

	bool Load( XmlNodeRef node );
	XmlNodeRef ToXml();

	CAnimationGraph * GetGraph() { return m_pGraph; }

	const CString& GetName() const { return m_name; }
	bool CanUseMinMaxCriteria() const { return m_type != eAGIT_String; }
	EAnimationGraphInputType GetType() const { return m_type; }

	typedef std::set<CString>::const_iterator key_iterator;
	key_iterator KeyBegin() { return m_keyValues.begin(); }
	key_iterator KeyEnd() { return m_keyValues.end(); }

	bool HasKey(const CString& key) const { assert(m_type == eAGIT_String); return m_keyValues.find(key) != m_keyValues.end(); }
	void AddKey(const CString& key) { m_keyValues.insert(key); }
	void RemoveKey(const CString& key) { m_keyValues.erase(key); }

	TAGPropMap GetGeneralProperties();
	TAGPropMap GetFloatProperties();
	TAGPropMap GetIntegerProperties();
	std::vector<CString> GetKeyProperties();

private:
	CAnimationGraph * m_pGraph;
	EAnimationGraphInputType m_type;
	CString m_name;
	float m_floatMin;
	float m_floatMax;
	int m_intMin;
	int m_intMax;
	std::set<CString> m_keyValues;
	bool m_signalled;
	bool m_forceGuard;
	bool m_mixinFilter;
	CString m_defaultValue;
	int m_priority;
	CString m_attachToBlendWeight;
	float m_blendWeightMoveSpeed;

	void SetType( IVariable * pVar );
	void SetName();
};
typedef _smart_ptr<CAGInput> CAGInputPtr;

class CAGState;
typedef _smart_ptr<CAGState> CAGStatePtr;

class CAGLink : public _reference_target_t
{
public:
	CAGLink( CAnimationGraph * pGraph );
	bool Load( XmlNodeRef node );
	XmlNodeRef ToXml();

	int GetForceFollowChance() const { return m_forceFollowChance; }
	void SetForceFollowChance( int chance ) { m_forceFollowChance = chance; }
	int	GetCost() const { return m_cost; }

	void GetLinkedStates( CAGStatePtr& fromState, CAGStatePtr& toState ) const;
	
	TAGPropMap GetGeneralProperties();
	TAGPropMap GetMappingProperties();
	void MappingChanged();

	CAnimationGraph* GetGraph() const { return m_pGraph; }

private:
	CAnimationGraph * m_pGraph;
	int m_forceFollowChance;
	int m_cost;
	float m_overrideTransitionTime;

public:
	typedef std::list< std::pair<CString,CString> > TListMappedParams;
	TListMappedParams m_listMappedParams;
};
typedef _smart_ptr<CAGLink> CAGLinkPtr;

class CAGStateTemplate;
typedef _smart_ptr<CAGStateTemplate> CAGStateTemplatePtr;
class CAGStateTemplate : public _reference_target_t
{
public:
	CAGStateTemplate() : m_name("Default") {}
	bool Load( XmlNodeRef node );
	bool HasParam( const CString& param ) const { return m_params.find(param) != m_params.end(); }
	bool IsSelected( const CString& criteria ) const { return m_selected.find(criteria) != m_selected.end(); }

	const std::map<CString, CString>& GetParams() const { return m_params; }

	CString GetName() const { return m_name; }

	// called only while loading
	bool ProcessParents( const CAnimationGraph* pGraph );

private:
	CString m_name;
	std::map<CString, CString> m_params;
	std::set<CString> m_selected;
	CString m_extend;
};

class CAGState;
typedef _smart_ptr<CAGState> CAGStatePtr;
class CAGState : public _reference_target_t
{
	struct CParamStateOverride;

public:
	struct CartesianProductHelper
	{
		typedef std::vector< std::pair< CString, const TSetOfCString* > > Sets;
		Sets sets;
		TParameterizationId defaults;

		void Make( std::vector< TParameterizationId >& result, Sets::const_iterator itSets )
		{
			if ( itSets == sets.end() )
				result.push_back( defaults );
			else
			{
				CString currentIndex = itSets->first;
				const TSetOfCString* currentValues = itSets->second;
				++itSets;

				for ( TSetOfCString::const_iterator it = currentValues->begin(); it != currentValues->end(); ++it )
				{
					defaults[ currentIndex ] = *it;
					Make( result, itSets );
				}
			}
		}
	};

	CAGState( CAnimationGraph * pGraph );
	CAGState( CAnimationGraph * pGraph, const CString& name );
	bool Load( XmlNodeRef node );
	XmlNodeRef ToXml();

	const CString& GetName() const { return m_name; }
	CAGStatePtr GetParent() const { return m_pExtend; }
	CAnimationGraph * GetGraph() const { return m_pGraph; }
	bool AllowSelect() const { return m_allowSelect; }
	bool IncludeInGame() const { return m_includeInGame; }

	void SetIconSnapshotTime(float iconSnapshotTime) {m_iconSnapshotTime = iconSnapshotTime;}
	float GetIconSnapshotTime() const {return m_iconSnapshotTime;}

	bool IsNullState() const;
	bool HasAnimation() const;

	TAGPropMap GetGeneralProperties();
	TAGPropMap GetCriteriaProperties( CAnimationGraphStateEditor * pSE );
	TAGPropMap GetOverridableProperties( CAnimationGraphStateEditor * pSE );
	TAGPropMap GetExtraProperties();
	TAGPropMap GetTemplateProperties( CAnimationGraphStateEditor * pSE );

	ECriteriaType GetCriteriaType( CAGInputPtr pInput ) const;
	void SetCriteriaType( CAGInputPtr pInput, ECriteriaType type );
	CVarBlockPtr CreateCriteriaVariables( CAGInputPtr pInput, bool enabled = true, CParamStateOverride* pParameterization = NULL );

	CString GetStateFactoryMode( CAGCategoryPtr pCategory ) const;
	void SetStateFactoryMode( CAGCategoryPtr pCategory, const CString& mode );
	CVarBlockPtr GetStateFactoryVarBlock( CAGCategoryPtr pCategory ) const;

	void AddExtraState( CAGStateFactoryPtr );
	void RemoveExtraState( int i );
	void GetExtraStateStrings( std::vector<CString>& items ) const;

	IAGCheckedOperationPtr CreateDeleteOp();

	std::map<CString, CString>& GetTemplateParameters()
	{
		return m_pActiveParameterization ? m_pActiveParameterization->m_templateParams : m_templateParams;
	}
	void SetTemplateType( const CString& type );
	CString GetTemplateType() const { return m_pTemplate->GetName(); }
	bool IsSelectionDefinedInTemplate( const CAGInput* pInput ) const { return m_pTemplate->IsSelected( pInput->GetName() ); }
	bool HasTemplateParam( const CString& name, bool failEmpty = true ) const
	{
		std::map<CString, CString>::const_iterator iter = m_templateParams.find(name);
		if (iter == m_templateParams.end())
			return false;
		if (failEmpty && iter->second.IsEmpty())
			return false;
		return true;
	}
	CString GetTemplateParameter( const CString& name, const TParameterizationId* pParamId = NULL ) const
	{
		const std::map<CString, CString>* pTemplateParams = &m_templateParams;
		if ( pParamId )
		{
			TParameterizationMap::const_iterator it = m_paramsMap.find( *pParamId );
			if ( it != m_paramsMap.end() )
				pTemplateParams = &it->second.m_templateParams;
		}
		std::map<CString, CString>::const_iterator iter = pTemplateParams->find(name);
		if (iter == pTemplateParams->end())
			return "";
		return iter->second;
	}
	void SetTemplateParameter( const CString& name, const CString& value )
	{
		m_templateParams[name] = value;
	}

	size_t GetFactoryCount()
	{
		return m_overridableStateFactories.size() + m_extraStateFactories.size();
	}

private:
	void OnChangeName();
	void OnChangeParent();
	void OnChangeUseTemplate();
	void OnChangeIconSnapshotTime();
	void NullifyParent() { m_pExtend = NULL; }

	CAnimationGraph * m_pGraph;
	CString m_name;
	CAGStatePtr m_pExtend;
	bool m_allowSelect;
	bool m_includeInGame;
	bool m_canMix;
	EMovementControlMethod m_movementControlMethodH;
	EMovementControlMethod m_movementControlMethodV;
	bool m_bAnimationControlledView;
	bool m_bNoCollider;
	bool m_bHurryable;
	bool m_bSkipFP;
	float m_iconSnapshotTime;

	// templates
	CAGStateTemplatePtr m_pTemplate;
	std::map<CString, CString> m_templateParams;
	int m_cost;
	float m_turnMul;

	std::set<CString> m_mixins;

	typedef std::map<CAGInputPtr, SCriteria> TSelectWhenMap;
	TSelectWhenMap m_selectWhen;
	bool SelectWhen_ToXml( TSelectWhenMap& selectWhenMap, XmlNodeRef parentNode ) const;
	bool Load_SelectWhen( TSelectWhenMap& selectWhenMap, XmlNodeRef node ) const;

	struct SStateFactoryInfo
	{
		SStateFactoryInfo()
		{
		}
		SStateFactoryInfo( CAGStateFactoryPtr pFactory )
		{
			this->pFactory = pFactory;
			this->pVarBlock = new CVarBlock();
			this->pFactory->FillVarBlock( this->pVarBlock );
		}
		void Load( XmlNodeRef node )
		{
			this->pFactory->Load( node, pVarBlock );
		}
		CAGStateFactoryPtr pFactory;
		CVarBlockPtr pVarBlock;
	};

	std::map<CAGCategoryPtr, SStateFactoryInfo> m_overridableStateFactories;
	std::vector<SStateFactoryInfo> m_extraStateFactories;

	// parameterization
	CParamsDeclaration m_paramsDeclaration;

	struct CParamStateOverride
	{
		bool m_bExcludeFromGraph;
		TSelectWhenMap m_selectWhen;
		std::map< CString, CString > m_templateParams;
		CParamStateOverride() : m_bExcludeFromGraph(false) {}
	};
	typedef std::map< TParameterizationId, CParamStateOverride > TParameterizationMap;
	TParameterizationMap m_paramsMap;
	CParamStateOverride m_MultipleStateOverride;
	CParamStateOverride* m_pActiveParameterization;
	TParameterizationId m_ActiveParameterizationId;
	typedef std::vector< TParameterizationId > TVectorParamIds;
	TVectorParamIds m_vParameterizationIds;
	CString m_activeViewName;

public:
	bool m_bMixedExcludeFromGraph;
	CParamsDeclaration * GetParamsDeclaration() { return &m_paramsDeclaration; }
	void ActivateParameterization( const TParameterizationId* pParameterizationId, const CString& viewName = "" );
	CParamStateOverride* GetActiveParameterization() const { return m_pActiveParameterization; }
	void ResetParameterization() { ActivateParameterization( NULL ); m_paramsMap.clear(); }
	bool IsParameterized() const { return !m_paramsMap.empty(); }
	void OptimizeParameterization();
	int GetExcludeFromGraph() const;
	void SetExcludeFromGraph( bool bExclude );
	bool IsParameterizationExcluded( const TParameterizationId* pParameterizationId );
	void GetParameterizations( std::vector< TParameterizationId >& paramIds );

	// state parameters manipulation
	bool AddParamValue( const char* param, const char* value );
	bool DeleteParamValue( const char* param, const char* value );
	bool RenameParamValue( const char* param, const char* oldValue, const char* newValue );
	bool AddParameter( const CString& name );
	bool DeleteParameter( const CString& name );
	bool RenameParameter( const CString& oldName, const CString& newName );
};

class CAGNode;
class CAGView : public _reference_target_t, public IHyperGraphListener
{
public:
	CAGView( CAnimationGraph * pGraph );
	CAGView( CAnimationGraph * pGraph, const CString& name );
	virtual ~CAGView();

	bool Load( XmlNodeRef node );
	XmlNodeRef ToXml();

	const CString& GetName() const { return m_name; }
	CAnimationGraph * GetGraph() const { return m_pGraph; }
	_smart_ptr<CHyperGraph> GetHyperGraph();
	void OnHyperGraphEvent( IHyperNode * pNode, EHyperGraphEvent event );
	bool CanAddState( CAGStatePtr );
	void RemoveState( CAGStatePtr );
	bool HasState( CAGStatePtr );
	CAGNode* GetNodeForState(CAGStatePtr pState);

	TAGPropMap GetGeneralProperties();

	void CreateNodesForState(CAGStatePtr pState, CAGNode * pNode, bool bSelected = false);
	void PositionNodesForState(CAGStatePtr pState, CAGNode * pNode);

private:
	void OnChangeName();

	struct SStateInfo
	{
		SStateInfo() : pos(0,0), hid(-1) {}
		SStateInfo( HyperNodeID id ) : pos(0,0), hid(id) {}
		Gdiplus::PointF pos;
		HyperNodeID hid;
	};

	typedef std::map<CAGStatePtr, SStateInfo> StateMap;
	CAnimationGraph * m_pGraph;
	CString m_name;
	StateMap m_states;
	_smart_ptr<CHyperGraph> m_pLastCreatedGraph;
};
typedef _smart_ptr<CAGView> CAGViewPtr;

enum AGNodeType
{
	AG_NODE_MAIN,
	AG_NODE_IMAGE
};

class CAGNodeBase : public CHyperNode
{
public:
	CAGNodeBase(CAGStatePtr pState);

	virtual AGNodeType GetAGNodeType() = 0;

	CAGStatePtr GetState() { return m_pState; }

protected:
	CAGStatePtr m_pState;
};

class CAGImageNode;
class CAGNode : public CAGNodeBase
{
public:
	CAGNode( CHyperGraph * pGraph, HyperNodeID id, CAGStatePtr pState );
	// CHyperNode
	virtual void Init() {}
	virtual void Done() {}
	virtual CHyperNode * Clone()
	{
		return new CAGNode(m_pGraph, m_id, m_pState);
	}

	void AttachImageNode(CAGImageNode* pImageNode);

	virtual AGNodeType GetAGNodeType() {return AG_NODE_MAIN;}

	virtual IHyperGraphEnumerator* GetChildrenEnumerator()
	{ 
		ImageNodeCollection::iterator begin = m_imageNodes.begin();
		ImageNodeCollection::iterator end = m_imageNodes.end();
		return new CHyperGraphIteratorEnumerator<ImageNodeCollection::iterator>(begin, end);
	}

private:
	typedef std::vector<CAGImageNode*> ImageNodeCollection;
	std::vector<CAGImageNode*> m_imageNodes;
};

class CAGImageNode : public CAGNodeBase
{
public:
	CAGImageNode( CHyperGraph * pGraph, HyperNodeID id, CAGStatePtr pState );
	// CHyperNode
	virtual void Init() {}
	virtual void Done() {}
	virtual CHyperNode * Clone()
	{
		return new CAGNode(m_pGraph, m_id, m_pState);
	}

	void SetParent(CAGNode* pNode) {m_pParent = pNode;}
	void SetImage(CAnimationImagePtr pImage) {m_pImage = pImage;}
	CAnimationImagePtr GetImage() {return m_pImage;}

	virtual AGNodeType GetAGNodeType() {return AG_NODE_IMAGE;}

	virtual IHyperNode* GetParent() { return m_pParent; }

private:
	CAGNode* m_pParent;
	CAnimationImagePtr m_pImage;
};

class CAGEdge : public CHyperEdge
{
public:
	bool IsEditable() { return true; }
	void DrawSpecial(Gdiplus::Graphics * pGr, Gdiplus::PointF where);
	CAGLinkPtr pLink;
};

enum EAGStateEvent
{
	eAGSE_ChangeName,
	eAGSE_ChangeParent,
	eAGSE_ChangeUseTemplate,
	eAGSE_Removed,
	eAGSE_ChangeIconSnapshotTime,
	eAGSE_CanNotChangeName,
	eAGSE_Cloned,
};

enum EAGViewEvent
{
	eAGVE_ChangeName,
	eAGVE_Removed,
};

enum EAGInputEvent
{
	eAGIE_ChangeName,
	eAGIE_ChangeType
};

enum EAGLinkEvent
{
	eAGLE_ChangeMapping,
};

struct IAnimationGraphListener
{
	virtual void OnStateEvent( EAGStateEvent, CAGStatePtr pState ) {}
	virtual void OnViewEvent( EAGViewEvent, CAGViewPtr pView ) {}
	virtual void OnInputEvent( EAGInputEvent, CAGInputPtr pInput ) {}
	virtual void OnLinkEvent( EAGLinkEvent, CAGLinkPtr pLink ) {}
	virtual void OnGraphModified() {}
};

class CAnimationGraph : public CHyperGraphManager, public _reference_target_t
{
public:
	CAnimationGraph(CAnimationImageManagerPtr pImageManager);
	~CAnimationGraph();

	// CHyperGraphManager
	void ReloadClasses();
	CHyperGraph * CreateGraph();
	CHyperNode * CreateNode( CHyperGraph* pGraph,const char *sNodeClass,HyperNodeID nodeId, const Gdiplus::PointF& pos, CBaseObject* pObj = NULL);
	// ~CHyperGraphManager

	bool Load( const CString& filename );
	void SetFilename( const CString& filename ) { m_filename = filename; }
	CString GetFilename() const { return m_filename; }
	XmlNodeRef ToXml();
	void Save();

	void AddListener( IAnimationGraphListener * pListener );
	void RemoveListener( IAnimationGraphListener * pListener );

	bool AddInput( CAGInputPtr );
	bool RemoveInput( CAGInputPtr );
	CAGInputPtr FindInput( const CString& ) const;
	bool AddState( CAGStatePtr );
	bool CloneState( CAGStatePtr );
	void RemoveState( CAGStatePtr );
	CAGStatePtr FindState( const CString& ) const;
	bool AddLink( CAGStatePtr from, CAGStatePtr to, CAGLinkPtr link );
	bool RemoveLink( CAGStatePtr from, CAGStatePtr to );
	CAGLinkPtr FindLink( CAGStatePtr from, CAGStatePtr to ) const;
	void FindLinkedStates( const CAGLink* link, CAGStatePtr& from, CAGStatePtr& to ) const;

	bool IsLinked( CAGStatePtr from, CAGStatePtr to ) const;
	CAGCategoryPtr FindCategory( const CString& ) const;
	CAGStateFactoryPtr FindStateFactory( const CString& ) const;
	bool AddView( CAGViewPtr );
	bool RemoveView( CAGViewPtr );
	CAGViewPtr FindView( const CString& ) const;

	typedef std::set<CAGStatePtr>::const_iterator state_iterator;
	state_iterator StateBegin() const { return m_states.begin(); }
	state_iterator StateEnd() const { return m_states.end(); }

	typedef std::set<CAGInputPtr>::const_iterator input_iterator;
	input_iterator InputBegin() const { return m_inputs.begin(); }
	input_iterator InputEnd() const { return m_inputs.end(); }

	typedef std::vector<CAGCategoryPtr>::const_iterator category_iterator;
	category_iterator CategoryBegin() const { return m_categoryVec.begin(); }
	category_iterator CategoryEnd() const { return m_categoryVec.end(); }

	typedef std::vector<CAGStateFactoryPtr>::const_iterator state_factory_iterator;
	state_factory_iterator StateFactoryBegin() const { return m_stateFactoryVec.begin(); }
	state_factory_iterator StateFactoryEnd() const { return m_stateFactoryVec.end(); }

	typedef std::set<CAGViewPtr>::const_iterator view_iterator;
	view_iterator ViewBegin() const { return m_views.begin(); }
	view_iterator ViewEnd() const { return m_views.end(); }

	typedef std::set<CAGStateTemplatePtr>::const_iterator state_template_iterator;
	state_template_iterator StateTemplateBegin() const { return m_templates.begin(); }
	state_template_iterator StateTemplateEnd() const { return m_templates.end(); }
	CAGStateTemplatePtr GetDefaultTemplate() const; 
	CAGStateTemplatePtr GetTemplate( const CString& name ) const;

	void ChildStates( std::vector<CAGStatePtr>& states, CAGStatePtr parent );
	void StatesLinkedTo( std::vector<CAGStatePtr>& states, CAGStatePtr to );
	void StatesLinkedFrom( std::vector<CAGStatePtr>& states, CAGStatePtr from );
	void StateLinks( std::vector<CAGLinkPtr>& links, CAGStatePtr state, bool includeLinksTo, bool includeLinksFrom );

	CString OnChangedStateName( CAGStatePtr ptr );
	CString OnChangedViewName( CAGViewPtr ptr );
	CString OnChangedInputName( CAGInputPtr ptr );
	void SendStateEvent( EAGStateEvent, CAGStatePtr );
	void SendViewEvent( EAGViewEvent, CAGViewPtr );
	void SendInputEvent( EAGInputEvent, CAGInputPtr );
	void SendLinkEvent( EAGLinkEvent, CAGLinkPtr );

	bool IsModified() const { return m_modified; }
	void Modified();

	typedef std::pair<CAGStatePtr, CAGStatePtr> TLink;
	typedef std::vector<TLink> TLinkVec;
	void GetLinks( TLinkVec& links );

	const CString& GetCharacterName() const { return m_sCharacterName; }
	void SetCharacterName( const char* szCharacterName );

	CAnimationImageManagerPtr GetImageManager() {return m_pImageManager;}

	void ShowIcons(bool showIcons);
	bool IsShowingIcons() {return m_bShowIcons;}

private:
	template <class T>
	CString OnChangedName( T ptr, std::map<CString, T>& byName );

	void LoadTemplates();

	CString m_filename;
	bool m_modified;

	std::set<CAGInputPtr> m_inputs;
	std::map<CString, CAGInputPtr> m_inputsByName;
	std::set<CAGStatePtr> m_states;
	std::map<CString, CAGStatePtr> m_statesByName;
	std::map<TLink, CAGLinkPtr> m_links;
	std::set<CAGViewPtr> m_views;
	std::map<CString, CAGViewPtr> m_viewsByName;
	std::set<CAGStateTemplatePtr> m_templates;
	std::map<CString, CAGStateTemplatePtr> m_templatesByName;

	std::vector<IAnimationGraphListener*> m_listeners;

	CString m_sCharacterName;
	CAnimationImageManagerPtr m_pImageManager;
	bool m_bShowIcons;

	// metadata... kept here because I can't think of a better place to keep it
	std::vector<CAGCategoryPtr> m_categoryVec;
	std::map<CString, CAGCategoryPtr> m_categories;
	std::map<CString, CAGStateFactoryPtr> m_stateFactories;
	std::vector<CAGStateFactoryPtr> m_stateFactoryVec;
};
typedef _smart_ptr<CAnimationGraph> CAnimationGraphPtr;

template <class T>
CString GenerateName( CAnimationGraph* pGraph, T (CAnimationGraph::*func)(const CString&) const, const CString& base )
{
	for (int i=0; ; i++)
	{
		CString temp;
		temp.Format("%s%d", (const char*)base, i);
		if (!(pGraph->*func)(temp))
			return temp;
	}
}

class CAGStateAnimationEnumerator
{
public:
	CAGStateAnimationEnumerator(CAGState* pState, const TParameterizationId& hint);
	void Get(string& animationName) const;
	bool IsDone() const;
	void Step();

private:
	void AdvanceToNextAnimation();

	static const char* s_animationParameterNames[];
	static const int s_numAnimationParameterNames;

	CAGState* m_pState;
	int m_index;

	const TParameterizationId& m_hint;
	TParameterizationId m_guessId;
};

#endif
