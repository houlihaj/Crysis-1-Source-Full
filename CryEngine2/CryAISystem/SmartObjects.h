#ifndef _SMARTOBJECTS_H_
#define _SMARTOBJECTS_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include <IEntitySystem.h>
#include "STLPoolAllocator.h"

// forward declaration
class CSmartObject;
class CSmartObjectClass;
class CPuppet;
struct CCondition;
struct IAIAction;

enum EApproachStance
{
	eAS_Ignore,
	eAS_Prone,
	eAS_Crouch,
	eAS_Combat,
	eAS_Relaxed,
	eAS_Stealth,
};

struct SNavSOStates
{
	SNavSOStates() : objectEntId(0) {}

	void	Clear()
	{
		objectEntId = 0;
		sAnimationDoneUserStates.clear();
		sAnimationDoneObjectStates.clear();
		sAnimationFailUserStates.clear();
		sAnimationFailObjectStates.clear();
	}

	bool	IsEmpty() const { return objectEntId == 0; }

	void Serialize(TSerialize ser, class CObjectTracker& objectTracker)
	{
		ser.BeginGroup("SNavSOStates");
		ser.Value("objectEntId", objectEntId);
		ser.Value("sAnimationDoneUserStates", sAnimationDoneUserStates);
		ser.Value("sAnimationDoneObjectStates", sAnimationDoneObjectStates);
		ser.Value("sAnimationFailUserStates", sAnimationFailUserStates);
		ser.Value("sAnimationFailObjectStates", sAnimationFailObjectStates);
		ser.EndGroup();
	}

	EntityId	objectEntId;
	string		sAnimationDoneUserStates;
	string		sAnimationDoneObjectStates;
	string		sAnimationFailUserStates;
	string		sAnimationFailObjectStates;
};

///////////////////////////////////////////////
// CSmartObjectEvent represents the priority of usage of some Smart Object
///////////////////////////////////////////////
struct CSmartObjectEvent
{
	CSmartObject*	m_pObject;
	CCondition*		m_pCondition;

	// < 0 means this event should be removed
	// 0 means just started
	// 1 means ready to be used
	float			m_Delay;
};


class CSmartObjectClass;
typedef std::vector< CSmartObjectClass* > CSmartObjectClasses;


///////////////////////////////////////////////
// CSmartObjectBase will be the base for CSmartObject class
///////////////////////////////////////////////
class CSmartObjectBase
{
protected:
	IEntity*	m_pEntity;

public:
	CSmartObjectBase( IEntity* pEntity ) : m_pEntity( pEntity ) {}	

	IEntity* GetEntity() const { return m_pEntity; }

	Vec3 GetPos() const;
	Vec3 GetHelperPos( const SmartObjectHelper* pHelper ) const;
	Vec3 GetOrientation( const SmartObjectHelper* pHelper ) const;
	const char* GetName() const { return m_pEntity->GetName(); }
	IPhysicalEntity* GetPhysics() const { return m_pEntity->GetPhysics(); }
	CAIObject* GetAI() const { return (CAIObject*) m_pEntity->GetAI(); }
	CPuppet* GetPuppet() const;
};


enum ESO_Validate
{
	eSOV_Unknown,
	eSOV_InvalidStatic,
	eSOV_InvalidDynamic,
	eSOV_Valid,
};


///////////////////////////////////////////////
// Class Template
///////////////////////////////////////////////
class CClassTemplateData
{
	IStatObj* m_pStatObj;

public:
	CClassTemplateData() : m_pStatObj(0) {}
	~CClassTemplateData() { if ( m_pStatObj ) m_pStatObj->Release(); }

	string name;

	struct CTemplateHelper
	{
		string name;
		QuatT qt;
		float radius;
		bool project;
	};
	typedef std::vector< CTemplateHelper > TTemplateHelpers;

	string model;
	TTemplateHelpers helpers;

	const CTemplateHelper* GetHelper( const char* helperName ) const
	{
		TTemplateHelpers::const_iterator it, itEnd = helpers.end();
		for ( it = helpers.begin(); it != itEnd; ++it )
			if ( it->name == helperName )
				return &*it;
		return NULL;
	}

	IStatObj* GetIStateObj()
	{
		if ( m_pStatObj )
			return m_pStatObj;

		if ( !model.empty() )
		{
			m_pStatObj = gEnv->p3DEngine->LoadStatObj( "Editor/Objects/" + model );
			if ( m_pStatObj )
			{
				m_pStatObj->AddRef();
				if ( GetHelperMaterial() )
					m_pStatObj->SetMaterial( GetHelperMaterial() );
			}
		}

		return m_pStatObj;
	}

	static IMaterial* m_pHelperMtl;
	static IMaterial* GetHelperMaterial()
	{
		if ( !m_pHelperMtl )
			m_pHelperMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial( "Editor/Objects/Helper" );
		return m_pHelperMtl;
	};
};
// MapClassTemplateData contains all class templates indexed by name
typedef std::map< string, CClassTemplateData > MapClassTemplateData;


///////////////////////////////////////////////
// CSmartObject contains a pointer to the entity used as smart object and its additional info
///////////////////////////////////////////////
class CSmartObject : public CSmartObjectBase
{
public:
	///////////////////////////////////////////////
	// States internally are represented with IDs
	///////////////////////////////////////////////
	class CState
	{
		typedef std::map< string, int > MapSmartObjectStateIds;
		typedef std::vector< const char* > MapSmartObjectStates;

		static MapSmartObjectStateIds	g_mapStateIds;
		static MapSmartObjectStates		g_mapStates;

	protected:
		int	m_StateId;

	public:
		//CState( const CState& other ) : m_StateId( other.m_StateId ) {}
		//CState( int stateId ) : m_StateId( stateId ) {}
		CState( const char* state )
		{
			if ( *state )
			{
				std::pair< MapSmartObjectStateIds::iterator, bool > pr = g_mapStateIds.insert(std::make_pair( state, g_mapStates.size() ));
				if ( pr.second ) // was insertion made?
					g_mapStates.push_back( pr.first->first.c_str() );
				m_StateId = pr.first->second;
			}
			else
			{
				m_StateId = -1;
			}
		}

		bool operator < ( const CState& other ) const { return m_StateId < other.m_StateId; }
		bool operator == ( const CState& other ) const { return m_StateId == other.m_StateId; }
		bool operator != ( const CState& other ) const { return m_StateId != other.m_StateId; }

		int asInt() const { return m_StateId; }
		const char* c_str() const { return m_StateId >= 0 ? g_mapStates[ m_StateId ] : ""; }

		static int GetNumStates() { return g_mapStates.size(); }
		static const char* GetStateName( int i ) { return i >= 0 && i < g_mapStates.size() ? g_mapStates[i] : NULL; }
	};

	typedef std::set< CState > SetStates;

	typedef std::vector< CState > VectorStates;

	class DoubleVectorStates
	{
	public:
		VectorStates positive;
		VectorStates negative;

		string AsString() const
		{
			string temp;
			for ( int i = 0; i < positive.size(); ++i )
			{
				if ( i ) temp += ',';
				temp += positive[i].c_str();
			}
			if ( !negative.empty() )
			{
				temp += '-';
				for ( int i = 0; i < negative.size(); ++i )
				{
					if ( i ) temp += ',';
					temp += negative[i].c_str();
				}
			}
			return temp;
		}

		string AsUndoString() const
		{
			string temp;
			for ( int i = 0; i < negative.size(); ++i )
			{
				if ( i ) temp += ',';
				temp += negative[i].c_str();
			}
			if ( !positive.empty() )
			{
				temp += '-';
				for ( int i = 0; i < positive.size(); ++i )
				{
					if ( i ) temp += ',';
					temp += positive[i].c_str();
				}
			}
			return temp;
		}

		bool empty() const { return positive.empty() && negative.empty(); }

		bool operator == ( const DoubleVectorStates& other ) const
		{
			return positive == other.positive && negative == other.negative;
		}
		bool operator != ( const DoubleVectorStates& other ) const
		{
			return positive != other.positive || negative != other.negative;
		}

		bool Matches( const SetStates& states ) const
		{
			SetStates::const_iterator statesEnd = states.end();
			VectorStates::const_iterator it, itEnd = positive.end();
			for ( it = positive.begin(); it != itEnd; ++it )
				if ( states.find(*it) == statesEnd )
					return false;
			itEnd = negative.end();
			for ( it = negative.begin(); it != itEnd; ++it )
				if ( states.find(*it) != statesEnd )
					return false;
			return true;
		}

		bool ChecksState( CState state ) const
		{
			if ( std::find( positive.begin(), positive.end(), state ) != positive.end() )
				return true;
			if ( std::find( negative.begin(), negative.end(), state ) != negative.end() )
				return true;
			return false;
		}
	};

	class CStatePattern: public std::vector< DoubleVectorStates >
	{
	public:
		bool Matches( const SetStates& states ) const
		{
			const_iterator it, itEnd = end();
			for ( it = begin(); it != itEnd; ++it )
				if ( it->Matches(states) )
					return true;
			return empty();
		}

		string AsString() const
		{
			string temp;
			for ( int i = 0; i < size(); ++i )
			{
				if ( i )
					temp += " | ";
				temp += at(i).AsString();
			}
			return temp;
		}

		bool ChecksState( CState state ) const
		{
			const_iterator it, itEnd = end();
			for ( it = begin(); it != itEnd; ++it )
				if ( it->ChecksState(state) )
					return true;
			return false;
		}
	};

protected:
	friend class CSmartObjectClass;
	friend class CSmartObjects;

	CSmartObjectClasses	m_vClasses;
//	CSmartObjectClass*	m_pClass;

	SetStates m_States;

	float m_fKey;

	typedef std::vector< CSmartObjectEvent > VectorEvents;
	typedef std::map< CSmartObjectClass*, VectorEvents > MapEventsByClass;
	MapEventsByClass	m_Events;
	float				m_fRandom;

	typedef std::map< CSmartObjectClass*, CTimeValue > MapTimesByClass;
	MapTimesByClass		m_mapLastUpdateTimes;

	// LookAt helpers
	float	m_fLookAtLimit;
	Vec3	m_vLookAtPos;

	typedef std::map< const SmartObjectHelper*, unsigned > MapNavNodeIds;
	MapNavNodeIds	m_mapNavNodes;

	ESO_Validate m_eValidationResult;

	bool m_bHidden;

	typedef std::vector< unsigned > VectorNavNodes;
	VectorNavNodes m_vNavNodes;

	// returns the size of the AI puppet (or Vec3(ZERO) if it isn't a puppet)
	// x - radius, y - height, z - offset from the ground
	Vec3 MeasureUserSize() const;

public:
	struct CTemplateHelperInstance
	{
		Vec3 position;
		ESO_Validate validationResult;
	};
	typedef std::vector< CTemplateHelperInstance > TTemplateHelperInstances;

protected:
	struct CTemplateData
	{
		CSmartObjectClass* pClass;
		float userRadius;
		float userTop;
		float userBottom;
		bool staticTest;
		TTemplateHelperInstances helperInstances;
	};
	typedef std::map< CClassTemplateData*, CTemplateData > MapTemplates;
	MapTemplates* m_pMapTemplates;

public:
	CSmartObject( IEntity* pEntity );
	~CSmartObject();

	void Hide( bool hide ) { m_bHidden = hide; }
	bool IsHidden() const { return m_bHidden; }

	void Serialize( TSerialize ser );
	void SerializePointer( TSerialize ser, const char* name, CSmartObject* & pSmartObject );

	bool operator < ( const CSmartObject& other ) const { return m_pEntity < other.m_pEntity; }
	bool operator == ( const CSmartObject& other ) const { return m_pEntity == other.m_pEntity; }

	CSmartObjectClasses& GetClasses() { return m_vClasses; }
	const SetStates& GetStates() const { return m_States; }

	void Use( CSmartObject* pObject, CCondition* pCondition, int eventId = 0, bool bForceHighPriority = false ) const;

	/// Calculates the navigation nodes (up to 2) that we're, or else a particular helper is, attached to.
	/// If already attached it does nothing - the result is cached
	unsigned GetNavNodes( unsigned nodeIndices[2], const SmartObjectHelper* pHelper );

	MapTemplates& GetMapTemplates();

	ESO_Validate GetValidationResult() const { return m_eValidationResult; }
};


typedef std::pair< CSmartObjectClass*, CSmartObject::CState > PairClassState;


///////////////////////////////////////////////
// CSmartObjectClass maps entities to smart objects
///////////////////////////////////////////////
class CSmartObjectClass
{
protected:
	typedef std::vector< CSmartObject* > VectorSmartObjects;
	VectorSmartObjects m_allSmartObjectInstances;
	unsigned int m_indexNextObject;

	const string	m_sName;

	bool	m_bSmartObjectUser;
	bool	m_bNeeded;

public:

	// classes now keep track of all their instances
	typedef std::multimap< float, CSmartObject*, std::less<float>, stl::STLPoolAllocator<std::pair<float, CSmartObject*>, stl::PoolAllocatorSynchronizationSinglethreaded> > MapSmartObjectsByPos;
	MapSmartObjectsByPos m_MapObjectsByPos; // map of all smart objects indexed by their position


	typedef std::vector< SmartObjectHelper* > VectorHelpers;
	VectorHelpers m_vHelpers;

	typedef std::map< string, CSmartObjectClass* > MapClassesByName;
	static MapClassesByName g_AllByName;

	static const char* GetClassName( int id );

	typedef std::vector< CSmartObjectClass* > VectorClasses;
	static VectorClasses	g_AllUserClasses;
	static VectorClasses::iterator	g_itAllUserClasses;

	CSmartObjectClass( const char* className );
	~CSmartObjectClass();

	static CSmartObjectClass* find( const char* sClassName );

	bool operator < ( const CSmartObjectClass& other ) const { return m_sName < other.m_sName; }
	bool operator == ( const CSmartObjectClass& other ) const { return m_sName == other.m_sName; }

	const string GetName() const { return m_sName; }

	void RegisterSmartObject( CSmartObject* pSmartObject );
	void UnregisterSmartObject( CSmartObject* pSmartObject );
	void DeleteSmartObject( IEntity* pEntity );

	void MarkAsSmartObjectUser()
	{
		m_bSmartObjectUser = true;
		if ( m_bNeeded )
		{
			if ( std::find( g_AllUserClasses.begin(), g_AllUserClasses.end(), this ) == g_AllUserClasses.end() )
				g_AllUserClasses.push_back( this );
			g_itAllUserClasses = g_AllUserClasses.end();
		}
	}
	bool IsSmartObjectUser() const { return m_bSmartObjectUser; }

	void MarkAsNeeded()
	{
		m_bNeeded = true;
		if ( m_bSmartObjectUser )
		{
			if ( std::find( g_AllUserClasses.begin(), g_AllUserClasses.end(), this ) == g_AllUserClasses.end() )
				g_AllUserClasses.push_back( this );
			g_itAllUserClasses = g_AllUserClasses.end();
		}
	}
	bool IsNeeded() const { return m_bNeeded; }

	void FirstObject() { m_indexNextObject = 0; }
	CSmartObject* NextObject()
	{
		if ( m_indexNextObject >= m_allSmartObjectInstances.size() )
			return NULL;
		return m_allSmartObjectInstances[ m_indexNextObject++ ];
	}
	CSmartObject* NextVisibleObject()
	{
		unsigned int size = m_allSmartObjectInstances.size();
		while ( m_indexNextObject < size && m_allSmartObjectInstances[ m_indexNextObject ]->IsHidden() )
			++m_indexNextObject;
		if ( m_indexNextObject >= size )
			return NULL;
		return m_allSmartObjectInstances[ m_indexNextObject++ ];
	}

	void AddHelper( SmartObjectHelper* pHelper ) { m_vHelpers.push_back( pHelper ); }
	SmartObjectHelper* GetHelper( const char* name ) const
	{
		VectorHelpers::const_iterator it, itEnd = m_vHelpers.end();
		for ( it = m_vHelpers.begin(); it != itEnd; ++it )
			if ( (*it)->name == name )
				return *it;
		return NULL;
	}
	const char* GetHelperName( int id ) const
	{
		if ( id >= 0 && id < m_vHelpers.size() )
			return m_vHelpers[id]->name;
		else
			return NULL;
	}
	bool DeleteHelper( SmartObjectHelper* pHelper )
	{
		VectorHelpers::iterator it, itEnd = m_vHelpers.end();
		for ( it = m_vHelpers.begin(); it != itEnd; ++it )
			if ( *it == pHelper )
			{
				m_vHelpers.erase(it);
				return true;
			}
		return false;
	}

	struct HelperLink
	{
		float				passRadius;
		SmartObjectHelper*	from;
		SmartObjectHelper*	to;
		CCondition*			condition;
	};
	typedef std::vector< HelperLink > THelperLinks;
	THelperLinks m_vHelperLinks;

	typedef std::set< SmartObjectHelper* > SetHelpers;
	SetHelpers m_setNavHelpers;

	void ClearHelperLinks();
	bool AddHelperLink( CCondition* condition );
	int FindHelperLinks( const SmartObjectHelper* from, const SmartObjectHelper* to, const CSmartObjectClass* pClass, float radius,
		CSmartObjectClass::HelperLink** pvHelperLinks, int iCount = 1 );
	int FindHelperLinks( const SmartObjectHelper* from, const SmartObjectHelper* to, const CSmartObjectClass* pClass, float radius,
		const CSmartObject::SetStates& userStates, const CSmartObject::SetStates& objectStates,
		CSmartObjectClass::HelperLink** pvHelperLinks, int iCount = 1 );

	struct UserSize
	{
		float radius;
		float bottom;
		float top;

		UserSize() : radius(0), top(0), bottom(FLT_MAX) {}
		UserSize( float r, float t, float b ) : radius(r), top(t), bottom(b) {}
		operator bool () const { return radius > 0 && top > bottom; }
		const UserSize& operator += ( const UserSize& other )
		{
			if ( *this )
			{
				if ( other )
				{
					if ( other.radius > radius )
						radius = other.radius;
					if ( other.top > top )
						top = other.top;
					if ( other.bottom < bottom )
						bottom = other.bottom;
				}
			}
			else
				*this = other;
			return *this;
		}
	};
	UserSize m_StanceMaxSize;
	UserSize m_NavUsersMaxSize;
	void AddNavUserClass( CSmartObjectClass* pUserClass )
	{
		m_NavUsersMaxSize += pUserClass->m_StanceMaxSize;
	}

	CClassTemplateData* m_pTemplateData;

	// optimization: each user class keeps a list of active update rules
	typedef std::vector< CCondition* > VectorRules;
	VectorRules m_vActiveUpdateRules[3]; // three vectors - one for each alertness level (0 includes all in 1 and 2; 1 includes all in 2)
};


///////////////////////////////////////////////
// CCondition defines conditions under which a smart object should be used.
// It defines everything except conditions about the user, because user's condition will be the key of a map later...
///////////////////////////////////////////////
struct CCondition
{
	int									iTemplateId;

	CSmartObjectClass*					pUserClass;
	CSmartObject::CStatePattern			userStatePattern;

	CSmartObjectClass*					pObjectClass;
	CSmartObject::CStatePattern			objectStatePattern;

	string								sUserHelper;
	SmartObjectHelper*					pUserHelper;
	string								sObjectHelper;
	SmartObjectHelper*					pObjectHelper;

	float								fDistanceFrom;
	float								fDistanceTo;
	float								fOrientationLimit;
	float								fOrientationToTargetLimit;

	float								fMinDelay;
	float								fMaxDelay;
	float								fMemory;

	float								fProximityFactor;
	float								fOrientationFactor;
	float								fVisibilityFactor;
	float								fRandomnessFactor;

	float								fLookAtPerc;
	CSmartObject::DoubleVectorStates	userPreActionStates;
	CSmartObject::DoubleVectorStates	objectPreActionStates;
	EActionType							eActionType;
//	bool								bHighPriority;
	string								sAction;
	bool								bEarlyPathRegeneration;
//	IAIAction*							pAction;
	CSmartObject::DoubleVectorStates	userPostActionStates;
	CSmartObject::DoubleVectorStates	objectPostActionStates;

	int									iMaxAlertness;
	bool								bEnabled;

	int									iRuleType; // 0 - normal rule; 1 - navigational rule;
	string								sEvent;
	string								sChainedUserEvent;
	string								sChainedObjectEvent;
	string								sEntranceHelper;
	string								sExitHelper;

	string								sName;
	string								sDescription;
	string								sFolder;
	int									iOrder;

	// exact positioning related
	float								fApproachSpeed;
	int									iApproachStance;
	string								sAnimationHelper;
	string								sApproachHelper;
	float								fStartRadiusTolerance;
	float								fDirectionTolerance;
	float								fTargetRadiusTolerance;

	bool operator == ( const CCondition& other ) const;
};

typedef std::multimap< CSmartObjectClass*, CCondition > MapConditions;
typedef std::multimap< CSmartObjectClass*, CCondition* > MapPtrConditions;


///////////////////////////////////////////////
// Smart Object Event
///////////////////////////////////////////////
class CEvent
{
public:
	const string		m_sName;		// event name
	string				m_sDescription;	// description of this event
	MapPtrConditions	m_Conditions;	// map of all conditions indexed by user's class

	//CEvent( const CEvent& other ) : m_sName( other.name ) {}
	CEvent( const string& name ) : m_sName( name ) {}
};


///////////////////////////////////////////////
// CQueryEvent
///////////////////////////////////////////////

class CQueryEvent
{
public:
	CSmartObject*	pUser;
	CSmartObject*	pObject;
	CCondition*		pRule;
	CQueryEvent*	pChainedUserEvent;
	CQueryEvent*	pChainedObjectEvent;
	
	CQueryEvent():pUser(0),pObject(0),pRule(0),pChainedUserEvent(0),pChainedObjectEvent(0)
	{}

	CQueryEvent( const CQueryEvent& other )
	{
		*this = other;
	}

	~CQueryEvent()
	{
		if ( pChainedUserEvent )
			delete pChainedUserEvent;
		if ( pChainedObjectEvent )
			delete pChainedObjectEvent;
	}

	void Clear()
	{
		pUser = NULL;
		pObject = NULL;
		pRule = NULL;
		if ( pChainedUserEvent )
			delete pChainedUserEvent;
		if ( pChainedObjectEvent )
			delete pChainedObjectEvent;
	}

	const CQueryEvent& operator = ( const CQueryEvent& other )
	{
		pUser = other.pUser;
		pObject = other.pObject;
		pRule = other.pRule;
		pChainedUserEvent = other.pChainedUserEvent ? new CQueryEvent( *other.pChainedUserEvent ) : NULL;
		pChainedObjectEvent = other.pChainedObjectEvent ? new CQueryEvent( *other.pChainedObjectEvent ) : NULL;
		return *this;
	}

	void Serialize(TSerialize ser);

};

// while querying matches the events will be stored in a container of this type sorted by priority
typedef std::multimap< float, CQueryEvent > QueryEventMap;


///////////////////////////////////////////////
// CSmartObjects receives notifications from entity system about entities being spawned and deleted.
// Keeps track of registered classes, and automatically creates smart object representation for every instance belonging to them.
///////////////////////////////////////////////
class CSmartObjects
	: public IEntitySystemSink
{
private:
	CSmartObject::CState m_StateAttTarget;
	CSmartObject::CState m_StateSameGroup;
	CSmartObject::CState m_StateSameSpecies;
	CSmartObject::CState m_StateBusy;

	MapConditions			m_Conditions;		// map of all conditions indexed by user's class

	// used for debug-drawing used smart objects
	struct CDebugUse
	{
		CTimeValue m_Time;
		CSmartObject* m_pUser;
		CSmartObject* m_pObject;
	};
	typedef std::vector< CDebugUse > VectorDebugUse;
	VectorDebugUse	m_vDebugUse;

	// Implementation of IEntitySystemSink methods
	///////////////////////////////////////////////
	virtual bool OnBeforeSpawn( SEntitySpawnParams& params ) { return true; };
	virtual void OnSpawn( IEntity* pEntity, SEntitySpawnParams& params );
	virtual bool OnRemove( IEntity* pEntity );
	virtual void OnEvent( IEntity* pEntity, SEntityEvent& event );

	void DoRemove( IEntity* pEntity );

	void AddSmartObjectClass( CSmartObjectClass* soClass );

	bool m_bRecalculateUserSize;
	void RecalculateUserSize();

/*
	struct CUpdateStruct
	{
		int m_Count;
		MapConditions::iterator m_itCondition;
		MapSmartObjectsByClass::iterator m_itByClass;
		MapSmartObjectsByState::iterator m_itByState;
		MapSmartObjectsByState::iterator m_itObject;
	};
	CUpdateStruct m_UpdateStruct;
*/

public:
	typedef std::map< IEntity*, CSmartObject* > MapSmartObjectsByEntity;
	static MapSmartObjectsByEntity g_AllSmartObjects;

	MapSOHelpers m_mapHelpers;

	typedef std::map< string, CEvent > MapEvents;
	MapEvents m_mapEvents;	// map of all smart object events indexed by name

	MapClassTemplateData m_mapClassTemplates;
	void LoadSOClassTemplates();

	static CSmartObject* IsSmartObject( IEntity* pEntity );

	void RescanSOClasses( IEntity* pEntity );

	void String2Classes( const string& sClass, CSmartObjectClasses& vClasses );
	static void String2States( const char* listStates, CSmartObject::DoubleVectorStates& states );
	static void String2StatePattern( const char* sPattern, CSmartObject::CStatePattern& pattern );

	CSmartObjects();
	~CSmartObjects();

	void Serialize( TSerialize ser );
	static void SerializePointer( TSerialize ser, const char* name, CSmartObject* & pSmartObject );
	static void SerializePointer( TSerialize ser, const char* name, CCondition* & pRule );

	CSmartObjectClass* GetSmartObjectClass( const char* className );

	void AddSmartObjectCondition( const SmartObjectCondition& conditionData );
	void SetSmartObjectConditions( SmartObjectConditions& conditions );

	bool ParseClassesFromProperties( const IEntity* pEntity, CSmartObjectClasses& vClasses );

	void ClearConditions();
	void Reset();
	void SoftReset();

	float CalculateDelayTime( CSmartObject* pUser, const Vec3& posUser,
		CSmartObject* pObject, const Vec3& posObject, CCondition* pCondition ) const;
	void Update();
	int Process( CSmartObject* pSmartObjectUser, CSmartObjectClass* pClass );
	void UseSmartObject( CSmartObject* pSmartObjectUser, CSmartObject* pObject, CCondition* pCondition, int eventId = 0, bool bForceHighPriority = false );
//	int UseNavigationSmartObject( CSmartObject* pSmartObjectUser, CSmartObject* pSmartObject, const CCondition* pCondition );
	bool PrepareUseNavigationSmartObject( SAIActorTargetRequest& pReq, SNavSOStates& states, CSmartObject* pSmartObjectUser, CSmartObject* pSmartObject, const CCondition* pCondition, SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper );

	void SetSmartObjectState( IEntity* pEntity, CSmartObject::CState state ) const;
	void SetSmartObjectState( CSmartObject* pSmartObject, CSmartObject::CState state ) const;
	void AddSmartObjectState( IEntity* pEntity, CSmartObject::CState state ) const;
	void AddSmartObjectState( CSmartObject* pSmartObject, CSmartObject::CState state ) const;
	void RemoveSmartObjectState( IEntity* pEntity, CSmartObject::CState state ) const;
	void RemoveSmartObjectState( CSmartObject* pSmartObject, CSmartObject::CState state ) const;
	void ModifySmartObjectStates( IEntity* pEntity, const CSmartObject::DoubleVectorStates& states );
	void ModifySmartObjectStates( CSmartObject* pSmartObject, const CSmartObject::DoubleVectorStates& states ) const;
	bool CheckSmartObjectStates( IEntity* pEntity, const CSmartObject::CStatePattern& pattern ) const;

	void DebugDraw( IRenderer * pRenderer );

	void RebuildNavigation();
	void RegisterInNavigation( CSmartObject* pSmartObject, CSmartObjectClass* pClass ) const;
	void UnregisterFromNavigation( CSmartObject* pSmartObject ) const;

	void RemoveEntity( IEntity* pEntity );

	CEvent* String2Event( const string& name )
	{
		MapEvents::iterator it = m_mapEvents.insert(MapEvents::value_type( name, name )).first;
		return & it->second;
	}
	//int GetNumEvents() { return m_mapEvents.size(); }
	const char* GetEventName( int index )
	{
		if ( index < 0 || index >= m_mapEvents.size() )
			return NULL;
		MapEvents::const_iterator it = m_mapEvents.begin();
		std::advance( it, index );
		return it->second.m_sName;
	}

	// returns the id of the inserted goal pipe if a rule was found or 0 if no rule
	int TriggerEvent( const char* sEventName, IEntity*& pUser, IEntity*& pObject, QueryEventMap* pQueryEvents = NULL, const Vec3* pExtraPoint = NULL, bool bHighPriority = false );

	// SOClass template related
	void GetTemplateIStatObj( IEntity* pEntity, std::vector< IStatObj* >& statObjects );
	bool ValidateTemplate( IEntity* pEntity, bool bStaticOnly, IEntity* pUserEntity = NULL, int fromTemplateHelperIdx = -1, int toTemplateHelperIdx = -1 );
	bool ValidateTemplate( CSmartObject* pSmartObject, bool bStaticOnly, IEntity* pUserEntity = NULL, int fromTemplateHelperIdx = -1, int toTemplateHelperIdx = -1 );
	void DrawTemplate( IEntity* pEntity );
	void DrawTemplate( CSmartObject* pSmartObject, bool bStaticOnly );

	void OnNewPuppetCreated( CAIObject* pObject );

private:
	IEntity* m_pPreOnSpawnEntity; // the entity for which OnSpawn was called just right before the current OnSpawn call

	int TriggerEventUserObject( const char* sEventName, CSmartObject* pUser, CSmartObject* pObject, QueryEventMap* pQueryEvents, const Vec3* pExtraPoint, bool bHighPriority );
	int TriggerEventUser( const char* sEventName, CSmartObject* pUser, QueryEventMap* pQueryEvents, IEntity** ppObjectEntity, const Vec3* pExtraPoint, bool bHighPriority );
	int TriggerEventObject( const char* sEventName, CSmartObject* pObject, QueryEventMap* pQueryEvents, IEntity** ppUserEntity, const Vec3* pExtraPoint, bool bHighPriority );
};


#endif
