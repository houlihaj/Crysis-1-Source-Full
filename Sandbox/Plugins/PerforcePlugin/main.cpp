////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   main.cpp
//  Version:     v1.00
//  Created:     21 Sen 2004 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "PerforcePlugin.h"

#include "Include/ISourceControl.h"
#include "Include/IEditorClassFactory.h"
#include "PerforceSourceControl.h"


ISystem* g_pSystem;

//IEditor * pEditor = 0;

/*
ISystem * GetISystem()
{
	if(pEditor)
		return pEditor->GetSystem();
	return 0;
}
*/



PLUGIN_API IPlugin * CreatePluginInstance(PLUGIN_INIT_PARAM * pInitParam)
{
	g_pSystem = pInitParam->pIEditorInterface->GetSystem();
	ModuleInitISystem(g_pSystem);
	pInitParam->pIEditorInterface->GetClassFactory()->RegisterClass(new CPerforceSourceControl);
	g_pSystem->GetILog()->Log("Perforce plugin: CreatePluginInstance");
	return new CPerforcePlugin;
}
