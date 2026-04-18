////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   AlienBrainPlugin.cpp
//  Version:     v1.00
//  Created:     21 Sen 2004 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AlienBrainPlugin.h"


void CAlienBrainPlugin::Release()
{
	delete this;
}

void CAlienBrainPlugin::ShowAbout()
{
}

const char * CAlienBrainPlugin::GetPluginGUID()
{
	return 0;
}

DWORD CAlienBrainPlugin::GetPluginVersion()
{
	return 1;
}

const char * CAlienBrainPlugin::GetPluginName()
{
	return "AlienBrain plug-in";
}

bool CAlienBrainPlugin::CanExitNow()
{
	return true;
}

void CAlienBrainPlugin::Serialize(FILE *hFile, bool bIsStoring)
{

}

void CAlienBrainPlugin::ResetContent()
{

}

bool CAlienBrainPlugin::CreateUIElements()
{
	return true;
}

bool CAlienBrainPlugin::ExportDataToGame(const char * pszGamePath)
{
	return false;
}