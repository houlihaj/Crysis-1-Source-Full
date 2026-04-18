// defines a spacial target for exact positioning, with operations we need to perform on these things

#ifndef __ANIMATIONTRIGGER_H__
#define __ANIMATIONTRIGGER_H__

#pragma once

class CAnimationTrigger
{
public:
	CAnimationTrigger();
	CAnimationTrigger( const Vec3& pos, const Vec3& posRadius, const Quat& orient, float orientRadius, float optimizeTime, float animMovementLength );

	void Update( float frameTime, Vec3 userPos, Quat userOrient, bool allowTriggering );
	void DebugDraw();

	bool IsReached() const { return m_state >= eS_Optimizing; }
	bool IsTriggered() const { return m_state >= eS_Triggered; }

	void ResetRadius( const Vec3& posRadius, float orientRadius );
	void SetMinMaxSpeed( const Vec2& speed );
	void SetDistanceErrorFactor( float factor ) { m_distanceErrorFactor = factor; }

private:
	enum EState
	{
		eS_Invalid,
		eS_Initializing,
		eS_Before,
		eS_Optimizing,
		eS_Triggered
	};

	Vec3 m_pos;
	Vec3 m_posRadius;
	Quat m_orient;
	float m_orientRadius;
	float m_optimizeTime;
	float m_optimizeTimeUsed;
	float m_sideTime;
	float m_distanceErrorFactor;

	float m_animMovementLength;
	float m_distanceError;
	float m_orientError;

	EState m_state;

	Vec3 m_userPos;
	Quat m_userOrient;
};

#endif
