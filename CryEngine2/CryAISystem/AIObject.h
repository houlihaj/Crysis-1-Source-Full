// AIObject.h: interface for the CAIObject class.
//
//////////////////////////////////////////////////////////////////////

#ifndef AIOBJECT_H
#define AIOBJECT_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IAgent.h"
#include "AStarSolver.h"
#include "IPhysics.h"

struct GraphNode;
class CAISystem;
class CFormation;
class COPWaitSignal;


/*! Basic ai object class. Defines a framework that all puppets and points of interest
later follow.
*/
class CAIObject : public IAIObject
{
public:
	CAIObject(CAISystem*);
	virtual ~CAIObject();

	//===================================================================
	// inherited virtual interface functions
	//===================================================================
	virtual void Reset(EObjectResetType type);
	virtual void	RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData=NULL) {};
	virtual void	RecordSnapshot() {};
	virtual IAIDebugRecord* GetAIDebugRecord() {return NULL;};
	virtual bool IsEnabled() const {return m_bEnabled;}
	virtual const char * GetName() const {return m_sName.c_str();}
	virtual void SetName( const char *pName);
	virtual IAIObject * GetAssociation() const  {return m_pAssociation;}
	virtual void SetType(unsigned short type);  
	virtual unsigned short GetAIType() const {return m_nObjectType;} // used by the interface
	virtual ESubTypes GetSubType() const {return m_objectSubType;}
	virtual const Vec3 &GetPos( ) const {m_bTouched = true; return m_vPosition;} ///< Returns the eye position
	virtual const Vec3 &GetFirePos( ) const {return m_vFirePosition;} ///< Returns the position weapon position (from where it will fire)
	virtual void SetPos(const Vec3 &pos,const Vec3 &dirForw=Vec3(1,0,0));
	virtual const Vec3 &GetBodyDir() const { return m_vBodyDir; }
	virtual void SetBodyDir(const Vec3 &dir);
	virtual const Vec3 &GetMoveDir() const { return m_vMoveDir; }
	virtual void SetMoveDir(const Vec3 &dir);
	virtual const Vec3 &GetViewDir() const { return m_vView; }
	virtual void SetViewDir(const Vec3 &dir);
	virtual void Release() {delete this;}
	virtual Vec3 GetVelocity() const;
	virtual void Event(unsigned short eType, SAIEVENT *pEvent);
	virtual void OnObjectRemoved(CAIObject *pObject );
	virtual IPhysicalEntity* GetPhysics(bool wantCharacterPhysics=false) const {return NULL;}
	virtual void SetPFBlockerRadius(int blockerType, float radius) {};
	virtual void EDITOR_DrawRanges(bool bEnable);
	virtual void SetRadius(float fRadius);
	virtual float GetRadius() const {return m_fRadius;}
	virtual bool IsMoving() const { return m_bMoving;}
	virtual bool IsHostile(const IAIObject* pOther, bool bUsingAIIgnorePlayer=true) const;
	virtual void Serialize( TSerialize ser, class CObjectTracker& objectTracker );
	virtual void SetEntityID(unsigned ID);
	virtual unsigned GetEntityID() const {return m_entityID;}
	virtual IEntity* GetEntity() const;
	virtual bool CreateFormation(const char * szName, Vec3 vTargetPos = ZERO);
  /// useful implementation in CPipeUser
  virtual void SetPathToFollow(const char *pathName) {}
  virtual void SetPathAttributeToFollow(bool bSpline) {}
  virtual void SetPointListToFollow( const std::list<Vec3>& pointList,IAISystem::ENavigationType navType,bool bSpline ){}
  virtual ETriState CanTargetPointBeReached(CTargetPointRequest &request) {request.SetResult(eTS_false); return eTS_false;}
  virtual bool UseTargetPointRequest(const CTargetPointRequest &request) {return false;}
  /// useful implementation in CPuppet
  virtual bool GetValidPositionNearby(const Vec3 &proposedPosition, Vec3 &adjustedPosition) const {return false;}
  /// useful implementation in CPuppet
  virtual bool GetTeleportPosition(Vec3 &teleportPos) const {return false;}
	virtual void Following(IAIObject* pFollowed); 
	virtual IAIObject* GetFollowed() {return m_pFollowed;}
	virtual IAIObject* GetFollowedLeader();
	virtual IAIObject* GetSpecialAIObject(const char* objName, float range = 0.0f) {return NULL;};
	virtual bool IsUpdatedOnce() const { return m_bUpdatedOnce; }
	virtual IUnknownProxy* GetProxy() const { return NULL; };
	/// last finished AI action sets uses this method to set its status (succeed or failed)
	virtual void SetLastActionStatus( bool bSucceed ) { m_bLastActionSucceed = bSucceed; }
	virtual void SetWeaponDescriptor(const AIWeaponDescriptor& descriptor) {}
	virtual bool	IsAgent() const { return false; }
	/// Returns true if tho given point is in puppet's field/range of view
	virtual bool IsPointInFOVCone(const Vec3 &pos, float distanceScale = 1.0f) {return false;}
	/// sets a randomly rotating range for the AIObject's formation sight directions
	virtual void SetFormationUpdateSight(float range,float minTime,float maxTime);

	inline virtual bool HasFormation() {return m_pFormation!=NULL;}

	virtual void SetAssociation(CAIObject* pAssociation);

	virtual void SetGroupId(int id);
	virtual int GetGroupId() const;

	virtual const Vec3 &GetShootAtPos( ) const {return m_vPosition;} ///< Returns the position to shoot (bbox middle for tank, etc)

	//===================================================================
	// non-virtual functions
	//===================================================================

	const Vec3& GetLastPosition() {return m_vLastPosition;}
	Vec3 GetPhysicsPos() const; ///<returns the physics/body position
	void SetSubType(ESubTypes type);
	const char* GetEventName(unsigned short eType) const;
	bool ReleaseFormation();

	/// if this is an anchor this calculates the navigation node that we're attached to
	/// if already attached it does nothing.
	const GraphNode *GetAnchorNavNode();
	inline	unsigned short GetType() const {return m_nObjectType;} // used internally to inline the function calls

	bool						m_bUncheckedBody;
	bool						m_bCanReceiveSignals;
	unsigned				m_lastNavNodeIndex;
	unsigned				m_lastHideNodeIndex;
  bool            m_bLastNearForbiddenEdge;
	bool						m_bMoving;
	float						m_DEBUGFLOAT;
	bool						m_bDEBUGDRAWBALLS;
	Vec3						m_vLastPosition;
	mutable bool		m_bTouched;

	CFormation			*m_pFormation;

	IAIObject*			m_pFollowed;

	bool m_bUpdatedOnce;
	bool m_bLastActionSucceed;

protected:

	unsigned short	m_nObjectType;		//	type would be Dummy
	ESubTypes				m_objectSubType;	//	subType would be soundDummy, formationPoint, etc	
	bool						m_bEnabled;

private:

	Vec3						m_vPosition;

	Vec3						m_vFirePosition;
	Vec3						m_vPredictedPosition; // used for store the predicted position of AI object

	Vec3						m_vBodyDir;		// body forward direction of the entity
	Vec3						m_vMoveDir;		// last move direction of the entity
	Vec3						m_vView;			// view direction (where my head is looking at, tank turret turned, etc)

protected:
	//	Vec3						m_vOrientation;
	CAIObject*			m_pAssociation;
	float						m_fRadius;

	string			m_sName;
	unsigned		m_entityID; // will be 0 if not owned by an entity

	int m_groupId;
};

#endif 
