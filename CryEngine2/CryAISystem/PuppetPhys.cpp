/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004
-------------------------------------------------------------------------
File name:   PuppetPhys.cpp
$Id$
Description:	should contaioin all the methods of CPuppet which have to deal with Physics 

-------------------------------------------------------------------------
History:
- 2007				: Created by Kirill Bulatsev


*********************************************************************/

#include "StdAfx.h"
#include "Puppet.h"
#include "AILog.h"
#include "GoalOp.h"
#include "Graph.h"
#include "AIPlayer.h"
#include "Leader.h"
#include "CAISystem.h"
#include "AICollision.h"
#include "FlightNavRegion.h"
#include "VertexList.h"
#include "SquadMember.h"
#include "ObjectTracker.h"
#include "SmartObjects.h"
#include "TickCounter.h"
#include "PathFollower.h"
#include "AIVehicle.h"

#include "IConsole.h"
#include "IPhysics.h"
#include "ISystem.h"
#include "ILog.h"
#include "ITimer.h"
#include "I3DEngine.h"
#include "ISerialize.h"
#include "IRenderer.h"


// Helper structure to sort indices pointing to an array of weights.
struct SDamageLUTSorter
{
	SDamageLUTSorter(const std::vector<float> &values) : m_values(values) {}
	bool operator()(int lhs, int rhs) const { return m_values[lhs] < m_values[rhs]; }
	const std::vector<float>&	m_values;
};


//====================================================================
// ActorObstructingAim
//====================================================================
bool CPuppet::ActorObstructingAim(const CAIActor* pActor, const Vec3& firePos, const Vec3& dir, const Ray& fireRay, float checkDist)
{
	ray_hit	hit;
	if (gEnv->pPhysicalWorld->CollideEntityWithBeam(pActor->GetPhysics(false), firePos, dir, 0.05f, &hit) &&
			hit.dist < checkDist)
				return true;

	// Additional check for the head, in case of leaning.
	if (fabsf(pActor->GetState()->lean) > 0.01f)
	{
		// Simulate the bent upper body position.
		Vec3	toeToHead = pActor->GetPos() - pActor->GetPhysicsPos();
		float	len = toeToHead.GetLength();
		if (len > 0.0001f)
			toeToHead *= (len - 0.3f) / len;
		Vec3	hit0, hit1;
		if (Intersect::Ray_Sphere(fireRay, Sphere(pActor->GetPhysicsPos() + toeToHead, 0.4f + 0.05f), hit0, hit1) &&
				Distance::Point_PointSq(fireRay.origin, hit0) < sqr(checkDist) )
					return true;
	}
	return false;
}


//====================================================================
// CanAimWithoutObstruction
//====================================================================
bool CPuppet::CanAimWithoutObstruction(const Vec3 &vTargetPos)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	if (m_bDryUpdate)
		return m_lastAimObstructionResult;

	if (m_updatePriority == AIPUP_LOW || m_updatePriority == AIPUP_MED)
		return true;

	if	( GetProxy() && GetProxy()->GetLinkedVehicleEntityId() )
		return true;

	if	( GetSubType() == CAIObject::STP_2D_FLY )
		return true;

	const Vec3&	firePos = GetFirePos();
	Vec3	dir = vTargetPos - firePos;

	Ray	fireRay(firePos, dir);

	const VectorSet<CPuppet*>& enabledPuppetsSet = GetAISystem()->GetEnabledPuppetSet();
	for (VectorSet<CPuppet*>::const_iterator it = enabledPuppetsSet.begin(), itend = enabledPuppetsSet.end(); it != itend; ++it)
	{
		CPuppet* pPuppet = *it;
		if (!pPuppet || pPuppet == this || !pPuppet->IsEnabled())
			continue;
		if (!pPuppet->GetProxy())
			continue;

		if (Distance::Point_PointSq(GetPos(), pPuppet->GetPos()) > sqr(10.0f))
			continue;

		// Allow to aim closer when the target is enemy.
		float	checkDist = 1.2f;
		if (IsHostile(pPuppet))
			checkDist = 0.7f;

		if(ActorObstructingAim(pPuppet, firePos, dir, fireRay, checkDist))
		{
			m_lastAimObstructionResult = false;
			return false;
		}
	}
	//
	//check the player (friends only)
	CAIActor* pPlayer(CastToCAIActorSafe(GetAISystem()->GetPlayer()));
	if (pPlayer && 
			!IsHostile(pPlayer) &&
			Distance::Point_PointSq(GetPos(), pPlayer->GetPos()) < sqr(5.0f) &&
			ActorObstructingAim(pPlayer, firePos, dir, fireRay, 1.2f))
	{
		m_lastAimObstructionResult = false;
		return false;
	}

	if (m_fireMode != FIREMODE_KILL) // when in KILL mode - just shoot no matter what
	{
		std::vector<IPhysicalEntity*> skipList;
		GetPhysicsEntitiesToSkip(skipList);

		const float	checkDistance = 1.2f;
		const float softCheckDistance = 0.5f;
		dir.Normalize();
		dir *= checkDistance;

		ray_hit hit;
		if (gEnv->pPhysicalWorld->RayWorldIntersection(firePos, dir, COVER_OBJECT_TYPES, HIT_COVER|HIT_SOFT_COVER,
				&hit, 1, skipList.empty() ? 0 : &skipList[0], skipList.size()))
		{
			pe_status_pos	stat;
			if (hit.pCollider && hit.pCollider->GetStatus(&stat) != 0)
			{
				if ((stat.flagsOR & geom_colltype_obstruct) != 0)
				{
					// Hit soft cover, shorter check distance.
					if (hit.dist < softCheckDistance)
					{
						m_lastAimObstructionResult = false;
						return false;
					}
				}
				else
				{
					// Normal hit
					m_lastAimObstructionResult = false;
					return false;
				}
			}
			else
			{
				m_lastAimObstructionResult = false;
				return false;
			}
		}
	}

	m_lastAimObstructionResult = true;
	return true;
}

//====================================================================
// ClampPointInsideCone
//====================================================================
static void ClampPointInsideCone(const Vec3& conePos, const Vec3& coneDir, float coneAngle, Vec3& pos)
{
	Vec3 dirToPoint = pos - conePos;
	float distOnLine = coneDir.Dot(dirToPoint);
	if (distOnLine < 0.0f)
	{
		pos = conePos;
		AILogComment("ClampPointInsideCone(): Point clamped distOnLine = %f", distOnLine);
		return;
	}

	Vec3 pointOnLine = coneDir * distOnLine;

	const float dmax = tanf(coneAngle) * distOnLine + 0.75f;

	Vec3 diff = dirToPoint - pointOnLine;
	float d = diff.NormalizeSafe();

	if (d > dmax)
	{
		pos = conePos + pointOnLine + diff * dmax;
		AILogComment("ClampPointInsideCone(): Point clamped d = %f, dmax = %f", d, dmax);
	}
}

//====================================================================
// DeltaAngle
// returns delta angle from a to b in -PI..PI
//====================================================================
inline float DeltaAngle(float a, float b)
{
	float d = b - a;
	d = fmodf(d, gf_PI2);
	if(d < -gf_PI) d += gf_PI2;
	if(d > gf_PI) d -= gf_PI2;
	return d;
};

//====================================================================
// CheckAndAdjustFireTarget
//====================================================================
bool CPuppet::CheckAndAdjustFireTarget(const Vec3& targetPos, float extraRad, Vec3& posOut, float clampAngle, float maxMoveDelta)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	Vec3 missAxisX(0,0,0), missAxisY(0,0,0);

	// Project target velocity to
	CAIActor* pLiveTargetActor = CastToCAIActorSafe(GetLiveTarget(GetFireTargetObject()));

	if (m_targetLastMissPointSelectionTime.GetSeconds() < 0.00001f)
		m_targetLastMissPointSelectionTime = GetAISystem()->GetFrameStartTime();

	float targetDistanceToSilhouette = FLT_MAX;

	if (!m_targetSilhouette.valid)
	{
		targetDistanceToSilhouette = FLT_MAX;
		Vec3	dir = targetPos - GetFirePos();
		if(dir.IsZero(.1f))
			return false;
		dir.Normalize();
		Matrix33	mat;
		mat.SetRotationVDir(dir, 0.0f);
		missAxisX = mat.GetColumn0();
		missAxisY = mat.GetColumn2();
	}
	else
	{
		// Project the target point on the silhouette plane.
		Vec3 targetPosOnSilhouettePlane = m_targetSilhouette.IntersectSilhouettePlane(GetFirePos(), targetPos);
		Vec3 targetPosOnSilhouettePlaneProj = m_targetSilhouette.ProjectVectorOnSilhouette(targetPosOnSilhouettePlane - m_targetSilhouette.center);

		Vec3 nearestPtOnSil;

		// Calculate the distance between the target pos and the silhouette.
		if (Overlap::Point_Polygon2D(targetPosOnSilhouettePlaneProj, m_targetSilhouette.points))
		{
			// Inside the polygon, zero dist.
			targetDistanceToSilhouette = 0.0f;
		}
		else
		{
			// Distance to the nearest edge.
			Vec3	pt;
			targetDistanceToSilhouette = Distance::Point_Polygon2D(targetPosOnSilhouettePlaneProj, m_targetSilhouette.points, pt);
			const Vec3& u = m_targetSilhouette.baseMtx.GetColumn0();
			const Vec3& v = m_targetSilhouette.baseMtx.GetColumn2();
			nearestPtOnSil = m_targetSilhouette.center + u * pt.x + v * pt.y;

			// Build coordinate system pointing away from the silhouette.
			missAxisY = targetPosOnSilhouettePlane - nearestPtOnSil;
			missAxisY.Normalize();
			const Vec3& axisZ = m_targetSilhouette.baseMtx.GetColumn1();
			missAxisX = axisZ.Cross(missAxisY);
		}
	}

	// If the aim target is outside the silhouette, choose random point around the silhouette to miss the target.
	if (!pLiveTargetActor || targetDistanceToSilhouette > 0.01f)
	{

//		GetAISystem()->AddDebugLine(targetPosOnSilhouettePlane, targetPosOnSilhouettePlane+axisX, 255,0,0, 2.0f);
//		GetAISystem()->AddDebugLine(targetPosOnSilhouettePlane, targetPosOnSilhouettePlane+axisY, 0,255,0, 2.0f);

		// Choose random point on outside the silhouette
		const float scaleDownDist = 10.0f;
		const float r = min((Distance::Point_Point(targetPos, GetFirePos()) / scaleDownDist) * extraRad, extraRad);
		float x = (2*ai_frand()-1) * r;
		float y = (2*ai_frand()-1) * r;
		// Clamp the point on silhouette.
		if (y > (targetDistanceToSilhouette - 0.01f))
			y = targetDistanceToSilhouette - 0.01f;

		posOut = targetPos + missAxisX*x + missAxisY*y;

		// Limit the maximum movement.
		if (maxMoveDelta > 0.0f && !m_targetLastMissPoint.IsZero())
		{
			Vec3	delta = posOut - m_targetLastMissPoint;
			float len = delta.NormalizeSafe();
			if (len > maxMoveDelta)
				posOut = m_targetLastMissPoint + maxMoveDelta * delta;
		}

		m_targetLastMissPoint = posOut;

		Vec3 aimDir = m_State.vAimTargetPos - GetFirePos();
		aimDir.Normalize();
	
		SAIBodyInfo bodyInfo;
		GetProxy()->QueryBodyInfo(bodyInfo);
		if ( bodyInfo.linkedVehicleEntity && bodyInfo.linkedVehicleEntity->GetAI() )
		{
			if ( aimDir.Dot( bodyInfo.vFireDir ) < cosf(clampAngle) )
				return false;
			aimDir = bodyInfo.vFireDir;
		}

		if (clampAngle > 0.0f)
			ClampPointInsideCone(GetFirePos(), aimDir, clampAngle, posOut);

		return IsFireTargetValid(posOut, pLiveTargetActor);
	}

	// Target inside the silhouette.

	// If it is allowed to hit, try to hit.
	if (CanDamageTarget() && pLiveTargetActor)
	{
		if (GetHitPointOnTarget(pLiveTargetActor, targetPos, posOut, clampAngle))
		{
			m_targetLastMissPoint = posOut;
			return true;
		}
	}

	// Choose miss point.
	// Choose the direction to miss based on the direction bias and spread.
	Vec3 targetBiasDirectionProj = m_targetSilhouette.ProjectVectorOnSilhouette(m_targetBiasDirection);
	targetBiasDirectionProj.NormalizeSafe(Vec3(0,0,0));
	float	reqAngle = atan2f(targetBiasDirectionProj.y, targetBiasDirectionProj.x);
	const float spread = DEG2RAD(60.0f - 50.0f * m_targetFocus);; //DEG2RAD(100.0f - 70.0f * m_targetFocus);
	reqAngle += (ai_frand() * 2.0f - 1.0f) * spread;

	// Allow to move more the more time has passed since last missed shot.
	float timeSinceLastMiss = (GetAISystem()->GetFrameStartTime() - m_targetLastMissPointSelectionTime).GetSeconds();
	float angleLimit = DEG2RAD(5.0f + clamp(timeSinceLastMiss / 2.0f, 0.0f, 1.0f) * 45.0f); // DEG2RAD(30.0f + Clamp(timeSinceLastMiss / 2.0f, 0.0f, 1.0f) * 45.0f);
	m_targetLastMissPointSelectionTime = GetAISystem()->GetFrameStartTime();

	// Limit the maximum delta between two succeeding miss points based on angle.
	if (!m_targetLastMissPoint.IsZero())
	{
		Vec3 deltaProj = m_targetSilhouette.ProjectVectorOnSilhouette(m_targetLastMissPoint - m_targetSilhouette.center);
		float lastMissAngle = atan2f(deltaProj.y, deltaProj.x);
		float	da = DeltaAngle(lastMissAngle, reqAngle);
		Limit(da, -angleLimit, angleLimit);
		reqAngle = lastMissAngle + da;
		if(reqAngle < 0.0f) reqAngle += gf_PI2;
	}

	// Find the point on the silhouette
	Vec3	intrPt;
	Vec3	sampleDir(cosf(reqAngle), sinf(reqAngle), 0);
	if (Intersect::Lineseg_Polygon2D(Lineseg(ZERO, sampleDir*100.0f), m_targetSilhouette.points, intrPt))
	{
		const Vec3& u = m_targetSilhouette.baseMtx.GetColumn0();
		const Vec3& v = m_targetSilhouette.baseMtx.GetColumn2();

		float	r = ai_frand() * extraRad;
		posOut = m_targetSilhouette.center + u * (intrPt.x + sampleDir.x*r) + v * (intrPt.y + sampleDir.y*r);

		m_targetLastMissPoint = posOut;

		Vec3 aimDir = (m_State.vAimTargetPos - GetFirePos()).GetNormalizedSafe();

		SAIBodyInfo bodyInfo;
		GetProxy()->QueryBodyInfo(bodyInfo);
		if ( bodyInfo.linkedVehicleEntity && bodyInfo.linkedVehicleEntity->GetAI() )
		{
			if ( aimDir.Dot( bodyInfo.vFireDir ) < cosf(clampAngle) )
				return false;
			aimDir = bodyInfo.vFireDir;
		}

		if (clampAngle > 0.0f)
			ClampPointInsideCone(GetFirePos(), aimDir, clampAngle, posOut);

		if (IsFireTargetValid(posOut, pLiveTargetActor))
		{
			m_targetEscapeLastMiss = clamp(m_targetEscapeLastMiss - 0.1f, 0.0f, 1.0f);
			return true;
		}
		// Try to steer away from the current miss location.
		m_targetEscapeLastMiss = clamp(m_targetEscapeLastMiss + 0.1f, 0.0f, 1.0f);
	}
	return false;
}

//====================================================================
// GetHitPointOnTarget
//====================================================================
bool CPuppet::GetHitPointOnTarget(CAIObject* pTarget, const Vec3& targetPos, Vec3& posOut, float clampAngle)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	if (!pTarget->IsAgent())
		return false;
	CAIActor* pTargetActor = pTarget->CastToCAIActor();
	if (!pTargetActor)
		return false;

	if (!pTargetActor->GetDamageParts())
		return false;

	const DamagePartVector&	parts = *pTargetActor->GetDamageParts();
	size_t n = parts.size();

	if (m_fireMode == FIREMODE_KILL)
	{
		float maxMult = 0.0f;
		for(int i = 0; i < n; ++i)
		{
			if (parts[i].damageMult > maxMult)
			{
				posOut = parts[i].pos;
				maxMult = parts[i].damageMult;
			}
		}
		return maxMult > 0.0f;
	}

	// Check that the indices are not out of bounds.
	for (unsigned i = 0, ni = m_targetHitPartIndex.size(); i < ni; ++i)
	{
		if (m_targetHitPartIndex[i] >= parts.size())
		{
			m_targetHitPartIndex.clear();
			break;
		}
	}

	// No indices left, create new set.
	if (m_targetHitPartIndex.empty())
	{
		for(int i = 0; i < n; ++i)
			m_targetHitPartIndex.push_back(i);
	}

	// Sort the damage parts based on the distance to current target.
	static std::vector<float> weights;
	weights.resize(m_targetHitPartIndex.size());
	Lineseg	LOF(GetFirePos(), targetPos);
	for(int i = 0, ni = m_targetHitPartIndex.size(); i < ni; ++i)
	{
		// Use negative weight since we use the last point first.
		float t;
		weights[i] = -Distance::Point_LinesegSq(parts[m_targetHitPartIndex[i]].pos, LOF, t);
	}
	std::sort(m_targetHitPartIndex.begin(), m_targetHitPartIndex.end(), SDamageLUTSorter(weights));

	if (!m_targetHitPartIndex.empty())
	{

		Vec3 aimDir = m_State.vAimTargetPos - GetFirePos();
		aimDir.Normalize();

		SAIBodyInfo bodyInfo;
		GetProxy()->QueryBodyInfo(bodyInfo);
		if ( bodyInfo.linkedVehicleEntity && bodyInfo.linkedVehicleEntity->GetAI() )
		{
			if ( aimDir.Dot( bodyInfo.vFireDir ) < cosf(clampAngle) )
				return false;
			aimDir = bodyInfo.vFireDir;
		}
		unsigned i = m_targetHitPartIndex.back();
	
		// don't change body part unless the shot was actually done
		if(m_pFireCmdHandler && m_LastShotsCount!=m_pFireCmdHandler->GetShotCount())
		{
			m_targetHitPartIndex.pop_back();
			m_LastShotsCount = m_pFireCmdHandler->GetShotCount();
		}

		posOut = parts[i].pos;
		// Small jitter
/*		const float jitterAmount = 0.1f;
		posOut.x += (ai_frand()*2-1) * jitterAmount;
		posOut.y += (ai_frand()*2-1) * jitterAmount;
		posOut.z += (ai_frand()*2-1) * jitterAmount;*/
		
		// Luciano: add prediction based on bullet and target speeds
		AdjustWithPrediction(pTargetActor, posOut);

		if (clampAngle > 0.0f)
			ClampPointInsideCone(GetFirePos(), aimDir, clampAngle, posOut);

		if (IsFireTargetValid(posOut, pTargetActor))
		{
//			GetAISystem()->AddDebugLine((GetFirePos()+posOut)*0.5f, posOut, 255,255,255, 5.0f);

//			m_targetHitPartInxdex.clear();
			return true;
		}

/*		if (!CheckFriendsInLineOfFire(posOut - GetFirePos(), false))
		{

			ray_hit hit;
			Vec3	dir = (posOut - GetFirePos());
			float dist = dir.GetLength();
			int res = gEnv->pPhysicalWorld->RayWorldIntersection(GetFirePos(), dir, COVER_OBJECT_TYPES, HIT_COVER,
				&hit, 1, skipList.empty() ? 0 : &skipList[0], skipList.size());

			if (!res || hit.dist > dist * 0.95f)
			{
				m_targetHitPartIndex.clear();
				return true;
			}
		}*/
	}

	//	m_targetHitPartIndex.clear();

	return false;
}

//====================================================================
// AdjustWithPrediction
//====================================================================
void CPuppet::AdjustWithPrediction(CAIObject* pTarget, Vec3& posOut)
{
	if (!pTarget) return;

	if	( GetSubType() == CAIObject::STP_2D_FLY )
		return;

	float sp = m_CurrentWeaponDescriptor.fSpeed;//bullet speed
	if(sp > 0)
	{	

		if(pTarget->GetPhysics())
		{
			pe_status_dynamics	dyn;
			pTarget->GetPhysics()->GetStatus(&dyn);

			Vec3 Q(GetFirePos());
			Vec3 X0(posOut - Q);
			Vec3 V(dyn.v);//target velocity
			// solve a 2nd degree equation in t = time at which bullet and target will collide given their velocities
			float x0v = X0.Dot(V);
			float v02 = V.GetLengthSquared();
			float w02 = sp*sp;
			float x02 = X0.GetLengthSquared();
			float b = x0v;
			float sq = x0v*x0v - x02*(v02 - w02);
			if(sq<0)// bullet can't reach the target
				return;
			sq = sqrt(sq);
			float d = (v02 - w02);
			float t = (-b+sq)/d;
			float t1 = (-b-sq)/d;
			if(t<0 && t1>0 || t1>0 && t1<t)
				t = t1;
			if(t<0)
				return;
			Vec3 W(X0/t + V);//bullet velocity
	
			posOut = Q+W*t;
			//GetAISystem()->AddDebugLine(Q,posOut,255,255,255,3);
		}
	}
}

//====================================================================
// IsFireTargetValid
//====================================================================
bool CPuppet::IsFireTargetValid(const Vec3& targetPos, const CAIActor* pTargetActor)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_AI);

	// Check only friendly units if drawing fire or if at update low priority (usually means not visible to player).
	if (m_fireMode == FIREMODE_BURST_DRAWFIRE || (m_updatePriority == AIPUP_LOW || m_updatePriority == AIPUP_MED))
	{
		if (!CheckFriendsInLineOfFire(targetPos - GetFirePos(), false))
			return true;
		return false;
	}

	// Accept the point if:
	// 1) shooting at the direction hits something relatively far away
	// 2) there is no friendly units between the target and the shooter
	ray_hit hit;
	const Vec3& firePos = GetFirePos();
	Vec3 dir = targetPos - firePos;
	float	len = dir.NormalizeSafe();

	const float minCheckDist = 2.0f;
	const float maxCheckDist = 20.0f;
	const float reqTravelDistPercent = 0.25f;
	const float reqDist = clamp(len * reqTravelDistPercent, minCheckDist, maxCheckDist);

	float dist = m_fireTargetCache.QueryCachedResult(firePos, dir, reqDist);

	if (dist < 0.0f)
	{
		// Could not find cached result, need raycast.
		std::vector<IPhysicalEntity*> skipList;
		GetPhysicsEntitiesToSkip(skipList);
		if (pTargetActor)
			pTargetActor->GetPhysicsEntitiesToSkip(skipList);

		Vec3	delta = dir * reqDist;

		if (gEnv->pPhysicalWorld->RayWorldIntersection(firePos, delta, COVER_OBJECT_TYPES, HIT_COVER,
			&hit, 1, skipList.empty() ? 0 : &skipList[0], skipList.size()))
		{
			dist = hit.dist;
		}
		else
		{
			dist = FLT_MAX;
		}

		m_fireTargetCache.Insert(firePos, dir, reqDist, dist);
	}

	if (dist > reqDist)
	{
		if (!CheckFriendsInLineOfFire(targetPos - firePos, false))
			return true;
	}

	return false;
}


//====================================================================
// ChooseMissPoint
//====================================================================
Vec3 CPuppet::ChooseMissPoint_Deprecated(const Vec3 &vTargetPos) const
{
	int	trysLimit(5);	// how many times try to get good point to shoot

	CAIObject* pTarget = m_pAttentionTarget ? m_pAttentionTarget : m_pLastOpResult;
	Vec3 dir(vTargetPos - GetFirePos());
	float     distToTarget = dir.len();
	if(distToTarget > 0.00001f)
		dir /= distToTarget;

	Matrix33 mat(Matrix33::CreateRotationXYZ(Ang3::GetAnglesXYZ(Quat::CreateRotationVDir(dir))));
	Vec3 right(mat.GetColumn(0));
	//          Vec3 up(b3DTarget ? mat.GetColumn(2).normalize(): ZERO);
	Vec3 up(mat.GetColumn(2));
	float     spreadHoriz(0), spreadVert(0);

	//Vec3 candidateShootPos(vTargetPos + 1.2f * right + .2f * up);
	//return candidateShootPos;


	//Vec3 candidateShootPos(vTargetPos + 1.2f * right + .2f * up);
	//return candidateShootPos;


	while(--trysLimit>=0)
	{
		//miss randomly on left/right
		spreadHoriz = (0.9f + Random(0.3f)) * (Random(100)<50 ? -1.0f : 1.0f);
		//miss randomly on up/dowm - only in 3D
		//		spreadVert = (1.2f + Random(0.3f)) * (Random(100)<50 ? -1.0f : 1.0f);
		//miss randomly on up/dowm
		spreadVert = (Random(1.5f)) * (Random(100)<50 ? -1.0f : .35f);
		//		float     maxOffset(tanf(DEG2RAD(15.0f)) * distToTarget);
		//		spreadHoriz = Clamp(spreadHoriz, -maxOffset, maxOffset);
		//		spreadVert = Clamp(spreadVert, -maxOffset, maxOffset);
		Vec3 candidateShootPos(vTargetPos + spreadHoriz * right + spreadVert * up);
		if(m_pFireCmdHandler->ValidateFireDirection(candidateShootPos - GetFirePos(), false))
			return candidateShootPos;
	}
	// can't find any good point to miss
	return ZERO;
}

// 
//----------------------------------------------------------------------------------------------------
bool CPuppet::CheckAndGetFireTarget_Deprecated(IAIObject* pTarget, bool lowDamage, Vec3 &vTargetPos, Vec3 &vTargetDir) const
{
	TICK_COUNTER_FUNCTION

		CAIObject* pTargetAI = static_cast<CAIObject*>(pTarget);

	if(!m_pFireCmdHandler)
		return false;

	CAIActor *pTargetActor = pTarget->CastToCAIActor();
	if(pTargetActor && pTargetActor->GetDamageParts())
	{
		DamagePartVector&	parts = *(pTargetActor->GetDamageParts());
		std::vector<float>	weights;
		std::vector<int>	partLut;

		// Check if the parts have multipliers set up.
		float	accMult = 0.0f;
		int	n = (int)parts.size();
		for(int i = 0; i < n; ++i)
			accMult += parts[i].damageMult;

		weights.resize(n);

		if(accMult > 0.001f)
		{
			// Sort based on damage multiplier.
			// Add slight random scaling to randomize selection of objects of same multiplier.
			for(int i = 0; i < n; ++i)
			{
				weights[i] = parts[i].damageMult * (1.0f + ai_frand() * 0.01f);
				if(lowDamage && parts[i].damageMult > 0.95f) continue;
				partLut.push_back(i);
			}
		}
		else
		{
			// Sort based on part volume.
			// Add slight random scaling to randomize selection of objects of same multiplier.
			for(int i = 0; i < n; ++i)
			{
				// Sort from smallest volume to largest, hence negative sign.
				weights[i] = -parts[i].volume * (1.0f + ai_frand() * 0.01f);
				partLut.push_back(i);
			}
		}

		std::sort(partLut.begin(), partLut.end(), SDamageLUTSorter(weights));

		for(std::vector<int>::iterator it = partLut.begin(); it != partLut.end(); ++it)
		{
			vTargetPos = parts[*it].pos;
			vTargetDir = vTargetPos - GetFirePos();
			if(m_pFireCmdHandler->ValidateFireDirection(vTargetDir, true))
				return true;
		}
	}
	else
	{
		// Inanimate target, use the default position.
		vTargetPos = pTarget->GetPos();
		vTargetDir = vTargetPos - GetFirePos();

		// The head is accessible.
		if(m_pFireCmdHandler->ValidateFireDirection(vTargetDir, false))
			return true;
	}

	// TODO: Should probably use one of those missed points instead, since they cannot reach the target anyway.
	vTargetPos = ChooseMissPoint_Deprecated(vTargetPos);

	return true;
}
