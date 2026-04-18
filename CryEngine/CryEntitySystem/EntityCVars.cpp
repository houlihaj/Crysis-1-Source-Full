////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   EntityCVars.h
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EntityCVars.h"
#include "EntitySystem.h"
#include "Boids/Flock.h"
#include "AreaManager.h"

ICVar* CVar::pDebug = NULL;
ICVar* CVar::pCharacterIK = NULL;
ICVar* CVar::pProfileEntities = NULL;
//ICVar* CVar::pUpdateInvisibleCharacter = NULL;
//ICVar* CVar::pUpdateBonePositions = NULL;
ICVar* CVar::pUpdateScript = NULL;
ICVar* CVar::pUpdateTimer = NULL;
ICVar* CVar::pUpdateCamera = NULL;
ICVar* CVar::pUpdatePhysics = NULL;
ICVar* CVar::pUpdateAI = NULL;
ICVar* CVar::pUpdateEntities = NULL;
ICVar* CVar::pUpdateCollision = NULL;
ICVar* CVar::pUpdateCollisionScript = NULL;
ICVar* CVar::pUpdateContainer = NULL;
ICVar* CVar::pUpdateCoocooEgg = NULL;
ICVar* CVar::pPiercingCamera = NULL;
ICVar* CVar::pVisCheckForUpdate = NULL;
ICVar* CVar::pEntityBBoxes = NULL;
ICVar* CVar::pEntityHelpers = NULL;
ICVar* CVar::pOnDemandPhysics = NULL;
ICVar* CVar::pMinImpulseVel = NULL;
ICVar* CVar::pImpulseScale = NULL;
ICVar* CVar::pMaxImpulseAdjMass = NULL;
ICVar* CVar::pDebrisLifetimeScale = NULL;
ICVar* CVar::pSplashThreshold = NULL;
ICVar* CVar::pSplashTimeout = NULL;
ICVar* CVar::pHitCharacters = NULL;
ICVar* CVar::pHitDeadBodies = NULL;
ICVar* CVar::pCharZOffsetSpeed = NULL;
ICVar* CVar::pEnableFullScriptSave = NULL;
ICVar* CVar::pLogCollisions = NULL;
ICVar* CVar::pNotSeenTimeout = NULL;
ICVar* CVar::pDebugNotSeenTimeout = NULL;
ICVar* CVar::pDrawAreas = NULL;
ICVar* CVar::pDrawAreaGrid = NULL;

ICVar* CVar::pMotionBlur = NULL;

int CVar::es_DebugTimers = 0;
int CVar::es_DebugFindEntity = 0;
int CVar::es_UsePhysVisibilityChecks = 1;
float CVar::es_MaxPhysDist;
float CVar::es_MaxPhysDistInvisible;
float CVar::es_FarPhysTimeout;
int   CVar::es_DebugEvents = 0;
int   CVar::es_SortUpdatesByClass = 0;
int   CVar::es_DisableTriggers = 0;

int		CVar::es_LogDrawnActors = 0;

void CVar::Init( struct IConsole *pConsole )
{
	pConsole->AddCommand("es_dump_entities", (ConsoleCommandFunc)DumpEntities, VF_CHEAT, "Dumps current entities and their states!");
	pConsole->AddCommand("es_dump_entity_classes_in_use", (ConsoleCommandFunc)DumpEntityClassesInUse, VF_CHEAT, "Dumps all used entity classes");
	pConsole->AddCommand("es_compile_area_grid", (ConsoleCommandFunc)CompileAreaGrid, 0, "Trigger a recompile of the area grid");
	pConsole->Register( "es_sortupdatesbyclass", &es_SortUpdatesByClass, 0, 0, "Sort entity updates by class (possible optimization)" );
	pDebug = pConsole->RegisterInt("es_debug",0,VF_CHEAT,
		"Enable entity debugging info\n"
		"Usage: es_debug [0/1]\n"
		"Default is 0 (on).");
	pCharacterIK = pConsole->RegisterInt("p_characterik",1,VF_CHEAT,
		"Toggles character IK.\n"
		"Usage: p_characterik [0/1]\n"
		"Default is 1 (on). Set to 0 to disable inverse kinematics.");	
	pEntityBBoxes = pConsole->RegisterInt("es_bboxes",0,VF_CHEAT,
		"Toggles entity bounding boxes.\n"
		"Usage: es_bboxes [0/1]\n"
		"Default is 0 (off). Set to 1 to display bounding boxes.");
	pEntityHelpers = pConsole->RegisterInt("es_helpers",0,VF_CHEAT,
		"Toggles helpers.\n"
		"Usage: es_helpers [0/1]\n"
		"Default is 0 (off). Set to 1 to display entity helpers.");
	pProfileEntities = pConsole->RegisterInt("es_profileentities",0,VF_CHEAT,
		"\n"
		"Usage: 1,2,3\n"
		"Default is 0 (off).");
/*	pUpdateInvisibleCharacter = pConsole->RegisterInt("es_UpdateInvisibleCharacter",0,VF_CHEAT,
		"\n"
		"Usage: \n"
		"Default is 0 (off).");
	pUpdateBonePositions = pConsole->RegisterInt("es_UpdateBonePositions",1,VF_CHEAT,
		"\n"
		"Usage: \n"
		"Default is 1 (on).");
*/	pUpdateScript = pConsole->RegisterInt("es_UpdateScript",1,VF_CHEAT,
		"\n"
		"Usage: \n"
		"Default is 1 (on).");
	pUpdatePhysics = pConsole->RegisterInt("es_UpdatePhysics",1,VF_CHEAT,
		"Toggles updating of entity physics.\n"
		"Usage: es_UpdatePhysics [0/1]\n"
		"Default is 1 (on). Set to 0 to prevent entity physics from updating.");
	pUpdateAI = pConsole->RegisterInt("es_UpdateAI",1,VF_CHEAT,
		"Toggles updating of AI entities.\n"
		"Usage: es_UpdateAI [0/1]\n"
		"Default is 1 (on). Set to 0 to prevent AI entities from updating.");
	pUpdateEntities = pConsole->RegisterInt("es_UpdateEntities",1,VF_CHEAT,
		"Toggles entity updating.\n"
		"Usage: es_UpdateEntities [0/1]\n"
		"Default is 1 (on). Set to 0 to prevent all entities from updating.");
	pUpdateCollision= pConsole->RegisterInt("es_UpdateCollision",1,VF_CHEAT,
		"Toggles updating of entity collisions.\n"
		"Usage: es_UpdateCollision [0/1]\n"
		"Default is 1 (on). Set to 0 to disable entity collision updating.");
	pUpdateContainer= pConsole->RegisterInt("es_UpdateContainer",1,VF_CHEAT,
		"\n"
		"Usage: es_UpdateContainer [0/1]\n"
		"Default is 1 (on).");
	pUpdateTimer = pConsole->RegisterInt("es_UpdateTimer",1,VF_CHEAT,
		"\n"
		"Usage: es_UpdateTimer [0/1]\n"
		"Default is 1 (on).");
	pUpdateCollisionScript = pConsole->RegisterInt("es_UpdateCollisionScript",1,VF_CHEAT,
		"\n"
		"Usage: es_UpdateCollisionScript [0/1]\n"
		"Default is 1 (on).");
	pVisCheckForUpdate = pConsole->RegisterInt("es_VisCheckForUpdate",1,VF_CHEAT,
		"\n"
		"Usage: es_VisCheckForUpdate [0/1]\n"
		"Default is 1 (on).");
	pOnDemandPhysics = pConsole->RegisterInt("es_OnDemandPhysics",0,VF_CHEAT,
		"\n"
		"Usage: es_OnDemandPhysics [0/1]\n"
		"Default is 1 (on).");
	pMinImpulseVel = pConsole->RegisterFloat("es_MinImpulseVel",0.0f,VF_CHEAT,
		"\n"
		"Usage: es_MinImpulseVel 0.0\n"
		"");
	pImpulseScale = pConsole->RegisterFloat("es_ImpulseScale",0.0f,VF_CHEAT,
		"\n"
		"Usage: es_ImpulseScale 0.0\n"
		"");
	pMaxImpulseAdjMass = pConsole->RegisterFloat("es_MaxImpulseAdjMass",2000.0f,VF_CHEAT,
		"\n"
		"Usage: es_MaxImpulseAdjMass 2000.0\n"
		"");
	pDebrisLifetimeScale = pConsole->RegisterFloat("es_DebrisLifetimeScale",1.0f,0,
		"\n"
		"Usage: es_DebrisLifetimeScale 1.0\n"
		"");
	pSplashThreshold = pConsole->RegisterFloat("es_SplashThreshold",1.0f,VF_CHEAT,
		"minimum instantaneous water resistance that is detected as a splash"
		"Usage: es_SplashThreshold 200.0\n"
		"");
	pSplashTimeout = pConsole->RegisterFloat("es_SplashTimeout",3.0f,VF_CHEAT,
		"minimum time interval between consecutive splashes"
		"Usage: es_SplashTimeout 3.0\n"
		"");
	pHitCharacters = pConsole->RegisterInt("es_HitCharacters",1,0,
		"specifies whether alive characters are affected by bullet hits (0 or 1)");
	pHitDeadBodies = pConsole->RegisterInt("es_HitDeadBodies",1,0,
		"specifies whether dead bodies are affected by bullet hits (0 or 1)");
	pCharZOffsetSpeed = pConsole->RegisterFloat("es_CharZOffsetSpeed",2.0f,VF_DUMPTODISK,
		"sets the character Z-offset change speed (in m/s), used for IK");

	pNotSeenTimeout = pConsole->RegisterInt("es_not_seen_timeout", 30, VF_DUMPTODISK,
		"number of seconds after which to cleanup temporary render buffers in entity");
	pDebugNotSeenTimeout = pConsole->RegisterInt("es_debug_not_seen_timeout", 0, VF_DUMPTODISK,
		"if true, log messages when entities undergo not seen timeout");

	pEnableFullScriptSave = pConsole->RegisterInt("es_enable_full_script_save",0,
		VF_DUMPTODISK|VF_SAVEGAME,"Enable (experimental) full script save functionality");
	
	pLogCollisions = pConsole->RegisterInt("es_log_collisions",0,0,"Enables collision events logging" );
	pConsole->Register("es_DebugTimers",&es_DebugTimers,0,VF_CHEAT,
		"This is for profiling and debugging (for game coders and level designer)\n"
		"By enabling this you get a lot of console printouts that show all entities that receive OnTimer\n"
		"events - it's good to minimize the call count. Certain entities might require this feature and\n"
		"using less active entities can often be defined by the level designer.\n"
		"Usage: es_DebugTimers 0/1");
	pConsole->Register("es_DebugFindEntity",&es_DebugFindEntity,0,VF_CHEAT );
	pConsole->Register("es_DebugEvents",&es_DebugEvents,0,VF_CHEAT,"Enables logging of entity events\n" );
	pConsole->Register("es_DisableTriggers",&es_DisableTriggers,0,0,"Disable enter/leave events for proximity and area triggers");

	pDrawAreas = pConsole->RegisterInt("es_DrawAreas",0,VF_CHEAT,		
		"0: Off.\n1: Enables drawing of Areas.\n2: Shows area sizes.");

	pDrawAreaGrid = pConsole->RegisterInt("es_DrawAreaGrid",0,VF_CHEAT,"Enables drawing of Area Grid" );

	pConsole->Register( "e_flocks",&CFlock::m_e_flocks,1,VF_DUMPTODISK,"Enable Flocks (Birds/Fishes)" );
	pConsole->Register( "e_flocks_hunt",&CFlock::m_e_flocks_hunt,1,0,"Birds will fall down..." );

	pConsole->Register( "es_UsePhysVisibilityChecks",&es_UsePhysVisibilityChecks,1,0,
		"Activates physics quality degradation and forceful sleeping for invisible and faraway entities" );
	pConsole->Register( "es_MaxPhysDist",&es_MaxPhysDist,300.0f,0,
		"Physical entities farther from the camera than this are forcefully deactivated" );
	pConsole->Register( "es_MaxPhysDistInvisible",&es_MaxPhysDistInvisible,40.0f,0,
		"Invisible physical entities farther from the camera than this are forcefully deactivated" );
	pConsole->Register( "es_FarPhysTimeout",&es_FarPhysTimeout,4.0f,0,
		"Timeout for faraway physics forceful deactivation" );

  pMotionBlur = GetISystem()->GetIConsole()->GetCVar("r_MotionBlur");

	pConsole->Register("es_LogDrawnActors", &es_LogDrawnActors, 0, VF_NOT_NET_SYNCED, "Log all actors rendered each frame");
}


void CVar::DumpEntities(IConsoleCmdArgs *args)
{
	GetIEntitySystem()->DumpEntities();
}

void CVar::DumpEntityClassesInUse(IConsoleCmdArgs * args)
{
	IEntityItPtr it	= GetIEntitySystem()->GetEntityIterator();
	it->MoveFirst();

	std::map<string, int> classes;
	while(IEntity *pEntity = it->Next())
	{
		classes[pEntity->GetClass()->GetName()]++;
	}

	CryLogAlways("--------------------------------------------------------------------------------");
	for (std::map<string, int>::iterator iter = classes.begin(); iter != classes.end(); ++iter)
		CryLogAlways("%s: %d instances", iter->first.c_str(), iter->second);
}

void CVar::CompileAreaGrid(IConsoleCmdArgs*)
{
	CEntitySystem* pEntitySystem = GetIEntitySystem();
	CAreaManager* pAreaManager = (pEntitySystem ? pEntitySystem->GetAreaManager() : 0);
	if (pAreaManager)
		pAreaManager->SetAreasDirty();
}
