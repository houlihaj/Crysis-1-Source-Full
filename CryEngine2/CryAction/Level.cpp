#include "StdAfx.h"
#include "Level.h"


CLevel::CLevel()
{
}

CLevel::~CLevel()
{
}

void CLevel::Release()
{
	delete this;
}

ILevelInfo *CLevel::GetLevelInfo()
{
	return &m_levelInfo;
}