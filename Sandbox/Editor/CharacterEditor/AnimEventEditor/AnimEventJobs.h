//---------------------------------------------------------------------------
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTJOBS_H__
#define __ANIMEVENTJOBS_H__

#include "IAnimEventDocJob.h"
#include <set>

class AnimEventChangeTimeJob : public IAnimEventDocJob
{
public:
	AnimEventChangeTimeJob(float fTime, const std::set<int>& ids);
	virtual ~AnimEventChangeTimeJob();

	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	float fTime;
	std::set<int> ids;
};

class AnimEventSetParameterJob : public IAnimEventDocJob
{
public:
	AnimEventSetParameterJob(const char* szParameter, const std::set<int>& ids);
	virtual ~AnimEventSetParameterJob();

	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	string sParameter;
	std::set<int> ids;
};

class AnimEventSetParameterNameJob : public IAnimEventDocJob
{
public:
	AnimEventSetParameterNameJob(const char* szParameter, const char* szName, const std::set<int>& ids);
	virtual ~AnimEventSetParameterNameJob();

	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	string sParameter;
	string sName;
	std::set<int> ids;
};

class AnimEventSetBoneJob : public IAnimEventDocJob
{
public:
	AnimEventSetBoneJob(const char* szBone, const std::set<int>& ids);
	virtual ~AnimEventSetBoneJob();

	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	string sBone;
	std::set<int> ids;
};

class AnimEventChangeOffsetJob : public IAnimEventDocJob
{
public:
	AnimEventChangeOffsetJob(const Vec3& vOffset, const std::set<int>& ids);
	virtual ~AnimEventChangeOffsetJob();

	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	Vec3 vOffset;
	std::set<int> ids;
};

class AnimEventChangeDirJob : public IAnimEventDocJob
{
public:
	AnimEventChangeDirJob(const Vec3& vDir, const std::set<int>& ids);
	virtual ~AnimEventChangeDirJob();

	virtual void Perform(IAnimEventDocChangeTarget* pTarget);

private:
	Vec3 vDir;
	std::set<int> ids;
};

#endif //__ANIMEVENTJOBS_H__
