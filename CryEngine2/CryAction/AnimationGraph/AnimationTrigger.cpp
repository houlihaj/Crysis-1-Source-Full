#include "StdAfx.h"
#include "AnimationTrigger.h"
#include "AnimationGraphCVars.h"

#include "PersistantDebug.h"

CAnimationTrigger::CAnimationTrigger() : m_state(eS_Invalid)
{
}

CAnimationTrigger::CAnimationTrigger( const Vec3& pos, const Vec3& posRadius, const Quat& orient, float orientRadius, float optimizeTime, float animMovementLength )
	: m_pos(pos)
	, m_userPos(pos)
	, m_posRadius(posRadius)
	, m_orient(orient)
	, m_userOrient(orient)
	, m_orientRadius(orientRadius)
	, m_optimizeTime(optimizeTime)
	, m_optimizeTimeUsed(0.0f)
	, m_state(eS_Initializing)
	, m_sideTime(0)
	, m_distanceError(1000.0f)
	, m_orientError(1000.0f)
	, m_distanceErrorFactor(1.0f)
{
	assert(m_pos.IsValid());
	assert(m_userPos.IsValid());
	assert(m_orient.IsValid());
	assert(m_userOrient.IsValid());
	assert(m_posRadius.IsValid());
	assert(NumberValid(m_orientRadius));
	assert(NumberValid(m_optimizeTime));

	// Make sure we have bigger than zero length, 
	// so that the error measurement is not zero no matter what direction.
	m_animMovementLength = max(animMovementLength, 1.0f);

	// Make sure the trigger is not rotated, only Z-axis rotations are allowed
	m_orient.v.x = m_orient.v.y = 0.0f; m_orient.Normalize();
	m_userOrient = m_orient;
}

void CAnimationTrigger::Update( float frameTime, Vec3 userPos, Quat userOrient, bool allowTriggering )
{
	if (m_state == eS_Invalid)
		return;

	assert(m_pos.IsValid());
	assert(m_userPos.IsValid());
	assert(m_orient.IsValid());
	assert(m_userOrient.IsValid());
	assert(m_posRadius.IsValid());
	assert(NumberValid(m_orientRadius));
	assert(NumberValid(m_optimizeTime));

	assert(NumberValid(frameTime));
	assert(userPos.IsValid());
	assert(userOrient.IsValid());

	m_userPos = userPos;
	m_userOrient = userOrient;

	if (m_state == eS_Initializing)
		m_state = eS_Before;

	Plane threshold;
	threshold.SetPlane( m_orient.GetColumn1(), m_pos );
	if (threshold.DistFromPlane(userPos) >= 0.0f)
	{
		if (m_sideTime < 0.0f)
			m_sideTime = 0.0f;
		else
			m_sideTime += frameTime;
	}
	else
	{
		if (m_sideTime > 0.0f)
			m_sideTime = 0.0f;
		else
			m_sideTime -= frameTime;
	}

	Vec3 curDir = userOrient.GetColumn1();
	Vec3 wantDir = m_orient.GetColumn1();

	if (m_state == eS_Before)
	{
		OBB triggerBox;
		triggerBox.SetOBB( Matrix33(m_orient), m_posRadius, ZERO );
		if (Overlap::Point_OBB(m_userPos, m_pos, triggerBox))
			m_state = eS_Optimizing;
	}

	if ((m_state == eS_Optimizing) && allowTriggering)
	{
		m_optimizeTimeUsed += frameTime;

/*
		// This caused premature triggering/popping.
		if (m_optimizeTimeUsed > m_optimizeTime)
			m_state = eS_Triggered;
		else if (m_sideTime >= 0.01f)
			m_state = eS_Triggered;
*/

		bool debug = (CAnimationGraphCVars::Get().m_debugExactPos != 0);
		CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();

		Vec3 bump(0.0f, 0.0f, 0.1f);

		Vec3 posDistanceError = m_userPos - m_pos;
		if ( posDistanceError.z > -1.0f && posDistanceError.z < 1.0f )
			posDistanceError.z = 0;

		Vec3 orientFwd = m_orient.GetColumn1(); orientFwd.z = 0.0f; orientFwd.Normalize();
		Vec3 rotAnimMovementWanted = orientFwd * m_animMovementLength;
		Vec3 rotAnimMovementUser = m_userOrient.GetColumn1() * m_animMovementLength;
		Vec3 rotDistanceError = rotAnimMovementUser - rotAnimMovementWanted;

		float distanceError = posDistanceError.len() * m_distanceErrorFactor;
		float orientError = rotDistanceError.len();
		float totalDistanceError = distanceError + orientError;
		if (((m_distanceError * 1.1f) < distanceError) && ((m_orientError * 1.1f) < orientError) ||
			(totalDistanceError < 0.1f))
		{ // found local minimum in distance error, force triggering.
			m_state = eS_Triggered;

			if (debug)
			{
				pPD->Begin("AnimationTrigger LocalMinima Triggered", false);
				pPD->AddPlanarDisc(m_pos + bump, 0.0f, m_distanceError, ColorF(0,1,0,0.5), 10.0f);
			}
		}
		else
		{
			m_distanceError = m_distanceError > distanceError ? distanceError : m_distanceError * 0.999f; // should timeout in ~2 secs. on 50 FPS
			m_orientError = m_orientError > orientError ? orientError : m_orientError - 0.0001;

			if (debug)
			{
				pPD->Begin("AnimationTrigger LocalMinima Optimizing", true);
				pPD->AddPlanarDisc(m_pos + bump, 0.0f, m_distanceError, ColorF(1,1,0,0.5), 10.0f);
			}
		}

		if (debug)
		{
			pPD->AddLine(m_userPos + bump, m_pos + bump, ColorF(1,0,0,1), 10.0f);
			pPD->AddLine(m_userPos + rotAnimMovementUser + bump, m_pos + rotAnimMovementWanted + bump, ColorF(1,0,0,1), 10.0f);
			pPD->AddLine(m_pos + bump, m_pos + rotAnimMovementWanted + bump, ColorF(1,0.5,0,1), 10.0f);
			pPD->AddLine(m_userPos + bump, m_pos + rotAnimMovementUser + bump, ColorF(1,0.5,0,1), 10.0f);
		}
	}

	assert(m_pos.IsValid());
	assert(m_userPos.IsValid());
	assert(m_orient.IsValid());
	assert(m_userOrient.IsValid());
	assert(m_posRadius.IsValid());
	assert(NumberValid(m_orientRadius));
	assert(NumberValid(m_optimizeTime));
}

void CAnimationTrigger::DebugDraw()
{
	if (m_state == eS_Invalid)
		return;

	assert(m_pos.IsValid());
	assert(m_userPos.IsValid());
	assert(m_orient.IsValid());
	assert(m_userOrient.IsValid());
	assert(m_posRadius.IsValid());
	assert(NumberValid(m_orientRadius));
	assert(NumberValid(m_optimizeTime));

	OBB triggerBox;
	triggerBox.SetOBB( Matrix33(m_orient), m_posRadius, ZERO );

	ColorF clr(1,0,0,1);

	Vec3 curDir = m_userOrient.GetColumn1();
	Vec3 wantDir = m_orient.GetColumn1();
	float angle = cry_acosf(curDir.Dot(wantDir));

	bool inAngleRange = false;

	switch (m_state)
	{
	case eS_Initializing:
		clr = ColorF(1,0,0,0.3f);
		break;
	case eS_Before:
		inAngleRange = angle < 2.0f * m_orientRadius;
		clr = ColorF(1,0,1,0.3f);
		break;
	case eS_Optimizing:
		inAngleRange = angle < m_orientRadius;
		clr = ColorF(1,1,0,0.3f + 0.7f * (m_optimizeTimeUsed / m_optimizeTime));
		break;
	case eS_Triggered:
		inAngleRange = true;
		clr = ColorF(1,0,1,0.1f);
		break;
	}

	gEnv->pRenderer->GetIRenderAuxGeom()->DrawOBB( triggerBox, Matrix34::CreateTranslationMat(m_pos), true, clr, eBBD_Faceted );

	clr = ColorF(0,1,0,1);
	if (inAngleRange)
		clr = ColorF(1,0,0,1);
	static const int NUM_STEPS = 20;
	for (int i=0; i<NUM_STEPS; i++)
	{
		float t = float(i)/float(NUM_STEPS-1);
		Vec3 dir = Quat::CreateSlerp( m_userOrient, m_orient, t ).GetColumn1();
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( m_pos, clr, m_pos + 4.0f * dir, clr );
	}
}

void CAnimationTrigger::ResetRadius( const Vec3& posRadius, float orientRadius )
{
	assert(m_pos.IsValid());
	assert(m_userPos.IsValid());
	assert(m_orient.IsValid());
	assert(m_userOrient.IsValid());
	assert(m_posRadius.IsValid());
	assert(NumberValid(m_orientRadius));
	assert(NumberValid(m_optimizeTime));

	if (m_state == eS_Invalid)
		return;
	m_state = eS_Initializing;
	m_posRadius = posRadius;
	m_orientRadius = orientRadius;

	// TODO: Update should not be called here. Maybe it's only used to set some values or something, not really an update.
	Update( 0.0f, m_userPos, m_userOrient, false );
}

void CAnimationTrigger::SetMinMaxSpeed( const Vec2& speed )
{
	if (m_state == eS_Invalid)
		return;

	assert(m_pos.IsValid());
	assert(m_userPos.IsValid());
	assert(m_orient.IsValid());
	assert(m_userOrient.IsValid());
	assert(m_posRadius.IsValid());
	assert(NumberValid(m_orientRadius));
	assert(NumberValid(m_optimizeTime));

	m_optimizeTime = 0.5f * std::min( std::min(m_posRadius.x, m_posRadius.y), m_posRadius.z ) / std::max(1.0f, speed[1]);
}
