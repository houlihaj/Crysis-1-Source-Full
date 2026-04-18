#include "stdafx.h"
#include "AnimEventJobs.h"
#include "IAnimEventDocChangeTarget.h"

AnimEventChangeTimeJob::AnimEventChangeTimeJob(float fTime, const std::set<int>& ids)
:	fTime(fTime),
	ids(ids)
{
}

AnimEventChangeTimeJob::~AnimEventChangeTimeJob()
{
}

void AnimEventChangeTimeJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	for (std::set<int>::iterator itID = this->ids.begin(); itID != this->ids.end(); ++itID)
		pTarget->SetAnimEventTime(*itID, this->fTime);
}

AnimEventSetParameterJob::AnimEventSetParameterJob(const char* szParameter, const std::set<int>& ids)
:	sParameter(szParameter),
	ids(ids)
{
}

AnimEventSetParameterJob::~AnimEventSetParameterJob()
{
}

void AnimEventSetParameterJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	for (std::set<int>::iterator itID = this->ids.begin(); itID != this->ids.end(); ++itID)
		pTarget->SetEventParameter(*itID, this->sParameter);
}

AnimEventSetParameterNameJob::AnimEventSetParameterNameJob(const char* szParameter, const char* szName, const std::set<int>& ids)
:	sParameter(szParameter),
	sName(szName),
	ids(ids)
{
}

AnimEventSetParameterNameJob::~AnimEventSetParameterNameJob()
{
}

void AnimEventSetParameterNameJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	for (std::set<int>::iterator itID = this->ids.begin(); itID != this->ids.end(); ++itID)
	{
		pTarget->SetEventName(*itID, this->sName);
		pTarget->SetEventParameter(*itID, this->sParameter);
	}
}

AnimEventSetBoneJob::AnimEventSetBoneJob(const char* szBone, const std::set<int>& ids)
:	sBone(szBone),
	ids(ids)
{
}

AnimEventSetBoneJob::~AnimEventSetBoneJob()
{
}

void AnimEventSetBoneJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	for (std::set<int>::iterator itID = this->ids.begin(); itID != this->ids.end(); ++itID)
		pTarget->SetEventBone(*itID, this->sBone);
}

AnimEventChangeOffsetJob::AnimEventChangeOffsetJob(const Vec3& vOffset, const std::set<int>& ids)
:	vOffset(vOffset),
	ids(ids)
{
}

AnimEventChangeOffsetJob::~AnimEventChangeOffsetJob()
{
}

void AnimEventChangeOffsetJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	for (std::set<int>::iterator itID = this->ids.begin(); itID != this->ids.end(); ++itID)
		pTarget->SetAnimEventOffset(*itID, this->vOffset);
}

AnimEventChangeDirJob::AnimEventChangeDirJob(const Vec3& vDir, const std::set<int>& ids)
:	vDir(vDir),
	ids(ids)
{
}

AnimEventChangeDirJob::~AnimEventChangeDirJob()
{
}

void AnimEventChangeDirJob::Perform(IAnimEventDocChangeTarget* pTarget)
{
	for (std::set<int>::iterator itID = this->ids.begin(); itID != this->ids.end(); ++itID)
		pTarget->SetAnimEventDir(*itID, this->vDir);
}
