/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2007.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  network breakability: state of an object pre-explosion
 -------------------------------------------------------------------------
 History:
 - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"
#include "ExplosiveObjectState.h"

bool SExplosiveObjectState::operator<(const SExplosiveObjectState& s) const
{
#define LT_EL(xxx) else if (xxx < s.xxx) return true; else if (xxx > s.xxx) return false
#define LT_EL_VEC(xxx) LT_EL(xxx.x); LT_EL(xxx.y); LT_EL(xxx.z)
#define LT_EL_QUAT(xxx) LT_EL(xxx.w); LT_EL_VEC(xxx.v)
	if (isEnt < s.isEnt)
		return true;
	else if (isEnt > s.isEnt)
		return false;
	else if (isEnt)
	{
		if (false);
		LT_EL(entId);
		else return false;
	}
	else
	{
		if (false);
		LT_EL_VEC(objCenter);
		LT_EL_VEC(objPos);
		LT_EL(objVolume);
		else return false;
	}
	return false;
#undef LT_EL
#undef LT_EL_VEC
#undef LT_EL_QUAT
}

#include "CryAction.h"
#include "GameContext.h"

void SDeclareExplosiveObjectState::SerializeWith( TSerialize ser )
{
	ser.Value("breakId", breakId, 'brId');
	ser.Value("isEnt", isEnt);
	if (isEnt)
	{
		if (ser.IsWriting())
			assert(CCryAction::GetCryAction()->GetGameContext()->GetNetContext()->IsBound(entId));
		ser.Value("entid", entId, 'eid');
		ser.Value("entpos", entPos);
		ser.Value("entrot", entRot);
		ser.Value("entscale", entScale);
	}
	else
	{
		ser.Value("objpos", objPos);
		ser.Value("objcenter", objCenter);
		ser.Value("objvolume", objVolume);
	}
}

bool ExplosiveObjectStateFromPhysicalEntity( SExplosiveObjectState& s, IPhysicalEntity * pPEnt )
{
	bool ok = false;
	switch (pPEnt->GetiForeignData()) 
	{
	case PHYS_FOREIGN_ID_ENTITY:
		if (IEntity * pEnt = (IEntity *) pPEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
		{
			s.isEnt = true;
			s.entId = pEnt->GetId();
			s.entPos = pEnt->GetWorldPos();
			s.entRot = pEnt->GetWorldRotation();
			s.entScale = pEnt->GetScale();
			ok = true;
		}
		break;
	case PHYS_FOREIGN_ID_STATIC:
		if (IRenderNode * rn = (IRenderNode*)pPEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC))
		{
			s.isEnt = false;
			s.objPos = rn->GetPos();
			s.objCenter = rn->GetBBox().GetCenter();
			s.objVolume = 0;
			if (IStatObj * pStatObj = rn->GetEntityStatObj(0))
				if (phys_geometry * pPhysGeom = pStatObj->GetPhysGeom())
					s.objVolume = pPhysGeom->V;
			ok = true;
		}
		break;
	}
	return ok;
}
