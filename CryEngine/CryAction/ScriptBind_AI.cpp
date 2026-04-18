/********************************************************************
CryGame Source File.
Copyright (C), Crytek Studios, 2001-2004. 
-------------------------------------------------------------------------
File name:   ScriptBind_AI.cpp
Version:     v1.00
Description: 

-------------------------------------------------------------------------
History:
- 3:11:2004   15:41 : Created by Kirill Bulatsev

*********************************************************************/



#include "StdAfx.h"
#include "ScriptBind_AI.h"
#include "AIProxy.h"
//#include "AIHandler.h"
#include <ISystem.h>
#include <IAISystem.h>
#include <IAgent.h>
#include <IAIGroup.h>
#include "IGame.h"
#include "IGameFramework.h"
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "VehicleSystem/VehicleSeat.h"
#include "VehicleSystem/VehicleSeatActionWeapons.h"

#include <IEntityProxy.h>
#include <IFlowSystem.h>
#include <IInterestSystem.h>
#include <list>
#include <functional>

#if defined(LINUX)
#include <float.h> // for FLT_MAX
#endif
#include "Cry_GeoDistance.h"

enum EAIForcedEventTypes
{
	SOUND_INTERESTING = 0,
	SOUND_THREATENING = 1,
};

enum EAIUseCoverAction
{
	COVER_HIDE = 0,
	COVER_UNHIDE = 1,
};

enum EAIProximityFlags
{
	AIPROX_SIGNAL_ON_OBJ_DISABLE = 0x1,
	AIPROX_VISIBLE_TARGET_ONLY = 0x2,
};

enum EAIPathType
{
	AIPATH_DEFAULT,
	AIPATH_HUMAN,
	AIPATH_HUMAN_COVER,
	AIPATH_CAR,
	AIPATH_TANK,
	AIPATH_BOAT,
	AIPATH_HELI,
	AIPATH_3D,
	AIPATH_SCOUT,
	AIPATH_TROOPER,
	AIPATH_HUNTER,
};

enum EAIParamNames
{
	AIPARAM_SIGHTRANGE			= 1,
	AIPARAM_ATTACKRANGE,
	AIPARAM_ACCURACY,
	AIPARAM_GROUPID,
	AIPARAM_FOVPRIMARY,
	AIPARAM_FOVSECONDARY,
	AIPARAM_COMMRANGE,
	AIPARAM_FWDSPEED,
	AIPARAM_SPECIES,
	AIPARAM_SPECIESHOSILITY,
	AIPARAM_RANK,
	AIPARAM_CAMOSCALE,
	AIPARAM_TRACKPATNAME,
	AIPARAM_TRACKPATADVANCE,
	AIPARAM_HEATSCALE,
	AIPARAM_STRAFINGPITCH,
	AIPARAM_COMBATCLASS,
	AIPARAM_INVISIBLE,
	AIPARAM_PERCEPTIONSCALE_VISUAL,
	AIPARAM_PERCEPTIONSCALE_AUDIO,
	AIPARAM_CLOAK_SCALE,
	AIPARAM_FORGETTIME_TARGET,
	AIPARAM_FORGETTIME_SEEK,
	AIPARAM_FORGETTIME_MEMORY,
	AIPARAM_LOOKIDLE_TURNSPEED,
	AIPARAM_LOOKCOMBAT_TURNSPEED,
	AIPARAM_AIM_TURNSPEED,
	AIPARAM_FIRE_TURNSPEED,
	AIPARAM_MELEE_DISTANCE,
	AIPARAM_MIN_ALARM_LEVEL,
	AIPARAM_SIGHTENVSCALE_NORMAL,
	AIPARAM_SIGHTENVSCALE_ALARMED,
	AIPARAM_GRENADE_THROWDIST,
};

enum EAIMoveAbilityNames
{
	AIMOVEABILITY_OPTIMALFLIGHTHEIGHT = 1,
	AIMOVEABILITY_MINFLIGHTHEIGHT,
	AIMOVEABILITY_MAXFLIGHTHEIGHT,
	AIMOVEABILITY_TELEPORTENABLE,
	AIMOVEABILITY_USEPREDICTIVEFOLLOWING
};

enum EStickFlags
{
	STICK_BREAK = 0x01,
	STICK_SHORTCUTNAV	= 0x02,
};

enum FindObjectOfTypeFlags
{
	AIFO_FACING_TARGET		= 0x0001,
	AIFO_NONOCCUPIED			= 0x0002,
	AIFO_CHOOSE_RANDOM		= 0x0004,
	AIFO_NONOCCUPIED_REFPOINT	= 0x0008,
	AIFO_USE_BEACON_AS_FALLBACK_TGT	= 0x0010,
	AIFO_NO_DEVALUE				= 0x0020,
};


//std::vector< string > CScriptBind_AI::m_CustomOnSeenSignals;

#undef GET_ENTITY
#define GET_ENTITY(i) \
	ScriptHandle hdl;\
	pH->GetParam(i,hdl);\
	int nID = hdl.n;\
	IEntity* pEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);


//====================================================================
// OverlapSphere
//====================================================================
static bool OverlapSphere(const Vec3& pos, float radius, IPhysicalEntity **entities, unsigned nEntities, Vec3& hitDir)
{
	primitives::sphere spherePrim;
	spherePrim.center = pos;
	spherePrim.r = radius;

	unsigned hitCount = 0;
	ray_hit hit;
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	for (unsigned iEntity = 0 ; iEntity < nEntities ; ++iEntity)
	{
		IPhysicalEntity *pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithPrimitive(pEntity, spherePrim.type, &spherePrim, Vec3(ZERO), &hit))
		{
			hitDir += hit.n;
			hitCount++;
		}
	}
	hitDir.NormalizeSafe();
	return hitCount != 0;
}


//====================================================================
// OverlapSweptSphere
//====================================================================
static bool OverlapSweptSphere(const Vec3& pos, const Vec3& dir, float radius, IPhysicalEntity **entities, unsigned nEntities, Vec3& hitDir)
{
	primitives::sphere spherePrim;
	spherePrim.center = pos;
	spherePrim.r = radius;

	unsigned hitCount = 0;
	ray_hit hit;
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	for (unsigned iEntity = 0 ; iEntity < nEntities ; ++iEntity)
	{
		IPhysicalEntity *pEntity = entities[iEntity];
		if (pPhysics->CollideEntityWithPrimitive(pEntity, spherePrim.type, &spherePrim, dir, &hit))
		{
			hitDir += hit.n;
			hitCount++;
		}
	}
	hitDir.NormalizeSafe();
	
	return hitCount != 0;
}


//====================================================================
// HasPointInRange
//====================================================================
static bool	HasPointInRange(const Vec3& pos, float range, const std::list<Vec3>& points)
{
	const float rangeSqr = sqr(range);
	for(std::list<Vec3>::const_iterator ptIt = points.begin(); ptIt != points.end(); ++ ptIt)
	{
		if(Distance::Point_PointSq((*ptIt), pos) < rangeSqr)
			return true;
	}
	return false;
}

//====================================================================
// ReloadReadabilityXML
//====================================================================
static void ReloadReadabilityXML(IConsoleCmdArgs* /* pArgs */)
{
	CAIHandler::s_ReadabilityManager.Reload();
	CAIFaceManager::LoadStatic();
}

//====================================================================
// CalcLMGSpeedRanges
//====================================================================
static void CalcLMGSpeedRanges(const char** names, int n, IAnimationSet* pAnimSet, float* speedMin, float* speedMax)
{
	Vec2	moveDirs[4] = { Vec2(0,1), Vec2(0,-1), Vec2(1,0), Vec2(-1,0) };
//	const char* moveDirNames[4] = { "FWD", "BWD", "RGT", "LFT" };

	for (int i = 0; i < 4; ++i)
	{
		speedMin[i] = -FLT_MAX;
		speedMax[i] = FLT_MAX;
	}

//	float speedMin[4] = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
//	float speedMax[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

	for (int i = 0; i < n; ++i)
	{
//		CryLog("%s", names[i]);
		for (unsigned j = 0; j < 4; ++j)
		{
			LMGCapabilities caps = pAnimSet->GetLMGPropertiesByName(names[i], moveDirs[j], 0.0f, 0.0f);
			float smin = caps.m_vMinVelocity.GetLength();
			float smax = caps.m_vMaxVelocity.GetLength();
//			CryLog("  %s: %f, %f", moveDirNames[j], smin, smax);
			speedMin[j] = max(speedMin[j], smin);
			speedMax[j] = min(speedMax[j], smax);
		}
	}

	for (int i = 0; i < 4; ++i)
	{
		if (speedMin[i] > speedMax[i])
		{
			float a = (speedMin[i] + speedMax[i]) / 2.0f;
			speedMin[i] = speedMax[i] = a;
		}
	}


//	CryLog("** COMMON:");
//	for (unsigned j = 0; j < 4; ++j)
//		CryLog("    %s: %f, %f (%f, %f)", moveDirNames[j], speedMin[j], speedMax[j], speedMinAvg[j]/(float)n, speedMaxAvg[j]/(float)n);
//	CryLog("\n");
}

//====================================================================
// CalcHumanMovementTable
//====================================================================
static void CalcHumanMovementTable(IConsoleCmdArgs* pArgs)
{
	if(pArgs->GetArgCount() < 1)
	{
		gEnv->pAISystem->Warning("<CalcHumanMovementTable> ", "Expecting entity name as parameter.");
		return;
	}

	// Start readability testing
	IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(pArgs->GetArg(1));
	if(!pEntity)
	{
		gEnv->pAISystem->Warning("<CalcHumanMovementTable> ", "Could not find entity '%s'.", pArgs->GetArg(1));
		return;
	}

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
	{
		gEnv->pAISystem->Warning("<CalcHumanMovementTable> ", "Entity '%s' does not have character.", pEntity->GetName());
		return;
	}

	IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
	if (!pAnimSet)
	{
		gEnv->pAISystem->Warning("<CalcHumanMovementTable> ", "Entity '%s' character does not have anim set.", pEntity->GetName());
		return;
	}


	CryLog("\t\tAIMovementSpeeds =");
	CryLog("\t\t{");


	float speedMin[4], speedMax[4];


	const char* relaxed_walk_LMG[1] =
	{
		"_RELAXED_WALKSTRAFE_NW_01",
	};
	const char* relaxed_run_LMG[1] =
	{
		"_RELAXED_RUNSTRAFE_NW_01",
	};

	CryLog("\t\t\tRelaxed =");
	CryLog("\t\t\t{");
	CalcLMGSpeedRanges(relaxed_walk_LMG, 1, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tWalk = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CalcLMGSpeedRanges(relaxed_run_LMG, 1, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tRun = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CryLog("\t\t\t},\n");

	const char* combat_walk_LMG[6] =
	{
		"_COMBAT_WALKSTRAFE_NW_01",
		"_COMBAT_WALKSTRAFE_PISTOL_01",
		"_COMBAT_WALKSTRAFE_DUALPISTOL_01",
		"_COMBAT_WALKSTRAFE_RIFLE_01",
		"_COMBAT_WALKSTRAFE_MG_01",
		"_COMBAT_WALKSTRAFE_ROCKET_01",
	};
	const char* combat_run_LMG[6] =
	{
		"_COMBAT_RUNSTRAFE_NW_01",
		"_COMBAT_RUNSTRAFE_PISTOL_01",
		"_COMBAT_RUNSTRAFE_DUALPISTOL_01",
		"_COMBAT_RUNSTRAFE_RIFLE_01",
		"_COMBAT_RUNSTRAFE_MG_01",
		"_COMBAT_RUNSTRAFE_ROCKET_01",
	};
	const char* combat_sprint_LMG[6] =
	{
		"_COMBAT_SPRINTSTRAFE_NW_01",
		"_COMBAT_SPRINTSTRAFE_PISTOL_01",
		"_COMBAT_SPRINTSTRAFE_DUALPISTOL_01",
		"_COMBAT_SPRINTSTRAFE_RIFLE_01",
		"_COMBAT_SPRINTSTRAFE_MG_01",
		"_COMBAT_SPRINTSTRAFE_ROCKET_01",
	};

	CryLog("\t\t\tCombat =");
	CryLog("\t\t\t{");
	CalcLMGSpeedRanges(combat_walk_LMG, 6, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tWalk = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CalcLMGSpeedRanges(combat_run_LMG, 6, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tRun = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CalcLMGSpeedRanges(combat_sprint_LMG, 6, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tSprint = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CryLog("\t\t\t},\n");


	const char* crouch_walk_LMG[6] =
	{
		"_CROUCH_WALKSTRAFE_NW_01",
		"_CROUCH_WALKSTRAFE_PISTOL_01",
		"_CROUCH_WALKSTRAFE_DUALPISTOL_01",
		"_CROUCH_WALKSTRAFE_RIFLE_01",
		"_CROUCH_WALKSTRAFE_MG_01",
		"_CROUCH_WALKSTRAFE_ROCKET_01",
	};
	const char* crouch_run_LMG[6] =
	{
		"_CROUCH_RUNSTRAFE_NW_01",
		"_CROUCH_RUNSTRAFE_PISTOL_01",
		"_CROUCH_RUNSTRAFE_DUALPISTOL_01",
		"_CROUCH_RUNSTRAFE_RIFLE_01",
		"_CROUCH_RUNSTRAFE_MG_01",
		"_CROUCH_RUNSTRAFE_ROCKET_01",
	};

	CryLog("\t\t\tCrouch =");
	CryLog("\t\t\t{");
	CalcLMGSpeedRanges(crouch_walk_LMG, 6, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tWalk = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CalcLMGSpeedRanges(crouch_run_LMG, 6, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tRun = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CryLog("\t\t\t},\n");


	const char* stealth_walk_LMG[6] =
	{
		"_STEALTH_WALKSTRAFE_NW_01",
		"_STEALTH_WALKSTRAFE_PISTOL_01",
		"_STEALTH_WALKSTRAFE_DUALPISTOL_01",
		"_STEALTH_WALKSTRAFE_RIFLE_01",
		"_STEALTH_WALKSTRAFE_MG_01",
		"_STEALTH_WALKSTRAFE_ROCKET_01",
	};
	const char* stealth_run_LMG[6] =
	{
		"_STEALTH_RUNSTRAFE_NW_01",
		"_STEALTH_RUNSTRAFE_PISTOL_01",
		"_STEALTH_RUNSTRAFE_DUALPISTOL_01",
		"_STEALTH_RUNSTRAFE_RIFLE_01",
		"_STEALTH_RUNSTRAFE_MG_01",
		"_STEALTH_RUNSTRAFE_ROCKET_01",
	};

	CryLog("\t\t\tStealth =");
	CryLog("\t\t\t{");
	CalcLMGSpeedRanges(stealth_walk_LMG, 6, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tWalk = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CalcLMGSpeedRanges(stealth_run_LMG, 6, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tRun = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CryLog("\t\t\t},\n");


	const char* prone_walk_LMG[4] =
	{
		"_PRONE_WALKSTRAFE_NW_01",
		"_PRONE_WALKSTRAFE_PISTOL_01",
		"_PRONE_WALKSTRAFE_DUALPISTOL_01",
		"_PRONE_WALKSTRAFE_RIFLE_01",
	};

	const char* prone_run_LMG[4] =
	{
		"_PRONE_RUNSTRAFE_NW_01",
		"_PRONE_RUNSTRAFE_PISTOL_01",
		"_PRONE_RUNSTRAFE_DUALPISTOL_01",
		"_PRONE_RUNSTRAFE_RIFLE_01",
	};

	CryLog("\t\t\tStealth =");
	CryLog("\t\t\t{");
	CalcLMGSpeedRanges(prone_walk_LMG, 4, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tWalk = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CalcLMGSpeedRanges(prone_run_LMG, 4, pAnimSet, speedMin, speedMax);
	CryLog("\t\t\t\tRun = {%f,%f, %f,%f ,%f,%f, %f,%f },", speedMin[0], speedMax[0], speedMin[1], speedMax[1], speedMin[2], speedMax[2], speedMin[3], speedMax[3]);
	CryLog("\t\t\t},\n");


	CryLog("\t\t},");

}

//====================================================================
// TestReadability
//====================================================================
static void TestReadability(IConsoleCmdArgs* pArgs)
{
	// Assume the first arg is the entity name.
	if(pArgs->GetArgCount() < 2)
	{
		gEnv->pAISystem->Warning("<TestReadability> ", "Expecting entity name as parameter.");
		return;
	}

	static EntityId lastEntityId = 0;

	IEntity* pEntity = 0;
	const char* szReadability = 0;
	bool start = true;

	if(strcmp(pArgs->GetArg(1), "0") == 0 || strcmp(pArgs->GetArg(1), "stop") == 0)
	{
		// Stop readability testing
		pEntity = gEnv->pEntitySystem->GetEntity(lastEntityId);
		start = false;

		if(!pEntity)
		{
			gEnv->pAISystem->Warning("<TestReadability> ", "Could not find entity which has previously used for the test.");
			return;
		}
	}
	else
	{
		// Start readability testing
		pEntity = gEnv->pEntitySystem->FindEntityByName(pArgs->GetArg(1));
		start = true;

		if(!pEntity)
		{
			gEnv->pAISystem->Warning("<TestReadability> ", "Could not find entity '%s'.", pArgs->GetArg(1));
			return;
		}

		if (pArgs->GetArgCount() > 2)
			szReadability = pArgs->GetArg(2);
	}

	IAIObject*	pAI = pEntity->GetAI();
	if(!pAI)
	{
		gEnv->pAISystem->Warning("<TestReadability> ", "Entity '%s' does not have AI.", pEntity->GetName());
		return;
	}
	IPuppetProxy *pPuppetProxy =  0;
	if(!pAI->GetProxy() || !pAI->GetProxy()->QueryProxy(AIPROXY_PUPPET, (void**)&pPuppetProxy))
	{
		gEnv->pAISystem->Warning("<TestReadability> ", "Entity '%s' does not have puppet AI proxy.", pEntity->GetName());
		return;
	}

	lastEntityId = pEntity->GetId();

	pPuppetProxy->TestReadabilityPack(start, szReadability);
}

//====================================================================
// RunStartupScript
//load the perception look-up table
//====================================================================
void InitLookUp( )
{
	std::vector<float> theTable;
	string sName = PathUtil::Make("Scripts/AI", "lookup.xml" );
	XmlNodeRef root = GetISystem()->LoadXmlFile( sName );
	if (!root)
		return ;

	XmlNodeRef nodeWorksheet = root->findChild( "Worksheet" );
	if (!nodeWorksheet)
		return ;

	XmlNodeRef nodeTable = nodeWorksheet->findChild( "Table" );
	if (!nodeTable)
		return ;

	for (int rowCntr(0), childN(0); childN<nodeTable->getChildCount(); ++childN)
	{
		XmlNodeRef nodeRow = nodeTable->getChild(childN);
		if (!nodeRow->isTag("Row"))
			continue;
		++rowCntr;
		for(int childrenCntr(0),cellIndex(1); childrenCntr<nodeRow->getChildCount(); ++childrenCntr,++cellIndex)
		{
			XmlNodeRef nodeCell = nodeRow->getChild(childrenCntr);
			if (!nodeCell->isTag("Cell"))
				continue;

			if(nodeCell->haveAttr("ss:Index"))
			{
				const char *pStrIdx=nodeCell->getAttr("ss:Index");
				sscanf(pStrIdx, "%d", &cellIndex);
			}
			XmlNodeRef nodeCellData = nodeCell->findChild("Data");
			if (!nodeCellData)
				continue;

			if(cellIndex==1)
			{
				const char* item(nodeCellData->getContent());
				if(item)
				{
					float	theValue(.0f);
					sscanf(item, "%f", &theValue);
					theTable.push_back(theValue);
				}
				break;
			}
		}
	}
	int	tableSize(theTable.size());
	float	*pTemBuffer( new float[tableSize] );
	for(int i(0); i<tableSize; ++i)
		pTemBuffer[i]=theTable[i];
	gEnv->pAISystem->SetPerceptionDistLookUp(pTemBuffer, tableSize);
	delete [] pTemBuffer;
}


//====================================================================
// ReloadLookUpXML
//====================================================================
void ReloadLookUpXML(IConsoleCmdArgs* /* pArgs */)
{
	InitLookUp( );
}




//====================================================================
// CScriptBind_AI
//====================================================================
CScriptBind_AI::CScriptBind_AI(ISystem *pSystem):
m_pSystem( pSystem ),
m_pAISystem (pSystem->GetAISystem()),
m_pActiveTrackPatternDesc(0),
m_pCurrentGoalPipe(0),
m_IsGroupOpen(false)
{
	Init(pSystem->GetIScriptSystem(), pSystem);

	SetGlobalName( "AI" );
	InitConVars();

#undef SCRIPT_REG_CLASSNAME 
#define SCRIPT_REG_CLASSNAME &CScriptBind_AI::

	SCRIPT_REG_FUNC(Warning);
	SCRIPT_REG_FUNC(Error);
	SCRIPT_REG_FUNC(LogProgress);
	SCRIPT_REG_FUNC(LogEvent);
	SCRIPT_REG_FUNC(LogComment);
	SCRIPT_REG_FUNC(RecComment);

	SCRIPT_REG_FUNC(RegisterWithAI);
	SCRIPT_REG_FUNC(ResetParameters);
	SCRIPT_REG_FUNC(ChangeParameter);
	SCRIPT_REG_FUNC(GetAIParameter);
	SCRIPT_REG_FUNC(IsEnabled);
	//	SCRIPT_REG_FUNC(AddSmartObjectCondition);
	SCRIPT_REG_FUNC(ChangeMovementAbility);
	SCRIPT_REG_FUNC(ExecuteAction);
	SCRIPT_REG_FUNC(AbortAction);
	SCRIPT_REG_FUNC(SetSmartObjectState);
	SCRIPT_REG_FUNC(ModifySmartObjectStates);
	SCRIPT_REG_FUNC(SmartObjectEvent);
	SCRIPT_REG_FUNC(GetLastUsedSmartObject);
	SCRIPT_REG_FUNC(GetLastSmartObjectExitPoint);
	SCRIPT_REG_FUNC(CreateGoalPipe);
	SCRIPT_REG_FUNC(BeginGoalPipe);
	SCRIPT_REG_FUNC(EndGoalPipe);
	SCRIPT_REG_FUNC(BeginGroup);
	SCRIPT_REG_FUNC(EndGroup);
	SCRIPT_REG_FUNC(PushGoal);
	SCRIPT_REG_FUNC(PushLabel);
	SCRIPT_REG_FUNC(IsGoalPipe);
	SCRIPT_REG_FUNC(Signal);
	SCRIPT_REG_FUNC(FreeSignal);
	SCRIPT_REG_FUNC(SetIgnorant);
	SCRIPT_REG_FUNC(SetAssesmentMultiplier);
	SCRIPT_REG_FUNC(SetSpeciesThreatMultiplier);
	SCRIPT_REG_FUNC(GetGroupCount);
	SCRIPT_REG_FUNC(GetGroupMember);
	SCRIPT_REG_FUNC(GetGroupOf);
	SCRIPT_REG_FUNC(GetGroupAveragePosition);
	SCRIPT_REG_FUNC(Hostile);
	SCRIPT_REG_FUNC(FindObjectOfType);
	SCRIPT_REG_FUNC(SoundEvent);
	SCRIPT_REG_FUNC(GetAnchor);
	SCRIPT_REG_FUNC(GetSpeciesOf);
	SCRIPT_REG_FUNC(GetTypeOf);
	SCRIPT_REG_FUNC(GetSubTypeOf);
	SCRIPT_REG_FUNC(GetAttentionTargetOf);
	SCRIPT_REG_FUNC(GetAttentionTargetPosition);
	SCRIPT_REG_FUNC(GetAttentionTargetDirection);
	SCRIPT_REG_FUNC(GetAttentionTargetViewDirection);
	SCRIPT_REG_FUNC(GetAttentionTargetDistance);
	SCRIPT_REG_FUNC(GetAttentionTargetEntity);
	SCRIPT_REG_FUNC(GetAttentionTargetType);
	SCRIPT_REG_FUNC(GetTargetType);
	SCRIPT_REG_FUNC(GetAIObjectPosition);
	SCRIPT_REG_FUNC(GetBeaconPosition);
	SCRIPT_REG_FUNC(SetBeaconPosition);
	SCRIPT_REG_FUNC(GetTotalLengthOfPath);
	SCRIPT_REG_FUNC(GetNearestEntitiesOfType);
	SCRIPT_REG_FUNC(SetRefPointPosition);
	SCRIPT_REG_FUNC(SetRefPointDirection);
	SCRIPT_REG_FUNC(GetRefPointPosition);
	SCRIPT_REG_FUNC(GetRefPointDirection);
	SCRIPT_REG_FUNC(SetRefPointRadius);
	SCRIPT_REG_FUNC(SetRefShapeName);
	SCRIPT_REG_FUNC(GetRefShapeName);
	SCRIPT_REG_FUNC(SetTerritoryShapeName);
	SCRIPT_REG_FUNC(CreateTempGenericShapeBox);
	SCRIPT_REG_FUNC(GetForwardDir);
	SCRIPT_REG_FUNC(SetCharacter);
	SCRIPT_REG_FUNC(GetAlienApproachParams);
	SCRIPT_REG_FUNC(GetHunterApproachParams);
	SCRIPT_REG_FUNC(VerifyAlienTarget);
	SCRIPT_REG_FUNC(GetDirectAnchorPos);
	SCRIPT_REG_FUNC(GetNearestHidespot);
	SCRIPT_REG_FUNC(GetEnclosingGenericShapeOfType);
	SCRIPT_REG_FUNC(IsPointInsideGenericShape);
	SCRIPT_REG_FUNC(DistanceToGenericShape);
	SCRIPT_REG_FUNC(ConstrainPointInsideGenericShape);
	SCRIPT_REG_FUNC(EvalHidespot);
	SCRIPT_REG_FUNC(EvalPeek);
	SCRIPT_REG_FUNC(NotifyGroupTacticState);
	SCRIPT_REG_FUNC(GetGroupTacticState);
	SCRIPT_REG_FUNC(GetGroupTacticPoint);
	SCRIPT_REG_FUNC(NotifyReinfDone);
	SCRIPT_REG_FUNC(IntersectsForbidden);
	SCRIPT_REG_FUNC(IsPointInFlightRegion);
	SCRIPT_REG_FUNC(IsPointInWaterRegion);
	SCRIPT_REG_FUNC(GetEnclosingSpace);
	SCRIPT_REG_FUNC(Event);
	SCRIPT_REG_FUNC(CreateFormation);
	SCRIPT_REG_FUNC(AddFormationPoint);
	SCRIPT_REG_FUNC(AddFormationPointFixed);
	SCRIPT_REG_FUNC(GetFormationPointClass);
	SCRIPT_REG_FUNC(GetFormationPointPosition);
	SCRIPT_REG_FUNC(ChangeFormation);
	SCRIPT_REG_FUNC(ScaleFormation);
	SCRIPT_REG_FUNC(SetFormationUpdate);
	SCRIPT_REG_FUNC(SetFormationUpdateSight);
	SCRIPT_REG_FUNC(GetLeader);
	SCRIPT_REG_FUNC(SetLeader);
	SCRIPT_REG_FUNC(UpTargetPriority);
	SCRIPT_REG_FUNC(SetExtraPriority);
	SCRIPT_REG_FUNC(GetExtraPriority);
	SCRIPT_REG_FUNC(GetPlayerThreatLevel);
	SCRIPT_REG_FUNC(GetStance);
	SCRIPT_REG_FUNC(SetStance);
	SCRIPT_REG_FUNC(GetUnitInRank);
	SCRIPT_REG_FUNC(SetUnitProperties);
	SCRIPT_REG_FUNC(GetUnitCount);
	SCRIPT_REG_FUNC(IsFlightSpaceVoid);
	SCRIPT_REG_FUNC(IsFlightSpaceVoidByRadius);
	SCRIPT_REG_FUNC(SetForcedNavigation);
	SCRIPT_REG_FUNC(SetAdjustPath);
	SCRIPT_REG_FUNC(GetHeliAdvancePoint);
	SCRIPT_REG_FUNC(CheckVehicleColision);
	SCRIPT_REG_FUNC(GetFlyingVehicleFlockingPos);
	SCRIPT_REG_FUNC(SetPFBlockerRadius);
	SCRIPT_REG_FUNC(SetPFProperties);
	SCRIPT_REG_FUNC(GetAlertStatus);
	SCRIPT_REG_FUNC(GetGroupTarget);
	SCRIPT_REG_FUNC(GetGroupTargetCount);
	SCRIPT_REG_FUNC(RequestAttack);
	SCRIPT_REG_FUNC(GetNavigationType);
	SCRIPT_REG_FUNC(SetPathToFollow);
	SCRIPT_REG_FUNC(SetPathAttributeToFollow);
	SCRIPT_REG_FUNC(GetPredictedPosAlongPath);
	SCRIPT_REG_FUNC(SetPointListToFollow);
	SCRIPT_REG_FUNC(GetNearestPointOnPath);
	SCRIPT_REG_FUNC(GetPathSegNoOnPath);
	SCRIPT_REG_FUNC(GetPointOnPathBySegNo);
	SCRIPT_REG_FUNC(GetPathLoop);
	SCRIPT_REG_FUNC(GetNearestPathOfTypeInRange);
	SCRIPT_REG_FUNC(GetDistanceAlongPath);
	SCRIPT_REG_FUNC(BeginTrackPattern);
	SCRIPT_REG_FUNC(AddPatternNode);
	SCRIPT_REG_FUNC(AddPatternBranch);
	SCRIPT_REG_FUNC(EndTrackPattern);
	SCRIPT_REG_FUNC(SetMinFireTime);
	SCRIPT_REG_FUNC(EnableCoverFire);
	SCRIPT_REG_FUNC(SetRefPointToGrenadeAvoidTarget);
	SCRIPT_REG_FUNC(IsAgentInTargetFOV);
	SCRIPT_REG_FUNC(AddCombatClass);
	SCRIPT_REG_FUNC(SetRefPointAtDefensePos);
	SCRIPT_REG_FUNC(RegisterDamageRegion);
	SCRIPT_REG_FUNC(SetCurrentHideObjectUnreachable);
	SCRIPT_REG_FUNC(FindStandbySpotInShape);
	SCRIPT_REG_FUNC(FindStandbySpotInSphere);
	SCRIPT_REG_FUNC(GetObjectRadius);
	SCRIPT_REG_FUNC(GetProbableTargetPosition);
	SCRIPT_REG_FUNC(NotifySurpriseEntityAction);
	SCRIPT_REG_FUNC(AddObstructSphere);
	SCRIPT_REG_FUNC(Animation);
	SCRIPT_REG_FUNC(CanJumpToPoint);
#ifndef SP_DEMO
	SCRIPT_REG_FUNC(SetRefpointToAlienHidespot);
	SCRIPT_REG_FUNC(MarkAlienHideSpotUnreachable);
#endif
	SCRIPT_REG_FUNC(SetRefpointToAnchor);
	SCRIPT_REG_FUNC(SetRefpointToPunchableObject);
	SCRIPT_REG_FUNC(MeleePunchableObject);
	SCRIPT_REG_FUNC(IsPunchableObjectValid);

	SCRIPT_REG_FUNC(ProcessBalancedDamage);
	SCRIPT_REG_FUNC(CanMoveStraightToPoint);

	SCRIPT_REG_FUNC(CanMelee);
	SCRIPT_REG_FUNC(CheckMeleeDamage);
	SCRIPT_REG_FUNC(IsMoving);

	SCRIPT_REG_FUNC(GetDirLabelToPoint);

	SCRIPT_REG_FUNC(DebugReportHitDamage);
	SCRIPT_REG_FUNC(SetInterestStatus);
	SCRIPT_REG_FUNC(GetInterestStatus);

	SCRIPT_REG_FUNC(PlayReadabilitySound);

	SCRIPT_REG_FUNC(AutoDisable);
	SCRIPT_REG_FUNC(AggressionImpact);
	SCRIPT_REG_FUNC(IsMountedWeaponUsableWithTarget);
//	SCRIPT_REG_FUNC(GetClosestPointToOBB);

	SCRIPT_REG_GLOBAL(AIWEPA_LASER);
	SCRIPT_REG_GLOBAL(AIWEPA_COMBAT_LIGHT);
	SCRIPT_REG_GLOBAL(AIWEPA_PATROL_LIGHT);
	SCRIPT_REG_FUNC(EnableWeaponAccessory);

	SCRIPT_REG_GLOBAL(AIFO_FACING_TARGET);
	SCRIPT_REG_GLOBAL(AIFO_NONOCCUPIED);
	SCRIPT_REG_GLOBAL(AIFO_CHOOSE_RANDOM);
	SCRIPT_REG_GLOBAL(AIFO_NONOCCUPIED_REFPOINT);
	SCRIPT_REG_GLOBAL(AIFO_USE_BEACON_AS_FALLBACK_TGT);
	SCRIPT_REG_GLOBAL(AIFO_NO_DEVALUE);

	SCRIPT_REG_GLOBAL(AIPARAM_SIGHTRANGE);
	SCRIPT_REG_GLOBAL(AIPARAM_ATTACKRANGE);
	SCRIPT_REG_GLOBAL(AIPARAM_ACCURACY);
	SCRIPT_REG_GLOBAL(AIPARAM_GROUPID);
	SCRIPT_REG_GLOBAL(AIPARAM_FOVPRIMARY);
	SCRIPT_REG_GLOBAL(AIPARAM_FOVSECONDARY);
	SCRIPT_REG_GLOBAL(AIPARAM_COMMRANGE);
	SCRIPT_REG_GLOBAL(AIPARAM_FWDSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_SPECIES);
	SCRIPT_REG_GLOBAL(AIPARAM_SPECIESHOSILITY);
	SCRIPT_REG_GLOBAL(AIPARAM_RANK);
	SCRIPT_REG_GLOBAL(AIPARAM_CAMOSCALE);
	SCRIPT_REG_GLOBAL(AIPARAM_HEATSCALE);
	SCRIPT_REG_GLOBAL(AIPARAM_TRACKPATNAME);
	SCRIPT_REG_GLOBAL(AIPARAM_TRACKPATADVANCE);
	SCRIPT_REG_GLOBAL(AIPARAM_STRAFINGPITCH);
	SCRIPT_REG_GLOBAL(AIPARAM_COMBATCLASS);
	SCRIPT_REG_GLOBAL(AIPARAM_INVISIBLE);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONSCALE_VISUAL);
	SCRIPT_REG_GLOBAL(AIPARAM_PERCEPTIONSCALE_AUDIO);
	SCRIPT_REG_GLOBAL(AIPARAM_CLOAK_SCALE);
	SCRIPT_REG_GLOBAL(AIPARAM_FORGETTIME_TARGET);
	SCRIPT_REG_GLOBAL(AIPARAM_FORGETTIME_MEMORY);
	SCRIPT_REG_GLOBAL(AIPARAM_FORGETTIME_SEEK);
	SCRIPT_REG_GLOBAL(AIPARAM_LOOKIDLE_TURNSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_LOOKCOMBAT_TURNSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_AIM_TURNSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_FIRE_TURNSPEED);
	SCRIPT_REG_GLOBAL(AIPARAM_MELEE_DISTANCE);
	SCRIPT_REG_GLOBAL(AIPARAM_MIN_ALARM_LEVEL);
	SCRIPT_REG_GLOBAL(AIPARAM_SIGHTENVSCALE_NORMAL);
	SCRIPT_REG_GLOBAL(AIPARAM_SIGHTENVSCALE_ALARMED);
	SCRIPT_REG_GLOBAL(AIPARAM_GRENADE_THROWDIST);

	SCRIPT_REG_GLOBAL(AIMOVEABILITY_OPTIMALFLIGHTHEIGHT);
	SCRIPT_REG_GLOBAL(AIMOVEABILITY_MINFLIGHTHEIGHT);
	SCRIPT_REG_GLOBAL(AIMOVEABILITY_MAXFLIGHTHEIGHT);
	SCRIPT_REG_GLOBAL(AIMOVEABILITY_TELEPORTENABLE);
	SCRIPT_REG_GLOBAL(AIMOVEABILITY_USEPREDICTIVEFOLLOWING);

	SCRIPT_REG_GLOBAL(AIOBJECT_PUPPET);
	SCRIPT_REG_GLOBAL(AIOBJECT_VEHICLE);
	SCRIPT_REG_GLOBAL(AIOBJECT_CAR);
	SCRIPT_REG_GLOBAL(AIOBJECT_BOAT);
	SCRIPT_REG_GLOBAL(AIOBJECT_HELICOPTER);

	SCRIPT_REG_GLOBAL(AISIGNAL_INCLUDE_DISABLED);
	SCRIPT_REG_GLOBAL(AISIGNAL_DEFAULT);
	SCRIPT_REG_GLOBAL(AISIGNAL_PROCESS_NEXT_UPDATE);
	SCRIPT_REG_GLOBAL(AISIGNAL_NOTIFY_ONLY);
	SCRIPT_REG_GLOBAL(AISIGNAL_ALLOW_DUPLICATES);

	SCRIPT_REG_GLOBAL(AIOBJECT_2D_FLY);

	SCRIPT_REG_GLOBAL(AIOBJECT_ATTRIBUTE);
	SCRIPT_REG_GLOBAL(AIOBJECT_WAYPOINT);
	SCRIPT_REG_GLOBAL(AIOBJECT_SNDSUPRESSOR);
	SCRIPT_REG_GLOBAL(AIOBJECT_MOUNTEDWEAPON);
	SCRIPT_REG_GLOBAL(AIOBJECT_GLOBALALERTNESS);
	SCRIPT_REG_GLOBAL(AIOBJECT_PLAYER);
	SCRIPT_REG_GLOBAL(AIOBJECT_DUMMY);
	SCRIPT_REG_GLOBAL(AIOBJECT_NONE);
	SCRIPT_REG_GLOBAL(AIOBJECT_ORDER);
	SCRIPT_REG_GLOBAL(AIOBJECT_GRENADE);
	SCRIPT_REG_GLOBAL(AIOBJECT_RPG);

	SCRIPT_REG_GLOBAL(AI_USE_HIDESPOTS);

	SCRIPT_REG_GLOBAL(STICK_BREAK);
	SCRIPT_REG_GLOBAL(STICK_SHORTCUTNAV);

	SCRIPT_REG_GLOBAL(GE_GROUP_STATE);
	SCRIPT_REG_GLOBAL(GE_UNIT_STATE);			
	SCRIPT_REG_GLOBAL(GE_ADVANCE_POS);
	SCRIPT_REG_GLOBAL(GE_SEEK_POS);
	SCRIPT_REG_GLOBAL(GE_DEFEND_POS);
	SCRIPT_REG_GLOBAL(GE_LEADER_COUNT);
	SCRIPT_REG_GLOBAL(GE_MOST_LOST_UNIT);
	SCRIPT_REG_GLOBAL(GE_MOVEMENT_SIGNAL);
	SCRIPT_REG_GLOBAL(GE_NEAREST_SEEK);

	SCRIPT_REG_GLOBAL(GN_INIT);
	SCRIPT_REG_GLOBAL(GN_MARK_DEFEND_POS);
	SCRIPT_REG_GLOBAL(GN_CLEAR_DEFEND_POS);
	SCRIPT_REG_GLOBAL(GN_AVOID_CURRENT_POS);
	SCRIPT_REG_GLOBAL(GN_PREFER_ATTACK);
	SCRIPT_REG_GLOBAL(GN_PREFER_FLEE);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_ADVANCING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_COVERING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_WEAK_COVERING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_HIDING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_SEEKING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_ALERTED);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_UNAVAIL);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_IDLE);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_SEARCHING);
	SCRIPT_REG_GLOBAL(GN_NOTIFY_REINFORCE);

	SCRIPT_REG_GLOBAL(GS_IDLE);
	SCRIPT_REG_GLOBAL(GS_COVER);
	SCRIPT_REG_GLOBAL(GS_ADVANCE);
	SCRIPT_REG_GLOBAL(GS_SEEK);
	SCRIPT_REG_GLOBAL(GS_SEARCH);

	SCRIPT_REG_GLOBAL(GU_HUMAN_CAMPER);
	SCRIPT_REG_GLOBAL(GU_HUMAN_COVER);
	SCRIPT_REG_GLOBAL(GU_HUMAN_SNEAKER);
	SCRIPT_REG_GLOBAL(GU_HUMAN_LEADER);
	SCRIPT_REG_GLOBAL(GU_HUMAN_SNEAKER_SPECOP);
	SCRIPT_REG_GLOBAL(GU_ALIEN_MELEE);
	SCRIPT_REG_GLOBAL(GU_ALIEN_ASSAULT);
	SCRIPT_REG_GLOBAL(GU_ALIEN_MELEE_DEFEND);
	SCRIPT_REG_GLOBAL(GU_ALIEN_ASSAULT_DEFEND);
	SCRIPT_REG_GLOBAL(GU_ALIEN_EVADE);

	SCRIPT_REG_GLOBAL(AIEVENT_ONBODYSENSOR);
	SCRIPT_REG_GLOBAL(AIEVENT_ONVISUALSTIMULUS);
	SCRIPT_REG_GLOBAL(AIEVENT_AGENTDIED);
	SCRIPT_REG_GLOBAL(AIEVENT_SLEEP);
	SCRIPT_REG_GLOBAL(AIEVENT_WAKEUP);
	SCRIPT_REG_GLOBAL(AIEVENT_ENABLE);
	SCRIPT_REG_GLOBAL(AIEVENT_DISABLE);
	SCRIPT_REG_GLOBAL(AIEVENT_REJECT);
	SCRIPT_REG_GLOBAL(AIEVENT_PATHFINDON);
	SCRIPT_REG_GLOBAL(AIEVENT_PATHFINDOFF);
	SCRIPT_REG_GLOBAL(AIEVENT_CLEAR);
	SCRIPT_REG_GLOBAL(AIEVENT_DROPBEACON);
	SCRIPT_REG_GLOBAL(AIEVENT_USE);
	SCRIPT_REG_GLOBAL(AIEVENT_CLEARACTIVEGOALS);
	SCRIPT_REG_GLOBAL(AIEVENT_DRIVER_IN);
	SCRIPT_REG_GLOBAL(AIEVENT_DRIVER_OUT);
	SCRIPT_REG_GLOBAL(AIEVENT_FORCEDNAVIGATION);

	SCRIPT_REG_GLOBAL(AISE_GENERIC);
	SCRIPT_REG_GLOBAL(AISE_MOVEMENT);
	SCRIPT_REG_GLOBAL(AISE_MOVEMENT_LOUD);
	SCRIPT_REG_GLOBAL(AISE_WEAPON);
	SCRIPT_REG_GLOBAL(AISE_EXPLOSION);

	SCRIPT_REG_GLOBAL(AI_LOOKAT_CONTINUOUS);
	SCRIPT_REG_GLOBAL(AI_LOOKAT_USE_BODYDIR);

	SCRIPT_REG_GLOBAL(AISYSEVENT_DISABLEMODIFYER);

	SCRIPT_REG_GLOBAL(AIGOALTYPE_UNDEFINED);
	SCRIPT_REG_GLOBAL(AIGOALTYPE_GOTO);
	SCRIPT_REG_GLOBAL(AIGOALTYPE_ATTACK);
	SCRIPT_REG_GLOBAL(AIGOALTYPE_PATH);
	SCRIPT_REG_GLOBAL(AIGOALTYPE_TRANSPORT);
	SCRIPT_REG_GLOBAL(AIGOALTYPE_REINFORCEMENT);
	SCRIPT_REG_GLOBAL(AIGOALTYPE_FOLLOW);

	SCRIPT_REG_GLOBAL(AILASTOPRES_LOOKAT);
	SCRIPT_REG_GLOBAL(AILASTOPRES_USE);
	SCRIPT_REG_GLOBAL(AI_LOOK_FORWARD);
	SCRIPT_REG_GLOBAL(AI_MOVE_RIGHT);
	SCRIPT_REG_GLOBAL(AI_MOVE_LEFT);
	SCRIPT_REG_GLOBAL(AI_MOVE_FORWARD);
	SCRIPT_REG_GLOBAL(AI_MOVE_BACKWARD);
	SCRIPT_REG_GLOBAL(AI_MOVE_BACKLEFT);
	SCRIPT_REG_GLOBAL(AI_MOVE_BACKRIGHT);
	SCRIPT_REG_GLOBAL(AI_MOVE_TOWARDS_GROUP);
	SCRIPT_REG_GLOBAL(AI_USE_TARGET_MOVEMENT);
	SCRIPT_REG_GLOBAL(AI_CHECK_SLOPE_DISTANCE);

	SCRIPT_REG_GLOBAL(AI_REQUEST_PARTIAL_PATH);
	SCRIPT_REG_GLOBAL(AI_BACKOFF_FROM_TARGET);
	SCRIPT_REG_GLOBAL(AI_BREAK_ON_LIVE_TARGET);
	SCRIPT_REG_GLOBAL(AI_RANDOM_ORDER);
	SCRIPT_REG_GLOBAL(AI_USE_TIME);
	SCRIPT_REG_GLOBAL(AI_STOP_ON_ANIMATION_START);
	SCRIPT_REG_GLOBAL(AI_CONSTANT_SPEED);
	SCRIPT_REG_GLOBAL(AI_ADJUST_SPEED);
	SCRIPT_REG_GLOBAL(AI_DONT_STEER_AROUND_TARGET);

	SCRIPT_REG_GLOBAL(FIREMODE_OFF);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST);
	SCRIPT_REG_GLOBAL(FIREMODE_CONTINUOUS);
	SCRIPT_REG_GLOBAL(FIREMODE_FORCED);
	SCRIPT_REG_GLOBAL(FIREMODE_AIM);
	SCRIPT_REG_GLOBAL(FIREMODE_SECONDARY);
	SCRIPT_REG_GLOBAL(FIREMODE_SECONDARY_SMOKE);
	SCRIPT_REG_GLOBAL(FIREMODE_MELEE);
	SCRIPT_REG_GLOBAL(FIREMODE_MELEE_FORCED);
	SCRIPT_REG_GLOBAL(FIREMODE_KILL);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST_WHILE_MOVING);
	SCRIPT_REG_GLOBAL(FIREMODE_PANIC_SPREAD);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST_DRAWFIRE);
	SCRIPT_REG_GLOBAL(FIREMODE_BURST_SNIPE);
	SCRIPT_REG_GLOBAL(FIREMODE_AIM_SWEEP);

	SCRIPT_REG_GLOBAL(CHECKTYPE_MIN_DISTANCE);
	SCRIPT_REG_GLOBAL(CHECKTYPE_MIN_ROOMSIZE);

	SCRIPT_REG_GLOBAL(AITSR_NONE);
	SCRIPT_REG_GLOBAL(AITSR_SEE_STUNT_ACTION);
	SCRIPT_REG_GLOBAL(AITSR_SEE_CLOAKED);

	SCRIPT_REG_GLOBAL(AIGOALPIPE_NOTDUPLICATE);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_LOOP);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_RUN_ONCE);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_HIGHPRIORITY);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_SAMEPRIORITY);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_DONT_RESET_AG);
	SCRIPT_REG_GLOBAL(AIGOALPIPE_KEEP_LAST_SUBPIPE);

	RegisterGlobal("NAV_TRIANGULAR",IAISystem::NAV_TRIANGULAR);
	RegisterGlobal("NAV_ROAD",IAISystem::NAV_ROAD);
	RegisterGlobal("NAV_WAYPOINT_HUMAN",IAISystem::NAV_WAYPOINT_HUMAN);
	RegisterGlobal("NAV_VOLUME",IAISystem::NAV_VOLUME);
	RegisterGlobal("NAV_UNSET",IAISystem::NAV_UNSET);
	RegisterGlobal("NAV_FLIGHT",IAISystem::NAV_FLIGHT);
	RegisterGlobal("NAV_WAYPOINT_3DSURFACE",IAISystem::NAV_WAYPOINT_3DSURFACE);
	RegisterGlobal("NAV_SMARTOBJECT",IAISystem::NAV_SMARTOBJECT);

	RegisterGlobal("GROUP_ALL",IAISystem::GROUP_ALL);
	RegisterGlobal("GROUP_ENABLED",IAISystem::GROUP_ENABLED);
	RegisterGlobal("GROUP_MAX",IAISystem::GROUP_MAX);

	SCRIPT_REG_GLOBAL(LA_NONE);
	SCRIPT_REG_GLOBAL(LA_HIDE);
	SCRIPT_REG_GLOBAL(LA_HOLD);
	SCRIPT_REG_GLOBAL(LA_ATTACK);
	SCRIPT_REG_GLOBAL(LA_SEARCH);
	SCRIPT_REG_GLOBAL(LA_FOLLOW);
	SCRIPT_REG_GLOBAL(LA_USE);
	SCRIPT_REG_GLOBAL(LA_USE_VEHICLE);

	SCRIPT_REG_GLOBAL(LAS_DEFAULT);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FUNNEL);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FLANK);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FLANK_HIDE);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FOLLOW_LEADER);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_ROW);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_CIRCLE);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_LEAPFROG);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_FRONT);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_CHAIN);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_COORDINATED_FIRE1);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_COORDINATED_FIRE2);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_USE_SPOTS);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_HIDE);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_HIDE_COVER);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_SWITCH_POSITIONS);
	SCRIPT_REG_GLOBAL(LAS_ATTACK_CHASE);
	SCRIPT_REG_GLOBAL(LAS_SEARCH_DEFAULT);
	SCRIPT_REG_GLOBAL(LAS_SEARCH_COVER);
	SCRIPT_REG_GLOBAL(UPR_COMBAT_FLIGHT);
	SCRIPT_REG_GLOBAL(UPR_COMBAT_GROUND);
	SCRIPT_REG_GLOBAL(UPR_COMBAT_MARINE);
	SCRIPT_REG_GLOBAL(UPR_COMBAT_RECON);

	SCRIPT_REG_GLOBAL(AITARGET_NONE);
	SCRIPT_REG_GLOBAL(AITARGET_MEMORY);
	SCRIPT_REG_GLOBAL(AITARGET_BEACON);
	SCRIPT_REG_GLOBAL(AITARGET_ENEMY);
	SCRIPT_REG_GLOBAL(AITARGET_SOUND);
	SCRIPT_REG_GLOBAL(AITARGET_GRENADE);
	SCRIPT_REG_GLOBAL(AITARGET_RPG);
	SCRIPT_REG_GLOBAL(AITARGET_FRIENDLY);

	SCRIPT_REG_GLOBAL(AIALERTSTATUS_SAFE);
	SCRIPT_REG_GLOBAL(AIALERTSTATUS_UNSAFE);
	SCRIPT_REG_GLOBAL(AIALERTSTATUS_READY);
	SCRIPT_REG_GLOBAL(AIALERTSTATUS_ACTION);

	SCRIPT_REG_GLOBAL(AIUSEOP_PLANTBOMB);
	SCRIPT_REG_GLOBAL(AIUSEOP_VEHICLE);
	SCRIPT_REG_GLOBAL(AIUSEOP_RPG);

	SCRIPT_REG_GLOBAL(AIREADIBILITY_NORMAL);
	SCRIPT_REG_GLOBAL(AIREADIBILITY_NOPRIORITY);
	SCRIPT_REG_GLOBAL(AIREADIBILITY_SEEN);
	SCRIPT_REG_GLOBAL(AIREADIBILITY_LOST);
	SCRIPT_REG_GLOBAL(AIREADIBILITY_INTERESTING);

	//RegisterGlobal("SIGNALID_THROWGRENADE", -10);
	RegisterGlobal("SIGNALID_READIBILITY", SIGNALFILTER_READIBILITY);
	RegisterGlobal("SIGNALID_READIBILITYAT", SIGNALFILTER_READIBILITYAT);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_LASTOP);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_GROUPONLY);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_GROUPONLY_EXCEPT);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SPECIESONLY);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_ANYONEINCOMM);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_TARGET);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SUPERGROUP);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SUPERSPECIES);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SUPERTARGET);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_NEARESTGROUP);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_NEARESTINCOMM);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_NEARESTINCOMM_SPECIES);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_NEARESTINCOMM_LOOKING);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_HALFOFGROUP);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_SENDER);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_LEADER);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_LEADERENTITY);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_FORMATION);
	SCRIPT_REG_GLOBAL(SIGNALFILTER_FORMATION_EXCEPT);

	SCRIPT_REG_GLOBAL(AIOBJECTFILTER_SAMESPECIES);
	SCRIPT_REG_GLOBAL(AIOBJECTFILTER_SAMEGROUP);
	SCRIPT_REG_GLOBAL(AIOBJECTFILTER_NOGROUP);
	SCRIPT_REG_GLOBAL(AIOBJECTFILTER_INCLUDEINACTIVE);

	SCRIPT_REG_GLOBAL(AI_NOGROUP);

	SCRIPT_REG_GLOBAL(HM_NEAREST);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TO_ME);
	SCRIPT_REG_GLOBAL(HM_FARTHEST_FROM_TARGET);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TO_TARGET);
	SCRIPT_REG_GLOBAL(HM_NEAREST_BACKWARDS);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TOWARDS_TARGET);
	SCRIPT_REG_GLOBAL(HM_FARTHEST_FROM_GROUP);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TO_GROUP);
	SCRIPT_REG_GLOBAL(HM_LEFTMOST_FROM_TARGET);
	SCRIPT_REG_GLOBAL(HM_RIGHTMOST_FROM_TARGET);
	SCRIPT_REG_GLOBAL(HM_RANDOM);
	SCRIPT_REG_GLOBAL(HM_FRONTLEFTMOST_FROM_TARGET);
	SCRIPT_REG_GLOBAL(HM_FRONTRIGHTMOST_FROM_TARGET);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TO_FORMATION);
	SCRIPT_REG_GLOBAL(HM_FARTHEST_FROM_FORMATION);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TO_LASTOPRESULT);
	SCRIPT_REG_GLOBAL(HM_RANDOM_AROUND_LASTOPRESULT);
	SCRIPT_REG_GLOBAL(HM_USEREFPOINT);
	SCRIPT_REG_GLOBAL(HM_ASKLEADER);
	SCRIPT_REG_GLOBAL(HM_ASKLEADER_NOSAME);
	SCRIPT_REG_GLOBAL(HM_BEHIND_VEHICLES);
	SCRIPT_REG_GLOBAL(HM_INCLUDE_SOFTCOVERS);
	SCRIPT_REG_GLOBAL(HM_IGNORE_ENEMIES);
	SCRIPT_REG_GLOBAL(HM_NEAREST_PREFER_SIDES);
//	SCRIPT_REG_GLOBAL(HM_NEAREST_HALF_SIDE);
	SCRIPT_REG_GLOBAL(HM_BACK);
	SCRIPT_REG_GLOBAL(HM_AROUND_LASTOP);
	SCRIPT_REG_GLOBAL(HM_FROM_LASTOP);
	SCRIPT_REG_GLOBAL(HM_USE_LASTOP_RADIUS);
	SCRIPT_REG_GLOBAL(HM_ON_SIDE);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TOWARDS_TARGET_PREFER_SIDES);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TOWARDS_TARGET_LEFT_PREFER_SIDES);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TOWARDS_TARGET_RIGHT_PREFER_SIDES);
	SCRIPT_REG_GLOBAL(HM_NEAREST_TOWARDS_REFPOINT);

	SCRIPT_REG_GLOBAL(BODYPOS_NONE);
	SCRIPT_REG_GLOBAL(BODYPOS_STAND);
	SCRIPT_REG_GLOBAL(BODYPOS_CROUCH);
	SCRIPT_REG_GLOBAL(BODYPOS_PRONE);
	SCRIPT_REG_GLOBAL(BODYPOS_RELAX);
	SCRIPT_REG_GLOBAL(BODYPOS_STEALTH);

	SCRIPT_REG_GLOBAL(UNIT_CLASS_UNDEFINED);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_INFANTRY);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_SCOUT);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_ENGINEER);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_MEDIC);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_LEADER);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_CIVILIAN);
	SCRIPT_REG_GLOBAL(UNIT_CLASS_COMPANION);
	SCRIPT_REG_GLOBAL(SPECIAL_FORMATION_POINT);
	SCRIPT_REG_GLOBAL(SHOOTING_SPOT_POINT);

	SCRIPT_REG_GLOBAL(AIANCHOR_NEAREST);
	SCRIPT_REG_GLOBAL(AIANCHOR_NEAREST_IN_FRONT);
	SCRIPT_REG_GLOBAL(AIANCHOR_RANDOM_IN_RANGE);
	SCRIPT_REG_GLOBAL(AIANCHOR_RANDOM_IN_RANGE_FACING_AT);
	SCRIPT_REG_GLOBAL(AIANCHOR_NEAREST_FACING_AT);	
	SCRIPT_REG_GLOBAL(AIANCHOR_NEAREST_TO_REFPOINT);
	SCRIPT_REG_GLOBAL(AIANCHOR_FARTHEST);
	SCRIPT_REG_GLOBAL(AIANCHOR_BEHIND_IN_RANGE);
	SCRIPT_REG_GLOBAL(AIANCHOR_LEFT_TO_REFPOINT);
	SCRIPT_REG_GLOBAL(AIANCHOR_RIGHT_TO_REFPOINT);
	SCRIPT_REG_GLOBAL(AIANCHOR_HIDE_FROM_REFPOINT);
	SCRIPT_REG_GLOBAL(AIANCHOR_SEES_TARGET);
	SCRIPT_REG_GLOBAL(AIANCHOR_BEHIND);

	SCRIPT_REG_GLOBAL(BRANCH_ALWAYS);
	SCRIPT_REG_GLOBAL(IF_ACTIVE_GOALS);
	SCRIPT_REG_GLOBAL(IF_ACTIVE_GOALS_HIDE);
	SCRIPT_REG_GLOBAL(IF_NO_PATH);
	SCRIPT_REG_GLOBAL(IF_PATH_STILL_FINDING);
	SCRIPT_REG_GLOBAL(IF_IS_HIDDEN);
	SCRIPT_REG_GLOBAL(IF_CAN_HIDE);
	SCRIPT_REG_GLOBAL(IF_CANNOT_HIDE);
	SCRIPT_REG_GLOBAL(IF_STANCE_IS);
	SCRIPT_REG_GLOBAL(IF_FIRE_IS);
	SCRIPT_REG_GLOBAL(IF_HAS_FIRED);
	SCRIPT_REG_GLOBAL(IF_NO_LASTOP);
	SCRIPT_REG_GLOBAL(IF_SEES_LASTOP);
	SCRIPT_REG_GLOBAL(IF_SEES_TARGET);
	SCRIPT_REG_GLOBAL(IF_EXPOSED_TO_TARGET);
	SCRIPT_REG_GLOBAL(IF_CAN_SHOOT_TARGET);
	SCRIPT_REG_GLOBAL(IF_CAN_MELEE);
	SCRIPT_REG_GLOBAL(IF_NO_ENEMY_TARGET);
	SCRIPT_REG_GLOBAL(IF_PATH_LONGER);
	SCRIPT_REG_GLOBAL(IF_PATH_SHORTER);
	SCRIPT_REG_GLOBAL(IF_PATH_LONGER_RELATIVE);
	SCRIPT_REG_GLOBAL(IF_NAV_TRIANGULAR);
	SCRIPT_REG_GLOBAL(IF_NAV_WAYPOINT_HUMAN);
	SCRIPT_REG_GLOBAL(IF_LASTOP_DIST_LESS);
	SCRIPT_REG_GLOBAL(IF_LASTOP_DIST_LESS_ALONG_PATH);
	SCRIPT_REG_GLOBAL(IF_TARGET_DIST_LESS);
	SCRIPT_REG_GLOBAL(IF_TARGET_DIST_LESS_ALONG_PATH);
	SCRIPT_REG_GLOBAL(IF_TARGET_DIST_GREATER);
	SCRIPT_REG_GLOBAL(IF_TARGET_IN_RANGE);
	SCRIPT_REG_GLOBAL(IF_TARGET_OUT_OF_RANGE);
	SCRIPT_REG_GLOBAL(IF_TARGET_TO_REFPOINT_DIST_LESS);
	SCRIPT_REG_GLOBAL(IF_TARGET_TO_REFPOINT_DIST_GREATER);
	SCRIPT_REG_GLOBAL(IF_TARGET_MOVED_SINCE_START);
	SCRIPT_REG_GLOBAL(IF_TARGET_MOVED);
	SCRIPT_REG_GLOBAL(IF_TARGET_LOST_TIME_MORE);
	SCRIPT_REG_GLOBAL(IF_TARGET_LOST_TIME_LESS);
	SCRIPT_REG_GLOBAL(IF_COVER_COMPROMISED);
	SCRIPT_REG_GLOBAL(IF_COVER_NOT_COMPROMISED);
	SCRIPT_REG_GLOBAL(IF_COVER_SOFT);
	SCRIPT_REG_GLOBAL(IF_COVER_NOT_SOFT);
	SCRIPT_REG_GLOBAL(IF_CAN_SHOOT_TARGET_CROUCHED);
	SCRIPT_REG_GLOBAL(IF_COVER_FIRE_ENABLED);
	SCRIPT_REG_GLOBAL(IF_RANDOM);
	SCRIPT_REG_GLOBAL(IF_LASTOP_FAILED);
	SCRIPT_REG_GLOBAL(IF_LASTOP_SUCCEED);
	SCRIPT_REG_GLOBAL(NOT);

	SCRIPT_REG_GLOBAL(AIFAF_VISIBLE_FROM_REQUESTER);
	SCRIPT_REG_GLOBAL(AIFAF_INCLUDE_DEVALUED);
	SCRIPT_REG_GLOBAL(AIFAF_INCLUDE_DISABLED);

	SCRIPT_REG_GLOBAL(AIPATH_DEFAULT);
	SCRIPT_REG_GLOBAL(AIPATH_HUMAN);
	SCRIPT_REG_GLOBAL(AIPATH_HUMAN_COVER);
	SCRIPT_REG_GLOBAL(AIPATH_CAR);
	SCRIPT_REG_GLOBAL(AIPATH_TANK);
	SCRIPT_REG_GLOBAL(AIPATH_BOAT);
	SCRIPT_REG_GLOBAL(AIPATH_HELI);
	SCRIPT_REG_GLOBAL(AIPATH_3D);
	SCRIPT_REG_GLOBAL(AIPATH_SCOUT);
	SCRIPT_REG_GLOBAL(AIPATH_TROOPER);
	SCRIPT_REG_GLOBAL(AIPATH_HUNTER);

	SCRIPT_REG_GLOBAL(AITRACKPAT_CHOOSE_ALWAYS);
	SCRIPT_REG_GLOBAL(AITRACKPAT_CHOOSE_LESS_DEFORMED);
	SCRIPT_REG_GLOBAL(AITRACKPAT_CHOOSE_RANDOM);
	SCRIPT_REG_GLOBAL(AITRACKPAT_CHOOSE_MOST_EXPOSED);
	SCRIPT_REG_GLOBAL(AITRACKPAT_NODE_START);
	SCRIPT_REG_GLOBAL(AITRACKPAT_NODE_ABSOLUTE);
	SCRIPT_REG_GLOBAL(AITRACKPAT_NODE_SIGNAL);
	SCRIPT_REG_GLOBAL(AITRACKPAT_NODE_STOP);
	SCRIPT_REG_GLOBAL(AITRACKPAT_NODE_DIRBRANCH);
	SCRIPT_REG_GLOBAL(AITRACKPAT_VALIDATE_NONE);
	SCRIPT_REG_GLOBAL(AITRACKPAT_VALIDATE_SWEPTSPHERE);
	SCRIPT_REG_GLOBAL(AITRACKPAT_VALIDATE_RAYCAST);
	SCRIPT_REG_GLOBAL(AITRACKPAT_ALIGN_ORIENT_TO_TARGET);
	SCRIPT_REG_GLOBAL(AITRACKPAT_ALIGN_LEVEL_TO_TARGET);
	SCRIPT_REG_GLOBAL(AITRACKPAT_ALIGN_TARGET_DIR);
	SCRIPT_REG_GLOBAL(AITRACKPAT_ALIGN_RANDOM);

	SCRIPT_REG_GLOBAL(PFB_NONE);
	SCRIPT_REG_GLOBAL(PFB_ATT_TARGET);
	SCRIPT_REG_GLOBAL(PFB_REF_POINT);
	SCRIPT_REG_GLOBAL(PFB_BEACON);
	SCRIPT_REG_GLOBAL(PFB_DEAD_BODIES);
	SCRIPT_REG_GLOBAL(PFB_EXPLOSIVES);
	SCRIPT_REG_GLOBAL(PFB_PLAYER);
	SCRIPT_REG_GLOBAL(PFB_BETWEEN_NAV_TARGET);

	SCRIPT_REG_GLOBAL(COVER_UNHIDE);
	SCRIPT_REG_GLOBAL(COVER_HIDE);

	SCRIPT_REG_GLOBAL(AIPROX_SIGNAL_ON_OBJ_DISABLE);
	SCRIPT_REG_GLOBAL(AIPROX_VISIBLE_TARGET_ONLY);

	SCRIPT_REG_GLOBAL(AIANIM_SIGNAL);
	SCRIPT_REG_GLOBAL(AIANIM_ACTION);

	SCRIPT_REG_GLOBAL(WAIT_ALL);
	SCRIPT_REG_GLOBAL(WAIT_ANY);
	SCRIPT_REG_GLOBAL(WAIT_ANY_2);

	SCRIPT_REG_GLOBAL(SOUND_INTERESTING);
	SCRIPT_REG_GLOBAL(SOUND_THREATENING);

	SCRIPT_REG_GLOBAL(AI_JUMP_CHECK_COLLISION);
	SCRIPT_REG_GLOBAL(AI_JUMP_ON_GROUND);
	SCRIPT_REG_GLOBAL(AI_JUMP_RELATIVE);
	SCRIPT_REG_GLOBAL(AI_JUMP_MOVING_TARGET);

	SCRIPT_REG_GLOBAL(JUMP_ANIM_FLY);
	SCRIPT_REG_GLOBAL(JUMP_ANIM_LAND);


	RunStartupScript();
	InitLookUp( );
}

//====================================================================
// ~CScriptBind_AI
//====================================================================
CScriptBind_AI::~CScriptBind_AI(void)
{
//	m_CustomOnSeenSignals.clear();

	gEnv->pConsole->RemoveCommand("ai_ReloadLookUp");
	gEnv->pConsole->RemoveCommand("ai_ReadabilityReload");
	gEnv->pConsole->RemoveCommand("ai_ReadabilityTest");
	gEnv->pConsole->RemoveCommand("ai_CalcHumanMovementTable");
}


//====================================================================
// RunStartupScript
//====================================================================
void CScriptBind_AI::RunStartupScript( )
{
	m_pSS->ExecuteFile("scripts/ai/aiconfig.lua", true );

	CAIHandler::s_ReadabilityManager.Reload();
	CAIFaceManager::LoadStatic();
}


//====================================================================
// InitConVars
//====================================================================
void CScriptBind_AI::InitConVars( )
{
	gEnv->pConsole->AddCommand( "ai_ReloadLookUp", ReloadLookUpXML, 0, "Reloads perception distance scale look-up xml files." );
	gEnv->pConsole->AddCommand( "ai_ReadabilityReload", ReloadReadabilityXML, 0, "Reloads readability xml files." );
	gEnv->pConsole->AddCommand( "ai_ReadabilityTest", TestReadability, 0,
		"Tests the readability pack of specified entity.\n"
		"If no readability name is specified all readabilities will be played.\n"
		"Usage: ai_ReadabilityTest <entity name> [readability name]   - to play a readability\n"
		"       ai_ReadabilityTest stop   - to stop the currently playing readability set.");
	gEnv->pConsole->AddCommand( "ai_CalcHumanMovementTable", CalcHumanMovementTable, 0, "" );
}


//====================================================================
// Warning
//====================================================================
int CScriptBind_AI::Warning(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char *sParam=NULL;
	pH->GetParam(1,sParam);
	if (sParam && m_pAISystem)
		m_pAISystem->Warning("<Lua> ", sParam);
	return (pH->EndFunction());
}

//====================================================================
// Error
//====================================================================
int CScriptBind_AI::Error(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char *sParam=NULL;
	pH->GetParam(1,sParam);
	if (sParam && m_pAISystem)
		m_pAISystem->Error("<Lua> ", sParam);
	return (pH->EndFunction());
}

//====================================================================
// LogProgress
//====================================================================
int CScriptBind_AI::LogProgress(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char *sParam=NULL;
	pH->GetParam(1,sParam);
	if (sParam && m_pAISystem)
		m_pAISystem->LogProgress("<Lua> ", sParam);
	return (pH->EndFunction());
}

//====================================================================
// LogEvent
//====================================================================
int CScriptBind_AI::LogEvent(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char *sParam=NULL;
	pH->GetParam(1,sParam);
	if (sParam && m_pAISystem)
		m_pAISystem->LogEvent("<Lua> ", sParam);
	return (pH->EndFunction());
}

//====================================================================
// LogComment
//====================================================================
int CScriptBind_AI::LogComment(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	const char *sParam=NULL;
	pH->GetParam(1,sParam);
	if (sParam && m_pAISystem)
		m_pAISystem->LogComment("<Lua> ", sParam);
	return (pH->EndFunction());
}

//====================================================================
// RecComment
//====================================================================
int CScriptBind_AI::RecComment(IFunctionHandler *pH)
{
	//	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	const char *sComment=NULL;
	pH->GetParam(2, sComment);
	if (sComment && pEntity && pEntity->GetAI())
	{
		IAIRecordable::RecorderEventData recorderEventData(sComment);
		pEntity->GetAI()->RecordEvent(IAIRecordable::E_LUACOMMENT, &recorderEventData);
	}
	return (pH->EndFunction());
}

/// character that travels on the surface but has no preferences - except it prefers to walk around
/// hills rather than over them
static const AgentPathfindingProperties sDefaultCharacterProperties(
	IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_SMARTOBJECT, 
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
	5.0f, 0.5f, -10000.0f, 0.0f, 20.0f, 7.0f);

/// character likes cover
static const AgentPathfindingProperties sDefaultCharacterCoverProperties(
	IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_SMARTOBJECT, 
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	5.0f, 0.5f, -10000.0f, 10.0f, 20.0f, 7.0f);

/// character that prefers (x10) to travel on roads
static const AgentPathfindingProperties sDefaultCharacterRoadProperties(
	IAISystem::NAVMASK_SURFACE , 
	18.0f, 18.0f, 0.0f, 0.0f, 0.0f, 
	5.0f, 1.5f, -10000.0f, 0.0f, 20.0f, 7.0f);

/// Default properties for car/vehicle agents
static const AgentPathfindingProperties sDefaultCarProperties(
	IAISystem::NAVMASK_SURFACE, 
	18.0f, 18.0f, 0.0f, 0.0f, 0.0f, 
	0.0f, 1.5f, -10000.0f, 0.0f, 0.0f, 7.0f);

/// Default properties for boat agents
static const AgentPathfindingProperties sDefaultBoatProperties(
	IAISystem::NAV_TRIANGULAR | IAISystem::NAV_ROAD, 
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 10000.0f, 1.5f, 0.0f, 0.0f, 0.0f);

/// Default properties for flight (heli, scout etc) agents
static const AgentPathfindingProperties sDefaultFlightProperties(
	IAISystem::NAV_FLIGHT | IAISystem::NAV_SMARTOBJECT, 
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

/// Default properties for flight (heli, scout etc) agents, allow 3D navigation too.
static const AgentPathfindingProperties sDefaultScout(
	IAISystem::NAV_FLIGHT | IAISystem::NAV_VOLUME | IAISystem::NAV_SMARTOBJECT, 
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

/// character likes cover, allow 3D navigation too.
static const AgentPathfindingProperties sDefaultTrooper(
	IAISystem::NAV_TRIANGULAR | IAISystem::NAV_WAYPOINT_HUMAN /*| IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_VOLUME*/  | IAISystem::NAV_SMARTOBJECT, 
	0.0f, 0.0f, 0.0f, 5.0f, 0.0f,
	5.0f, 10000.0f, -10000.0f, 10.0f, 20.0f, 7.0f);


/// Default properties for 3D agents
static const AgentPathfindingProperties sDefaultVolumeProperties(
	IAISystem::NAV_VOLUME | IAISystem::NAV_SMARTOBJECT | IAISystem::NAV_ROAD, 
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

/// Default properties for hunter agents
static const AgentPathfindingProperties sDefaultHunterProperties(
	IAISystem::NAV_FREE_2D /* | IAISystem::NAV_WAYPOINT_3DSURFACE *//*| IAISystem::NAV_SMARTOBJECT*/,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);


/*
//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddSmartObjectCondition(IFunctionHandler *pH)
{
SCRIPT_CHECK_PARAMETERS( 1 );
SmartScriptTable pTable, pObject, pUser, pLimits, pDelay, pMultipliers, pAction; 
pH->GetParam( 1, pTable );

const char* temp;
SmartObjectCondition condition;
pTable->GetValue( "sName", temp ); condition.sName = temp;
pTable->GetValue( "sDescription", temp ); condition.sDescription = temp;
pTable->GetValue( "sFolder", temp ); condition.sFolder = temp;
pTable->GetValue( "iMaxAlertness", condition.iMaxAlertness );
pTable->GetValue( "bEnabled", condition.bEnabled );
pTable->GetValue( "iRuleType", condition.iRuleType );
pTable->GetValue( "sohelper_EntranceHelper", temp ); condition.sEntranceHelper = temp;
pTable->GetValue( "sohelper_ExitHelper", temp ); condition.sExitHelper = temp;
pTable->GetValue( "Object", pObject );
pObject->GetValue( "soclass_Class", temp ); condition.sObjectClass = temp;
pObject->GetValue( "sopattern_State", temp ); condition.sObjectState = temp;
pObject->GetValue( "sohelper_Helper", temp ); condition.sObjectHelper = temp;
pTable->GetValue( "User", pUser );
pUser->GetValue( "soclass_Class", temp ); condition.sUserClass = temp;
pUser->GetValue( "soPattern_State", temp ); condition.sUserState = temp;
pUser->GetValue( "sohelper_Helper", temp ); condition.sUserHelper = temp;
pTable->GetValue( "Limits", pLimits );
pLimits->GetValue( "fDistanceFrom", condition.fDistanceFrom );
pLimits->GetValue( "fDistanceTo", condition.fDistanceTo );
pLimits->GetValue( "fOrientation", condition.fOrientationLimit );
pLimits->GetValue( "fOrientToTargetLimit", condition.fOrientationToTargetLimit );
pTable->GetValue( "Delay", pDelay );
pDelay->GetValue( "fMinimum", condition.fMinDelay );
pDelay->GetValue( "fMaximum", condition.fMaxDelay );
pDelay->GetValue( "fMemory", condition.fMemory );
pTable->GetValue( "Multipliers", pMultipliers );
pMultipliers->GetValue( "fProximity", condition.fProximityFactor );
pMultipliers->GetValue( "fOrientation", condition.fOrientationFactor );
pMultipliers->GetValue( "fVisibility", condition.fVisibilityFactor );
pMultipliers->GetValue( "fRandomness", condition.fRandomnessFactor );
pTable->GetValue( "Action", pAction );
pAction->GetValue( "fLookAtOnPerc", condition.fLookAtOnPerc );
pAction->GetValue( "sostates_ObjectPostActionState", temp ); condition.sObjectPostActionState = temp;
pAction->GetValue( "sostates_UserPostActionState", temp ); condition.sUserPostActionState = temp;
pAction->GetValue( "soaction_Name", temp ); condition.sAction = temp;
pAction->GetValue( "sostates_ObjectPreActionState", temp ); condition.sObjectPreActionState = temp;
pAction->GetValue( "sostates_UserPreActionState", temp ); condition.sUserPreActionState = temp;

m_pAISystem->AddSmartObjectCondition( condition );
return pH->EndFunction();
}
*/

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::ExecuteAction(IFunctionHandler *pH)
{
	const char* sActionName = NULL;
	if (!pH->GetParam( 1, sActionName ))
	{
		gEnv->pAISystem->LogComment("<CScriptBind_AI::ExecuteAction> ", "ERROR: AI Action (param 1) must reference the Action name!");
		return pH->EndFunction();
	}

	/*
	ExecuteActionData eaData( pAction );

	ScriptHandle hdl;
	for ( int i = 2; pH->GetParam( i, hdl ); ++i )
	{
	int nID = hdl.n;
	IEntity* pEntity = m_pSystem->GetIEntitySystem()->GetEntity( nID );
	if ( pEntity )
	{
	eaData.AddParticipant( pEntity );
	}
	else
	{
	gEnv->pAISystem->LogComment("<CScriptBind_AI> ", "ERROR: Specified entity id (param %d) not found!", i);
	return pH->EndFunction();
	}
	}

	m_pAISystem->ExecuteSmartObjectAction( eaData );
	*/

	IEntity* pUser = NULL;
	IEntity* pObject = NULL;

	ScriptHandle hdl;
	if (!pH->GetParam( 2, hdl ))
	{
		gEnv->pAISystem->LogComment( "<CScriptBind_AI::ExecuteAction> ", "ERROR: Not an entity id (param 2)!" );
		return pH->EndFunction();
	}
	else
	{
		pUser = m_pSystem->GetIEntitySystem()->GetEntity( hdl.n );
		if ( !pUser )
		{
			gEnv->pAISystem->LogComment( "<CScriptBind_AI::ExecuteAction> ", "ERROR: Specified entity id (param 2) not found!" );
			return pH->EndFunction();
		}
	}

	if (!pH->GetParam( 3, hdl ))
	{
		gEnv->pAISystem->LogComment( "<CScriptBind_AI::ExecuteAction> ", "ERROR: Not an entity id (param 3)!" );
		return pH->EndFunction();
	}
	else
	{
		pObject = m_pSystem->GetIEntitySystem()->GetEntity( hdl.n );
		if ( !pObject )
		{
			gEnv->pAISystem->LogComment( "<CScriptBind_AI::ExecuteAction> ", "ERROR: Specified entity id (param 3) not found!" );
			return pH->EndFunction();
		}
	}

	int maxAlertness = 0;
	pH->GetParam( 4, maxAlertness );

	int goalPipeId = 0;
	if ( pH->GetParamCount() > 4 )
		pH->GetParam( 5, goalPipeId );

	m_pAISystem->ExecuteAIAction( sActionName, pUser, pObject, maxAlertness, goalPipeId );

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AbortAction(IFunctionHandler *pH)
{
	IEntity* pUser = NULL;

	ScriptHandle hdl;
	if (!pH->GetParam( 1, hdl ))
	{
		gEnv->pAISystem->LogComment( "<CScriptBind_AI::AbortAction> ", "ERROR: Not an entity id (param 1)!" );
		return pH->EndFunction();
	}
	else
	{
		pUser = m_pSystem->GetIEntitySystem()->GetEntity( hdl.n );
		if ( !pUser )
		{
			gEnv->pAISystem->LogComment( "<CScriptBind_AI::AbortAction> ", "ERROR: Specified entity id (param 1) not found!" );
			return pH->EndFunction();
		}
	}

	int goalPipeId = 0;
	if ( pH->GetParamCount() > 1 )
		pH->GetParam( 2, goalPipeId );

	m_pAISystem->AbortAIAction( pUser, goalPipeId );

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetSmartObjectState(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	const char* stateName;
	if(pEntity)
	{
		pH->GetParam(2,stateName);
		m_pAISystem->SetSmartObjectState(pEntity,stateName);
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::ModifySmartObjectStates(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if ( pEntity )
	{
		const char* listStates;
		pH->GetParam( 2, listStates );
		m_pAISystem->ModifySmartObjectStates( pEntity, listStates );
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SmartObjectEvent(IFunctionHandler *pH)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_AI );

	const char* sActionName = NULL;
	if ( !pH->GetParam(1, sActionName) )
	{
		gEnv->pAISystem->Error("<CScriptBind_AI::SmartObjectEvent> ", "ERROR: The first parameter must be the name of the smart action!");
		return pH->EndFunction();
	}

	IEntity* pUser = NULL;
	IEntity* pObject = NULL;

	ScriptHandle hdl;
	if ( pH->GetParamCount() > 1 && pH->GetParamType(2) != svtNull && pH->GetParam(2, hdl) )
	{
		pUser = m_pSystem->GetIEntitySystem()->GetEntity( hdl.n );
		if ( !pUser )
			gEnv->pAISystem->Warning( "<CScriptBind_AI::SmartObjectEvent> ", "WARNING: Specified entity id (param 2) not found!" );
	}

	if ( pH->GetParamCount() > 2 && pH->GetParamType(3) != svtNull && pH->GetParam(3, hdl) )
	{
		pObject = m_pSystem->GetIEntitySystem()->GetEntity( hdl.n );
		if ( !pObject )
			gEnv->pAISystem->Warning( "<CScriptBind_AI::SmartObjectEvent> ", "WARNING: Specified entity id (param 3) not found!" );
	}

	bool bUseRefPoint = false;
	Vec3 vRefPoint;
	if ( pH->GetParamCount() > 3 && pH->GetParamType(4) != svtNull && pH->GetParam(4, vRefPoint) )
		bUseRefPoint = true;

	int id = m_pAISystem->SmartObjectEvent( sActionName, pUser, pObject, bUseRefPoint ? &vRefPoint : NULL );
	return pH->EndFunction( id ); // returns the id of the inserted goal pipe or 0 if no rule was found
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetLastUsedSmartObject(IFunctionHandler *pH)
{
	GET_ENTITY(1);
	if ( !pEntity )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetLastUsedSmartObject: Entity with id [%d] doesn't exist.", nID);
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if ( !pAI )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetLastUsedSmartObject: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if ( !pPipeUser )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetLastUsedSmartObject: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}

	IEntity* pObject = m_pSystem->GetIEntitySystem()->GetEntity( pPipeUser->GetLastUsedSmartObjectId() );
	if ( pObject )
		return pH->EndFunction( pObject->GetScriptTable() ); // returns the script table of the last used smart object entity
	else
		return pH->EndFunction(); // there's no last used smart object - return nil
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetLastSmartObjectExitPoint(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if ( !pEntity )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetLastSmartObjectExitPoint: Entity with id [%d] doesn't exist.", nID);
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if ( !pAI )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetLastSmartObjectExitPoint: Entity [%s] is not registered in AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if ( !pPipeUser )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetLastSmartObjectExitPoint: Entity [%s] is registered in AI but not as a PipeUser.", pEntity->GetName());
		return pH->EndFunction();
	}
	
	SmartScriptTable stPos;
	if(!pH->GetParam(2,stPos))
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetLastSmartObjectExitPoint: wrong parameter 2 format");
		return pH->EndFunction();
	}

	IEntity* pObject = m_pSystem->GetIEntitySystem()->GetEntity( pPipeUser->GetLastUsedSmartObjectId() );
	if ( pObject )
	{
		Vec3 pos = pPipeUser->GetLastSOExitHelperPos();
		if(!pos.IsZero())
		{
			stPos->SetValue("x",pos.x);
			stPos->SetValue("y",pos.y);
			stPos->SetValue("z",pos.z);
			return pH->EndFunction(true); 
		}
	}

	return pH->EndFunction(); 
}

//
//-----------------------------------------------------------------------------------------------------------
bool CScriptBind_AI::ParseTables(int firstTable, bool parseMovementAbility, IFunctionHandler* pH, AIObjectParameters& params, bool& autoDisable)
{
	SmartScriptTable pTable;
	SmartScriptTable pTableInstance;
	SmartScriptTable pMovementTable;
	SmartScriptTable pMeleeTable;
	// Properties table
	if (pH->GetParamCount() > firstTable-1)
		pH->GetParam(firstTable, pTable);
	else
		return false;
	// Instance Properties table
	if (pH->GetParamCount() > firstTable)
		pH->GetParam(firstTable+1, pTableInstance);
	else
		return false;
	if (parseMovementAbility)
	{
		// Movement Abilities table
		if (pH->GetParamCount() > firstTable+1)
			pH->GetParam(firstTable+2, pMovementTable);
		else
			return false;
	}

	if (!pTable->GetValue("special",params.m_sParamStruct.m_bSpecial))
		params.m_sParamStruct.m_bSpecial = false;

	pTable->GetValue("bSpeciesHostility",params.m_sParamStruct.m_bSpeciesHostility);
	pTable->GetValue("bAffectSOM",params.m_sParamStruct.m_bPerceivePlayer);
	pTable->GetValue("awarenessOfPlayer",params.m_sParamStruct.m_fAwarenessOfPlayer);
	pTable->GetValue("fGroupHostility",params.m_sParamStruct.m_fGroupHostility);
	if (!pTable->GetValue("attackrange",params.m_sParamStruct.m_fAttackRange))
		pTableInstance->GetValue("attackrange",params.m_sParamStruct.m_fAttackRange);
	if (!pTable->GetValue("commrange",params.m_sParamStruct.m_fCommRange))
		pTableInstance->GetValue("commrange",params.m_sParamStruct.m_fCommRange);
	if (!pTable->GetValue("strafingPitch",params.m_sParamStruct.m_fStrafingPitch))
		params.m_sParamStruct.m_fStrafingPitch = 0.0f;
	if (!pTable->GetValue("accuracy",params.m_sParamStruct.m_fAccuracy))
		pTableInstance->GetValue("accuracy",params.m_sParamStruct.m_fAccuracy);
	if (!pTable->GetValue("groupid",params.m_sParamStruct.m_nGroup))
		pTableInstance->GetValue("groupid",params.m_sParamStruct.m_nGroup);
	if (!pTable->GetValue("rank",params.m_sParamStruct.m_nRank))
		pTableInstance->GetValue("rank",params.m_sParamStruct.m_nRank);

	pTable->GetValue("species",params.m_sParamStruct.m_nSpecies);       
	pTableInstance->GetValue("bAutoDisable",autoDisable);       
	pTable->GetValue("distanceToHideFrom",params.m_sParamStruct.m_fDistanceToHideFrom);
	pTable->GetValue("preferredCombatDistance",params.m_sParamStruct.m_fPreferredCombatDistance);

	params.m_sParamStruct.m_bAiIgnoreFgNode = false;

	float maneuverSpeed, walkSpeed, runSpeed, sprintSpeed;

	if (parseMovementAbility)
	{
		pMovementTable->GetValue( "b3DMove",		params.m_moveAbility.b3DMove);
		pMovementTable->GetValue( "usePathfinder",	params.m_moveAbility.bUsePathfinder);
		pMovementTable->GetValue( "usePredictiveFollowing",	params.m_moveAbility.usePredictiveFollowing);
		pMovementTable->GetValue( "allowEntityClampingByAnimation",	params.m_moveAbility.allowEntityClampingByAnimation);
		pMovementTable->GetValue( "walkSpeed",		walkSpeed );
		pMovementTable->GetValue( "runSpeed",		runSpeed );
		pMovementTable->GetValue( "sprintSpeed",		sprintSpeed );
		pMovementTable->GetValue( "maxAccel",		params.m_moveAbility.maxAccel );
		pMovementTable->GetValue( "maxDecel",		params.m_moveAbility.maxDecel );
		pMovementTable->GetValue( "maneuverSpeed", maneuverSpeed );
		pMovementTable->GetValue( "minTurnRadius", params.m_moveAbility.minTurnRadius );
		pMovementTable->GetValue( "maxTurnRadius", params.m_moveAbility.maxTurnRadius );
		pMovementTable->GetValue( "pathLookAhead", params.m_moveAbility.pathLookAhead);
		pMovementTable->GetValue( "pathRadius", params.m_moveAbility.pathRadius);
		pMovementTable->GetValue( "pathSpeedLookAheadPerSpeed", params.m_moveAbility.pathSpeedLookAheadPerSpeed);
		pMovementTable->GetValue( "cornerSlowDown", params.m_moveAbility.cornerSlowDown);
		pMovementTable->GetValue( "slopeSlowDown", params.m_moveAbility.slopeSlowDown);
		pMovementTable->GetValue( "maneuverTrh", params.m_moveAbility.maneuverTrh );
		pMovementTable->GetValue( "velDecay", params.m_moveAbility.velDecay );
		// for flying.
		pMovementTable->GetValue( "optimalFlightHeight", params.m_moveAbility.optimalFlightHeight);
		pMovementTable->GetValue( "minFlightHeight", params.m_moveAbility.minFlightHeight);
		pMovementTable->GetValue( "maxFlightHeight", params.m_moveAbility.maxFlightHeight);

		pMovementTable->GetValue( "passRadius", params.m_sParamStruct.m_fPassRadius );
		pMovementTable->GetValue( "attackZoneHeight", params.m_sParamStruct.m_fAttackZoneHeight );

		pMovementTable->GetValue( "pathFindPrediction", params.m_moveAbility.pathFindPrediction );

		pMovementTable->GetValue( "lookIdleTurnSpeed", params.m_sParamStruct.m_lookIdleTurnSpeed);
		pMovementTable->GetValue( "lookCombatTurnSpeed", params.m_sParamStruct.m_lookCombatTurnSpeed);
		pMovementTable->GetValue( "aimTurnSpeed", params.m_sParamStruct.m_aimTurnSpeed);
		pMovementTable->GetValue( "fireTurnSpeed", params.m_sParamStruct.m_fireTurnSpeed);
		params.m_sParamStruct.m_lookIdleTurnSpeed = DEG2RAD(params.m_sParamStruct.m_lookIdleTurnSpeed);
		params.m_sParamStruct.m_lookCombatTurnSpeed = DEG2RAD(params.m_sParamStruct.m_lookCombatTurnSpeed);
		params.m_sParamStruct.m_aimTurnSpeed = DEG2RAD(params.m_sParamStruct.m_aimTurnSpeed);
		params.m_sParamStruct.m_fireTurnSpeed = DEG2RAD(params.m_sParamStruct.m_fireTurnSpeed);

		pMovementTable->GetValue( "resolveStickingInTrace", params.m_moveAbility.resolveStickingInTrace );
		pMovementTable->GetValue( "pathRegenIntervalDuringTrace", params.m_moveAbility.pathRegenIntervalDuringTrace );
		pMovementTable->GetValue( "avoidanceRadius", params.m_moveAbility.avoidanceRadius );
		pMovementTable->GetValue( "lightAffectsSpeed", params.m_moveAbility.lightAffectsSpeed );

		pMovementTable->GetValue( "directionalScaleRefSpeedMin", params.m_moveAbility.directionalScaleRefSpeedMin);
		pMovementTable->GetValue( "directionalScaleRefSpeedMax", params.m_moveAbility.directionalScaleRefSpeedMax);

		int pathType(AIPATH_DEFAULT);
		pMovementTable->GetValue( "pathType", pathType );
		SetPFProperties(params.m_moveAbility, pathType);

		params.m_moveAbility.movementSpeeds.SetBasicSpeeds(0.5f * walkSpeed, walkSpeed, runSpeed, sprintSpeed, maneuverSpeed);

		const char* tableName = 0;
		SmartScriptTable pSpeedTable;
		if (pMovementTable->GetValue("AIMovementSpeeds", pSpeedTable))
		{
			SmartScriptTable pStanceSpeedTable;
			for (int s = 0; s < AgentMovementSpeeds::AMS_NUM_VALUES; ++s)
			{
				switch (s)
				{
				case AgentMovementSpeeds::AMS_RELAXED: tableName = "Relaxed"; break;
				case AgentMovementSpeeds::AMS_COMBAT: tableName = "Combat"; break;
				case AgentMovementSpeeds::AMS_STEALTH: tableName = "Stealth"; break;
				case AgentMovementSpeeds::AMS_CROUCH: tableName = "Crouch"; break;
				case AgentMovementSpeeds::AMS_PRONE: tableName = "Prone"; break;
				case AgentMovementSpeeds::AMS_SWIM: tableName = "Swim"; break;
				default: tableName = "invalid"; break;
				}
				if (pSpeedTable->GetValue(tableName, pStanceSpeedTable))
				{
					for (int u = 0 ; u < AgentMovementSpeeds::AMU_NUM_VALUES-1; ++u)
					{
						switch (u)
						{
						case AgentMovementSpeeds::AMU_SLOW:		tableName = "Slow"; break;
						case AgentMovementSpeeds::AMU_WALK:		tableName = "Walk"; break;
						case AgentMovementSpeeds::AMU_RUN:		tableName = "Run"; break;
						case AgentMovementSpeeds::AMU_SPRINT:	tableName = "Sprint"; break;
						default: tableName = "invalid"; break;
						}
						SmartScriptTable pUrgencySpeedTable;
						if (pStanceSpeedTable->GetValue(tableName, pUrgencySpeedTable))
						{
							float sdef = 0, smin = 0, smax = 0;
							pUrgencySpeedTable->GetAt(1, sdef);
							pUrgencySpeedTable->GetAt(2, smin);
							pUrgencySpeedTable->GetAt(3, smax);

							params.m_moveAbility.movementSpeeds.SetRanges(s, u, sdef,smin,smax);
						}
						else
						{
							if (u > 0)
								params.m_moveAbility.movementSpeeds.CopyRanges(s, u, u-1);
//							params.m_moveAbility.movementSpeeds.SetRanges(s, u, -1, -1,-1, -1,-1, -1,-1);
						}
					}
				}
				else
				{
					gEnv->pAISystem->Warning("<CScriptBind_AI> ", "ParseTables: Unable to get element %s from AIMovementSpeeds table", tableName);
				}
			}

		}
	}

	// perception table
	SmartScriptTable pPerceptionTable;
	if(pTable->GetValue("Perception",pPerceptionTable))
	{
		pPerceptionTable->GetValue( "camoScale", params.m_sParamStruct.m_PerceptionParams.camoScale);
		pPerceptionTable->GetValue( "velBase", params.m_sParamStruct.m_PerceptionParams.velBase);
		pPerceptionTable->GetValue( "velScale", params.m_sParamStruct.m_PerceptionParams.velScale);
		pPerceptionTable->GetValue( "FOVSecondary", params.m_sParamStruct.m_PerceptionParams.FOVSecondary);
		pPerceptionTable->GetValue( "FOVPrimary", params.m_sParamStruct.m_PerceptionParams.FOVPrimary);
		pPerceptionTable->GetValue( "stanceScale", params.m_sParamStruct.m_PerceptionParams.stanceScale);
		pPerceptionTable->GetValue( "sightrange", params.m_sParamStruct.m_PerceptionParams.sightRange);
		pPerceptionTable->GetValue( "sightrangeVehicle", params.m_sParamStruct.m_PerceptionParams.sightRangeVehicle);
		pPerceptionTable->GetValue( "audioScale", params.m_sParamStruct.m_PerceptionParams.audioScale);
		pPerceptionTable->GetValue( "heatScale", params.m_sParamStruct.m_PerceptionParams.heatScale);
		pPerceptionTable->GetValue( "persistence", params.m_sParamStruct.m_PerceptionParams.targetPersistence);
		pPerceptionTable->GetValue( "bulletHitRadius", params.m_sParamStruct.m_PerceptionParams.bulletHitRadius);
		pPerceptionTable->GetValue( "bThermalVision", params.m_sParamStruct.m_PerceptionParams.bThermalVision );
		pPerceptionTable->GetValue( "StuntTimeout", params.m_sParamStruct.m_PerceptionParams.StuntTimeout );
		pPerceptionTable->GetValue( "bIsAffectedByLight", params.m_sParamStruct.m_PerceptionParams.isAffectedByLight );
		pPerceptionTable->GetValue( "minAlarmLevel", params.m_sParamStruct.m_PerceptionParams.minAlarmLevel );
		Limit(params.m_sParamStruct.m_PerceptionParams.minAlarmLevel, 0.0f, 1.0f);
		pTable->GetValue("stuntReactionTimeOut",params.m_sParamStruct.m_PerceptionParams.stuntReactionTimeOut);
		pTable->GetValue("collisionReactionScale",params.m_sParamStruct.m_PerceptionParams.collisionReactionScale);
	}

	if (pH->GetParamCount() > firstTable+2 && pH->GetParamType(firstTable+3)==svtObject)
		if(pH->GetParam(firstTable+3,pMeleeTable))
			pMeleeTable->GetValue("damageRadius",params.m_sParamStruct.m_fMeleeDistance);
	return true;
}

//
//-----------------------------------------------------------------------------------------------------------
void CScriptBind_AI::SetPFProperties(AgentMovementAbility& moveAbility, int pathType) const
{
	switch((EAIPathType)pathType)
	{
	case AIPATH_DEFAULT:
		moveAbility.pathfindingProperties = sDefaultCharacterProperties;
		break;
	case AIPATH_HUMAN:
		moveAbility.pathfindingProperties = sDefaultCharacterProperties;
		break;
	case AIPATH_HUMAN_COVER:
		moveAbility.pathfindingProperties = sDefaultCharacterCoverProperties;
		break;
	case AIPATH_CAR:
		moveAbility.pathfindingProperties = sDefaultCarProperties;
		break;
	case AIPATH_TANK:
		moveAbility.pathfindingProperties = sDefaultCarProperties;
		break;
	case AIPATH_BOAT:
		moveAbility.pathfindingProperties = sDefaultBoatProperties;
		break;
	case AIPATH_HELI:
		moveAbility.pathfindingProperties = sDefaultFlightProperties;
		break;
	case AIPATH_3D:
		moveAbility.pathfindingProperties = sDefaultVolumeProperties;
		break;
	case AIPATH_SCOUT:
		moveAbility.pathfindingProperties = sDefaultScout;
		break;
	case AIPATH_TROOPER:
		moveAbility.pathfindingProperties = sDefaultTrooper;
		break;
	case AIPATH_HUNTER:
		moveAbility.pathfindingProperties = sDefaultHunterProperties;
		break;
	default:
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "SetPFProperties: Path type %d not handled - using default", pathType);
		moveAbility.pathfindingProperties = sDefaultCharacterProperties;
		break;
	}
	return;
}


//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::RegisterWithAI(IFunctionHandler *pH)
{
	IEntity *pEntity;
	int nID;
	int type;
	ScriptHandle hdl;

	if (gEnv->bMultiplayer)
		return pH->EndFunction();

	pH->GetParams(hdl, type);

	//retrieve the entity
	nID = hdl.n;
	pEntity=m_pSystem->GetIEntitySystem()->GetEntity(nID);
	if(!pEntity)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Tried to set register with AI nonExisting entity with id [%d]. ", nID);
		return pH->EndFunction();
	}

	/*
	// suspend entity flow graph to prevent updating it before initialization
	IEntityFlowGraphProxy* pFlowGraphProxy = (IEntityFlowGraphProxy*) pEntity->GetProxy( ENTITY_PROXY_FLOWGRAPH );
	if ( pFlowGraphProxy )
	{
	assert( pFlowGraphProxy->GetFlowGraph() );
	pFlowGraphProxy->GetFlowGraph()->SetSuspended( true );
	}
	*/

	AIObjectParameters params;
	bool	autoDisable(true);

	switch( type )
	{
	case AIOBJECT_PUPPET:
	case AIOBJECT_2D_FLY:
		{
			if(!ParseTables(3, true, pH,params, autoDisable))
				return pH->EndFunction();
			IActorSystem*	pASystem = m_pSystem->GetIGame()->GetIGameFramework()->GetIActorSystem();
			if(!pASystem)
			{
				if(!ParseTables(3, true, pH,params, autoDisable))
					return pH->EndFunction();
				IActorSystem*	pASystem = m_pSystem->GetIGame()->GetIGameFramework()->GetIActorSystem();
				if(!pASystem)
				{
					gEnv->pAISystem->Error("<CScriptBind_AI> ", "RegisterWithAI: no ActorSystem for %s.", pEntity->GetName() );
					return pH->EndFunction();
				}
				IActor*  pActor = pASystem->GetActor( pEntity->GetId() );
				if(!pActor)
				{
					gEnv->pAISystem->Error("<CScriptBind_AI> ", "RegisterWithAI: no Actor for %s.", pEntity->GetName());
					return pH->EndFunction(); 
				}
				assert(pActor);
				params.pProxy = new CAIProxy( pActor->GetGameObject() );
			}
			IActor*  pActor = pASystem->GetActor( pEntity->GetId() );
			if(!pActor)
			{
				if(!ParseTables(3, true, pH,params, autoDisable))
					return pH->EndFunction();				
				IVehicleSystem*	pVSystem = m_pSystem->GetIGame()->GetIGameFramework()->GetIVehicleSystem();
				IVehicle*  pVehicle = pVSystem->GetVehicle( pEntity->GetId() );
				assert( pVehicle );
        if(!pVehicle)
        {
          gEnv->pAISystem->Error("<CScriptBind_AI> ", "RegisterWithAI: no Vehicle for %s (Id %i).", pEntity->GetName(), pEntity->GetId());
          return pH->EndFunction(); 
        }
				params.pProxy = new CAIProxy( pVehicle->GetGameObject() );
			}
			assert(pActor);
			params.pProxy = new CAIProxy( pActor->GetGameObject() );
		}
		break;
	case AIOBJECT_BOAT:
	case AIOBJECT_CAR:
		{
			if(!ParseTables(3, true, pH,params, autoDisable))
				return pH->EndFunction();				
			IVehicleSystem*	pVSystem = m_pSystem->GetIGame()->GetIGameFramework()->GetIVehicleSystem();
			IVehicle*  pVehicle = pVSystem->GetVehicle( pEntity->GetId() );
			assert( pVehicle );
			if(!pVehicle)
			{
				if(!ParseTables(3, true, pH,params, autoDisable))
					return pH->EndFunction();				
				IVehicleSystem*	pVSystem = m_pSystem->GetIGame()->GetIGameFramework()->GetIVehicleSystem();
				IVehicle*  pVehicle = pVSystem->GetVehicle( pEntity->GetId() );
				assert( pVehicle );
        if(!pVehicle)
        {
          gEnv->pAISystem->Error("<CScriptBind_AI> ", "RegisterWithAI: no Vehicle for %s (Id %i).", pEntity->GetName(), pEntity->GetId());
          return pH->EndFunction(); 
        }
				params.pProxy = new CAIProxy( pVehicle->GetGameObject() );
				params.m_moveAbility.b3DMove = true;
			}
			params.pProxy = new CAIProxy( pVehicle->GetGameObject() );
		}
		break;
	case AIOBJECT_HELICOPTER:
		{
			if(!ParseTables(3, true, pH,params, autoDisable))
				return pH->EndFunction();				
			IVehicleSystem*	pVSystem = m_pSystem->GetIGame()->GetIGameFramework()->GetIVehicleSystem();
			IVehicle*  pVehicle = pVSystem->GetVehicle( pEntity->GetId() );
			assert( pVehicle );
			if(!pVehicle)
			{
				gEnv->pAISystem->Error("<CScriptBind_AI> ", "RegisterWithAI: no Vehicle for %s (Id %i).", pEntity->GetName(), pEntity->GetId());
				return pH->EndFunction(); 
			}
			params.pProxy = new CAIProxy( pVehicle->GetGameObject() );
			params.m_moveAbility.b3DMove = true;
		}
		break;
	case AIOBJECT_PLAYER:
		{
			if(IsDemoPlayback())
				return pH->EndFunction();

			SmartScriptTable pTable;

			if (pH->GetParamCount() > 2)
				pH->GetParam(3,pTable);
			else
				return pH->EndFunction();

			IActorSystem*	pASystem = m_pSystem->GetIGame()->GetIGameFramework()->GetIActorSystem();
			IActor*  pActor = pASystem->GetActor( pEntity->GetId() );
			assert( pActor );
			params.pProxy = new CAIProxy( pActor->GetGameObject(), true );

			pTable->GetValue("groupid",params.m_sParamStruct.m_nGroup);       
			pTable->GetValue("species",params.m_sParamStruct.m_nSpecies);       
			pTable->GetValue("commrange",params.m_sParamStruct.m_fCommRange); //Luciano - added to use GROUPONLY signals

			SmartScriptTable pPerceptionTable;
			if(pTable->GetValue("Perception",pPerceptionTable))
			{
				pPerceptionTable->GetValue( "camoScale", params.m_sParamStruct.m_PerceptionParams.camoScale);
				pPerceptionTable->GetValue( "velBase", params.m_sParamStruct.m_PerceptionParams.velBase);
				pPerceptionTable->GetValue( "velScale", params.m_sParamStruct.m_PerceptionParams.velScale);
				pPerceptionTable->GetValue( "sightrange", params.m_sParamStruct.m_PerceptionParams.sightRange);
				pPerceptionTable->GetValue( "heatScale", params.m_sParamStruct.m_PerceptionParams.heatScale);
			}
		}
		break;
	case AIOBJECT_SNDSUPRESSOR:
		{
			SmartScriptTable pTable;
			// Properties table
			if (pH->GetParamCount() > 2)
				pH->GetParam(3,pTable);
			else
				return pH->EndFunction();
			if (!pTable->GetValue("radius",params.m_moveAbility.pathRadius))
				params.m_moveAbility.pathRadius = 10.f;
			break;
		}
	case AIOBJECT_WAYPOINT:
		break;
		/*
		// this block is commented out since params.m_sParamStruct is currently ignored in pEntity->RegisterInAISystem()
		// instead of setting the group id here, it will be set from the script right after registering
		default:
		// try to get groupid settings for anchors
		params.m_sParamStruct.m_nGroup = -1;
		params.m_sParamStruct.m_nSpecies = -1;
		{
		SmartScriptTable pTable;
		if ( pH->GetParamCount() > 2 )
		pH->GetParam( 3, pTable );
		if ( *pTable )
		pTable->GetValue( "groupid", params.m_sParamStruct.m_nGroup );
		}
		break;
		*/
	}
	pEntity->RegisterInAISystem( type, params );
	// AI object was not created (possibly AI System is disabled)
	if(!pEntity->GetAI())
		return pH->EndFunction();

	if(type==AIOBJECT_SNDSUPRESSOR && pEntity->GetAI())
		pEntity->GetAI()->SetRadius(params.m_moveAbility.pathRadius);
	else if(type>=AIANCHOR_FIRST)	// if anchor - set radius
	{
		SmartScriptTable pTable;
		// Properties table
		if (pH->GetParamCount() > 2)
			pH->GetParam(3,pTable);
		else
			return pH->EndFunction();
		float radius(0.f);
		pTable->GetValue("radius",radius);
		int groupId = -1;
		pTable->GetValue("groupid", groupId);
		pEntity->GetAI()->SetGroupId(groupId);
		pEntity->GetAI()->SetRadius(radius);
	}

	if( params.pProxy )
	{
		CAIProxy*	proxy = (CAIProxy*)params.pProxy;
		proxy->UpdateMeAlways(!autoDisable);
		// we don't want to override auto-disable from entity properties - only way to change it -- use FG node
//		proxy->UpdateMeAlways(false);
		// let's notify people if the auto-disable off - has to be taken care of
		//if(!autoDisable)
		//	gEnv->pAISystem->Warning("<AUTO_DISABLE>", " %s has AutiDisable turned off in editor - please use FG node", pEntity->GetName());
	}
	return pH->EndFunction();
}


//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::ResetParameters(IFunctionHandler *pH)
{
	if (gEnv->bMultiplayer)
		return pH->EndFunction();

	GET_ENTITY(1);

	if(!pEntity)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Tried to reset parameters with nonExisting entity with id [%d]. ", nID);
		return pH->EndFunction();
	}

	IAIActor* pActor = CastToIAIActorSafe(pEntity->GetAI());
	if (!pActor)
	{
		// SNH: don't display this warning for vehicles.
		if(!CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle(pEntity->GetId()))
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Tried to set parameters with entity without AI actor [%s]. ", pEntity->GetName());
		return pH->EndFunction();
	}

	AIObjectParameters params;
	bool	autoDisable(true);

	if (!ParseTables(2, false, pH, params, autoDisable))
		return pH->EndFunction();

	pActor->SetParameters(params.m_sParamStruct);

	return pH->EndFunction();
}



/*!Create a sequence of AI atomic commands (a goal pipe)
@param desired name of the goal pipe
*/
//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::CreateGoalPipe(IFunctionHandler *pH)
{
	//	CHECK_PARAMETERS(1);

	const char *name;

	if (!pH->GetParams(name))
		return pH->EndFunction();

	IGoalPipe *pPipe = m_pAISystem->CreateGoalPipe(name);

	//	m_mapGoals.insert(goalmap::iterator::value_type(name,pPipe));
	m_IsGroupOpen = false;

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::BeginGoalPipe(IFunctionHandler *pH)
{
	if(m_pCurrentGoalPipe)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Calling AI.BeginGoalPipe() with active goalpipe '%s', must call AI.EndGoalPipe() to end goalpipe.", m_pCurrentGoalPipe->GetName());
		m_pCurrentGoalPipe = 0;
	}

	const char *name;
	if (!pH->GetParams(name))
		return pH->EndFunction();

	m_IsGroupOpen = false;
	m_pCurrentGoalPipe = m_pAISystem->CreateGoalPipe(name);
	if(!m_pCurrentGoalPipe)
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "AI.BeginGoalPipe: Goalpipe was not created.");

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::EndGoalPipe(IFunctionHandler *pH)
{
	if(!m_pCurrentGoalPipe)
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Calling AI.EndGoalPipe() without AI.BeginGoalPipe() (current pipe is null).");
	m_pCurrentGoalPipe = 0;
	return pH->EndFunction();
}


//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::BeginGroup(IFunctionHandler *pH)
{
	if(m_IsGroupOpen)
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Calling AI.BeginGroup() twice (no nesed grouping supported).");
	m_IsGroupOpen = true;
	return pH->EndFunction();
}


//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::EndGroup(IFunctionHandler *pH)
{
	if(!m_IsGroupOpen)
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Calling AI.EndGroup() without AI.BeginGroup().");
	m_IsGroupOpen = false;
	return pH->EndFunction();
}


/*!Push a label into a goal pipe. The label is appended at the end of the goal pipe. Pipe of given name has to be previously created.
@param name of the goal pipe in which the goal will be pushed.
@param name of the label that needs to be pushed into the pipe
@see CScriptBind_AI::CreateGoalPipe
*/
//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::PushLabel(IFunctionHandler *pH)
{
	const char *pipename;
	const char *labelname;
	string goalname;

	if(m_pCurrentGoalPipe)
	{
		// Begin/EndGoalpipe
		if (!pH->GetParams(labelname))
			return pH->EndFunction();
		m_pCurrentGoalPipe->PushLabel(labelname);
	}
	else
	{
		// CreateGoalPipe
		GoalParameters params;
		bool blocking = false;

		if (!pH->GetParams(pipename, labelname))
			return pH->EndFunction();

		IGoalPipe *pPipe=0;

		if (!(pPipe = m_pAISystem->OpenGoalPipe(pipename))) 
			return pH->EndFunction();

		pPipe->PushLabel(labelname);
	}

	return pH->EndFunction();
}


/*!Push a goal into a goal pipe. The goal is appended at the end of the goal pipe. Pipe of given name has to be previously created.
@param name of the goal pipe in which the goal will be pushed.
@param name of atomic goal that needs to be pushed into the pipe
@param 1 if the goal should block the pipe execution, 0 if the goal should not block the goal execution
@see CScriptBind_AI::CreateGoalPipe
*/
//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::PushGoal(IFunctionHandler *pH)
{
	const char *pipename;
	const char *temp;
	string goalname;
	//	int id;
	GoalParameters params;
	bool blocking(false);
	IGoalPipe::EGroupType grouped(IGoalPipe::GT_NOGROUP);
	IGoalPipe *pPipe(0);
	int	cnt(0);

	if(m_pCurrentGoalPipe)
	{
		// Begin/EndGoalPipe
		if (!pH->GetParam(1,temp))
			temp = 0;
		if(pH->GetParamCount()>1)
			pH->GetParam(2,blocking);
		pPipe = m_pCurrentGoalPipe;
		cnt = 2;
	}
	else
	{
		// CreateGoalPipe
		if (!pH->GetParam(1,pipename))
			pipename = 0;
		if (!pH->GetParam(2,temp))
			temp = 0;
		if(pH->GetParamCount()>2)
			pH->GetParam(3,blocking);
		pPipe = m_pAISystem->OpenGoalPipe(pipename);
		cnt = 3;
	}

	if (!pPipe) 
		return pH->EndFunction();

	if (m_IsGroupOpen)
		grouped = IGoalPipe::GT_GROUPED; 
	if (temp && *temp == '+')
	{
		++temp;
		grouped = IGoalPipe::GT_GROUPWITHPREV; 
	}
	goalname = temp;

	EGoalOperations op = pPipe->GetGoalOpEnum(goalname.c_str());

	switch (op)
	{
	case AIOP_ACQUIRETARGET:
		{
			pH->GetParam(cnt+1,temp);
			//		IAIObject *pObject = m_pAISystem->GetAIObjectByName(AIOBJECT_WAYPOINT,temp);
			//		params.m_pTarget = pObject;
			params.szString = temp;
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_CONTINUOUS:
		{
			if (pH->GetParamCount() > cnt) 
				pH->GetParam(cnt+1,params.bValue);
			else
				params.bValue = false;
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_FORM:
		{
			pH->GetParam(cnt+1,temp);
			params.szString = temp;
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_PATHFIND:
		{
			if(pH->GetParamCount() > cnt)
			{
				pH->GetParam(cnt+1,temp);
				IAIObject *pObject = 0;
				if (temp[0]) 
					pObject = m_pAISystem->GetAIObjectByName(0,temp);
				params.m_pTarget = pObject;
				params.szString = temp;
				if (pH->GetParamCount() > cnt+1 ) 
					pH->GetParam(cnt+2,params.bValue);
				else
					params.bValue = false;
			}
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_LOCATE:
		{
			if (pH->GetParamType(cnt+1) == svtString)
			{
				const char *temp;
				pH->GetParam(cnt+1,temp);
				params.szString = temp;
				params.nValue = 0;
			}
			else if (pH->GetParamType(cnt+1) == svtNumber)
			{
				params.szString.clear();
				pH->GetParam(cnt+1,params.nValue);
			}

			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2,params.fValue);
			else
				params.fValue = 0;

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_HIDE:
		{
			// Distance
			pH->GetParam(cnt+1,params.fValue);
			// Method
			pH->GetParam(cnt+2,params.nValue);
			// Exact
			if (pH->GetParamCount() > cnt+2 ) 
				pH->GetParam(cnt+3,params.bValue);
			else
				params.bValue = true;
			// Min Distance
			if (pH->GetParamCount() > cnt+3 ) 
				pH->GetParam(cnt+4,params.fValueAux);
			// Lastop result
			if (pH->GetParamCount() > cnt+4) 
				pH->GetParam(cnt+5,params.nValueAux);

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_TRACE:
		{
			params.fValue = 0;
			params.nValue = 0;
			params.fValueAux = 0;
			params.nValueAux = 0;
			if (pH->GetParamCount() > cnt)
			{
				// Exact
				pH->GetParam(cnt+1,params.nValue);
				// Single step
				if (pH->GetParamCount() > cnt+1)
					pH->GetParam(cnt+2,params.nValueAux);
				// Distance
				if (pH->GetParamCount() > cnt+2)
					pH->GetParam(cnt+3,params.fValue);
				if (pH->GetParamCount() > cnt+3)
					pH->GetParam(cnt+4,params.fValueAux);
				else
					params.fValueAux = params.fValue;
			}
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_LOOKAT:
		{
			pH->GetParam(cnt+1, params.fValue);
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2,params.fValueAux);
			if (pH->GetParamCount() > cnt+2)				
				pH->GetParam(cnt+3,params.bValue);		//use LastOp	
			if (pH->GetParamCount() > cnt+3)
				pH->GetParam(cnt+4,params.nValue);

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_LOOKAROUND:
		{
			// float lookAtRange, float scanIntervalRange, float intervalMin, float intervalMax, flags
			// Look at range
			pH->GetParam(cnt+1,params.fValue);
			// Scan Interval
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2,params.fValueAux);
			else
				params.fValueAux = -1;
			// Timeout min
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3,params.m_vPosition.x);
			else
				params.m_vPosition.x = -1;
			// Timeout max
			if (pH->GetParamCount() > cnt+3)
				pH->GetParam(cnt+4,params.m_vPosition.y);
			else
				params.m_vPosition.y = -1;
			// flags: break look when live target , use last op as reference direction
			int	flags = 0;
			if (pH->GetParamCount() > cnt+4)
				pH->GetParam(cnt+5,flags);
			params.nValue = flags;

			pPipe->PushGoal(op,blocking,grouped,params);
		}
	case AIOP_DEVALUE:
		{
			if (pH->GetParamCount() > cnt)
			{
				pH->GetParam(cnt+1, params.fValue);
				if (pH->GetParamCount() > cnt+1)
					pH->GetParam(cnt + 2,params.bValue);
				else
					params.bValue = false;
			}
			else
				params.fValue = false;

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_SIGNAL:
		{
			pH->GetParam(cnt+1, params.fValue);	// get the signal id
			params.nValue = 0;
			params.fValueAux = 0;

			if (pH->GetParamCount() > cnt+1)
			{
				const char *sTemp;
				if (pH->GetParam(cnt+2, sTemp))	// get the signal text
					params.szString = sTemp;
			}

			if (pH->GetParamCount() > cnt+2)
			{
				pH->GetParam(cnt+3,params.nValue);	// get the desired filter
			}

			if (pH->GetParamCount() > cnt+3)
			{
				pH->GetParam(cnt+4, params.fValueAux);	// extra signal data passed as data.iValue.
			}

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_APPROACH:
		{
			// Approach distance
			pH->GetParam(cnt+1, params.fValue);

			// Lastop result usage flags, see EAILastOpResFlags.
			params.nValue = 0;
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.nValue);
			// get the accuracy.
			params.fValueAux = 1.f;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.fValueAux);
			// Special signal to send when pathfinding fails.
			if (pH->GetParamCount() > cnt+3)
			{
				const char* sTemp = "";
				if (pH->GetParam(cnt+4,sTemp))
				{
					if (strlen(sTemp) > 1)
						params.szString = sTemp;
				}
			}
			// Approach distance Variance
			params.m_vPosition.x = 0.0f;
			if (pH->GetParamCount() > cnt+4)
				pH->GetParam(cnt+5, params.m_vPosition.x);

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_BACKOFF:
		{
			pH->GetParam(cnt+1,params.fValue); // distance
			if(pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2,params.fValueAux);//max duration
			if(pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.nValue);//filter (use/look last op, direction)
			if(pH->GetParamCount() > cnt+3)
				pH->GetParam(cnt+4, params.nValueAux); //min distance

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_MOVETOWARDS:
		{
			pH->GetParam(cnt+1,params.fValue); // distance
			if(pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.nValue); //filter (use/look last op, direction)
			if(pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3,params.m_vPosition.x); //variation

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_BODYPOS:
		{
			pH->GetParam(cnt+1, params.fValue);
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.nValue);
			else 
				params.nValue = 0;
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_TIMEOUT:
		{
			pH->GetParam(cnt+1, params.fValue);
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.fValueAux);
			else 
				params.fValueAux = 0;
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_BRANCH:
		{
			// Label or Branch offset.
			if (pH->GetParamType(cnt+1) == svtString)
			{
				// Label
				pH->GetParam(cnt+1, temp);
				params.szString = temp;
			}
			else
			{
				// Jump offset
				pH->GetParam(cnt+1, params.nValueAux);
			}

			if (pH->GetParamCount() > cnt+1)
			{
				// Branch type (see EOPBranchType).
				pH->GetParam(cnt+2, params.nValue);

				// Parameters.
				if (pH->GetParamCount() > cnt+2)
					pH->GetParam(cnt+3, params.fValue);
				if (pH->GetParamCount() > cnt+3)
					pH->GetParam(cnt+4, params.fValueAux);
				else
					params.fValueAux = -1.0f;
			}
			else 
				params.nValue = 0;

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_RANDOM:
		{
			pH->GetParam(cnt+1, params.nValue);
			pH->GetParam(cnt+2, params.fValue);
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_CLEAR:
		{
			if (pH->GetParamCount() > cnt)
				pH->GetParam(cnt+1, params.fValue);
			else
				params.fValue = 1.0f;
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_FIRECMD:
		{
			// Fire mode, see EFireMode (IAgent.h ) for complete list of modes.
			if (pH->GetParamCount() > cnt)
				pH->GetParam(cnt+1, params.nValue);
			// Use last op result.
			int useLastOpResult = 0;
			params.bValue = false;
			if (pH->GetParamCount() > cnt+1)
			{
				pH->GetParam(cnt+2, useLastOpResult);
				if(useLastOpResult & 1)
					params.bValue = true;
			}
			// Min timeout
			params.fValue = -1.f;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.fValue);
			// Max timeout
			params.fValueAux = -1.f;
			if (pH->GetParamCount() > cnt+3)
				pH->GetParam(cnt+4, params.fValueAux);

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_STICK:
		{
			// Stick distance
			pH->GetParam(cnt+1, params.fValue);

			// Lastop result usage flags, see EAILastOpResFlags.
			params.nValue = 0;
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.nValue);

			// Stick flags, see EStickFlags.
			// When creating the goal pipe this flag actually indicates "break when approached the target".
			params.nValueAux = 0;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.nValueAux);

			// Pathfinding accuracy
			params.fValueAux = params.fValue;
			if (pH->GetParamCount() > cnt+3)
				pH->GetParam(cnt+4, params.fValueAux);

			// End distance variance, the variance distance is substraceted from the end distance.
			params.m_vPosition.x = 0.0f;
			if (pH->GetParamCount() > cnt+4)
				pH->GetParam(cnt+5, params.m_vPosition.x);

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_MOVE:
		{
			// Move distance
			pH->GetParam(cnt+1,params.fValue);

			// Move speed, negative speed means flee.
			params.fValueAux = 1.0f;
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.fValueAux);

			// Lastop result usage flags, see EAILastOpResFlags.
			params.nValue = 0;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.nValue);

			// Continuouse, default is continuouse.
			// When creating the goal pipe this flag actually indicates "break when approached the target".
			params.bValue = true;
			if (pH->GetParamCount() > cnt+3)
			{
				int cont = 0;
				pH->GetParam(cnt+4, cont);
				params.bValue = (cont == 0);
			}

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_CHARGE:
		{
			// Charge distance front
			pH->GetParam(cnt+1, params.fValue);

			// Charge distance back
			params.fValueAux = 0;
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.fValueAux);

			// Lastop result usage flags, see EAILastOpResFlags.
			params.nValue = 0;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.nValue);

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_STRAFE:
		{
			int	n = pH->GetParamCount();
			// Distance Start
			params.fValue = 0;
			pH->GetParam(cnt+1, params.fValue);
			// Distance End
			params.fValueAux = 0;
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.fValueAux);
			// Strafe while moving
			int whileMoving = 0;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, whileMoving);
			params.bValue = whileMoving != 0;

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_WAITSIGNAL:
		{
			// signal to wait for
			pH->GetParam(cnt+1, temp);
			params.szString = temp;

			// extra signal data to wait for
			if ( pH->GetParamType(cnt+2) == svtNull )
				params.fValue = 0;
			else if ( pH->GetParamType(cnt+2) == svtString )
			{
				params.fValue = 1.0f;
				pH->GetParam(cnt+2, temp);
				params.szString2 = temp;
			}
			else if ( pH->GetParamType(cnt+2) == svtNumber )
			{
				params.fValue = 2.0f;
				pH->GetParam( cnt+2, params.nValue );
			}
			else if ( pH->GetParamType(cnt+2) == svtPointer )
			{
				params.fValue = 2.0f;
				ScriptHandle hdl;
				pH->GetParam( cnt+2, hdl );
				params.nValue = hdl.n;
			}

			// maximum waiting time
			if ( pH->GetParamCount() == cnt+3 )
				pH->GetParam( cnt+3, params.fValueAux );

			pPipe->PushGoal( op, blocking, grouped, params );
		}
		break;
	case AIOP_ANIMATION:
		{
			// signal/action flag
			pH->GetParam(cnt+1, params.nValue);
			// Anim action to set.
			pH->GetParam(cnt+2, temp);
			params.szString = temp;
			// Timeout
			params.fValue = -1;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3,params.fValue);

			pPipe->PushGoal( op, blocking, grouped, params );
		}
		break;
	case AIOP_ANIMTARGET:
		{
			// signal/action flag
			pH->GetParam(cnt+1, params.bValue);
			// Anim action to set.
			pH->GetParam(cnt+2, temp);
			params.szString = temp;
			// start radius
			pH->GetParam(cnt+3, params.m_vPosition.x);
			// direction tolerance
			pH->GetParam(cnt+4, params.m_vPosition.y);
			// target radius
			pH->GetParam(cnt+5, params.m_vPosition.z);
			pPipe->PushGoal( op, blocking, grouped, params );
		}
		break;
	case AIOP_FOLLOWPATH:
		{
			bool pathFindToStart = false, reverse = false, startNearest = false, bUsePointList = false, bAvoidObjects = false;
			float fValueAux = 0.1f;

			pH->GetParam(cnt+1, pathFindToStart);
			pH->GetParam(cnt+2, reverse);
			pH->GetParam(cnt+3, startNearest);
			pH->GetParam(cnt+4, params.nValueAux);	// Loops
			if (pH->GetParamCount() > cnt+4)
				pH->GetParam(cnt+5, fValueAux);
			if (pH->GetParamCount() > cnt+5)
				pH->GetParam(cnt+6, bUsePointList);
			if (pH->GetParamCount() > cnt+6)
				pH->GetParam(cnt+7, bAvoidObjects);
			params.nValue = (pathFindToStart ? 1 : 0) | (reverse ? 2 : 0) | (startNearest ? 4 : 0) | (bUsePointList ? 8 : 0) |  (bAvoidObjects ? 16 : 0);
			params.fValueAux = fValueAux;
			pPipe->PushGoal( op, blocking, grouped, params );
		}
		break;
	case AIOP_USECOVER:
		{
			params.fValueAux = 0.0f;
			params.nValue = 0;
			params.nValueAux = 0;
			pH->GetParam(cnt+1, params.bValue);			// hide/unhide
			pH->GetParam(cnt+2, params.fValue);			// time min
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.fValueAux);	// time max
			if (pH->GetParamCount() > cnt+3)
				pH->GetParam(cnt+4, params.nValue);		// use lastop as backup
			if (pH->GetParamCount() > cnt+4)
				pH->GetParam(cnt+5, params.nValueAux);	// faster time when target not visible.
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_WAIT:
		{
			if(m_IsGroupOpen)
				gEnv->pAISystem->Warning("<CScriptBind_AI> ", "No WAIT goalOp allowed withing a group. Pipe: %s ", pPipe->GetName());
			else
			{
				params.nValue = WAIT_ALL;
				pH->GetParam(cnt+1, params.nValue);			
				pPipe->PushGoal(op,blocking,grouped,params);
			}
		}
		break;
	case AIOP_ADJUSTAIM:
		{
			params.nValue = 0;
			int	hide = 0;
			int	useLastOpResultAsBackup = 0;
			int allowProne = 0;
			if (pH->GetParamCount() > cnt+0)
				pH->GetParam(cnt+1, hide);	// aim = 0, hide = 1
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, useLastOpResultAsBackup);	// useLastOpResultAsBackup = 1
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, allowProne);

			if (hide) params.nValue |= 0x1;
			if (useLastOpResultAsBackup) params.nValue |= 0x2;
			if (allowProne) params.nValue |= 0x4;

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_SEEKCOVER:
		{
			pH->GetParam(cnt+1, params.bValue);	// hide/unhide

			pH->GetParam(cnt+2, params.fValue);	// radius

			params.nValue = 3;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.nValue);	// iterations

			int	useLastOpResultAsBackup = 0;
			if (pH->GetParamCount() > cnt+3)
				pH->GetParam(cnt+4, useLastOpResultAsBackup);	// use lastopresult if attention target does not exists
			params.nValueAux = useLastOpResultAsBackup;

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_PROXIMITY:
		{
			pH->GetParam(cnt+1, params.fValue);	// radius

			const char* szSignalName = 0;
			pH->GetParam(cnt+2, szSignalName);	// signal name
			params.szString = szSignalName;

			params.nValue = 0;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.nValue);	// flags

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_DODGE:
		{
			pH->GetParam(cnt+1, params.fValue);	// radius

			int	useLastOpResultAsBackup = 0;
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, useLastOpResultAsBackup);	// use lastopresult if attention target does not exists
			params.nValueAux = useLastOpResultAsBackup;

			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;
	case AIOP_RUN:
		{
			// Maximum Movement urgency
			params.fValue = 0;
			pH->GetParam(cnt+1, params.fValue);
			// Minimum Movement urgency
			params.fValueAux = 0;
			if (pH->GetParamCount() > cnt+1)
				pH->GetParam(cnt+2, params.fValueAux);
			// Scale down path length, if the path length is less than this parameter
			// the urgency is scaled down towards the minimum.
			params.m_vPosition.x = 0;
			if (pH->GetParamCount() > cnt+2)
				pH->GetParam(cnt+3, params.m_vPosition.x);
			pPipe->PushGoal(op,blocking,grouped,params);
		}
		break;

	default:
		{
			if (pH->GetParamCount() > cnt)
			{
				// with float parameter one
				pH->GetParam(cnt+1,params.fValue);
				if (op != LAST_AIOP)
					pPipe->PushGoal(op,blocking,grouped,params);
				else
					pPipe->PushPipe(goalname,blocking,grouped,params); // Assume another goalpipe
			}
			else 
			{
				// without parameters
				if (op != LAST_AIOP)
					pPipe->PushGoal(op,blocking,grouped,params);
				else
					pPipe->PushPipe(goalname,blocking,grouped,params); // Assume another goalpipe
			}
		}
		break;
	}

	return pH->EndFunction();
}


//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsGoalPipe(IFunctionHandler *pH)
{
	const char *pipename;

	if (!pH->GetParams(pipename))
		return pH->EndFunction();

	if (m_pAISystem->OpenGoalPipe(pipename)) 
		return pH->EndFunction( true );
	return pH->EndFunction( false );
}

bool CScriptBind_AI::GetSignalExtraData(IFunctionHandler * pH, int iParam, IAISignalExtraData* pEData)
{
	bool bDataFound = false;
	SmartScriptTable theObj;
	int iNumberValues=0;

	for(int i=iParam; i<=iParam+3 && i<=pH->GetParamCount(); i++)
	{
		//int iType = pH->GetParamType(i);
		if(pH->GetParamType(i) == svtNumber)
		{
			//entity ID or whatever

			if(++iNumberValues==1)
				pH->GetParam(i,pEData->iValue);//convention: first parameter is always an integer. if you want to send
			// a float, always send an integer first
			else
				pH->GetParam(i,pEData->fValue);

			bDataFound = true;
		}
		else if(pH->GetParamType(i) == svtPointer)
		{
			ScriptHandle hdl;
			pH->GetParam(i,hdl);
			pEData->nID = hdl;// .n;
			bDataFound = true;
		}
		else if(pH->GetParamType(i) == svtString)
		{
			const char* sTemp = 0;
			if (pH->GetParam(i, sTemp))
				pEData->SetObjectName(sTemp);
			bDataFound = true;
		}

		else if(pH->GetParamType(i) == svtObject)//supposed table
		{
			pH->GetParam(i,theObj);
			if(theObj.GetPtr())
			{	// a vector is passed
				if(theObj->GetValue("x",pEData->point.x))
				{	
					theObj->GetValue("y",pEData->point.y);
					theObj->GetValue("z",pEData->point.z);
					bDataFound = true;
				}
				else if(theObj->GetValue("point",pEData->point))
				{  // the entire Extra data structure (nID, fValue,point, pObject) is passed
					theObj->GetValue("point2",pEData->point2);
					theObj->GetValue("fValue",pEData->fValue);
					theObj->GetValue("iValue",pEData->iValue);
					theObj->GetValue("iValue2",pEData->iValue2);
					if(!theObj->GetValue("id",pEData->nID))
						pEData->nID.n =0;
					const char* sTemp = 0;
					if(theObj->GetValue("ObjectName",sTemp))
						pEData->SetObjectName(sTemp);
					else
						pEData->SetObjectName("");
					bDataFound = true;
				}
				else
				{

					// Wrong type of data passed from LUA
				}
			}
		}
	}
	return bDataFound;
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::Signal(IFunctionHandler * pH)
{
	//CHECK_PARAMETERS(4);
	if(pH->GetParamCount() <4)
	{
		//		m_pSystem->GetIEntitySystem()->RaiseError("AISignal: Need at least 4 params");
		return pH->EndFunction();
	}

	int cFilter;
	int nSignalID;
	const char *szSignalText = 0;
	ScriptHandle hdl;
	int EntityID;

	Vec3	point;
	//	CScriptObjectVector pPoint(m_pScriptSystem,true);
	IAISignalExtraData* pEData = gEnv->pAISystem->CreateSignalExtraData();

	pH->GetParam(1,cFilter);
	pH->GetParam(2,nSignalID);
	pH->GetParam(3,szSignalText);
	pH->GetParam(4,hdl);
	EntityID = hdl.n;

	bool bExtraData = GetSignalExtraData(pH,5,pEData);
	if ( !bExtraData )
	{
		m_pAISystem->FreeSignalExtraData( pEData );
		pEData = NULL;
	}

	IEntity* pEntity = m_pSystem->GetIEntitySystem()->GetEntity(EntityID);

	if ((pEntity) && (pEntity->GetAI()))
		m_pAISystem->SendSignal(cFilter,nSignalID,szSignalText,pEntity->GetAI(),pEData);

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::FreeSignal(IFunctionHandler * pH)
{
	//CHECK_PARAMETERS(4);
	float fRadius;
	int nSignalID,nID = 0;
	const char *szSignalText;
	Vec3	pos;
	ScriptHandle hdl;
	IAISignalExtraData* pEData = gEnv->pAISystem->CreateSignalExtraData();

	pH->GetParam(1,nSignalID);
	pH->GetParam(2,szSignalText);
	pH->GetParam(3,pos);
	pH->GetParam(4,fRadius);
	if (pH->GetParamCount()>4)
	{
		pH->GetParam(5,hdl); 
		nID = hdl.n;
	}

	bool bExtraData = GetSignalExtraData(pH,6,pEData);
	if ( !bExtraData )
	{
		m_pAISystem->FreeSignalExtraData( pEData );
		pEData = NULL;
	}

	IEntity *pEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);
	IAIObject *pObject=0;
	if (pEntity)
	{
		pObject = pEntity->GetAI();
	}

	m_pAISystem->SendAnonymousSignal(nSignalID,szSignalText,pos,fRadius,pObject,pEData);

	return pH->EndFunction();
}


//////////////////////////////////////////////////////////////////////
int CScriptBind_AI::SetIgnorant(IFunctionHandler * pH)
{
	//	gEnv->pAISystem->Warning("<CScriptBind_AI> ", "AI.MakePuppetIgnorant(...) is obsolete!" );
	GET_ENTITY(1);
	int iIgnorant;
	pH->GetParam(2,iIgnorant);

	IAIObject *pObject=0;
	if (pEntity)
	{
		pObject = pEntity->GetAI();
		IPipeUser* pPiper = pObject->CastToIPipeUser();
		if(pPiper)
			pPiper->MakeIgnorant(iIgnorant > 0);
	}

	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetAssesmentMultiplier(IFunctionHandler * pH)
{
	int type;
	float fMultiplier;

	pH->GetParams(type,fMultiplier);

	if (type < 0)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Tried to set assesment multiplier to a negative type. Not allowed.");
		return pH->EndFunction();
	}


	m_pAISystem->SetAssesmentMultiplier((unsigned short)type, fMultiplier);

	return pH->EndFunction();
}


//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupCount(IFunctionHandler * pH)
{

	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pAI);
		if (pAIActor )
		{
			int groupId = pAI->GetGroupId();
			int flags = IAISystem::GROUP_ALL;
			if(pH->GetParamCount()>1)
				pH->GetParam(2,flags);
			int type = 0;
			if(pH->GetParamCount()>2)
				pH->GetParam(3,type);

			return pH->EndFunction(m_pAISystem->GetGroupCount(groupId,flags,type));
		}
	}

	return pH->EndFunction(0);

}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupMember(IFunctionHandler * pH)
{
	if (pH->GetParamCount()<2)
		return pH->EndFunction();

	int groupId=-1;
	if (pH->GetParamType(1)==svtNumber)
		pH->GetParam(1,groupId);
	else if (pH->GetParamType(1)==svtPointer)
	{
		GET_ENTITY(1);
		if (pEntity)
		{
			if (pEntity->GetAI())
				groupId = pEntity->GetAI()->GetGroupId();
		}
	}
	if(groupId<0)
		return pH->EndFunction();
	int index=0;
	pH->GetParam(2,index);

	int flags = IAISystem::GROUP_ALL;
	if(pH->GetParamCount()>2)
		pH->GetParam(3,flags);

	int type = 0;
	if(pH->GetParamCount()>3)
		pH->GetParam(4,type);

	IAIObject *pObject = m_pAISystem->GetGroupMember(groupId, index - 1, flags, type);
	if(pObject)
	{
		IEntity* pEntityMember = pObject->GetEntity();
		if(pEntityMember)
			return pH->EndFunction( pEntityMember->GetScriptTable());
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupTarget(IFunctionHandler * pH)
{
	if(pH->GetParamCount()<1)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetGroupTarget: Too few parameters." );
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pAI);
		if (pAIActor)
		{
			int groupId = pAI->GetGroupId();
			IAIGroup* pGroup = m_pAISystem->GetIAIGroup(groupId);
			if(pGroup)
			{
				bool bHostileOnly = true;
				bool bLiveOnly = true;
				if(pH->GetParamCount()>1)
					pH->GetParam(2,bHostileOnly);
				if(pH->GetParamCount()>2)
					pH->GetParam(3,bLiveOnly);
				IAIObject* pTarget = pGroup->GetAttentionTarget(bHostileOnly,bLiveOnly);
				if(pTarget)
				{
					if(pTarget->CastToIPuppet() 
						|| pTarget->CastToIAIVehicle()
						|| pTarget->GetAIType() == AIOBJECT_PLAYER)
					{
						IEntity* pTargetEntity = pTarget->GetEntity();
						if(pTargetEntity)
						{
							IScriptTable* pScript = pTargetEntity->GetScriptTable();
							return pH->EndFunction(pScript);
							//return pH->EndFunction(pTargetEntity->GetId());
						}
					}
					return pH->EndFunction(pTarget->GetName());
				}
			}
		}
	}
	return pH->EndFunction();
}


//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetGroupTargetCount(IFunctionHandler * pH)
{
	if(pH->GetParamCount()<1)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetGroupTarget: Too few parameters." );
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
		if (pAIActor)
		{
			int groupId = pAI->GetGroupId();
			IAIGroup* pGroup = m_pAISystem->GetIAIGroup(groupId);
			if(pGroup)
			{
				bool bHostileOnly = true;
				bool bLiveOnly = true;
				if(pH->GetParamCount()>1)
					pH->GetParam(2,bHostileOnly);
				if(pH->GetParamCount()>2)
					pH->GetParam(3,bLiveOnly);
				int n = pGroup->GetTargetCount(bHostileOnly,bLiveOnly);
				return pH->EndFunction(n);
			}
		}
	}
	return pH->EndFunction(0);
}


static void	DrawDebugBox(const AABB& aabb, uint8 r, uint8 g, uint8 b, float t)
{
	Vec3	verts[8];

	verts[0].Set(aabb.min.x, aabb.min.y, aabb.min.z);
	verts[1].Set(aabb.max.x, aabb.min.y, aabb.min.z);
	verts[2].Set(aabb.max.x, aabb.max.y, aabb.min.z);
	verts[3].Set(aabb.min.x, aabb.max.y, aabb.min.z);

	verts[4].Set(aabb.min.x, aabb.min.y, aabb.max.z);
	verts[5].Set(aabb.max.x, aabb.min.y, aabb.max.z);
	verts[6].Set(aabb.max.x, aabb.max.y, aabb.max.z);
	verts[7].Set(aabb.min.x, aabb.max.y, aabb.max.z);

	gEnv->pAISystem->AddDebugLine(verts[0], verts[1], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[1], verts[2], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[2], verts[3], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[3], verts[0], r, g, b, t);

	gEnv->pAISystem->AddDebugLine(verts[4], verts[5], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[5], verts[6], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[6], verts[7], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[7], verts[4], r, g, b, t);

	gEnv->pAISystem->AddDebugLine(verts[0], verts[4], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[1], verts[5], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[2], verts[6], r, g, b, t);
	gEnv->pAISystem->AddDebugLine(verts[3], verts[7], r, g, b, t);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::FindObjectOfType(IFunctionHandler * pH)
{	
	int type;
	Vec3 searchPos;
	Vec3 returnPos;
	float fRadius;
	int nFlags = 0;
	CScriptVector vReturnPos;
	CScriptVector vReturnDir;
	CScriptVector vInputPos;
	IAIObject*	pAI = 0;

	if (pH->GetParamType(1) == svtPointer)
	{
		pH->GetParam(2,fRadius);
		pH->GetParam(3,type);
		if (pH->GetParamCount()>3)
			pH->GetParam(4,nFlags);
		if (pH->GetParamCount()>4)
		{
			if(pH->GetParamType(5)==svtObject)
				pH->GetParam(5,vReturnPos);
			else
			{
				gEnv->pAISystem->Warning("<CScriptBind_AI> ", "FindObjectOfType: wrong position return value passed (not a table)" );
				return pH->EndFunction();
			}
		}
		if (pH->GetParamCount()>5)
			pH->GetParam(6,vReturnDir);

		GET_ENTITY(1);
		if (!pEntity || !pEntity->GetAI())
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "FindObjectOfType: The specified entity does not exists or does not have AI." );
			return pH->EndFunction();
		}
		pAI = pEntity->GetAI();
		searchPos = pAI->GetPos();
	}
	else
	{
		if(pH->GetParamType(1)!=svtObject)
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "FindObjectOfType: wrong search position value passed (not a table)" );
			return pH->EndFunction( );
		}
		pH->GetParam(1,vInputPos);
		searchPos = vInputPos.Get();
		pH->GetParam(2,fRadius);
		pH->GetParam(3,type);
		if (pH->GetParamCount()>3)
			pH->GetParam(4,nFlags);
		if (pH->GetParamCount()>4)
		{
			if(pH->GetParamType(5)==svtObject)
				pH->GetParam(5,vReturnPos);
			else
			{
				gEnv->pAISystem->Warning("<CScriptBind_AI> ", "FindObjectOfType: wrong position return value passed (not a table)" );
				return pH->EndFunction( );
			}
		}
		if (pH->GetParamCount()>5)
			pH->GetParam(6,vReturnDir);
	}


	IAIObject*	pFoundObject = 0;

	if(nFlags != 0)
	{
		// [mikko] This is inconsistent with the AISystem version of the FindObjectOfType.
		// The only excuse to do it this way is that there already is a lot of different
		// versions of the find objects of different ways and it is just a big mess anyway.

		bool	facingTarget = (nFlags & AIFO_FACING_TARGET) != 0;
		bool	nonOccupied = (nFlags & AIFO_NONOCCUPIED) != 0;
		bool	chooseRandom = (nFlags & AIFO_CHOOSE_RANDOM) != 0;
		bool	nonOccupiedRefPoint = (nFlags & AIFO_NONOCCUPIED_REFPOINT) != 0;
		bool	useBeaconAsFallback = (nFlags & AIFO_USE_BEACON_AS_FALLBACK_TGT) != 0;
		bool	noDevalue = (nFlags & AIFO_NO_DEVALUE) != 0;

		float	nearestDist = FLT_MAX;

		Vec3	targetPos = searchPos;
		bool	hasTarget = false;
		IPipeUser *pPipeUser = CastToIPipeUserSafe(pAI);
		IAIActor *pAIActor = CastToIAIActorSafe(pAI);
		if (pPipeUser)
		{
			if(pPipeUser->GetAttentionTarget())
			{
				targetPos = pPipeUser->GetAttentionTarget()->GetPos();
				hasTarget = true;
			}
			else if(useBeaconAsFallback)
			{
				if(IAIObject* pBeacon = m_pAISystem->GetBeacon(pAI->GetGroupId()))
				{
					targetPos = pBeacon->GetPos();
					hasTarget = true;
				}
			}
		}

		// Find puppets that are within the search range.
		std::list<AABB>	avoidBoxes;
		std::vector<IAIObject*>	randomObjs;

		if(nonOccupied)
		{
			// Send a signal to all entities that see the specified entity, that that entity have just done something miraculous.
			AutoAIObjectIter it(m_pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET, searchPos, fRadius + 1.0f, false));
			for(; it->GetObject(); it->Next())
			{
				IAIObject*	obj = it->GetObject();
				if(obj == pAI) continue;
				if(!obj->IsEnabled()) continue;
				if(!obj->GetEntity()) continue;
				IPipeUser *pCurPipeUser = obj->CastToIPipeUser();
				if (!pCurPipeUser)
					continue;

				AABB	bounds;
				obj->GetEntity()->GetWorldBounds(bounds);
				bounds.min -= Vec3(0.1f, 0.1f, 0.1f);
				bounds.max += Vec3(0.1f, 0.1f, 0.1f);

				avoidBoxes.push_back(bounds);
				//				DrawDebugBox(bounds, 255,255,255, 10);

				if(nonOccupiedRefPoint && pCurPipeUser->GetRefPoint() && !pCurPipeUser->GetRefPoint()->GetPos().IsZero(0.001f))
				{
					// This is not very accurate.
					Vec3	size = bounds.GetSize() * 0.5f;
					const Vec3&	refPos = pCurPipeUser->GetRefPoint()->GetPos();
					AABB	refBounds(refPos - size, refPos + size);
					avoidBoxes.push_back(refBounds);
					//					DrawDebugBox(bounds, 255,255,255, 10);
				}
			}
		}

		AutoAIObjectIter it(m_pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, type, searchPos, fRadius, false));
		for(; it->GetObject(); it->Next())
		{
			IAIObject*	obj = it->GetObject();
			if(!obj->IsEnabled()) continue;

			const Vec3&	objPos = obj->GetPos();
			const Vec3& objDir = obj->GetMoveDir();

			if(nonOccupied)
			{
				// Skip occupied
				bool	occupied = false;
				for(std::list<AABB>::iterator bit = avoidBoxes.begin(); bit != avoidBoxes.end(); ++bit)
				{
					if(bit->IsContainPoint(objPos))
					{
						occupied = true;
						break;
					}
				}
				if(occupied)
				{
					//					m_pAISystem->AddDebugLine(objPos - Vec3(0.5f,0,0), objPos + Vec3(0.5f,0,0), 255, 0, 0, 10 );
					//					m_pAISystem->AddDebugLine(objPos - Vec3(0,0.5f,0), objPos + Vec3(0,0.5f,0), 255, 0, 0, 10 );
					continue;
				}
			}

			float	dotToTarget = 1.0f;
			if(facingTarget)
			{
				Vec3	dirToTarget;
				if(hasTarget)
					dirToTarget = targetPos - objPos;
				else
					dirToTarget = objPos - searchPos;

				dirToTarget.NormalizeSafe();
				const float	dotTreshold = cosf(DEG2RAD(160.0f));
				dotToTarget = dirToTarget.Dot(objDir);
				if(dotToTarget < dotTreshold)
				{
					Vec3	norm(dirToTarget.y, -dirToTarget.x, 0);
					norm.NormalizeSafe();
					//					m_pAISystem->AddDebugLine(objPos, objPos + dirToTarget, 255, 0, 0, 4 );
					//					m_pAISystem->AddDebugLine(objPos - norm * 0.25f, objPos + norm * 0.25f, 255, 0, 0, 10 );

					continue;
				}
			}

			if(chooseRandom)
			{
				randomObjs.push_back(obj);
			}
			else
			{
				if(facingTarget)
				{
					float	d = -dotToTarget;
					if(d < nearestDist)
					{
						pFoundObject = obj;
						nearestDist = d;
					}
				}
				else
				{
					float	d = Distance::Point_PointSq(searchPos, objPos);
					if(d < nearestDist)
					{
						pFoundObject = obj;
						nearestDist = d;
					}
				}
			}
		}

		if(chooseRandom && !randomObjs.empty())
		{
			std::random_shuffle(randomObjs.begin(), randomObjs.end());
			pFoundObject = randomObjs[0];
		}

		if(pPipeUser && pFoundObject && !noDevalue)
			m_pAISystem->Devalue(pAI, pFoundObject, false);
//			pPipeUser->Devalue(pFoundObject, false);
	}
	else
	{
		if(pAI)
			pFoundObject = m_pAISystem->GetNearestObjectOfTypeInRange(pAI, type, 0, fRadius);
		else
			pFoundObject = m_pAISystem->GetNearestObjectOfTypeInRange(searchPos, type, 0, fRadius);
	}

	if(pFoundObject)
	{
		if (type==6)
		{
			gEnv->pAISystem->LogComment("<CScriptBind_AI> ", "Entity requested a waypoint anchor! Here is a dump of its state:");
			m_pAISystem->DumpStateOf(pFoundObject);
			return pH->EndFunction();
		}

		if(pH->GetParamCount() > 4)
		{ //requested AIObject position as return value
			vReturnPos.Set(pFoundObject->GetPos());
		}
		if(pH->GetParamCount() > 5)
		{ 
			//requested AIObject direction as return value
			vReturnDir.Set(pFoundObject->GetMoveDir());
		}

		return pH->EndFunction(pFoundObject->GetName()); 
	}

	return pH->EndFunction();
}




int CScriptBind_AI::SoundEvent(IFunctionHandler *pH)
{
	int type = 0;
	float radius = 0;
	Vec3 pos(0,0,0);
	ScriptHandle hdl(0);
	pH->GetParams(pos, radius, type, hdl);
	// can be called from scripts without owner
	if(pH->GetParamCount()>3)
		pH->GetParam(4, hdl);

	//retrieve the entity
	int nID = hdl.n;
	IEntity* pEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);
	// cerate AI sound for AI objects and for non-entities (explosions, etc)
	if (pEntity && pEntity->GetAI())
		m_pAISystem->SoundEvent(pos, radius, (EAISoundEventType)type, pEntity->GetAI());
	else 
		m_pAISystem->SoundEvent(pos, radius, (EAISoundEventType)type, 0);

	return pH->EndFunction();
}


//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAnchor(IFunctionHandler * pH)
{
	int nAnchor;
	float fRadiusMin = 0;
	float fRadiusMax = 0;
	ScriptHandle hdl;
	float fCone = -1.0f;
	bool bFaceAttTarget = false;
	int	findType = 0;
	pH->GetParams( hdl, nAnchor );

	if (pH->GetParamType(3) == svtNumber)
		pH->GetParam(3,fRadiusMax);
	else if( pH->GetParamType(3) == svtObject )
	{
		SmartScriptTable radiusTable;
		pH->GetParam(3,radiusTable);
		if(radiusTable.GetPtr())
		{
			radiusTable->GetValue("min",fRadiusMin);
			radiusTable->GetValue("max",fRadiusMax);
		}
	}
	if (pH->GetParamCount() > 3)
		pH->GetParam(4,findType);

	if( findType == AIANCHOR_NEAREST_IN_FRONT )
		fCone = 0.8f;
	else if( findType == AIANCHOR_NEAREST_FACING_AT )
		bFaceAttTarget = true;

	//retrieve the entity
	int nID = hdl.n;
	IEntity *pEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);
	IAIObject *pObject = 0;
	if (pEntity)
	{
		pObject = pEntity->GetAI();

		IAIObject*	pRefPoint = 0;
		if (IPipeUser* pPipeUser = CastToIPipeUserSafe(pObject))
			pRefPoint = pPipeUser->GetRefPoint();

		if (pObject)
		{
			IAIObject *pAnchor = 0;
			if( findType == AIANCHOR_RANDOM_IN_RANGE )
				pAnchor = m_pAISystem->GetRandomObjectInRange(pObject,nAnchor,fRadiusMin,fRadiusMax);
			else if( findType == AIANCHOR_RANDOM_IN_RANGE_FACING_AT )
				pAnchor = m_pAISystem->GetRandomObjectInRange(pObject,nAnchor,fRadiusMin,fRadiusMax, true);
			else if( findType == AIANCHOR_BEHIND_IN_RANGE)
				pAnchor = m_pAISystem->GetBehindObjectInRange(pObject,nAnchor,fRadiusMin,fRadiusMax);
			else if( findType == AIANCHOR_NEAREST_TO_REFPOINT && pRefPoint )
				pAnchor = m_pAISystem->GetNearestObjectOfTypeInRange(pObject,nAnchor,fRadiusMin,fRadiusMax, AIFAF_USE_REFPOINT_POS);
			else if( findType == AIANCHOR_LEFT_TO_REFPOINT && pRefPoint )
				pAnchor = m_pAISystem->GetNearestObjectOfTypeInRange(pObject,nAnchor,fRadiusMin,fRadiusMax, AIFAF_LEFT_FROM_REFPOINT);
			else if( findType == AIANCHOR_RIGHT_TO_REFPOINT && pRefPoint )
				pAnchor = m_pAISystem->GetNearestObjectOfTypeInRange(pObject,nAnchor,fRadiusMin,fRadiusMax, AIFAF_RIGHT_FROM_REFPOINT);
			else
				pAnchor = m_pAISystem->GetNearestToObjectInRange(pObject,nAnchor,fRadiusMin,fRadiusMax,fCone,bFaceAttTarget);
			if (pAnchor)
				return pH->EndFunction(pAnchor->GetName());
		}
	}
	return pH->EndFunction();
}


//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetSpeciesOf(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	IAIObject *pObject=0;
	if (pEntity)
	{
		IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
		if (pAIActor)
			return pH->EndFunction(pAIActor->GetParameters().m_nSpecies);
	}
	return pH->EndFunction(0);

}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetTypeOf(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	IAIObject *pObject=0;
	if (pEntity)
	{
		pObject = pEntity->GetAI();
		if (pObject)
		{
			return pH->EndFunction(pObject->GetAIType());
		}
	}
	return pH->EndFunction(0);

}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetSubTypeOf(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	IAIObject *pObject=0;
	if (pEntity)
	{
		pObject = pEntity->GetAI();
		if (pObject)
		{
			int subType = pObject->GetSubType();
			int result = 0;
			if ( subType == IAIObject::STP_CAR )
				result = AIOBJECT_CAR;
			if ( subType == IAIObject::STP_BOAT )
				result = AIOBJECT_BOAT;
			if ( subType == IAIObject::STP_HELI )
				result = AIOBJECT_HELICOPTER;
			return pH->EndFunction(result);
		}
	}
	return pH->EndFunction(0);

}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetGroupOf(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	if (pEntity)
	{
		IAIObject* pAI = pEntity->GetAI();
		if (pAI)
			return pH->EndFunction(pAI->GetGroupId());
	}
	return pH->EndFunction();

}


//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::Hostile(IFunctionHandler * pH)
{
	if(pH->GetParamCount()<1)
		return pH->EndFunction(false);
	bool bHostile = false;
	GET_ENTITY(1);
	IAIObject* pObject=NULL;
	IAIObject* pAI2 = NULL;
	if (pEntity)
	{
		pObject = pEntity->GetAI();
		if (pObject && pH->GetParamCount()>1)
		{
			if(pH->GetParamType(2)==svtPointer)
			{
				ScriptHandle hdl;
				pH->GetParam(2,hdl);

				IEntity* pEntity2 = m_pSystem->GetIEntitySystem()->GetEntity(hdl.n);
				if (pEntity2)
					pAI2 = pEntity2->GetAI();
			} 
			else if(pH->GetParamType(2)==svtObject)
			{
				SmartScriptTable pEntityScript;
				ScriptHandle hdl;
				if(pH->GetParam(2,pEntityScript) && pEntityScript->GetValue("id",hdl))
				{
					IEntity* pEntity2 = m_pSystem->GetIEntitySystem()->GetEntity(hdl.n);
					if (pEntity2)
						pAI2 = pEntity2->GetAI();
				}
			}
			else if(pH->GetParamType(2)==svtString)
			{
				const char* sName;
				pH->GetParam(2,sName);
				pAI2 = m_pAISystem->GetAIObjectByName(0,sName);
			}

		}
	}

	if(pObject && pAI2)
	{
		bool bUsingAIIgnorePlayer=true;
		if (pH->GetParamCount()>2)
		{
			if(pH->GetParamType(3)==svtBool)
				pH->GetParam(3,bUsingAIIgnorePlayer);
			else if(pH->GetParamType(3)==svtNumber)
			{
				int i=1;
				pH->GetParam(3,i);
				bUsingAIIgnorePlayer=i!=0;
			}
		}
		bHostile=pObject->IsHostile(pAI2,bUsingAIIgnorePlayer);
	}

	return pH->EndFunction(bHostile);

}

int CScriptBind_AI::SetRefPointPosition(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	//retrieve the entity
	Vec3 vPos(0,0,0);
	if (pEntity && pEntity->GetAI())
	{
		IAIObject *pObject = pEntity->GetAI();
		IPipeUser* pPipeUser = CastToIPipeUserSafe(pObject);
		if (pPipeUser)
		{
			// get new refPoint coordinates
			if(pH->GetParamCount()< 2)
			{
				//missing parameter 2, assume the refpoint owner's attention target as a target position
				if(pPipeUser->GetAttentionTarget())
				{
					vPos= pPipeUser->GetAttentionTarget()->GetPos();
					pPipeUser->SetRefPointPos(vPos);
					return pH->EndFunction(1);
				}
			}
			else
			{
				if (pH->GetParamType(2) == svtNumber)
				{
					int nIDTarget;
					pH->GetParam(2,nIDTarget);
					IEntity *pEntity = m_pSystem->GetIEntitySystem()->GetEntity(nIDTarget);
					if (pEntity && pEntity->GetAI())
					{
						pPipeUser->SetRefPointPos(pEntity->GetAI()->GetPos());
						return pH->EndFunction(1);
					}
				}
				else if (pH->GetParamType(2) == svtString)
				{
					const char* sTargetName;
					pH->GetParam(2,sTargetName);
					if(strcmp(sTargetName,"formation")==0)
					{
						if(pObject)
						{
							IAIObject* pFormationPoint = m_pAISystem->GetFormationPoint(pObject);
							if(pFormationPoint)
							{
								pPipeUser->SetRefPointPos(pFormationPoint->GetPos());
								return pH->EndFunction(1);
							}

						}
					}
					IAIObject* pAIObject = (IAIObject*) m_pAISystem->GetAIObjectByName(0,sTargetName);
					if(pAIObject)
					{
						pPipeUser->SetRefPointPos(pAIObject->GetPos());
						return pH->EndFunction(1);
					}
				}
				else if (pH->GetParamType(2) == svtNull)
				{
					IPipeUser* pPipeUser = CastToIPipeUserSafe(pObject);
					if (pPipeUser)
					{
						if(pPipeUser->GetAttentionTarget())
						{
							vPos= pPipeUser->GetAttentionTarget()->GetPos();
							pPipeUser->SetRefPointPos(vPos);
							return pH->EndFunction(1);
						}
					}
				}
				else
				{
					if(pH->GetParam(2,vPos))
					{
						pPipeUser->SetRefPointPos(vPos);
						return pH->EndFunction(1);
					}
				}
			}
		}

	}

	return pH->EndFunction();
}


int CScriptBind_AI::SetRefPointDirection(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	//retrieve the entity
	Vec3 vPos(0,0,0);
	if (pEntity)
	{
		IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			if(pH->GetParam(2,vPos))
			{
				if( !vPos.IsZero() )
				{
					vPos.Normalize();
					pPipeUser->SetRefPointPos(pPipeUser->GetRefPoint()->GetPos(), vPos);
				}
				return pH->EndFunction(1);
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::SetRefPointRadius(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	//retrieve the entity
	float	radius(0);
	if (pEntity)
	{
		IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			if(pH->GetParam(2,radius))
			{
				pPipeUser->GetRefPoint()->SetRadius(radius);
				return pH->EndFunction(1);
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetRefPointPosition(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (pEntity)
	{
		IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			Vec3 vPos = pPipeUser->GetRefPoint()->GetPos();
			return pH->EndFunction( Script::SetCachedVector( vPos, pH, 1 ) );
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetRefPointDirection(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if (pEntity)
	{
		IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			Vec3 vdir = pPipeUser->GetRefPoint()->GetMoveDir();
			return pH->EndFunction( Script::SetCachedVector( vdir, pH, 1 ) );
		}
	}

	return pH->EndFunction();
}


int CScriptBind_AI::GetFormationPointPosition(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	SmartScriptTable	pTgtPos;
	if( !pH->GetParam( 2, pTgtPos ) )
		return pH->EndFunction();


	if (pEntity && pEntity->GetAI())
	{
		IAIObject *pObject = pEntity->GetAI();
		IAIObject* pFormationPoint = m_pAISystem->GetFormationPoint(pObject);
		if(pFormationPoint)
		{
			Vec3	pos = pFormationPoint->GetPos();
			pTgtPos->SetValue( "x", pos.x );
			pTgtPos->SetValue( "y", pos.y );
			pTgtPos->SetValue( "z", pos.z );
			return pH->EndFunction( true);
		}
	}

	return pH->EndFunction();
}


//====================================================================
// SetRefShapeName
//====================================================================
int CScriptBind_AI::SetRefShapeName(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	const char*	shapeName = 0;
	if(!pH->GetParam(2, shapeName))
		return pH->EndFunction();
	if(!pEntity)
		return pH->EndFunction();

	IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if(pPipeUser)
		pPipeUser->SetRefShapeName(shapeName);

	return pH->EndFunction();
}

//====================================================================
// GetRefShapeName
//====================================================================
int CScriptBind_AI::GetRefShapeName(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	const char*	shapeName = 0;
	if(!pEntity)
		return pH->EndFunction();

	IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if(pPipeUser)
		return pH->EndFunction(pPipeUser->GetRefShapeName());

	return pH->EndFunction();
}

int CScriptBind_AI::SetCharacter(IFunctionHandler * pH)
{
	//SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	const char* sCharacterName = 0;
	const char* sBehaviourName = 0;
	pH->GetParam(2,sCharacterName);

	if (pEntity && sCharacterName)
	{
		IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
		if (pPipeUser)
		{
			if(pH->GetParamCount()>2)
				pH->GetParam(3,sBehaviourName);
			pPipeUser->SetCharacter( sCharacterName,sBehaviourName );
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetBestVehicles(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	SmartScriptTable pVehicleList; 

	GET_ENTITY(1);
	pH->GetParam(2,pVehicleList);

	IAIObject* pAIObject;
	if(pEntity)
		pAIObject = pEntity->GetAI();
	else
		return pH->EndFunction(0);

	TAIObjectList* pList = gEnv->pAISystem->GetVehicleList(pAIObject);
	if(pList && pList->size())		
	{
		int i=0;
		for(TAIObjectList::iterator it = pList->begin();it!=pList->end();++it)
		{
			IEntity* pVehicleEntity = (*it)->GetEntity();
			if(pVehicleEntity)
			{
				IScriptTable* pVehicleTable = pVehicleEntity->GetScriptTable();
				pVehicleList->SetAt(++i,pVehicleTable);
			}
		}
		return pH->EndFunction(i);
	}
	else
		return pH->EndFunction(0);

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsFlightSpaceVoid(IFunctionHandler *pH)
{

	if(pH->GetParamCount() < 4)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsFlightSpaceVoid: Too few parameters." );
		return pH->EndFunction( 0 );
	}

	Vec3 vPos,vFwd,vWng,vUp;
	if( !pH->GetParam( 1, vPos ) || !pH->GetParam( 2, vFwd ) || !pH->GetParam( 3, vWng ) || !pH->GetParam( 4, vUp ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsFlightSpaceVoid: No type param." );
		return pH->EndFunction( 0 );
	}

	return pH->EndFunction( gEnv->pAISystem->IsFlightSpaceVoid(vPos,vFwd,vWng,vUp) );

}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsFlightSpaceVoidByRadius(IFunctionHandler *pH)
{

	if(pH->GetParamCount() < 3 )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsFlightSpaceVoidByRadius: Too few parameters." );
		return pH->EndFunction( 0 );
	}

	Vec3 vPos,vFwd;
	float radius;
	if( !pH->GetParam( 1, vPos ) || !pH->GetParam( 2, vFwd ) || !pH->GetParam( 3, radius ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsFlightSpaceVoidByRadius: No type param." );
		return pH->EndFunction( 0 );
	}

	return pH->EndFunction( gEnv->pAISystem->IsFlightSpaceVoidByRadius(vPos,vFwd,radius) );

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetForcedNavigation(IFunctionHandler * pH)
{

	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();	
	IAIObject *pAIObject(pEntity->GetAI());
	if(!pAIObject)
		return pH->EndFunction();	

	Vec3 vPos(ZERO);
	if(!pH->GetParam(2,vPos))
		return pH->EndFunction();

	SAIEVENT event;
	event.vForcedNavigation = vPos;

	pAIObject->Event(AIEVENT_FORCEDNAVIGATION,&event);

	return pH->EndFunction();

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetAdjustPath(IFunctionHandler * pH)
{

	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();	
	IAIObject *pAIObject(pEntity->GetAI());
	if(!pAIObject)
		return pH->EndFunction();	

	int nType = 0;
	if(!pH->GetParam(2,nType))
		return pH->EndFunction();

	SAIEVENT event;
	event.nType = nType;

	pAIObject->Event(AIEVENT_ADJUSTPATH,&event);

	return pH->EndFunction();

}
//
//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetHeliAdvancePoint(IFunctionHandler * pH)
{

	// First param is the entity that is attacking, and the second param is the target location (script vector3).

	if(pH->GetParamCount() < 5)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: Too few parameters." );
		return pH->EndFunction( 0 );
	}

	GET_ENTITY(1);

	if( !pEntity || !pEntity->GetAI() )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No Entity or AI." );
		return pH->EndFunction( 0 );
	}

	//retrieve the entity
	Vec3 targetPos( 0, 0, 0 );
	Vec3 targetDir( 0, 0, 0 );
	SmartScriptTable	pNewTgtPos;
	SmartScriptTable	pNewTgtDir;
	int	type = 0;

	if( !pH->GetParam( 2, type ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No type param." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 3, targetPos ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No target pos." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 4, targetDir ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No target dir." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 5, pNewTgtPos ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No out target pos." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 6, pNewTgtDir ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No out target dir." );
		return pH->EndFunction( 0 );
	}

	IAIObject* pAIObj = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if(!pAIActor)
		return pH->EndFunction(0);

	Vec3	entPos = pEntity->GetPos();
	Vec3	diff = entPos - targetPos;

	//--------------------------------------------------------------------------------

	Vec3	diff2DN(diff);
	diff2DN.z=0;
	diff2DN.normalize();

	AgentParameters	agentParams = pAIActor->GetParameters();
	float	attackHeight = agentParams.m_fAttackZoneHeight;
	float	attackRad = agentParams.m_fAttackRange * 0.1f;
	Vec3	desiredPos = targetPos - diff2DN*attackRad;

	desiredPos = targetPos - diff2DN*attackRad + Vec3(0,0,attackHeight);
	// Returns the calculated target position and direction.
	pNewTgtPos->SetValue( "x", desiredPos.x );
	pNewTgtPos->SetValue( "y", desiredPos.y );
	pNewTgtPos->SetValue( "z", desiredPos.z );

	pNewTgtDir->SetValue( "x", diff2DN.x );
	pNewTgtDir->SetValue( "y", diff2DN.y );
	pNewTgtDir->SetValue( "z", diff2DN.z );

	return pH->EndFunction( 1 );




	/*
	//--------------------------------------------------------------------------------

	// Use target direction if available
	if( targetDir.GetLength() > 0.0001f )
	diff = -targetDir;

	float	angle = atan2( diff.y, diff.x );

	Vec3	attackPos[6];
	Vec3	attackDir[6];
	bool	bAttackSight[6] = { false, false, false, false, false, false };
	float	attackPosScore[6] = { 4, 8, 0, 8, 4, 0 };

	Vec3	bbox[2];
	bbox[0].Set( -3, -3, -3 );
	bbox[1].Set( 3, 3, 3 );

	AgentParameters	agentParams = pAIObj->GetParameters();

	float	attackHeight = agentParams.m_fAttackZoneHeight;
	float	attackRad = agentParams.m_fAttackRange * 0.5f;

	// Randomize the location a little.
	angle += (((cry_rand() * 10) / RAND_MAX) * 0.2f - 1.0f) * (gf_PI * 2.0f / 15.0f);
	attackHeight = (float)(cry_rand() / (float)RAND_MAX) * agentParams.m_fAttackZoneHeight * 0.25f;

	int	sightScore = 10;	// Is the target visible.
	int	heightScore = 2;	// Is the location near the attack height.
	int	blockingScore = 0;	// Is there an entity at the same location.
	int	randomScore = 4;	// Random offset.
	float	maxObstacleHeight = 0.0f;	// The maximum obstacle height above the terrain which is accepted.
	float	maxTargetObstacleHeight = 0.0f;	// The maximum obstacle height above the terrain at the target which is accepted.

	if( type == 0 )
	{
	// Attack position.
	attackPosScore[0] = 0;	// Enter direction.
	attackPosScore[1] = 8;
	attackPosScore[2] = 4;
	attackPosScore[3] = 0;	// Opposite direction.
	attackPosScore[4] = 4;
	attackPosScore[5] = 8;

	attackHeight += agentParams.m_fAttackZoneHeight * 2.0f;

	attackRad = agentParams.m_fAttackRange * (0.1f + 0.1f * (float)((cry_rand() * 4) / RAND_MAX) / 4.0f);

	sightScore = 10;	// Is the target visible.
	heightScore = 2;	// Is the location near the attack height.
	blockingScore = 0;	// Is there an entity at the same location.
	randomScore = 4;	// Random offset.

	maxObstacleHeight = 100;
	maxTargetObstacleHeight = 100;
	attackHeight += (float)((cry_rand() * 3) / RAND_MAX) * agentParams.m_fAttackZoneHeight; // * 2.0f;
	}

	int	best = 0;
	float bestScore = -1.f;
	int	numValidPos = 0;
	for( int i = 0; i < 6; i++ )
	{
	// Calc new target position.
	angle += (float)i * (gf_PI * 2.0f / 6.0f);
	attackDir[i].Set( cos( angle ), sin( angle ), 0 );
	attackPos[i] = targetPos - attackDir[i] * attackRad;
	attackPos[i].z += attackHeight;

	Vec3	closestValid( attackPos[i] );
	bool	bValidPos = false;

	if( m_pAISystem->GetNodeGraph()->GetEnclosing( attackPos[i], IAISystem::NAV_FLIGHT, 0, 0, &closestValid ) )
	{
	bValidPos = true;
	if( closestValid != attackPos[i] )
	{
	attackPos[i] = closestValid;
	attackPos[i].x += pAIObj->GetMovementAbility().minFlightHeight;
	}
	}

	//		if( attackPos[i].z < maxObstHeight )
	//			attackPos[i].z = maxObstHeight;

	// Recalc the attack direction.
	attackDir[i] = targetPos - attackPos[i];

	// Check if the target point can be seen from the new target pos.
	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

	ray_hit	hit;
	int	numHits = pPhysWorld->RayWorldIntersection( attackPos[i], attackDir[i], ent_static, rwi_stop_at_pierceable, &hit, 1 );
	bAttackSight[i] = numHits > 0 ? false : true;

	// Normalize the direction. the unnormalized version is needed for the ray intersection.
	attackDir[i].NormalizeSafe();

	if( bAttackSight[i] )
	attackPosScore[i] += sightScore;

	// Favor positions closer to the real attack range.
	if( fabs( targetPos.z - attackPos[i].z ) < attackHeight * 1.1f )
	attackPosScore[i] += heightScore;

	// Check if the point is too close to the old ones.
	for( std::list<SLastApproachParam>::iterator ptIt = m_lstLastestApproachParam.begin(); ptIt != m_lstLastestApproachParam.end(); ++ptIt )
	{
	SLastApproachParam&	param = (*ptIt);
	Vec3	diff = attackPos[i] - param.pos;
	if( diff.len2() < 10 * 10 )
	attackPosScore[i] += -30;
	}

	// Add small jitter into the selection.
	attackPosScore[i] += (cry_rand() * randomScore) / RAND_MAX;

	if( attackPosScore[i] > bestScore )
	{
	best = i;
	bestScore = attackPosScore[i];
	}

	//		if( bAttackSight[i] && !bTooHighObstacle && !bTooHighTargetObstacle )
	if( bAttackSight[i] && bValidPos )
	numValidPos++;
	}

	// Keep history of path points.
	SLastApproachParam	params;
	params.pos = attackPos[best];
	params.dir = attackDir[best];
	m_lstLastestApproachParam.push_back( params );

	// Keep the history count sane.
	if( m_lstLastestApproachParam.size() > 12 )
	m_lstLastestApproachParam.pop_front();

	gEnv->pAISystem->LogComment("<CScriptBind_AI> ", "%d valid pos", numValidPos );

	// Returns the calculated target position and direction.
	pNewTgtPos->SetValue( "x", attackPos[best].x );
	pNewTgtPos->SetValue( "y", attackPos[best].y );
	pNewTgtPos->SetValue( "z", attackPos[best].z );

	pNewTgtDir->SetValue( "x", attackDir[best].x );
	pNewTgtDir->SetValue( "y", attackDir[best].y );
	pNewTgtDir->SetValue( "z", attackDir[best].z );

	return pH->EndFunction( numValidPos );

	*/
}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetFlyingVehicleFlockingPos(IFunctionHandler * pH)
{

	Vec3 vSumOfPotential(ZERO);

	GET_ENTITY(1);
	if( !pEntity || !pEntity->GetAI() )
		return pH->EndFunction( vSumOfPotential );

	if(  pH->GetParamCount() != 5 )
		return pH->EndFunction( vSumOfPotential );

	float	radius		=40.0f;
	float	massfilter	=200.0f;
	float	sec			=3.0f;
	float	checkAbove	=10.0f;
	Vec3	vTarget(ZERO);

	if( !pH->GetParam(2,radius) )
		return pH->EndFunction( vSumOfPotential );

	if( !pH->GetParam(3,massfilter) )
		return pH->EndFunction( vSumOfPotential );

	if( !pH->GetParam(4,sec) )
		return pH->EndFunction( vSumOfPotential );

	if( !pH->GetParam(5,checkAbove) )
		return pH->EndFunction( vSumOfPotential );

	//---------------------------------------------------------------------------

	const Vec3 vUp( 0.0f, 0.0f, 1.0f);
	const Vec3 vDown( 0.0f, 0.0f, -1.0f);

	// get my information 
	pe_status_dynamics pMyDynamics;
	IPhysicalEntity *pMyPhycs = pEntity->GetPhysics();
	if( !pMyPhycs )
		return pH->EndFunction( vSumOfPotential );

	pMyPhycs->GetStatus(&pMyDynamics);
	Vec3 vMyVelocity(pMyDynamics.v);
	{
		// at least make a 5m circle in any case
		if ( vMyVelocity.GetLength() < 0.01f )
		{
			Matrix33 worldMat(pEntity->GetWorldTM());
			vMyVelocity = worldMat * FORWARD_DIRECTION * 5.0f;
		}
		else
		if ( vMyVelocity.GetLength() < 5.0f )
		{
			vMyVelocity.NormalizeSafe();
			vMyVelocity *=5.0f;
		}
	}

	Vec3 vMyPos(pEntity->GetWorldPos());
	Vec3 vMyPos2D( vMyPos.x, vMyPos.y, 0.0f );
	Vec3 vMyPosInFuture( vMyPos + vMyVelocity *sec );
	Vec3 vMyPosInFuture2D( vMyPosInFuture.x, vMyPosInFuture.y, 0.0f );
	Vec3 vMyCircleCetner2D( vMyPos2D );
	Vec3 vR2( vMyPosInFuture2D - vMyCircleCetner2D );
	float r2 = vR2.GetLength();

	float mySpeed = vMyVelocity.GetLength();

	Vec3 vMyVelocity2D( vMyVelocity.x, vMyVelocity.y, 0.0f );
	Vec3 vMyVelocity2DUnit( vMyVelocity2D.GetNormalizedSafe() );
	Vec3 vMyVelocity2DUnitWng( (vMyVelocity2DUnit.Cross(vUp)).GetNormalizedSafe() );

	for ( float deg = DEG2RAD(0.0f); deg < 3.1416f*2.0f ; deg += DEG2RAD(5.0f) )
	{
		float degSrc =deg;
		float degDst =deg + DEG2RAD(5.0f);

		Vec3 debugVecSrc;
		Vec3 debugVecDst;

		debugVecSrc.x = cos( degSrc ) *  r2;
		debugVecSrc.y = sin( degSrc ) *  r2;
		debugVecSrc.z = 0.0f;

		debugVecDst.x = cos( degDst ) *  r2;
		debugVecDst.y = sin( degDst ) *  r2;
		debugVecDst.z = 0.0f;

		debugVecSrc += vMyPos;
		debugVecDst += vMyPos;

		gEnv->pAISystem->AddDebugLine(debugVecSrc, debugVecDst, 255, 255, 255, 1.0f);

	}

	//---------------------------------------------------------------------------
	// get enemy information
	float entityRadius = radius + 10.0f;
	const Vec3 bboxsize( entityRadius, entityRadius, entityRadius );

	IPhysicalWorld *pWorld = m_pSystem->GetIPhysicalWorld();

	IPhysicalEntity **pEntityList;
	int nCount = pWorld->GetEntitiesInBox( vMyPos-bboxsize, vMyPos+bboxsize, pEntityList, ent_living|ent_rigid|ent_sleeping_rigid);

	float totalFound = 0.0f;
	float minLen = radius * 20.0f;

	for ( int i=0; i<nCount; ++i ){

		IEntity *pEntityAround = m_pSystem->GetIEntitySystem()->GetEntityFromPhysics(pEntityList[i]);

		if ( !pEntityAround )								// just in case
			continue;

		if ( pEntityAround->GetId() == pEntity->GetId() )	// skip myself
			continue;

		pe_status_dynamics pdynamics;						// if it has a small mass. guess can cllide.
		pEntityList[i]->GetStatus(&pdynamics);				// falling humans/ small aliens etc
		if ( pdynamics.mass < massfilter )
			continue;

 		Vec3 vHisVelocity(pdynamics.v);
		{
			if ( vHisVelocity.GetLength() < 0.01f )
			{
				Matrix33 worldMat(pEntityAround->GetWorldTM());
				vHisVelocity = worldMat * FORWARD_DIRECTION * 5.0f;
			}
			else
			if ( vHisVelocity.GetLength() < 5.0f )
			{
				vHisVelocity.NormalizeSafe();
				vHisVelocity *=5.0f;
			}
		}

		float hisSpeed = vHisVelocity.GetLength();

		Vec3 vHisVelocity2D( vHisVelocity.x, vHisVelocity.y, 0.0f );
		Vec3 vHisVelocity2DUnit( vHisVelocity2D.GetNormalizedSafe() );
		Vec3 vHisVelocity2DUnitWng( (vHisVelocity2DUnit.Cross(vUp)).GetNormalizedSafe() );

		Vec3 vHisPos(pEntityAround->GetWorldPos());
		Vec3 vHisPos2D( vHisPos.x, vHisPos.y, 0.0f );
		Vec3 vHisPosInFuture( vHisPos + vHisVelocity *sec );
		Vec3 vHisPosInFuture2D( vHisPosInFuture.x, vHisPosInFuture.y, 0.0f );
		Vec3 vHisCircleCetner2D( vHisPos2D );
		Vec3 vR1( vHisPosInFuture2D - vHisCircleCetner2D );

		float r1 = vR1.GetLength();

		float distance2d = ( vHisPos2D - vMyPos2D ).GetLength();
		if ( distance2d > r1+r2 )
			continue;

		if ( distance2d > minLen )
			continue;
		
		float height = vHisPos.z - m_pSystem->GetI3DEngine()->GetTerrainElevation( vHisPos.x, vHisPos.y );
		if ( height < checkAbove )							// only think flying object
			continue;

		float heightAboveWater = vHisPos.z - m_pSystem->GetI3DEngine()->GetWaterLevel(&vHisPos);
		if ( heightAboveWater < checkAbove )				// only think flying object
			continue;

		if ( vHisVelocity.GetLength() < 0.1f )				// will write other implement for this later
			continue;

		Vec3 vUnitDir2D( vHisPos2D - vMyPos2D );
		{
			vUnitDir2D.z =0.0f;
			if ( vUnitDir2D.GetLength() < 0.1f )
				continue;									// too close to calculate
		}

		// condition : only check the object in FOV
		Vec3 vUnitDir2DUnit( vUnitDir2D.GetNormalizedSafe() );

		if ( vUnitDir2DUnit.Dot( vMyVelocity2DUnit ) < 0.0f )
			continue;										// he is not in FOV of me

		// calculate intersections between 2 circle

		float a = vHisCircleCetner2D.x - vMyCircleCetner2D.x;
		float b = vHisCircleCetner2D.y - vMyCircleCetner2D.y;

		if ( fabsf(a) < 0.0001f )
			continue;										// ideally need swap(a,b), but just continue for easiness

		if ( fabsf(b) < 0.0001f )
			continue;										// ideally need swap(a,b), but just continue for easiness

		float t		= -1.0f * b / a;
		float ss	= ( a*a + b*b - r1*r1 + r2*r2 );
		float s		= ss / ( 2.0f *a );
		float k		= t*t + 1.0f;
		float l		= 2.0f * s * t;
		float m		= s*s - r2 * r2;
		float det	= l*l - 4.0f * k * m;

		if ( det < 0.0f )
			continue;

		float y1	= ( -1.0f * l + sqrt( det ) ) / ( 2.0f *k );
		float y2	= ( -1.0f * l - sqrt( det ) ) / ( 2.0f *k );
		float x1	= t * y1 + s;
		float x2	= t * y2 + s;

		Vec3 evadeVector2D_1 ( x1, y1, 0.0f );
		Vec3 evadeVector2D_2 ( x2, y2, 0.0f );
		Vec3 evadeVectorUnit2D_1 ( evadeVector2D_1.GetNormalizedSafe() );
		Vec3 evadeVectorUnit2D_2 ( evadeVector2D_2.GetNormalizedSafe() );

		// condition : current speed vector sould be in the corn which is made by these 2 evadeVector2D

		if (  sgn( vMyVelocity2DUnitWng.Dot(evadeVectorUnit2D_1) ) 
			* sgn( vMyVelocity2DUnitWng.Dot(evadeVectorUnit2D_2) ) > 0.0f )
			continue;

		// condition : choose a vector which is more pararrel each other

		float dot1 = vHisVelocity2DUnitWng.Dot(evadeVectorUnit2D_1);
		float dot2 = vHisVelocity2DUnitWng.Dot(evadeVectorUnit2D_2);

		if ( dot1 > dot2 )
		{
			vSumOfPotential = evadeVector2D_1;
		}
		else
		{
			vSumOfPotential = evadeVector2D_2;
		}

		{

			Vec3 debugVec;
			Vec3 debugVec2;

			debugVec = evadeVector2D_1 + vMyPos2D;
			debugVec.z =  vMyPos.z;
			debugVec2 = evadeVector2D_2 + vMyPos2D;
			debugVec2.z =  vMyPos.z;
			if ( dot1 > dot2 )
			{
				gEnv->pAISystem->AddDebugLine( vMyPos, debugVec, 0, 255, 0, 1.0f);
				gEnv->pAISystem->AddDebugLine( vMyPos, debugVec2, 255, 255, 255, 1.0f);
			}
			else
			{
				gEnv->pAISystem->AddDebugLine( vMyPos, debugVec, 255, 255, 255, 1.0f);
				gEnv->pAISystem->AddDebugLine( vMyPos, debugVec2, 0, 255, 0, 1.0f);
			}

		}
		minLen = distance2d;

	}

	return pH->EndFunction( vSumOfPotential );

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::CheckVehicleColision(IFunctionHandler * pH)
{
	Vec3 vHitPos(ZERO);

	if( pH->GetParamCount() != 4 )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CCheckVehicleColision: suppose 4 arguments");
		return pH->EndFunction( vHitPos );
	}

	GET_ENTITY(1);

	IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
	if( !pEntity || !pPhysicalEntity || !pEntity->GetAI() )
		return pH->EndFunction( vHitPos );

	Vec3 vPos,vFwd;
	float radius;

	if( !pH->GetParam( 2, vPos ) || !pH->GetParam( 3, vFwd ) || !pH->GetParam( 4, radius ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CheckVehicleColision: No type param." );
		return pH->EndFunction( vHitPos );
	}

	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;

	primitives::sphere spherePrim;
	spherePrim.center = vPos;
	spherePrim.r = radius;

	geom_contact*	pContact = 0;
	geom_contact**	ppContact = &pContact;

	int geomFlagsAll=0;
	int geomFlagsAny=geom_colltype0;

	float d = pPhysics->PrimitiveWorldIntersection(primitives:: sphere:: type, &spherePrim, vFwd, 
		ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid|ent_ignore_noncolliding, ppContact, 
		0, geomFlagsAny, 0,0,0, &pPhysicalEntity, 1);

	if ( d > 0.0f )
	{
		vHitPos = pContact->pt;
		gEnv->pAISystem->AddDebugLine( pEntity->GetPos(), vHitPos, 0, 255, 255, 1.0f );
	}
	else
	{
		gEnv->pAISystem->AddDebugLine( pEntity->GetPos(), pEntity->GetPos()+vFwd, 0, 0, 255, 1.0f );
	}

	return pH->EndFunction( vHitPos );

}
//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IntersectsForbidden(IFunctionHandler * pH)
{

	Vec3 vStart,vEnd,vResult;

	if(!pH->GetParam(1,vStart))
		return pH->EndFunction( 0 );
	if(!pH->GetParam(2,vEnd))
		return pH->EndFunction( 0 );

	bool result =gEnv->pAISystem->IntersectsForbidden( vStart, vEnd, vResult );
	if ( result == true )
	{
		return pH->EndFunction( vResult );
	}
	else
	{
		return pH->EndFunction( vEnd );
	}


}
//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsPointInFlightRegion(IFunctionHandler * pH)
{

	Vec3 vStart;
	int nBuildingID;
	IVisArea *pGoalArea; 

	if(!pH->GetParam(1,vStart))
		return pH->EndFunction( 0 );

	return pH->EndFunction( IAISystem::NAV_UNSET != gEnv->pAISystem->CheckNavigationType(vStart, nBuildingID, pGoalArea, IAISystem::NAV_FLIGHT) );

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsPointInWaterRegion(IFunctionHandler * pH)
{

	Vec3 vStart;

	if(!pH->GetParam(1,vStart))
		return pH->EndFunction( 0 );

	float level = m_pSystem->GetI3DEngine()->GetWaterLevel(&vStart) -  m_pSystem->GetI3DEngine()->GetTerrainElevation( vStart.x,vStart.y );

	return pH->EndFunction( level );

}

//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetEnclosingSpace(IFunctionHandler * pH)
{
	// This function tests the approximate space around certain point in space.
	// The test is done by shootin 4 or 6 rays depending on the current navigation type of the specified entity.
	//

	// Get parameters. The expected parameters are entity.id, check position, and check radius.
	if(pH->GetParamCount() < 3)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetNavigableSpaceSize: Too few parameters." );
		return pH->EndFunction( 0 );
	}

	Vec3	pos( 0, 0, 0 );
	float	rad = 0;

	int checkType = CHECKTYPE_MIN_DISTANCE;

	GET_ENTITY(1);
	if( !pEntity)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetNavigableSpaceSize: No Entity or AI." );
		return pH->EndFunction( 0 );
	}
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if( !pAIActor )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetNavigableSpaceSize: No Entity or AI." );
		return pH->EndFunction( 0 );
	}

	if( !pH->GetParam( 2, pos ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetNavigableSpaceSize: No pos." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 3, rad ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetNavigableSpaceSize: No radius." );
		return pH->EndFunction( 0 );
	}
	if(pH->GetParamCount() > 3)
		pH->GetParam( 4, checkType );

	// Calculate approximate distance to close by geometry.
	const Vec3	tests[6] = 
	{
		Vec3(  1, 0, 0 ),
		Vec3( -1, 0, 0 ),
		Vec3( 0,  1, 0 ),
		Vec3( 0, -1, 0 ),
		Vec3( 0, 0,  1 ),
		Vec3( 0, 0, -1 ),
	};

	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

	/*	float	avgDist = 0;
	float	weight = 0;*/
	float	minDist = rad;

	unsigned numTests = 6;

	IAIObject*	pObj = pEntity->GetAI();

	// The down test is not done when in 2D navigation area.
	IVisArea *pGoalArea; 
	int leaderBuilding = -1;
	int nBuilding;
	IAISystem::ENavigationType	type = gEnv->pAISystem->CheckNavigationType( pos, nBuilding, pGoalArea, pAIActor->GetMovementAbility().pathfindingProperties.navCapMask );
	if( (type & (IAISystem::NAV_VOLUME | IAISystem::NAV_WAYPOINT_3DSURFACE)) == 0 )
		numTests = 4;

	float prevDist=0;
	int numIntermediateTests = (checkType == CHECKTYPE_MIN_ROOMSIZE ? 2:1);
	// Do the ray tests. The purpose of the weighting is to bias the clamped values.
	for( unsigned i = 0; i < numTests;  )
	{
		ray_hit	hit;
		float totalDist = 0;
		for(int k=0;k<numIntermediateTests; k++)
		{
			float	d = rad;
			if( RayWorldIntersectionWrapper( pos, tests[i] * rad, ent_static, rwi_stop_at_pierceable, &hit, 1 ) )
				d = hit.dist;
			totalDist += d;
			i++;
		}
		/*		float	w = 1.0f - (d / (rad * 2.0f));
		avgDist += d * w;
		weight += w;*/
		if( totalDist < minDist )
			minDist = totalDist;
	}
	//	if( weight > 0.0001f )
	//		avgDist /= weight;
	//	return pH->EndFunction( avgDist );
	return pH->EndFunction( minDist );
}

//
//-------------------------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetAlienApproachParams(IFunctionHandler * pH)
{
	// First param is the entity that is attacking, and the second param is the target location (script vector3).

	if(pH->GetParamCount() < 5)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: Too few parameters." );
		return pH->EndFunction( 0 );
	}

	GET_ENTITY(1);

	if( !pEntity || !pEntity->GetAI() )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No Entity or AI." );
		return pH->EndFunction( 0 );
	}

	//retrieve the entity
	Vec3 targetPos( 0, 0, 0 );
	Vec3 targetDir( 0, 0, 0 );
	SmartScriptTable	pNewTgtPos;
	SmartScriptTable	pNewTgtDir;
	int	type = 0;

	if( !pH->GetParam( 2, type ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No type param." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 3, targetPos ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No target pos." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 4, targetDir ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No target dir." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 5, pNewTgtPos ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No out target pos." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 6, pNewTgtDir ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: No out target dir." );
		return pH->EndFunction( 0 );
	}

	IAIObject*	pAIObj = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if(!pAIActor)
		return pH->EndFunction(0);

	Vec3	entPos = pEntity->GetPos();
	Vec3	diff = entPos - targetPos;

	// Use target direction if available
	if( targetDir.GetLength() > 0.0001f )
		diff = -targetDir;

	float	angle = atan2( diff.y, diff.x );

	Vec3	attackPos[6];
	Vec3	attackDir[6];
	bool	bAttackSight[6] = { false, false, false, false, false, false };
	float	attackPosScore[6] = { 4, 8, 0, 8, 4, 0 };

	Vec3	bbox[2];
	bbox[0].Set( -3, -3, -3 );
	bbox[1].Set( 3, 3, 3 );

	AgentParameters	agentParams = pAIActor->GetParameters();

	float	attackHeight = agentParams.m_fAttackZoneHeight * 2.0f;
	float	attackRad = agentParams.m_fAttackRange * 0.25f;

	// Randomize the location a little.
	angle += (((cry_rand() * 10) / RAND_MAX) * 0.2f - 1.0f) * (gf_PI * 2.0f / 15.0f);
	attackHeight = (float)(cry_rand() / (float)RAND_MAX) * agentParams.m_fAttackZoneHeight * 0.25f;

	int	sightScore = 10;	// Is the target visible.
	int	heightScore = 2;	// Is the location near the attack height.
	int	blockingScore = 0;	// Is there an entity at the same location.
	int	randomScore = 4;	// Random offset.
	float	maxObstacleHeight = 0.0f;	// The maximum obstacle height above the terrain which is accepted.
	float	maxTargetObstacleHeight = 0.0f;	// The maximum obstacle height above the terrain at the target which is accepted.

	if( type == 0 )
	{
		// Attack position.
		attackPosScore[0] = 0;	// Enter direction.
		attackPosScore[1] = 8;
		attackPosScore[2] = 4;
		attackPosScore[3] = 0;	// Opposite direction.
		attackPosScore[4] = 4;
		attackPosScore[5] = 8;

		attackHeight += agentParams.m_fAttackZoneHeight;

		attackRad = agentParams.m_fAttackRange * (0.1f + 0.1f * (float)((cry_rand() * 4) / RAND_MAX) / 4.0f);

		sightScore = 10;	// Is the target visible.
		heightScore = 2;	// Is the location near the attack height.
		blockingScore = 0;	// Is there an entity at the same location.
		randomScore = 4;	// Random offset.

		maxObstacleHeight = 100;
		maxTargetObstacleHeight = 100;
		attackHeight += (float)(cry_rand() / RAND_MAX) * agentParams.m_fAttackZoneHeight;
	}
	else if( type == 3 )
	{
		// Attack position.
		attackPosScore[0] = 8;	// Enter direction.
		attackPosScore[1] = 4;
		attackPosScore[2] = 2;
		attackPosScore[3] = 0;	// Opposite direction.
		attackPosScore[4] = 2;
		attackPosScore[5] = 4;

		attackHeight += agentParams.m_fAttackZoneHeight;

		attackRad = agentParams.m_fAttackRange * 0.1f; //agentParams.m_fAttackRange * (0.1f + 0.1f * (float)((cry_rand() * 4) / RAND_MAX) / 4.0f);

		sightScore = 10;	// Is the target visible.
		heightScore = 2;	// Is the location near the attack height.
		blockingScore = 0;	// Is there an entity at the same location.
		randomScore = 2;	// Random offset.

		// Require clear place for melee attacks
		maxObstacleHeight = 4.0f;
		maxTargetObstacleHeight = 1.5f;
		//		attackHeight += (float)((cry_rand() * 3) / RAND_MAX) * agentParams.m_fAttackZoneHeight;
	}
	else if( type == 1 )
	{
		// Recoil position.
		attackPosScore[0] = 0;	// Enter direction
		attackPosScore[1] = 0;
		attackPosScore[2] = 4;
		attackPosScore[3] = 8;	// Opposite direction.
		attackPosScore[4] = 4;
		attackPosScore[5] = 0;

		attackHeight += agentParams.m_fAttackZoneHeight * 2.0f;
		attackRad = agentParams.m_fAttackRange * 0.15f;

		sightScore = 10;	// Is the target visible.
		heightScore = 2;	// Is the location near the attack height.
		blockingScore = 0;	// Is there an entity at the same location.
		randomScore = 6;	// Random offset.

		maxObstacleHeight = 100;
		maxTargetObstacleHeight = 100;
	}
	else if( type == 2 )
	{
		// Search position.
		attackPosScore[0] = 0;	// Enter direction
		attackPosScore[1] = 0;
		attackPosScore[2] = 1;
		attackPosScore[3] = 10;	// Opposite direction.
		attackPosScore[4] = 1;
		attackPosScore[5] = 0;

		attackHeight += agentParams.m_fAttackZoneHeight;
		attackRad = 10.0f; //agentParams.m_fAttackRange * 0.1f;

		sightScore = 8;	// Is the target visible.
		heightScore = 0;	// Is the location near the attack height.
		blockingScore = -10;	// Is there an entity at the same location.
		randomScore = 4;	// Random offset.

		maxObstacleHeight = 5;
		maxTargetObstacleHeight = 5;
	}
	else if( type == 4 )
	{
		// Attack position.
		attackPosScore[0] = 4;	// Enter direction.
		attackPosScore[1] = 4;
		attackPosScore[2] = 2;
		attackPosScore[3] = 2;	// Opposite direction.
		attackPosScore[4] = 2;
		attackPosScore[5] = 4;

		attackHeight += agentParams.m_fAttackZoneHeight;

		attackRad = agentParams.m_fAttackRange * (0.02f + (float)((cry_rand() * 4) / RAND_MAX) * 0.02f);

		sightScore = 10;	// Is the target visible.
		heightScore = 0;	// Is the location near the attack height.
		blockingScore = 0;	// Is there an entity at the same location.
		randomScore = 6;	// Random offset.

		maxObstacleHeight = 20;
		maxTargetObstacleHeight = 15;
		attackHeight += (float)((cry_rand() * 3) / RAND_MAX) * agentParams.m_fAttackZoneHeight * 0.25f;
	}

	int	best = 0;
	float bestScore = -1.f;
	int	numValidPos = 0;

	// Check the target height.
	/*	bool	bTooHighTargetObstacle = false;
	{
	float	targetRadius = 2.0f;
	// Make sure the point is above the terrain.
	float	terrainHeight = m_pSystem->GetI3DEngine()->GetTerrainElevation( targetPos.x, targetPos.y );
	float	maxObstHeight = terrainHeight;

	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

	IPhysicalEntity**	ppList = NULL;
	int	numEntites = pPhysWorld->GetEntitiesInBox( targetPos + Vec3( -1, -1, -1 ) * targetRadius, targetPos + Vec3( 1, 1, 1 ) * targetRadius, ppList, ent_static );
	for( int j = 0; j < numEntites; j++ )
	{
	pe_status_pos status;
	// Get bbox (status) from physics engine
	if(ppList[j]->GetType() == PE_LIVING)
	continue;
	ppList[j]->GetStatus( &status );

	float	height = status.pos.z + status.BBox[1].z; 

	if( height > maxObstHeight )
	maxObstHeight = height;
	}

	bTooHighTargetObstacle = (maxObstHeight - terrainHeight) > maxTargetObstacleHeight;
	}

	if( bTooHighTargetObstacle )
	{
	gEnv->pAISystem->LogComment("<CScriptBind_AI> ", "bTooHighTargetObstacle" );
	}*/

	for( int i = 0; i < 6; i++ )
	{
		// Calc new target position.
		angle += (float)i * (gf_PI * 2.0f / 6.0f);
		attackDir[i].Set( cos( angle ), sin( angle ), 0 );
		attackPos[i] = targetPos - attackDir[i] * attackRad;
		attackPos[i].z += attackHeight;

		// Make sure the point is above the terrain.
		/*		float	terrainHeight = m_pSystem->GetI3DEngine()->GetTerrainElevation( attackPos[i].x, attackPos[i].y );
		float	maxObstHeight = terrainHeight;

		IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

		IPhysicalEntity**	ppList = NULL;
		int	numEntites = pPhysWorld->GetEntitiesInBox( attackPos[i] + bbox[0], attackPos[i] + bbox[1], ppList, ent_static );
		for( int j = 0; j < numEntites; j++ )
		{
		pe_status_pos status;
		// Get bbox (status) from physics engine
		if(ppList[j]->GetType() == PE_LIVING)
		continue;
		ppList[j]->GetStatus( &status );

		float	height = status.pos.z + status.BBox[1].z; 

		if( height > maxObstHeight )
		{
		maxObstHeight = height;
		attackPosScore[i] += blockingScore;
		}
		}

		bool	bTooHighObstacle = (maxObstHeight - terrainHeight) > maxObstacleHeight;

		maxObstHeight += attackHeight;*/

		/*
		virtual GraphNode *GetEnclosing(const Vec3 &pos, int navCap, GraphNode *pStart = 0 ,bool bOutsideOnly = false,
		float range = 0.0f, Vec3 * closestValid = 0) = 0;
		*/

		Vec3	checkPos( attackPos[i] - Vec3( 0, 0, pAIActor->GetMovementAbility().minFlightHeight ) );
		Vec3	closestValid( attackPos[i] );
		bool	bValidPos = false;

		if( m_pAISystem->GetNodeGraph()->GetEnclosing( checkPos, IAISystem::NAV_FLIGHT, 0.0f, 0, 0.0f, &closestValid ) )
		{
			bValidPos = true;
			attackPos[i] = closestValid + Vec3( 0, 0, pAIActor->GetMovementAbility().minFlightHeight );
		}

		//		if( attackPos[i].z < maxObstHeight )
		//			attackPos[i].z = maxObstHeight;

		// Recalc the attack direction.
		attackDir[i] = targetPos - attackPos[i];

		// Check if the target point can be seen from the new target pos.
		IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

		ray_hit	hit;
		int	numHits = RayWorldIntersectionWrapper( attackPos[i], attackDir[i], ent_static, rwi_stop_at_pierceable, &hit, 1 );
		bAttackSight[i] = numHits > 0 ? false : true;

		// Normalize the direction. the unnormalized version is needed for the ray intersection.
		attackDir[i].NormalizeSafe();

		if( bAttackSight[i] )
			attackPosScore[i] += sightScore;

		// Favor positions closer to the real attack range.
		if( fabs( targetPos.z - attackPos[i].z ) < attackHeight * 1.1f )
			attackPosScore[i] += heightScore;

		// Check if the point is too close to the old ones.
		for( std::list<SLastApproachParam>::iterator ptIt = m_lstLastestApproachParam.begin(); ptIt != m_lstLastestApproachParam.end(); ++ptIt )
		{
			SLastApproachParam&	param = (*ptIt);
			Vec3	diff = attackPos[i] - param.pos;
			if( diff.len2() < 10 * 10 )
				attackPosScore[i] += -30;
		}

		// Add small jitter into the selection.
		attackPosScore[i] += (cry_rand() * randomScore) / RAND_MAX;

		if( attackPosScore[i] > bestScore )
		{
			best = i;
			bestScore = attackPosScore[i];
		}

		//		if( bAttackSight[i] && !bTooHighObstacle && !bTooHighTargetObstacle )
		if( bAttackSight[i] && bValidPos )
			numValidPos++;
	}

	// Keep history of path points.
	SLastApproachParam	params;
	params.pos = attackPos[best];
	params.dir = attackDir[best];
	m_lstLastestApproachParam.push_back( params );

	// Keep the history count sane.
	if( m_lstLastestApproachParam.size() > 12 )
		m_lstLastestApproachParam.pop_front();

	gEnv->pAISystem->LogComment("<CScriptBind_AI> ", "%d valid pos", numValidPos );

	// Returns the calculated target position and direction.
	pNewTgtPos->SetValue( "x", attackPos[best].x );
	pNewTgtPos->SetValue( "y", attackPos[best].y );
	pNewTgtPos->SetValue( "z", attackPos[best].z );

	pNewTgtDir->SetValue( "x", attackDir[best].x );
	pNewTgtDir->SetValue( "y", attackDir[best].y );
	pNewTgtDir->SetValue( "z", attackDir[best].z );

	return pH->EndFunction( numValidPos );
}

int CScriptBind_AI::GetHunterApproachParams(IFunctionHandler * pH)
{
	// First param is the entity that is attacking, and the second param is the target location (script vector3).

	if(pH->GetParamCount() < 5)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetHunterApproachParams: Too few parameters." );
		return pH->EndFunction( 0 );
	}

	GET_ENTITY(1);

	if( !pEntity || !pEntity->GetAI() )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetHunterApproachParams: No Entity or AI." );
		return pH->EndFunction( 0 );
	}

	//retrieve the entity
	Vec3 targetPos( 0, 0, 0 );
	Vec3 targetDir( 0, 0, 0 );
	SmartScriptTable	pNewTgtPos;
	SmartScriptTable	pNewTgtDir;
	int	type = 0;

	if( !pH->GetParam( 2, type ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetHunterApproachParams: No type param." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 3, targetPos ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetHunterApproachParams: No target pos." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 4, targetDir ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetHunterApproachParams: No target dir." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 5, pNewTgtPos ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetHunterApproachParams: No out target pos." );
		return pH->EndFunction( 0 );
	}
	if( !pH->GetParam( 6, pNewTgtDir ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetHunterApproachParams: No out target dir." );
		return pH->EndFunction( 0 );
	}

	IAIObject* pAIObj = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if(!pAIActor)
		return pH->EndFunction(0);


	Vec3	entPos = pEntity->GetPos();
	Vec3	diff = entPos - targetPos;

	// Use target direction if available
	if( targetDir.GetLength() > 0.0001f )
		diff = -targetDir;

	float	angle = atan2( diff.y, diff.x );

	Vec3	attackPos[6];
	Vec3	attackDir[6];
	bool	bAttackSight[6] = { false, false, false, false, false, false };
	float	attackPosScore[6] = { 4, 8, 0, 8, 4, 0 };

	Vec3	bbox[2];
	bbox[0].Set( -3, -3, -3 );
	bbox[1].Set( 3, 3, 3 );

	AgentParameters	agentParams = pAIActor->GetParameters();

	float	attackHeight = agentParams.m_fAttackZoneHeight * 2.0f;
	float	attackRad = agentParams.m_fAttackRange * 0.25f;

	// Randomize the location a little.
	angle += (((cry_rand() * 10) / RAND_MAX) * 0.2f - 1.0f) * (gf_PI * 2.0f / 15.0f);
	attackHeight = (float)(cry_rand() / (float)RAND_MAX) * agentParams.m_fAttackZoneHeight * 0.25f;

	int	sightScore = 10;	// Is the target visible.
	int	heightScore = 2;	// Is the location near the attack height.
	int	blockingScore = 0;	// Is there an entity at the same location.
	int	randomScore = 4;	// Random offset.
	float	maxObstacleHeight = 0.0f;	// The maximum obstacle height above the terrain which is accepted.
	float	maxTargetObstacleHeight = 0.0f;	// The maximum obstacle height above the terrain at the target which is accepted.

	if( type == 0 )
	{
		// Attack position.
		attackPosScore[0] = 0;	// Enter direction.
		attackPosScore[1] = 8;
		attackPosScore[2] = 4;
		attackPosScore[3] = 0;	// Opposite direction.
		attackPosScore[4] = 4;
		attackPosScore[5] = 8;

		attackHeight += agentParams.m_fAttackZoneHeight * 2.5f;

		attackRad = agentParams.m_fAttackRange * (0.1f + 0.2f * (float)((cry_rand() * 4) / RAND_MAX) / 4.0f);

		sightScore = 10;	// Is the target visible.
		heightScore = 2;	// Is the location near the attack height.
		blockingScore = 0;	// Is there an entity at the same location.
		randomScore = 4;	// Random offset.

		maxObstacleHeight = 100;
		maxTargetObstacleHeight = 100;
	}
	else if( type == 1 )
	{
		// Attack position.
		attackPosScore[0] = 16;	// Enter direction.
		attackPosScore[1] = 4;
		attackPosScore[2] = 2;
		attackPosScore[3] = 0;	// Opposite direction.
		attackPosScore[4] = 2;
		attackPosScore[5] = 4;

		attackHeight += agentParams.m_fAttackZoneHeight * 1.5f;

		attackRad = agentParams.m_fAttackRange * 0.05f;

		sightScore = 10;	// Is the target visible.
		heightScore = 2;	// Is the location near the attack height.
		blockingScore = 0;	// Is there an entity at the same location.
		randomScore = 2;	// Random offset.

		// Require clear place for melee attacks
		maxObstacleHeight = 4.0f;
		maxTargetObstacleHeight = 1.5f;
	}

	int	best = 0;
	float bestScore = -1.f;
	int	numValidPos = 0;

	// Check the target height.
	/*	bool	bTooHighTargetObstacle = false;
	{
	float	targetRadius = 2.0f;
	// Make sure the point is above the terrain.
	float	terrainHeight = m_pSystem->GetI3DEngine()->GetTerrainElevation( targetPos.x, targetPos.y );
	float	maxObstHeight = terrainHeight;

	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

	IPhysicalEntity**	ppList = NULL;
	int	numEntites = pPhysWorld->GetEntitiesInBox( targetPos + Vec3( -1, -1, -1 ) * targetRadius, targetPos + Vec3( 1, 1, 1 ) * targetRadius, ppList, ent_static );
	for( int j = 0; j < numEntites; j++ )
	{
	pe_status_pos status;
	// Get bbox (status) from physics engine
	if(ppList[j]->GetType() == PE_LIVING)
	continue;
	ppList[j]->GetStatus( &status );

	float	height = status.pos.z + status.BBox[1].z; 

	if( height > maxObstHeight )
	maxObstHeight = height;
	}

	bTooHighTargetObstacle = (maxObstHeight - terrainHeight) > maxTargetObstacleHeight;
	}

	if( bTooHighTargetObstacle )
	{
	gEnv->pAISystem->LogComment("<CScriptBind_AI> ", "bTooHighTargetObstacle" );
	}*/

	for( int i = 0; i < 6; i++ )
	{
		// Calc new target position.
		angle += (float)i * (gf_PI * 2.0f / 6.0f);
		attackDir[i].Set( cos( angle ), sin( angle ), 0 );
		attackPos[i] = targetPos - attackDir[i] * attackRad;
		attackPos[i].z += attackHeight;

		// Make sure the point is above the terrain.
		/*		float	terrainHeight = m_pSystem->GetI3DEngine()->GetTerrainElevation( attackPos[i].x, attackPos[i].y );
		float	maxObstHeight = terrainHeight;

		IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

		IPhysicalEntity**	ppList = NULL;
		int	numEntites = pPhysWorld->GetEntitiesInBox( attackPos[i] + bbox[0], attackPos[i] + bbox[1], ppList, ent_static );
		for( int j = 0; j < numEntites; j++ )
		{
		pe_status_pos status;
		// Get bbox (status) from physics engine
		if(ppList[j]->GetType() == PE_LIVING)
		continue;
		ppList[j]->GetStatus( &status );

		float	height = status.pos.z + status.BBox[1].z; 

		if( height > maxObstHeight )
		{
		maxObstHeight = height;
		attackPosScore[i] += blockingScore;
		}
		}

		bool	bTooHighObstacle = (maxObstHeight - terrainHeight) > maxObstacleHeight;

		maxObstHeight += attackHeight;*/

		/*
		virtual GraphNode *GetEnclosing(const Vec3 &pos, int navCap, GraphNode *pStart = 0 ,bool bOutsideOnly = false,
		float range = 0.0f, Vec3 * closestValid = 0) = 0;
		*/

		Vec3	closestValid( attackPos[i] );
		bool	bValidPos = false;

		if( m_pAISystem->GetNodeGraph()->GetEnclosing( attackPos[i], IAISystem::NAV_FLIGHT, 0.0f, 0, 0.0f, &closestValid ) )
		{
			bValidPos = true;
			if( closestValid != attackPos[i] )
			{
				attackPos[i] = closestValid;
				attackPos[i].x += pAIActor->GetMovementAbility().minFlightHeight;
			}
		}

		//		if( attackPos[i].z < maxObstHeight )
		//			attackPos[i].z = maxObstHeight;

		// Recalc the attack direction.
		attackDir[i] = targetPos - attackPos[i];

		// Check if the target point can be seen from the new target pos.
		IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

		ray_hit	hit;
		int	numHits = RayWorldIntersectionWrapper( attackPos[i], attackDir[i], ent_static, rwi_stop_at_pierceable, &hit, 1 );
		bAttackSight[i] = numHits > 0 ? false : true;

		// Normalize the direction. the unnormalized version is needed for the ray intersection.
		attackDir[i].NormalizeSafe();

		if( bAttackSight[i] )
			attackPosScore[i] += sightScore;

		// Favor positions closer to the real attack range.
		if( fabs( targetPos.z - attackPos[i].z ) < attackHeight * 1.1f )
			attackPosScore[i] += heightScore;

		// Check if the point is too close to the old ones.
		for( std::list<SLastApproachParam>::iterator ptIt = m_lstLastestApproachParam.begin(); ptIt != m_lstLastestApproachParam.end(); ++ptIt )
		{
			SLastApproachParam&	param = (*ptIt);
			Vec3	diff = attackPos[i] - param.pos;
			if( diff.len2() < 10 * 10 )
				attackPosScore[i] += -30;
		}

		// Add small jitter into the selection.
		attackPosScore[i] += (cry_rand() * randomScore) / RAND_MAX;

		if( attackPosScore[i] > bestScore )
		{
			best = i;
			bestScore = attackPosScore[i];
		}

		//		if( bAttackSight[i] && !bTooHighObstacle && !bTooHighTargetObstacle )
		if( bAttackSight[i] && bValidPos )
			numValidPos++;
	}

	// Keep history of path points.
	SLastApproachParam	params;
	params.pos = attackPos[best];
	params.dir = attackDir[best];
	m_lstLastestApproachParam.push_back( params );

	// Keep the history count sane.
	if( m_lstLastestApproachParam.size() > 12 )
		m_lstLastestApproachParam.pop_front();

	gEnv->pAISystem->LogComment("<CScriptBind_AI> ", "%d valid pos", numValidPos );

	// Returns the calculated target position and direction.
	pNewTgtPos->SetValue( "x", attackPos[best].x );
	pNewTgtPos->SetValue( "y", attackPos[best].y );
	pNewTgtPos->SetValue( "z", attackPos[best].z );

	pNewTgtDir->SetValue( "x", attackDir[best].x );
	pNewTgtDir->SetValue( "y", attackDir[best].y );
	pNewTgtDir->SetValue( "z", attackDir[best].z );

	return pH->EndFunction( numValidPos );
}

int CScriptBind_AI::VerifyAlienTarget(IFunctionHandler * pH)
{
	// First param is the entity that is attacking, and the second param is the target location (script vector3).

	if(pH->GetParamCount() < 1)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAlienApproachParams: Too few parameters." );
		return pH->EndFunction( 0 );
	}

	//retrieve the entity
	Vec3 targetPos( 0, 0, 0 );

	if( !pH->GetParam( 1, targetPos ) )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "VerifyAlienTarget: No target pos." );
		return pH->EndFunction( 0 );
	}

	// Check the target height.

	float	targetRadius = 3.0f;
	// Make sure the point is above the terrain.
	float	terrainHeight = m_pSystem->GetI3DEngine()->GetTerrainElevation( targetPos.x, targetPos.y );
	float	maxObstHeight = terrainHeight;

	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

	IPhysicalEntity**	ppList = NULL;
	const float	checkRadius = 4.0f;
	int	numEntites = pPhysWorld->GetEntitiesInBox( targetPos + Vec3( -checkRadius, -checkRadius, -checkRadius ) * targetRadius, targetPos + Vec3( checkRadius, checkRadius, checkRadius ) * targetRadius, ppList, ent_static );
	for( int j = 0; j < numEntites; j++ )
	{
		pe_status_pos status;
		// Get bbox (status) from physics engine
		if(ppList[j]->GetType() == PE_LIVING)
			continue;
		ppList[j]->GetStatus( &status );

		float	height = status.pos.z + status.BBox[1].z; 

		if( height > maxObstHeight )
			maxObstHeight = height;
	}

	if( (maxObstHeight - terrainHeight) > 3.0f )
		return pH->EndFunction( 0 );

	return pH->EndFunction( 1 );
}


int CScriptBind_AI::GetAIObjectPosition(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	const char* objName = "";
	if(pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if (pEntity)
		{
			Vec3 vPos = pEntity->GetPos();
			return pH->EndFunction(vPos);
		}

	}
	else if(pH->GetParamType(1) == svtString)
	{
		pH->GetParam(1,objName);
		IAIObject* pAIObjectTarget = (IAIObject*) m_pAISystem->GetAIObjectByName(0,objName);
		if(pAIObjectTarget )
		{
			return pH->EndFunction(pAIObjectTarget->GetPos());
		}
	}
	return pH->EndFunction();
}

int CScriptBind_AI::GetBeaconPosition(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	SmartScriptTable	pBeaconPos;
	if( !pH->GetParam( 2, pBeaconPos ) )
		return pH->EndFunction(false);


	IAIObject* pAI = NULL;
	if(pH->GetParamType(1) == svtPointer)
	{
		//retrieve the entity
		ScriptHandle hdl;
		pH->GetParams(hdl);

		IEntity* pEntity = m_pSystem->GetIEntitySystem()->GetEntity(hdl.n);
		if (pEntity)
			pAI = pEntity->GetAI();
	}
	else if(pH->GetParamType(1) == svtString)
	{
		const char* objName = "";
		pH->GetParam(1, objName);
		pAI = m_pAISystem->GetAIObjectByName(0, objName);
	}

	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
	{
		IAIObject* pBeacon = m_pAISystem->GetBeacon(pAI->GetGroupId());
		if (pBeacon)
		{
			Vec3	pos = pBeacon->GetPos();
			pBeaconPos->SetValue( "x", pos.x );
			pBeaconPos->SetValue( "y", pos.y );
			pBeaconPos->SetValue( "z", pos.z );
			return pH->EndFunction(true);
		}
	}
	return pH->EndFunction(false);
}

int CScriptBind_AI::SetBeaconPosition(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	Vec3 vPos;
	if(!pH->GetParam(2,vPos))
		return pH->EndFunction();
	IAIObject* pAI = NULL;
	if(pH->GetParamType(1) == svtPointer)
	{
		//retrieve the entity
		ScriptHandle hdl;
		pH->GetParams(hdl);

		IEntity* pEntity = m_pSystem->GetIEntitySystem()->GetEntity(hdl.n);
		if (pEntity)
			pAI = pEntity->GetAI();
	}
	else if(pH->GetParamType(1) == svtString)
	{
		const char* objName = "";
		pH->GetParam(1, objName);
		pAI = m_pAISystem->GetAIObjectByName(0, objName);
	}

	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
		m_pAISystem->UpdateBeacon( pAI->GetGroupId(), vPos, pAI );

	return pH->EndFunction();
}


struct PairDistanceObject
{
	float fDistance;
	IAIObject* pObject;
};
bool operator < (const PairDistanceObject& left, const PairDistanceObject& right)
{
	return (left.fDistance < right.fDistance);
}

int CScriptBind_AI::GetNearestEntitiesOfType(IFunctionHandler * pH)
{
	SmartScriptTable pAIObjectScriptList; 
	int iObjectType;
	int iNumObjects;
	int iFilter = 0;
	float fRadius = 30;
	IAIObject *pAIObject= NULL;
	Vec3 vPos ;

	if(pH->GetParamCount() <4) 
		return pH->EndFunction();

	pH->GetParam(2,iObjectType);
	pH->GetParam(3,iNumObjects);
	pH->GetParam(4,pAIObjectScriptList);

	if(pH->GetParamCount() > 4) 
		pH->GetParam(5,iFilter);

	if(pH->GetParamCount() > 5) 
		pH->GetParam(6,fRadius);

	if (pH->GetParamType(1) == svtPointer)
	{
		GET_ENTITY(1);
		if(!pEntity)
			return pH->EndFunction(0);
		pAIObject = pEntity->GetAI();
		vPos = pAIObject->GetPos();
	}
	else if (pH->GetParamType(1) == svtString)
	{
		const char* sObjName;
		pH->GetParam(1,sObjName);
		pAIObject = (IAIObject*) m_pAISystem->GetAIObjectByName(0,sObjName);
		if(!pAIObject)
			return pH->EndFunction(0);
		vPos = pAIObject->GetPos();
	}
	else if (pH->GetParamType(1) == svtObject)
	{
		pH->GetParam(1,vPos);
		//		iFilter = 0; // can't have a species or a group if it's not relative to an AIObject
	}
	else // unrecognized 1st parameter
		return pH->EndFunction(0);



	float	fRadiusSQR = fRadius*fRadius;
	int iObjectsfound=0;

	typedef std::multimap<float,IAIObject*> AIObjectList_t;
	AIObjectList_t AIObjectList;

	IAIActor* pAIActor = CastToIAIActorSafe(pAIObject);

	for(AutoAIObjectIter it(m_pAISystem->GetFirstAIObject(IAISystem::OBJFILTER_TYPE, iObjectType)); it->GetObject(); it->Next())
	{
		IAIObject*	pObjectIt = it->GetObject();
		IAIActor* pAIActorIt = CastToIAIActorSafe(pObjectIt);
		if (pObjectIt == pAIObject || !pObjectIt->GetEntity() )	// skip AIObjects without an entity associated
			continue;

		if(!(iFilter & AIOBJECTFILTER_INCLUDEINACTIVE ))
		{
			if (!pObjectIt->IsEnabled()) 
				continue;
		}

		if(pAIActor && pAIActorIt && iFilter)
		{
			if((iFilter & AIOBJECTFILTER_SAMESPECIES) && 
				pAIActor->GetParameters().m_nSpecies != pAIActorIt->GetParameters().m_nSpecies)
				continue;
		}
		if((iFilter & AIOBJECTFILTER_SAMEGROUP) && pAIActorIt->GetParameters().m_nGroup!=AI_NOGROUP &&
			pAIObject->GetGroupId() != pObjectIt->GetGroupId())
			continue;
		if((iFilter & AIOBJECTFILTER_NOGROUP) && 
			pObjectIt->GetGroupId() != AI_NOGROUP)
			continue;

		Vec3 vObjectPos = pObjectIt->GetPos();
		float ds = (vObjectPos - vPos).len2();
		//float fActivationRadius = (ai->second)->GetRadius();
		//fActivationRadius*=fActivationRadius;
		if ( ds < fRadiusSQR) 
		{
			/*			if (pPuppet->m_mapDevaluedPoints.find(pObjectIt) == pPuppet->m_mapDevaluedPoints.end())
			{
			if ((fActivationRadius>0) && (ds>fActivationRadius))
			continue;
			}
			*/
			iObjectsfound++;
			AIObjectList.insert(std::make_pair(ds,pObjectIt));
			//for(int i=1; i< iObjectsfound; i++)


		}
	}

	AIObjectList_t::iterator iOL = AIObjectList.begin();

	int	i;
	for(i = 1; i<= iObjectsfound && i <= iNumObjects && iOL != AIObjectList.end(); i++)
	{
		IEntity* pEntity = (*iOL).second->GetEntity();
		if(pEntity)
			pAIObjectScriptList->SetAt(i,pEntity->GetScriptTable()); 
		++iOL;
	}
	iObjectsfound = i-1; //used as a return value

	for(; i<= iNumObjects; i++)
		pAIObjectScriptList->SetNullAt(i);

	return pH->EndFunction(iObjectsfound);

}
/*
int CScriptBind_AI::RequestSpeedVariation(IFunctionHandler * pH)
{

if(pH->GetParamCount() <2) 
return pH->EndFunction();
int nID;
float fVariation;
float fTime;
pH->GetParam(1,nID);
pH->GetParam(2,fVariation);
if(pH->GetParamCount() >2) 
pH->GetParam(3,fTime);
else
fTime = 0.00001f;//enough for just one frame

IEntity *pEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);
if (pEntity)
{
IAIObject* pAIObject = pEntity->GetAI();
if(pAIObject)
{
IUnknownProxy *pProxy = pAIObject->GetProxy();
if (pProxy)
{
IehicleProxy *pVehicleProxy =  0;
IPuppetProxy *pPuppetProxy =  0;
if (pProxy->QueryProxy(AIPROXY_VEHICLE,(void**) &pVehicleProxy))
pVehicleProxy->RequestSpeedVariation(fVariation, fTime);
else if (pProxy->QueryProxy(AIPROXY_PUPPET,(void**) &pPuppetProxy))
pPuppetProxy->RequestSpeedVariation(fVariation, fTime);
}
}
}
return pH->EndFunction();
}
*/

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::Event(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	const char *szEventText;
	int event;
	pH->GetParams(event, szEventText);

	m_pAISystem->Event( event, szEventText );
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetAttentionTargetOf(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();

	IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		if(pTarget)
			return pH->EndFunction(pTarget->GetName());
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetAttentionTargetType(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction(AIOBJECT_NONE);

	IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		if(pTarget)
			return pH->EndFunction(pTarget->GetAIType());
	}
	return pH->EndFunction(AIOBJECT_NONE);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetAttentionTargetEntity(IFunctionHandler * pH)
{
	if (pH->GetParamCount() < 1)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "GetAttentionTargetEntity: Not enough parameters." );
		return pH->EndFunction();
	}

	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();

	IPipeUser *pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		if(pTarget)
		{
			if(pTarget->CastToIAIActor())
			{
				IEntity* pTargetEntity = pTarget->GetEntity();
				if(pTargetEntity)
					return pH->EndFunction(pTargetEntity->GetScriptTable());
			}
			else
			{
				bool bLiveTargetOnly=false;
				if (pH->GetParamCount() >= 2)
					pH->GetParam(2,bLiveTargetOnly);

				if (!bLiveTargetOnly && pTarget->GetAIType()==AIOBJECT_DUMMY) 
				{
					// return the owner entity of the target AI Object(memory, sound etc) if there is
					IPuppet* pPuppet = pEntity->GetAI()->CastToIPuppet();
					if (pPuppet)
					{
						pTarget = pPuppet->GetEventOwner(pTarget);
						if(pTarget && pTarget->CastToIAIActor())
						{
							IEntity* pTargetEntity = pTarget->GetEntity();
							if(pTargetEntity)
								return pH->EndFunction(pTargetEntity->GetScriptTable());
						}
					}
				}
			}
		}			
	}
	return pH->EndFunction();
}
//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAttentionTargetPosition(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	SmartScriptTable	pTgtPos;
	if( !pH->GetParam( 2, pTgtPos ) )
		return pH->EndFunction();

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		if(pTarget)
		{
			Vec3	pos = pTarget->GetPos();
			pTgtPos->SetValue( "x", pos.x );
			pTgtPos->SetValue( "y", pos.y );
			pTgtPos->SetValue( "z", pos.z );
			return pH->EndFunction(true);
		}
	}
	return pH->EndFunction(false);
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAttentionTargetDistance(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		if(pTarget)
		{
			return pH->EndFunction((pTarget->GetPos()- pEntity->GetPos()).GetLength());
		}
	}
	return pH->EndFunction();
}


//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAttentionTargetDirection(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	SmartScriptTable	pTgtPos;
	if( !pH->GetParam( 2, pTgtPos ) )
		return pH->EndFunction();

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		if(pTarget)
		{
			Vec3	dir = pTarget->GetMoveDir();
			dir.NormalizeSafe();
			pTgtPos->SetValue( "x", dir.x );
			pTgtPos->SetValue( "y", dir.y );
			pTgtPos->SetValue( "z", dir.z );
		}
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetAttentionTargetViewDirection(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	SmartScriptTable	pTgtPos;
	if( !pH->GetParam( 2, pTgtPos ) )
		return pH->EndFunction();

	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		if(pTarget)
		{
			Vec3	dir = pTarget->GetViewDir();
			dir.NormalizeSafe();
			pTgtPos->SetValue( "x", dir.x );
			pTgtPos->SetValue( "y", dir.y );
			pTgtPos->SetValue( "z", dir.z );
		}
	}
	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::CreateFormation(IFunctionHandler * pH)
{
	if (pH->GetParamCount() < 1)
		return pH->EndFunction();

	const char *name;

	if (pH->GetParam(1,name))
	{
		FormationNode nodeDescr;
		m_pAISystem->CreateFormationDescriptor(name);
		if (pH->GetParamCount() > 1)
			pH->GetParam(2,nodeDescr.eClass);
		// create the owner's node at 0,0,0
		m_pAISystem->AddFormationPoint(name,nodeDescr);
	}
	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddFormationPointFixed(IFunctionHandler * pH)
{
	FormationNode nodeDescr;
	float fSight=0;
	const char *name;
	if (pH->GetParamCount() < 5)
		return pH->EndFunction();
	if (!pH->GetParams(name,fSight,nodeDescr.vOffset.x, nodeDescr.vOffset.y, nodeDescr.vOffset.z))
		return pH->EndFunction();
	if(pH->GetParamCount()>5)
		pH->GetParam(6,nodeDescr.eClass);

	Matrix33 m = Matrix33::CreateRotationXYZ( DEG2RAD(Ang3(0,0,fSight)) ); 
	nodeDescr.vSightDirection = m * Vec3(0,60,0); //put the sight target far away in order to not make change 
	// head orientation too much while approaching

	m_pAISystem->AddFormationPoint(name,nodeDescr);

	return pH->EndFunction();

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddFormationPoint(IFunctionHandler * pH)
{
	FormationNode	nodeDescr;
	float fSight;
	const char *name;

	if (pH->GetParamCount() < 4)
		return pH->EndFunction();
	if (!pH->GetParams(name,fSight,nodeDescr.fFollowDistance,nodeDescr.fFollowOffset))
		return pH->EndFunction();
	if(pH->GetParamCount()>4)
		pH->GetParam(5,nodeDescr.eClass);
	if(pH->GetParamCount()>5)
	{
		pH->GetParam(6,nodeDescr.fFollowDistanceAlternate);
		pH->GetParam(7,nodeDescr.fFollowOffsetAlternate);
	}
	if (pH->GetParamCount()>7)
		pH->GetParam(8,nodeDescr.fFollowHeightOffset);
	if(nodeDescr.fFollowDistanceAlternate==0)
	{
		nodeDescr.fFollowDistanceAlternate = nodeDescr.fFollowDistance;
		nodeDescr.fFollowOffsetAlternate = nodeDescr.fFollowOffset;
	}

	Matrix33 m = Matrix33::CreateRotationXYZ( DEG2RAD(Ang3(0,0,fSight)) ); 
	nodeDescr.vSightDirection = m * Vec3(0,60,0); //put the sight target far away in order to not make change 
	// head orientation too much while approaching

	m_pAISystem->AddFormationPoint(name, nodeDescr);

	return pH->EndFunction();
}


//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetFormationPointClass(IFunctionHandler * pH)
{
	FormationNode	nodeDescr;
	int pos;
	const char* name;
	SCRIPT_CHECK_PARAMETERS(2);
	if (!pH->GetParams(name,pos))
		return pH->EndFunction(-1);

	return pH->EndFunction(m_pAISystem->GetFormationPointClass(name,pos));

}


//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::BeginTrackPattern(IFunctionHandler * pH)
{
	const char* patternName = 0;
	int					flags = 0;
	float				validationRadius = 0;
	float				advanceRadius = 1;
	float				globalDefromTreshold = 0.0;
	float				localDefromTreshold = 0.0;
	float				stateTresholdMin = 0.35f;
	float				stateTresholdMax = 0.4f;
	float				exposureMod = 0;
	Ang3				angles( 0, 0, 0 );

	if( m_pActiveTrackPatternDesc )
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "Calling CreateTrackPattern without finishing the previous track pattern with EndTrackPattern." );
		m_pActiveTrackPatternDesc = 0;
	}

	if (pH->GetParamCount() < 3)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CreateTrackPattern: Not enough parameters." );
		return pH->EndFunction();
	}

	// The required parameters.
	if( !pH->GetParams( patternName, flags, validationRadius, advanceRadius ) )
		return pH->EndFunction();

	// Optional - state treshold min.
	if( pH->GetParamCount() > 4 )
		pH->GetParam( 5, stateTresholdMin );
	// Optional - state treshold max.
	if( pH->GetParamCount() > 5 )
		pH->GetParam( 6, stateTresholdMax );
	// Optional - global deformation value treshold.
	if( pH->GetParamCount() > 6 )
		pH->GetParam( 7, globalDefromTreshold );
	// Optional - local deformation value treshold.
	if( pH->GetParamCount() > 7 )
		pH->GetParam( 8, localDefromTreshold );
	// Optional - exposure modifier.
	if( pH->GetParamCount() > 8 )
		pH->GetParam( 9, exposureMod );
	// Optional - random rotation angles.
	if( pH->GetParamCount() > 9 )
		pH->GetParam( 10, angles );

	m_pActiveTrackPatternDesc = m_pAISystem->CreateTrackPatternDescriptor( patternName );
	if( !m_pActiveTrackPatternDesc )
	{
		gEnv->pAISystem->Error("<CScriptBind_AI> ", "CreateTrackPattern: AISystem->CreateTrackPatternDescriptor returned NULL." );
		return pH->EndFunction();
	}

	m_pActiveTrackPatternDesc->SetFlags( flags );
	m_pActiveTrackPatternDesc->SetValidationRadius( validationRadius );
	m_pActiveTrackPatternDesc->SetAdvanceRadius( advanceRadius );
	m_pActiveTrackPatternDesc->SetDeformationTreshold( globalDefromTreshold, localDefromTreshold );
	m_pActiveTrackPatternDesc->SetStateTreshold( stateTresholdMin, stateTresholdMax );
	m_pActiveTrackPatternDesc->SetExposureMod( exposureMod );
	m_pActiveTrackPatternDesc->SetRandomRotation( Ang3( angles.x, angles.y, angles.z ) );

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddPatternNode(IFunctionHandler * pH)
{
	if( !m_pActiveTrackPatternDesc )
	{
		gEnv->pAISystem->Error("<CScriptBind_AI> ", "Calling AddPatternNode without starting a pattern using BeginTrackPattern." );
		m_pActiveTrackPatternDesc = 0;
		return pH->EndFunction();
	}

	const char* nodeName = 0;
	const char* parentName = 0;
	Vec3				offset( 0, 0, 0 );
	int					flags = 0;
	int					signalValue = 0;

	if (pH->GetParamCount() < 5)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "AddPatternNode: Not enough parameters." );
		return pH->EndFunction();
	}

	// The required parameters.
	if( !pH->GetParams( nodeName, offset.x, offset.y, offset.z, flags ) )
		return pH->EndFunction();

	// Optional - parent name.
	if( pH->GetParamCount() > 5 )
		pH->GetParam( 6, parentName );

	// Optional - signal value.
	if( pH->GetParamCount() > 6 )
		pH->GetParam( 7, signalValue );

	m_pActiveTrackPatternDesc->AddNode( nodeName, offset, flags, parentName, signalValue );

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddPatternBranch(IFunctionHandler * pH)
{
	if( !m_pActiveTrackPatternDesc )
	{
		gEnv->pAISystem->Error("<CScriptBind_AI> ", "Calling AddPatternPoint without starting a pattern using BeginTrackPattern." );
		m_pActiveTrackPatternDesc = 0;
		return pH->EndFunction();
	}

	const char* nodeName = 0;
	const char* targetName = 0;
	int	method = AITRACKPAT_CHOOSE_ALWAYS;
	std::list<string>	branchNames;

	if (pH->GetParamCount() < 3)
	{
		gEnv->pAISystem->Error("<CScriptBind_AI> ", "AddPatternSeqBranch: Not enough parameters." );
		return pH->EndFunction();
	}

	// The required parameters.
	if( !pH->GetParams( nodeName, method ) )
		return pH->EndFunction();

	for( int i = 3; i <= pH->GetParamCount(); i++ )
	{
		const char*	targetName;
		if( pH->GetParam( i, targetName ) )
			branchNames.push_back( targetName );
	}

	ITrackPatternDescriptorNode*	node = m_pActiveTrackPatternDesc->GetNode( nodeName );
	if( node )
	{
		// Copy the sequence to the node.
		node->SetBranchMethod( method );
		for( std::list<string>::iterator iter = branchNames.begin(); iter != branchNames.end(); ++iter )
			node->AddBranchNodeName( (*iter) );
	}
	else
	{
		// The node could not be found, dump the sequence
		string	errMsg( "" );
		for( std::list<string>::const_iterator iter = branchNames.begin(); iter != branchNames.end(); ++iter )
		{
			if( iter != branchNames.end() )
				errMsg += ", ";
			errMsg += (*iter);
		}
		gEnv->pAISystem->Error("<CScriptBind_AI> ", "Track pattern '%s': Could not find node '%s' while trying to create sequence to %s", m_pActiveTrackPatternDesc->GetName(), nodeName, errMsg.c_str() );
	}

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::EndTrackPattern(IFunctionHandler * pH)
{
	// Finalise the crete track pattern.
	if( !m_pActiveTrackPatternDesc )
	{
		gEnv->pAISystem->Error("<CScriptBind_AI> ", "Calling EndTrackPattern without matching BeginTrackPattern." );
		return pH->EndFunction();
	}

	// Finished with this descriptor.
	m_pActiveTrackPatternDesc->Finalize();
	m_pActiveTrackPatternDesc = 0;

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetLeader(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	IAIObject* pAILeader = NULL;
	if(pH->GetParamType(1)==svtNumber)
	{
		int iGroupId;
		pH->GetParam(1,iGroupId);
		pAILeader = m_pAISystem->GetLeaderAIObject(iGroupId);
	}
	else if(pH->GetParamType(1)==svtPointer)
	{
		GET_ENTITY(1);
		if(pEntity)
		{
			IAIObject* pObject = pEntity->GetAI();
			if(pObject)
				pAILeader = m_pAISystem->GetLeaderAIObject(pObject);
		}
	}
	if(pAILeader)
	{
		IEntity* pLeaderEntity = pAILeader->GetEntity();
		if(pLeaderEntity)
			return pH->EndFunction(pLeaderEntity->GetScriptTable());
	}
	return pH->EndFunction();
}



int CScriptBind_AI::UpTargetPriority(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if (!pEntity || !pEntity->GetAI())
		return pH->EndFunction();

	pH->GetParam(2,hdl);
	nID = hdl.n;
	IEntity* pOtherEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);
	if (!pOtherEntity || !pOtherEntity->GetAI())
		return pH->EndFunction();
	float increment = 0;
	pH->GetParam(3,increment);
	IPuppet *pPuppet = CastToIPuppetSafe(pEntity->GetAI());
	if (pPuppet)
	{
		pPuppet->UpTargetPriority(pOtherEntity->GetAI(), increment);
	}
	return pH->EndFunction();
}

int CScriptBind_AI::SetExtraPriority(IFunctionHandler * pH)
{

	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction(false);

	IPipeUser* pPiper = CastToIPipeUserSafe(pEntity->GetAI());
	if(!pPiper)
		return pH->EndFunction(false);

	float priority = 0.0f;
	pH->GetParam(2,priority);

	pPiper->SetExtraPriority( priority );

	return pH->EndFunction(true);

}

int CScriptBind_AI::GetExtraPriority(IFunctionHandler * pH)
{

	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction(false);

	IPipeUser* pPiper = CastToIPipeUserSafe(pEntity->GetAI());
	if(!pPiper)
		return pH->EndFunction(false);

	return pH->EndFunction(pPiper->GetExtraPriority());

}

int CScriptBind_AI::GetPlayerThreatLevel(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	SAIDetectionLevels levels;
	gEnv->pAISystem->GetDetectionLevels(0, levels);
	return pH->EndFunction(max(levels.vehicleThreat, levels.puppetThreat));
}

int CScriptBind_AI::GetUnitInRank(IFunctionHandler * pH)
{
	if(pH->GetParamCount()==0)
	{
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if(pAIActor)
	{
		int groupId = pAI->GetGroupId();
		IAIGroup* pGroup = m_pAISystem->GetIAIGroup(groupId);
		/* TO DO: fix this problem with ILeader interface
		if(pLeader )
		{
		int rank = 0;
		if(pH->GetParamCount()>1)
		pH->GetParam(2,rank);
		IAIObject* pAIRanked = pLeader->GetUnitInRank(rank);
		if(pAIRanked)
		{
		IEntity* pRankedEntity = static_cast<IEntity*>(pAIRanked->GetAssociation());
		if(pRankedEntity)
		return pH->EndFunction(pRankedEntity->GetScriptTable());
		}
		}
		*/	}
	return pH->EndFunction();

}

int CScriptBind_AI::SetUnitProperties(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	int properties = 0;
	pH->GetParam(2,properties);
	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if(pAIActor)
	{
		int groupId = pAI->GetGroupId();
		IAIGroup* pGroup = m_pAISystem->GetIAIGroup(groupId);
		if( pGroup )
			pGroup->SetUnitProperties( pAI, properties );
	}
	return pH->EndFunction();
}

int CScriptBind_AI::GetUnitCount(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();
	int properties = 0;
	pH->GetParam(2,properties);
	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if(pAIActor)
	{
		int groupId = pAI->GetGroupId();
		IAIGroup* pGroup = m_pAISystem->GetIAIGroup(groupId);
		if( pGroup )
			return pH->EndFunction(pGroup->GetUnitCount(properties));
	}
	return pH->EndFunction();
}

int CScriptBind_AI::SetSpeciesThreatMultiplier(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (pEntity )
	{
		IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
		if(pAIActor)
		{
			int nSpecies = pAIActor->GetParameters().m_nSpecies;

			// Avoid bugs due to accidental setting of player species threat multiplier. Must use SetPlayerSpeciesThreatMultiplier for this.
			if(0!=nSpecies)
			{
				float fMultiplier;
				pH->GetParam(2,fMultiplier);
				m_pAISystem->SetSpeciesThreatMultiplier(nSpecies,fMultiplier);
			}
			else
			{
				gEnv->pAISystem->Error("<CScriptBind_AI> ", "SetSpeciesThreatMultiplier: Species 0 not allowed (Entity '%s'). Use SetPlayerSpeciesThreatMultiplier if this is really needed.",pEntity->GetName());
			}
		}
		else
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "SetSpeciesThreatMultiplier: Entity %s has no AIObject associated",pEntity->GetName());
	}
	else
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "SetSpeciesThreatMultiplier: wrong Entity id passed.");
	return pH->EndFunction( );
}

int CScriptBind_AI::SetPlayerSpeciesThreatMultiplier(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	int nSpecies = 0;
	float fMultiplier;
	pH->GetParam(1,fMultiplier);
	m_pAISystem->SetSpeciesThreatMultiplier(nSpecies,fMultiplier);

	return pH->EndFunction( );
}

/*! Makes an entity a leader of the group
@return 1 if succeeded
*/
int CScriptBind_AI::SetLeader(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	IAIObject* pLeader = NULL;
	if(pEntity)
	{
		IAIObject* pAIObject = pEntity->GetAI();
		if (pAIObject)
		{
			IAIObject *pOldLeader = m_pAISystem->GetLeaderAIObject(pAIObject);
			if(pOldLeader!=pAIObject)
			{
				if(pOldLeader)
					m_pAISystem->SendSignal(0,0,"OnLeaderDeassigned",pOldLeader);
				m_pAISystem->SetLeader(pAIObject);
				m_pAISystem->SendSignal(0,0,"OnLeaderAssigned",pAIObject);
			}
		}
	}
	return pH->EndFunction(pLeader != NULL);
}


int CScriptBind_AI::GetStance(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	IAIActor* pAIActor;
	if(!pEntity || !(pAIActor = CastToIAIActorSafe(pEntity->GetAI())))
		return pH->EndFunction();
	SOBJECTSTATE* pState = pAIActor->GetState();
	int stance = (pState ? pState->bodystate :  BODYPOS_RELAX);
	return pH->EndFunction(stance);
}


int CScriptBind_AI::SetStance(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	IAIActor* pAIActor;
	if(!pEntity || !(pAIActor = CastToIAIActorSafe(pEntity->GetAI())))
		return pH->EndFunction();
	
	SOBJECTSTATE* pState = pAIActor->GetState();
	
	int stance;
	if(pState && pH->GetParam(2,stance))
		pState->bodystate = stance;
	
	return pH->EndFunction();
}


int CScriptBind_AI::SetPFProperties(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if(!pAIActor)
		return pH->EndFunction();

	int pathType(AIPATH_DEFAULT);
	pH->GetParam(2,pathType);
	AgentMovementAbility	mov = pAIActor->GetMovementAbility();
	SetPFProperties(mov, pathType);
	pAIActor->SetMovementAbility(mov);
	return pH->EndFunction();
}

int CScriptBind_AI::GetAlertStatus(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();
	/* TO DO: fix this problem with ILeader interface
	ILeader* pLeader = m_pAISystem->GetILeader(pAIObject->GetParameters().m_nGroup);
	if(pLeader)
	return pH->EndFunction( pLeader->GetAlertStatus());
	*/
	int alert = m_pAISystem->GetAlertStatus(pAIObject);
	if(alert <0)
		return pH->EndFunction();
	else
		return pH->EndFunction(alert);
}


int CScriptBind_AI::GetTargetType(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction(AITARGET_NONE);
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction(AITARGET_NONE);
	IPipeUser* pPiper = pAIObject->CastToIPipeUser();
	if(pPiper)
	{
		IAIObject* pTarget = pPiper->GetAttentionTarget();
		if(pTarget)
		{
			switch(pTarget->GetAIType())
			{
			case AIOBJECT_PUPPET:
			case AIOBJECT_PLAYER:
			case AIOBJECT_VEHICLE:
				if(pAIObject->IsHostile(pTarget))
					return pH->EndFunction(AITARGET_ENEMY);
				else
					return pH->EndFunction(AITARGET_FRIENDLY);
				break;
			case AIOBJECT_DUMMY:
				switch(pTarget->GetSubType())
				{
				case IAIObject::STP_MEMORY:
					return pH->EndFunction(AITARGET_MEMORY);
					break;
				case IAIObject::STP_BEACON:
					return pH->EndFunction(AITARGET_BEACON);
					break;
				case IAIObject::STP_SOUND:
					return pH->EndFunction(AITARGET_SOUND);
					break;
				}
				break;
			case AIOBJECT_GRENADE:
				return pH->EndFunction(AITARGET_GRENADE);

			case AIOBJECT_RPG:
				return pH->EndFunction(AITARGET_RPG);

			default:
				return pH->EndFunction(AITARGET_NONE);
				break;
			}
		}
	}
	return pH->EndFunction(AITARGET_NONE);
}

int CScriptBind_AI::ChangeFormation(IFunctionHandler *pH)
{
	if(pH->GetParamCount()<2)
		return pH->EndFunction();
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();
	const char* szDescriptor;
	pH->GetParam(2,szDescriptor);

	float fScale = 1;
	if(pH->GetParamCount()>2)
		pH->GetParam(3,fScale);
	bool bForceCreate = false;
	if(pH->GetParamCount()>3)
		pH->GetParam(4,bForceCreate);

	bool bSuccessful = m_pAISystem->ChangeFormation(pAIObject/*->GetParameters().m_nGroup*/,szDescriptor,fScale, bForceCreate);

	return pH->EndFunction(bSuccessful);

}

int CScriptBind_AI::ScaleFormation(IFunctionHandler *pH)
{
	if(pH->GetParamCount()<1)
		return pH->EndFunction();
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();

	float fScale = 1;
	if(pH->GetParamCount()>1)
		pH->GetParam(2,fScale);

	bool bSuccessful = m_pAISystem->ScaleFormation(pAIObject, fScale);

	return pH->EndFunction(bSuccessful);

}

int CScriptBind_AI::SetFormationUpdate(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);	
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();

	bool bUpdate;
	pH->GetParam(2,bUpdate);

	bool bSuccessful = m_pAISystem->SetFormationUpdate(pAIObject, bUpdate);

	return pH->EndFunction(bSuccessful);

}

int CScriptBind_AI::RequestAttack(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS_MIN(3);	
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if(pAIActor)
		return pH->EndFunction();

	ILeader* pLeader = m_pAISystem->GetILeader(pAI->GetGroupId());
	if(!pLeader)
		return pH->EndFunction();

	uint32 properties;
	pH->GetParam(2,properties);

	int actionType;

	pH->GetParam(3,actionType);

	float fDuration =0;
	Vec3 defensePoint(0,0,0);

	if(pH->GetParamCount()>3)
		pH->GetParam(4,fDuration);
	if(pH->GetParamCount()>4)
		pH->GetParam(5,defensePoint);
	pLeader->RequestAttack(properties,actionType,fDuration,defensePoint);
	//m_pAISystem->RequestGroupAttack(pAIObject->GetParameters().m_nGroup, properties,actionType, fDuration);
	return pH->EndFunction();

}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_AI::ChangeParameter(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();

	int nParameter;
	float fValue;
	const char *name;
	pH->GetParam(2,nParameter);

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
	{
		AgentParameters ap = pAIActor->GetParameters();

		if( pH->GetParamType( 3 ) == svtNumber )
		{
			pH->GetParam(3,fValue);

			switch (nParameter)
			{
			case AIPARAM_SIGHTRANGE:
				ap.m_PerceptionParams.sightRange = fValue;
				break;
			case AIPARAM_ATTACKRANGE:
				ap.m_fAttackRange = fValue;
				break;
			case AIPARAM_ACCURACY:
				ap.m_fAccuracy = fValue;
				break;
			case AIPARAM_GROUPID:
				ap.m_nGroup = (int) fValue;
				break;
			case AIPARAM_FOVPRIMARY:
				ap.m_PerceptionParams.FOVPrimary = fValue;
				break;
			case AIPARAM_FOVSECONDARY:
				ap.m_PerceptionParams.FOVSecondary = fValue;
				break;
			case AIPARAM_COMMRANGE:
				ap.m_fCommRange = fValue;
				break;
			case AIPARAM_SPECIES:
				ap.m_nSpecies = (int) fValue;
				break;
			case AIPARAM_SPECIESHOSILITY:
				ap.m_bSpeciesHostility  = fValue!=0.f;
				break;
			case AIPARAM_RANK:
				ap.m_nRank = (int) fValue;
				break;
			case AIPARAM_CAMOSCALE:
				ap.m_PerceptionParams.camoScale = fValue;
				break;
			case AIPARAM_HEATSCALE:
				ap.m_PerceptionParams.heatScale = fValue;
				break;
			case AIPARAM_TRACKPATADVANCE:
				if( fValue > 0 )
					ap.m_trackPatternContinue = 1;
				else
					ap.m_trackPatternContinue = 2;
				break;
			case AIPARAM_STRAFINGPITCH:
				ap.m_fStrafingPitch = fValue;
				break;
			case AIPARAM_COMBATCLASS:
				ap.m_CombatClass = (int) fValue;
				break;
			case AIPARAM_INVISIBLE:
				ap.m_bInvisible = fValue > 0.01f;
				break;
			case AIPARAM_PERCEPTIONSCALE_VISUAL:
				ap.m_PerceptionParams.perceptionScale.visual = fValue;
				break;
			case AIPARAM_PERCEPTIONSCALE_AUDIO:
				ap.m_PerceptionParams.perceptionScale.audio = fValue;
				break;
			case AIPARAM_CLOAK_SCALE:
				ap.m_fCloakScaleTarget = fValue;
				break;
			case AIPARAM_FORGETTIME_TARGET:
				ap.m_PerceptionParams.forgetfulnessTarget = fValue;
				break;
			case AIPARAM_FORGETTIME_SEEK:
				ap.m_PerceptionParams.forgetfulnessSeek = fValue;
				break;
			case AIPARAM_FORGETTIME_MEMORY:
				ap.m_PerceptionParams.forgetfulnessMemory = fValue;
				break;
			case AIPARAM_LOOKIDLE_TURNSPEED:
				ap.m_lookIdleTurnSpeed = DEG2RAD(fValue);
				break;
			case AIPARAM_LOOKCOMBAT_TURNSPEED:
				ap.m_lookCombatTurnSpeed = DEG2RAD(fValue);
				break;
			case AIPARAM_AIM_TURNSPEED:
				ap.m_aimTurnSpeed = DEG2RAD(fValue);
				break;
			case AIPARAM_FIRE_TURNSPEED:
				ap.m_fireTurnSpeed = DEG2RAD(fValue);
				break;
			case AIPARAM_MELEE_DISTANCE:
				ap.m_fMeleeDistance = fValue;
				break;
			case AIPARAM_MIN_ALARM_LEVEL:
				ap.m_PerceptionParams.minAlarmLevel = fValue;
				break;
			case AIPARAM_SIGHTENVSCALE_NORMAL:
				ap.m_PerceptionParams.sightEnvScaleNormal = fValue;
				break;
			case AIPARAM_SIGHTENVSCALE_ALARMED:
				ap.m_PerceptionParams.sightEnvScaleAlarmed = fValue;
				break;
			case AIPARAM_GRENADE_THROWDIST:
				ap.m_fGrenadeThrowDistScale = fValue;
				break;
			}
		}
		else if( pH->GetParamType( 3 ) == svtString )
		{
			pH->GetParam(3,name);

			switch (nParameter)
			{
			case AIPARAM_TRACKPATNAME:
				ap.m_trackPatternName = name;
				break;
				// more to come as needed
			}
		}
		pAIActor->SetParameters(ap);

		if (pAI->CastToIAIVehicle())
		{
			pH->GetParam(3,fValue);

			switch (nParameter)
			{
			case AIPARAM_FWDSPEED:
				IAIVehicleProxy *proxy=NULL;
				if(pAIActor->GetProxy()->QueryProxy(AIPROXY_VEHICLE, (void**)&proxy))
				{
					proxy->SetSpeeds( fValue, -1 );
				}
				return pH->EndFunction();
				// more to come as needed
			}	
		}
	}
	else if (pAI)
	{
		if( pH->GetParamType( 3 ) == svtNumber )
		{
			pH->GetParam(3,fValue);
			if (nParameter == AIPARAM_GROUPID)
				pAI->SetGroupId((int)fValue);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_AI::GetAIParameter(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if(!pEntity)
		return pH->EndFunction();

	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();

	int nParameter;
	pH->GetParam(2,nParameter);

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (pAIActor)
	{
		AgentParameters ap = pAIActor->GetParameters();

		switch (nParameter)
		{
		case AIPARAM_PERCEPTIONSCALE_VISUAL:
			return pH->EndFunction(ap.m_PerceptionParams.perceptionScale.visual);
			break;
		case AIPARAM_PERCEPTIONSCALE_AUDIO:
			return pH->EndFunction(ap.m_PerceptionParams.perceptionScale.audio);
			break;
		case AIPARAM_LOOKIDLE_TURNSPEED:
			return pH->EndFunction(RAD2DEG(ap.m_lookIdleTurnSpeed));
			break;
		case AIPARAM_LOOKCOMBAT_TURNSPEED:
			return pH->EndFunction(RAD2DEG(ap.m_lookCombatTurnSpeed));
			break;
		case AIPARAM_AIM_TURNSPEED:
			return pH->EndFunction(RAD2DEG(ap.m_aimTurnSpeed));
			break;
		case AIPARAM_FIRE_TURNSPEED:
			return pH->EndFunction(RAD2DEG(ap.m_fireTurnSpeed));
			break;

		default:
			return pH->EndFunction();
			break;
		}
	}

	return pH->EndFunction();

}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_AI::ChangeMovementAbility(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();

	int nParameter;
	float fValue;
	pH->GetParam(2,nParameter);

	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (pAIActor)
	{
		AgentMovementAbility	mov = pAIActor->GetMovementAbility();

		if( pH->GetParamType( 3 ) == svtNumber )
		{
			pH->GetParam(3,fValue);

			switch (nParameter)
			{
			case AIMOVEABILITY_OPTIMALFLIGHTHEIGHT:
				mov.optimalFlightHeight = fValue;
				break;
			case AIMOVEABILITY_MINFLIGHTHEIGHT:
				mov.minFlightHeight = fValue;
				break;
			case AIMOVEABILITY_MAXFLIGHTHEIGHT:
				mov.maxFlightHeight = fValue;
				break;
			case AIMOVEABILITY_TELEPORTENABLE:
				mov.teleportEnabled = fValue != 0.0f;
				break;
			case AIMOVEABILITY_USEPREDICTIVEFOLLOWING:
				mov.usePredictiveFollowing = fValue != 0;
				break;
			default:
				m_pAISystem->Error("<Lua> ", "ChangeMovementAbility: unhandled param %d", nParameter);
				break;
			}
		}
		pAIActor->SetMovementAbility( mov );
	}

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------

int CScriptBind_AI::GetForwardDir(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	SmartScriptTable	pTgtPos;
	if( !pH->GetParam( 2, pTgtPos ) )
		return pH->EndFunction();

	IAIObject* pObject = pEntity->GetAI();
	IPipeUser* pPipeUser = 0;
	if (pObject )
	{
		Vec3	dir = pObject->GetMoveDir();
		dir.NormalizeSafe();
		pTgtPos->SetValue( "x", dir.x );
		pTgtPos->SetValue( "y", dir.y );
		pTgtPos->SetValue( "z", dir.z );
	}
	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetNavigationType(IFunctionHandler * pH)
{
	if(pH->GetParamCount()<1)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetNavigationType(): wrong number of parameters passed");
		return pH->EndFunction();
	}
	IVisArea *pGoalArea; 
	int nBuilding;
	if(pH->GetParamType(1)==svtPointer)
	{
		GET_ENTITY(1);
		if(!pEntity)
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetNavigationType(): wrong type of parameter 1");
			return pH->EndFunction();
		}
		IAIObject* pAIObject = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pAIObject);
		if(!pAIActor)
			return pH->EndFunction();
		return pH->EndFunction(m_pAISystem->CheckNavigationType(pAIObject->GetPos(),nBuilding,pGoalArea,pAIActor->GetMovementAbility().pathfindingProperties.navCapMask)) ;
	}
	else if(pH->GetParamType(1)==svtNumber)
	{
		int groupId = -1;
		pH->GetParam(1,groupId);
		uint32 unitProp = UPR_ALL;
		if(pH->GetParamCount()>1 && pH->GetParamType(2)==svtNumber)
			pH->GetParam(2,unitProp);

		typedef std::map<int,int> TmapIntInt;
		TmapIntInt NavTypeCount;

		for(int i=1;i<= IAISystem::NAV_MAX_VALUE; i<<=1)
			NavTypeCount.insert(std::make_pair(i,0));

		int numAgents = m_pAISystem->GetGroupCount(groupId, IAISystem::GROUP_ENABLED);
		for(int i=0; i<numAgents; i++)
		{
			IAIObject* pAIObject = m_pAISystem->GetGroupMember(groupId, i, IAISystem::GROUP_ENABLED);
			IAIActor* pAIActor = CastToIAIActorSafe(pAIObject);
			if(pAIActor)
			{
				int navType = m_pAISystem->CheckNavigationType(pAIObject->GetPos(),nBuilding,pGoalArea,pAIActor->GetMovementAbility().pathfindingProperties.navCapMask);
				if(NavTypeCount.find(navType)!=NavTypeCount.end())
					NavTypeCount[navType] = NavTypeCount[navType]+1;
			}
		}
		int maxNavigationTypeCount = 0;
		int navTypeSelected = IAISystem::NAV_UNSET;
		for(TmapIntInt::iterator it = NavTypeCount.begin(); it!=NavTypeCount.end();++it)
		{
			int navCount = it->second;
			if(navCount>maxNavigationTypeCount)
			{
				navCount = maxNavigationTypeCount;
				navTypeSelected = it->first;
			}
		}
		return pH->EndFunction(navTypeSelected);
	}
	else
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetNavigationType(): wrong type of parameter 1");
	return pH->EndFunction();

}

// Binary finction to sort AI objects based on distance to given point.
struct AIObjDistanceTo : public std::binary_function <IAIObject*, IAIObject*, bool> 
{
	AIObjDistanceTo( const Vec3& target, float range ) : m_target(target), m_range(range) {}
	bool operator()( const IAIObject* lhs,  const IAIObject* rhs ) const
	{
		return fabsf( Distance::Point_PointSq( lhs->GetPos(), m_target ) - m_range) > fabsf( Distance::Point_Point( rhs->GetPos(), m_target ) - m_range);
	}
	const Vec3& m_target;
	const float m_range;
};

// Binary finction to sort AI objects based on distance to given point.
struct AIObjDistanceToDir : public std::binary_function <IAIObject*, IAIObject*, bool> 
{
	AIObjDistanceToDir( const Vec3& target, const Vec3& dir, float range ) : m_target(target), m_dir(dir), m_range(range) {}
	bool operator()( const IAIObject* lhs,  const IAIObject* rhs ) const
	{
		float	lhsMod = m_dir.Dot( lhs->GetPos() - m_target ) < 0.0f ? 10.0f : 1.0f;
		float	rhsMod = m_dir.Dot( rhs->GetPos() - m_target ) < 0.0f ? 10.0f : 1.0f;
		return lhsMod * fabsf( Distance::Point_PointSq( lhs->GetPos(), m_target ) - m_range) > lhsMod * fabsf( Distance::Point_Point( rhs->GetPos(), m_target ) - m_range);
	}
	const Vec3& m_target;
	const Vec3& m_dir;
	const float m_range;
};


//====================================================================
// IntersectSweptSphere
// hitPos is optional - may be faster if 0
//====================================================================
bool IntersectSweptSphere(Vec3 *hitPos, float& hitDist, const Lineseg& lineseg, float radius,IPhysicalEntity** pSkipEnts=0, int nSkipEnts=0, int additionalFilter = 0)
{
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	primitives::sphere spherePrim;
	spherePrim.center = lineseg.start;
	spherePrim.r = radius;
	Vec3 dir = lineseg.end - lineseg.start;

	geom_contact *pContact = 0;
	geom_contact **ppContact = hitPos ? &pContact : 0;
	int geomFlagsAll=0;
	int geomFlagsAny=geom_colltype0;

	float d = pPhysics->PrimitiveWorldIntersection(spherePrim.type, &spherePrim, dir, 
		ent_static | ent_terrain | ent_ignore_noncolliding | additionalFilter, ppContact, 
		geomFlagsAll, geomFlagsAny, 0,0,0, pSkipEnts, nSkipEnts);

	if (d > 0.0f)
	{
		hitDist = d;
		if (pContact && hitPos)
			*hitPos = pContact->pt;
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::GetNearestHidespot(IFunctionHandler * pH)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	//	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	float	rangeMin = 0;
	float	rangeMax = 5;
	bool	useCenter = false;
	Vec3	center(0,0,0);

	if(!pH->GetParam(2, rangeMin))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetNearestHidespot(): wrong type of parameter 2");
		return pH->EndFunction();
	}
	if(!pH->GetParam(3, rangeMax))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetNearestHidespot(): wrong type of parameter 2");
		return pH->EndFunction();
	}
	if(pH->GetParamCount() > 3)
	{
		if(!pH->GetParam(4, center))
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetNearestHidespot(): wrong type of parameter 3");
			return pH->EndFunction();
		}
		useCenter = true;
	}

	IAISystem*			pAISystem = m_pSystem->GetAISystem();
	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	IPipeUser* pPipeUser = CastToIPipeUserSafe(pAI);
	if (!pAIActor || !pPipeUser)
		return pH->EndFunction();

	IAIObject* pTarget = pPipeUser->GetAttentionTarget();
	IAIObject* pBeacon = pAISystem->GetBeacon(pAI->GetGroupId());

	if(!useCenter)
		center = pAI->GetPos();

	Vec3 hideFrom;
	if(pTarget)
		hideFrom = pTarget->GetPos();
	else if(pBeacon)
		hideFrom = pBeacon->GetPos();
	else 
		hideFrom = pAI->GetPos();	// Just in case there is nothing to hide from (not even beacon), at least try to hide.

	Vec3	coverPos(0,0,0);
	if(!pAISystem->GetHideSpotsInRange(pAI, center, hideFrom, rangeMin, rangeMax, true, true, 1, &coverPos, 0, 0, 0, 0))
		return pH->EndFunction();

	return pH->EndFunction(coverPos);
}

//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::GetDirectAnchorPos(IFunctionHandler * pH)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	//	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	float	fRadiusMax = 15;
	float	fRadiusMin = 1;
	int		nAnchor(0);

	if(!pH->GetParam(2, nAnchor))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetDirectAttackPos(): wrong type of parameter 2");
		return pH->EndFunction();
	}
	if(!pH->GetParam(3, fRadiusMin))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetDirectAttackPos(): wrong type of parameter 3");
		return pH->EndFunction();
	}
	if(!pH->GetParam(4, fRadiusMax))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetDirectAttackPos(): wrong type of parameter 3");
		return pH->EndFunction();
	}
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	IPuppet* pPuppet = pAI->CastToIPuppet();
	if (!pPuppet)
		return pH->EndFunction();

	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction();

	IAIObject* pTarget = pPipeUser->GetAttentionTarget();
	if(!pTarget)
		return pH->EndFunction();

	IAIObject *pAnchor = m_pAISystem->GetNearestToObjectInRange(pAI,nAnchor,fRadiusMin,fRadiusMax,-1,true,true,false);

	if(pAnchor)
		return pH->EndFunction(pAnchor->GetPos());
	
	return pH->EndFunction();

}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::EvalHidespot(IFunctionHandler * pH)
{
	// Evaluation:
	// -1 - bad target or shooter
	//  0 - no cover at all
	//  1 - low cover
	//  2 - blind cover
	//  3 - high cover

	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	//	SCRIPT_CHECK_PARAMETERS(4);
	GET_ENTITY(1);

	IAISystem*			pAISystem = m_pSystem->GetAISystem();
	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

	IAIObject* pShooter = pEntity->GetAI();
	IPuppet* pShooterPuppet = CastToIPuppetSafe(pShooter);
	IPipeUser* pShooterPipeUser = CastToIPipeUserSafe(pShooter);
	if (!pShooter || !pShooterPuppet || !pShooterPipeUser)
		return pH->EndFunction(-1);

	IAIObject* pTarget = pShooterPipeUser->GetAttentionTarget();
	if(!pTarget)
		return pH->EndFunction(-1);

	IPhysicalEntity*	pPhysSkip[4];
	int nSkip = 0;
	if(pShooter->GetProxy())
	{
		// Get both the collision proxy and the animation proxy
		pPhysSkip[nSkip] = pShooter->GetProxy()->GetPhysics(false);
		if(pPhysSkip[nSkip]) nSkip++;
		pPhysSkip[nSkip] = pShooter->GetProxy()->GetPhysics(true);
		if(pPhysSkip[nSkip]) nSkip++;
	}
	if(pTarget->GetProxy())
	{
		// Get both the collision proxy and the animation proxy
		pPhysSkip[nSkip] = pTarget->GetProxy()->GetPhysics(false);
		if(pPhysSkip[nSkip]) nSkip++;
		pPhysSkip[nSkip] = pTarget->GetProxy()->GetPhysics(true);
		if(pPhysSkip[nSkip]) nSkip++;
	}

	Vec3	floorPos = pShooterPuppet->GetFloorPosition(pShooter->GetPos());
	SAIBodyInfo bodyInfo;
	pShooter->GetProxy()->QueryBodyInfo(bodyInfo);

	// Hard-coded for human size
	Vec3	waistPos = floorPos + Vec3(0, 0, 0.75f);
	Vec3	headPos = floorPos + Vec3(0, 0, 1.3f);
	Vec3	targetPos = pTarget->GetPos();

	int	evaluation = 0;
	ray_hit hit;
	int rayresult;

	rayresult = RayWorldIntersectionWrapper(waistPos, (targetPos - waistPos), ent_static, rwi_stop_at_pierceable, &hit, 1, pPhysSkip, nSkip);
	if(rayresult && hit.dist < 2.5f)
		evaluation |= 1;

	rayresult = RayWorldIntersectionWrapper(headPos, (targetPos - headPos), ent_static, rwi_stop_at_pierceable, &hit, 1, pPhysSkip, nSkip);
	if(rayresult && hit.dist < 2.5f)
		evaluation |= 2;

	return pH->EndFunction(evaluation);
}

//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::EvalPeek(IFunctionHandler * pH)
{
	// Evaluation:
	//  -1 - don't need to peek
	//  0 - cannot peek
	//  1 - can peek from left
	//  2 - can peek from right
	//  3 - can peek from left & right

	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	//	SCRIPT_CHECK_PARAMETERS(4);
	GET_ENTITY(1);

	IAISystem*			pAISystem = m_pSystem->GetAISystem();
	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();

	IAIObject* pShooter = pEntity->GetAI();
	IPuppet* pShooterPuppet = CastToIPuppetSafe(pShooter);
	IPipeUser* pShooterPipeUser = CastToIPipeUserSafe(pShooter);
	IAIActor* pShooterActor = CastToIAIActorSafe(pShooter);
	if (!pShooter || !pShooterPuppet || !pShooterPipeUser || !pShooterActor)
		return pH->EndFunction(0);

	IAIObject* pTarget = pShooterPipeUser->GetAttentionTarget();

	if(!pTarget)
		pTarget = m_pAISystem->GetBeacon(pShooter->GetGroupId());

	if(!pTarget)
		return pH->EndFunction(0);

	IPhysicalEntity*	pPhysSkip[4];
	int nSkip = 0;
	if(pShooter->GetProxy())
	{
		// Get both the collision proxy and the animation proxy
		pPhysSkip[nSkip] = pShooter->GetProxy()->GetPhysics(false);
		if(pPhysSkip[nSkip]) nSkip++;
		pPhysSkip[nSkip] = pShooter->GetProxy()->GetPhysics(true);
		if(pPhysSkip[nSkip]) nSkip++;
	}
	if(pTarget->GetProxy())
	{
		// Get both the collision proxy and the animation proxy
		pPhysSkip[nSkip] = pTarget->GetProxy()->GetPhysics(false);
		if(pPhysSkip[nSkip]) nSkip++;
		pPhysSkip[nSkip] = pTarget->GetProxy()->GetPhysics(true);
		if(pPhysSkip[nSkip]) nSkip++;
	}

	Vec3	floorPos = pShooterPuppet->GetFloorPosition(pShooter->GetPos());
	SAIBodyInfo bodyInfo;
	pShooter->GetProxy()->QueryBodyInfo(bodyInfo);

	// Hard-coded for human size
	float	height = 1.3f;
	if(bodyInfo.stance == STANCE_STAND)
		height = 1.3f;
	else if(bodyInfo.stance == STANCE_STEALTH)
		height = 1.0f;
	else if(bodyInfo.stance == STANCE_CROUCH)
		height = 0.6f;

	Vec3	shooterPos(floorPos.x, floorPos.y, floorPos.z + height);
	Vec3	targetPos(pTarget->GetPos());
	Vec3	dir(targetPos - shooterPos);
	Vec3	right(pEntity->GetWorldTM().TransformVector(Vec3(1,0,0)));
	dir.NormalizeSafe();

	int	evaluation = -1;  
	ray_hit hit;

	float	dist = 3.0f;			// how far to check for the obstruction
	float	rad = 0.5f;			// the radius of the obstruction checked in the middle. 
	float	peekDist = 0.7f;	// the lateral distance to check for visibility
	float	peekAnticipate = 2.0f;

	AABB	rayBox;
	rayBox.Reset();
	rayBox.Add(shooterPos, rad);
	rayBox.Add(shooterPos + dir * dist, rad);

	IPhysicalEntity **entities;
	unsigned nEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(rayBox.min, rayBox.max, entities, ent_static|ent_ignore_noncolliding);
	Vec3	hitDir;

	//	if(pPhysWorld->RayWorldIntersection(shooterPos, dir * dist, ent_static, rwi_stop_at_pierceable, &hit, 1))
	if(OverlapSweptSphere(shooterPos + Vec3(0,0,rad*0.5f), dir * dist, rad, entities, nEntities, hitDir))
	{
		// The center must be partially obscured for the peek to make sense.
		evaluation = 0;

		pAISystem->AddDebugLine(shooterPos, shooterPos + right * peekDist, 255, 255, 255, 4 );

		if(!RayWorldIntersectionWrapper(shooterPos, right * peekDist, ent_static, rwi_stop_at_pierceable, &hit, 1))
		{
			Vec3	p = shooterPos + right * peekDist;
			pAISystem->AddDebugLine(p, p + dir * dist, 255, 255, 255, 4 );
			if(!RayWorldIntersectionWrapper(p, dir * dist, ent_static, rwi_stop_at_pierceable, &hit, 1))
				evaluation |= 1;
		}

		pAISystem->AddDebugLine(shooterPos, shooterPos - right * peekDist, 255, 255, 255, 4 );

		if(!RayWorldIntersectionWrapper(shooterPos, -right * peekDist, ent_static, rwi_stop_at_pierceable, &hit, 1))
		{
			Vec3	p = shooterPos - right * peekDist;
			pAISystem->AddDebugLine(p, p + dir * dist, 255, 255, 255, 4 );
			if(!RayWorldIntersectionWrapper(p, dir * dist, ent_static, rwi_stop_at_pierceable, &hit, 1))
				evaluation |= 2;
		}
	}

	return pH->EndFunction(evaluation);
}

//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::NotifyReinfDone(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	int done = 0;
	if(!pH->GetParam(2,done))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.NotifyGroupTacticState(): wrong type of parameter 2");
		return pH->EndFunction();
	}

	if(pEntity && pEntity->GetAI())
	{
		IAISystem* pAISystem = m_pSystem->GetAISystem();
		IAIObject* pAI = pEntity->GetAI();
		IAIGroup*	pGroup = pAISystem->GetIAIGroup(pAI->GetGroupId());
		if(pGroup)
			pGroup->NotifyReinfDone(pEntity->GetAI(), done!=0);
	}
	return pH->EndFunction(0);
}

//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::NotifyGroupTacticState(IFunctionHandler * pH)
{
	GET_ENTITY(1);

	if(pH->GetParamCount() < 3)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.NotifyGroupTacticState(): too few parameters %d", pH->GetParamCount());
		return pH->EndFunction();
	}

	int eval = 0;
	int	type = 0;
	float	param = 0.0f;
	if(!pH->GetParam(2,eval))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.NotifyGroupTacticState(): wrong type of parameter 2");
		return pH->EndFunction();
	}
	if(!pH->GetParam(3,type))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.NotifyGroupTacticState(): wrong type of parameter 3");
		return pH->EndFunction();
	}
	if(pH->GetParamCount() > 3)
	{
		if(!pH->GetParam(4,param))
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "AI.NotifyGroupTacticState(): wrong type of parameter 4");
			return pH->EndFunction();
		}
	}

	if(pEntity && pEntity->GetAI())
	{
		IAISystem* pAISystem = m_pSystem->GetAISystem();
		IAIObject* pAI = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pAI);
		if(pAIActor)
		{
			IAIGroup*	pGroup = pAISystem->GetIAIGroup(pAI->GetGroupId());
			if(pGroup)
				pGroup->NotifyGroupTacticState(pEntity->GetAI(), eval, type, param);
		}
	}
	return pH->EndFunction(0);
}

//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::GetGroupTacticState(IFunctionHandler * pH)
{
	GET_ENTITY(1);

	if(pH->GetParamCount() < 3)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetGroupTacticState(): too few parameters %d", pH->GetParamCount());
		return pH->EndFunction();
	}

	int eval = 0;
	int	type = 0;
	float	param = 0.0f;
	if(!pH->GetParam(2,eval))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetGroupTacticState(): wrong type of parameter 2");
		return pH->EndFunction();
	}
	if(!pH->GetParam(3,type))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetGroupTacticState(): wrong type of parameter 3");
		return pH->EndFunction();
	}
	if(pH->GetParamCount() > 3)
	{
		if(!pH->GetParam(4,param))
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetGroupTacticState(): wrong type of parameter 4");
			return pH->EndFunction();
		}
	}

	if(pEntity && pEntity->GetAI())
	{
		IAISystem* pAISystem = m_pSystem->GetAISystem();
		IAIObject* pAI = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pAI);
		if(pAIActor)
		{
			IAIGroup*	pGroup = pAISystem->GetIAIGroup(pAI->GetGroupId());
			if(pGroup)
				return pH->EndFunction(pGroup->GetGroupTacticState(pEntity->GetAI(), eval, type, param));
		}
	}
	return pH->EndFunction(0);
}

//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::GetGroupTacticPoint(IFunctionHandler * pH)
{
	GET_ENTITY(1);

	if(pH->GetParamCount() < 3)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetGroupTacticPoint(): too few parameters %d", pH->GetParamCount());
		return pH->EndFunction();
	}


	int eval = 0;
	int	type = 0;
	float	param = 0.0f;
	if(!pH->GetParam(2,eval))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetGroupTacticPoint(): wrong type of parameter 2");
		return pH->EndFunction();
	}
	if(!pH->GetParam(3,type))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetGroupTacticPoint(): wrong type of parameter 3");
		return pH->EndFunction();
	}
	if(pH->GetParamCount() > 3)
	{
		if(!pH->GetParam(4,param))
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "AI.GetGroupTacticPoint(): wrong type of parameter 4");
			return pH->EndFunction();
		}
	}

	if(pEntity && pEntity->GetAI())
	{
		IAISystem* pAISystem = m_pSystem->GetAISystem();
		IAIObject* pAI = pEntity->GetAI();
		IAIActor* pAIActor = CastToIAIActorSafe(pAI);
		if(pAIActor)
		{
			IAIGroup*	pGroup = pAISystem->GetIAIGroup(pAI->GetGroupId());
			if(pGroup)
			{
				Vec3	point(pGroup->GetGroupTacticPoint(pEntity->GetAI(), eval, type, param));
				if(!point.IsZero())
					return pH->EndFunction(point);
			}
		}
	}
	return pH->EndFunction();
}

int CScriptBind_AI::GetGroupAveragePosition(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);	
	GET_ENTITY(1);
	uint32 properties;
	pH->GetParam(2,properties);

	CScriptVector vReturnPos;
	pH->GetParam(3,vReturnPos);

	if(!pEntity)
	{
		vReturnPos.Set(ZERO);
		return pH->EndFunction();
	}
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
	{
		vReturnPos.Set(ZERO);
		return pH->EndFunction();
	}
	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if (!pAI)
	{
		vReturnPos.Set(ZERO);
		return pH->EndFunction();
	}

	IAIGroup* pGroup = m_pAISystem->GetIAIGroup(pAI->GetGroupId());
	if(!pGroup)
	{
		vReturnPos.Set(ZERO);
		return pH->EndFunction();
	}


	Vec3 pos = pGroup->GetAveragePosition(IAIGroup::AVMODE_PROPERTIES,properties);

	vReturnPos.Set(pos);

	return pH->EndFunction();

}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetPFBlockerRadius(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);	
	GET_ENTITY(1);
	if(pEntity)
	{

		// todo: make it understand define/entity/pos for secont argument
		//	if (pH->GetParamType(4) == svtString)
		//	pH->GetParam(2,hdl);
		//	nID = hdl.n;
		//	IEntity* pBlockerEntity(m_pSystem->GetIEntitySystem()->GetEntity(nID));
		int	blockerType(PFB_NONE);
		pH->GetParam(2,blockerType);

		float radius;
		pH->GetParam(3,radius);
		IAIObject* pObject = pEntity->GetAI();
		if(pObject)
			pObject->SetPFBlockerRadius(blockerType, radius);
		else
			gEnv->pAISystem->Warning( "<CScriptBind_AI::SetPFBlockerRadius> ", "WARNING: no AI for entity %s", pEntity->GetName() );
	}
	return pH->EndFunction();
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetMinFireTime(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);	
	GET_ENTITY(1);
	if(pEntity)
	{
		IAIObject* pObject = pEntity->GetAI();
		if(pObject && pObject->CastToIPuppet())
		{
			CAIProxy* pProxy = (CAIProxy*)(pObject->GetProxy());
			if(pProxy)
			{
				float time=0;
				pH->GetParam(2,time);
				pProxy->SetMinFireTime(time);
			}
		}
	}
	return pH->EndFunction();
}



//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetRefPointToGrenadeAvoidTarget(IFunctionHandler *pH)
{
	if ( pH->GetParamCount() < 3)
	{
		gEnv->pAISystem->Warning( "<CScriptBind_AI::SetRefPointToGrenadeAvoidTarget> ", "WARNING: too few parameters (expected at least 3)" );
		return pH->EndFunction(-1.0f);
	}
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(-1.0f);

	Vec3 grenadePos(0,0,0);
	if(!pH->GetParam(2, grenadePos))
		return pH->EndFunction(-1.0f);

	float reactionRadius = 0.0f;
	if(!pH->GetParam(3, reactionRadius))
		return pH->EndFunction(-1.0f);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(-1.0f);
	IPipeUser *pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction(-1.0f);

	const Vec3&	posAI = pAI->GetPos();
	float distToGrenadeSq = Distance::Point_PointSq(posAI, grenadePos);
	if (distToGrenadeSq > sqr(reactionRadius))
		return pH->EndFunction(-1.0f);

	IAIObject* pTarget = pPipeUser->GetAttentionTarget();

	Vec3 dir(0,0,0);

	if (pTarget && pAI->IsHostile(pTarget))
	{
		// Avoid both the target and the grenade.
		Lineseg	line(grenadePos, pTarget->GetPos());
		float	t = 0;
		float dist = Distance::Point_Lineseg(posAI, line, t);
		dir = posAI - line.GetPoint(t);
//		m_pAISystem->AddDebugLine(line.start, line.end, 255,0,0, 10.0f);
	}
	else
	{
		// Away from the grenade.
		dir = posAI - grenadePos;
	}

	dir.z = 0;
	dir.Normalize();
	dir *= reactionRadius * 2;
	pPipeUser->SetRefPointPos(posAI + dir);

//	m_pAISystem->AddDebugLine(posAI, posAI + dir, 255,255,255, 10.0f);
//	m_pAISystem->AddDebugSphere(posAI + dir, 0.3f, 255,255,255, 10.0f);

	return pH->EndFunction(sqrtf(distToGrenadeSq));
}


//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::IsAgentInTargetFOV(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	float	FOV;
	if(!pH->GetParam(2, FOV))
		return pH->EndFunction(0);

	if(!pEntity)
		return pH->EndFunction(0);

	IAIObject* pAI = pEntity->GetAI();
	if(!pAI)
		return pH->EndFunction(0);

	IPipeUser *pPipeUser = pAI->CastToIPipeUser();
	if(!pPipeUser)
		return pH->EndFunction(0);

	IAIObject	*pTarget(pPipeUser->GetAttentionTarget());
	if(!pTarget)
		return pH->EndFunction(0);

	// Check if the entity is within the target FOV.
	Vec3	dirToTarget = pAI->GetPos() - pTarget->GetPos();
	dirToTarget.z = 0.0f;
	dirToTarget.NormalizeSafe();

	Vec3	viewDir(pTarget->GetViewDir());
	viewDir.z = 0.0f;
	viewDir.NormalizeSafe();

	Vec3	p(pTarget->GetPos() + Vec3(0,0,0.5f));
	float	dot = dirToTarget.Dot(viewDir);

	if(dot > cosf(DEG2RAD(FOV/2)))
		return pH->EndFunction(1);

	return pH->EndFunction(0);
}

//
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::AddCombatClass(IFunctionHandler *pH)
{
	if (pH->GetParamCount() == 0)
	{
		m_pAISystem->AddCombatClass(0, NULL, 0, 0);
		return pH->EndFunction();
	}

	int combatClass = 0;
	if (!pH->GetParam(1, combatClass))
		return pH->EndFunction();

	int n = 0;
	std::vector<float> scalesVector;
	SmartScriptTable pTable; 
	if (pH->GetParam(2, pTable))
	{
		float scaleValue = 1.f;
		while (pTable->GetAt(n+1, scaleValue))
		{
			scalesVector.push_back(scaleValue);
			++n;
		}
	}

	// Check is optional custom OnSeen signal specified
	const char* szCustomSignal = 0;
	if (pH->GetParamCount() > 2)
		pH->GetParam(3, szCustomSignal);

	m_pAISystem->AddCombatClass(combatClass, &scalesVector[0], n, szCustomSignal);

	return pH->EndFunction();
}


//
// wrapper, to make it use tick-counter
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::RayWorldIntersectionWrapper(Vec3 org,Vec3 dir, int objtypes, unsigned int flags, ray_hit *hits,int nMaxHits,
																								IPhysicalEntity **pSkipEnts,int nSkipEnts, void *pForeignData,int iForeignData, int iCaller)
{
	IPhysicalWorld*	pPhysWorld = m_pSystem->GetIPhysicalWorld();
	int rwiResult(0);
	m_pAISystem->TickCountStartStop();
	rwiResult = pPhysWorld->RayWorldIntersection( org, dir, objtypes, flags, hits,nMaxHits,
		pSkipEnts, nSkipEnts, pForeignData,iForeignData, "RayWorldIntersectionWrapper(AI)", 0, iCaller);
	m_pAISystem->TickCountStartStop(false);
	return rwiResult;
}
//
//-----------------------------------------------------------------------------------------------------------
//

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetRefPointAtDefensePos(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	Vec3 vDefendSpot;
	float distance;
	pH->GetParam(2,vDefendSpot);
	pH->GetParam(3,distance);

	IAIObject* pAI = pEntity->GetAI();
	if(!pAI)
		return pH->EndFunction();

	IPipeUser *pPipeUser = pAI->CastToIPipeUser();
	if(!pPipeUser)
		return pH->EndFunction();
	IAIObject* pTarget = pPipeUser->GetAttentionTarget();
	if(!pTarget)
		return pH->EndFunction();
	Vec3 dir = (pTarget->GetPos() - vDefendSpot);
	float maxdist = dir.GetLength();
	distance = min(maxdist/2,distance);
	Vec3 defendPos = dir.GetNormalizedSafe()*distance + vDefendSpot;
	pPipeUser->SetRefPointPos(defendPos);

	return pH->EndFunction(1);
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetFormationUpdateSight(IFunctionHandler *pH)
{
	if(pH->GetParamCount()<2)
	{
		gEnv->pAISystem->Warning( "<CScriptBind_AI::SetFormationUpdateSight> ", "WARNING: too few parameters" );
		return pH->EndFunction();
	}

	GET_ENTITY(1);
	IAIObject* pAI = pEntity->GetAI();
	if(!pAI)
		return pH->EndFunction();

	float range=0,minTime=2,maxTime=2;

	pH->GetParam(2,range);
	if(range>0)
	{
		if(pH->GetParamCount()>2)
		{
			pH->GetParam(3,minTime);
			if(pH->GetParamCount()>3)
				pH->GetParam(4,maxTime);
			else
				maxTime = minTime;
		}
	}

	pAI->SetFormationUpdateSight(DEG2RAD(range),minTime,maxTime);

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetPathToFollow(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	const char*	pathName = 0;
	pH->GetParam(2, pathName);

	IAIObject* pAI = pEntity->GetAI();
	if(!pAI)
		return pH->EndFunction();

	pAI->SetPathToFollow(pathName);

	return pH->EndFunction();
}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetPathAttributeToFollow(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	bool bSpline = false;
	pH->GetParam(2, bSpline);

	IAIObject* pAI = pEntity->GetAI();
	if(!pAI)
		return pH->EndFunction();

	pAI->SetPathAttributeToFollow(bSpline);

	return pH->EndFunction();
}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetTotalLengthOfPath(IFunctionHandler *pH)
{

	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char*	pathName = 0;

	if(!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3	vTargetPos = pAI->GetPos();
	Vec3	vResult(ZERO);
	bool	bLoop(false);
	float	totalLength=0.0f;

	m_pAISystem->GetNearestPointOnPath( pathName, vTargetPos, vResult, bLoop, totalLength );

	return pH->EndFunction( totalLength );

}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetNearestPointOnPath(IFunctionHandler *pH)
{

	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char*	pathName = 0;

	if(!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3 vTargetPos;

	if(!pH->GetParam(3, vTargetPos))
		return pH->EndFunction();

	Vec3	vResult(ZERO);
	bool	bLoop(false);
	float	totalLength=0.0f;

	m_pAISystem->GetNearestPointOnPath( pathName, vTargetPos, vResult, bLoop, totalLength );

	return pH->EndFunction( Script::SetCachedVector( vResult, pH, 1 ) );

}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetPathSegNoOnPath(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char*	pathName = 0;

	if(!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3	vTargetPos;
	Vec3	vResult(ZERO);
	bool	bLoop(false);
	float	totalLength=0.0f;

	if(!pH->GetParam(3, vTargetPos))
		return pH->EndFunction();

	return pH->EndFunction( m_pAISystem->GetNearestPointOnPath( pathName, vTargetPos, vResult, bLoop, totalLength ) );

}
//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetPointOnPathBySegNo(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char*	pathName = 0;

	if(!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3	vResult(ZERO);
	float	segNo =0.0f;

	if(!pH->GetParam(3, segNo))
		return pH->EndFunction();

	m_pAISystem->GetPointOnPathBySegNo( pathName, vResult, segNo );
	gEnv->pAISystem->AddDebugLine( pAI->GetPos(), vResult, 0, 255, 0, 0.4f);

	return pH->EndFunction( vResult );

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetPathLoop(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	const char*	pathName = 0;

	if(!pH->GetParam(2, pathName))
		return pH->EndFunction();

	Vec3	vTargetPos(ZERO);
	Vec3	vResult(ZERO);
	bool	bLoop(false);
	float	totalLength=0.0f;

	m_pAISystem->GetNearestPointOnPath( pathName, vTargetPos, vResult, bLoop, totalLength );

	return pH->EndFunction( bLoop );

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::SetPointListToFollow(IFunctionHandler *pH)
{

	GET_ENTITY(1);

	if(pH->GetParamCount()<4)
	{
		gEnv->pAISystem->Warning("<SetPointListToFollow> ", "Too few parameters.");
		return pH->EndFunction();
	}
	if(pH->GetParamCount()>5)
	{
		gEnv->pAISystem->Warning("<SetPointListToFollow> ", "Too many parameters.");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(false);

	if (pH->GetParamType(2) != svtObject)
		return pH->EndFunction(false);

	SmartScriptTable theObj;

	if (!pH->GetParam(2,theObj))
		return pH->EndFunction(false);

	int nPoints = 0;
	if (!pH->GetParam(3, nPoints))
		return pH->EndFunction(false);

	bool bSpline = false;
	if (!pH->GetParam(4, bSpline))
		return pH->EndFunction(false);

	IAISystem::ENavigationType navType = IAISystem::NAV_FLIGHT;
	int navTypeScript =  static_cast<int>(IAISystem::NAV_FLIGHT);

	if(pH->GetParamCount() > 4)
	{
		if (!pH->GetParam(5, navTypeScript))
			return pH->EndFunction(false);
		navType = static_cast<IAISystem::ENavigationType>(navTypeScript); 
	}

	Vec3 vec;
	std::list<Vec3> pointList;
	int i;
	for ( i=1; i<=nPoints; i++ )
	{
		if(!theObj->GetAt(i,vec))
			break;
		pointList.push_back(vec);
	}

	if ( i >= 2 )
	{
		pAI->SetPointListToFollow(pointList,navType,bSpline);
		return pH->EndFunction(true);
	}

	return pH->EndFunction(false);

}

//-----------------------------------------------------------------------------------------------------------
int CScriptBind_AI::GetNearestPathOfTypeInRange(IFunctionHandler *pH)
{
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	if(!pAI)
		return pH->EndFunction();

	Vec3	pos(pAI->GetPos());
	float	range(0);
	int		type(0);
	float	devalue(0.0f);
	bool	useStartNode(false);

	if(!pH->GetParam(2, pos))
		return pH->EndFunction();
	if(!pH->GetParam(3, range))
		return pH->EndFunction();
	if(!pH->GetParam(4, type))
		return pH->EndFunction();
	if(pH->GetParamCount() > 4)
		if(!pH->GetParam(5, devalue))
			return pH->EndFunction();
	if(pH->GetParamCount() > 5)
		if(!pH->GetParam(6, useStartNode))
			return pH->EndFunction();

	const char* pathName = m_pAISystem->GetNearestPathOfTypeInRange(pAI, pos, range, type, devalue, useStartNode);

	return pH->EndFunction(pathName);
}

//-----------------------------------------------------------------------------------------------------------
int	CScriptBind_AI::GetGroupAlertness(IFunctionHandler * pH)
{
	GET_ENTITY(1);

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pAI);
	if(!pAIActor)
		return pH->EndFunction();

	int	maxAlertness = 0;
	int	groupID = pAI->GetGroupId();
	int n = m_pAISystem->GetGroupCount(groupID, IAISystem::GROUP_ENABLED);
	for (int i = 0; i < n; i++)
	{
		IAIObject*	pMember = m_pAISystem->GetGroupMember(groupID, i, IAISystem::GROUP_ENABLED);
		if (pMember && pMember->GetProxy())
			maxAlertness = max(maxAlertness, pMember->GetProxy()->GetAlertnessState());
	}

	return maxAlertness;
}


//====================================================================
// RegisterDamageRegion 
//====================================================================
int CScriptBind_AI::RegisterDamageRegion(IFunctionHandler *pH)
{
	//	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	Sphere damageSphere;
	if(!pH->GetParam(2, damageSphere.radius))
		damageSphere.radius = -1.f;
	damageSphere.center = pEntity->GetWorldPos();
	m_pAISystem ->RegisterDamageRegion(pEntity, damageSphere);
	return (pH->EndFunction());
}

//====================================================================
// GetDistanceAlongPath 
//====================================================================
int CScriptBind_AI::GetDistanceAlongPath(IFunctionHandler *pH)
{
	if(pH->GetParamCount()<2)
	{
		gEnv->pAISystem->Warning("<GetDistanceAlongPath> ", "Too few parameters.");
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	IEntity* pEntity1 = pEntity;
	pH->GetParam(2,hdl);
	int nID2 = hdl.n;
	IEntity* pEntity2 = m_pSystem->GetIEntitySystem()->GetEntity(nID2);
	if(pEntity1 && pEntity2)
	{	
		IAIObject* pAI = pEntity1->GetAI();
		bool bInit=false;
		if(pH->GetParamCount()>2)
			pH->GetParam(3,bInit);
		IPuppet* pPuppet = CastToIPuppetSafe(pAI);
		if(pPuppet)
		{	
			Vec3 pos(pEntity2->GetPos());
			return pH->EndFunction(pPuppet->GetDistanceAlongPath(pos,bInit));
		}
	}	
	return pH->EndFunction(0);
}

//====================================================================
// SetCurrentHideObjectUnreachable 
//====================================================================
int CScriptBind_AI::SetCurrentHideObjectUnreachable(IFunctionHandler *pH)
{
	if(pH->GetParamCount()<1)
	{
		gEnv->pAISystem->Warning("<SetCurrentHideObjectUnreachable> ", "Too few parameters.");
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	IEntity* pEntity1 = pEntity;
	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (pPipeUser)
		pPipeUser->SetCurrentHideObjectUnreachable();
	return pH->EndFunction(0);
}

//====================================================================
// FindStandbySpotInShape 
//====================================================================
int CScriptBind_AI::FindStandbySpotInShape(IFunctionHandler *pH)
{
	GET_ENTITY(1);
	IAIObject*	pAI = pEntity->GetAI();
	if(!pAI)
		return pH->EndFunction();
	const Vec3&	reqPos = pAI->GetPos();

	const char* shapeName = 0;
	int	type = 0;
	Vec3	targetPos(0,0,0);

	if(!pH->GetParam(2, shapeName))
		return pH->EndFunction();
	if(!shapeName)
	{
		gEnv->pAISystem->Warning("<FindStandbySpotInShape> ", "Invalid parameter 1.");
		return pH->EndFunction();
	}
	if(!pH->GetParam(3, targetPos))
		return pH->EndFunction();
	if(!pH->GetParam(4, type))
		return pH->EndFunction();

	const float	TRESHOLD = cosf(DEG2RAD(120.0f/2));

	IAIObject*	bestObj = 0;
	float				bestDist = FLT_MAX;
	AutoAIObjectIter it(m_pAISystem->GetFirstAIObjectInShape(IAISystem::OBJFILTER_TYPE, type, shapeName, false));
	for(; it->GetObject(); it->Next())
	{
		IAIObject*	obj = it->GetObject();

		const Vec3& pos = obj->GetPos();
		Vec3	dirObjToTarget = targetPos - pos;
		Vec3	forward = obj->GetMoveDir();

		dirObjToTarget.z = 0.0f;
		dirObjToTarget.NormalizeSafe();
		forward.z = 0.0f;
		forward.NormalizeSafe();

		float	dot = dirObjToTarget.Dot(forward);
		if(dot < TRESHOLD)
			continue;

		float	d = Distance::Point_Point(pos, reqPos);

		if(d < bestDist)
		{
			bestObj = obj;
			bestDist = d;
		}
	}

	if(bestObj)
		return pH->EndFunction(bestObj->GetName());

	return pH->EndFunction();
}

//====================================================================
// FindStandbySpotInSphere 
//====================================================================
int CScriptBind_AI::FindStandbySpotInSphere(IFunctionHandler *pH)
{
	GET_ENTITY(1);
	IAIObject*	pAI = pEntity->GetAI();
	if(!pAI)
		return pH->EndFunction();
	const Vec3&	reqPos = pAI->GetPos();

	int	type = 0;
	Vec3	spherePos;
	float	sphereRad;
	Vec3	targetPos(0,0,0);

	if(!pH->GetParam(2, spherePos))
		return pH->EndFunction();
	if(!pH->GetParam(3, sphereRad))
		return pH->EndFunction();
	if(!pH->GetParam(4, targetPos))
		return pH->EndFunction();
	if(!pH->GetParam(5, type))
		return pH->EndFunction();

	const float	TRESHOLD = cosf(DEG2RAD(120.0f/2));

	IAIObject*	bestObj = 0;
	float				bestDist = FLT_MAX;
	AutoAIObjectIter it(m_pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, type, spherePos, sphereRad, false));
	for(; it->GetObject(); it->Next())
	{
		IAIObject*	obj = it->GetObject();

		const Vec3& pos = obj->GetPos();
		Vec3	dirObjToTarget = targetPos - pos;
		Vec3	forward = obj->GetMoveDir();

		dirObjToTarget.z = 0.0f;
		dirObjToTarget.NormalizeSafe();
		forward.z = 0.0f;
		forward.NormalizeSafe();

		float	dot = dirObjToTarget.Dot(forward);
		if(dot < TRESHOLD)
			continue;

		float	d = Distance::Point_Point(pos, reqPos);

		if(d < bestDist)
		{
			bestObj = obj;
			bestDist = d;
		}
	}

	if(bestObj)
		return pH->EndFunction(bestObj->GetName());

	return pH->EndFunction();
}

//====================================================================
// GetProbableTargetPosition 
//====================================================================
int CScriptBind_AI::GetProbableTargetPosition(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();
	IPipeUser* pPipeUser = CastToIPipeUserSafe(pEntity->GetAI());
	if (!pPipeUser)
		return pH->EndFunction();
	return pH->EndFunction(pPipeUser->GetProbableTargetPosition());
}

//====================================================================
// GetObjectRadius 
//====================================================================
int CScriptBind_AI::GetObjectRadius(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	if(!pEntity || !pEntity->GetAI())
		return pH->EndFunction(0);
	return pH->EndFunction(pEntity->GetAI()->GetRadius());
}

//====================================================================
// GetEnclosingGenericShapeOfType 
//====================================================================
int CScriptBind_AI::GetEnclosingGenericShapeOfType(IFunctionHandler * pH)
{
	Vec3	pos(0,0,0);
	int	type = 0;
	int	checkHeight = 0;

	if(!pH->GetParam(1, pos))
		return pH->EndFunction();
	if(!pH->GetParam(2, type))
		return pH->EndFunction();
	if(!pH->GetParam(3, checkHeight))
		return pH->EndFunction();

	const char*	name = gEnv->pAISystem->GetEnclosingGenericShapeOfType(pos, type, checkHeight != 0);
	if(name)
		return pH->EndFunction(name);

	return pH->EndFunction();
}

//====================================================================
// IsPointInsideGenericShape 
//====================================================================
int CScriptBind_AI::IsPointInsideGenericShape(IFunctionHandler * pH)
{
	Vec3	pos(0,0,0);
	const char*	shapeName = 0;
	int	checkHeight = 0;

	if(!pH->GetParam(1, pos))
		return pH->EndFunction(0);
	if(!pH->GetParam(2, shapeName))
		return pH->EndFunction(0);
	if(!pH->GetParam(3, checkHeight))
		return pH->EndFunction(0);

	if(!shapeName)
		return pH->EndFunction(0);

	return pH->EndFunction(gEnv->pAISystem->IsPointInsideGenericShape(pos, shapeName, checkHeight != 0) ? 1 : 0);
}

//====================================================================
// DistanceToGenericShape 
//====================================================================
int CScriptBind_AI::DistanceToGenericShape(IFunctionHandler * pH)
{
	Vec3	pos(0,0,0);
	const char*	shapeName = 0;
	int	checkHeight = 0;

	if(!pH->GetParam(1, pos))
		return pH->EndFunction(0);
	if(!pH->GetParam(2, shapeName))
		return pH->EndFunction(0);
	if(!pH->GetParam(3, checkHeight))
		return pH->EndFunction(0);

	if(!shapeName)
		return pH->EndFunction(0);

	return pH->EndFunction(gEnv->pAISystem->DistanceToGenericShape(pos, shapeName, checkHeight != 0));
}

//====================================================================
// ConstrainPointInsideGenericShape 
//====================================================================
int CScriptBind_AI::ConstrainPointInsideGenericShape(IFunctionHandler * pH)
{
	Vec3	pos(0,0,0);
	const char*	shapeName = 0;
	int	checkHeight = 0;

	if(!pH->GetParam(1, pos))
		return pH->EndFunction(0);
	if(!pH->GetParam(2, shapeName))
		return pH->EndFunction(0);
	if(!pH->GetParam(3, checkHeight))
		return pH->EndFunction(0);

	if(!shapeName)
		return pH->EndFunction(0);

	gEnv->pAISystem->ConstrainInsideGenericShape(pos, shapeName, checkHeight != 0);

	return pH->EndFunction(pos);
}

//====================================================================
// CreateTempGenericShapeBox 
//====================================================================
int CScriptBind_AI::CreateTempGenericShapeBox(IFunctionHandler * pH)
{
	Vec3	c(0,0,0);
	float	r = 0;
	float	height = 0;
	int type = 0;

	if(!pH->GetParams(c, r, height, type))
		return pH->EndFunction();

	Vec3	points[4];

	points[0].Set(c.x - r, c.y - r, c.z);
	points[1].Set(c.x + r, c.y - r, c.z);
	points[2].Set(c.x + r, c.y + r, c.z);
	points[3].Set(c.x - r, c.y + r, c.z);

	const char*	name = gEnv->pAISystem->CreateTemporaryGenericShape(points, 4, height, type);
	if(name)
		return pH->EndFunction(name);
	return pH->EndFunction();
}

//====================================================================
// SetTerritoryShapeName
//====================================================================
int CScriptBind_AI::SetTerritoryShapeName(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	//retrieve the entity
	const char*	shapeName = 0;
	if (pEntity && pEntity->GetAI())
	{
		if(pH->GetParam(2,shapeName) && shapeName)
		{
			IPuppet *pPuppet = CastToIPuppetSafe(pEntity->GetAI());
			if (pPuppet)
			{
				pPuppet->SetTerritoryShapeName(shapeName);
				return pH->EndFunction(1);
			}
		}
	}
	return pH->EndFunction();
}

//====================================================================
// NotifySurpriseEntityAction
//====================================================================
int CScriptBind_AI::NotifySurpriseEntityAction(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	float	radius = 0;
	int	note = 0;
	if(!pH->GetParam(2, radius))
		return pH->EndFunction();
	if(!pH->GetParam(3, note))
		return pH->EndFunction();

	EntityId	miracleId = pEntity->GetId();

	// Send a signal to all entities that see the specified entity, that that entity have just done something miraculous.
	AutoAIObjectIter it(m_pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET, pEntity->GetWorldPos(), radius, false));
	for(; it->GetObject(); it->Next())
	{
		IPipeUser *pPipeUser = CastToIPipeUserSafe(it->GetObject());
		if(pPipeUser)
		{
			if(pPipeUser->GetAttentionTarget() && pPipeUser->GetAttentionTarget()->GetEntityID() == miracleId)
			{
				IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
				pData->iValue = note;
				IAIActor* pAIActor = it->GetObject()->CastToIAIActor();
				if(pAIActor)
					pAIActor->SetSignal(1, "SURPRISE_ACTION", pEntity, pData);
			}
		}
	}

	return pH->EndFunction();
}

//====================================================================
// DebugReportHitDamage 
//====================================================================
int CScriptBind_AI::DebugReportHitDamage(IFunctionHandler * pH)
{
	ScriptHandle victimHdl;
	pH->GetParam(1, victimHdl);
	int victimID = victimHdl.n;
	IEntity* pVictimEntity = m_pSystem->GetIEntitySystem()->GetEntity(victimID);

	ScriptHandle shooterHdl;
	pH->GetParam(2, shooterHdl);
	int shooterID = shooterHdl.n;
	IEntity* pShooterEntity = m_pSystem->GetIEntitySystem()->GetEntity(shooterID);

	float	damage = 0;
	const char*	material = 0;

	if(!pH->GetParam(3, damage))
		return pH->EndFunction();
	if(!pH->GetParam(4, material))
		return pH->EndFunction();

	gEnv->pAISystem->DebugReportHitDamage(pVictimEntity, pShooterEntity, damage, material);

	return pH->EndFunction();
}


//====================================================================
// CanMelee
//====================================================================
int CScriptBind_AI::CanMelee(IFunctionHandler * pH)
{
	GET_ENTITY(1);


	if(!pEntity)
		return pH->EndFunction(0);	
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction(0);	

	SAIWeaponInfo weaponInfo;
	pAIObject->GetProxy()->QueryWeaponInfo(weaponInfo);
	if(weaponInfo.canMelee)
		return pH->EndFunction(0);

	return pH->EndFunction(0);
}

//====================================================================
// IsMoving
//====================================================================
int CScriptBind_AI::IsMoving(IFunctionHandler * pH)
{
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction(0);
	IAIObject* pAIObject = pEntity->GetAI();
	if (!pAIObject)
		return pH->EndFunction(0);
	IAIActor* pActor = pAIObject->CastToIAIActor();
	if (!pActor)
		return pH->EndFunction(0);

	int run = 0;
	if(!pH->GetParam(2, run))
		return pH->EndFunction(0);

	switch(run)
	{
	case 0: run = (int)AISPEED_WALK; break;
	case 1: run = (int)AISPEED_RUN; break; 
	case 2: run = (int)AISPEED_WALK; break;
	default: run = (int)AISPEED_SPRINT; break;
	}

	int idx = MovementUrgencyToIndex(pActor->GetState()->fMovementUrgency);

	if (idx < run)
		return 0;

	if (pActor->GetState()->fDesiredSpeed > 0.0f && pActor->GetState()->vMoveDir.GetLength() > 0.01f)
		return pH->EndFunction(1);
	return pH->EndFunction(0);
}

//====================================================================
// IsEnabled
//====================================================================
int CScriptBind_AI::IsEnabled(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);

	if(!pEntity)
		return pH->EndFunction();	
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();	

	return pH->EndFunction(pAIObject->IsEnabled());
}

//====================================================================
// EnableCoverFire
//====================================================================
int CScriptBind_AI::EnableCoverFire(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	if(!pEntity)
		return pH->EndFunction();	
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();	

	IPuppet* pPuppet = pAIObject->CastToIPuppet(); 
	if (pPuppet)
	{
		bool bEnable = false;
		pH->GetParam(2,bEnable);
		pPuppet->EnableCoverFire(bEnable);
	}

	return pH->EndFunction();
}


// 
//====================================================================
int CScriptBind_AI::AddObstructSphere(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(4);
	Vec3	pos(0,0,0);
	float	radius(0.f);
	float	lifeTime(0.f);
	float	fadeTime(0.f);


	if(!pH->GetParam(1, pos))
		return pH->EndFunction(0);
	if(!pH->GetParam(2, radius))
		return pH->EndFunction(0);
	if(!pH->GetParam(3, lifeTime))
		return pH->EndFunction(0);
	if(!pH->GetParam(4, fadeTime))
		return pH->EndFunction(0);

	gEnv->pAISystem->AddObstructSphere(pos, radius, lifeTime, fadeTime);
	return pH->EndFunction(1);
}

// 
//====================================================================
int CScriptBind_AI::Animation(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();	
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();	

	EAnimationMode type;
	const char* animName;

	pH->GetParam(2,(int&)type);
	pH->GetParam(3,animName);

	if(pAIObject->GetProxy())
		pAIObject->GetProxy()->SetAGInput(type == AIANIM_ACTION ? AIAG_ACTION : AIAG_SIGNAL,animName);

	return pH->EndFunction();
}

int CScriptBind_AI::ProcessBalancedDamage(IFunctionHandler *pH)
{
	ScriptHandle shooterHdl;
	pH->GetParam(1, shooterHdl);
	int shooterID = shooterHdl.n;
	IEntity* pShooterEntity = m_pSystem->GetIEntitySystem()->GetEntity(shooterID);
	if(!pShooterEntity)
		return pH->EndFunction(0);

	ScriptHandle targetHdl;
	pH->GetParam(2, targetHdl);
	int targetID = targetHdl.n;
	IEntity* pTargetEntity = m_pSystem->GetIEntitySystem()->GetEntity(targetID);
	if(!pTargetEntity)
		return pH->EndFunction(0);

	float damage;
	const char* damageType;

	pH->GetParam(3, damage);
	pH->GetParam(4, damageType);

	return pH->EndFunction(gEnv->pAISystem->ProcessBalancedDamage(pShooterEntity, pTargetEntity, damage, damageType));
}


void DebugJumpCheck(Vec3& startpos, Vec3& destpos, float radius)
{
	uint8 r = 128;
	uint8 g = 14;
	uint8 b = 255;
	Vec3 dir(destpos - startpos );
	gEnv->pAISystem->AddDebugLine(startpos, destpos, r, g, b, 5);
	Vec3 center((startpos+destpos)/2);
	Vec3 dirx(dir.y,-dir.x,0);
	dirx.NormalizeSafe();
	dirx*=radius;
	Vec3 dirz(0,0,radius);
	gEnv->pAISystem->AddDebugLine(startpos, center + dirx - dirz, r, g, b, 5);
	gEnv->pAISystem->AddDebugLine(startpos, center + dirx + dirz, r, g, b, 5);
	gEnv->pAISystem->AddDebugLine(startpos, center - dirx + dirz, r, g, b, 5);
	gEnv->pAISystem->AddDebugLine(startpos, center - dirx - dirz, r, g, b, 5);
	gEnv->pAISystem->AddDebugLine(destpos, center + dirx - dirz, r, g, b, 5);
	gEnv->pAISystem->AddDebugLine(destpos, center + dirx + dirz, r, g, b, 5);
	gEnv->pAISystem->AddDebugLine(destpos, center - dirx + dirz, r, g, b, 5);
	gEnv->pAISystem->AddDebugLine(destpos, center - dirx - dirz, r, g, b, 5);

}

// 
//====================================================================
// 
//====================================================================
float getTofPointOnParabula(const Vec3& A, const Vec3& V, const Vec3& P, float g)
{
	float a = -g/2;
	float b = V.z;
	float c = A.z - P.z;
	float d = b*b - 4*a*c;
	if(d<0)
		return -1.f;
	float sqrtd = sqrt(d);
	float tP = (-b + sqrtd)/ (2*a);
	float tP1 = (-b - sqrtd)/ (2*a);
	float y = V.y*tP + A.y;
	float y1 = V.y*tP1 + A.y;
	if(fabs(P.y - y) > fabs(P.y - y1) )
		return tP1;
	return tP;
}

Vec3 getPointOnParabula(const Vec3& A, const Vec3& V, float t, float g)
{
	return Vec3(V.x * t + A.x, V.y * t + A.y, -g/2*t*t + V.z*t + A.z);
}

// 
//====================================================================
// TO DO: remove this

int CScriptBind_AI::CanJumpToPointOld(IFunctionHandler * pH)
{
	if (pH->GetParamCount() < 6)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint: Not enough parameters." );
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	// Get bbox (status) from physics engine
	if(!pEntity)
		return pH->EndFunction();
	IEntity* pEntity2 = NULL;

//	IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();
//	if(pPhysEntity)
	{
		Vec3 hitPos;
		float hitdist;
//		float radius;
//		pe_status_pos status;

		float theta=0; // initial angle
		float tB=0; // time at which entity will reach B
		float maxheight; // maximum height allowed
		Vec3 B(ZERO); // final position (parameter)
		IAIObject* pAI = pEntity->GetAI(); 
		if(!pAI)
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint: Not enough parameters." );
			return pH->EndFunction();
		}
//		pPhysEntity->GetStatus( &status );
		//radius = fabsf(status.BBox[1].x - status.BBox[0].x);
		//float radius2 = fabsf(status.BBox[1].y - status.BBox[0].y);
		SAIBodyInfo bodyInfo;
		pAI->GetProxy()->QueryBodyInfo(bodyInfo);

//		float radius = bodyInfo.colliderSize.GetSize().GetLength();
	//	radius = radius/2 + 0.1f;
		float radius = bodyInfo.colliderSize.GetSize().x;
		float radius2 = bodyInfo.colliderSize.GetSize().y;
		if(radius < radius2)
			radius = radius2;
		float myHeight = bodyInfo.colliderSize.GetSize().z;
		radius = sqrt(radius*radius + myHeight*myHeight);
		radius = radius*1.2f/2;

		SmartScriptTable	retVelocity;

		ScriptVarType type = pH->GetParamType(2);
		switch(type)
		{
		case svtString:
			if(pAI)
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if(pPipeUser)
				{
					const char* name;
					if(pH->GetParam(2,name))
					{
						IAIObject* pAItarget = pPipeUser->GetSpecialAIObject(name);
						if(pAItarget)
							B = pAItarget->GetPos();
					}
				}
			}
			break;
		case svtPointer:
			{
				ScriptHandle hdl;
				pH->GetParam(2,hdl);
				int nID = hdl.n;
				pEntity2 = m_pSystem->GetIEntitySystem()->GetEntity(nID);
				if(pEntity2)
					B = pEntity2->GetWorldPos();
			}
			break;
		case svtObject:
			{
				SmartScriptTable vecTable;
				if(pH->GetParam(2,vecTable))
				{	
					float x,y,z;
					if(vecTable->GetValue("x",x) && vecTable->GetValue("y",y) && vecTable->GetValue("z",z))
						B.Set(x,y,z);
				}
			}
			break;
		default:
			break;

		}

		if(B.IsZero())
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint: wrong parameter 2 format" );
			return pH->EndFunction();
		}
		else if(_isnan(B.x+B.y+B.z))
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint: NAN in destination position" );
			return pH->EndFunction();
		}

		int building;
		IVisArea *pArea;
		IAIActor* pActor = CastToIAIActorSafe(pAI);
		IAISystem::tNavCapMask navProperties =  pActor->GetMovementAbility().pathfindingProperties.navCapMask;
		IAISystem::ENavigationType navType =gEnv->pAISystem->CheckNavigationType(B,building,pArea,navProperties);

		Vec3 A(pEntity->GetWorldPos());//initial position


		int flags;
		pH->GetParam(5,flags);

		pH->GetParam(3,theta);

		pH->GetParam(4,maxheight);

		theta = DEG2RAD(theta);

		bool bCheckCollision = (flags & AI_JUMP_CHECK_COLLISION)!=0;

		if(flags & AI_JUMP_ON_GROUND)
		{
			// project B on terrain		
			ray_hit	hit;
			Vec3 rayDir(0,0,-30);
			float heightTerrain = m_pSystem->GetI3DEngine()->GetTerrainElevation( B.x, B.y );
			if(B.z < heightTerrain)
				B.z = heightTerrain;

			if (RayWorldIntersectionWrapper(B, rayDir, ent_static|ent_terrain, rwi_stop_at_pierceable, &hit, 1))
				B = hit.pt;
		}

		if( !pH->GetParam( 6, retVelocity )) 
			return pH->EndFunction();

		IPhysicalEntity* pSkipEnt = NULL;
		IPhysicalEntity* pSkipMyEnt = pEntity->GetPhysics();
		IPhysicalEntity* SkipEnt[2];
		SkipEnt[0] = pSkipMyEnt;

		if (pH->GetParamCount() >6)
		{
			if(pH->GetParamType(7)==svtPointer)
			{
				ScriptHandle hdl;
				pH->GetParam(7,hdl);
				int nID = hdl.n;
				IEntity* pEntity2 = m_pSystem->GetIEntitySystem()->GetEntity(nID);
				if(pEntity2)
					pSkipEnt = pEntity2->GetPhysics();
			}
		}
		if (pH->GetParamCount() >7 && pH->GetParamType(8)!=svtNull)
		{
			pH->GetParam(8,A);
		}

		int nSkipMyEnts = pSkipMyEnt ? 1 : 0;
		if(pSkipEnt)
			SkipEnt[nSkipMyEnts] = pSkipEnt;
		int nSkipEnts = pSkipEnt ? nSkipMyEnts + 1 : nSkipMyEnts;

		Vec3 AB = B-A;

		if(AB.z > maxheight)
		{
			return pH->EndFunction();
		}

		float cosTheta = cos(theta);

		Vec3 dir(AB);
		if(fabs(cosTheta)< 0.01f)
		{ // almost vertical, let's approximate to it to avoid lua imprecisions
			cosTheta = 0;
			theta = gf_PI/2;
			dir.x = dir.y = 0;
		}

		dir.z=0;
		float x = dir.GetLength();
		dir.NormalizeSafe();

		float tanTheta = cosTheta != 0 ? tan(theta) : 0;

		// check if the initial angle is too low for destination
		float cosDestAngle = dir.Dot(AB.GetNormalizedSafe());

		// adding a threshold to avoid super velocities when destination angle is just slightly below requested angle
		float thr = 0.05f;
		// (assuming negative gravity)
		if(B.z > A.z) // destination above
		{
			if( theta<=0 || (cosTheta > cosDestAngle - thr) && cosTheta!=0)
				return pH->EndFunction();
		}
		else if(theta<=0 && (cosTheta < cosDestAngle + thr) && cosTheta!=0)
			return pH->EndFunction();

		float g = 9.81f;  

		IPhysicalEntity *phys = pEntity->GetPhysics();
		Vec3 vAddVelocity(ZERO);
		float addSpeed = 0;

		if(phys)
		{
			pe_player_dynamics	dyn;
			phys->GetParams(&dyn);

			// use current actual gravity	of the object
			g = -dyn.gravity.z;

			if(flags & AI_JUMP_RELATIVE)
			{
				// jump in the entity's reference frame
				pe_status_dynamics	dyns;
				phys->GetStatus(&dyns);
				vAddVelocity = dyns.v;
				addSpeed = vAddVelocity.GetLength();
			}

		}
/*
		if((flags & AI_JUMP_MOVING_TARGET) !=0  && pEntity2)
		{
			int i=10;
			while(pEntity2->GetParent() && i--)
				pEntity2 = pEntity2->GetParent();

			IPhysicalEntity *phys = pEntity2->GetPhysics();

			if(phys)
			{
				pe_status_dynamics	dyns;
				phys->GetStatus(&dyns);
				vAddVelocity += dyns.v;
				addSpeed = vAddVelocity.GetLength();
				flags |= AI_JUMP_RELATIVE;
			}
		}
*/

		float z0 = AB.z;
		float v; //initial velocity (module), the incognita

		if(cosTheta!=0)
		{
			float t0 = sqrt(fabs(2/g *(z0 - tanTheta *x)));
			v = x/(cosTheta*t0);
		}
		else
		{
			if(x == 0) // jumping vertically
			{	
				// use the given height as parameter, v = v(AB.z)
				if(z0<0)
					return pH->EndFunction();
				v = sqrt(2* z0 *g);
			}
			else 
				return pH->EndFunction();
		}

		// check max height
		float Xc; // max distance on origin's plane
		float ZMax; // max height with v
		float tMax;
		Vec3 dir90;//(dir.y,-dir.x,0);
		Vec3 VelN;//(dir.GetRotated(dir90,theta));

		if(x !=0) // not vertical
		{
			dir90.Set(dir.y,-dir.x,0);
			VelN = dir.GetRotated(dir90,theta);
			if(flags & AI_JUMP_RELATIVE)
			{
				VelN = (VelN*v + vAddVelocity);
				v = VelN.GetLength();
				if(v>0)
					VelN /= v;
			}
			Xc = sin(2*theta)*v*v /g; // max distance on origin's plane
			ZMax = Xc * tanTheta / 4; // max height with v
			if(ZMax > maxheight)
				return pH->EndFunction();
			tMax = Xc/v;
		}
		else
		{
			VelN.Set(0,0,1);
			if(flags & AI_JUMP_RELATIVE)
			{
				VelN = (VelN*v + vAddVelocity);
				v = VelN.GetLength();
				if(v>0)
					VelN /= v;
			}
			tMax = 0; //ignored
			Xc = 0;
			ZMax = maxheight;
		}
		

		Vec3 V(VelN * v); // initial velocity vector;

#ifdef USER_luciano
		// shouldn't compile in gcc
		gEnv->pAISystem->AddDebugSphere(B,0.5,128,128,255,5);
		gEnv->pAISystem->AddDebugLine(B-Vec3(1,0,0),B+Vec3(1,0,0),128,128,255,5);
		gEnv->pAISystem->AddDebugLine(B-Vec3(0,1,0),B+Vec3(0,1,0),128,128,255,5);
#endif

		if(bCheckCollision)
		{

			//---------------------------------------------------
			//check obstacles in trajectory
			//---------------------------------------------------
			Vec3 P(A + VelN*radius);

			if(x< radius)
			{
				// (almost) vertical jump
				Lineseg T(P,B);
				gEnv->pAISystem->AddDebugLine(P,B,128,40,255,5);

				if(IntersectSweptSphere(&hitPos, hitdist, T, radius*1.2f, nSkipEnts ? SkipEnt : NULL, nSkipEnts))
					return pH->EndFunction();
			}
			else if(theta>0)
			{

				float halfXc = Xc/2;

				float Xm = Xc/2;
				float invTanTheta = 1/tanTheta;
				Vec3 Zm(A.x + dir.x*Xm, A.y + dir.y*Xm, A.z + ZMax);

				/*
				x = Vx*t + Ax
				y = Vy*t + Ay
				z = -g/2*t^2 + Vz*t + Az
				*/
				// Zm is such that t = ...
				float vx = v*cosTheta;
				// x=vt
				float tZm = Xm/vx;
				float tMax = 2*tZm; // time to run the parabola to the next point with same Z as start point

				// B is such that t = ...
				float tB = getTofPointOnParabula(A,V,B,g); // time to reach destination
				if(tB<0) // no solution
					return pH->EndFunction();
				
				float radiusd = 2*radius;
				float t = 0;
				Vec3 P(A + VelN * radius); // start point for collision check
				Vec3 Vb(V.x,V.y, -g*tB + V.z); // velocity in end point B
				float speedB = Vb.GetLength();
				if(speedB==0)
					return pH->EndFunction();

				float tB1 = tB - radiusd/speedB; // time to reach approximate point to end collision check

				float tStep = tB/4;

				//Vec3 Q(B - Vb.GetNormalizedSafe() * radiusd); // end point for collision check
				
				float zOffset = bodyInfo.colliderSize.GetCenter().z;
				P.z += zOffset;
				
				ICVar* aiDeBugDraw = gEnv->pConsole->GetCVar("ai_DebugDraw");
				if(aiDeBugDraw && aiDeBugDraw->GetIVal())
				{
					float step = tB/40;
					for(float t2= 0;t2<tB;t2+=step)
					{
						Vec3 P1 = getPointOnParabula(A,V,t2,g);
						Vec3 P2 = getPointOnParabula(A,V,t2+step,g);
						gEnv->pAISystem->AddDebugLine(P1,P2,255,255,0,6);
					}
				}

				ray_hit hit;

				for(int i=0;i<4;i++)
				{
					float t1 = t + tStep;
//					Vec3 P1 = getPointOnParabula(A,V,t1,g);
					if(tB1 < t1 || i==3 && tB1>=t1) // next point will be end point
					{
						i = 4; // last loop
						t1 = tB1;
						//P1 = Q;
					}
					Vec3 P1 = getPointOnParabula(A,V,t1,g);
					Vec3 Pr(P.x,P.y, P.z - zOffset); 
					if( RayWorldIntersectionWrapper( Pr, P1 - Pr, ent_static|ent_terrain|ent_living, rwi_stop_at_pierceable, &hit, 1 ) )
						return pH->EndFunction();
					P1.z += zOffset;
					Lineseg T(P ,P1 );
					gEnv->pAISystem->AddDebugLine(P,P1,40,40,255,8);
					if(IntersectSweptSphere(&hitPos, hitdist, T, radius, nSkipEnts ? SkipEnt:NULL, nSkipEnts,ent_sleeping_rigid))
						return pH->EndFunction();
					P = P1;	
					t = t1;
				}

			}
			else //theta <=0
			{
				// TO DO not supported yet
				// ZMax calculations above won't be valid here

			}

		}

		gEnv->pAISystem->AddDebugLine(A,A+VelN*4,255,255,255,5);
		/*
		IEntityPhysicalProxy *pPhysicsProxy = (IEntityPhysicalProxy*)pEntity->GetProxy(ENTITY_PROXY_PHYSICS);
		if (pPhysicsProxy)
		{
		IPhysicalEntity *phys = pEntity->GetPhysics();
		if(phys)
		{
		pe_status_dynamics	dyn;
		phys->GetStatus(&dyn);
		float mass = dyn.mass;
		pPhysicsProxy->AddImpulse( -1,ZERO,V*mass,false,1);
		}
		}
		*/

		retVelocity->SetValue( "x", V.x );
		retVelocity->SetValue( "y", V.y );
		retVelocity->SetValue( "z", V.z );

		// compute duration
		if(tB==0)
		{
			float a = -g/2;
			float b = V.z;
			float c = A.z - B.z;
			float d = b*b - 4*a*c;
			float sqrtd = (x==0? 0:sqrt(d));
			tB = (-b + sqrtd)/ (2*a);
			float tB1 = (-b - sqrtd)/ (2*a);
			float y = V.y*tB + A.y;
			float y1 = V.y*tB1 + A.y;
			if(fabs(B.y - y) > fabs(B.y - y1) )
				tB = tB1;
		}

		return pH->EndFunction(tB);
	}
	return pH->EndFunction();
}

// 
//====================================================================
int CScriptBind_AI::CanJumpToPoint(IFunctionHandler * pH)
{
	if (pH->GetParamCount() < 6)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint: Not enough parameters." );
		return pH->EndFunction();
	}
	GET_ENTITY(1);
	// Get bbox (status) from physics engine
	if(!pEntity)
		return pH->EndFunction();
	IEntity* pEntity2 = NULL;

	//	IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();
	//	if(pPhysEntity)
	{
		//		float radius;
		//		pe_status_pos status;

		float theta=0; // initial angle
		float tB=0; // returned time at which entity will reach B
		float maxheight; // maximum height allowed
		Vec3 B(ZERO); // final position (parameter)
		Vec3 V(ZERO); // returned initial velocity
		IAIObject* pAI = pEntity->GetAI(); 
		if(!pAI)
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint: Not enough parameters." );
			return pH->EndFunction();
		}

		IPuppetProxy* pPuppetProxy =  0;
		if (!pAI->GetProxy() || !pAI->GetProxy()->QueryProxy(AIPROXY_PUPPET, (void**)&pPuppetProxy))
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint(): Entity '%s' does not have puppet AI proxy.", pEntity->GetName());
			return pH->EndFunction();
		}

		//		pPhysEntity->GetStatus( &status );

		SmartScriptTable	retVelocity;

		ScriptVarType type = pH->GetParamType(2);
		switch(type)
		{
		case svtString:
			if(pAI)
			{
				IPipeUser* pPipeUser = pAI->CastToIPipeUser();
				if(pPipeUser)
				{
					const char* name;
					if(pH->GetParam(2,name))
					{
						IAIObject* pAItarget = pPipeUser->GetSpecialAIObject(name);
						if(pAItarget)
							B = pAItarget->GetPos();
					}
				}
			}
			break;
		case svtPointer:
			{
				ScriptHandle hdl;
				pH->GetParam(2,hdl);
				int nID = hdl.n;
				pEntity2 = m_pSystem->GetIEntitySystem()->GetEntity(nID);
				if(pEntity2)
					B = pEntity2->GetWorldPos();
			}
			break;
		case svtObject:
			{
				SmartScriptTable vecTable;
				if(pH->GetParam(2,vecTable))
				{	
					float x,y,z;
					if(vecTable->GetValue("x",x) && vecTable->GetValue("y",y) && vecTable->GetValue("z",z))
						B.Set(x,y,z);
				}
			}
			break;
		default:
			break;

		}

		if(B.IsZero())
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint: wrong parameter 2 format" );
			return pH->EndFunction();
		}
		else if(_isnan(B.x+B.y+B.z))
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CanJumpToPoint: NAN in destination position" );
			return pH->EndFunction();
		}


		int flags;
		if( !pH->GetParam( 6, retVelocity )) 
			return pH->EndFunction();

		pH->GetParam(5,flags);

		pH->GetParam(3,theta);

		pH->GetParam(4,maxheight);


		IPhysicalEntity* pSkipEnt = NULL;

		if (pH->GetParamCount() >6)
		{
			if(pH->GetParamType(7)==svtPointer)
			{
				ScriptHandle hdl;
				pH->GetParam(7,hdl);
				int nID = hdl.n;
				IEntity* pEntity2 = m_pSystem->GetIEntitySystem()->GetEntity(nID);
				if(pEntity2)
					pSkipEnt = pEntity2->GetPhysics();
			}
		}
		Vec3 altStartPos;
		Vec3* pAltStartPos = NULL;
		if (pH->GetParamCount() >7 && pH->GetParamType(8)!=svtNull)
		{
			pH->GetParam(8,altStartPos);
			pAltStartPos = &altStartPos;
		}

		
//		if(gEnv->pAISystem->CanJumpToPoint(pAI, B, theta, maxheight, flags,V, tB, pSkipEnt, pAltStartPos))
		if(pPuppetProxy->CanJumpToPoint(B, theta, maxheight, flags,V, tB, pSkipEnt, pAltStartPos))
		{

			retVelocity->SetValue( "x", V.x );
			retVelocity->SetValue( "y", V.y );
			retVelocity->SetValue( "z", V.z );

			return pH->EndFunction(tB);
		}
	}
	return pH->EndFunction();
}

// 
//====================================================================
int CScriptBind_AI::SetRefpointToAlienHidespot(IFunctionHandler * pH)
{
#ifndef SP_DEMO
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction(0);	
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction(0);	
	IPipeUser* pPipeUser = pAIObject->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction(0);

	float rangeMin = 0.0f;
	float rangeMax = 1.0f;
	pH->GetParam(2,rangeMin);
	pH->GetParam(3,rangeMax);

	const int ALIEN_HIDESPOT = 504;
	const int COMBAT_TERRITORY = 342;

	Vec3 nearestPos(0,0,0);
	float nearestDist = FLT_MAX;

	const Vec3& searchPos = pAIObject->GetPos();
	Vec3 targetPos(searchPos);
	Vec3 targetDir(0,0,0);
	bool targetDirValid = false;
	if (IAIObject* pTarget = pPipeUser->GetAttentionTarget())
	{
		targetPos = pTarget->GetPos();
		if (pTarget->IsAgent())
		{
			targetDir = pTarget->GetViewDir();
			targetDirValid = true;
		}
	}

	// update unavailable hidespots
	CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
	for (unsigned i = 0; i < m_unreachableAlienHideSpots.size(); )
	{
		float dt = (curTime - m_unreachableAlienHideSpots[i].t).GetSeconds();
		// If the unreachable pos is older than 15s or in the future (can happen because of QL/QS), remove it.
		if (dt > 15.0f || dt < -0.00001f)
		{
			m_unreachableAlienHideSpots[i] = m_unreachableAlienHideSpots.back();
			m_unreachableAlienHideSpots.pop_back();
		}
		else
			++i;
	}

	std::vector<Vec3> occupiedPos;

	AutoAIObjectIter itBuddy(m_pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, AIOBJECT_PUPPET, searchPos, rangeMax + 1.0f, false));
	for (; itBuddy->GetObject(); itBuddy->Next())
	{
		IAIObject* pBuddyAI = itBuddy->GetObject();
		if (pBuddyAI == pAIObject) continue;
		if (!pBuddyAI->IsEnabled()) continue;
		if (!pBuddyAI->GetEntity()) continue;
		IPipeUser* pBuddyPipeUser = pBuddyAI->CastToIPipeUser();
		if (!pBuddyPipeUser)
			continue;

		occupiedPos.push_back(pBuddyAI->GetPos());

		if (!pBuddyAI->IsHostile(pAIObject))
			occupiedPos.push_back(pBuddyPipeUser->GetRefPoint()->GetPos());
	}

	int nBuildingID = -1;
	IVisArea* pGoalArea; 
	gEnv->pAISystem->CheckNavigationType(pAIObject->GetPos(), nBuildingID, pGoalArea, IAISystem::NAV_VOLUME);

//	AutoAIObjectIter it(m_pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, ALIEN_HIDESPOT, searchPos, rangeMax, false));

	IAIObjectIter* it = 0;

	const char* szShapeName = m_pAISystem->GetEnclosingGenericShapeOfType(pAIObject->GetPos(), COMBAT_TERRITORY, true);
	if (szShapeName)
		it = m_pAISystem->GetFirstAIObjectInShape(IAISystem::OBJFILTER_TYPE, ALIEN_HIDESPOT, szShapeName, true);
	else
		it = m_pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, ALIEN_HIDESPOT, searchPos, rangeMax, false);

	if (!it)
		return pH->EndFunction(0);

	EntityId userId = pEntity->GetId();
	Vec3 refPointPos = pPipeUser->GetRefPoint()->GetPos();

	for (; it->GetObject(); it->Next())
	{
		IAIObject*	obj = it->GetObject();
		if (!obj->IsEnabled()) continue;

		const Vec3& pos = obj->GetPos();

		float d = Distance::Point_PointSq(searchPos, pos);
		if (d < sqr(rangeMin))
			continue;

		// Dont use same point twice in a row.
		if (Distance::Point_PointSq(refPointPos, pos) < 0.5f)
			continue;

		if (nBuildingID != -1)
		{
			if (!gEnv->pAISystem->IsPointInBuilding(pos, nBuildingID))
				continue;
		}

		bool unreachable = false;
		for (unsigned i = 0, ni = m_unreachableAlienHideSpots.size(); i < ni; ++i)
		{
			if (m_unreachableAlienHideSpots[i].entId != userId) continue;
			if (Distance::Point_PointSq(m_unreachableAlienHideSpots[i].pos, pos) < sqr(0.5f))
			{
				unreachable = true;
				break;
			}
		}
		if (unreachable)
			continue;

		bool occupied = false;
		for (unsigned i = 0, ni = occupiedPos.size(); i < ni; ++i)
		{
			if (Distance::Point_PointSq(occupiedPos[i], pos) < sqr(2.0f))
			{
				occupied = true;
				break;
			}
		}
		if (occupied)
			continue;

		if (targetDirValid)
		{
			Vec3 delta = pos - targetPos;
			delta.Normalize();
			float dot = targetDir.Dot(delta);
			const float thr = cosf(DEG2RAD(15));
			if (dot > thr)
			{
				d += sqr(500.0f);
				gEnv->pAISystem->AddDebugLine(pos, (pos+searchPos)*0.5f, 255,196,0, 5.0f);
			}
		}

		if (d < nearestDist)
		{
			ray_hit	hit;
			if (RayWorldIntersectionWrapper(pos, targetPos-pos, ent_static|ent_terrain, rwi_stop_at_pierceable, &hit, 1))
			{
//				gEnv->pAISystem->AddDebugLine(searchPos, hit.pt, 255,0,0, 10.0f);
				nearestDist = d;
				nearestPos = pos;
			}
		}
	}
	SAFE_RELEASE(it);

	if (!nearestPos.IsZero())
	{
		gEnv->pAISystem->AddDebugLine(searchPos, nearestPos, 255,255,255, 10.0f);
		pPipeUser->SetRefPointPos(nearestPos);
		return pH->EndFunction(1);
	}
#endif
	return pH->EndFunction(0);
}

// 
//====================================================================
int CScriptBind_AI::MarkAlienHideSpotUnreachable(IFunctionHandler * pH)
{
#ifndef SP_DEMO
	SCRIPT_CHECK_PARAMETERS(1);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction(0);	
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction(0);	
	IPipeUser* pPipeUser = pAIObject->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction(0);

	Vec3 pos = pPipeUser->GetRefPoint()->GetPos();
	EntityId entId = pEntity->GetId();
	CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();

	// If the spot already exists, just update time.
	for (unsigned i = 0, ni = m_unreachableAlienHideSpots.size(); i < ni; ++i)
	{
		if (m_unreachableAlienHideSpots[i].entId == entId && Distance::Point_PointSq(pos, m_unreachableAlienHideSpots[i].pos) < sqr(0.1f))
		{
			m_unreachableAlienHideSpots[i].t = curTime;
			return pH->EndFunction(0);
		}
	}

	// Add new spot
	m_unreachableAlienHideSpots.push_back(SUnreachableAlienHideSpot(curTime, pos, entId));
#endif

	return pH->EndFunction(0);
}

// 
//====================================================================
int CScriptBind_AI::SetRefpointToAnchor(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(5);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();	
	IAIObject* pAIObject = pEntity->GetAI();
	if(!pAIObject)
		return pH->EndFunction();	
	IPipeUser* pPipeUser = pAIObject->CastToIPipeUser();
	if (!pPipeUser)
		return pH->EndFunction();

	float rangeMin = 0.0f;
	float rangeMax = 1.0f;
	pH->GetParam(2,rangeMin);
	pH->GetParam(3,rangeMax);

	int	findType = 0;
	pH->GetParam(4,findType);

	int	findMethod = AIANCHOR_NEAREST;
	pH->GetParam(5,findMethod);

	bool	bSeesTarget	= (findMethod & AIANCHOR_SEES_TARGET)!=0;
	bool	bBehind	= (findMethod & AIANCHOR_BEHIND)!=0;
	findMethod &= ~(AIANCHOR_SEES_TARGET | AIANCHOR_BEHIND);

	Vec3 nearestPos(0,0,0);
//	float nearestDist = FLT_MAX;
	float	bestCandidatreDist( findMethod==AIANCHOR_FARTHEST ? FLT_MIN : FLT_MAX );

	const Vec3& searchPos = pAIObject->GetPos();
	Vec3 targetPos(searchPos);
	if (pPipeUser->GetAttentionTarget())
		targetPos = pPipeUser->GetAttentionTarget()->GetPos();

	AutoAIObjectIter it(m_pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, findType, searchPos, rangeMax, false));
	for (; it->GetObject(); it->Next())
	{
		IAIObject*	obj = it->GetObject();
		if (!obj->IsEnabled()) continue;

		const Vec3& pos = obj->GetPos() + Vec3(0.f,0.f,.8f); // put it a bit up - normally anchor would be on the ground. BAD for aliens/3D?

		float d = Distance::Point_PointSq(searchPos, pos);
		if (d < sqr(rangeMin))
			continue;

		if( bBehind )
		{
			Vec3 toTarget(targetPos-searchPos);
			Vec3 toAnchor(pos-searchPos);
			toTarget.Normalize();
			toAnchor.Normalize();
			float dotProd(toTarget*toAnchor);
			if(dotProd > 0.f)//-.5f)
				continue;
		}

		if( findMethod==AIANCHOR_FARTHEST && d>bestCandidatreDist ||
				findMethod!=AIANCHOR_FARTHEST && d<bestCandidatreDist )
		{
			ray_hit	hit;
			int hitsCount = RayWorldIntersectionWrapper(pos, targetPos-pos, ent_static|ent_terrain|ent_sleeping_rigid|ent_rigid, 
														rwi_stop_at_pierceable, &hit, 1);
			if(bSeesTarget&&hitsCount==0 || !bSeesTarget&&hitsCount)
			{
				gEnv->pAISystem->AddDebugLine(searchPos, hit.pt, 255,0,0, 10.0f);
				bestCandidatreDist = d;
				nearestPos = pos;
			}
		}
	}

	if (!nearestPos.IsZero())
	{
		gEnv->pAISystem->AddDebugLine(searchPos, nearestPos, 255,255,255, 10.0f);
		pPipeUser->SetRefPointPos(nearestPos);
		return pH->EndFunction(true);
	}

	return pH->EndFunction(false);
}

// 
//====================================================================
int CScriptBind_AI::SetRefpointToPunchableObject(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if(!pEntity)
		return pH->EndFunction();	
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();	
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser || !pPipeUser->GetAttentionTarget())
		return pH->EndFunction();

	float range = 10.0f;
	pH->GetParam(2,range);

/*	SmartScriptTable	pTgtPos;
	if (!pH->GetParam(3, pTgtPos))
		return pH->EndFunction();*/

	Vec3 posOut;
	Vec3 dirOut;
	IEntity* objEntOut = 0;

	if (!m_pAISystem->GetNearestPunchableObjectPosition(pAI, pAI->GetPos(), range, pPipeUser->GetAttentionTarget()->GetPos(),
		0.5f, 4.0f, 10.0f, 1550.0f, posOut, dirOut, &objEntOut))
		return pH->EndFunction();

	if (!objEntOut)
		return pH->EndFunction();

	pPipeUser->SetRefPointPos(posOut, dirOut);

	m_pAISystem->AddDebugLine(posOut, posOut+Vec3(0,0,4), 255,255,255, 10.0f);
	m_pAISystem->AddDebugLine(posOut+Vec3(0,0,1), posOut+Vec3(0,0,1)+dirOut, 255,255,255, 10.0f);

	return pH->EndFunction(objEntOut->GetScriptTable());
}

// 
//====================================================================
int CScriptBind_AI::MeleePunchableObject(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	ScriptHandle hdl;
	if (!pH->GetParam(1, hdl))
		return pH->EndFunction(0);
	IEntity* pUser = m_pSystem->GetIEntitySystem()->GetEntity(hdl.n);
	if (!pUser)
	{
		gEnv->pAISystem->LogComment( "<CScriptBind_AI::MeleePunchableObject> ", "ERROR: Specified entity id (param 1) not found!" );
		return pH->EndFunction(0);
	}

	if (!pH->GetParam(2, hdl))
		return pH->EndFunction(0);
	IEntity* pObject = m_pSystem->GetIEntitySystem()->GetEntity(hdl.n);
	if (!pObject)
	{
		gEnv->pAISystem->LogComment( "<CScriptBind_AI::MeleePunchableObject> ", "ERROR: Specified entity id (param 2) not found!" );
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pUser->GetAI();
	if (!pAI)
		return pH->EndFunction(0);	
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser || !pPipeUser->GetAttentionTarget())
		return pH->EndFunction(0);

	// Make sure the AI is on spot
	const float dirThr = cosf(DEG2RAD(45.0f));
	const Vec3& standPos = pPipeUser->GetRefPoint()->GetPos();
	const Vec3& standDir = pPipeUser->GetRefPoint()->GetBodyDir();
	if (Distance::Point_Point2D(standPos, pAI->GetPos()) > 0.4f || standDir.Dot(pAI->GetBodyDir()) < dirThr)
		return pH->EndFunction(0);

	Vec3 origPos;
	if (!pH->GetParam(3, origPos))
		return pH->EndFunction(0);

	if (Distance::Point_PointSq(pObject->GetWorldPos(), origPos) < 0.5f)
	{
		IEntityPhysicalProxy* pPhysicsProxy = (IEntityPhysicalProxy*)pObject->GetProxy(ENTITY_PROXY_PHYSICS);
		if (pPhysicsProxy && pObject->GetPhysics())
		{
			pe_status_dynamics	dyn;

			pObject->GetPhysics()->GetStatus(&dyn);
			float mass = dyn.mass;

			Vec3 vel = pPipeUser->GetAttentionTarget()->GetPos() - pAI->GetPos(); // standDir;
			vel.Normalize();
			vel += pAI->GetBodyDir();
			vel.z += 0.2f;
			vel.Normalize();
			vel *= 18.0f;

			Vec3	pt = standPos;
			pt.x += (1 - cry_frand()*2)*0.3f;
			pt.y += (1 - cry_frand()*2)*0.3f;
			pt.z += 0.6f; // + (1 - ai_frand()*2) * 0.3f;
			pPhysicsProxy->AddImpulse(-1, pt, vel * mass, true, 1);

			return pH->EndFunction(1);
		}
	}

	return pH->EndFunction(0);
}


// 
//====================================================================
int CScriptBind_AI::IsPunchableObjectValid(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);

	ScriptHandle hdl;
	if (!pH->GetParam(1, hdl))
		return pH->EndFunction(0);
	IEntity* pUser = m_pSystem->GetIEntitySystem()->GetEntity(hdl.n);
	if (!pUser)
	{
		gEnv->pAISystem->LogComment( "<CScriptBind_AI::MeleePunchableObject> ", "ERROR: Specified entity id (param 1) not found!" );
		return pH->EndFunction(0);
	}

	if (!pH->GetParam(2, hdl))
		return pH->EndFunction(0);
	IEntity* pObject = m_pSystem->GetIEntitySystem()->GetEntity(hdl.n);
	if (!pObject)
	{
		gEnv->pAISystem->LogComment( "<CScriptBind_AI::MeleePunchableObject> ", "ERROR: Specified entity id (param 2) not found!" );
		return pH->EndFunction(0);
	}

	IAIObject* pAI = pUser->GetAI();
	if (!pAI)
		return pH->EndFunction(0);	
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser || !pPipeUser->GetAttentionTarget())
		return pH->EndFunction(0);

	// Make sure the AI is on spot
	const float dirThr = cosf(DEG2RAD(45.0f));
	const Vec3& standPos = pPipeUser->GetRefPoint()->GetPos();
	const Vec3& standDir = pPipeUser->GetRefPoint()->GetBodyDir();
	if (Distance::Point_Point2D(standPos, pUser->GetWorldPos()) > 0.4f || standDir.Dot(pAI->GetBodyDir()) < dirThr)
		return pH->EndFunction(0);

	Vec3 origPos;
	if (!pH->GetParam(3, origPos))
		return pH->EndFunction(0);

	if (Distance::Point_PointSq(pObject->GetWorldPos(), origPos) < 0.5f)
	{
		return pH->EndFunction(1);
	}

	return pH->EndFunction(0);
}



struct SSortedAIObject
{
	SSortedAIObject(IAIObject* pObj) : pObj(pObj), w(0) {}
	bool operator<(const SSortedAIObject& rhs) const { return w < rhs.w; }
	IAIObject* pObj;
	float w;
};

// 
//====================================================================
bool CScriptBind_AI::GetGroupSpatialProperties(IAIObject* pAI, float& offset, Vec3& avgGroupPos, Vec3& targetPos, Vec3& dirToTarget, Vec3& normToTarget)
{
	IPipeUser* pPipeUser = pAI->CastToIPipeUser();
	if (!pPipeUser)
		return false;

	int groupId = pAI->GetGroupId();

	const Vec3& requesterPos = pAI->GetPos();

	targetPos.Set(0,0,0);
	if (pPipeUser->GetAttentionTarget())
		targetPos = pPipeUser->GetAttentionTarget()->GetPos();
	else if (IAIObject* pBeacon = m_pAISystem->GetBeacon(groupId))
		targetPos = pBeacon->GetPos();

	if (targetPos.IsZero())
		targetPos = requesterPos + pAI->GetBodyDir() * 30.0f;

	avgGroupPos.Set(0,0,0);
	float	avgGroupPosWeight = 0;

	std::vector<SSortedAIObject> group;

	AutoAIObjectIter it(m_pAISystem->GetFirstAIObject(IAISystem::OBJFILTER_GROUP, groupId));
	for (; it->GetObject(); it->Next())
	{
		IAIObject* pObj = it->GetObject();
		if (!pObj->IsEnabled()) continue;

		IPuppetProxy* pPuppetProxy =  0;
		if (pObj->GetProxy() && pObj->GetProxy()->QueryProxy(AIPROXY_PUPPET, (void**)&pPuppetProxy))
		{
			if (pPuppetProxy->IsCurrentBehaviorExclusive())
				continue;
		}

		const Vec3& pos = pObj->GetPos();

		avgGroupPos += pos;
		avgGroupPosWeight += 1.0f;

		group.push_back(SSortedAIObject(pObj));
	}

	if (avgGroupPosWeight > 0.0f)
		avgGroupPos /= avgGroupPosWeight;
	else
		return false;

	dirToTarget = targetPos - requesterPos;
	dirToTarget.z = 0;
	dirToTarget.Normalize();
	normToTarget.Set(dirToTarget.y, -dirToTarget.x, 0);

	for (unsigned i = 0, ni = group.size(); i < ni; ++i)
		group[i].w = normToTarget.Dot(group[i].pObj->GetPos() - avgGroupPos);

	std::sort(group.begin(), group.end());

	unsigned selfIdx = 0;
	for (unsigned i = 0, ni = group.size(); i < ni; ++i)
	{
		if (group[i].pObj == pAI)
		{
			selfIdx = i;
			break;
		}
	}

	offset = (float)selfIdx - group.size()*0.5f;

	return true;
}

// 
//====================================================================
int CScriptBind_AI::GetDirLabelToPoint(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction(-1);	
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction(-1);

	Vec3	pos;
	if (!pH->GetParam(2, pos))
		return pH->EndFunction(-1);

	Vec3 forw = pAI->GetBodyDir();
	forw.z = 0;
	forw.NormalizeSafe();
	Vec3 right(forw.y, -forw.x, 0);

	Vec3 dir = pos - pAI->GetPos();
	dir.NormalizeSafe();

	const float upThr = sinf(DEG2RAD(20.0f));
	if (dir.z > upThr)
		return pH->EndFunction(4); // up

	float x = forw.x*dir.x + forw.y *dir.y;
	float y = right.x*dir.x + right.y *dir.y;

	float angle = atan2f(y, x);

	// Prefer sides slightly
	if (fabsf(angle) <= gf_PI*0.2f)
		return pH->EndFunction(0); // front
	else if (fabsf(angle) >= gf_PI*0.8f)
		return pH->EndFunction(1); // back
	else if (angle < 0)
		return pH->EndFunction(2); // left
	else
		return pH->EndFunction(3); // right
}


// 
//====================================================================
int CScriptBind_AI::GetPredictedPosAlongPath(IFunctionHandler * pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);
	if (!pEntity)
		return pH->EndFunction();	
	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return pH->EndFunction();

	IPuppet* pPuppet = pAI->CastToIPuppet();
	if(!pPuppet)
		return pH->EndFunction();

	float time;
	if (!pH->GetParam(2, time))
		return pH->EndFunction();

	SmartScriptTable	retPosTable;
	Vec3 retPos;

	if (!pH->GetParam(3, retPosTable))
		return pH->EndFunction();
	
	pe_status_dynamics dyn;
	IPhysicalEntity* phys = pEntity->GetPhysics();
	if(!phys)
		return pH->EndFunction();
	phys->GetStatus(&dyn);

	if (pPuppet->GetPosAlongPath(time*dyn.v.GetLength(), false, retPos))
	{
		retPosTable->SetValue("x",retPos.x);
		retPosTable->SetValue("y",retPos.y);
		retPosTable->SetValue("z",retPos.z);
		return pH->EndFunction(true);
	}

	return pH->EndFunction();

}

//====================================================================
// GetInterestStatus
//====================================================================
int CScriptBind_AI::GetInterestStatus(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	// Check entity
	if( pEntity == NULL )
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetInterestStatus(): no entity given parameter 1");
		return pH->EndFunction();
	}

	// Check and grab table
	SmartScriptTable statusTable;
	if (! pH->GetParam(2,statusTable))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetInterestStatus(): no table given parameter 2");
		return pH->EndFunction();
	}

	// Get the AI
	IAIObject *pObject = pEntity->GetAI();
	IAIActor *pAIActor = ( pObject ? CastToIAIActorSafe( pObject ) : NULL );
	if (!pAIActor)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetInterestStatus(): entity is not at AIActor");
		return pH->EndFunction();
	}

	// Get CIM, PIM
	ICentralInterestManager *pCIM = gEnv->pAISystem->GetCentralInterestManager();
	IPersonalInterestManager *pPIM = pAIActor->GetInterestManager();

	// Check if there is no interest information relevant to this AI
	if (!pPIM) return pH->EndFunction();	// Perfectly normal exit conditions

	// Clear then populate table
	IEntity *pIntEnt;
	statusTable->Clear();
	statusTable->SetValue("enabled", pCIM->IsEnabled()); // Still have to settle CVAR activation here 
	statusTable->SetValue("minInterest", pPIM->GetMinimumInterest() );
	if (pPIM->IsInterested())
	{
		pIntEnt = pPIM->GetInterestEntity();
		statusTable->SetValue("isInterested", true);
		if (pIntEnt)	
		{
			statusTable->SetValue("interestEntityId", pIntEnt->GetId());
			statusTable->SetValue("distance", pIntEnt->GetPos().GetDistance(pEntity->GetPos()));
		}
	}

	return pH->EndFunction(statusTable);
}



//====================================================================
// SetInterestStatus
//====================================================================
int CScriptBind_AI::SetInterestStatus(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	// Check entity
	if( pEntity == NULL )
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetInterestStatus(): no entity given parameter 1");
		return pH->EndFunction();
	}

	// Check and grab table
	SmartScriptTable statusTable;
	if (! pH->GetParam(2,statusTable))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetInterestStatus(): no table given parameter 2");
		return pH->EndFunction();
	}

	// Get the AI
	IAIObject *pObject = pEntity->GetAI();
	IAIActor *pAIActor = ( pObject ? CastToIAIActorSafe( pObject ) : NULL );
	if (!pAIActor)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetInterestStatus(): entity is not at AIActor");
		return pH->EndFunction();
	}

	// Get CIM, PIM
	ICentralInterestManager *pCIM = gEnv->pAISystem->GetCentralInterestManager();
	IPersonalInterestManager *pPIM = pAIActor->GetInterestManager();

	float fValue;
	statusTable->GetValue("minInterest", fValue);
	pPIM->SetMinimumInterest(fValue);

	return pH->EndFunction(true);
}

//====================================================================
// CanMoveStraightToPoint
//====================================================================
int CScriptBind_AI::CanMoveStraightToPoint(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	Vec3 posFrom,posTo;
	IAIObject* pAI;
	GET_ENTITY(1);
	if(pEntity && (pAI = pEntity->GetAI()))
	{
		IAIActor* pAIActor = pAI->CastToIAIActor();
		if(pAIActor)
		{
			IAISystem::ENavigationType navTypeFrom;
			IAISystem::tNavCapMask navCap = pAIActor->GetMovementAbility().pathfindingProperties.navCapMask;
			const float passRadius = pAIActor->GetParameters().m_fPassRadius;
			posFrom = pAI->GetPos();
			if(pH->GetParam(2,posTo))
			{
				bool res = m_pAISystem->IsSegmentValid(navCap,passRadius,posFrom,posTo,navTypeFrom);
				return pH->EndFunction(res);
			}
			else
				m_pAISystem->Warning("<CScriptBind_AI> ", "CanMoveStraightToPoint(): wrong parameter 2 format");
		}
		else
			m_pAISystem->Warning("<CScriptBind_AI> ", "CanMoveStraightToPoint(): entity is not at AIActor");
	}
	else
		m_pAISystem->Warning("<CScriptBind_AI> ", "CanMoveStraightToPoint(): first parameter has no AI");

	return pH->EndFunction();
}

//====================================================================
// PlayReadabilitySound
//====================================================================
int CScriptBind_AI::PlayReadabilitySound(IFunctionHandler *pH)
{
//	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	// Check entity
	if (!pEntity)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "PlayReadabilitySound(): no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "PlayReadabilitySound(): Entity '%s' does not have AI.", pEntity->GetName());
		return pH->EndFunction();
	}
	IPuppetProxy* pPuppetProxy =  0;
	if (!pAI->GetProxy() || !pAI->GetProxy()->QueryProxy(AIPROXY_PUPPET, (void**)&pPuppetProxy))
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "PlayReadabilitySound(): Entity '%s' does not have puppet AI proxy.", pEntity->GetName());
		return pH->EndFunction();
	}

	const char* szReadability = 0;
	if (!pH->GetParam(2, szReadability))
		return pH->EndFunction();
	bool stopPreviousSounds = false;
	if (pH->GetParamCount() > 2)
		if (!pH->GetParam(3, stopPreviousSounds))
			return pH->EndFunction();

	int soundId = pPuppetProxy->PlayReadabilitySound(szReadability, stopPreviousSounds);

	if (soundId != INVALID_SOUNDID)
		return pH->EndFunction(ScriptHandle(soundId));
	else
		return pH->EndFunction();
}

//====================================================================
// EnableWeaponAccessory
//====================================================================
int CScriptBind_AI::EnableWeaponAccessory(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(1);

	// Check entity
	if (!pEntity)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "EnableWeaponAccessory(): no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
	if (!pAI || !pAIActor)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "EnableWeaponAccessory(): Entity '%s' does not have AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	int accessory = 0;
	if (!pH->GetParam(2, accessory))
		return pH->EndFunction();

	bool state = 0;
	if (!pH->GetParam(3, state))
		return pH->EndFunction();

	AgentParameters	params = pAIActor->GetParameters();

	if (state)
		params.m_weaponAccessories |= accessory;
	else
		params.m_weaponAccessories &= ~accessory;

	pAIActor->SetParameters(params);

	return pH->EndFunction();

}

//====================================================================
// AutoDisable
//====================================================================
int CScriptBind_AI::AutoDisable(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	GET_ENTITY(1);

	// Check entity
	if (!pEntity)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "EnableWeaponAccessory(): no entity given parameter 1");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI || !pAI->GetProxy())
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "AutoDisable(): Entity '%s' does not have AI.", pEntity->GetName());
		return pH->EndFunction();
	}

	int enable = 0;
	if (!pH->GetParam(2, enable))
		return pH->EndFunction();
	CAIProxy*	pProxy = (CAIProxy*)pAI->GetProxy();
	if(enable==0)
		pProxy->UpdateMeAlways(true);
	else
		pProxy->UpdateMeAlways(false);

	return pH->EndFunction();
}

//====================================================================
// AggressionImpact
//====================================================================
int CScriptBind_AI::AggressionImpact(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);
	ScriptHandle hdl;
	pH->GetParam(1,hdl);
	int nID = hdl.n;
	IEntity* pEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);
	pH->GetParam(2, hdl);
	nID = hdl.n;
	IEntity* pTarget = m_pSystem->GetIEntitySystem()->GetEntity(nID);

	// Check entity
	if (!pEntity)
	{
		//m_pAISystem->Warning("<CScriptBind_AI> ", "AggressionImpact(): no entity given parameter 1");
		return pH->EndFunction();
	}

	if (!pTarget)
	{
		//m_pAISystem->Warning("<CScriptBind_AI> ", "AggressionImpact(): no entity given parameter 2");
		return pH->EndFunction();
	}

	gEnv->pAISystem->AggressionImpact(pEntity, pTarget);

	return pH->EndFunction();
}

//====================================================================
// CheckMeleeDamage
//====================================================================
int CScriptBind_AI::CheckMeleeDamage(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(6);

	ScriptHandle hdl;
	ScriptHandle hdl2;
	float radius,minheight,maxheight,angle;
	if(pH->GetParams(hdl,hdl2,radius,minheight,maxheight,angle))
	{
		int nID = hdl.n;
		IEntity* pEntity = m_pSystem->GetIEntitySystem()->GetEntity(nID);

		// Check entity
		if (!pEntity)
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "CheckMeleeDamage(): no entity given parameter 1");
			return pH->EndFunction();
		}

		IAIObject* pAI = pEntity->GetAI();
		if (!pAI)
		{
			gEnv->pAISystem->Warning("<CScriptBind_AI> ", "CheckMeleeDamage(): Entity '%s' does not have AI.", pEntity->GetName());
			return pH->EndFunction();
		}
		IAIActor* pAIActor = pAI->CastToIAIActor();
		bool b3d = pAIActor && pAIActor->GetMovementAbility().b3DMove;

		int nID2 = hdl2.n;
		IEntity* pEntityTarget = m_pSystem->GetIEntitySystem()->GetEntity(nID2);
		if (!pEntityTarget)
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "CheckMeleeDamage(): no target entity given parameter 2");
			return pH->EndFunction();
		}

		angle = DEG2RAD(angle);
		Vec3 mydir(pEntity->GetWorldTM().TransformVector(Vec3(0,1,0)));
		Vec3 targetdir(pEntityTarget->GetWorldPos() - pEntity->GetWorldPos());

		Vec3 mydirN(mydir.GetNormalizedSafe());
		Vec3 targetdirN(targetdir.GetNormalizedSafe());
		
		float dot= mydirN.Dot(targetdirN);
		float myangle = cry_acosf(CLAMP(dot,-1,1));
		if(myangle>angle/2)
			return pH->EndFunction();
		
		float dist = b3d ? targetdir.GetLength(): targetdir.GetLength2D();
		if(dist > radius)
			return pH->EndFunction();

		if(targetdir.z < minheight || targetdir.z > maxheight )
			return pH->EndFunction();

		return pH->EndFunction(dist,RAD2DEG(myangle));
	}

	return pH->EndFunction();
}

//====================================================================
// IsMountedWeaponUsableWithTarget
//====================================================================
int CScriptBind_AI::IsMountedWeaponUsableWithTarget(IFunctionHandler *pH)
{
	int paramCount = pH->GetParamCount();
	if(paramCount<2)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsTargetAimableWithMG(): too few parameters.");
		return pH->EndFunction();
	}

	GET_ENTITY(1);

	if(!pEntity)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsTargetAimableWithMG(): wrong entity id in parameter 1.");
		return pH->EndFunction();
	}

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsTargetAimableWithMG(): Entity '%s' does not have AI.", pEntity->GetName());
		return pH->EndFunction();
	}
	
	EntityId itemEntityId;
	ScriptHandle hdl2;

	if(!pH->GetParam(2,hdl2))
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsTargetAimableWithMG(): wrong parameter 2 format.");
		return pH->EndFunction();
	}

	itemEntityId = hdl2.n;

	if (!itemEntityId)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "IsTargetAimableWithMG(): wrong entity id in parameter 2.");
		return pH->EndFunction();
	}
	IItem* pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem(itemEntityId); 
	if (!pItem)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "entity in parameter 2 is not an item/weapon");
		return pH->EndFunction();
	}

	float minDist = 7;
	bool bSkipTargetCheck = false;
	Vec3 targetPos(ZERO);

	if(paramCount > 2)
	{
		for(int i=3;i <= paramCount ; i++)
		{
			if(pH->GetParamType(i) == svtBool)
				pH->GetParam(i,bSkipTargetCheck);
			else if(pH->GetParamType(i) == svtNumber)
				pH->GetParam(i,minDist);
			else if(pH->GetParamType(i) == svtObject)
				pH->GetParam(i,targetPos);
		}
	}
	
	IPipeUser* pPiper = CastToIPipeUserSafe(pAI);
	if(!pPiper)
	{
		gEnv->pAISystem->Warning("<CScriptBind_AI> ", "entity '%s' in parameter 1 is not a puppet", pEntity->GetName());
		return pH->EndFunction();
	}


	IEntity* pItemEntity = pItem->GetEntity();
	if(!pItemEntity)
		return pH->EndFunction();
	
		
	if(!pItem->GetOwnerId())
	{
		// weapon is not used, check if it is on a vehicle
		IEntity* pParentEntity = pItemEntity->GetParent();
		if(pParentEntity)
		{
			IAIObject* pParentAI = pParentEntity->GetAI();
			if(pParentAI && pParentAI->GetAIType()==AIOBJECT_VEHICLE)
			{
				IAIActor* pParentActor = pParentAI->CastToIAIActor();
				if(pParentActor && pParentActor->IsActive())
					return pH->EndFunction();

				IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
				IVehicle* pVehicle = pVehicleSystem->GetVehicle(pParentEntity->GetId());
				if(pVehicle)
				{
					int numSeats = pVehicle->GetSeatCount();
					bool found = false;
					for(int i=0; !found && i< numSeats; i++)
					{
						CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pVehicle->GetSeatById(i));
						if(pSeat)
						{
							CVehicleSeatActionWeapons* pAWeapons = pSeat->GetSeatActionWeapons();
							if(pAWeapons)
							{
								int weaponCount = pAWeapons->GetWeaponCount();
								for(int j=0; j<weaponCount;++j)
								{
									if(pAWeapons->GetWeaponEntityId(j) == itemEntityId)
									{
										if(pSeat->GetPassenger())
											return pH->EndFunction();
										found = true;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else if( pItem->GetOwnerId()!= pEntity->GetId()) // item is used by someone else?
		return pH->EndFunction(false);

	// check target
	if(bSkipTargetCheck)
		return pH->EndFunction(true);

	IAIObject* pTarget = pPiper->GetAttentionTarget();
	if(targetPos.IsZero())
	{
		if(!pTarget)
			return pH->EndFunction();
		targetPos = pTarget->GetPos();
	}

	Vec3 targetDir(targetPos - pItemEntity->GetWorldPos());
	Vec3 targetDirXY(targetDir.x, targetDir.y, 0);
	
	float length2D = targetDirXY.GetLength();
	if(length2D < minDist || length2D<=0)
		return pH->EndFunction();

	targetDirXY /= length2D;//normalize

	Vec3 mountedAngleLimits(pItem->GetMountedAngleLimits());

	float yawRange = DEG2RAD(mountedAngleLimits.z);
	if(yawRange > 0 && yawRange < gf_PI)
	{
		float deltaYaw = pItem->GetMountedDir().Dot(targetDirXY);
		if(deltaYaw < cos(yawRange))
			return pH->EndFunction(false);
	}
	
	float minPitch = DEG2RAD(mountedAngleLimits.x);
	float maxPitch = DEG2RAD(mountedAngleLimits.y);

	//maxPitch = (maxPitch - minPitch)/2;
	//minPitch = -maxPitch;

	float pitch = atanf(targetDir.z / length2D);

	if ( pitch < minPitch || pitch > maxPitch )
		return pH->EndFunction(false);

	if(pTarget)
	{
		IEntity* pTargetEntity = pTarget->GetEntity();
		if(pTargetEntity)
		{
			// check target distance and where he's going
			IPhysicalEntity *phys = pTargetEntity->GetPhysics();
			if(phys)
			{
				pe_status_dynamics	dyn;
				phys->GetStatus(&dyn);
				Vec3 velocity ( dyn.v);
				velocity.z = 0;

				float speed = velocity.GetLength2D();
				if(speed>0)
				{
					//velocity /= speed;
					if(length2D< minDist * 0.75f && velocity.Dot(targetDirXY)<=0)
						return pH->EndFunction(false);
				}
			}
		}
	}
	return pH->EndFunction(true);
	
}

/* unused now
//====================================================================
// GetClosestPointToOBB
//====================================================================
int CScriptBind_AI::GetClosestPointToOBB(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(3);
	GET_ENTITY(2);

	// Check entity
	if (!pEntity)
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetClosestPointToOBB(): no entity given parameter 2");
		return pH->EndFunction();
	}
	AABB aabb;
	pEntity->GetLocalBounds(aabb);
	aabb.min.z = 0;
	OBB obb;
	obb.SetOBBfromAABB(Matrix33(pEntity->GetWorldTM()), aabb);

	Vec3 pos;
	if(pH->GetParamType(1) == svtPointer)
	{
		ScriptHandle hdl;
		pH->GetParam(1,hdl);
		int nID = hdl.n;
		IEntity* pEntity2 = m_pSystem->GetIEntitySystem()->GetEntity(nID);
		if(pEntity2)
		{
			pos = pEntity2->GetWorldPos();
		}
		else
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "GetClosestPointToOBB(): wrong entity given parameter 1");
			return pH->EndFunction();
		}

	}
	else if(pH->GetParamType(1) == svtObject)//supposed table
	{
		SmartScriptTable theObj;
		pH->GetParam(1,theObj);// a vector is passed
		if(!(theObj.GetPtr() && theObj->GetValue("x",pos.x) && theObj->GetValue("y",pos.y) && theObj->GetValue("z",pos.z)))
		{
			m_pAISystem->Warning("<CScriptBind_AI> ", "GetClosestPointToOBB(): wrong parameter parameter 1");
			return pH->EndFunction();
		}
	}
	else
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetClosestPointToOBB(): wrong parameter parameter 1");
		return pH->EndFunction();
	}

	CScriptVector scriptRetPos;
	if(!(pH->GetParamType(3) == svtObject && pH->GetParam(3,scriptRetPos)))
	{
		m_pAISystem->Warning("<CScriptBind_AI> ", "GetClosestPointToOBB(): wrong parameter parameter 3");
		return pH->EndFunction();
	}
	

	std::vector<Vec3> Vertices;
	// get the 4 lower points of the obb
	//AABB aabb( obb.c - obb.h,obb.c + obb.h );
	Vec3 boxPos (pEntity->GetWorldPos());
	boxPos.z +=1;
	Vertices.push_back(obb.m33 * Vec3( aabb.min.x, aabb.min.y, aabb.min.z ) + boxPos);
	Vertices.push_back(obb.m33 * Vec3( aabb.min.x, aabb.max.y, aabb.min.z ) + boxPos);
	Vertices.push_back(obb.m33 * Vec3( aabb.max.x, aabb.max.y, aabb.min.z ) + boxPos);
	Vertices.push_back(obb.m33 * Vec3( aabb.max.x, aabb.min.y, aabb.min.z ) + boxPos);

//	gEnv->pAISystem->AddDebugLine(Vertices[0],Vertices[1],200,200,200,5);
//	gEnv->pAISystem->AddDebugLine(Vertices[1],Vertices[2],200,200,200,5);
//	gEnv->pAISystem->AddDebugLine(Vertices[2],Vertices[3],200,200,200,5);
//	gEnv->pAISystem->AddDebugLine(Vertices[3],Vertices[0],200,200,200,5);

	Vec3 retPos;
	float dist = Distance::Point_Polygon2D(pos,Vertices,retPos);
	
	scriptRetPos.Set(retPos);

	return pH->EndFunction(dist);

}

*/
