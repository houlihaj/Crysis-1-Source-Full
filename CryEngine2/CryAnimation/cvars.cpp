//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:CVars.cpp
//  Implementation of CVars class
//
//	History:
//	September 23, 2004: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"


void Console::Init()
{
	ca_CharEditModel="Objects/Characters/human_male/character.cdf";

	g_pIConsole->RegisterString( "ca_CharEditModel", ca_CharEditModel,	VF_CHEAT );

	g_pIConsole->Register("ca_DrawBBox", &ca_DrawBBox, 0,	VF_CHEAT,	"if set to 1, the own bounding box of the character is drawn"	);
	g_pIConsole->Register("ca_DrawSkeleton",	&ca_DrawSkeleton, 0,	VF_CHEAT,	"if set to 1, the skeleton is drawn" );
	g_pIConsole->Register("ca_DrawDecalsBBoxes",	&ca_DrawDecalsBBoxes, 0,	VF_CHEAT,	"if set to 1, the decals bboxes are drawn"	);
	g_pIConsole->Register("ca_DrawPositionPre",&ca_DrawPositionPre,	0,	VF_CHEAT,"draws the world position of the character (before update)");
	g_pIConsole->Register("ca_DrawPositionPost",&ca_DrawPositionPost,	0,	VF_CHEAT, "draws the world position of the character (after update)");
	g_pIConsole->Register("ca_DrawEmptyAttachments", &ca_DrawEmptyAttachments, 0,	VF_CHEAT, "draws a wireframe cube if there is no object linked to an attachment");
	g_pIConsole->Register("ca_DrawWireframe",	&ca_DrawWireframe, 0,	VF_CHEAT, "draws a wireframe on top of the rendered character");
	g_pIConsole->Register("ca_DrawAimPoses",	&ca_DrawAimPoses, 0,	VF_CHEAT, "draws the wireframe of the aim poses");
	g_pIConsole->Register("ca_DrawLookIK", &ca_DrawLookIK, 0, VF_CHEAT, "draws a visualization of look ik");
	g_pIConsole->Register("ca_DrawTangents",	&ca_DrawTangents, 0,	VF_CHEAT, "draws the tangents of the rendered character");
	g_pIConsole->Register("ca_DrawBinormals",	&ca_DrawBinormals, 0,	VF_CHEAT, "draws the binormals of the rendered character");
	g_pIConsole->Register("ca_DrawNormals",	&ca_DrawNormals, 0,	VF_CHEAT,"draws the normals of the rendered character");
	g_pIConsole->Register("ca_DrawAttachments", &ca_DrawAttachments, 1,	VF_CHEAT, "if this is 0, will not draw the attachments objects");
	g_pIConsole->Register("ca_DrawFaceAttachments", &ca_DrawFaceAttachments, 1,	0, "if this is 0, will not draw the skin attachments objects");
	
	g_pIConsole->Register("ca_DrawAttachmentOBB", &ca_DrawAttachmentOBB, 0,	VF_CHEAT, "if this is 0, will not draw the attachments objects"	);
	g_pIConsole->Register("ca_DrawCharacter", &ca_DrawCharacter, 1,	VF_CHEAT, "if this is 0, will not draw the characters");
	g_pIConsole->Register("ca_DrawMotionBlurTest", &ca_DrawMotionBlurTest, 0,	VF_CHEAT, "if this is 1, we draw the motion-blur test mesh"	);
	g_pIConsole->Register("ca_DrawBodyMoveDir", &ca_DrawBodyMoveDir, 0,	0,	"if this is 1, we will draw the body and move-direction" );
	g_pIConsole->Register("ca_DrawIdle2MoveDir", &ca_DrawIdle2MoveDir, 0,	0,	"if this is 1, we will draw the initial Idle2Move dir" );
	g_pIConsole->Register("ca_DrawFootPlants", &ca_DrawFootPlants, 0,	VF_CHEAT|VF_DUMPTODISK,	"if this is 1, it will print some debug boxes at the feet of the character"	);

	g_pIConsole->Register("ca_AimIKFadeout",	&ca_AimIKFadeout, 0,	VF_CHEAT,	"if set to 0, the Aim-IK will not fade out when aiming in extreme directions"	);

	g_pIConsole->Register("ca_JustRootUpdate", &ca_JustRootUpdate, 0,	VF_CHEAT,	"if this is 1 it will force just update of the root-bone, even if the character is visible; if it is 2, it will force everything to be always updated."	);
	g_pIConsole->Register("ca_DebugText", &ca_DebugText, 0,	VF_CHEAT|VF_DUMPTODISK,	"if this is 1, it will print some debug text on the screen" );
	g_pIConsole->Register("ca_PrintDesiredSpeed", &ca_PrintDesiredSpeed, 0,	VF_CHEAT|VF_DUMPTODISK, "if this is 1, it will print the desired speed of the human characters"	);
	g_pIConsole->Register("ca_DebugFootPlants", &ca_DebugFootPlants, 0,	VF_CHEAT|VF_DUMPTODISK,	"if this is 1, it will print some debug text on the screen"	);
	g_pIConsole->Register("ca_FootAnchoring", &ca_FootAnchoring, 0,	VF_CHEAT|VF_DUMPTODISK,	"if this is 1, it will print some debug boxes at the feet of the character"	);
	g_pIConsole->Register("ca_GroundAlignment", &ca_GroundAlignment, 1,	VF_CHEAT|VF_DUMPTODISK,	"if this is 1, the legs of human characters will align with the terrain"	);

	g_pIConsole->Register("ca_SphericalSkinning", &ca_SphericalSkinning,	1, VF_CHEAT|VF_DUMPTODISK,	"If this is 1, then use spherical-blend-skinning with up to 4 bones per vertex"	);
	g_pIConsole->Register("ca_EnableCoolReweighting", &ca_EnableCoolReweighting,	0, VF_CHEAT,"If this is 1, then we re-weight all vertices to fix some issues with spherical skinning");
	g_pIConsole->Register("ca_EnableCoolTransitions", &ca_EnableCoolTransitions,	0, VF_CHEAT,"If this is 1, then we adjust all transition-times for humans");
	g_pIConsole->Register( 
		"ca_LoadUncompressedChunks", &ca_LoadUncompressedChunks,	0, VF_CHEAT, 
		"If this 1, then uncompressed chunks prefer compressed while loading" 
		);

	g_pIConsole->Register( 
		"ca_NoDeform", &ca_NoDeform, 0,	VF_CHEAT, 
		"the skinning is not performed during rendering if this is 1" 
		);
	g_pIConsole->Register( 
		"ca_UseMorph", &ca_UseMorph, 1,	VF_CHEAT, 
		"the morph skinning step is skipped (it's part of overall skinning during rendering)" 
		);
	g_pIConsole->Register( 
		"ca_NoAnim", &ca_NoAnim, 0,	VF_CHEAT, 
		"the animation isn't updated (the characters remain in the same pose)" 
		);
	g_pIConsole->Register( 
		"ca_UsePhysics", &ca_UsePhysics, 1,	VF_CHEAT, 
		"the physics is not applied (effectively, no IK)" 
		);
	g_pIConsole->Register( 
		"ca_UseLookIK", &ca_UseLookIK, 1,	VF_CHEAT, 
		"If this is set to 1, then we are adding a look-at animation to the skeleton" 
		);
	g_pIConsole->Register( "ca_UseAimIK", &ca_UseAimIK, 1,	VF_CHEAT,	"If this is set to 1, then we are adding a look-at animation to the skeleton"	);

	g_pIConsole->Register( "ca_UseFacialAnimation", &ca_UseFacialAnimation, 1,	VF_CHEAT,	"If this is set to 1, we can play facial animations"	);
	g_pIConsole->Register( "ca_FacialAnimationRadius", &ca_FacialAnimationRadius, 30.0f, 0, "Maximum distance at which facial animations are updated - handles zooming correctly" );

	g_pIConsole->Register( 
		"ca_RandomScaling", &ca_RandomScaling, 0,	VF_CHEAT, 
		"If this is set to 1, then we apply ransom scaling to characters" 
		);



	g_pIConsole->Register("ca_UseDecals", &ca_UseDecals, 1,	0,	"if set to 0, effectively disables creation of decals on characters\n2 - alternative method of calculating/building the decals" );
	g_pIConsole->Register("ca_DecalSizeMultiplier", &ca_DecalSizeMultiplier, 1.0f,	VF_CHEAT,"The multiplier for the decal sizes"	);

	g_pIConsole->Register("ca_DebugModelCache", &ca_DebugModelCache, 0,	VF_CHEAT,	"shows what models are currently loaded");
	g_pIConsole->Register("ca_DebugAnimUpdates", &ca_DebugAnimUpdates, 0,	VF_CHEAT,	"shows the amount of skeleton-updates" );
	g_pIConsole->Register("ca_DebugAnimUsage", &ca_DebugAnimUsage, 0,	VF_CHEAT,	"shows what animation assets are used in the level" );
	g_pIConsole->Register("ca_DumpAssetStatistics", &ca_DumpAssetStatistics, 0,	VF_CHEAT,	"writes animation asset statistics to the disk" );

	g_pIConsole->Register( 
		"ca_Profile", &ca_Profile, 0,	VF_CHEAT, 
		"Enables a table of real-time profile statistics" 
		);

	g_pIConsole->Register( 
		"ca_AnimWarningLevel", &ca_AnimWarningLevel, 0/*Timur nDefaultAnimWarningLevel*/,	VF_CHEAT|VF_DUMPTODISK, 
		"if you set this to 0, there won't be any\nfrequest warnings from the animation system" 
		);
	g_pIConsole->Register( 
		"ca_DisableAnimEvents", &ca_DisableAnimEvents, 0,	VF_CHEAT, 
		"If this is not 0, then the OnAnimationEvent methods are not called upon animation events" 
		);
	g_pIConsole->Register( 
		"ca_KeepModels", &ca_KeepModels, 0,	VF_CHEAT, 
		"If set to 1, will prevent models from unloading from memory\nupon destruction of the last referencing character" 
		);



	// if this is not empty string, the animations of characters with the given model will be logged
	ca_LogAnimation="";

	g_pIConsole->RegisterString( "ca_LogAnimation",ca_LogAnimation, VF_CHEAT	);

	g_pIConsole->Register( "ca_EnableAnimationLog", &ca_EnableAnimationLog, 0,	VF_CHEAT, "enables a special kind of log: Animation.log file, solely for debugging" );
	g_pIConsole->Register( "ca_MemoryUsageLog", &ca_MemoryUsageLog, 0,	VF_CHEAT, "enables a memory usage log" );

	g_pIConsole->Register( "ca_DebugSkeletonEffects", &ca_DebugSkeletonEffects, 0,	VF_CHEAT, "If true, dump log messages when skeleton effects are handled." );

	g_pIConsole->Register( "ca_DeathBlendTime", &ca_DeathBlendTime, 0.3f,	VF_CHEAT, "Specifies the blending time between low-detail dead body skeleton and current skeleton" );

	g_pIConsole->Register( "ca_lipsync_phoneme_offset", &ca_lipsync_phoneme_offset, 20,	VF_CHEAT, "Offset phoneme start time by this value in milliseconds" );

	g_pIConsole->Register( "ca_lipsync_phoneme_crossfade", &ca_lipsync_phoneme_crossfade, 70,	VF_CHEAT, "Cross fade time between phonemes in milliseconds" );

	g_pIConsole->Register( "ca_lipsync_vertex_drag", &ca_lipsync_vertex_drag, 1.2f,	VF_CHEAT, "Vertex drag coefficient when blending morph targets" );

	//g_pIConsole->Register( "ca_lipsync_phoneme_strong", &ca_lipsync_phoneme_strong, 0.75f, 0, "Strong phoneme intensity border" );
	//g_pIConsole->Register( "ca_lipsync_phoneme_weak", &ca_lipsync_phoneme_weak, 0.25f, 0, "Weak phoneme intensity border" );

	g_pIConsole->Register( "ca_eyes_procedural", &ca_eyes_procedural, 1, 0, "Enables/Disables procedural eyes animation" );
	//g_pIConsole->Register( "ca_face_procedural_strength", &ca_face_procedural_strength, 1.0f, 0, "Procedural face animation strength" );
	//g_pIConsole->Register( "ca_face_procedural_timing", &ca_face_procedural_timing, 1.0f, 0, "Procedural face animation strength" );

	g_pIConsole->Register( "ca_lipsync_phoneme_strength", &ca_lipsync_phoneme_strength, 1.0f, 0, "LipSync phoneme strength" );

	g_pIConsole->Register( "ca_lipsync_debug", &ca_lipsync_debug, 0,	VF_CHEAT,	"Enables facial animation debug draw" );

	g_pIConsole->Register( "ca_DebugFacial", &ca_DebugFacial, 0,	VF_CHEAT,	"Debug facial playback info"	);

	g_pIConsole->Register( "ca_DebugFacialEyes", &ca_DebugFacialEyes, 0, VF_CHEAT, "Debug facial eyes info" );
  

	g_pIConsole->Register( "ca_LoadDatabase", &ca_LoadDatabase, 1, VF_CHEAT,	"Enable loading animations from database"	);

	g_pIConsole->Register( "ca_ForceNullAnimation", &ca_ForceNullAnimation, 0,	VF_CHEAT,	"replaces the mocap-data by default position" );

	g_pIConsole->Register( "ca_travelSpeedScaleMin", &ca_travelSpeedScaleMin, 0.5f,	VF_CHEAT,	"Minimum motion travel speed scale (default 0.5)." );
	g_pIConsole->Register( "ca_travelSpeedScaleMax", &ca_travelSpeedScaleMax, 2.0f,	VF_CHEAT,	"Maximum motion travel speed scale (default 2.0)." );

	g_pIConsole->Register( "ca_GameControlledStrafing", &ca_GameControlledStrafing, 1, VF_CHEAT,	"Use game controlled strafing/curving flag, instead of low level calculated curving weight." );

	g_pIConsole->Register( "ca_debugCaps", &ca_debugCaps, 0,	0,	"Display current blended motion capabilities." );
	g_pIConsole->Register( "ca_EnableAssetTurning", &ca_EnableAssetTurning, 1,	0,	"asset tuning is disabled by default" );
	g_pIConsole->Register( "ca_SaveAABB", &ca_SaveAABB, 0,	0,	"if the AABB is invalid, replace it by the default AABB" );
	g_pIConsole->Register( "ca_AttachmentCullingRation", &ca_AttachmentCullingRation, 200.0f,	0,	"ration between size of attachment and distance to camera" );
	g_pIConsole->Register( "ca_FPWeaponInCamSpace", &ca_FPWeaponInCamSpace, 1,	0,	"if this is 1, then we attach the wepon to the camera" );
	g_pIConsole->Register( "ca_EnableAssetStrafing", &ca_EnableAssetStrafing, 1,	0,	"asset strafing is disabled by default" );
	g_pIConsole->Register( "ca_UnloadAimPoses", &ca_UnloadAimPoses, 1,	VF_DUMPTODISK,	"remove aim-poses from memory" );

	g_pIConsole->Register( "ca_UseAnimationsCache", &ca_UseAnimationsCache, 0,	VF_DUMPTODISK,	"use dynamically unpacked animations cache" );
	g_pIConsole->Register( "ca_AnimationsUsageStatistics", &ca_AnimationsUsageStatistics, 0,	VF_DUMPTODISK,	"Output animations usage statistics" );

	static ICVar *p_e_lod_ratio = g_pISystem->GetIConsole()->GetCVar("ca_lod_ratio");
	float eLod = 6.0f;
	if (p_e_lod_ratio)
		eLod = p_e_lod_ratio->GetFVal();

	g_pIConsole->Register( "ca_lod_ratio", &ca_lod_ratio, eLod,	VF_DUMPTODISK,	"Caharcters LOD ratio" );
	

  m_pMotionBlurShutterSpeed = g_pIConsole->GetCVar("r_MotionBlurShutterSpeed");  
  m_pMotionBlurFrameTimeScale = g_pIConsole->GetCVar("r_MotionBlurFrameTimeScale");  
	m_pMotionBlur = g_pIConsole->GetCVar("r_MotionBlur");  
}




