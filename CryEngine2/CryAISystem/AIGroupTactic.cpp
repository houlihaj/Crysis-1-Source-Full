/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   AIGroup.cpp
$Id$
Description: 

-------------------------------------------------------------------------
History:
- 2006				: Created by Mikko Mononen

*********************************************************************/


#include "StdAfx.h"
#include "IAISystem.h"
#include "CAISystem.h"
#include "AIPlayer.h"
#include "Puppet.h"
#include "AIGroupTactic.h"
#include "AIDebugDrawHelpers.h"
#include "WaypointHumanNavRegion.h"
#include <float.h>

#define COVER_SAMPLES 4

static const float COHESION_DIST = 15.0f;

//====================================================================
// SimpleVec3Hash
//====================================================================
unsigned int SimpleVec3Hash(const Vec3& pt, const Vec3& center)
{
	const	Vec3	extends(256.0f, 256.0f, 256.0f);	// 0.5m accuracy assuming 2^10 range
	const int	bits = 10;
	const int	range = (1 << bits) - 1;
	const int	half = 1 << (bits - 1);
	unsigned	x = clamp_tpl((int)((1.0f + (pt.x - center.x) / extends.x) * half), 0, range);
	unsigned	y = clamp_tpl((int)((1.0f + (pt.y - center.y) / extends.y) * half), 0, range);
	unsigned	z = clamp_tpl((int)((1.0f + (pt.z - center.z) / extends.z) * half), 0, range);
	return x | (y << bits) | (z << (bits * 2));
}


ColorB	Transparent(ColorB col, unsigned char alpha) { return ColorB(col.r, col.b, col.g, alpha); }


struct SAITempTeamMember
{
	SAITempTeamMember() {}
	SAITempTeamMember(int i, const Vec3& pos) : i(i), pos(pos) {}
	int i;
	Vec3 pos;
};

struct SAITempTeam
{
	std::vector<SAITempTeamMember> units;
	AABB aabb;
};

struct SAITeamSorterX
{
	SAITeamSorterX() {}
	bool operator()(const SAITempTeamMember& lhs, const SAITempTeamMember& rhs) const { return lhs.pos.x < rhs.pos.x; }
};

struct SAITeamSorterY
{
	SAITeamSorterY() {}
	bool operator()(const SAITempTeamMember& lhs, const SAITempTeamMember& rhs) const { return lhs.pos.y < rhs.pos.y; }
};



//====================================================================
// CHumanGroupTactic
//====================================================================
CHumanGroupTactic::CHumanGroupTactic(CAIGroup* pGroup) :
	m_pGroup(pGroup)
{
	GetAISystem()->RegisterPathFinderListener(this);
}

//====================================================================
// ~CHumanGroupTactic
//====================================================================
CHumanGroupTactic::~CHumanGroupTactic()
{
	GetAISystem()->UnregisterPathFinderListener(this);
	for (unsigned i = 0; i < m_teams.size(); ++i)
	{
		delete m_teams[i];
		m_teams[i] = 0;
	}
	m_teams.clear();
}

//====================================================================
// Update
//====================================================================
void CHumanGroupTactic::Update(float dt)
{
	FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );

	if (m_state.m_units.empty())
		return;

	// Find more or less live targets.
	m_state.m_enabledUnits = 0;
	m_state.m_leaderCount = 0;

	int	unknownTeamCount = 0;

	for (UnitList::iterator it = m_state.m_units.begin(); it != m_state.m_units.end(); ++it)
	{
		SUnit&	unit = *it;

		CPuppet*	pPuppet = unit.obj;
		CAIObject*	pTarget = (CAIObject*)pPuppet->GetAttentionTarget();

		if (!pPuppet->IsEnabled())
		{
			unit.enabled = false;
			continue;
		}

		if (unit.team == -1)
			unknownTeamCount++;

		if (!unit.enabled)
		{
			// New unit enabled, refactor teams.
			DeleteTeams();
			unit.enabled = true;
		}

		m_state.m_enabledUnits++;

		if (unit.leader)
			m_state.m_leaderCount++;

		bool	liveTarget = false;

		if (pTarget)
		{
			unsigned short nType = pTarget->GetType();
			IAIObject::ESubTypes subType = pTarget->GetSubType();

			// Skip threatening sounds from friendly sources.
			if (subType == IAIObject::STP_SOUND)
			{
				IAIObject* pOwner = unit.obj->GetEventOwner(pTarget);
				if (!unit.obj->IsHostile(pOwner))
					continue;
			}
			else if (subType == IAIObject::STP_MEMORY)
			{
				IAIObject* pOwner = pTarget->GetAssociation();
				if (!unit.obj->IsHostile(pOwner))
					continue;
			}
			
			if (nType == AIOBJECT_PUPPET || nType == AIOBJECT_PLAYER || subType == IAIObject::STP_SOUND || subType == IAIObject::STP_MEMORY)
			{
				bool	found = false;
//				Vec3	pos = target->GetPos();
				float	speed = pTarget->GetVelocity().GetLength();

				liveTarget = pPuppet->GetAttentionTargetType() == AITARGET_VISUAL &&
					pPuppet->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE;

/*				for(EnemyList::iterator eit = m_state.m_enemies.begin(); eit != m_state.m_enemies.end(); ++eit)
				{
					SEnemy&	enemy = (*eit);
					if(Distance::Point_Point2DSq(target->GetPos(), (*eit).pos) < sqr(1.0f))
					{
						enemy.importance += 1.0f;
						enemy.speed = max(enemy.speed, speed);
						enemy.live = liveTarget || enemy.live;
						found = true;
						break;
					}
				}
				if(!found)
				{
					// Use good valid point.
					Vec3	floorPos(target->GetPos());
					GetFloorPos(floorPos, floorPos + Vec3(0,0,0.25f), walkabilityFloorUpDist, walkabilityFloorDownDist, walkabilityDownRadius, AICE_ALL);
					floorPos.z += 1.5f;	// Shoot height.

					m_state.m_enemies.push_back(SEnemy(floorPos, speed, liveTarget));
				}*/
			}
		}
	}

	if (unknownTeamCount > 0)
		SetupTeams();

	// Pick new leader if none is assigned and some os available)
/*	if (m_state.m_leaderCount > 0 && !m_state.m_currentLeader)
	{
		// Pick nearest to target.
		float	minDist = FLT_MAX;
		for (UnitList::iterator it = m_state.m_units.begin(); it != m_state.m_units.end(); ++it)
		{
			SUnit&	unit = *it;
			if (!unit.leader) continue;
			float	d = FLT_MAX;
			if (unit.obj->GetAttentionTarget())
				d = Distance::Point_PointSq(unit.obj->GetAttentionTarget()->GetPos(), unit.obj->GetPos());
			if (d < minDist)
			{
				m_state.m_currentLeader = unit.obj;
				minDist = d;
			}
		}
	}*/

	UpdateAvoidPoints(dt);

	if (!m_state.m_enabledUnits)
		return;

	EvaluateSituation(dt);
}

void CHumanGroupTactic::OnPathResult(int id, const std::vector<unsigned>* pathNodes)
{
	for (unsigned i = 0; i < m_teams.size(); ++i)
	{
		SAIGroupTeam& team = *m_teams[i];
		if (team.attackPathId == id)
		{
			// Reset the id
			team.attackPathId = 0;

			team.attackPath.clear();
			team.attackPathNodes.clear();
			if (!pathNodes)
				return;

			const std::vector<unsigned>& nodes = *pathNodes;
			CGraph* pGraph = (CGraph*)GetAISystem()->GetNodeGraph();
			for (unsigned j = 0, nj = nodes.size(); j < nj; ++j)
			{
				const GraphNode* pNode = pGraph->GetNodeManager().GetNode(nodes[j]);
				team.attackPathNodes.push_back(nodes[j]);
				team.attackPath.push_back(pNode->GetPos());
			}

			team.coversDirty = true;
		}
	}
}

void CHumanGroupTactic::OnUnitAttentionTargetChanged()
{
}

void CHumanGroupTactic::OnObjectRemoved(IAIObject* pObject)
{
	CheckAndRemoveUnit(pObject);

	CAIObject* pAI = (CAIObject*)pObject;
	for (unsigned i = 0; i < m_teams.size(); ++i)
	{
		if (m_teams[i] && m_teams[i]->pGroupCenter == pAI)
			m_teams[i]->pGroupCenter = 0;
	}
}

void CHumanGroupTactic::OnMemberAdded(IAIObject* pMember)
{
	if (!pMember) return;

//	AILogEvent("CHumanGroupTactic::OnMemberAdded() #%d %s", m_pGroup->GetGroupId(), pMember->GetName());

/*	CPuppet* pPuppet = pMember->CastToCPuppet();
	if (!pPuppet)
		return;
	int role = GetTeamRole(pPuppet);
	if (role != -1)
		CheckAndAddUnit(pMember, role);*/
}

void CHumanGroupTactic::OnMemberRemoved(IAIObject* pMember)
{
	if (!pMember) return;

//	AILogEvent("CHumanGroupTactic::OnMemberRemoved() #%d %s", m_pGroup->GetGroupId(), pMember->GetName());

	CPuppet* pPuppet = pMember->CastToCPuppet();
	if (!pPuppet)
		return;

	// Allow to leave dead units in the groups, the group tactic should not be interrupted if
	// someone dies.
	if (!pPuppet->IsEnabled() && pPuppet->GetProxy()->GetActorHealth() < 1)
		return;

	CheckAndRemoveUnit(pMember);
}

int CHumanGroupTactic::GetTeamRole(CPuppet* pPuppet)
{
	IEntity* pEntity = pPuppet->GetEntity();
	if (!pEntity)
		return -1;

	if (!pPuppet->GetProxy())
		return -1;

	const char* szCharacter = pPuppet->GetProxy()->GetCharacter();
	if (!szCharacter)
		return -1;

	// Get the global character table.
	SmartScriptTable pGlobalCharacterTable;
	if (!gEnv->pScriptSystem->GetGlobalValue("AICharacter", pGlobalCharacterTable))
		return -1;

	SmartScriptTable pCharacterTable;
	if (!pGlobalCharacterTable->GetValue(szCharacter, pCharacterTable))
		return -1;

	int role = -1;
	pCharacterTable->GetValue("TeamRole", role);

	return role;
}

//====================================================================
// EvaluateSituation
//====================================================================
void CHumanGroupTactic::EvaluateSituation(float dt)
{
FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );
	for (unsigned i = 0; i < m_teams.size(); ++i)
		UpdateTeam(*m_teams[i], dt);
}


//====================================================================
// DeleteTeams
//====================================================================
void CHumanGroupTactic::DeleteTeams()
{
	FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );

	if (m_teams.empty())
		return;

//	AILogEvent("CHumanGroupTactic::DeleteTeams() Group #%d", m_pGroup->GetGroupId());
	for (unsigned i = 0; i < m_teams.size(); ++i)
	{
//		AILogEvent(" - team %d", i);
		SAIGroupTeam* team = m_teams[i];
		m_teams[i] = 0;
		delete team;
	}
	m_teams.clear();

	for (unsigned i = 0, ni = m_state.m_units.size(); i < ni; ++i)
		m_state.m_units[i].team = -1;
}

//====================================================================
// SetupTeams
//====================================================================
void CHumanGroupTactic::SetupTeams()
{
//	if (!m_state.m_enabledUnits)
//		return;

	FUNCTION_PROFILER( gEnv->pSystem,PROFILE_AI );

	DeleteTeams();

//	AILogEvent("CHumanGroupTactic::SetupTeams()");

	for (UnitList::iterator it = m_state.m_units.begin(); it != m_state.m_units.end(); ++it)
	{
		if (it->leader)
			UpdateLeaderTypeMimicry(*it);
	}

	SetupTeam(GU_HUMAN_COVER);
	SetupTeam(GU_HUMAN_SNEAKER);
	SetupTeam(GU_HUMAN_CAMPER);
	SetupTeam(GU_HUMAN_SNEAKER_SPECOP);

//	m_teamUpdateUnitCount = m_state.m_enabledUnits; // m_units.size();
}

void CHumanGroupTactic::SetupTeam(int type)
{
	const float groupingDist = 15.0f;
	const float smallMergeDist = 50.0f;

	int smallTeamSize = 2;
	int maxTeamSize = 5;

	if (type == GU_HUMAN_SNEAKER)
	{
		smallTeamSize = 1;
		maxTeamSize = 3;
	}
	else if (type == GU_HUMAN_CAMPER)
	{
		smallTeamSize = 2;
		maxTeamSize = 6;
	}
	else if (type == GU_HUMAN_SNEAKER_SPECOP)
	{
		smallTeamSize = 1;
		maxTeamSize = 3;
	}

	// Delete old team.
	std::vector<SAITempTeam*>	teams;

	for (unsigned x = 0; x < m_state.m_units.size(); ++x)
	{
		CPuppet* pPuppet = m_state.m_units[x].obj;
		if (m_state.m_units[x].type != type) continue;
		if (!pPuppet->IsEnabled()) continue;

		const Vec3& pos = pPuppet->GetPos();

		int nearestGroup = -1;
		float nearestDist = FLT_MAX;
		for (unsigned i = 0; i < teams.size(); ++i)
		{
			for (unsigned j = 0; j < teams[i]->units.size(); ++j)
			{
				float d = Distance::Point_PointSq(teams[i]->units[j].pos, pos);
				if (d < sqr(groupingDist) && d < nearestDist)
				{
					nearestGroup = (int)i;
					nearestDist = d;
				}
			}
		}

		if (nearestGroup != -1)
		{
			teams[nearestGroup]->units.push_back(SAITempTeamMember(x, pos));
			teams[nearestGroup]->aabb.Add(pos, groupingDist/2);
		}
		else
		{
			teams.push_back(new SAITempTeam);
			teams.back()->aabb.Reset();
			teams.back()->units.push_back(SAITempTeamMember(x, pos));
			teams.back()->aabb.Add(pos, groupingDist/2);
		}
	}

	// Merge overlapping groups
	for (int i = 0; i < (int)teams.size() - 1; ++i)
	{
		if (!teams[i]) continue;
		for (int j = i + 1; j < (int)teams.size(); ++j)
		{
			if (!teams[j]) continue;
			// Only try to merge the groups if the BBs overlap.
			if (!Overlap::AABB_AABB(teams[i]->aabb, teams[j]->aabb))
				continue;

			// If any of the units in the group touch within the grouping distance, merge the teams.
			bool merge = false;
			for (unsigned m = 0; m < teams[i]->units.size(); ++m)
			{
				for (unsigned n = 0; n < teams[j]->units.size(); ++n)
				{
					if (Distance::Point_PointSq(teams[i]->units[m].pos, teams[j]->units[n].pos) < sqr(groupingDist))
					{
						merge = true;
						break;
					}
				}
			}

			if (merge)
			{
				// Move units from the second group to the first and 
				for (unsigned n = 0; n < teams[j]->units.size(); ++n)
				{
					teams[i]->units.push_back(teams[j]->units[n]);
					teams[i]->aabb.Add(teams[j]->units[n].pos, groupingDist/2);
				}
				delete teams[j];
				teams[j] = 0;
			}
		}
	}

	// NOTE: from now on, there can be empty teams.

	// Merge too small groups to larger ones.
	for (unsigned i = 0; i < teams.size(); ++i)
	{
		if (teams[i] && (int)teams[i]->units.size() <= smallTeamSize)
		{
			int nearestSmall = -1;
			float nearestSmallDist = FLT_MAX;
			int nearest = -1;
			float nearestDist = FLT_MAX;

			AABB inflated = teams[i]->aabb;
			float v = smallMergeDist - groupingDist;
			inflated.min -= Vec3(v,v,v);
			inflated.max += Vec3(v,v,v);

			for (unsigned j = 0; j < teams.size(); ++j)
			{
				if (!teams[j]) continue;
				if (i == j) continue;
				if (!Overlap::AABB_AABB(inflated, teams[j]->aabb))
					continue;

				float minDist = FLT_MAX;

				for (unsigned m = 0; m < teams[i]->units.size(); ++m)
				{
					for (unsigned n = 0; n < teams[j]->units.size(); ++n)
					{
						float d = Distance::Point_PointSq(teams[i]->units[m].pos, teams[j]->units[n].pos);
						if (d < sqr(smallMergeDist) && d < minDist)
							minDist = d;
					}
				}

				if (teams[j]->units.size() <= 3)
				{
					if (minDist < nearestSmallDist)
					{
						nearestSmall = j;
						nearestSmallDist = minDist;
					}
				}
				else
				{
					if (minDist < nearestDist)
					{
						nearest = j;
						nearestDist = minDist;
					}
				}
			}

			if (nearestSmall != -1)
				nearest = nearestSmall;

			if (nearest != -1)
			{
				// Move units to the other group.
				for (unsigned n = 0; n < teams[i]->units.size(); ++n)
				{
					teams[nearest]->units.push_back(teams[i]->units[n]);
					teams[nearest]->aabb.Add(teams[i]->units[n].pos, groupingDist/2);
				}
				delete teams[i];
				teams[i] = 0;
			}
		}
	}


	// Purge empty groups
	for (unsigned i = 0; i < teams.size();)
	{
		if (!teams[i])
		{
			teams[i] = teams.back();
			teams.pop_back();
		}
		else
			++i;
	}

	// Split too large groups
	int n = teams.size();
	for (unsigned i = 0; i < n; ++i)
	{
		int np = teams[i]->units.size();
		if (np > maxTeamSize)
		{
			int top = teams.size();
			int ng = (np + maxTeamSize-1) / maxTeamSize; // group count
			teams.resize(teams.size() + ng-1); // create groups

			Vec3 size = teams[i]->aabb.GetSize();
			if (size.x > size.y)
				std::sort(teams[i]->units.begin(), teams[i]->units.end(), SAITeamSorterX());
			else
				std::sort(teams[i]->units.begin(), teams[i]->units.end(), SAITeamSorterY());

			int a = teams[i]->units.size() - 1;
			int ap = np / ng;

			for (int k = 0; k < ng-1; ++k)
			{
				// Move half the units
				teams[top+k] = new SAITempTeam;
				teams[top+k]->aabb.Reset();
				for (int j = 0; j < ap; ++j)
				{
					teams[top+k]->units.push_back(teams[i]->units[a]);
					teams[top+k]->aabb.Add(teams[i]->units[a].pos, groupingDist/2);
					--a;
				}
			}

			// Resize the old group
			teams[i]->units.resize(a+1);
			teams[i]->aabb.Reset();
			for (int j = 0; j < teams[i]->units.size(); ++j)
				teams[i]->aabb.Add(teams[i]->units[j].pos, groupingDist/2);
		}
	}

//	bool side = ai_rand() > RAND_MAX/2 ? SIDE_LEFT : SIDE_RIGHT;

	for (unsigned i = 0; i < teams.size(); ++i)
	{
		if (!teams[i]->units.empty())
		{
			// Validate team.
			SAITempTeam* t = teams[i];

			// All units must have same territory
			SShape* pLastShape = 0;
			for (unsigned j = 0; j < t->units.size(); ++j)
			{
				AIAssert(t->units[j].i >= 0 && t->units[j].i < m_state.m_units.size());
				SUnit& unit = m_state.m_units[t->units[j].i];
				if (j > 0)
				{
					if (pLastShape != unit.obj->GetTerritoryShape())
					{
						AIWarning("Territory shapes for units in group #%d are not the same (dump follows).", m_pGroup->GetGroupId());
						for (unsigned k = 0; k < t->units.size(); ++k)
						{
							AIAssert(t->units[k].i >= 0 && t->units[k].i < m_state.m_units.size());
							SUnit& un = m_state.m_units[t->units[k].i];
							if (un.obj->GetTerritoryShape())
								AIWarning(" - Entity %s is in territory %s.", un.obj->GetName(), un.obj->GetTerritoryShapeName());
							else
								AIWarning(" - Entity %s is in not in any territory.", un.obj->GetName());
						}
						break;
					}
				}
				pLastShape = unit.obj->GetTerritoryShape();
			}

			SAIGroupTeam* newTeam = new SAIGroupTeam;
			newTeam->unitTypes = type;

			int teamId = m_teams.size();

			for (unsigned j = 0; j < teams[i]->units.size(); ++j)
			{
				newTeam->units.push_back(teams[i]->units[j].i);
				AIAssert(t->units[j].i >= 0 && t->units[j].i < m_state.m_units.size());
				SUnit& unit = m_state.m_units[t->units[j].i];
				unit.team = teamId;
			}

			m_teams.push_back(newTeam);
		}

		delete teams[i]; // delete temp.
	}
}

struct SAIUnitSorter
{
	SAIUnitSorter(const std::vector<float> &values) : m_values(values) {}
	bool operator()(int lhs, int rhs) const { return m_values[lhs] < m_values[rhs]; }
	const std::vector<float>&	m_values;
};

//====================================================================
// UpdateAttackDirection
//====================================================================
void CHumanGroupTactic::UpdateAttackDirection(SAIGroupTeam& team, SUnit& unit)
{
	if (team.coversDirty)
	{
		int lastNavNode = 0;
		team.attackPathTarget.Set(0,0,0);

		CPuppet* pLeader = GetLeaderUnit(team);
		if (!pLeader)
			return;

		Vec3 leaderPos = pLeader->GetPos();
		team.attackPathTarget = leaderPos;

		float minDist = 6.0f;
		float spacing = 3.0f;

		if (team.lastAttackPathTo.IsZero() || team.lastAttackPathFrom.IsZero())
		{
			team.lastAttackPathTo = team.avgEnemyPos;
			team.lastAttackPathFrom = team.avgTeamPos;
		}

		Vec3 moveDir = team.lastAttackPathTo - team.lastAttackPathFrom;
		float moveDist = moveDir.NormalizeSafe();
		float advanceDist = moveDist * 0.5f;

		// Allow to get closer when in not in combat
		if (team.eval == GS_SEEK || team.eval == GS_SEARCH || team.eval == GS_ALERTED || team.eval == GS_IDLE)
		{
			if (moveDist < 20.0f)
				advanceDist = moveDist * 0.8f;
			minDist = 2.0f;
		}

		if (!team.attackPath.empty())
		{
			SShape* pTerritoryShape = pLeader->GetTerritoryShape();

			// Find proper attack pos, walk back from the targe and pick the first valid location.
			for (int i = team.attackPath.size() - 1; i >= 0; --i)
			{
				team.attackPathTarget = team.attackPath[i];
				lastNavNode = team.attackPathNodes[i];
				if (pTerritoryShape && pTerritoryShape->IsPointInsideShape(team.attackPath[i], true))
					break;
				float d = moveDir.Dot(team.attackPath[i] - team.lastAttackPathFrom);
				if (d < advanceDist)
					break;
			}

			// Adjust formation size for indoors.
			CGraph* pGraph = (CGraph*)GetAISystem()->GetNodeGraph();
			const GraphNode* pNode = pGraph->GetNodeManager().GetNode(lastNavNode);
			if (pNode)
			{
				if (pNode->navType != IAISystem::NAV_TRIANGULAR)
				{
					// Indoor
					minDist = 3.0f;
					spacing = 1.5f;
				}
			}
		}

		if (team.unitTypes == GU_HUMAN_SNEAKER_SPECOP)
		{
			minDist = 3.0f;
			spacing = 1.5f;
		}

		// Assume that the whole group has same territory.
		if (pLeader->GetTerritoryShape())
			pLeader->GetTerritoryShape()->ConstrainPointInsideShape(team.attackPathTarget, true);

		// Get possible cover positions from the navigation graph.
		IAISystem::tNavCapMask navCapMask = pLeader->GetMovementAbility().pathfindingProperties.navCapMask;
		float passRadius = pLeader->m_Parameters.m_fPassRadius;
		team.coversDirty = false;
		CollectCoverPos(team.attackPathTarget, lastNavNode, 17.0f, passRadius, navCapMask, team.covers);

		// Calculate the formation, snap the formation points to nearest covers and assign points.

		// Create an order lookup table which maps the current units from left to right along avg attack normal.
		static std::vector<int> order;
		static std::vector<float> value;
		order.clear();
		value.clear();

		Vec3 dirToTarget = -team.avgTeamDir;
		Vec3 normToTarget = team.avgTeamNorm;

		for (unsigned i = 0, n = team.units.size(); i < n; ++i)
		{
			AIAssert(team.units[i] >= 0 && team.units[i] < m_state.m_units.size());
			SUnit& other = m_state.m_units[team.units[i]];
			if (!other.obj->IsEnabled()) continue;
			if (other.obj->GetProxy()->IsCurrentBehaviorExclusive()) continue;
			order.push_back(i);
			value.push_back(normToTarget.Dot(other.obj->GetPos() - team.avgTeamPos));
		}
		std::sort(order.begin(), order.end(), SAIUnitSorter(value));

		// Clamp the formation distance from target.
		float dist = Distance::Point_Point(team.lastAttackPathTo, team.attackPathTarget);
		if (dist < minDist)
			dist = minDist;

		// The formation is aligned along a circle. Calculate the delta angle between units based on distance.
		float s = clamp(spacing / dist, -1.0f, 1.0f);
		float da = asinf(s);

		float a;
		if ((team.unitTypes == GU_HUMAN_SNEAKER || team.unitTypes == GU_HUMAN_SNEAKER_SPECOP) && !team.defendPositionSet)
		{
			// Bias the formation to the side based on flank side.
			if (team.side == SIDE_LEFT)
				a = -da*(order.size()+1);
			else
				a = da;
		}
		else
		{
			// Center formation.
			a = -da*order.size()*0.5f;
		}

		for (unsigned i = 0, ni = order.size(); i < ni; ++i)
		{
			unsigned idx = order[i];
			AIAssert(team.units[idx] >= 0 && team.units[idx] < m_state.m_units.size());
			SUnit& curUnit = m_state.m_units[team.units[idx]];
			SShape* pTerritoryShape = curUnit.obj->GetTerritoryShape();
			float curPassRadius = curUnit.obj->m_Parameters.m_fPassRadius;

			// Calculate the position in the formation.
			float dd = dist + (i&1)*spacing;
			Vec3 pos = team.lastAttackPathTo + (sinf(a)*dd*normToTarget) + (cosf(a)*dd*dirToTarget);

			// Bias towards the side the point belongs to prevent the formation points to switch order and cross each other.
			const bool biasLeft = a < 0.0f;

			float nearestDist = 15.0f;	// Max deviation
			int nearestIdx = -1;

			// Snap the formation point on the cover points.
			for (unsigned j = 0, nj = team.covers.size(); j < nj; ++j)
			{
				SCoverPos& cover = team.covers[j];
				if (cover.status == CHECK_INVALID) continue;

				// Too close to other points so far.
				bool tooClose = false;
				for (unsigned k = 0, nk = m_state.m_units.size(); k < nk; ++k)
				{
					SUnit& unit2 = m_state.m_units[k];
					if (unit2.obj == curUnit.obj) continue;
					if (Distance::Point_Point2DSq(cover.pos, unit2.advancePos) < sqr(spacing*0.7f))
					{
						tooClose = true;
						break;
					}
				}
				if (tooClose) continue;

				// Choose nearest point
				Vec3 diff = cover.pos - pos;
				float x = normToTarget.Dot(diff);
				float y = dirToTarget.Dot(diff);

				if (biasLeft)
				{
					if (x > 0.0f) x *= 5.0f;
				}
				else
				{
					if (x < 0.0f) x *= 5.0f;
				}

				float d = sqr(x) + sqr(y);
				if (d < nearestDist)
				{
					ValidateCover(cover, curPassRadius, pTerritoryShape);
					if (cover.status == CHECK_OK)
					{
						nearestDist = d;
						nearestIdx = (int)j;
					}
				}
			}

			if (nearestIdx >= 0)
			{
				curUnit.advancePos = team.covers[nearestIdx].pos;
				team.covers[nearestIdx].status = CHECK_INVALID;	// mark it used.
			}
			else
				curUnit.advancePos = pos;

			a += da;
		}
	}

	unit.obj->SetRefPointPos(unit.advancePos);
}

void CHumanGroupTactic::ValidateCover(SCoverPos& cover, float passRadius, SShape* pTerritoryShape)
{
	if (cover.status == CHECK_UNSET)
	{
		if (pTerritoryShape && !pTerritoryShape->IsPointInsideShape(cover.pos, true))
		{
			cover.status = CHECK_INVALID;
			return;
		}

		// TODO: Check for path obstacles?

		if (cover.navType == IAISystem::NAV_TRIANGULAR)
		{
			if (GetAISystem()->IsPointForbidden(cover.pos, passRadius, 0, 0))
			{
				cover.status = CHECK_INVALID;
				return;
			}
		}

		cover.status = CHECK_OK;
	}
}


//====================================================================
// UpdateTeam
//====================================================================
void CHumanGroupTactic::UpdateTeamStats(SAIGroupTeam& team, float dt)
{
	CTimeValue frameTime = GetAISystem()->GetFrameStartTime();
	if (team.lastUpdateTime == frameTime)
		return;
	team.lastUpdateTime = frameTime;

	std::vector<Vec3> hullPoints;

	team.bounds.Reset();
	team.enabledUnits = 0;
	team.availableUnits = 0;
	team.liveEnemyCount = 0;

	team.advancingCount = 0;
	team.targetCount = 0;
	team.minAlertness = 10;
	team.maxAlertness = 0;
	team.needSeekCount = 0;
	team.needSearchCount = 0;
	team.searchCount = 0;
	team.reinforceCount = 0;

	team.avgTeamPos.Set(0,0,0);
	team.avgTeamDir.Set(0,0,0);
	team.avgTeamNorm.Set(0,0,0);
	team.avgEnemyPos.Set(0,0,0);
	float avgTeamPosWeight = 0.0f;
	Vec3 avgTeamPosUnavail(0,0,0);

	Vec3 avgLiveTargetPos(0,0,0);
	Vec3 avgTargetPos(0,0,0);
	Vec3 avgBodyDir(0,0,0);
	Vec3 avgTargetDir(0,0,0);
	Vec3 avgLiveTargetDir(0,0,0);
	float avgLiveTargetWeight = 0.0f;
	float avgTargetWeight = 0.0f;

	int debugGroup = GetAISystem()->m_cvDrawGroupTactic->GetIVal();
	bool calcHull = debugGroup >= 2;

	SUnit* pMostLostUnit = 0;
	float mostLostUnitDist = 0.0f;

	EAITargetType		newMaxType = AITARGET_NONE;
	EAITargetThreat	newMaxThreat = AITHREAT_NONE;

	for (unsigned i = 0, ni = team.units.size(); i < ni; ++i)
	{
		AIAssert(team.units[i] >= 0 && team.units[i] < m_state.m_units.size());
		SUnit& unit = m_state.m_units[team.units[i]];
		if (!unit.obj->IsEnabled()) continue;

		// Min distance to team member
		unit.nearestUnit = -1;
		unit.nearestUnitDist = FLT_MAX;
		unit.mostLost = false;
		for (unsigned j = 0, nj = team.units.size(); j < nj; ++j)
		{
			if (i == j) continue;
			AIAssert(team.units[j] >= 0 && team.units[j] < m_state.m_units.size());
			SUnit& mate = m_state.m_units[team.units[j]];
			if (!mate.obj->IsEnabled()) continue;
			float dist = Distance::Point_Point(unit.obj->GetPos(), mate.obj->GetPos());
			if (dist < unit.nearestUnitDist)
			{
				unit.nearestUnit = team.units[j];
				unit.nearestUnitDist = dist;
			}
		}

		if (unit.nearestUnit != -1 && unit.nearestUnitDist > mostLostUnitDist)
		{
			pMostLostUnit = &unit;
			mostLostUnitDist = unit.nearestUnitDist;
		}

		if (unit.obj->GetAttentionTarget())
		{
			avgTargetPos += unit.obj->GetAttentionTarget()->GetPos();
			avgTargetDir += unit.obj->GetAttentionTarget()->GetPos() - unit.obj->GetPos();
			avgTargetWeight += 1.0f;

			if (unit.obj->GetAttentionTargetType() == AITARGET_VISUAL &&
					unit.obj->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
			{
				avgLiveTargetPos += unit.obj->GetAttentionTarget()->GetPos();
				avgLiveTargetDir += unit.obj->GetAttentionTarget()->GetPos() - unit.obj->GetPos();
				avgLiveTargetWeight += 1.0f;
			}
		}

		newMaxType = max(newMaxType, unit.obj->GetAttentionTargetType());
		newMaxThreat = max(newMaxThreat, unit.obj->GetAttentionTargetThreat());

		if (unit.curAction == UA_REINFORCING)
			team.reinforceCount++;

		team.enabledUnits++;
		avgTeamPosUnavail += unit.obj->GetPos();

		if (unit.obj->GetProxy()->IsCurrentBehaviorExclusive()) continue;
		//		if (unit.curAction == UA_UNAVAIL) continue;

		team.availableUnits++;

		float w = 1.0f;
		if (unit.nearestUnit != -1 && unit.nearestUnitDist > 0.001f)
			w = 1.0f / unit.nearestUnitDist;

		team.avgTeamPos += unit.obj->GetPos() * w;
		avgTeamPosWeight += w;

		avgBodyDir += unit.obj->GetBodyDir();

		if (unit.curAction == UA_ADVANCING)
			team.advancingCount++;
		if (unit.curAction == UA_SEARCHING)
			team.searchCount++;

		if (unit.obj->GetAttentionTarget())
		{
//			int perceivedType = unit.obj->GetPerceivedAttentionTargetType();

			if (unit.obj->GetAttentionTargetType() == AITARGET_VISUAL &&
				unit.obj->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
				team.liveEnemyCount++;
			if (unit.obj->GetAttentionTargetThreat() == AITHREAT_THREATENING)
				team.needSeekCount++;
			else if (unit.obj->GetAttentionTargetThreat() <= AITHREAT_INTERESTING)
				team.needSearchCount++;
			team.targetCount++;
		}
//		else
//			team.needSeekCount++;

		AIAssert(unit.obj->GetProxy());
		SAIBodyInfo bi;
		unit.obj->GetProxy()->QueryBodyInfo(bi);
		bi.stanceSize.Move(unit.obj->GetPhysicsPos());
		team.bounds.Add(bi.stanceSize);

		team.minAlertness = min(team.minAlertness, unit.obj->GetAlertness());
		team.maxAlertness = max(team.maxAlertness, unit.obj->GetAlertness());

		Vec3 center = bi.stanceSize.GetCenter();
		Vec3 s = bi.stanceSize.GetSize();

		if (calcHull)
		{
			hullPoints.push_back(center + Vec3(-s.x, -s.y, 0));
			hullPoints.push_back(center + Vec3(s.x, -s.y, 0));
			hullPoints.push_back(center + Vec3(s.x, s.y, 0));
			hullPoints.push_back(center + Vec3(-s.x, s.y, 0));
		}
	}

	if (pMostLostUnit)
		pMostLostUnit->mostLost = true;

	if (!team.enabledUnits)
		return;

	if (team.availableUnits)
	{
		team.avgTeamPos /= avgTeamPosWeight;
	}
	else
	{
		team.avgTeamPos = avgTeamPosUnavail / (float)team.enabledUnits;
	}

	team.avgEnemyPos = team.avgTeamPos;

	if (avgLiveTargetWeight > 0.0f)
	{
		team.avgTeamDir = avgLiveTargetDir / avgLiveTargetWeight;
		team.avgTeamDir.NormalizeSafe();
		team.avgEnemyPos = avgLiveTargetPos / avgLiveTargetWeight;
	}
	else if (avgTargetWeight > 0.0f)
	{
		team.avgTeamDir = avgTargetDir / avgTargetWeight;
		team.avgTeamDir.NormalizeSafe();
		team.avgEnemyPos = avgTargetPos / avgTargetWeight;
	}
	else if (team.enabledUnits)
	{
		team.avgTeamDir = avgBodyDir.GetNormalizedSafe(ZERO);
	}
	team.avgTeamNorm.Set(team.avgTeamDir.y, -team.avgTeamDir.x, 0);

	if (team.liveEnemyCount > 0)
		team.targetLostTime = 0.0f;
	else
		team.targetLostTime += dt;

	if (!team.initialGroupCenterSet)
	{
		const short COMBAT_PROTECT_THIS_POINT	= 314;
		IAIObject* pAnchor = GetAISystem()->GetNearestObjectOfTypeInRange(team.avgTeamPos, COMBAT_PROTECT_THIS_POINT, 0.0f, 60.0f, 0, 0);
		if (pAnchor && !pAnchor->GetPos().IsZero())
		{
			team.initialGroupCenter = pAnchor->GetPos();
			team.initialGroupCenterSet = true;
		}
		else if (team.availableUnits && !team.avgTeamPos.IsZero())
		{
			team.initialGroupCenter = team.avgTeamPos;
			team.initialGroupCenterSet = true;
		}
	}

	if (calcHull)
	{
		ConvexHull2D(team.hull, hullPoints);
		if (!Overlap::Point_Polygon2D(team.labelPos, team.hull))
			team.labelPos = team.bounds.GetCenter();
	}

	// Update transitions
	EGroupState lastEval = team.eval;

	team.maxThreat = newMaxThreat;
	team.maxType = newMaxType;

	if (team.reinforceCount > 0)
	{
		team.eval = GS_REINFORCE;
	}
	else if (team.eval == GS_IDLE)
	{
		if (newMaxThreat == AITHREAT_THREATENING)
			team.eval = GS_ALERTED;
		else if (newMaxThreat == AITHREAT_AGGRESSIVE)
		{
			if (newMaxType == AITARGET_VISUAL || newMaxType == AITARGET_MEMORY)
				team.eval = GS_ADVANCE;
			else
				team.eval = GS_SEEK;
		}
	}
	else if (team.eval == GS_ALERTED)
	{
		if (newMaxThreat == AITHREAT_NONE)
			team.eval = GS_IDLE;
		else if (newMaxThreat == AITHREAT_AGGRESSIVE)
		{
			if (newMaxType == AITARGET_VISUAL || newMaxType == AITARGET_MEMORY)
				team.eval = GS_ADVANCE;
			else
				team.eval = GS_SEEK;
		}
	}
	else if (team.eval == GS_ADVANCE)
	{
		if (newMaxThreat <= AITHREAT_INTERESTING)
			team.eval = GS_SEARCH;
		else if (newMaxThreat == AITHREAT_THREATENING)
			team.eval = GS_SEEK;
	}
	else if (team.eval == GS_SEEK)
	{
		if (newMaxThreat <= AITHREAT_INTERESTING)
			team.eval = GS_SEARCH;
		else if (newMaxThreat == AITHREAT_AGGRESSIVE)
		{
			if (newMaxType == AITARGET_VISUAL || newMaxType == AITARGET_MEMORY)
				team.eval = GS_ADVANCE;
		}
	}
	else if (team.eval == GS_SEARCH)
	{
		if (newMaxThreat == AITHREAT_NONE)
			team.eval = GS_IDLE;
		else if (newMaxThreat == AITHREAT_THREATENING)
			team.eval = GS_SEEK;
		else if (newMaxThreat == AITHREAT_AGGRESSIVE)
		{
			if (newMaxType == AITARGET_VISUAL || newMaxType == AITARGET_MEMORY)
				team.eval = GS_ADVANCE;
			else
				team.eval = GS_SEEK;
		}
	}
	else if (team.eval == GS_REINFORCE)
	{
		if (newMaxThreat == AITHREAT_THREATENING)
			team.eval = GS_ALERTED;
		else if (newMaxThreat == AITHREAT_AGGRESSIVE)
		{
			if (newMaxType == AITARGET_VISUAL || newMaxType == AITARGET_MEMORY)
				team.eval = GS_ADVANCE;
			else
				team.eval = GS_SEEK;
		}
		if (team.searchCount >= team.availableUnits)
			team.eval = GS_SEARCH;
	}
}

//====================================================================
// CollectCoverPos
//====================================================================
void CHumanGroupTactic::CollectCoverPos(const Vec3& reqPos, int lastNavNode, float searchRange, float passRadius, IAISystem::tNavCapMask navCapMask, std::vector<SCoverPos>& covers)
{
	MapConstNodesDistance traversedNodes;
	CGraph* pGraph = GetAISystem()->GetGraph();
	pGraph->GetNodesInRange(traversedNodes, reqPos, searchRange, navCapMask, passRadius, lastNavNode, 0);

	covers.clear();
	if (traversedNodes.empty())
		return;

	VectorSet<int> touchedLinks;

	CVertexList& vertList = GetAISystem()->m_VertexList;
	for (MapConstNodesDistance::iterator it = traversedNodes.begin(); it != traversedNodes.end(); ++it)
	{
		const GraphNode* node = it->first;

		if (node->navType != IAISystem::NAV_TRIANGULAR)
		{
			covers.push_back(SCoverPos(node->GetPos(), node->navType));
		}
		else
		{
			const STriangularNavData* pTriData = node->GetTriangularNavData();

			unsigned n = 0;
			for (unsigned link = node->firstLinkIndex; link; link = pGraph->GetLinkManager().GetNextLink(link))
			{
				unsigned bidirLinkId = link & ~1;
				if (touchedLinks.find(bidirLinkId) != touchedLinks.end())
					continue;
				touchedLinks.insert(bidirLinkId);

				float rad = pGraph->GetLinkManager().GetRadius(link);
				if (rad > 1.0f)
				{
					if (rad > 3.0f)
					{
						const ObstacleData& v0 = vertList.GetVertex(pTriData->vertices[pGraph->GetLinkManager().GetStartIndex(link)]);
						const ObstacleData& v1 = vertList.GetVertex(pTriData->vertices[pGraph->GetLinkManager().GetEndIndex(link)]);
						Vec3 delta = v1.vPos - v0.vPos;
						float len = delta.NormalizeSafe();
						covers.push_back(SCoverPos(v0.vPos + delta * (v0.fApproxRadius + 0.5f), node->navType));
						covers.push_back(SCoverPos(v0.vPos + delta * (len - v1.fApproxRadius - 0.5f), node->navType));
						n += 2;
					}
					else
					{
						covers.push_back(SCoverPos(pGraph->GetLinkManager().GetEdgeCenter(link), node->navType));
						n++;
					}
				}
			}
			if (n == 0)
				covers.push_back(SCoverPos(node->GetPos(), node->navType));
		}
	}
}

//====================================================================
// GetLeaderUnit
//====================================================================
CPuppet* CHumanGroupTactic::GetLeaderUnit(SAIGroupTeam& team)
{
	if (!team.pLeader)
	{
		// Due bad level design, sometimes not all the units have territories setup properly.
		// As long as the territorial units are alive, keep them leaders.
		bool requireTerritory = false;
		for (unsigned i = 0, n = team.units.size(); i < n; ++i)
		{
			AIAssert(team.units[i] >= 0 && team.units[i] < m_state.m_units.size());
			SUnit& unit = m_state.m_units[team.units[i]];
			if (!unit.obj->IsEnabled()) continue;
			if (unit.obj->GetProxy()->IsCurrentBehaviorExclusive()) continue;
			if (unit.obj->GetTerritoryShape())
			{
				requireTerritory = true;
				break;
			}
		}

		float nearestDist = FLT_MAX;
		for (unsigned i = 0, n = team.units.size(); i < n; ++i)
		{
			AIAssert(team.units[i] >= 0 && team.units[i] < m_state.m_units.size());
			SUnit& unit = m_state.m_units[team.units[i]];
			if (!unit.obj->IsEnabled()) continue;
			if (unit.obj->GetProxy()->IsCurrentBehaviorExclusive()) continue;

			if (requireTerritory && !unit.obj->GetTerritoryShape()) continue;

			float d = Distance::Point_PointSq(team.avgEnemyPos, unit.obj->GetPos());
			if (d < nearestDist)
			{
				nearestDist = d;
				team.pLeader = unit.obj;
			}
		}
	}

	return team.pLeader;
}

//====================================================================
// UpdateTeam
//====================================================================
void CHumanGroupTactic::UpdateTeam(SAIGroupTeam& team, float dt)
{
	UpdateTeamStats(team, dt);

	if (team.readabilityBlockTime > 0.0f)
		team.readabilityBlockTime -= dt;

	if (!team.availableUnits)
		return;

	if (team.targetCount)
	{
		bool lastAttackPathTimeRes = team.lastAttackPathTime < 0.0f;
		team.lastAttackPathTime -= dt;
		if (team.lastAttackPathTime < 0.0f)
		{
			if (team.lastAttackPathTime < -4.0f || Distance::Point_PointSq(team.lastAttackPathTo, team.avgEnemyPos) > sqr(5.0f))
			{
				// Choose target pos
				Vec3 targetPos;
				if (team.defendPositionSet)
					targetPos = team.defendPosition;
				else if (team.unitTypes == GU_HUMAN_CAMPER)
				{
					if (team.eval == GS_ALERTED)
						targetPos = team.avgEnemyPos;	// When no enemy seen yet, try to approach it.
					else
						targetPos = team.initialGroupCenter;
				}
				else
					targetPos = team.avgEnemyPos;

				CPuppet* pLeader = GetLeaderUnit(team);
				if (pLeader&& team.attackPathId <= 0)
				{
					IAISystem::tNavCapMask navCapMask = pLeader->GetMovementAbility().pathfindingProperties.navCapMask;
					float passRadius = pLeader->m_Parameters.m_fPassRadius;
					team.lastNavNode = pLeader->m_lastNavNodeIndex;

					Vec3 fromPos = pLeader->GetPos();
/*					if (Distance::Point_PointSq(team.attackPathTarget, fromPos) < sqr(30.0f))
						fromPos = team.attackPathTarget;*/

					CStandardHeuristic::ExtraConstraints extraConst;
					if ((team.unitTypes == GU_HUMAN_SNEAKER || team.unitTypes == GU_HUMAN_SNEAKER_SPECOP) && !team.defendPositionSet)
					{
						// Calculate and apply flank fence.
						Vec3 flankFenceDir = team.flankFenceQ - team.flankFenceP;
						Vec3 flankFenceNorm(flankFenceDir.y, -flankFenceDir.x, 0.0f);
						bool updateFence = flankFenceNorm.IsZero();
						if (!updateFence)
						{
							// Check if the enemy or the team has crossed the line.
							flankFenceNorm.Normalize();
							float puppetDist = flankFenceNorm.Dot(pLeader->GetPos() - team.flankFenceP);
							float enemyDist = flankFenceNorm.Dot(team.avgEnemyPos - team.flankFenceP);
							updateFence = puppetDist < -3.0f || enemyDist > -3.0f;
						}

						float fenceRad = 3.0f;
						int nbid; IVisArea *iva;
						IAISystem::ENavigationType targetNavType = GetAISystem()->CheckNavigationType(targetPos, nbid, iva, navCapMask);
						if (targetNavType == IAISystem::NAV_WAYPOINT_HUMAN)
							fenceRad *= 0.5f;

						if (updateFence)
						{
							Vec3 dir = targetPos - fromPos;
							float len = dir.NormalizeSafe();
							Vec3 norm(dir.y, -dir.x, 0);
							norm.Normalize();

							// Update team side, check how many teams are left and right from the current team.
							int left = 0;
							int right = 0;
							for (unsigned i = 0, ni = m_teams.size(); i < ni; ++i)
							{
								if (m_teams[i] == &team) continue;
								if (m_teams[i]->avgTeamPos.IsZero()) continue;
								if (m_teams[i]->unitTypes != GU_HUMAN_SNEAKER && m_teams[i]->unitTypes != GU_HUMAN_SNEAKER_SPECOP) continue;
								if (norm.Dot(m_teams[i]->avgTeamPos - fromPos) < 0.0f)
									left++;
								else
									right++;
							}
							// Bias current selection.
							if (team.side == SIDE_LEFT)
							{
								if (left <= right)
									team.side = SIDE_LEFT;
								else
									team.side = SIDE_RIGHT;
							}
							else
							{
								if (right <= left)
									team.side = SIDE_RIGHT;
								else
									team.side = SIDE_LEFT;
							}

							if (team.side == SIDE_LEFT)
								norm = -norm;
							Vec3 mid = fromPos + (targetPos - fromPos) * 0.5f;
							float fenceWidth = clamp(len/2, 4.0f, 15.0f);
							if (targetNavType == IAISystem::NAV_WAYPOINT_HUMAN)
								fenceWidth *= 0.5f;
							team.flankFenceP = mid - norm * fenceWidth*1.5f;
							team.flankFenceQ = mid + norm * fenceWidth*0.2f;
//							team.flankFenceP = mid - norm * fenceWidth;
//							team.flankFenceQ = mid + norm * fenceWidth;
						}

						CStandardHeuristic::SExtraConstraint capsule;
						capsule.type = CStandardHeuristic::SExtraConstraint::ECT_AVOIDCAPSULE;
						capsule.constraint.avoidCapsule.minDistSq = sqr(fenceRad);
						capsule.constraint.avoidCapsule.px = team.flankFenceP.x;
						capsule.constraint.avoidCapsule.py = team.flankFenceP.y;
						capsule.constraint.avoidCapsule.pz = team.flankFenceP.z;
						capsule.constraint.avoidCapsule.qx = team.flankFenceQ.x;
						capsule.constraint.avoidCapsule.qy = team.flankFenceQ.y;
						capsule.constraint.avoidCapsule.qz = team.flankFenceQ.z;
						extraConst.push_back(capsule);
					}
					
					team.attackPathId = GetAISystem()->RequestRawPathTo(fromPos, targetPos, passRadius, navCapMask, team.lastNavNode, true, 15.0f, extraConst, pLeader);

					team.lastAttackPathFrom = pLeader->GetPos();
					team.lastAttackPathTo = team.avgEnemyPos;
					team.lastAttackPathTime = 4.0f;
				}
			}
			else
			{
				// The time expired, but no need to regen the path, adjust the covers.
				if (!lastAttackPathTimeRes)
					team.coversDirty = true;
			}
		}
	}

	static std::vector<int> order;
	static std::vector<float> value;
	order.clear();
	value.clear();

	Vec3 attackDir = team.avgTeamDir;
	attackDir.z = 0;
	attackDir.NormalizeSafe();
	Vec3 attackNorm(attackDir.y, -attackDir.x, 0);

	for (unsigned i = 0, n = team.units.size(); i < n; ++i)
	{
		AIAssert(team.units[i] >= 0 && team.units[i] < m_state.m_units.size());
		SUnit& unit = m_state.m_units[team.units[i]];
		if (!unit.obj->IsEnabled()) continue;
		if (unit.obj->GetProxy()->IsCurrentBehaviorExclusive()) continue;
//		if (unit.curAction == UA_UNAVAIL) continue;
		order.push_back(i);
		value.push_back(attackNorm.Dot(unit.obj->GetPos() - team.avgTeamPos));
	}

	std::sort(order.begin(), order.end(), SAIUnitSorter(value));

	for (unsigned i = 0, ni = order.size(); i < ni; ++i)
	{
		unsigned idx = order[i];
		AIAssert(team.units[idx] >= 0 && team.units[idx] < m_state.m_units.size());
		SUnit& unit = m_state.m_units[team.units[idx]];
		unit.order = (int)i - (int)ni/2;
	}

	CAIObject* pPlayer = GetAISystem()->GetPlayer();

	if (pPlayer && team.availableUnits > 1)
	{
		const Vec3& playerPos = pPlayer->GetPos();
		const Vec3& playerView = pPlayer->GetViewDir();
		Lineseg playerLOS(playerPos, playerPos + playerView*30.0f);

		// Update team readability
		SUnit* pNearest = 0;
		float nearestDist = FLT_MAX;

		SUnit* pNearestSeek = 0;
		float nearestSeekDist = FLT_MAX;

		for (unsigned i = 0, ni = order.size(); i < ni; ++i)
		{
			unsigned idx = order[i];
			AIAssert(team.units[idx] >= 0 && team.units[idx] < m_state.m_units.size());
			SUnit& unit = m_state.m_units[team.units[idx]];
			unit.readability = false;
			unit.nearestSeek = false;
			
			if (unit.underFireTime > 0.0f)
				unit.underFireTime -= dt;
			if (unit.targetLookTime > 0.0f)
			{
				unit.targetLookTime -= dt;
				continue;
			}

			if (team.readabilityBlockTime > 0.0f)
				continue;

			if (!unit.obj->IsEnabled()) continue;
			if (unit.curAction == UA_UNAVAIL) continue;
			if (unit.obj->GetProxy()->IsCurrentBehaviorExclusive()) continue;

			CAIObject* pTarget = (CAIObject*)unit.obj->GetAttentionTarget();

			// If there is only one unit seeing the target, skip it to avoid funny situations.
			if (team.liveEnemyCount == 1 && unit.obj->GetAttentionTargetType() != AITARGET_VISUAL)
				continue;

			if (unit.obj->GetAttentionTargetType() != AITARGET_VISUAL &&
					unit.obj->GetAttentionTargetType() != AITARGET_MEMORY)
				continue;

			if (unit.obj->GetAlertness() != team.maxAlertness)
				continue;

			Vec3 dirUnitToTarget = unit.obj->GetPos() - pPlayer->GetPos();
			float distToTarget = dirUnitToTarget.NormalizeSafe();

			if (unit.curAction == UA_SEEKING)
			{
				if (distToTarget < nearestSeekDist)
				{
					pNearestSeek = &unit;
					nearestSeekDist = distToTarget;
				}
			}

			const float viewThr = cosf(DEG2RAD(65.0f/2.0f));
			if (dirUnitToTarget.Dot(playerView) < viewThr)
				continue;

			float w = unit.leader ? 0.5f : 1.0f;

/*			if ((targetType & AITGT_MEMORY) == 0)
				w *= 0.5f;*/

			if (unit.obj->GetAttentionTargetType() == AITARGET_VISUAL ||
					unit.obj->GetAttentionTargetType() == AITARGET_MEMORY)
			{
				if (distToTarget < 5.0f)
				{
					unit.targetLookTime = 2.0f;
					continue;
				}
				if (IEntity* pEnt = unit.obj->GetEntity())
				{
					AABB bounds;
					pEnt->GetWorldBounds(bounds);
					bounds.min -= Vec3(1.5f, 1.5f, 0.5f);
					bounds.max += Vec3(1.5f, 1.5f, 0.5f);

					Vec3 junk;
					if (Intersect::Lineseg_AABB(playerLOS, bounds, junk))
					{
						unit.targetLookTime = 4.0f;
						continue;
					}
				}
			}

			float d = Distance::Point_Point(unit.obj->GetPos(), playerPos) * w;
			if (d > 12.0f && d < nearestDist)
			{
				nearestDist = d;
				pNearest = &unit;
			}
		}

		if (pNearest)
			pNearest->readability = true;
		if (pNearestSeek)
			pNearestSeek->nearestSeek = true;
	}
	else
	{
		for (unsigned i = 0, ni = order.size(); i < ni; ++i)
		{
			unsigned idx = order[i];
			AIAssert(team.units[idx] >= 0 && team.units[idx] < m_state.m_units.size());
			SUnit& unit = m_state.m_units[team.units[idx]];

			unit.readability = false;
			if (!unit.obj->IsEnabled()) continue;
			if (unit.curAction == UA_UNAVAIL) continue;
			if (unit.obj->GetProxy()->IsCurrentBehaviorExclusive()) continue;

			unit.nearestSeek = true;
		}
	}

}

//====================================================================
// Reset
//====================================================================
void CHumanGroupTactic::Reset()
{
	m_state.Reset();
	m_state.m_avoidPoints.clear();
	DeleteTeams();
}

//====================================================================
// FindUnit
//====================================================================
CHumanGroupTactic::UnitList::iterator CHumanGroupTactic::FindUnit(CPuppet* puppet)
{
	UnitList::iterator it = m_state.m_units.begin();
	for(; it != m_state.m_units.end(); ++it)
		if((*it).obj == puppet)
			break;
	return it;
}

//====================================================================
// CheckAndAddUnit
//====================================================================
void CHumanGroupTactic::CheckAndAddUnit(IAIObject* object, int type)
{
	CPuppet* pPuppet = CastToCPuppetSafe(object);
	if(!pPuppet)
		return;

	UnitList::iterator it = FindUnit(pPuppet);
	if(it == m_state.m_units.end())
	{
		// Add new unit.
		bool	leader(false);
		if(type == GU_HUMAN_LEADER)
		{
			leader = true;
			type = GU_HUMAN_COVER;
		}
		m_state.m_units.push_back(SUnit(pPuppet, type, leader));
	}
	else
	{
		SUnit&	unit(*it);
		// Update type and reset unit.
		unit.Reset();
		if(type == GU_HUMAN_LEADER)
		{
			unit.leader = true;
			unit.type = GU_HUMAN_COVER;
		}
		else
			unit.type = type;
	}

	AILogEvent("CHumanGroupTactic::CheckAndAddUnit #%d m_state.m_units=", m_pGroup->GetGroupId());
	for (unsigned i = 0, ni = m_state.m_units.size(); i < ni; ++i)
		AILogEvent(" - %s", m_state.m_units[i].obj->GetName());

	DeleteTeams();
}

//====================================================================
// CheckAndRemoveUnit
//====================================================================
void CHumanGroupTactic::CheckAndRemoveUnit(IAIObject* object)
{
	CPuppet* pPuppet = CastToCPuppetSafe(object);
	if(!pPuppet)
		return;

	for (unsigned i = 0, ni = m_teams.size(); i < ni; ++i)
	{
		if (m_teams[i]->pLeader == pPuppet)
			m_teams[i]->pLeader = 0;
	}

	UnitList::iterator it = FindUnit(pPuppet);
	if(it != m_state.m_units.end())
	{
		m_state.m_units.erase(it);
		for (unsigned i = 0, ni = m_state.m_units.size(); i <ni; ++i)
			m_state.m_units[i].nearestUnit = -1;
		DeleteTeams();
	}

	AILogEvent("CHumanGroupTactic::CheckAndRemoveUnit #%d m_state.m_units=", m_pGroup->GetGroupId());
	for (unsigned i = 0, ni = m_state.m_units.size(); i < ni; ++i)
		AILogEvent(" - %s", m_state.m_units[i].obj->GetName());

}

//====================================================================
// IsHideSpotCollidable
//====================================================================
static bool IsHideSpotCollidable(const SHideSpot* hs)
{
	if (hs->type == SHideSpot::HST_WAYPOINT || hs->type == SHideSpot::HST_VOLUME ||hs->type == SHideSpot::HST_DYNAMIC)
		return true;
	if (hs->pObstacle && hs->pObstacle->bCollidable)
		return true;
	if (hs->type == SHideSpot::HST_ANCHOR && hs->pAnchorObject && hs->pAnchorObject->GetType() == AIANCHOR_COMBAT_HIDESPOT)
		return true;
	return false;
}

//====================================================================
// UpdateLeaderTypeMimicry
//====================================================================
void CHumanGroupTactic::UpdateLeaderTypeMimicry(SUnit& unit)
{
	unsigned coverCount = 0;
	unsigned sneakerCount = 0;
	unsigned camperCount = 0;

	for (UnitList::iterator it = m_state.m_units.begin(); it != m_state.m_units.end(); ++it)
	{
		SUnit&	other(*it);
		switch(other.type)
		{
		case GU_HUMAN_COVER: coverCount++; break;
		case GU_HUMAN_SNEAKER: sneakerCount++; break;
		case GU_HUMAN_SNEAKER_SPECOP: sneakerCount++; break;
		case GU_HUMAN_CAMPER: camperCount++; break;
		}
	}

	// Prefer to be cover.
	if(camperCount> 0)
	{
		unit.type = GU_HUMAN_CAMPER;
	}
	else if(coverCount > 0)
	{
		unit.type = GU_HUMAN_COVER;
	}
	else if(sneakerCount > 0)
	{
		unit.type = GU_HUMAN_SNEAKER;
	}
	else
	{
		unit.type = GU_HUMAN_COVER;
	}
}

//====================================================================
// HasPointInRange
//====================================================================
bool CHumanGroupTactic::HasPointInRange(const Vec3& pos, float range, const std::vector<Vec3>& points)
{
	const float rangeSqr = sqr(range);
	for(std::vector<Vec3>::const_iterator ptIt = points.begin(); ptIt != points.end(); ++ ptIt)
	{
		if(Distance::Point_Point2DSq((*ptIt), pos) < rangeSqr)
			return true;
	}
	return false;
}

//====================================================================
// AddAvoidPoint
//====================================================================
void CHumanGroupTactic::AddAvoidPoint(const Vec3& pos, float r, float t)
{
	AvoidPointList::iterator it = m_state.m_avoidPoints.begin(), end = m_state.m_avoidPoints.end();
	for (; it != end; ++it)
	{
		if(Distance::Point_Point2DSq(it->pos, pos) < sqr(it->rad))
		{
			it->pos = (it->pos + pos) / 2;
			it->rad = max(it->rad, r);
			it->time = max(it->time, t);
			break;
		}
	}
	if(it == end)
		m_state.m_avoidPoints.push_back(SAvoidPoint(pos, r, t));
}

//====================================================================
// UpdateAvoidPoints
//====================================================================
void CHumanGroupTactic::UpdateAvoidPoints(float dt)
{
	AvoidPointList::iterator it = m_state.m_avoidPoints.begin();
	while(it != m_state.m_avoidPoints.end())
	{
		it->time -= dt;
		if(it->time < 0.0f)
			it = m_state.m_avoidPoints.erase(it);
		else
			++it;
	}
}

//====================================================================
// ShouldAvoidPoint
//====================================================================
bool CHumanGroupTactic::ShouldAvoidPoint(const Vec3& pos)
{
	for (AvoidPointList::iterator it = m_state.m_avoidPoints.begin(), end = m_state.m_avoidPoints.end(); it != end; ++it)
	{
		if(Distance::Point_Point2DSq(it->pos, pos) < sqr(it->rad))
			return true;
	}
	return false;
}

//====================================================================
// GetUnitNavType
//====================================================================
IAISystem::ENavigationType CHumanGroupTactic::GetUnitNavType(const SUnit& unit)
{
	CAISystem* pAISystem(GetAISystem());
	int nbid; IVisArea *iva;
	IAISystem::tNavCapMask	mask(unit.obj->GetMovementAbility().pathfindingProperties.navCapMask);
	return pAISystem->CheckNavigationType(unit.obj->GetPos(), nbid, iva, mask);
}

//====================================================================
// GetEnemyNavType
//====================================================================
IAISystem::ENavigationType CHumanGroupTactic::GetEnemyNavType(const SUnit& unit)
{
	CAISystem* pAISystem(GetAISystem());
	if(unit.obj->GetAttentionTarget())
	{
		int nbid; IVisArea *iva;
		IAISystem::tNavCapMask	mask(unit.obj->GetMovementAbility().pathfindingProperties.navCapMask);
		return pAISystem->CheckNavigationType(unit.obj->GetAttentionTarget()->GetPos(), nbid, iva, mask);
	}
	return IAISystem::NAV_UNSET;
}

//====================================================================
// GetUnitAttackRange
//====================================================================
float CHumanGroupTactic::GetUnitAttackRange(const SUnit& unit)
{
	IAISystem::ENavigationType	type;
	float	range = unit.obj->GetParameters().m_fPreferredCombatDistance;
	// Allow to come closer if the target is indoors.
	type = GetEnemyNavType(unit);
	if(type != IAISystem::NAV_UNSET && type != IAISystem::NAV_TRIANGULAR)
		range *= 0.75f;
	// And even closer if both are indoors.
/*	type = GetUnitNavType(unit);
	if(type != IAISystem::NAV_UNSET && type != IAISystem::NAV_TRIANGULAR)
		range /= 2.0f;*/
	return range;
}

//====================================================================
// GetState
//====================================================================
int CHumanGroupTactic::GetState(IAIObject* pRequester, int type, float param)
{
	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);
	if(!pPuppet)
		return 0;

	UnitList::iterator it = FindUnit(pPuppet);
	if(it == m_state.m_units.end())
		return 0;
	SUnit&	unit = (*it);

	if(type == GE_GROUP_STATE)
	{
		SAIGroupTeam* pTeam = GetTeam(unit);
		if (pTeam)
			return pTeam->eval;
		return GS_IDLE;
	}
	else if(type == GE_ADVANCE_POS)
	{
		// Check if the current advance point is ok.
		return 0; //unit.shouldAdvance ? 1 : 0;
	}
	else if(type == GE_LEADER_COUNT)
	{
//		if (GetAISystem()->m_cvProtoGroupTactic->GetIVal() > 0)
		return 2;
//		return (m_state.m_leaderCount && m_state.m_enabledUnits > 1) ? 1 : 0;
//		return m_state.m_enabledUnits > 1 ? 1 : 0;
//		return m_state.m_leaderCount;
	}
	else if(type == GE_UNIT_STATE)
	{
		switch(unit.curAction)
		{
		case UA_ADVANCING: return GN_NOTIFY_ADVANCING;
		case UA_COVERING: return GN_NOTIFY_COVERING;
		case UA_WEAK_COVERING: return GN_NOTIFY_WEAK_COVERING;
		case UA_HIDING: return GN_NOTIFY_HIDING;
		case UA_SEEKING: return GN_NOTIFY_SEEKING;
		case UA_IDLE: return GN_NOTIFY_IDLE;
		case UA_UNAVAIL: return GN_NOTIFY_UNAVAIL;
		case UA_SEARCHING: return GN_NOTIFY_SEARCHING;
		case UA_ALERTED: return GN_NOTIFY_ALERTED;
		default: return GN_NOTIFY_IDLE;
		}
	}
	else if (type == GE_MOST_LOST_UNIT)
	{
//		if (unit.mostLost && unit.nearestUnit != -1 && unit.nearestUnitDist > COHESION_DIST)
		if (unit.doCohesion)
			return 1;
	}
	else if (type == GE_MOVEMENT_SIGNAL)
	{
		if (unit.readability)
		{
			SAIGroupTeam* pTeam = GetTeam(unit);
			if (pTeam)
				pTeam->readabilityBlockTime = 8.0f;

			Vec3 dirToRefpoint = unit.obj->GetRefPoint()->GetPos() - unit.obj->GetPos();
			const Vec3& forw = unit.obj->GetBodyDir();
			Vec3 right(forw.y, -forw.x, 0);
			return right.Dot(dirToRefpoint) < 0.0f ? -1 : 1;
		}
		return 0;
	}
	else if (type == GE_NEAREST_SEEK)
	{
		return unit.nearestSeek ? 1 : 0;
	}
	else if (type == GE_DEFEND_POS)
	{
		SAIGroupTeam* pTeam = GetTeam(unit);
		if (pTeam)
			return pTeam->defendPositionSet ? 1 : 0;
		return 0;
	}

	return 0;
}

//====================================================================
// NotifyState
//====================================================================
CHumanGroupTactic::SAIGroupTeam* CHumanGroupTactic::GetTeam(SUnit& unit)
{
	if (unit.team == -1)
		return 0;

#ifdef _DEBUG
	// Make sure the 
	AIAssert(unit.team >= 0 && unit.team < m_teams.size());
	SAIGroupTeam* pCurTeam = m_teams[unit.team];
	bool found = false;
	for (unsigned j = 0, nj = pCurTeam->units.size(); j < nj; ++j)
	{
		if (unit.obj ==  m_state.m_units[pCurTeam->units[j]].obj)
		{
			found = true;
			break;
		}
	}
	AIAssert(found);
#endif

	return m_teams[unit.team];
}

//====================================================================
// NotifyState
//====================================================================
void CHumanGroupTactic::NotifyState(IAIObject* pRequester, int type, float param)
{
	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);
	if(!pPuppet)
		return;

	UnitList::iterator it = FindUnit(pPuppet);
	if (it == m_state.m_units.end())
	{
		// This can happen if the character gets changed from non-group to group.
//		AIAssert(0);
		int role = GetTeamRole(pPuppet);
		if (role == -1)
		{
			AILogEvent("CHumanGroupTactic::NotifyState - %s Unit not found role=-1", pPuppet->GetName());
			return;
		}
		CheckAndAddUnit(pRequester, role);
		it = FindUnit(pPuppet);

		//		return;
	}
	SUnit&	unit = (*it);


	if (type == GN_INIT)
	{
		int role = GetTeamRole(pPuppet);
		if (role != -1 && role != unit.type)
			CheckAndAddUnit(pRequester, role);
		AILogEvent("CHumanGroupTactic::NotifyState - %s GN_INIT role=%d", pPuppet->GetName(), role);
		return;
	}


	if (type == GN_AVOID_CURRENT_POS)
	{
//		unit.advancePosValid = false;
		int nbid;
		IVisArea *iva;
		CAISystem*	pAISystem(GetAISystem());
		IAISystem::ENavigationType	navType = pAISystem->CheckNavigationType(unit.obj->GetPos(), nbid, iva, unit.obj->GetMovementAbility().pathfindingProperties.navCapMask);
		float	rad = 4.0f;
		if(navType == IAISystem::NAV_WAYPOINT_HUMAN)
			rad = 2.0f;
		AddAvoidPoint(unit.obj->GetPos(), rad, param);
		unit.underFireTime = 2.0f;
	}
	else if (type == GN_MARK_DEFEND_POS)
	{
		SAIGroupTeam* pTeam = GetTeam(unit);
		if (pTeam)
		{
			pTeam->defendPosition = unit.obj->GetRefPoint()->GetPos();
			pTeam->defendPositionSet = !pTeam->defendPosition.IsZero();
		}
	}
	else if (type == GN_CLEAR_DEFEND_POS)
	{
		SAIGroupTeam* pTeam = GetTeam(unit);
		if (pTeam)
			pTeam->defendPositionSet = false;
	}
	else if (type == GN_NOTIFY_ADVANCING)
	{
		unit.curAction = UA_ADVANCING;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_COVERING)
	{
		unit.curAction = UA_COVERING;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_WEAK_COVERING)
	{
		unit.curAction = UA_WEAK_COVERING;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_HIDING)
	{
		unit.curAction = UA_HIDING;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_SEEKING)
	{
		unit.curAction = UA_SEEKING;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_IDLE)
	{
		unit.curAction = UA_IDLE;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_UNAVAIL)
	{
		unit.curAction = UA_UNAVAIL;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_SEARCHING)
	{
		unit.curAction = UA_SEARCHING;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_ALERTED)
	{
		unit.curAction = UA_ALERTED;
		unit.curActionTime = 0.0f;
	}
	else if (type == GN_NOTIFY_REINFORCE)
	{
		unit.curAction = UA_REINFORCING;
		unit.curActionTime = 0.0f;
	}

	

	if (unit.curAction != UA_UNAVAIL)
	{
		if (unit.team == -1 || m_teams.empty())
			SetupTeams();

		SAIGroupTeam* pTeam = GetTeam(unit);
		if (pTeam)
		{
			UpdateTeamStats(*pTeam, 0.0f);
			UpdateAttackDirection(*pTeam, unit);
		}
		else
		{
			AIWarning("CHumanGroupTactic::NotifyState no team for %s ", pRequester->GetName());
			AIAssert(1);
		}

	}
}

//====================================================================
// GetPoint
//====================================================================
Vec3 CHumanGroupTactic::GetPoint(IAIObject* pRequester, int type, float param)
{
	CAISystem* pAISystem(GetAISystem());

	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);
	if(!pPuppet)
		return ZERO;

	UnitList::iterator it = FindUnit(pPuppet);
	if(it == m_state.m_units.end())
		return ZERO;
	SUnit&	unit = (*it);

	if (type == GE_DEFEND_POS)
	{
		SAIGroupTeam* pTeam = GetTeam(unit);
		if (pTeam)
		{
			if (pTeam->defendPositionSet)
				return pTeam->defendPosition;
			else
			{
				if (pTeam->unitTypes == GU_HUMAN_SNEAKER ||
						pTeam->unitTypes == GU_HUMAN_SNEAKER_SPECOP ||
						pTeam->unitTypes == GU_HUMAN_COVER)
				{
					if (unit.obj->GetAttentionTarget())
						return unit.obj->GetAttentionTarget()->GetPos();
					else
						return pTeam->avgEnemyPos;
				}
				else
				{
					if (pTeam->initialGroupCenterSet)
						return pTeam->initialGroupCenter;
					else
						return pTeam->avgTeamPos;
				}
			}
		}
	}

/*	if(type == GE_ADVANCE_POS)
	{
		// Use current position if it is good.
		if(!unit.shouldAdvance)
			return unit.advancePos;

		// Get new advance position.
		float	aggression = unit.obj->GetParameters().m_fAggression;
		float	advanceDist = 8.0f;
		if(unit.type == GU_HUMAN_CAMPER)
			advanceDist = 5.0f;
		else if(unit.type == GU_HUMAN_COVER)
			advanceDist = 6.0f;
		else if(unit.type == GU_HUMAN_SNEAKER)
			advanceDist = 8.0f;
		if(GetUnitNavType(unit) == IAISystem::NAV_WAYPOINT_HUMAN)
			advanceDist *= 0.7f;
		return GetAttackPoint(unit, GetUnitAttackRange(unit), advanceDist * (1.0f + aggression));
	}
	else if(type == GE_SEEK_POS)
	{
		return GetAttackPoint(unit, GetUnitAttackRange(unit)*0.5f, 8.0f, true);
	}*/

	return ZERO;
}


//====================================================================
// SUnit::Serialize
//====================================================================
void CHumanGroupTactic::SUnit::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	ser.BeginGroup("Unit");
	
	objectTracker.SerializeObjectPointer(ser, "obj", obj, false);
	ser.Value("type", type);
//	ser.Value("side", side);
	ser.EnumValue("curAction", curAction, UA_IDLE, UA_LAST);
	ser.Value("curActionTime", curActionTime);
	ser.Value("leader", leader);
	ser.Value("enabled", enabled);
	ser.Value("team", team);
	if (ser.IsReading())
	{
		nearestUnit = -1;
		nearestUnitDist = 0;
		mostLost = false;
		doCohesion = false;
		readability = false;
		nearestSeek = false;
		targetLookTime = 0.0f;
		underFireTime = 0.0f;
	}

	ser.EndGroup();
}

//====================================================================
// SAvoidPoint::Serialize
//====================================================================
void CHumanGroupTactic::SAvoidPoint::Serialize(TSerialize ser)
{
	ser.BeginGroup("AvoidPoint");
	ser.Value("pos", pos);
	ser.Value("rad", rad);
	ser.Value("time", time);
	ser.EndGroup();
}

//====================================================================
// SGroupState::Serialize
//====================================================================
void CHumanGroupTactic::SGroupState::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	ser.BeginGroup("GroupState");

	// Serialize base values.
	ser.Value("m_leaderCount", m_leaderCount);
	ser.Value("m_enabledUnits", m_enabledUnits);

	// Serialize units.
	int	unitCount = m_units.size();
	ser.Value("unitCount", unitCount);
	ser.BeginGroup("Units");
	if(ser.IsReading())
	{
		m_units.clear();
		for(int i = 0; i < unitCount; ++i)
		{
			m_units.push_back(SUnit());	// Create the unit first so that objectTracker can patch the pointers.
			m_units.back().Serialize(ser, objectTracker);
		}
	}
	else
	{
		UnitList::iterator end = m_units.end();
		for(UnitList::iterator it = m_units.begin(); it != end; ++it)
			it->Serialize(ser, objectTracker);
	}
	ser.EndGroup();

	// Serialize Avoid points
	ser.Value("AvoidPoints", m_avoidPoints);
/*	int	avoidPointCount = m_avoidPoints.size();
	ser.Value("avoidPointCount", avoidPointCount);
	ser.BeginGroup("AvoidPoints");
	if(ser.IsReading())
	{
		m_avoidPoints.clear();
		for(int i = 0; i < avoidPointCount; ++i)
		{
			m_avoidPoints.push_back(SAvoidPoint());	// Create the point first so that objectTracker can patch the pointers.
			m_avoidPoints.back().Serialize(ser, objectTracker);
		}
	}
	else
	{
		AvoidPointList::iterator end = m_avoidPoints.end();
		for(AvoidPointList::iterator it = m_avoidPoints.begin(); it != end; ++it)
			it->Serialize(ser, objectTracker);
	}
	ser.EndGroup();	// "AvoidPoints"*/

	ser.EndGroup();	// "GroupState"
}

//====================================================================
// SAIGroupTeam::Serialize
//====================================================================
void CHumanGroupTactic::SAIGroupTeam::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	ser.BeginGroup("Team");

	ser.Value("unitTypes", unitTypes);
	ser.Value("liveEnemyCount", liveEnemyCount);
	ser.Value("targetLostTime", targetLostTime);
	ser.EnumValue("eval", eval ,GS_IDLE, LAST_GS);
	ser.Value("alertedTime", alertedTime);
	ser.Value("enabledUnits", enabledUnits);
	ser.Value("availableUnits", availableUnits);
	ser.Value("advancingCount", advancingCount);
	ser.Value("targetCount", targetCount);
	ser.Value("minAlertness", minAlertness);
	ser.Value("maxAlertness", maxAlertness);
	ser.Value("needSeekCount", needSeekCount);
	ser.Value("searchCount", searchCount);
	ser.Value("reinforceCount", reinforceCount);
	ser.Value("units", units);
	ser.Value("avgTeamPos", avgTeamPos);
	ser.Value("avgTeamDir", avgTeamDir);
	ser.Value("avgTeamNorm", avgTeamNorm);
	ser.Value("avgEnemyPos", avgEnemyPos);
	ser.Value("initialGroupCenter", initialGroupCenter);
	ser.Value("initialGroupCenterSet", initialGroupCenterSet);
	ser.Value("defendPosition", defendPosition);
	ser.Value("defendPositionSet", defendPositionSet);
	ser.EnumValue("side", side, SIDE_UNDEF, SIDE_RIGHT);
	ser.Value("lastUpdateTime", lastUpdateTime);
	ser.Value("readabilityBlockTime", readabilityBlockTime);
	objectTracker.SerializeObjectPointer(ser, "pLeader", pLeader, false);

	ser.Value("attackPathId", attackPathId);
	ser.Value("lastNavNode", lastNavNode);

	ser.EnumValue("maxType", maxType, AITARGET_NONE, AITARGET_LAST);
	ser.EnumValue("maxThreat", maxThreat, AITHREAT_NONE, AITHREAT_LAST);

	ser.Value("flankFenceP", flankFenceP);
	ser.Value("flankFenceQ", flankFenceQ);

	if (ser.IsReading())
	{
		bounds.Reset();
		ClearVectorMemory(hull);
		labelPos.Set(0,0,0);
		covers.clear();
		ClearVectorMemory(attackPath);
		ClearVectorMemory(attackPathNodes);
		lastAttackPathTime = 0.0f;
		lastAttackPathFrom.Set(0,0,0);
		lastAttackPathTo.Set(0,0,0);
		attackPathTarget.Set(0,0,0);
		coversDirty = true;
	}

	if (ser.IsReading() && pGroupCenter)
	{
		GetAISystem()->RemoveDummyObject(pGroupCenter);
		pGroupCenter = 0;
	}
	objectTracker.SerializeObjectPointer(ser, "pGroupCenter", pGroupCenter, false);

	ser.EndGroup();	// "Team"
}

//====================================================================
// Serialize
//====================================================================
void CHumanGroupTactic::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	// Serialize the state
	m_state.Serialize(ser, objectTracker);

	// Serialize teams
	int	teamCount = m_teams.size();
	ser.Value("teamCount", teamCount);
	ser.BeginGroup("Teams");
	if(ser.IsReading())
	{
		if (!m_teams.empty())
		{
			for (unsigned i = 0; i < m_teams.size(); ++i)
				delete m_teams[i];
			m_teams.clear();
		}
		m_teams.resize(teamCount, 0);
		for (int i = 0; i < teamCount; ++i)
		{
			m_teams[i] = new SAIGroupTeam;
			m_teams[i]->Serialize(ser, objectTracker);
		}
	}
	else
	{
		for (int i = 0; i < teamCount; ++i)
			m_teams[i]->Serialize(ser, objectTracker);
	}
	ser.EndGroup(); // "Teams"
}

//====================================================================
// GetEvaluationText
//====================================================================
const char* CHumanGroupTactic::GetEvaluationText(EGroupState eval)
{
	switch(eval)
	{
	case GS_IDLE: return "Idle"; break;
	case GS_ALERTED: return "Alerted"; break;
	case GS_COVER: return "Cover"; break;
	case GS_ADVANCE: return "Advance"; break;
	case GS_SEEK: return "Seek"; break;
	case GS_SEARCH: return "Search"; break;
	case GS_REINFORCE: return "Reinforce"; break;
	}

	return 0;
}

//====================================================================
// DebugDraw
//====================================================================
void CHumanGroupTactic::DebugDraw(IRenderer* pRend)
{
	if(m_state.m_units.empty() || m_state.m_enabledUnits == 0)
		return;

	SAuxGeomRenderFlags	oldFlags = pRend->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags	newFlags(oldFlags);
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	newFlags.SetDepthWriteFlag(e_DepthWriteOff);
	pRend->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

	const ColorB	red(240,8,0);
	const ColorB	blue(8,64,230);
	const ColorB	redTrans(240,8,0,128);
	const ColorB	blueTrans(8,64,230,128);
	const ColorB	white(255,255,255);
	const ColorB	whiteTrans(255,255,255,128);
	const ColorB	black(0,0,0);
	const ColorB	blackTrans(0,0,0,128);

	const ColorB	colCover(0,240,0);
	const ColorB	colCamper(0,0,240);
	const ColorB	colSneaker(240,240,0);
	const ColorB	colLeader(8,50,200);
	const ColorB	colSneakerSpecop(0,80,128);


	const Vec3	offset(0,0,-1.0f);

	int debugMode = GetAISystem()->m_cvDrawGroupTactic->GetIVal();

	for (unsigned j = 0; j < m_teams.size(); ++j)
	{
		SAIGroupTeam& team = *m_teams[j];

		if (!team.availableUnits)
			continue;

		ColorB teamCol = blue;
		if (team.unitTypes == GU_HUMAN_COVER)
			teamCol = colCover;
		else if (team.unitTypes == GU_HUMAN_CAMPER)
			teamCol = colCamper;
		else if (team.unitTypes == GU_HUMAN_SNEAKER)
			teamCol = colSneaker;
		else if (team.unitTypes == GU_HUMAN_SNEAKER_SPECOP)
			teamCol = colSneakerSpecop;

		ColorB teamColTrans = teamCol;
		teamColTrans.a = 128;

		if (debugMode > 1)
		{
			DebugDrawRangePolygon(pRend, team.hull, 0.5f, teamColTrans, teamCol, true);
			DebugDrawArrow(pRend, team.avgTeamPos, team.avgTeamDir * 4.0f, 1.0f, teamColTrans);
			pRend->DrawLabel(team.labelPos + Vec3(0, 0, 3), 1.5f, "%s", GetEvaluationText(team.eval));
		}

//		Vec3 enemyFloor(team.avgEnemyPos);
//		GetFloorPos(enemyFloor, enemyFloor + Vec3(0,0,0.25f), walkabilityFloorUpDist, walkabilityFloorDownDist, walkabilityDownRadius, AICE_ALL);
//		pRend->GetIRenderAuxGeom()->DrawCylinder(enemyFloor+Vec3(0,0,0.2f), Vec3(0,0,1), 1.0f, 0.1f, ColorB(255,0,0,128));


		if (team.defendPositionSet)
		{
			if (debugMode > 1)
				pRend->DrawLabel(team.defendPosition + Vec3(0, 0, 1), 1.5f, "DEFEND");
			pRend->GetIRenderAuxGeom()->DrawCylinder(team.defendPosition, Vec3(0,0,1), 0.3f, 1.0f, teamColTrans);
		}

		if (team.side == SIDE_LEFT)
		{
			pRend->GetIRenderAuxGeom()->DrawSphere(team.avgTeamPos+Vec3(0,0,2), 0.5f, ColorB(255,255,255));
			pRend->GetIRenderAuxGeom()->DrawLine(team.avgTeamPos+Vec3(0,0,2), ColorB(255,255,255),
				team.avgTeamPos+Vec3(0,0,2)-team.avgTeamNorm*5.0f, ColorB(255,255,255));
			pRend->DrawLabel(team.avgTeamPos+Vec3(0,0,2), 1.5f, "LEFT");
		}
		else if (team.side == SIDE_RIGHT)
		{
			pRend->GetIRenderAuxGeom()->DrawSphere(team.avgTeamPos+Vec3(0,0,2), 0.5f, ColorB(255,255,255));
			pRend->GetIRenderAuxGeom()->DrawLine(team.avgTeamPos+Vec3(0,0,2), ColorB(255,255,255),
				team.avgTeamPos+Vec3(0,0,2)+team.avgTeamNorm*5.0f, ColorB(255,255,255));
			pRend->DrawLabel(team.avgTeamPos+Vec3(0,0,2), 1.5f, "RIGHT");
		}
/*		else
		{
			pRend->GetIRenderAuxGeom()->DrawSphere(team.avgTeamPos+Vec3(0,0,2), 0.5f, ColorB(255,255,255));
			pRend->DrawLabel(team.avgTeamPos+Vec3(0,0,2), 1.5f, "UNDEF");
		}*/

		// Attack path.
//		pRend->GetIRenderAuxGeom()->DrawCone(team.lastAttackPathFrom+Vec3(0,0,1), Vec3(0,0,-1), 0.25f, 1.0f, ColorB(0,64,255));
//		pRend->GetIRenderAuxGeom()->DrawCone(team.lastAttackPathTo+Vec3(0,0,1), Vec3(0,0,-1), 0.25f, 1.0f, ColorB(64,255,0));
//		pRend->DrawLabel(team.lastAttackPathTo + Vec3(0, 0, 1), 1.5f, "TGT\n%f", team.lastAttackPathTime);
		for (unsigned i = 0, ni = team.attackPath.size(); i < ni; ++i)
			pRend->GetIRenderAuxGeom()->DrawSphere(team.attackPath[i]+Vec3(0,0,0.5f), 0.05f, ColorB(0,196,255,196));
		if (team.attackPath.size() > 1)
		{
/*			pRend->GetIRenderAuxGeom()->DrawLine(team.lastAttackPathFrom+Vec3(0,0,0.5f), ColorB(0,196,255,196),
				team.attackPath.front()+Vec3(0,0,0.5f), ColorB(0,196,255,196), 3.0f);*/

			for (unsigned i = 0, ni = team.attackPath.size()-1; i < ni; ++i)
				pRend->GetIRenderAuxGeom()->DrawLine(team.attackPath[i]+Vec3(0,0,0.25f), ColorB(0,196,255,196),
					team.attackPath[i+1]+Vec3(0,0,0.25f), ColorB(0,196,255,196), 1.0f);

/*			pRend->GetIRenderAuxGeom()->DrawLine(team.attackPath.back()+Vec3(0,0,0.5f), ColorB(0,196,255,196),
				team.lastAttackPathFrom+Vec3(0,0,0.5f), ColorB(0,196,255,196), 3.0f);*/
		}

//		pRend->GetIRenderAuxGeom()->DrawLine(team.lastAttackPathTo+Vec3(0,0,0.5f), ColorB(255,255,255),
//			team.attackPathTarget, ColorB(255,255,255), 3.0f);

		// Attack covers
		pRend->GetIRenderAuxGeom()->DrawCone(team.attackPathTarget+Vec3(0,0,2), Vec3(0,0,-1), 0.5f, 1.4f, ColorB(255,64,0));
		DebugDrawCircleOutline(pRend, team.attackPathTarget+Vec3(0,0,1), 17.0f, ColorB(255,255,255));
/*		for (unsigned i = 0, ni = team.covers.size(); i < ni; ++i)
		{
			ColorB col(255,255,255);
			if (team.covers[i].status == CHECK_UNSET)
				col = ColorB(128,128,128);
			else if (team.covers[i].status == CHECK_INVALID)
				col = ColorB(0,0,0,196);
			pRend->GetIRenderAuxGeom()->DrawCylinder(team.covers[i].pos, Vec3(0,0,1), 0.25f, 0.5f, col);
		}*/

		// Flank fence
		if ((team.unitTypes == GU_HUMAN_SNEAKER || team.unitTypes == GU_HUMAN_SNEAKER_SPECOP) && !team.defendPositionSet)
			DebugDrawCapsuleOutline(pRend, team.flankFenceP, team.flankFenceQ, 2.0f, ColorB(255,0,0));

		for (unsigned i = 0, ni = team.units.size(); i < ni; ++i)
		{
			AIAssert(team.units[i] >= 0 && team.units[i] < m_state.m_units.size());
			SUnit& unit = m_state.m_units[team.units[i]];
			if (!unit.obj->IsEnabled()) continue;
			if (unit.obj->GetProxy()->IsCurrentBehaviorExclusive()) continue;
			if (unit.curAction == UA_UNAVAIL) continue;

			const Vec3& unitPos = unit.obj->GetPos();
			const Vec3& refPointPos = unit.obj->GetRefPoint()->GetPos();
			Vec3 dir = refPointPos - unitPos;
			dir.z = 0;
			dir.NormalizeSafe();

			string text;

			if (unit.doCohesion)
				text += "LOST/COHESION\n";
//				pRend->DrawLabel(unitPos + Vec3(0, 0, 2), 1.5f, "LOST/COHESION");
			else if (unit.mostLost)
				text += "LOST\n";
//				pRend->DrawLabel(unitPos + Vec3(0, 0, 2), 1.5f, "LOST");

			if (unit.readability)
			{
				if (team.readabilityBlockTime > 0.0f)
					text += "Signal\n";
//					pRend->DrawLabel(unitPos + Vec3(0, 0, 1), 1.0f, "Signal");
				else
					text += "Signal!\n";
//					pRend->DrawLabel(unitPos + Vec3(0, 0, 1), 1.5f, "Signal!");
			}
			if (unit.nearestSeek)
				text += "SEEK\n";

			if (debugMode > 1)
				pRend->DrawLabel(unitPos + Vec3(0, 0, 1), 1.5f, text.c_str());

//			DebugDrawArrow(pRend, unitPos, dir * 2.0f, 0.5f, teamCol);

			if (!refPointPos.IsZero())
			{
				pRend->GetIRenderAuxGeom()->DrawCylinder(refPointPos+Vec3(0,0,0.2f), Vec3(0,0,1), 0.3f, 0.3f, ColorB(255,0,0,128));
				pRend->GetIRenderAuxGeom()->DrawLine(refPointPos, teamColTrans, unitPos, teamCol);

				pRend->GetIRenderAuxGeom()->DrawCylinder(unit.advancePos+Vec3(0,0,0.2f), Vec3(0,0,1), 0.15f, 1.5f, ColorB(255,196,0));
				pRend->GetIRenderAuxGeom()->DrawLine(refPointPos, teamColTrans, unit.advancePos, teamCol);
			}

//			pRend->DrawLabel(team.labelPos + Vec3(0, 0, 3), 1.5f, "TEAM%d / #%d\nEnabled:%d (%d)\nAvail: %d\n%s %.1f\nPoints:%d",
//				j+1, m_pGroup->GetGroupId(), team.enabledUnits, team.liveEnemyCount, team.availableUnits, GetEvaluationText(team.eval), team.targetLostTime, team.searchPoints.size());
		}
	}

	if (debugMode > 1)
	{
		// Draw points to avoid.
		for(AvoidPointList::iterator it = m_state.m_avoidPoints.begin(), end = m_state.m_avoidPoints.end(); it != end; ++it)
		{
			int	a = (int)CLAMP((it->time / 10.0f) * 64.0f, 0, 64.0f);
			pRend->GetIRenderAuxGeom()->DrawSphere(it->pos, it->rad, ColorB(0,0,0,a));
		}
	}

	pRend->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
}


//====================================================================
// Validate
//====================================================================
void CHumanGroupTactic::Validate()
{
	int thisGroupId(m_pGroup->GetGroupId());
	UnitList::iterator it = m_state.m_units.begin();
	for(int count=0; it != m_state.m_units.end(); ++it, ++count)
	{
//		if(count==0)
//			thisGroupId = (*it).obj->m_Parameters.m_nGroup;
		if((*it).obj->m_Parameters.m_nGroup != thisGroupId)
		{
			AIWarning("### Invalid CHumanGroupTactic  " );
//			AIAssert(0);
			Dump();
			return;
		}
	}		
	return;
}

//====================================================================
// Dump
//====================================================================
void CHumanGroupTactic::Dump()
{
	AILogAlways("---  CHumanGroupTactic ID %d; size %d ", m_pGroup->GetGroupId(), m_state.m_units.size());
	UnitList::iterator it = m_state.m_units.begin();
	for(; it != m_state.m_units.end(); ++it)
	{
		AILogAlways(" %s %d ", (*it).obj->GetName(), (*it).obj->m_Parameters.m_nGroup);
	}		
	return;
}




//====================================================================
// CFollowerGroupTactic
// Simple follower group tactic.
//====================================================================
CFollowerGroupTactic::CFollowerGroupTactic(CAIGroup* pGroup) :
	m_pGroup(pGroup)
{
}

CFollowerGroupTactic::~CFollowerGroupTactic()
{
}

//====================================================================
// Update
//====================================================================
void CFollowerGroupTactic::Update(float dt)
{
	CAIActor* pTarget = CastToCAIActorSafe(GetAISystem()->GetPlayer());

	bool fired = false;
	if (pTarget && pTarget->GetProxy())
	{
		SAIWeaponInfo wi;
		pTarget->GetProxy()->QueryWeaponInfo(wi);
		float elapsed = (GetAISystem()->GetFrameStartTime() - wi.lastShotTime).GetSeconds();
		if (wi.isFiring || elapsed < 0.5f)
			fired = true;
	}

	for (unsigned i = 0, ni = m_units.size(); i < ni; ++i)
	{
		if (fired)
		{
			if (!m_units[i].targetFiredWeapon)
			{
				m_units[i].obj->SetSignal(0, "OnFollowTargetFired", m_units[i].obj->GetEntity(), 0);
				m_units[i].targetFiredWeapon = true;
				m_units[i].stayBackTime = 4.0f; // -= dt;
			}
		}
		UpdateUnit(m_units[i], pTarget, dt);
	}
}

//====================================================================
// UpdateUnit
//====================================================================
void CFollowerGroupTactic::UpdateUnit(SUnit& unit, CAIActor* pTarget, float dt)
{
	if (!unit.follow)
		return;

	CAIObject* pFollowTarget = unit.obj->GetOrCreateSpecialAIObject(CPipeUser::AISPECIAL_GROUP_TAC_POS);
	if (!pFollowTarget)
		return;
	pFollowTarget->SetAssociation(pTarget);

	static int FOLLOW_AREA = 801;
	static int FOLLOW_SPOT = 802;

	const Vec3& targetPos = pTarget->GetPos();

	unit.stayBackTime -= dt;

	// Check follow modifiers
	if (Distance::Point_PointSq(targetPos, unit.lastTargetCheckPos) > sqr(0.1f))
	{
		unit.lastTargetCheckPos = targetPos;

		const SShape* pOldShape = unit.pShape;

		if (!unit.pShape || (unit.pShape && !unit.pShape->IsPointInsideShape(targetPos, true)))
			unit.pShape = GetEnclosingGenericShapeOfType(targetPos, FOLLOW_AREA, unit.shapeName);

		if (!unit.pShape)
		{
			unit.shapeName.clear();
			if (unit.mimicMode != MIMIC_FOLLOW)
			{
				unit.formDist = 0;
				unit.formAngle = 0;
			}
			unit.mimicMode = MIMIC_FOLLOW;
		}
		else
		{
			if (pOldShape != unit.pShape || (unit.coverPaths.empty() && unit.covers.empty()))
			{
				IAISystem::tNavCapMask navCapMask = unit.obj->m_movementAbility.pathfindingProperties.navCapMask;

				unit.covers.clear();

				for (AIObjects::iterator ai = GetAISystem()->m_Objects.find(FOLLOW_SPOT), end = GetAISystem()->m_Objects.end(); ai != end && ai->first == FOLLOW_SPOT; ++ai )
				{
					CAIObject* pObject = ai->second;

					const Vec3& pos = pObject->GetPos();

					if (!unit.pShape->IsPointInsideShape(pos, true))
						continue;

					unit.covers.push_back(SCoverPos(pos, IAISystem::NAV_UNSET));
				}

				unit.coverPaths.clear();
				unit.coverPathNames.clear();

				for (ShapeMap::const_iterator it = GetAISystem()->GetDesignerPaths().begin(), end = GetAISystem()->GetDesignerPaths().end(); it != end; ++it)
				{
					const SShape&	path = it->second;
					// Skip paths which we cannot travel
					if ((path.navType & unit.obj->GetMovementAbility().pathfindingProperties.navCapMask) == 0)
						continue;
					// Skip wrong type
					if (path.type != FOLLOW_SPOT)
						continue;
					bool valid = false;
					for (ListPositions::const_iterator pit = path.shape.begin(), pend = path.shape.end(); pit != pend; ++pit)
					{
						if (unit.pShape->IsPointInsideShape(*pit, true))
						{
							valid = true;
							break;
						}
					}
					if (valid)
					{
						unit.coverPaths.push_back(&path);
						unit.coverPathNames.push_back(it->first);
					}
				}
			}

			unit.mimicMode = MIMIC_STATIC;
		}
	}

	if (unit.follow && unit.mimicMode == MIMIC_STATIC)
	{
		bool doReadability = false;
		if (unit.lookatPlayerTime > 0.0f)
		{
			unit.lookatPlayerTime -= dt;
			if (unit.lookatPlayerTime <= 0.0f)
				doReadability = true;
		}
		else
			unit.lookatPlayerTime = -1.0f;

		if (unit.movementTimeOut > 0.0f)
		{
			unit.movementTimeOut -= dt;
			if (unit.movementTimeOut <= 0.0f)
				doReadability = true;
		}
		else
			unit.movementTimeOut = -1.0f;

		if (unit.lastReadabilityTime > 0.0f)
		{
			unit.lastReadabilityTime -= dt;
			if (unit.lastReadabilityTime <= 0.0f)
			{
				// Do the readability again if not moving.
				if (unit.movementTimeOut < 0.0f)
					doReadability = true;
			}
		}
		else
			unit.lastReadabilityTime = -1.0f;

		if (doReadability && unit.lastReadabilityTime < 0.0f)
		{
			unit.lastReadabilityTime = 8.0f+ai_frand()*4.0f;
			IEntity* pEntity = unit.obj->GetEntity();
			IEntity* pNullEntity = NULL;
			GetAISystem()->SmartObjectEvent("OnFollowerStopped", pEntity, pNullEntity);
		}
	}
	else
	{
		unit.lookatPlayerTime = -1.0f;
	}

	if (unit.mimicMode == MIMIC_FOLLOW)
		unit.mimicPos = targetPos;

	//		m_mimicPos = UpdateFollowCoverPos(pPuppet, pTargetActor, pOperand->m_fTimePassed);
	else if (unit.mimicMode == MIMIC_STATIC)
		unit.mimicPos = UpdateStaticPos(unit, pTarget, dt);

	pFollowTarget->SetPos(unit.mimicPos);

	UpdateLookat(unit, pTarget, dt);

//	UpdateFiring(pPuppet, pOperand->m_fTimePassed);
//	UpdateStance(pPuppet, pTargetActor, pOperand->m_fTimePassed);
}

inline float WrapAngle(float a)
{
	if (a < -gf_PI) a += gf_PI*2;
	if (a > gf_PI) a -= gf_PI*2;
	return a;
}

//===================================================================
// ValidateWalkability
//===================================================================
void CFollowerGroupTactic::ValidateWalkability(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, const Vec3& posTo, Vec3& posOut)
{
	unsigned iter = 3;
	Vec3 delta = posTo - posFrom;

	posOut = posTo;
	IAISystem::ENavigationType navTypeFrom;
	if (GetAISystem()->IsSegmentValid(navCap, rad, posFrom, posOut, navTypeFrom))
		return;

	posOut = posFrom;

	float t = 0.5f;
	float dt = 0.25f;
	while (iter)
	{
		Vec3 p = posFrom + delta*t;
		if (GetAISystem()->IsSegmentValid(navCap, rad, posFrom, p, navTypeFrom))
		{
			posOut = p;
			t += dt;
		}
		else
		{
			t -= dt;
		}
		dt /= 2;
		--iter;
	}
}

//===================================================================
// UpdateFollowPos
//===================================================================
Vec3 CFollowerGroupTactic::UpdateFollowPos(SUnit& unit, CAIActor* pTargetActor, float dt)
{
	if (!pTargetActor)
		return unit.obj->GetPos();

	Vec3 targetPos = pTargetActor->GetPhysicsPos(); 

	Vec3 axisX, axisY;
	axisY = pTargetActor->GetBodyDir();
	axisY.z = 0;
	axisY.Normalize();
	axisX.Set(axisY.y, -axisY.x, 0);

	if (unit.formDist < 0.1f)
	{
		unit.formAxisX = axisX;
		unit.formAxisY = axisY;
	}

	Vec3 dirTarget = unit.formAxisY;

	const float maxMovementTimeOut = 8.0f;

	Vec3 vel = pTargetActor->GetVelocity();
	float dot = axisY.Dot(vel);
	if (dot < 0.0f)
		vel += -dot*axisY*2.0f;

	float speed = vel.GetLength();
	if (speed > 0.5f || unit.stayBackTime > 0.0f)
	{
		if (speed > 0.0001f)
			vel /= speed;
		dirTarget = vel;
		unit.lastTargetVel = dirTarget;
		unit.movementTimeOut = maxMovementTimeOut;

		unit.formAxisX = axisX;
		unit.formAxisY = axisY;
	}
	else
	{
		unit.movementTimeOut -= dt;
		if (unit.movementTimeOut < 0.0f) unit.movementTimeOut = 0.0f;
		if (unit.movementTimeOut > 0.0f)
		{
			//			float u = Clamp((unit.movementTimeOut + maxMovementTimeOut*0.5f)/(maxMovementTimeOut*0.5f), 0.0f, 1.0f);
			//			dirTarget = dirTarget + (unit.lastTargetVel - dirTarget) * u;
			dirTarget = unit.lastTargetVel;
		}
	}

	float leadAngle = atan2f(dirTarget.y, dirTarget.x);
	float sideAngle0 = DEG2RAD(50.0f);
	if (unit.stayBackTime > 0.0f)
		sideAngle0 = DEG2RAD(25.0f);


	//	const float leadingDistance = 4.0f;
	//	const float sideDistance = 2.5f;

	float leadDist = 2.5f; //leadingDistance;

	float threat = 0.0f;


	Vec3 backDir = -unit.formAxisY;
	leadAngle = atan2f(backDir.y, backDir.x);
	//	leadDist = sideDistance;

	threat = 1.0f;
	if (pTargetActor && pTargetActor->GetProxy())
	{
		SAIBodyInfo bi;
		pTargetActor->GetProxy()->QueryBodyInfo(bi);
		if (bi.stance == STANCE_CROUCH)
		{
			//			threat = 0.0f;
			sideAngle0 = DEG2RAD(-70.0f);
			leadDist *= 0.7f;
		}
	}
	//	if (unit.stayBackTime > 0.0f)
	//		threat = 0.0f;

	float sideAngle1 = gf_PI - sideAngle0;


	Vec3 sideDir0 = cosf(sideAngle0)*unit.formAxisX + sinf(sideAngle0)*unit.formAxisY;
	Vec3 sideDir1 = cosf(sideAngle1)*unit.formAxisX + sinf(sideAngle1)*unit.formAxisY;

	sideAngle0 = atan2f(sideDir0.y, sideDir0.x);
	sideAngle1 = atan2f(sideDir1.y, sideDir1.x);

	float da0, da1;

	da0 = WrapAngle(sideAngle0 - leadAngle);
	da1 = WrapAngle(sideAngle1 - leadAngle);

	float desiredAngle0 = leadAngle + threat * da0;
	float desiredAngle1 = leadAngle + threat * da1;
	float desiredDist = leadDist; // + (1-sqr(1-threat)) * (sideDistance - leadDist);

	//	static float formAngle = desiredAngle0;
	//	static float formDist = desiredDist;

	Vec3 delta = unit.obj->GetPos() - targetPos;
	float curAngle = atan2f(delta.y, delta.x); 

	if (unit.formDist < 0.1f)
	{
		unit.formAngle = curAngle;
	}
	else
	{
		da0 = WrapAngle(desiredAngle0 - unit.formAngle);
		da1 = WrapAngle(desiredAngle1 - unit.formAngle);

		IAISystem::tNavCapMask navCap = unit.obj->m_movementAbility.pathfindingProperties.navCapMask;
		const float rad = unit.obj->m_Parameters.m_fPassRadius;
		IAISystem::ENavigationType navTypeFrom;

		if (fabsf(da0) < fabsf(da1))
		{
			Vec3 pt = targetPos + Vec3(cosf(unit.formAngle+da0), sinf(unit.formAngle+da0), 0) * desiredDist;
			if (GetAISystem()->IsSegmentValid(navCap, rad, targetPos, pt, navTypeFrom))
				unit.formAngle += da0 * dt;
			else
				unit.formAngle += WrapAngle(leadAngle - unit.formAngle) * dt;
		}
		else
		{
			Vec3 pt = targetPos + Vec3(cosf(unit.formAngle+da1), sinf(unit.formAngle+da1), 0) * desiredDist;
			if (GetAISystem()->IsSegmentValid(navCap, rad, targetPos, pt, navTypeFrom))
				unit.formAngle += da1 * dt;
			else
				unit.formAngle += WrapAngle(leadAngle - unit.formAngle) * dt;
		}
		unit.formAngle = WrapAngle(unit.formAngle);
	}

	// Clamp the desired angle based on current 
	da0 = WrapAngle(unit.formAngle - curAngle);
	if (fabsf(da0) > gf_PI/4)
		unit.formAngle = curAngle + (da0 < 0.0f ? -gf_PI/4 : gf_PI/4);


	Vec3 desiredPos = targetPos + Vec3(cosf(unit.formAngle), sinf(unit.formAngle), 0) * desiredDist;
	Vec3 validPos(desiredPos);
	ValidateWalkability(unit.obj->m_movementAbility.pathfindingProperties.navCapMask, unit.obj->m_Parameters.m_fPassRadius,
		targetPos, desiredPos, validPos);
	desiredDist = Distance::Point_Point(targetPos, validPos);
	if (desiredDist < 1.2f)
		desiredDist = 1.2f;

	unit.formDist += (desiredDist - unit.formDist) * dt;
	if (desiredDist < unit.formDist)
		unit.formDist = desiredDist;

	unit.DEBUG_Center = targetPos;
	unit.DEBUG_SidePos0 = targetPos + Vec3(cosf(sideAngle0), sinf(sideAngle0), 0) * leadDist; //sideDistance;
	unit.DEBUG_SidePos1 = targetPos + Vec3(cosf(sideAngle1), sinf(sideAngle1), 0) * leadDist; //sideDistance;
	unit.DEBUG_LeadPos = targetPos + Vec3(cosf(leadAngle), sinf(leadAngle), 0) * leadDist;

	Vec3 retPos = targetPos + Vec3(cosf(unit.formAngle) * unit.formDist, sinf(unit.formAngle) * unit.formDist, 0.0f);
	retPos.z = validPos.z;

	/*	int stance = STANCE_STAND;
	if (Distance::Point_Point(retPos, unit.obj->GetPhysicsPos()) < 1.5f)
	{
	if (pTargetActor && pTargetActor->GetProxy())
	{
	SAIBodyInfo bi;
	pTargetActor->GetProxy()->QueryBodyInfo(bi);
	if (bi.stance == STANCE_CROUCH)
	stance = STANCE_CROUCH;
	}
	}
	unit.obj->unit.State.bodystate = stance;*/

	return retPos;
}

//===================================================================
// CollectCoverPos
//===================================================================
bool CFollowerGroupTactic::CollectCoverPos(SUnit& unit, const Vec3& reqPos, const Vec3& targetPos, float searchRange)
{
	IAISystem::tNavCapMask navCapMask = unit.obj->m_movementAbility.pathfindingProperties.navCapMask;
	float passRadius = unit.obj->m_Parameters.m_fPassRadius;

	MultimapRangeHideSpots hidespots;
	MapConstNodesDistance traversedNodes;

	GetAISystem()->GetHideSpotsInRange(hidespots, traversedNodes, reqPos, searchRange, navCapMask, 0.0f, false);

	CGraph* pGraph = GetAISystem()->GetGraph();
	//	pGraph->GetNodesInRange(traversedNodes, reqPos, searchRange, navCapMask, passRadius, 0, true, pOperand);

	if (hidespots.empty() && traversedNodes.empty())
		//	if (traversedNodes.empty())
		return false;

	const float offsetFromCover = 1.5f;

	unit.covers.clear();

	// Build list of obstacle cover segments.
	for (MultimapRangeHideSpots::iterator it = hidespots.begin(); it != hidespots.end(); ++it)
	{
		float distance = it->first;
		const SHideSpot &hs = it->second;

		IAISystem::ENavigationType navType = IAISystem::NAV_UNSET;
		if (hs.type == SHideSpot::HST_TRIANGULAR)
			navType = IAISystem::NAV_TRIANGULAR;
		else if (hs.type == SHideSpot::HST_WAYPOINT)
			navType = IAISystem::NAV_WAYPOINT_HUMAN;

		if (hs.pObstacle && hs.pObstacle->fApproxRadius > 0.01f)
		{
			Vec3	dir;
			Vec3	pos = hs.pos;

			GetAISystem()->AdjustOmniDirectionalCoverPosition(pos, dir, max(hs.pObstacle->fApproxRadius, 0.0f), 0.4f, targetPos);
			unit.covers.push_back(SCoverPos(pos, navType));

			if (hs.pObstacle && !hs.pObstacle->bCollidable)
			{
				// On soft cover, try points at the side of the object, choose the visible one.
				//			odDir = enemyPos - hs.pObstacle->vPos;
				// Use sides for soft cover
				Vec3	norm(dir.y, -dir.x, 0.0f);
				norm.NormalizeSafe();

				float	rad = max(0.1f, hs.pObstacle->fApproxRadius);

				pos = hs.pos + norm * (rad + 1.0f);
				unit.covers.push_back(SCoverPos(pos, navType));

				pos = hs.pos - norm * (rad + 1.0f);
				unit.covers.push_back(SCoverPos(pos, navType));
			}

		}
		else
		{
			unit.covers.push_back(SCoverPos(hs.pos, navType));
		}

		/*		if (hs.pNavNodes)
		{
		for (std::vector<const GraphNode *>::const_iterator itn = hs.pNavNodes->begin(); itn != hs.pNavNodes->end(); ++itn)
		{
		const GraphNode* node = *itn;
		if (!node) continue;

		Vec3 dir = node->GetPos() - hs.pos;
		float	len = dir.GetLengthSquared();
		if (len > sqr(offsetFromCover))
		{
		Vec3	pos = hs.pos + dir * offsetFromCover/sqrtf(len);
		unit.covers.push_back(SCoverPos(pos, navType));
		}
		}
		}
		if (hs.pNavNode)
		{
		Vec3 dir = hs.pNavNode->GetPos() - hs.pos;
		float	len = dir.GetLength();
		if (len > offsetFromCover)
		{
		Vec3	pos = hs.pos + dir * offsetFromCover/len;
		unit.covers.push_back(SCoverPos(pos, navType));
		}
		}*/
	}

	VectorSet<int> touchedVertices;

	if (navCapMask & IAISystem::NAV_TRIANGULAR)
	{
		CVertexList& vertList = GetAISystem()->m_VertexList;

		for (MapConstNodesDistance::iterator it = traversedNodes.begin(); it != traversedNodes.end(); ++it)
		{
			const GraphNode* node = it->first;
			if (node->navType != IAISystem::NAV_TRIANGULAR)
				continue;

			const STriangularNavData* pTriData = node->GetTriangularNavData();
			for (unsigned i = 0; i < pTriData->vertices.size(); ++i)
				touchedVertices.insert(pTriData->vertices[i]);

			const Vec3& p = node->GetPos();

			for (unsigned link = node->firstLinkIndex; link; link = pGraph->GetLinkManager().GetNextLink(link))
			{
				float rad = pGraph->GetLinkManager().GetRadius(link);
				if (rad < 0.0f)
				{
					Vec3 c = pGraph->GetLinkManager().GetEdgeCenter(link);

					Vec3 v0 = vertList.GetVertex(node->GetTriangularNavData()->vertices[pGraph->GetLinkManager().GetStartIndex(link)]).vPos;
					Vec3 v1 = vertList.GetVertex(node->GetTriangularNavData()->vertices[pGraph->GetLinkManager().GetEndIndex(link)]).vPos;

					Vec3 dir = v1 - v0;
					Vec3 norm(dir.y, -dir.x, 0);
					norm.Normalize();
					if (norm.Dot(p - c) < 0.0f)
						norm = -norm;

					unit.covers.push_back(SCoverPos(c + norm*1.2f, IAISystem::NAV_TRIANGULAR));
				}
			}
		}
	}

	unit.avoidPos.clear();
	for (unsigned i = 0, ni = touchedVertices.size(); i < ni; ++i)
	{
		const ObstacleData& obstacle = GetAISystem()->m_VertexList.GetVertex(touchedVertices[i]);
		if (obstacle.fApproxRadius > 0.01f)
			unit.avoidPos.push_back(SAvoidPos(obstacle.vPos, max(0.1f, obstacle.fApproxRadius)));
	}

	return true;
}

//===================================================================
// UpdateFollowCoverPos
//===================================================================
Vec3 CFollowerGroupTactic::UpdateFollowCoverPos(SUnit& unit, CAIActor* pTargetActor, float dt)
{
	IAISystem::tNavCapMask navCapMask = unit.obj->m_movementAbility.pathfindingProperties.navCapMask;
	float passRadius = unit.obj->m_Parameters.m_fPassRadius;
	const CPathObstacles& pathAdjustmentObstacles = unit.obj->GetPathAdjustmentObstacles();

	Vec3 enemyPos;

	Vec3 targetViewDir = pTargetActor->GetViewDir();
	targetViewDir.z = 0.0f;
	targetViewDir.Normalize();
	const Vec3& targetPos = pTargetActor->GetPos();

	if (unit.obj->GetAttentionTargetType() != AITARGET_VISUAL &&
			unit.obj->GetAttentionTargetType() == AITARGET_MEMORY &&
			unit.obj->GetAttentionTargetThreat() >= AITHREAT_THREATENING)
	{
		enemyPos = unit.obj->GetAttentionTarget()->GetPos();
	}
	else
	{
		enemyPos = targetPos + targetViewDir * 30.0f;
	}


	if (Distance::Point_PointSq(pTargetActor->GetPos(), unit.lastCoverCollectPos) > sqr(7.0f))
	{
		unit.lastCoverCollectPos = targetPos;
		CollectCoverPos(unit, unit.lastCoverCollectPos, enemyPos, 20.0f);
	}

	Vec3 targetDir = pTargetActor->GetViewDir();
	Lineseg targetLOS(targetPos, targetPos + targetViewDir*5.0f);

	const float targetAvoidRadius = 2.5f;

	float ta, tb;

	bool selectTarget = false;

	if (unit.selectCoverTriggerRad < 0.0f)
		selectTarget = true;
	//	else if (Distance::Point_Point2DSq(unit.selectCoverTriggerPos, targetPos) > sqr(unit.selectCoverTriggerRad))
	else if (Distance::Point_Lineseg2DSq(targetPos, Lineseg(unit.lastSelectedPos, unit.selectCoverTriggerPos), ta) > sqr(unit.selectCoverTriggerRad))
		selectTarget = true;
	else if (Distance::Point_Point2DSq(unit.lastSelectedPos, targetPos) < sqr(1.5f))
		selectTarget = true;
	else if (unit.firingTime > 0.0f && Distance::Point_Lineseg2DSq(unit.lastSelectedPos, targetLOS, ta) < sqr(1.0f))
		selectTarget = true;

	if (selectTarget)
	{
		const Vec3& opPos = unit.obj->GetPos();

		float bestDist = FLT_MAX;
		unit.lastSelectedPos.Set(0,0,0);

		GetAISystem()->AddDebugLine(targetLOS.start, targetLOS.end, 255,255,255, 6.0f);

		Vec3 norm(targetViewDir.y, -targetViewDir.x, 0);

		for (unsigned i = 0, ni = unit.covers.size(); i < ni; ++i)
		{
			SCoverPos& cover = unit.covers[i];

			if (cover.status == CHECK_INVALID)
				continue;

			float d = Distance::Point_PointSq(cover.pos, targetPos);
			if (d  < sqr(targetAvoidRadius))
				continue;

			if (Distance::Point_PointSq(enemyPos, cover.pos) < sqr(5.0f))
				continue;

			d = sqrtf(d);
			float dx = cover.pos.x - targetPos.x;
			float dy = cover.pos.y - targetPos.y;
			float invLen = isqrt_safe_tpl(sqr(dx) + sqr(dy));
			dx *= invLen; dy *= invLen;
			//			float dot = dx*norm.x + dy*norm.y;
			//			d *= 1.5f - sqr(dot);
			float dot = dx*targetViewDir.x + dy*targetViewDir.y;
			if (dot < 0.0f)
				continue;

			if (d > bestDist)
				continue;

			if (Distance::Point_Lineseg2DSq(cover.pos, targetLOS, ta) < sqr(targetAvoidRadius))
				continue;

			Lineseg movement(opPos, cover.pos);

			if (Intersect::Lineseg_Lineseg2D(targetLOS, movement, ta, tb))
			{
				GetAISystem()->AddDebugLine(movement.start, movement.end, 196,0,0, 4.0f);
				continue;
			}

			GetAISystem()->AddDebugLine(movement.start, movement.end, 0,196,0, 5.0f);

			if (unit.covers[i].status == CHECK_UNSET)
			{
				/*				if (pTerrShape && !pTerrShape->IsPointInsideShape(curPos, true))
				{
				points[j].test = SAIStatFormPt::INVALID;
				outFormation.debugPointsOutsideTerritory++;
				continue;
				}*/

				// Discard the point if it is inside another vegetation obstacle.
				bool	insideObstacle = false;
				for (unsigned k = 0, nk = unit.avoidPos.size(); k < nk; ++k)
				{
					if (Distance::Point_Point2DSq(cover.pos, unit.avoidPos[k].pos) < sqr(unit.avoidPos[k].rad + passRadius/2))
					{
						insideObstacle = true;
						break;
					}
				}

				if (insideObstacle)
				{
					cover.status = CHECK_INVALID;
					continue;
				}

				/*				int nBuildingID;
				IVisArea* pArea;
				IAISystem::ENavigationType navType = points[j].navType != IAISystem::NAV_UNSET ? points[j].navType :
				GetAISystem()->CheckNavigationType(curPos, nBuildingID, pArea, puppetNavCapMask);*/

				if (cover.navType == IAISystem::NAV_TRIANGULAR)
				{
					if (GetAISystem()->IsPointForbidden(cover.pos, passRadius, 0, 0))
					{
						cover.status = CHECK_INVALID;
						continue;
					}
					if (pathAdjustmentObstacles.IsPointInsideObstacles(cover.pos))
					{
						cover.status = CHECK_INVALID;
						continue;
					}
				}

				cover.status = CHECK_OK;
			}

			bestDist = d;
			unit.lastSelectedPos = cover.pos;
		}

		if (!unit.lastSelectedPos.IsZero())
		{
			unit.selectCoverTriggerPos = targetPos; //(targetPos + unit.lastSelectedPos) * 0.5f;
			unit.selectCoverTriggerRad = 3.0f; //Distance::Point_Point2D(unit.selectCoverTriggerPos, unit.lastSelectedPos) + 2.0f;
		}
		else
		{
			unit.selectCoverTriggerPos.Set(0,0,0);
			unit.selectCoverTriggerRad = -1.0f;
		}
	}

	if (unit.lastSelectedPos.IsZero())
		return unit.obj->GetPos();

	return unit.lastSelectedPos;
}

//===================================================================
// GetEnclosingGenericShapeOfType
//===================================================================
const SShape* CFollowerGroupTactic::GetEnclosingGenericShapeOfType(const Vec3& pos, int type, string& outName)
{
	const ShapeMap& shapes = GetAISystem()->GetGenericShapes();
	float	nearestDist = FLT_MAX;
	for (ShapeMap::const_iterator it = shapes.begin(), end = shapes.end(); it != end; ++it)
	{
		const SShape&	shape = it->second;
		if (!shape.enabled) continue;
		if (shape.type != type) continue;
		if (shape.IsPointInsideShape(pos, true))
		{
			outName = it->first;
			return &shape;
		}
	}

	return 0;
}

//===================================================================
// UpdateLookat
//===================================================================
void CFollowerGroupTactic::UpdateLookat(SUnit& unit, CAIActor* pTargetActor, float dt)
{
/*	int targetType = unit.obj->GetPerceivedAttentionTargetType();
	bool liveTarget = false;

	if ((targetType & (AITGT_VISUAL|AITGT_MEMORY)) != 0 && (targetType & AITGT_THREAT) != 0)
		liveTarget = true;

	// Decide to fire
	bool fire = false;
	if (unit.targetFiredWeapon)
	{
		// The target/player has fired weapon during the last update
		if (liveTarget)
		{
			// We have a live target, allow to fire.
			unit.emptyFireTime = 0.0f;
			fire = true;
		}
		else
		{
			// When no live target, require the target/player to shoot a while before start firing.
			unit.emptyFireTime += dt;
			fire = unit.emptyFireTime > 0.6f;
		}
	}
	else
	{
		// The player has not fired, if we were previously shooting already
		// and have live target, continue shooting.
		if (unit.firingTime > 0.0f && liveTarget)
			fire = true;
		unit.emptyFireTime = max(0.0f, unit.emptyFireTime - dt*0.2f);
	}

	if (fire)
	{
		unit.firingTime = 1.0f;
		if (unit.obj->GetWeaponDescriptor().fChargeTime > 0.0f)
			unit.firingTime += unit.obj->GetWeaponDescriptor().fChargeTime;
		unit.stayBackTime = 4.0f;
	}
	else
	{
		unit.firingTime -= dt;
		unit.stayBackTime -= dt;
	}*/

	const Vec3& pos = pTargetActor->GetPos();
	const Vec3& dir = pTargetActor->GetViewDir();

/*	if (unit.firingTime > 0.0f)
	{
		if (!liveTarget)
		{
			std::vector<IPhysicalEntity*> skipList;
			unit.obj->GetPhysicsEntitiesToSkip(skipList);
			if (CAIActor* pActor = unit.pTarget->CastToCAIActor())
				pActor->GetPhysicsEntitiesToSkip(skipList);

			if (unit.targetFiredWeapon)
			{
				ray_hit hit;
				float d = 40.0f;
				if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir*d, COVER_OBJECT_TYPES, HIT_COVER,
					&hit,1, skipList.empty() ? 0 : &skipList[0], skipList.size()))
				{
					d = hit.dist;
				}
				unit.obj->SetRefPointPos(pos + dir*d);
			}

			unit.obj->SetFireMode(FIREMODE_FORCED, AIFT_REFPOINT);
		}
		else
		{
			unit.obj->SetFireMode(FIREMODE_FORCED, AIFT_ATTENTION_TARGET);
		}
	}
	else*/
	{

		CAIObject* pLookTarget = unit.obj->GetOrCreateSpecialAIObject(CPipeUser::AISPECIAL_GROUP_TAC_LOOK);
		if (!pLookTarget)
			return;

		if (unit.mimicMode == MIMIC_STATIC)
		{

			if (unit.lookatPlayerTime > 0.0f)
				pLookTarget->SetPos(pos);
			else if (unit.movementTimeOut < 0.0f && Distance::Point_PointSq(unit.obj->GetPos(), pos) > 4.0f)
				pLookTarget->SetPos(pos);
			else
				pLookTarget->SetPos(pos + dir*20.0f);

			/*	int targetType = unit.obj->GetPerceivedAttentionTargetType();
			bool liveTarget = false;

			if ((targetType & (AITGT_VISUAL|AITGT_MEMORY)) != 0 && (targetType & AITGT_THREAT) != 0)
			liveTarget = true;

			&& (unit.lookatPlayerTime > 0.0f || unit.movementTimeOut < 0.0f))
			pLookTarget->SetPos(unit.obj->GetPos() + unit.lastTargetVel * 20.0f);*/
		}
		else
		{
			pLookTarget->SetPos(pos + dir*20.0f);
		}

//		unit.obj->SetFireMode(FIREMODE_OFF, AIFT_ATTENTION_TARGET);
//		unit.obj->SetLooseAttentionTarget(pos + dir*20.0f);
	}

//	unit.targetFiredWeapon = false;
}


//===================================================================
// Execute
//===================================================================
/*void COPMimic::UpdateFiring(CPuppet* pOperand, float dt)
{
	int targetType = unit.obj->GetPerceivedAttentionTargetType();
	bool liveTarget = false;

	if ((targetType & (AITGT_VISUAL|AITGT_MEMORY)) != 0 && (targetType & AITGT_THREAT) != 0)
		liveTarget = true;

	// Decide to fire
	bool fire = false;
	if (unit.targetFiredWeapon)
	{
		// The target/player has fired weapon during the last update
		if (liveTarget)
		{
			// We have a live target, allow to fire.
			unit.emptyFireTime = 0.0f;
			fire = true;
		}
		else
		{
			// When no live target, require the target/player to shoot a while before start firing.
			unit.emptyFireTime += dt;
			fire = unit.emptyFireTime > 0.6f;
		}
	}
	else
	{
		// The player has not fired, if we were previously shooting already
		// and have live target, continue shooting.
		if (unit.firingTime > 0.0f && liveTarget)
			fire = true;
		unit.emptyFireTime = max(0.0f, unit.emptyFireTime - dt*0.2f);
	}

	if (fire)
	{
		unit.firingTime = 1.0f;
		if (unit.obj->GetWeaponDescriptor().fChargeTime > 0.0f)
			unit.firingTime += unit.obj->GetWeaponDescriptor().fChargeTime;
		unit.stayBackTime = 4.0f;
	}
	else
	{
		unit.firingTime -= dt;
		unit.stayBackTime -= dt;
	}

	const Vec3& pos = unit.pTarget->GetPos();
	const Vec3& dir = unit.pTarget->GetViewDir();

	if (unit.firingTime > 0.0f)
	{
		if (!liveTarget)
		{
			std::vector<IPhysicalEntity*> skipList;
			unit.obj->GetPhysicsEntitiesToSkip(skipList);
			if (CAIActor* pActor = unit.pTarget->CastToCAIActor())
				pActor->GetPhysicsEntitiesToSkip(skipList);

			if (unit.targetFiredWeapon)
			{
				ray_hit hit;
				float d = 40.0f;
				if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir*d, COVER_OBJECT_TYPES, HIT_COVER,
					&hit,1, skipList.empty() ? 0 : &skipList[0], skipList.size()))
				{
					d = hit.dist;
				}
				unit.obj->SetRefPointPos(pos + dir*d);
			}

			unit.obj->SetFireMode(FIREMODE_FORCED, AIFT_REFPOINT);
		}
		else
		{
			unit.obj->SetFireMode(FIREMODE_FORCED, AIFT_ATTENTION_TARGET);
		}
	}
	else
	{
		unit.obj->SetFireMode(FIREMODE_OFF, AIFT_ATTENTION_TARGET);
		unit.obj->SetLooseAttentionTarget(pos + dir*20.0f);
	}

	unit.targetFiredWeapon = false;
}

//===================================================================
// UpdateStance
//===================================================================
void COPMimic::UpdateStance(CPuppet* pOperand, CAIActor* pTargetActor, float dt)
{
	const Vec3& opPos = unit.obj->GetPos();

	bool enemyInRange = false;
	bool enemyValid = false;
	Vec3 enemyPos;
	if (unit.obj->GetAttentionTarget())
	{
		enemyInRange = Distance::Point_PointSq(opPos, unit.obj->GetAttentionTarget()->GetPos()) < sqr(unit.obj->unit.Parameters.unit.fAttackRange);
		enemyPos = unit.obj->GetAttentionTarget()->GetPos();
		enemyValid = true;
	}

	Vec3 targetViewDir = unit.pTarget->GetViewDir();
	targetViewDir.z = 0.0f;
	targetViewDir.Normalize();
	const Vec3& targetPos = pTargetActor->GetPos();

	if (Distance::Point_Point2DSq(unit.mimicPos, opPos) > sqr(1.5f))
	{
		if (enemyValid && !enemyInRange)
			unit.obj->unit.State.bodystate = STANCE_STEALTH;
		else
			unit.obj->unit.State.bodystate = STANCE_STAND;
		unit.stanceEvalTime = 1.0f;
	}
	else
	{
		unit.stanceEvalTime -= dt;
		if (unit.stanceEvalTime < 0.0f)
		{
			unit.stanceEvalTime = 0.4f + ai_frand() * 0.3f;

			EStance	stance = STANCE_NULL;
			float		lean = -1.0f;

			bool	stanceOk = false;

			if (pTargetActor && pTargetActor->GetProxy())
			{
				SAIBodyInfo bi;
				pTargetActor->GetProxy()->QueryBodyInfo(bi);
				if (bi.stance == STANCE_CROUCH)
				{
					stance = STANCE_CROUCH;
					stanceOk = true;
				}
			}

			if (!stanceOk)
			{
				if (enemyValid)
				{
					if (enemyInRange)
						stanceOk = unit.obj->SelectAimPosture(enemyPos, stance, lean);
					else
						stanceOk = unit.obj->SelectHidePosture(enemyPos, stance, lean);
				}
				else
				{
					stance = STANCE_CROUCH;
					stanceOk = true;
				}
			}

			if (stanceOk)
				unit.obj->unit.State.bodystate = stance;
		}
	}

	if (pOperand)
		unit.obj->SetDelayedStance(STANCE_NULL);
}*/

//===================================================================
// UpdateStaticPos
//===================================================================
Vec3 CFollowerGroupTactic::UpdateStaticPos(SUnit& unit, CAIActor* pTargetActor, float dt)
{
	IAISystem::tNavCapMask navCapMask = unit.obj->m_movementAbility.pathfindingProperties.navCapMask;
	float passRadius = unit.obj->m_Parameters.m_fPassRadius;
	const CPathObstacles& pathAdjustmentObstacles = unit.obj->GetPathAdjustmentObstacles();

	Vec3 enemyPos;

	Vec3 targetViewDir = pTargetActor->GetViewDir();
	targetViewDir.z = 0.0f;
	targetViewDir.Normalize();
	const Vec3& targetPos = pTargetActor->GetPos();

	if (unit.obj->GetAttentionTargetType() != AITARGET_VISUAL &&
		unit.obj->GetAttentionTargetType() == AITARGET_MEMORY &&
		unit.obj->GetAttentionTargetThreat() >= AITHREAT_THREATENING)
	{
		enemyPos = unit.obj->GetAttentionTarget()->GetPos();
	}
	else
	{
		enemyPos = targetPos + targetViewDir * 30.0f;
	}

	// Try to first use cover path.
	const float preferOldDist = 1.0f;

	if (!unit.coverPaths.empty())
	{
		Vec3 previousPosOnPath = unit.lastSelectedPos;
		Vec3 segmentDir(0,0,0);

		float mind = FLT_MAX;
		unsigned sel = 0xffffffff;
		float distAlongPath = 0.0f;
		for (unsigned i = 0, ni = unit.coverPaths.size(); i < ni; ++i)
		{
			const SShape* pShape = unit.coverPaths[i];
			float d, dpath;
			Vec3 nearestPt;
			Vec3 segDir;
			pShape->NearestPointOnPath(targetPos, d, nearestPt, &dpath, &segDir);

			if (pShape == unit.pCurrentCoverPath)
				d = max(0.0f, d - preferOldDist);

			if (d < mind)
			{
				mind = d;
				unit.lastSelectedPos = nearestPt;
				distAlongPath = dpath;
				segmentDir = segDir;
				sel = i;
			}
		}

		if (sel == 0xffffffff || unit.lastSelectedPos.IsZero())
			return unit.obj->GetPos();

		if (fabsf(unit.followDist) > 0.01f)
		{
			unit.lastSelectedPos = unit.coverPaths[sel]->GetPointAlongPath(distAlongPath + unit.followDist);
		}

		if (unit.coverPaths[sel] != unit.pCurrentCoverPath)
		{
			unit.pCurrentCoverPath = unit.coverPaths[sel];
			unit.currentCoverPathName = unit.coverPathNames[sel];
		}

		if (!previousPosOnPath.IsZero())
		{
			Vec3 followTargetMoveDir = unit.lastSelectedPos - previousPosOnPath;
			float lenSqr = followTargetMoveDir.GetLengthSquared();
			bool backwards = false;
			if (lenSqr > sqr(0.1f))
			{
				unit.movementTimeOut = 4.0f;
				if (segmentDir.Dot(followTargetMoveDir) < 0.0f)
				{
					unit.lookatPlayerTime = 3.0f;
					backwards = true;
				}
			}
/*			// The lastTargetVel in this mimic mode is used for determining where to look
			// When moving backwards.
			if (lenSqr > sqr(1.0f))
			{
				// The follow target is faraway, look at it (should give nice anticipation too).
				unit.lastTargetVel = followTargetMoveDir;
			}
			else
			{
				// Close to follow target, use the segment direction.
				if (unit.lookatPlayerTime > 0.0f)
					unit.lastTargetVel = -segmentDir;
				else
					unit.lastTargetVel = segmentDir;
			}
			unit.lastTargetVel.Normalize();*/
		}
		else
		{
			// First update.
			unit.movementTimeOut = 8.0f;
		}

		unit.selectCoverTriggerRad = -1.0f;

		return unit.lastSelectedPos;
	}
	else
	{
		unit.pCurrentCoverPath = 0;
		unit.currentCoverPathName.clear();
	}

	// Could not find anything meaningful, return the last point;
	if (unit.mimicPos.IsZero())
		return unit.obj->GetPos();
	return unit.mimicPos;


/*	// No cover paths found, try to use anchors.

	Lineseg targetLOS(targetPos, targetPos + targetViewDir*5.0f);

	const float targetAvoidRadius = 2.5f;

	float ta, tb;

	bool selectTarget = false;

	if (unit.selectCoverTriggerRad < 0.0f)
		selectTarget = true;
	//	else if (Distance::Point_Point2DSq(unit.selectCoverTriggerPos, targetPos) > sqr(unit.selectCoverTriggerRad))
	else if (Distance::Point_Lineseg2DSq(targetPos, Lineseg(unit.lastSelectedPos, unit.selectCoverTriggerPos), ta) > sqr(unit.selectCoverTriggerRad))
		selectTarget = true;
	else if (Distance::Point_Point2DSq(unit.lastSelectedPos, targetPos) < sqr(1.5f))
		selectTarget = true;
	else if (unit.firingTime > 0.0f && Distance::Point_Lineseg2DSq(unit.lastSelectedPos, targetLOS, ta) < sqr(1.0f))
		selectTarget = true;

	if (selectTarget)
	{
		const Vec3& opPos = unit.obj->GetPos();

		float bestDist = FLT_MAX;
		unit.lastSelectedPos.Set(0,0,0);

		GetAISystem()->AddDebugLine(targetLOS.start, targetLOS.end, 255,255,255, 6.0f);

		Vec3 norm(targetViewDir.y, -targetViewDir.x, 0);

		for (unsigned i = 0, ni = unit.covers.size(); i < ni; ++i)
		{
			SCoverPos& cover = unit.covers[i];

			if (cover.status == CHECK_INVALID)
				continue;

			float d = Distance::Point_PointSq(cover.pos, targetPos);
			if (d  < sqr(targetAvoidRadius))
				continue;

			if (Distance::Point_PointSq(enemyPos, cover.pos) < sqr(5.0f))
				continue;

			if (d > bestDist)
				continue;

			if (Distance::Point_Lineseg2DSq(cover.pos, targetLOS, ta) < sqr(targetAvoidRadius))
				continue;

			Lineseg movement(opPos, cover.pos);

			if (Intersect::Lineseg_Lineseg2D(targetLOS, movement, ta, tb))
			{
				GetAISystem()->AddDebugLine(movement.start, movement.end, 196,0,0, 4.0f);
				continue;
			}

			GetAISystem()->AddDebugLine(movement.start, movement.end, 0,196,0, 5.0f);

			cover.status = CHECK_OK;

			bestDist = d;
			unit.lastSelectedPos = cover.pos;
		}

		if (!unit.lastSelectedPos.IsZero())
		{
			unit.selectCoverTriggerPos = targetPos;
			unit.selectCoverTriggerRad = 4.0f;
		}
		else
		{
			unit.selectCoverTriggerPos.Set(0,0,0);
			unit.selectCoverTriggerRad = -1.0f;
		}
	}*/

/*	if (unit.lastSelectedPos.IsZero())
		return unit.obj->GetPos();

	return unit.lastSelectedPos;*/
}

//====================================================================
// OnObjectRemoved
//====================================================================
void CFollowerGroupTactic::OnObjectRemoved(IAIObject* pObject)
{
	CheckAndRemoveUnit(pObject);
}

//====================================================================
// OnMemberAdded
//====================================================================
void CFollowerGroupTactic::OnMemberAdded(IAIObject* pMember)
{
//	if (!pMember) return;
//	CheckAndAddUnit(pMember, 0);
}

//====================================================================
// OnMemberRemoved
//====================================================================
void CFollowerGroupTactic::OnMemberRemoved(IAIObject* pMember)
{
	if (!pMember) return;
	CPuppet* pPuppet = pMember->CastToCPuppet();
	if (!pPuppet)
		return;
	CheckAndRemoveUnit(pMember);
}


//====================================================================
// OnUnitAttentionTargetChanged
//====================================================================
void CFollowerGroupTactic::OnUnitAttentionTargetChanged()
{
}

//====================================================================
// FindUnit
//====================================================================
CFollowerGroupTactic::UnitList::iterator CFollowerGroupTactic::FindUnit(CPuppet* puppet)
{
	UnitList::iterator it = m_units.begin();
	for (; it != m_units.end(); ++it)
		if ((*it).obj == puppet)
			break;
	return it;
}

//====================================================================
// CheckAndAddUnit
//====================================================================
void CFollowerGroupTactic::CheckAndAddUnit(IAIObject* object, int type)
{
	CPuppet* pPuppet = CastToCPuppetSafe(object);
	if(!pPuppet)
		return;
	UnitList::iterator it = FindUnit(pPuppet);
	if (it == m_units.end())
	{
		m_units.push_back(SUnit(pPuppet));
	}
	else
	{
		SUnit&	unit(*it);
		// Update type and reset unit.
		unit.Reset();
	}
}

//====================================================================
// CheckAndRemoveUnit
//====================================================================
void CFollowerGroupTactic::CheckAndRemoveUnit(IAIObject* object)
{
	CPuppet* pPuppet = CastToCPuppetSafe(object);
	if (!pPuppet)
		return;

	UnitList::iterator it = FindUnit(pPuppet);
	if (it != m_units.end())
		m_units.erase(it);
}

//====================================================================
// Reset
//====================================================================
void CFollowerGroupTactic::Reset()
{
	for (unsigned i = 0, ni = m_units.size(); i < ni; ++i)
		m_units[i].Reset();
}

//====================================================================
// NotifyState
//====================================================================
void CFollowerGroupTactic::NotifyState(IAIObject* pRequester, int type, float param)
{
	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);
	if (!pPuppet)
		return;

	if (type == GN_INIT)
	{
		CheckAndAddUnit(pRequester, 0);
		return;
	}

	UnitList::iterator it = FindUnit(pPuppet);
	if (it == m_units.end())
		return;
	SUnit&	unit = (*it);

	if (type == GN_NOTIFY_UNAVAIL)
	{
		unit.follow = false;
	}
	else if (type == GN_NOTIFY_ADVANCING)
	{
		unit.follow = true;
		unit.followDist = param;
		CAIActor* pTarget = CastToCAIActorSafe(GetAISystem()->GetPlayer());
		UpdateUnit(unit, pTarget, 0.1f);
	}
}

//====================================================================
// GetState
//====================================================================
int CFollowerGroupTactic::GetState(IAIObject* pRequester, int type, float param)
{
	return 0;
}

//====================================================================
// GetState
//====================================================================
Vec3 CFollowerGroupTactic::GetPoint(IAIObject* pRequester, int type, float param)
{
	return ZERO;
}

//====================================================================
// SUnit::Serialize
//====================================================================
void CFollowerGroupTactic::SUnit::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	ser.BeginGroup("Unit");
	{
		objectTracker.SerializeObjectPointer(ser, "obj", obj, false);

		ser.Value("follow", follow);
		ser.Value("followDist", followDist);

		ser.Value("targetFiredWeapon", targetFiredWeapon);
		ser.Value("firingTime", firingTime);
		ser.Value("stayBackTime", stayBackTime);
		ser.Value("lookatPlayerTime", lookatPlayerTime);

		ser.Value("shapeName", shapeName);
		ser.Value("currentCoverPathName", currentCoverPathName);

		if (ser.IsReading())
		{
			if (!shapeName.empty())
				pShape = GetAISystem()->GetGenericShapeOfName(shapeName.c_str());
			if (!currentCoverPathName.empty())
				pCurrentCoverPath = GetAISystem()->GetGenericShapeOfName(currentCoverPathName.c_str());

			mimicMode = pShape ? MIMIC_STATIC : MIMIC_FOLLOW;

			covers.clear();
			lastCoverCollectPos.zero();
			lastSelectedPos.zero();
			selectCoverTriggerPos.zero();
			selectCoverTriggerRad = -1.0f;
			avoidPos.clear();
			coverPaths.clear();
			lastTargetCheckPos.zero();
			mimicPos.zero();
			lastTargetVel.zero();
			movementTimeOut = 0;
			lastReadabilityTime = -1.0f;
			formDist = 0.0f;
			formAngle = 0.0f;
			formAxisX.zero();
			formAxisY.zero();
		}
	}
	ser.EndGroup();
}

//====================================================================
// Serialize
//====================================================================
void CFollowerGroupTactic::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	ser.BeginGroup("GroupState");

	// Serialize units.
	int	unitCount = m_units.size();
	ser.Value("unitCount", unitCount);
	ser.BeginGroup("Units");
	if(ser.IsReading())
	{
		m_units.clear();
		for(int i = 0; i < unitCount; ++i)
		{
			m_units.push_back(SUnit());	// Create the unit first so that objectTracker can patch the pointers.
			m_units.back().Serialize(ser, objectTracker);
		}
	}
	else
	{
		UnitList::iterator end = m_units.end();
		for(UnitList::iterator it = m_units.begin(); it != end; ++it)
			it->Serialize(ser, objectTracker);
	}
	ser.EndGroup();

	ser.EndGroup();	// "GroupState"

}

//====================================================================
// DebugDraw
//====================================================================
void CFollowerGroupTactic::DebugDraw(IRenderer* pRenderer)
{
	for (unsigned i = 0, ni = m_units.size(); i < ni; ++i)
	{
		SUnit& unit = m_units[i];

		if (unit.pShape)
			DebugDrawRangePolygon(pRenderer, unit.pShape->shape, 0.75f, ColorB(255,128,0,128), ColorB(255,128,0), true);

		bool firing = false; //m_firingTime > 0.0f;

		if (unit.mimicMode == MIMIC_STATIC)
		{
			pRenderer->DrawLabel(unit.obj->GetPhysicsPos(), 1.2f, "STATIC\n%s\n%s", firing ? "-->" : "", unit.lookatPlayerTime > 0.0f ? "LOOK PLAYER" : "");

			/*		for (unsigned i = 0; i < unit.nCovers; ++i)
			{
			DebugDrawRangeCircle(pRenderer, unit.coverObjPos[i], unit.coverRad[i], min(0.2f, unit.coverRad[i]), ColorB(0,196,255,128), ColorB(0,196,255), true);
			pRenderer->GetIRenderAuxGeom()->DrawSphere(unit.coverPos[i]+Vec3(0,0,0.5f), 0.25f, ColorB(255,0,0));
			}*/

			CAIObject* pLookTarget = unit.obj->GetOrCreateSpecialAIObject(CPipeUser::AISPECIAL_GROUP_TAC_LOOK);
			if (pLookTarget)
			{
				Vec3 p0, p1;
				p0 = unit.obj->GetPos();
				p1 = pLookTarget->GetPos();
				Vec3 dir = p1 - p0;
				dir.Normalize();
				pRenderer->GetIRenderAuxGeom()->DrawLine(p0, ColorB(255,255,255), p1, ColorB(255,255,255));
				pRenderer->GetIRenderAuxGeom()->DrawCone((p0+p1*2)/3, dir, 0.3f, 0.6f, ColorB(255,255,255));
			}

			for (unsigned j = 0, nj = unit.covers.size(); j < nj; ++j)
			{
				ColorB col;
				switch (unit.covers[j].status)
				{
				case CHECK_UNSET: col.Set(128,128,128,255); break;
				case CHECK_INVALID: col.Set(255,64,0,255); break;
				case CHECK_OK: col.Set(128,255,0,255); break;
				}
				pRenderer->GetIRenderAuxGeom()->DrawSphere(unit.covers[j].pos+Vec3(0,0,1), 0.25f, col);
			}

			DebugDrawCircleOutline(pRenderer, unit.lastCoverCollectPos, 7.0f, ColorB(255,196,0));
			DebugDrawCircleOutline(pRenderer, unit.lastCoverCollectPos, 20.0f, ColorB(0,196,255));

			if (unit.selectCoverTriggerRad > 0.0f)
			{
				Vec3 dir = unit.lastSelectedPos - unit.selectCoverTriggerPos;
				Vec3 norm(dir.y, -dir.x, 0);
				norm.Normalize();
				DebugDrawCircleOutline(pRenderer, unit.selectCoverTriggerPos, unit.selectCoverTriggerRad, ColorB(255,255,255));
				DebugDrawCircleOutline(pRenderer, unit.lastSelectedPos, unit.selectCoverTriggerRad, ColorB(255,255,255));

				pRenderer->GetIRenderAuxGeom()->DrawLine(unit.selectCoverTriggerPos + norm*unit.selectCoverTriggerRad, ColorB(255,255,255),
					unit.lastSelectedPos + norm*unit.selectCoverTriggerRad, ColorB(255,255,255));
				pRenderer->GetIRenderAuxGeom()->DrawLine(unit.selectCoverTriggerPos - norm*unit.selectCoverTriggerRad, ColorB(255,255,255),
					unit.lastSelectedPos - norm*unit.selectCoverTriggerRad, ColorB(255,255,255));
			}

			for (unsigned j = 0, nj = unit.coverPaths.size(); j < nj; ++j)
			{
				const SShape* pShape = unit.coverPaths[j];
				ListPositions::const_iterator prev = pShape->shape.end();
				for (ListPositions::const_iterator pit = pShape->shape.begin(), pend = pShape->shape.end(); pit != pend; ++pit)
				{
					if (prev != pend)
						pRenderer->GetIRenderAuxGeom()->DrawLine(*prev + Vec3(0,0,0.5f), ColorB(255,196,0), *pit + Vec3(0,0,0.5f), ColorB(255,196,0), 3.0f);
					prev = pit;
				}
			}

		}
		else if (unit.mimicMode == MIMIC_FOLLOW)
		{
			pRenderer->DrawLabel(unit.obj->GetPhysicsPos(), 1.2f, "FOLLOW\n%s", firing ? "-->" : "");

			pRenderer->GetIRenderAuxGeom()->DrawLine(unit.DEBUG_Center, ColorB(255,196,0,0), unit.DEBUG_SidePos0, ColorB(255,196,0,255), 6.0f);
			pRenderer->GetIRenderAuxGeom()->DrawLine(unit.DEBUG_Center, ColorB(255,196,0,0), unit.DEBUG_SidePos1, ColorB(255,196,0,255), 6.0f);
			pRenderer->GetIRenderAuxGeom()->DrawLine(unit.DEBUG_Center, ColorB(64,255,0,0), unit.DEBUG_LeadPos, ColorB(64,255,0,255), 6.0f);
			pRenderer->GetIRenderAuxGeom()->DrawLine(unit.DEBUG_Center, ColorB(25,255,255,0), unit.mimicPos, ColorB(255,255,255,255), 4.0f);

			pRenderer->GetIRenderAuxGeom()->DrawLine(unit.DEBUG_Center, ColorB(255,64,0), unit.DEBUG_Center+unit.formAxisY*2.0f, ColorB(255,64,0,255), 2.0f);

			pRenderer->GetIRenderAuxGeom()->DrawSphere(unit.mimicPos, 0.5f, ColorB(255,255,255));

		}

		pRenderer->GetIRenderAuxGeom()->DrawCylinder(unit.mimicPos, Vec3(0,0,1), 0.3f, 0.6f, ColorB(255,255,255));
	}
}




//====================================================================
// CBossGroupTactic
// Simple boss group tactic.
//====================================================================
CBossGroupTactic::CBossGroupTactic(CAIGroup* pGroup) :
	m_pGroup(pGroup)
{
}

CBossGroupTactic::~CBossGroupTactic()
{
}

//====================================================================
// Update
//====================================================================
void CBossGroupTactic::Update(float dt)
{
	CAIActor* pTarget = CastToCAIActorSafe(GetAISystem()->GetPlayer());

	const int ALIEN_HIDESPOT = 504;
	const int COMBAT_ATTACK_DIRECTION = 321;
	const int COMBAT_PROTECT_THIS_POINT = 314;


	if (m_destroyables.empty())
	{
		for (AIObjects::iterator ai = GetAISystem()->m_Objects.find(ALIEN_HIDESPOT), end = GetAISystem()->m_Objects.end(); ai != end && ai->first == ALIEN_HIDESPOT; ++ai )
		{
			CAIObject* pObject = ai->second;
			EntityId ent = 0;
			EntityId parent = 0;
			Vec3 pos = pObject->GetPos();
			IEntity* pEnt = pObject->GetEntity();
			if (!pEnt) continue;
			if (!pEnt->GetParent()) continue;
			parent = pEnt->GetParent()->GetId();

			pos = pEnt->GetWorldPos();
			ent = pEnt->GetId();

			m_destroyables.push_back(SAIDestroyable(pos, pObject->GetRadius(), ent, parent));
		}
	}
	if (m_attackPaths.empty())
	{
		for (ShapeMap::const_iterator it = GetAISystem()->GetDesignerPaths().begin(), end = GetAISystem()->GetDesignerPaths().end(); it != end; ++it)
		{
			const SShape&	path = it->second;
			// Skip wrong type
			if (path.type != COMBAT_ATTACK_DIRECTION)
				continue;
			m_attackPaths.push_back(&path);
		}
	}


	for (unsigned i = 0, ni = m_units.size(); i < ni; ++i)
		UpdateUnit(m_units[i], pTarget, dt);


	// Update and draw connections.
	bool found = false;
	for (int j = 0; j < (int)m_DEBUG_connections.size(); )
	{
		m_DEBUG_connections[j].t -= dt;
		if (m_DEBUG_connections[j].t < 0.0f)
		{
			m_DEBUG_connections[j] = m_DEBUG_connections.back();
			m_DEBUG_connections.pop_back();
		}
		else
			++j;
	}

}

//====================================================================
// UpdateUnit
//====================================================================
void CBossGroupTactic::UpdateUnit(SUnit& unit, CAIActor* pTarget, float dt)
{
	int nearest = -1;
	float nearestDist = FLT_MAX;

	Vec3 bossPos = unit.obj->GetPhysicsPos();
	bossPos.z += 1.0f;

	int numActiveDestroyables = 0;
	int numConnectedDestroyables = 0;

	for (int i = 0, ni = (int)m_destroyables.size(); i < ni; ++i)
	{
		SAIDestroyable& dest = m_destroyables[i];

		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(dest.ent);
		if (!pEnt) continue;
		IEntity* pParentEnt = gEnv->pEntitySystem->GetEntity(dest.parent);
		if (!pParentEnt) continue;

		if (dest.destroyed) continue;

		dest.destroyed = GetAISystem()->CheckSmartObjectStates(pEnt, "Destroyed");

		if (dest.destroyed)
		{
			dest.SetInUse(false);
			continue;
		}

		dest.pos = pEnt->GetWorldPos() - Vec3(0,0,0.7f);
		numActiveDestroyables++;

//		DebugDrawWireSphere(pRenderer, dest.pos, 1.5f, dest.inUse ? ColorB(255,43,12,128) : ColorB(43,255,12,128));

		float d = Distance::Point_Point(dest.pos, bossPos);
		if (d < dest.rad)
		{
			dest.SetInUse(true);
			numConnectedDestroyables++;

			// DEBUG
			bool found = false;
			for (int j = 0; j < (int)m_DEBUG_connections.size(); ++j)
			{
				if (m_DEBUG_connections[j].i == i && m_DEBUG_connections[j].pUnit == &unit)
				{
					m_DEBUG_connections[j].t = 1.0f;
					m_DEBUG_connections[j].pos = dest.pos;
					m_DEBUG_connections[j].posSrc = bossPos;
					found = true;
					break;
				}
			}
			if (!found)
				m_DEBUG_connections.push_back(SAILightBoltProto(dest.pos, i, bossPos, &unit));

		}
		else
			dest.SetInUse(false);
	}

	Vec3 nearestPos(0,0,0);

	int phase = 0;

	Vec3 targetPos(0,0,0);

	if (unit.state == GN_NOTIFY_ADVANCING || unit.state == GN_NOTIFY_SEEKING)
	{
		if (unit.obj->GetAttentionTarget())
			targetPos = unit.obj->GetAttentionTarget()->GetPos();
	}
	else if (unit.state == GN_NOTIFY_SEARCHING)
	{
		if (unit.searchPoints.empty())
		{
			for (unsigned i = 0, ni = m_destroyables.size(); i < ni; ++i)
				unit.searchPoints.push_back(m_destroyables[i].pos);

			if (unit.obj->GetAttentionTarget())
				targetPos = unit.obj->GetAttentionTarget()->GetPos();
			else
				unit.searchPos = bossPos;
		}

		targetPos = unit.searchPos;
	}

	if (!targetPos.IsZero())
	{
		nearestDist = FLT_MAX;
		const SShape* pNearestShape = 0;

		targetPos.z -= 1.2f;

		if (numActiveDestroyables > 0)
		{
			phase = 1;

			for (int i = 0, ni = (int)m_attackPaths.size(); i < ni; ++i)
			{
				const SShape* pShape = m_attackPaths[i];
				float d = FLT_MAX;
				Vec3 pos;
				if (!GetNearestPointOnClippedPath(targetPos, pShape, m_destroyables, d, pos))
					continue;
				if (pShape == unit.pLastAttackPath)
					d = max(0.0f, d - 2.0f);
				if (d < nearestDist)
				{
					nearestDist = d;
					nearestPos = pos;
					pNearestShape = pShape;
				}
			}

			// Draw paths
//			for (int i = 0, ni = (int)m_attackPaths.size(); i < ni; ++i)
//				DrawClippedPath(pRenderer, attackPaths[i], destroyable, ColorB(255,0,0));
		}
		else
		{
			phase = 2;

			for (int i = 0, ni = (int)m_attackPaths.size(); i < ni; ++i)
			{
				const SShape* pShape = m_attackPaths[i];
				float d = FLT_MAX;
				Vec3 pos;
				pShape->NearestPointOnPath(targetPos, d, pos);
				if (pShape == unit.pLastAttackPath)
					d = max(0.0f, d - 2.0f);
				if (d < nearestDist)
				{
					nearestDist = d;
					nearestPos = pos;
					pNearestShape = pShape;
				}
			}
		}

		unit.phase = phase;


		if (pNearestShape)
		{
			unit.pLastAttackPath = pNearestShape;
		}
	}

	if (numConnectedDestroyables > 0)
	{
		// Make sure the boss is invincible as long as there are
		if (unit.obj->GetProxy()->GetActorHealth() < unit.obj->GetProxy()->GetActorMaxHealth())
		{
			unit.obj->SetSignal(1, "OnResetHealth", 0, 0);
		}
		if (unit.canDamage)
		{
			unit.obj->SetSignal(1, "OnDamageOff", 0, 0);
			unit.canDamage = false;
		}
	}
	else
	{
//		pRenderer->DrawLabel(pBossPuppet->GetPos()+Vec3(0,0,1), 1.5f, "%d", (int)pBossPuppet->GetProxy()->GetActorHealth());
		if (!unit.canDamage)
		{
			unit.obj->SetSignal(1, "OnDamageOn", 0, 0);
			unit.canDamage = true;
		}
	}

	if (nearestPos.IsZero())
		nearestPos = unit.obj->GetPos();

	// Update search pos
	if (unit.state == GN_NOTIFY_SEARCHING)
	{
		if (Distance::Point_Point2DSq(nearestPos, bossPos) < 1.0f && fabsf(nearestPos.z - bossPos.z) < 2.0f)
		{
			// Close to the search pos, find another one.
			unsigned nearest = 0;
			float nearestDist = FLT_MAX;
			for (unsigned j = 0, nj = unit.searchPoints.size(); j < nj; ++j)
			{
				float d = Distance::Point_PointSq(unit.searchPoints[j], bossPos);
				if (d < nearestDist)
				{
					nearestDist = d;
					nearest = j;
				}
			}
			unit.searchPos = unit.searchPoints[nearest];
			unit.searchPoints[nearest] = unit.searchPoints.back();
			unit.searchPoints.pop_back();
		}
	}

	unit.nearestPos = nearestPos;

	if (unit.state != GN_NOTIFY_UNAVAIL)
		unit.obj->SetRefPointPos(nearestPos);
}

//====================================================================
// OnObjectRemoved
//====================================================================
void CBossGroupTactic::OnObjectRemoved(IAIObject* pObject)
{
	CheckAndRemoveUnit(pObject);
}

//====================================================================
// OnMemberAdded
//====================================================================
void CBossGroupTactic::OnMemberAdded(IAIObject* pMember)
{
	//	if (!pMember) return;
	//	CheckAndAddUnit(pMember, 0);
}

//====================================================================
// OnMemberRemoved
//====================================================================
void CBossGroupTactic::OnMemberRemoved(IAIObject* pMember)
{
	if (!pMember) return;
	CPuppet* pPuppet = pMember->CastToCPuppet();
	if (!pPuppet)
		return;
	CheckAndRemoveUnit(pMember);
}


//====================================================================
// OnUnitAttentionTargetChanged
//====================================================================
void CBossGroupTactic::OnUnitAttentionTargetChanged()
{
}

//====================================================================
// FindUnit
//====================================================================
CBossGroupTactic::UnitList::iterator CBossGroupTactic::FindUnit(CPuppet* puppet)
{
	UnitList::iterator it = m_units.begin();
	for (; it != m_units.end(); ++it)
		if ((*it).obj == puppet)
			break;
	return it;
}

//====================================================================
// CheckAndAddUnit
//====================================================================
void CBossGroupTactic::CheckAndAddUnit(IAIObject* object, int type)
{
	CPuppet* pPuppet = CastToCPuppetSafe(object);
	if(!pPuppet)
		return;
	UnitList::iterator it = FindUnit(pPuppet);
	if (it == m_units.end())
	{
		m_units.push_back(SUnit(pPuppet));
	}
	else
	{
		SUnit&	unit(*it);
		// Update type and reset unit.
		unit.Reset();
	}
}

//====================================================================
// CheckAndRemoveUnit
//====================================================================
void CBossGroupTactic::CheckAndRemoveUnit(IAIObject* object)
{
	CPuppet* pPuppet = CastToCPuppetSafe(object);
	if (!pPuppet)
		return;

	UnitList::iterator it = FindUnit(pPuppet);
	if (it != m_units.end())
		m_units.erase(it);
}

//====================================================================
// Reset
//====================================================================
void CBossGroupTactic::Reset()
{
	for (unsigned i = 0, ni = m_units.size(); i < ni; ++i)
		m_units[i].Reset();

	m_destroyables.clear();
	m_attackPaths.clear();
	m_DEBUG_connections.clear();
}

//====================================================================
// NotifyState
//====================================================================
void CBossGroupTactic::NotifyState(IAIObject* pRequester, int type, float param)
{
	CPuppet* pPuppet = CastToCPuppetSafe(pRequester);
	if (!pPuppet)
		return;

	if (type == GN_INIT)
	{
		CheckAndAddUnit(pRequester, 0);
		return;
	}

	UnitList::iterator it = FindUnit(pPuppet);
	if (it == m_units.end())
		return;
	SUnit&	unit = (*it);

	if (type == GN_NOTIFY_UNAVAIL)
	{
		unit.state = GN_NOTIFY_UNAVAIL;
	}
	else if (type == GN_NOTIFY_ADVANCING)
	{
		unit.state = GN_NOTIFY_ADVANCING;
		unit.searchPoints.clear();
	}
	else if (type == GN_NOTIFY_SEEKING)
	{
		unit.state = GN_NOTIFY_SEEKING;
		unit.searchPoints.clear();
	}
	else if (type == GN_NOTIFY_SEARCHING)
	{
		unit.state = GN_NOTIFY_SEARCHING;
	}
}

//====================================================================
// GetState
//====================================================================
int CBossGroupTactic::GetState(IAIObject* pRequester, int type, float param)
{
	return 0;
}

//====================================================================
// GetState
//====================================================================
Vec3 CBossGroupTactic::GetPoint(IAIObject* pRequester, int type, float param)
{
	return ZERO;
}

//====================================================================
// SUnit::Serialize
//====================================================================
void CBossGroupTactic::SUnit::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	ser.BeginGroup("Unit");
	{
		objectTracker.SerializeObjectPointer(ser, "obj", obj, false);

/*		ser.Value("follow", follow);
		ser.Value("followDist", followDist);

		ser.Value("targetFiredWeapon", targetFiredWeapon);
		ser.Value("firingTime", firingTime);
		ser.Value("stayBackTime", stayBackTime);

		ser.Value("shapeName", shapeName);
		ser.Value("currentCoverPathName", currentCoverPathName);

		if (ser.IsReading())
		{
			if (!shapeName.empty())
				pShape = GetAISystem()->GetGenericShapeOfName(shapeName.c_str());
			if (!currentCoverPathName.empty())
				pCurrentCoverPath = GetAISystem()->GetGenericShapeOfName(currentCoverPathName.c_str());

			mimicMode = pShape ? MIMIC_STATIC : MIMIC_FOLLOW;

			covers.clear();
			lastCoverCollectPos.zero();
			lastSelectedPos.zero();
			selectCoverTriggerPos.zero();
			selectCoverTriggerRad = -1.0f;
			avoidPos.clear();
			coverPaths.clear();
			lastTargetCheckPos.zero();
			mimicPos.zero();
			lastTargetVel.zero();
			movementTimeOut = 0;
			formDist = 0.0f;
			formAngle = 0.0f;
			formAxisX.zero();
			formAxisY.zero();
		}*/
	}
	ser.EndGroup();
}

//====================================================================
// Serialize
//====================================================================
void CBossGroupTactic::Serialize(TSerialize ser, CObjectTracker& objectTracker)
{
	ser.BeginGroup("GroupState");

	// Serialize units.
	int	unitCount = m_units.size();
	ser.Value("unitCount", unitCount);
	ser.BeginGroup("Units");
	if(ser.IsReading())
	{
		m_units.clear();
		for(int i = 0; i < unitCount; ++i)
		{
			m_units.push_back(SUnit());	// Create the unit first so that objectTracker can patch the pointers.
			m_units.back().Serialize(ser, objectTracker);
		}
	}
	else
	{
		UnitList::iterator end = m_units.end();
		for(UnitList::iterator it = m_units.begin(); it != end; ++it)
			it->Serialize(ser, objectTracker);
	}
	ser.EndGroup();

	ser.EndGroup();	// "GroupState"
}

bool CBossGroupTactic::IntersectSegmentSphere(const Vec3& p0, const Vec3& p1, const Vec3& center, float rad, float& t0, float& t1)
{
	Vec3 dir = p1 - p0;

	float a = dir.GetLengthSquared();
	float b = (dir | (p0 - center)) * 2.0f;
	float c = Distance::Point_PointSq(p0, center) - sqr(rad);
	float desc = (b*b) - (4 * a *c);

	if (desc >= 0.0f)
	{
		t0 = (-b - sqrtf(desc)) / (2.0f * a);
		t1 = (-b + sqrtf(desc)) / (2.0f * a);
		return true;
	}
	return false; 
}

bool CBossGroupTactic::GetNearestPointOnClippedPath(const Vec3& pos, const SShape* pShape, const std::vector<SAIDestroyable>& destroyable, float& dist, Vec3& pt)
{
	ListPositions::const_iterator nearest(pShape->shape.begin());
	ListPositions::const_iterator cur = pShape->shape.begin();
	ListPositions::const_iterator next(cur);
	++next;

	bool hit = false;

	while (next != pShape->shape.end())
	{
		Lineseg	seg(*cur, *next);

		float t0 = FLT_MAX, t1 = -FLT_MAX;

		for (unsigned i = 0, ni = destroyable.size(); i < ni; ++i)
		{
			if (destroyable[i].destroyed) continue;
			float tmin, tmax;
			if (IntersectSegmentSphere(*cur, *next, destroyable[i].pos, destroyable[i].rad, tmin, tmax))
			{
				if (tmin < 1.0f && tmax > 0.0f)
				{
					tmin = max(0.0f, tmin);
					tmax = min(1.0f, tmax);
					t0 = min(t0, tmin);
					t1 = max(t1, tmax);
				}
			}
		}

		if (t0 < 1.0f && t1 > 0.0f)
		{
			Lineseg	segClipped(seg.GetPoint(t0), seg.GetPoint(t1));

			float t;
			float dd = Distance::Point_Lineseg(pos, segClipped, t);
			if (dd < dist)
			{
				dist = dd;
				pt = segClipped.GetPoint(t);
			}
			hit = true;
		}

		cur = next;
		++next;
	}

	return hit;
}

void CBossGroupTactic::DrawClippedPath(IRenderer *pRenderer, const SShape* pShape, const std::vector<SAIDestroyable>& destroyable, ColorB col)
{
	ListPositions::const_iterator nearest(pShape->shape.begin());
	ListPositions::const_iterator cur = pShape->shape.begin();
	ListPositions::const_iterator next(cur);
	++next;

	while (next != pShape->shape.end())
	{
		Lineseg	seg(*cur, *next);

		float t0 = FLT_MAX, t1 = -FLT_MAX;

		for (unsigned i = 0, ni = destroyable.size(); i < ni; ++i)
		{
			if (destroyable[i].destroyed) continue;
			float tmin, tmax;
			if (IntersectSegmentSphere(*cur, *next, destroyable[i].pos, destroyable[i].rad, tmin, tmax))
			{
				if (tmin < 1.0f && tmax > 0.0f)
				{
					tmin = max(0.0f, tmin);
					tmax = min(1.0f, tmax);
					t0 = min(t0, tmin);
					t1 = max(t1, tmax);
				}
			}
		}

		if (t0 < 1.0f && t1 > 0.0f)
			pRenderer->GetIRenderAuxGeom()->DrawLine(seg.GetPoint(t0), col, seg.GetPoint(t1), col, 3.0f);

		cur = next;
		++next;
	}
}

//====================================================================
// DebugDraw
//====================================================================
void CBossGroupTactic::DebugDraw(IRenderer* pRenderer)
{
	int phase = 0;

	bool debugDraw = false;

	if (debugDraw)
	{
		for (unsigned i = 0, ni = m_units.size(); i < ni; ++i)
		{
			SUnit& unit = m_units[i];
			pRenderer->GetIRenderAuxGeom()->DrawCylinder(unit.nearestPos, Vec3(0,0,1), 0.25f, 0.5f, ColorB(255,255,255));
			phase = max(phase, unit.phase);

			const char* szText = "";
			switch(unit.state)
			{
			case GN_NOTIFY_ADVANCING: szText = "Combat"; break;
			case GN_NOTIFY_SEEKING: szText = "Seek"; break;
			case GN_NOTIFY_SEARCHING: szText = "Search"; break;
			}

			pRenderer->DrawLabel(unit.obj->GetPos() + Vec3(0, 0, 3), 1.5f, "%s", szText);

			if (unit.state == GN_NOTIFY_SEARCHING)
			{
				DebugDrawCircleOutline(pRenderer, unit.searchPos, 2.0f, ColorB(255,255,255));
			}

		}
	}

	if (debugDraw)
	{
		for (int i = 0, ni = (int)m_destroyables.size(); i < ni; ++i)
		{
			SAIDestroyable& dest = m_destroyables[i];
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(dest.ent);
			if (!pEnt) continue;
			IEntity* pParentEnt = gEnv->pEntitySystem->GetEntity(dest.parent);
			if (!pParentEnt) continue;
			if (dest.destroyed) continue;
			DebugDrawWireSphere(pRenderer, dest.pos, 1.5f, dest.inUse ? ColorB(255,43,12,128) : ColorB(43,255,12,128));
		}
	}

	if (debugDraw)
	{
		ColorB	col(255,196,0,128);
		for (int i = 0, ni = (int)m_attackPaths.size(); i < ni; ++i)
		{
			ListPositions::const_iterator nearest(m_attackPaths[i]->shape.begin());
			ListPositions::const_iterator cur = m_attackPaths[i]->shape.begin();
			ListPositions::const_iterator next(cur);
			++next;

			while (next != m_attackPaths[i]->shape.end())
			{
				pRenderer->GetIRenderAuxGeom()->DrawLine(*cur+Vec3(0,0,-0.1f), col, *next+Vec3(0,0,-0.1f), col, 3.0f);
				cur = next;
				++next;
			}
		}
	}

	if (phase > 0)
	{
		// Draw paths
		if (debugDraw)
		{
			for (int i = 0, ni = (int)m_attackPaths.size(); i < ni; ++i)
				DrawClippedPath(pRenderer, m_attackPaths[i], m_destroyables, ColorB(255,0,0));
		}
	}


	const float dt = GetAISystem()->GetFrameDeltaTime();

	for (int i = 0, ni = (int)m_DEBUG_connections.size(); i < ni; ++i)
	{
		SAILightBoltProto& bolt = m_DEBUG_connections[i];

		IEntity* pParentEnt = gEnv->pEntitySystem->GetEntity(m_destroyables[bolt.i].parent);
		if (!pParentEnt) continue;

		AABB	aabb;
		pParentEnt->GetWorldBounds(aabb);

		Vec3 delta = bolt.pos - bolt.posSrc;
		float s = clamp(Distance::Point_Point(bolt.pos, bolt.posSrc)/6.0f, 0.1f, 1.0f);

		float a = bolt.t / 1.0f;

		bolt.updateTime += dt;
		if (bolt.updateTime > bolt.updateTimeMax)
		{
			bolt.boltPos.x = aabb.min.x + (aabb.max.x - aabb.min.x) * ai_frand();
			bolt.boltPos.y = aabb.min.y + (aabb.max.y - aabb.min.y) * ai_frand();
			bolt.boltPos.z = aabb.min.z + (aabb.max.z - aabb.min.z) * ai_frand();

			bolt.bolt.Generate(bolt.boltPos, bolt.posSrc, s, 4);
			bolt.updateTimeMax = ai_frand()*0.15f;
			bolt.updateTime = 0;
		}

		float v = bolt.updateTime / bolt.updateTimeMax;
		Vec3* pts = bolt.bolt.Animate(v*0.1f - ai_frand()*0.1f);

		for (int j = 0; j < bolt.bolt.GetPointCount()-1; ++j)
		{
			Vec3 dir = pts[j+1] - pts[j];
			Vec3 dirn(dir);
			float len = dirn.NormalizeSafe();
			pRenderer->GetIRenderAuxGeom()->DrawCylinder(pts[j]+dir*0.5f, dirn, -0.1f, len, ColorB(64,128,255,(uint8)(32*a)), false);
		}

		pRenderer->GetIRenderAuxGeom()->DrawPolyline(pts, bolt.bolt.GetPointCount(), false, ColorB(235,255,255,(uint8)(240*a)), 4.0f);

		Vec3 p;
		p = bolt.pos;
		p.x += (2*ai_frand()-1) * 0.7f;
		p.y += (2*ai_frand()-1) * 0.7f;
		p.z += (2*ai_frand()-1) * 0.7f;
		bolt.boltSmall.Generate(p, pts[bolt.bolt.GetPointCount()/2], s*0.5f, 3);
		pRenderer->GetIRenderAuxGeom()->DrawPolyline(bolt.boltSmall.GetPoints(), bolt.boltSmall.GetPointCount(), false, ColorB(235,255,255,(uint8)(128*a)));

		p = bolt.pos;
		p.x += (2*ai_frand()-1) * 0.7f;
		p.y += (2*ai_frand()-1) * 0.7f;
		p.z += (2*ai_frand()-1) * 0.7f;
		bolt.boltSmall.Generate(p, pts[bolt.bolt.GetPointCount()/4], s*0.5f, 2);
		pRenderer->GetIRenderAuxGeom()->DrawPolyline(bolt.boltSmall.GetPoints(), bolt.boltSmall.GetPointCount(), false, ColorB(235,255,255,(uint8)(128*a)));

		p = bolt.posSrc;
		p.x += (2*ai_frand()-1) * 0.7f;
		p.y += (2*ai_frand()-1) * 0.7f;
		p.z += (2*ai_frand()-1) * 0.7f;
		bolt.boltSmall.Generate(p, pts[bolt.bolt.GetPointCount()*3/4], s*0.5f, 2);
		pRenderer->GetIRenderAuxGeom()->DrawPolyline(bolt.boltSmall.GetPoints(), bolt.boltSmall.GetPointCount(), false, ColorB(235,255,255,(uint8)(128*a)));

	}

}
