#ifndef _AIGROUPTACTIC_H_
#define _AIGROUPTACTIC_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include "IAISystem.h"
#include "CAISystem.h"
#include "IRenderer.h"
#include "IRenderAuxGeom.h"	
#include "IAgent.h"
#include "IAIGroup.h"
#include "StlUtils.h"

class CAIGroup;

//====================================================================
// CHumanGroupTactic
// Tactical situation evaluation and attack spot finder for humans.
//====================================================================
class CHumanGroupTactic : public IAIGroupTactic, public IAIPathFinderListerner
{
public:
	CHumanGroupTactic(CAIGroup* pGroup);
	virtual ~CHumanGroupTactic();
	virtual void	Update(float dt);
	virtual void	OnObjectRemoved(IAIObject* pObject);
	virtual void	OnUnitAttentionTargetChanged();
	virtual void	OnMemberAdded(IAIObject* pMember);
	virtual void	OnMemberRemoved(IAIObject* pMember);
	virtual void	Reset();
	virtual void	NotifyState(IAIObject* pRequester, int type, float param);
	virtual int		GetState(IAIObject* pRequester, int type, float param);
	virtual Vec3	GetPoint(IAIObject* pRequester, int type, float param);
	virtual void	Serialize(TSerialize ser, CObjectTracker& objectTracker);
	virtual void	DebugDraw(IRenderer* pRend);

	virtual void OnPathResult(int id, const std::vector<unsigned>* pathNodes);

	void	Validate();
	void	Dump();

private:

	int GetTeamRole(CPuppet* pPuppet);

	enum ESide
	{
		SIDE_UNDEF,
		SIDE_LEFT,
		SIDE_RIGHT
	};

	// Note: do not forget to change serialization if change this enum!
	enum EUnitAction
	{
		UA_IDLE						= 0x0001,
		UA_ADVANCING			= 0x0002,
		UA_COVERING				= 0x0004,
		UA_WEAK_COVERING	= 0x0008,
		UA_HIDING					= 0x0010,
		UA_SEEKING				= 0x0020,
		UA_UNAVAIL				= 0x0040,
		UA_SEARCHING			= 0x0080,
		UA_REINFORCING		= 0x0100,
		UA_ALERTED				= 0x0200,
		UA_LAST,
	};

	struct SUnit
	{
		SUnit() { Reset(); }
		SUnit(CPuppet* obj_, int type_, bool leader_ = false)
		{
			Reset();
			obj = obj_;
			type = type_;
			leader = leader_;
		}

		void	Reset()
		{
			order = 0;
			curAction = UA_IDLE;
			curActionTime = 0.0f;
			leader = false;
			enabled = true;
			nearestUnit = -1;
			nearestUnitDist = 0;
			mostLost = false;
			doCohesion = false;
			team = -1;
			readability = false;
			nearestSeek = false;
			targetLookTime = 0.0f;
			underFireTime = 0.0f;
			advancePos.Set(0,0,0);
		}

		void Serialize(TSerialize ser, CObjectTracker& objectTracker);

		CPuppet*		obj;
		int					type;
//		bool				side;
		EUnitAction	curAction;
		float				curActionTime;
		bool				leader;
		bool				enabled;
		int					order;
		int					nearestUnit;
		float				nearestUnitDist;
		bool				mostLost;
		bool				doCohesion;
		int					team;
		bool				readability;
		bool				nearestSeek;
		float				targetLookTime;
		float				underFireTime;
		Vec3				advancePos;
	};
	typedef std::vector<SUnit>	UnitList;

	struct SAvoidPoint
	{
		SAvoidPoint() {}
		SAvoidPoint(const Vec3& p, float r, float t) : pos(p), rad(r), time(t) {}
		void Serialize(TSerialize ser);
		Vec3	pos;
		float	rad;
		float	time;
	};
	typedef std::vector<SAvoidPoint>	AvoidPointList;


	enum ECoverCheckStatus
	{
		CHECK_UNSET,
		CHECK_INVALID,
		CHECK_OK,
	};

	struct SCoverPos
	{
		SCoverPos(const Vec3& pos, IAISystem::ENavigationType navType) : pos(pos), navType(navType), status(CHECK_UNSET) {}
		Vec3 pos;
		ECoverCheckStatus status;
		IAISystem::ENavigationType navType;
	};


	struct SGroupState
	{
		SGroupState()
		{
			Reset();
		}

		void	Reset()
		{
			m_avoidPoints.clear();
			m_leaderCount = 0;
			m_enabledUnits = 0;
		}

		void Serialize(TSerialize ser, CObjectTracker& objectTracker);

		UnitList		m_units;
		unsigned		m_leaderCount;
		unsigned		m_enabledUnits;
		AvoidPointList	m_avoidPoints;
	};

	UnitList::iterator	FindUnit(CPuppet* puppet);
	void	CheckAndAddUnit(IAIObject* object, int type);
	void	CheckAndRemoveUnit(IAIObject* object);

	void	EvaluateSituation(float dt);
	const char* GetEvaluationText(EGroupState eval);

	bool HasPointInRange(const Vec3& pos, float range, const std::vector<Vec3>& points);
	void	AddAvoidPoint(const Vec3& pos, float r, float t);
	bool	ShouldAvoidPoint(const Vec3& pos);
	void	UpdateAvoidPoints(float dt);
	IAISystem::ENavigationType	GetEnemyNavType(const SUnit& unit);
	IAISystem::ENavigationType	GetUnitNavType(const SUnit& unit);
	float	GetUnitAttackRange(const SUnit& unit);
	void	UpdateLeaderTypeMimicry(SUnit& unit);

	void ValidateCover(SCoverPos& cover, float passRadius, SShape* pTerritoryShape);
	void CollectCoverPos(const Vec3& reqPos, int lastNavNode, float searchRange,
		float passRadius, IAISystem::tNavCapMask navCapMask, std::vector<SCoverPos>& covers);

	SGroupState			m_state;
	CAIGroup*				m_pGroup;

	struct SAIGroupTeam
	{
		SAIGroupTeam() :
			pGroupCenter(0)
		{
			char uniqueName[64];
			_snprintf(uniqueName, 64, "GroupCenter%p", this);
			pGroupCenter = GetAISystem()->CreateDummyObject(uniqueName);
			Reset();
		}
		
		~SAIGroupTeam()
		{
			if (pGroupCenter)
			{
				GetAISystem()->RemoveDummyObject(pGroupCenter);
				pGroupCenter = 0;
			}
		}

		void Serialize(TSerialize ser, CObjectTracker& objectTracker);

		void Reset()
		{
			ClearVectorMemory(units);

			enabledUnits = 0;
			availableUnits = 0;
			bounds.Reset();
			hull.clear();
			labelPos.Set(0,0,0);
			unitTypes = 0;
			liveEnemyCount = 0;
			targetLostTime = 0;
			eval = GS_IDLE;
			side = SIDE_UNDEF;
			advancingCount = 0;
			coveringCount = 0;
			targetCount = 0;
			minAlertness = 10;
			maxAlertness = 0;
			needSeekCount = 0;
			needSearchCount = 0;
			searchCount = 0;
			alertedTime = 0;
			reinforceCount = 0;
			avgTeamPos.Set(0,0,0);
			avgTeamDir.Set(1,0,0);
			avgTeamNorm.Set(0,1,0);
			avgEnemyPos.Set(0,0,0);
			initialGroupCenter.Set(0,0,0);
			initialGroupCenterSet = false;
			defendPosition.Set(0,0,0);
			defendPositionSet = false;
			readabilityBlockTime = 0.0f;
			maxType = AITARGET_NONE;
			maxThreat = AITHREAT_NONE;
			attackPathId = 0;
			lastNavNode = 0;
			lastAttackPathTime = 0.0f;
			lastAttackPathFrom.Set(0,0,0);
			lastAttackPathTo.Set(0,0,0);
			attackPathTarget.Set(0,0,0);
			flankFenceP.Set(0,0,0);
			flankFenceQ.Set(0,0,0);
			coversDirty = true;
			ClearVectorMemory(attackPath);
			ClearVectorMemory(attackPathNodes);
			pLeader = 0;
		}

		inline bool UseOcclusion() { return maxAlertness == 2 && targetLostTime < 20.0f; }


		int unitTypes;
		int liveEnemyCount;
		float targetLostTime;
		EGroupState eval;
		float alertedTime;
		int enabledUnits;
		int availableUnits;
		int advancingCount;
		int coveringCount;
		int searchCount;
		int reinforceCount;
		int targetCount;
		int minAlertness;
		int maxAlertness;
		int needSeekCount;
		int needSearchCount;
		float readabilityBlockTime;
		std::vector<int>	units;

		Vec3 avgTeamPos;
		Vec3 avgTeamDir;
		Vec3 avgTeamNorm;
		Vec3 avgEnemyPos;
		Vec3 initialGroupCenter;
		bool initialGroupCenterSet;
		ESide side;
		CTimeValue lastUpdateTime;

		Vec3 defendPosition;
		bool defendPositionSet;

		AABB bounds;
		std::vector<Vec3> hull;
		Vec3 labelPos;

		EAITargetType		maxType;
		EAITargetThreat	maxThreat;

		CAIObject* pGroupCenter;
		CPuppet* pLeader;

		bool coversDirty;
		float lastAttackPathTime;
		int attackPathId;
		unsigned lastNavNode;
		Vec3 lastAttackPathFrom;
		Vec3 lastAttackPathTo;
		Vec3 attackPathTarget;
		Vec3 flankFenceP, flankFenceQ;
		std::vector<Vec3> attackPath;
		std::vector<unsigned> attackPathNodes;
		std::vector<SCoverPos> covers;
	};
	std::vector<SAIGroupTeam*>	m_teams;

	CPuppet* GetLeaderUnit(SAIGroupTeam& team);
	void UpdateAttackDirection(SAIGroupTeam& team, SUnit& unit);

	SAIGroupTeam* GetTeam(SUnit& unit);

	void DeleteTeams();
	void SetupTeams();
	void SetupTeam(int type);
	void UpdateTeam(SAIGroupTeam& team, float dt);

	void UpdateTeamStats(SAIGroupTeam& team, float dt);

};



//====================================================================
// CFollowerGroupTactic
// Simple follower group tactic.
//====================================================================
class CFollowerGroupTactic : public IAIGroupTactic
{
public:
	CFollowerGroupTactic(CAIGroup* pGroup);
	virtual ~CFollowerGroupTactic();
	virtual void	Update(float dt);
	virtual void	OnObjectRemoved(IAIObject* pObject);
	virtual void	OnUnitAttentionTargetChanged();
	virtual void	OnMemberAdded(IAIObject* pMember);
	virtual void	OnMemberRemoved(IAIObject* pMember);
	virtual void	Reset();
	virtual void	NotifyState(IAIObject* pRequester, int type, float param);
	virtual int		GetState(IAIObject* pRequester, int type, float param);
	virtual Vec3	GetPoint(IAIObject* pRequester, int type, float param);
	virtual void	Serialize(TSerialize ser, CObjectTracker& objectTracker);
	virtual void	DebugDraw(IRenderer* pRend);

private:

	enum EMimicMode
	{
		MIMIC_FOLLOW,
		MIMIC_STATIC,
	};

	enum ECheckStatus
	{
		CHECK_UNSET,
		CHECK_INVALID,
		CHECK_OK,
	};

	struct SCoverPos
	{
		SCoverPos(const Vec3& pos, IAISystem::ENavigationType navType) : pos(pos), navType(navType), status(CHECK_UNSET) {}
		Vec3 pos;
		ECheckStatus status;
		IAISystem::ENavigationType navType;
	};

	struct SAvoidPos
	{
		SAvoidPos(const Vec3& pos, float rad) : pos(pos), rad(rad) {}
		Vec3 pos;
		float rad;
	};


	struct SUnit
	{
		SUnit() { Reset(); }
		SUnit(CPuppet* obj_)
		{
			Reset();
			obj = obj_;
		}

		void	Reset()
		{
			follow = false;
			followDist = 0.0f;
			mimicMode = MIMIC_FOLLOW;
			lastTargetCheckPos.Set(0,0,0);
			mimicPos.Set(0,0,0);
			lastTargetVel.Set(0,0,0);
			movementTimeOut = 0.0f;
			lastReadabilityTime = -1.0f;
			pShape = 0;
			shapeName.clear();
			formDist = 0;
			formAngle = 0;
			formAxisX.Set(1,0,0);
			formAxisY.Set(0,1,0);
			firingTime = -1.0f;
			stayBackTime = -1.0f;
			lookatPlayerTime = -1.0f;
			lastCoverCollectPos.Set(0,0,0);
			lastSelectedPos.Set(0,0,0);
			selectCoverTriggerPos.Set(0,0,0);
			selectCoverTriggerRad = -1.0f;
			pCurrentCoverPath = 0;
			targetFiredWeapon = false;
			currentCoverPathName.clear();
			covers.clear();
			coverPaths.clear();
			DEBUG_Center.Set(0,0,0);
			DEBUG_SidePos0.Set(0,0,0);
			DEBUG_SidePos1.Set(0,0,0);
			DEBUG_LeadPos.Set(0,0,0);
		}

		void Serialize(TSerialize ser, CObjectTracker& objectTracker);

		CPuppet* obj;
		bool follow;
		float followDist;

		EMimicMode mimicMode;
		Vec3	lastTargetCheckPos;
		const SShape* pShape;
		string shapeName;
		Vec3 mimicPos;
		Vec3 lastTargetVel;
		float movementTimeOut;
		float formDist;
		float formAngle;
		Vec3 formAxisX, formAxisY;
		float lastReadabilityTime;

		Vec3 DEBUG_Center, DEBUG_SidePos0, DEBUG_SidePos1, DEBUG_LeadPos;

		std::vector<SCoverPos> covers;
		Vec3 lastCoverCollectPos;
		Vec3 lastSelectedPos;
		Vec3 selectCoverTriggerPos;
		float selectCoverTriggerRad;
		std::vector<SAvoidPos> avoidPos;

		std::vector<string> coverPathNames;
		std::vector<const SShape*> coverPaths;

		const SShape* pCurrentCoverPath;
		string currentCoverPathName;

		float lookatPlayerTime;

		float firingTime;
		float stayBackTime;
//		float emptyFireTime;

		bool	targetFiredWeapon;
//		float stanceEvalTime;
	};

	typedef std::vector<SUnit>	UnitList;
	UnitList m_units;

	UnitList::iterator	FindUnit(CPuppet* puppet);
	void	CheckAndAddUnit(IAIObject* object, int type);
	void	CheckAndRemoveUnit(IAIObject* object);

	void UpdateUnit(SUnit& unit, CAIActor* pTarget, float dt);

	Vec3 UpdateFollowPos(SUnit& unit, CAIActor* pTargetActor, float dt);
	Vec3 UpdateFollowCoverPos(SUnit& unit, CAIActor* pTargetActor, float dt);
	Vec3 UpdateStaticPos(SUnit& unit, CAIActor* pTargetActor, float dt);

	void UpdateLookat(SUnit& unit, CAIActor* pTargetActor, float dt);
//	void UpdateFiring(SUnit& unit, float dt);
//	void UpdateStance(SUnit& unit, CAIActor* pTargetActor, float dt);

	const SShape* GetEnclosingGenericShapeOfType(const Vec3& pos, int type, string& outName);
	void ValidateWalkability(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, const Vec3& posTo, Vec3& posOut);
	bool CollectCoverPos(SUnit& unit, const Vec3& reqPos, const Vec3& targetPos, float searchRange);

	CAIGroup* m_pGroup;
};



//====================================================================
// CBossGroupTactic
// Simple boss group tactic.
//====================================================================
class CBossGroupTactic : public IAIGroupTactic
{
public:
	CBossGroupTactic(CAIGroup* pGroup);
	virtual ~CBossGroupTactic();
	virtual void	Update(float dt);
	virtual void	OnObjectRemoved(IAIObject* pObject);
	virtual void	OnUnitAttentionTargetChanged();
	virtual void	OnMemberAdded(IAIObject* pMember);
	virtual void	OnMemberRemoved(IAIObject* pMember);
	virtual void	Reset();
	virtual void	NotifyState(IAIObject* pRequester, int type, float param);
	virtual int		GetState(IAIObject* pRequester, int type, float param);
	virtual Vec3	GetPoint(IAIObject* pRequester, int type, float param);
	virtual void	Serialize(TSerialize ser, CObjectTracker& objectTracker);
	virtual void	DebugDraw(IRenderer* pRend);

private:

	struct SAIDestroyable
	{
		SAIDestroyable(const Vec3& pos, float rad, EntityId ent, EntityId parent) :
			pos(pos), rad(rad), ent(ent), parent(parent), inUse(false), destroyed(false) {}
		Vec3 pos;
		float rad;
		EntityId ent;
		EntityId parent;
		bool inUse;
		bool destroyed;

		void SetInUse(bool state)
		{
			if (inUse == state)
				return;

			inUse = state;

			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(ent);
			if (pEnt)
			{
				if (inUse)
					GetAISystem()->ModifySmartObjectStates(pEnt, "Activated");
				else
					GetAISystem()->ModifySmartObjectStates(pEnt, "-Activated");

				const char*	sEventName = "Use";
				SEntityEvent event(ENTITY_EVENT_SCRIPT_EVENT);
				event.nParam[0] = (INT_PTR)sEventName;
				event.nParam[1] = IEntityClass::EVT_BOOL;
				bool bValue = true;
				event.nParam[2] = (INT_PTR)&bValue;
				pEnt->SendEvent( event );
			}
		}
	};

	struct SConnection
	{
		SConnection(const Vec3& pos, unsigned i) : pos(pos), t(2.0f), i(i) {}
		Vec3 pos;
		float t;
		unsigned i;
	};


	bool IntersectSegmentSphere(const Vec3& p0, const Vec3& p1, const Vec3& center, float rad, float& t0, float& t1);
	bool GetNearestPointOnClippedPath(const Vec3& pos, const SShape* pShape, const std::vector<SAIDestroyable>& destroyable, float& dist, Vec3& pt);
	void DrawClippedPath(IRenderer *pRenderer, const SShape* pShape, const std::vector<SAIDestroyable>& destroyable, ColorB col);

	std::vector<SAIDestroyable>	m_destroyables;
	std::vector<const SShape*>	m_attackPaths;

	struct SUnit
	{
		SUnit() { Reset(); }
		SUnit(CPuppet* obj_)
		{
			Reset();
			obj = obj_;
		}

		void	Reset()
		{
			pLastAttackPath = 0;
			canDamage = false;
			nearestPos.Set(0,0,0);
			searchPos.Set(0,0,0);
			phase = 0;
			state = GN_NOTIFY_ADVANCING;
		}

		void Serialize(TSerialize ser, CObjectTracker& objectTracker);

		std::vector<Vec3>	searchPoints;
		Vec3 searchPos;

		CPuppet* obj;
		const SShape* pLastAttackPath;
		bool canDamage;
		Vec3 nearestPos;
		int phase;
		int state;
	};

	typedef std::vector<SUnit>	UnitList;
	UnitList m_units;


	struct SElectricBolt
	{
		int GetPointCount() const { return points.size(); }
		const Vec3* GetPoints() const { return &points[0]; }

		void Generate(const Vec3& p0, const Vec3& p1, float s, int nIter)
		{
			static std::vector<Vec3> points1;
			static std::vector<Vec3> points2;
			static std::vector<Vec3> offsets1;
			static std::vector<Vec3> offsets2;

			int nPts = 3;
			for (int i = 0; i < nIter; ++i)
				nPts = 2*nPts-1;

			points.resize(nPts);

			points1.resize(nPts);
			offsets1.resize(nPts);
			points2.resize(nPts);
			offsets2.resize(nPts);

			Matrix33	basis;
			basis.SetRotationVDir((p1 - p0).GetNormalized());

			// Create seed set
			points1[0] = p0;
			offsets1[0].Set(0,0,0);
			Vec3 off = basis.TransformVector(Vec3(RandFloat()*s, RandFloat()*s*0.5f, RandFloat()*s));
			points1[1] = (p0+p1)*0.5f + off;
			offsets1[1] = off;
			points1[2] = p1;
			offsets1[2].Set(0,0,0);
			int n = 3;

			Vec3* srcPts = &points1[0];
			Vec3* dstPts = &points2[0];
			Vec3* srcOff = &offsets1[0];
			Vec3* dstOff = &offsets2[0];

			for (int i = 0; i < nIter; ++i)
			{
				n = Subdivide(srcPts, srcOff, dstPts, dstOff, n, s, basis);
				s *= 0.5f;
				std::swap(dstPts, srcPts);
				std::swap(dstOff, srcOff);
			}

			points.resize(nPts);
			offsets.resize(nPts);
			for (int i = 0; i < nPts; ++i)
			{
				points[i] = srcPts[i];
				offsets[i] = srcOff[i].GetNormalized();
			}
		}

		inline float RandFloat() const { return 2*ai_frand() - 1; }

		int Subdivide(Vec3* srcPts, Vec3* srcOff, Vec3* dstPts, Vec3* dstOff, int n, float s, const Matrix33& basis)
		{
			for (int i = 0; i < n-1; ++i)
			{
				dstPts[i*2] = srcPts[i];
				dstOff[i*2] = srcOff[i];
				Vec3 off = basis.TransformVector(Vec3(RandFloat()*s, RandFloat()*s*0.5f, RandFloat()*s));
				dstPts[i*2+1] = (srcPts[i] + srcPts[i+1])*0.5f + off;
				dstOff[i*2+1] = off;
			}
			dstPts[(n-1)*2] = srcPts[n-1];
			dstOff[(n-1)*2] = srcOff[n-1];
			return (n-1)*2+1;
		}

		Vec3* Animate(float t)
		{
			static std::vector<Vec3> tr;
			if (tr.size() < points.size())
				tr.resize(points.size());

			for (int i = 0, ni = points.size(); i < ni; ++i)
				tr[i] = points[i] + offsets[i]*t;

			return &tr[0];
		}

		int niter;
		std::vector<Vec3>	points;
		std::vector<Vec3>	offsets;
	};


	struct SAILightBoltProto
	{
		SAILightBoltProto(const Vec3& pos, unsigned i, const Vec3& posSrc, SUnit* pUnit) :
			pos(pos), i(i), posSrc(posSrc), pUnit(pUnit), t(2.0f), updateTime(0.0f), updateTimeMax(0.0f) {}
		Vec3 pos;
		Vec3 posSrc;
		float t;
		unsigned i;
		SUnit* pUnit;

		SElectricBolt	bolt;
		SElectricBolt	boltSmall;
		float updateTime;
		float updateTimeMax;
		Vec3 boltPos;

	};

	std::vector<SAILightBoltProto> m_DEBUG_connections;


	UnitList::iterator	FindUnit(CPuppet* puppet);
	void	CheckAndAddUnit(IAIObject* object, int type);
	void	CheckAndRemoveUnit(IAIObject* object);

	void UpdateUnit(SUnit& unit, CAIActor* pTarget, float dt);

	CAIGroup* m_pGroup;
};



#endif
