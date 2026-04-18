/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2007.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  network breakability: generic 'remove parts' events - try not to use
 -------------------------------------------------------------------------
 History:
 - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"
#include "SimulateRemoveEntityParts.h"
#include "BreakReplicator.h"

void SSimulateRemoveEntityParts::SerializeWith( TSerialize ser )
{
	ent.SerializeWith(ser);
	ser.Value("partIds0", partIds[0]);
	ser.Value("partIds1", partIds[1]);
	ser.Value("partIds2", partIds[2]);
	ser.Value("partIds3", partIds[3]);
	ser.Value("massOrg", massOrg);
}

void SSimulateRemoveEntityPartsMessage::SerializeWith( TSerialize ser )
{
	SSimulateRemoveEntityParts::SerializeWith(ser);
	ser.Value("breakId", breakId, 'brId');
}

void SSimulateRemoveEntityPartsInfo::GetAffectedRegion(AABB& aabb)
{
	SMessagePositionInfo center;
	ent.GetPositionInfo(center);
	aabb.min = center.position - Vec3(20);
	aabb.max = center.position + Vec3(20);
}

void SSimulateRemoveEntityPartsInfo::AddSendables(INetSendableSink * pSink, int32 brkId)
{
	CBreakReplicator::SendSimulateRemoveEntityPartsWith( SSimulateRemoveEntityPartsMessage(*this, brkId), pSink );
}
