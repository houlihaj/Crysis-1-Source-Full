/********************************************************************
	Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   PathMarker.h

  Description: CPathMarker contains the history of a given moving point (i.e. an entity position)
	and can return a previous position (interpolated) given the distance from the current point 
	computed along the path

  
 -------------------------------------------------------------------------
  History:
  - 17:11:2004   14:23 : Created by Luciano Morpurgo

*********************************************************************/
#ifndef __PathMarker_H__
#define __PathMarker_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include <IAgent.h>
#include <vector>

struct IRenderer;

struct CSteeringDebugInfo
{
	std::vector<Vec3> pts;
	Vec3 segStart, segEnd;
};

typedef struct {Vec3 vPoint;float fDistance;bool bPassed;} pathStep_t;

class CPathMarker
{
private:
	std::vector<pathStep_t> m_cBuffer; //using vector instead of deque as a FIFO, optimizing memory allocation and speed
	float m_fStep;
	int m_iCurrentPoint;
	int m_iSize;
	int m_iUsed;
	float m_fTotalDistanceRun;
public:
	//static const int C_MAX_DIM = 40;
	//CPathMarker();
	CPathMarker(float fMaxDistanceNeeded, float fStep);
	void	Init(const Vec3& vInitPoint, const Vec3& vEndPoint);
	Vec3	GetPointAtDistance(const Vec3& vNewPoint, float fDistance ) const;
	Vec3	GetDirectionAtDistance(const Vec3& vTargetPoint, float fDesiredDistance) const;
	Vec3	GetMoveDirectionAtDistance(Vec3& vTargetPoint, float fDesiredDistance, 
																	 const Vec3 &vUserPos, float falloff, 
																	 float * alignmentWithPath,
																	 CSteeringDebugInfo * debugInfo);
	float	GetDistanceToPoint(Vec3& vTargetPoint, Vec3& vMyPoint);
	inline float GetTotalDistanceRun() {return m_fTotalDistanceRun;}
	void	Update(const Vec3& vNewPoint, bool b2D = false);
	size_t	GetPointCount() const { return m_cBuffer.size(); }
	void DebugDraw(IRenderer* pRenderer);
	void Serialize(TSerialize ser, CObjectTracker& objectTracker);

};

#endif __PathMarker_H__
