/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2007.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  helper for refering to either entities or statobj physical entities
 -------------------------------------------------------------------------
 History:
 - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "ObjectSelector.h"

CObjectSelector::CObjectSelector()
{
	m_selType = eST_Null;
	m_objCenter = ZERO;
}

CObjectSelector::CObjectSelector(IPhysicalEntity * pEnt)
{
	m_drawDistance = 0;
	if (!pEnt)
	{
		m_selType = eST_Null;
		m_objCenter = ZERO;
	}
	else if (pEnt->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
	{
		IRenderNode * pNode = (IRenderNode*) pEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC);
		IStatObj * pStatObj = pNode->GetEntityStatObj(0);
		phys_geometry * pPhysGeom = pStatObj? pStatObj->GetPhysGeom() : 0;
		m_objPos = pNode->GetPos();
		m_objCenter = pNode->GetBBox().GetCenter();
		m_objVolume = pPhysGeom? pPhysGeom->V : 0;
		m_selType = eST_StatObj;
		m_drawDistance = pNode->GetMaxViewDist();
	}
	else if (pEnt->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity * pEntity = (IEntity*)pEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		if (pEntity)
		{
			m_selType = eST_Entity;
			m_entity = pEntity->GetId();
			AABB aabb;
			pEntity->GetWorldBounds(aabb);
			m_objCenter = aabb.GetCenter();
			FillDrawDistance(pEntity);
		}
		else
		{
			assert(false);
			m_selType = eST_Null;
			m_objCenter = ZERO;
		}
	}
	else
	{
		assert(false);
		m_selType = eST_Null;
		m_objCenter = ZERO;
	}

//	assert(Find() == pEnt);
}

bool CObjectSelector::operator <(const CObjectSelector& sel) const
{
	if (m_selType < sel.m_selType)
		return true;
	else if (m_selType > sel.m_selType)
		return false;

	switch (m_selType)
	{
	default:
		assert(false);
	case eST_Null:
		return false;
	case eST_Entity:
		return m_entity < sel.m_entity;
	case eST_StatObj:
#define LT_EL(xxx) else if (xxx < sel.xxx) return true; else if (xxx > sel.xxx) return false
#define LT_EL_VEC(xxx) LT_EL(xxx.x); LT_EL(xxx.y); LT_EL(xxx.z)
#define LT_EL_QUAT(xxx) LT_EL(xxx.w); LT_EL_VEC(xxx.v)
		if (false);
		LT_EL_VEC(m_objCenter);
		LT_EL_VEC(m_objPos);
		LT_EL(m_objVolume);
		else return false;
#undef LT_EL
#undef LT_EL_VEC
#undef LT_EL_QUAT
	}
	return false;
}

void CObjectSelector::GetPositionInfo( SMessagePositionInfo& pos )
{
	pos.havePosition = !m_objCenter.IsZero();
	pos.position = m_objCenter;
	pos.haveDrawDistance = m_drawDistance > 0;
	pos.drawDistance = m_drawDistance;
}

IPhysicalEntity * CObjectSelector::Find() const
{
	switch (m_selType)
	{
	case eST_Null:
	default:
		break;

	case eST_Entity:
		if (IEntity * pEnt = gEnv->pEntitySystem->GetEntity(m_entity))
			return pEnt->GetPhysics();
		break;

	case eST_StatObj:
		{
			IPhysicalEntity **pents;
			phys_geometry *pPhysGeom;
			IRenderNode *pVeg;

			int j = gEnv->pPhysicalWorld->GetEntitiesInBox(m_objCenter-Vec3(0.01f),m_objCenter+Vec3(0.01f),pents,ent_static);
			for(--j;j>=0;j--) if (
				(pVeg = (IRenderNode*)pents[j]->GetForeignData(PHYS_FOREIGN_ID_STATIC)) && 
				(pVeg->GetPos()-m_objPos).len2()<sqr(0.03f) && 
				(m_objVolume==0 || pVeg->GetEntityStatObj(0) && (pPhysGeom=pVeg->GetEntityStatObj(0)->GetPhysGeom()) && 
				fabs_tpl(pPhysGeom->V-m_objVolume)<min(pPhysGeom->V,m_objVolume)*1e-4f))
				break;
			if (j>=0)
				return pVeg->GetPhysics();
		}
		break;
	}

	return 0;
}

void CObjectSelector::SerializeWith(TSerialize ser)
{
	ser.EnumValue("type", m_selType, eST_Null, eST_NUM_TYPES);
	switch (m_selType)
	{
	case eST_Null:
		break;
	case eST_Entity:
		ser.Value("entity", m_entity, 'eid');
		break;
	case eST_StatObj:
		ser.Value("center", m_objCenter);
		ser.Value("pos", m_objPos);
		ser.Value("volume", m_objVolume);
		break;
	}
}

string CObjectSelector::GetDescription()
{
	switch (m_selType)
	{
	case eST_Null:
		return "null-selector";
	case eST_Entity:
		if (IEntity * pEnt = gEnv->pEntitySystem->GetEntity(m_entity))
			return string().Format("entity[%.8x:%s]", m_entity, pEnt->GetName());
		else
			return string().Format("unbound-entity[%.8x]", m_entity);
	case eST_StatObj:
		return string().Format("statobj[pos=(%.8f,%.8f,%.8f),center=(%.8f,%.8f,%.8f),vol=%.8f", m_objPos.x, m_objPos.y, m_objPos.z, m_objCenter.x, m_objCenter.y, m_objCenter.z, m_objVolume);
	default:
		assert(false);
		return "invalid-selector";
	}
}
