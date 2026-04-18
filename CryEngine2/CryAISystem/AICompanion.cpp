

#include "StdAfx.h"
#include <Cry_Camera.h>
#include "AIPlayer.h"
#include <ISystem.h>
#include "CAISystem.h"
#include "LeaderAction.h"

//////////////////////////////////////////////////////////////////////////
bool CLeaderAction::IsUnitTooFar(const CUnitImg &tUnit,const Vec3 &vPos) const
{
	// TODO: check if it was blocked by something or was leading the player
	// through the map.

	if (!tUnit.m_pUnit->IsEnabled())
		return (false);

	const float fMaxDist=15.0f;
	if ((tUnit.m_pUnit->GetPos() - vPos).GetLengthSquared() > (fMaxDist*fMaxDist))
		return (true);	

	return (false);
}
