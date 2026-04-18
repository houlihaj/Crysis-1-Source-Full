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
#ifndef __OBJECTSELECTOR_H__
#define __OBJECTSELECTOR_H__

#pragma once

class CObjectSelector
{
public:
	CObjectSelector();
	CObjectSelector(IPhysicalEntity*);
	CObjectSelector(const Vec3& pos, const Vec3& center, float volume)
	{
		m_selType = eST_StatObj;
		m_objCenter = center;
		m_objPos = pos;
		m_objVolume = volume;
		m_drawDistance = 0;
	}
	CObjectSelector(EntityId ent)
	{
		if (ent)
		{
			m_selType = eST_Entity;
			m_entity = ent;
			m_objCenter = ZERO;
			m_drawDistance = 0;
		}
		else
		{
			m_selType = eST_Null;
			m_objCenter = ZERO;
		}
	}
	CObjectSelector(IEntity * pEnt)
	{
		m_drawDistance = 0;
		if (!pEnt)
		{
			m_selType = eST_Null;
			m_objCenter = ZERO;
		}
		else
		{
			m_selType = eST_Entity;
			m_entity = pEnt->GetId();
			m_objCenter = pEnt->GetWorldPos();
			FillDrawDistance(pEnt);
		}
	}
	void GetPositionInfo( SMessagePositionInfo& pos );
	IPhysicalEntity * Find() const;
	void SerializeWith(TSerialize ser);

	string GetDescription();

	bool operator<(const CObjectSelector& sel) const;

	bool operator==( const CObjectSelector& sel ) const
	{
		if (*this < sel)
			return false;
		else if (sel < *this)
			return false;
		return true;
	}

private:
	enum ESelType
	{
		eST_Null, // must be first
		eST_Entity,
		eST_StatObj,
		eST_NUM_TYPES // must be last
	};
	ESelType m_selType;

	EntityId m_entity;
	Vec3 m_objCenter;
	Vec3 m_objPos;
	float m_objVolume;
	float m_drawDistance;

	void FillDrawDistance( IEntity * pEnt )
	{
		if (IEntityRenderProxy * pRP = (IEntityRenderProxy*)pEnt->GetProxy(ENTITY_PROXY_RENDER))
			if (IRenderNode * pRN = pRP->GetRenderNode())
				m_drawDistance = pRN->GetMaxViewDist();
	}
};

#endif
