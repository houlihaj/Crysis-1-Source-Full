/********************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   ObstacleAllocator.h
$Id$
Description: 

-------------------------------------------------------------------------
History:
- 2:5:2006   16:22 : Created by Kirill Bulatsev

*********************************************************************/
#ifndef __AIHIdeObject_H__
#define __AIHIdeObject_H__

#if _MSC_VER > 1000
#pragma once
#endif

#include "SmartObjects.h"
#include <list>

class CAIObject;
struct ObstacleData;

enum ECoverUsage
{
	USECOVER_NONE,
	USECOVER_SMARTOBJECT_HIDE,
	USECOVER_SMARTOBJECT_UNHIDE,
	USECOVER_STRAFE_LEFT_STANDING,
	USECOVER_STRAFE_RIGHT_STANDING,
	USECOVER_STRAFE_TOP_STANDING,
	USECOVER_STRAFE_TOP_LEFT_STANDING,
	USECOVER_STRAFE_TOP_RIGHT_STANDING,
	USECOVER_STRAFE_LEFT_CROUCHED,
	USECOVER_STRAFE_RIGHT_CROUCHED,
	USECOVER_CENTER_CROUCHED,
};

class CAIHideObject
{
friend class CPipeUser;
public:
	CAIHideObject();
  ~CAIHideObject();

	void	Set(const struct SHideSpot *hs, const Vec3& hidePos, const Vec3& hideDir);

	void	Invalidate() { m_bIsValid = false; }
	bool	IsValid();
	bool	IsCompromised(const CPipeUser* requester, const Vec3& targetPos);
	bool	IsNearCover(const CPipeUser* requester) const;
	bool	IsSmartObject() const { return m_bIsSmartObject; }
	CQueryEvent& GetSmartObject() { return m_HideSmartObject; }
	void	SetSmartObject(const CQueryEvent& smObject) { m_HideSmartObject=smObject; }
	void	ClearSmartObject() {m_HideSmartObject.Clear();}
	// Returns the flag indicating if the agent is using the cover.
	bool IsUsingCover() const { return m_isUsingCover;}
	// Sets the flag indicating if the agent is using the cover.
	void SetUsingCover(bool state) { m_isUsingCover = state; }

	int GetCoverUsage() const { return m_useCover;}
	void SetCoverUsage(int type) { m_useCover = type; }

	// Returns unique ID which is updated each time the Set method succeeds. 0 is invalid ID.
	unsigned int	GetCoverId() const { return m_id; }

	// Returns the radius of the hide object (not valid for smart objects).
	float	GetObjectRadius() const { return m_objectRadius; }
	// Returns the position of the hide object  (not valid for smart objects).
	const Vec3&	GetObjectPos() const { return m_objectPos; }
	// Returns the direction of the hide object  (not valid for smart objects).
	const Vec3&	GetObjectDir() const { return m_objectDir; }
	// Returns true if 
	bool	IsObjectCollidable() const { return m_objectCollidable; }
	// Returns the position behind the hide object.
	const Vec3&	GetLastHidePos() const { return m_vLastHidePos; }

	void	Update(CPipeUser *pOperand);

	bool	HasLowCover() const;
	bool	HasHighCover() const;
	bool	IsLeftEdgeValid(bool useLowCover) const;
	bool	IsRightEdgeValid(bool useLowCover) const;
	void	GetCoverPoints(bool useLowCover, float peekOverLeft, float peekOverRight, const Vec3& targetPos,
		Vec3& hidePos, Vec3& peekPosLeft, Vec3& peekPosRight, bool& peekLeftClamped, bool& peekRightClamped, bool& coverCompromised) const;
	float	GetCoverWidth(bool useLowCover);
	bool	IsCoverPathComplete() const { return m_pathComplete; }
	void	HurryUpCoverPathGen() { m_pathHurryUp = true; }
	float	GetDistanceAlongCoverPath(const Vec3& pt) const { return m_pathDir.Dot(pt - m_pathOrig); }
	float	GetDistanceToCoverPath(const Vec3& pt) const;
	Vec3	ProjectPointOnCoverPath(const Vec3& pt) const { return m_pathOrig + m_pathDir * (m_pathDir.Dot(pt - m_pathOrig)); }
	const Vec3& GetCoverPathDir() const { return m_pathDir; }
	inline float GetMaxCoverPathLen() const { return 4.0f; }

	void DebugDraw(IRenderer *pRenderer);
	void Serialize(TSerialize ser, class CObjectTracker& objectTracker);	

private:

	void SetupPathExpand(CPipeUser *pOperand);
	void UpdatePathExpand(CPipeUser *pOperand);
	bool IsSegmentValid(CPipeUser *pOperand, const Vec3& posFrom, const Vec3& posTo);

	void SampleCover(CPipeUser *pOperand, float& maxCover, float& maxDepth, const Vec3& startPos, float maxWidth,
		float sampleDist, float sampleRad, float sampleDepth, std::list<Vec3>& points, bool pushBack, bool& reachedEdge);
	void SampleCoverRefine(CPipeUser *pOperand, float& maxCover, float& maxDepth, const Vec3& startPos, float maxWidth,
		float sampleDist, float sampleRad, float sampleDepth, std::list<Vec3>& points, bool pushBack);
	void SampleLine(CPipeUser *pOperand, float& maxMove, float maxWidth, float sampleDist);
	void SampleLineRefine(CPipeUser *pOperand, float& maxMove, float maxWidth, float sampleDist);

	bool					m_bIsValid;
	bool					m_isUsingCover;
	Vec3					m_objectPos;
	Vec3					m_objectDir;
	float					m_objectRadius;
	float					m_objectHeight;
	bool					m_objectCollidable;
	Vec3					m_vLastHidePos;		// actual position used for last hide
	Vec3					m_vLastHideDir;
	bool					m_bIsSmartObject;
	CQueryEvent		m_HideSmartObject;
	int						m_useCover;
	EntityId			m_dynCoverEntityId;
	Vec3					m_dynCoverEntityPos;		// The original position of the dynamic cover entity.
	Vec3					m_dynCoverPosLocal;	// The original local space position of the cover.
	bool					m_isCompromised;

	// Cover sampling
	Vec3					m_coverPos;
	float					m_distToCover;
	Vec3					m_pathOrig;
	Vec3					m_pathDir;
	Vec3					m_pathNorm;
	float					m_pathLimitLeft;
	float					m_pathLimitRight;
	float					m_tempCover;
	float					m_tempDepth;
	bool					m_pathComplete;
	bool					m_highCoverValid;
	bool					m_lowCoverValid;
	bool					m_pathHurryUp;
	bool					m_lowLeftEdgeValid;
	bool					m_lowRightEdgeValid;
	bool					m_highLeftEdgeValid;
	bool					m_highRightEdgeValid;
	std::list<Vec3>	m_lowCoverPoints;
	std::list<Vec3>	m_highCoverPoints;
	float					m_lowCoverWidth;
	float					m_highCoverWidth;
	int						m_pathUpdateIter;
	unsigned int	m_id;
};





#endif __AIHIdeObject_H__
