#include "StdAfx.h"
#include "AnimPhysicsNodes.h"
#include "ICryAnimation.h"
#include "IAISystem.h"
#include "IAgent.h"
#include "AnimatedCharacter.h"

void CAnimTentacleParams::EnterState( SAnimationStateData& data, bool dueToRollback )
{
	data.userData[m_jointLimitID] = m_jointLimit;
	data.userData[m_animBlendID] = m_animBlend;
}

bool CAnimTentacleParams::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAnimTentacleParams::LeaveState( SAnimationStateData& data )
{
}

const IAnimationStateNodeFactory::Params * CAnimTentacleParams::GetParameters()
{
	static const Params params[] = //editor display dummy
	{
		{true,  "string",   "bones",  "Tentacle names",          ""},
		{true,  "float",   "animBlend",  "Anim/Physics blend",   "0.0"},
		{true,  "float",   "jointLimit",  "Anim/Physics jointLimit",   "-1.0"},
		{0}
	};

	return params;
}

void CAnimTentacleParams::SetTentacles(ICharacterInstance *pCharacter,pe_params_rope *ppRope,const char *bones)
{
	if (pCharacter) 
	{
		ISkeletonPose* pISkeletonPose = pCharacter->GetISkeletonPose();
		if (pISkeletonPose==0)
			return;

		if (!bones || !stricmp(bones,"all"))
		{
			int tNum(0);
			IPhysicalEntity *pTentacle = pISkeletonPose->GetCharacterPhysics(tNum);

			while(pTentacle)
			{
				pTentacle->SetParams(ppRope);
				pTentacle = pISkeletonPose->GetCharacterPhysics(++tNum);
			}
		}
		else
		{
			char *pBone;
			char boneList[256];

			strncpy(boneList,bones,256);
			boneList[255] = 0;

			pBone = boneList;
			pBone = strtok(pBone,";");

			while (pBone != NULL && *pBone)
			{
				IPhysicalEntity *pTentacle = pISkeletonPose->GetCharacterPhysics(pBone);
				if (pTentacle) 
				{
					pTentacle->SetParams(ppRope);
					//CryLogAlways("tentacle %s animblend:%.1f",pBone,m_animBlend);
				}

				pBone = strtok(NULL,";");
			}
		}
	}
}

void CAnimTentacleParams::Update( SAnimationStateData& data )
{
	ICharacterInstance * pCharacter = data.pEntity->GetCharacter(0);

	/*
	float white[4]={1,1,1,1};
	gEnv->pRenderer->
		Draw2dLabel(50, 200, 2, white, false, "%f %f", 
			pCharacter->GetISkeleton()->GetUserData(m_animBlendID), 
			pCharacter->GetISkeleton()->GetUserData(m_jointLimitID));
	*/

	if (!pCharacter)
	{
		GameWarning("Entity %s has no character attached", data.pEntity->GetClass()->GetName());
		return;
	}

	//TODO: keep the IPhysicalEntity pointers for an eventual use of the update function.

	pe_params_rope pRope;

	float	animBlend = pCharacter->GetISkeletonAnim()->GetUserData(m_animBlendID);

	IActorSystem*	pASystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	if(pASystem )
	{
		IActor*  pActor = pASystem->GetActor( data.pEntity->GetId() );
		if(pActor)
		{
	//		animBlend = pActor->GetDynPhysicsAnimBlend();
			pActor->SetAnimTentacleParams(pRope,animBlend);
		}
	}
	

	//be sure to go through all the attachments
	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
	uint32 numAttachmnets = pIAttachmentManager ? pIAttachmentManager->GetAttachmentCount() : 0;

	for (uint32 i=0; i<numAttachmnets; ++i) 
	{			
		IAttachmentObject* pIAttachmentObject = pIAttachmentManager->GetInterfaceByIndex(i)->GetIAttachmentObject();
		if (pIAttachmentObject) 
			SetTentacles(pIAttachmentObject->GetICharacterInstance(),&pRope,m_bones.c_str());
	}

	SetTentacles(pCharacter,&pRope,m_bones.c_str());
}

bool CAnimTentacleParams::Init( const XmlNodeRef& node, IAnimationGraphPtr pGraph )
{
	m_animBlendID = pGraph->GetBlendValueID( "Tentacle_AnimBlend" );
	m_jointLimitID = pGraph->GetBlendValueID( "Tentacle_JointLimit" );

	m_bones = node->getAttr("bones");
	if (m_bones.empty())
		return false;

	m_jointLimit = -1.0f;

	node->getAttr("animBlend", m_animBlend);
	if(node->haveAttr("jointLimit"))
		node->getAttr("jointLimit", m_jointLimit);

	if(m_jointLimit >= 0)
	{
		if(m_jointLimit > 360)
			m_jointLimit = m_jointLimit - (m_jointLimit/int(360)) * 360;
	}

	return true;
}

void CAnimTentacleParams::Release()
{
	delete this;
}

IAnimationStateNode * CAnimTentacleParams::Create()
{
	return this;
}

void CAnimTentacleParams::DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement )
{
	ICharacterInstance * pChar = data.pEntity->GetCharacter(0);
	if (!pChar)
		return;

	float white[] = {1,1,1,1};
	pRenderer->Draw2dLabel( x, y, 1, white, false, "animBlend: want:%.2f have:%.2f", m_animBlend, pChar->GetISkeletonAnim()->GetUserData(m_animBlendID) );
	y += yIncrement;
	pRenderer->Draw2dLabel( x, y, 1, white, false, "jointLimit: want:%.2f have:%.2f", m_jointLimit, pChar->GetISkeletonAnim()->GetUserData(m_jointLimitID) );
	y += yIncrement;
}

void CAnimTentacleParams::SerializeAsFile(bool reading, AG_FILE *file)
{
	SerializeAsFile_NodeBase(reading, file);

	FileSerializationHelper h(reading, file);

	h.StringValue(&m_bones);
	h.Value(&m_animBlend);
	h.Value(&m_jointLimit);
	h.Value(&m_animBlendID);
	h.Value(&m_jointLimitID);
}


void CAnimFallAndPlay::LeaveState( SAnimationStateData& data )
{
	// Notify AI that the sleep is over.
	// The "TRANQUILIZED" signal is send from Tactical.h
	IAISystem* pAISystem = gEnv->pAISystem;
	if ( pAISystem && data.pEntity->GetAI() )
	{
		IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor( data.pEntity->GetId() );
		if ( !pActor || pActor->GetHealth() > 0 )
		{
			pActor->NotifyLeaveFallAndPlay();
		}
	}
}


bool CAGFreeFall::CanLeaveState( SAnimationStateData& data )
{
	if (data.pEntity)
	{
		// allow leaving the state if actor is dead
		IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor( data.pEntity->GetId() );
		if ( pActor && pActor->GetHealth() > 0 && data.pAnimatedCharacter->GetMCMH() == eMCM_Animation && data.pAnimatedCharacter->GetMCMV() == eMCM_Animation )
		{
			Vec3 pos = data.pEntity->GetWorldPos();
			float zSpeed = pos.z - data.pAnimatedCharacter->GetPrevEntityLocation().t.z;
			float elevation = gEnv->p3DEngine->GetTerrainElevation( pos.x, pos.y );
			if ( pos.z + 0.5f*zSpeed > elevation )
			{
				// the ground is still far - don't allow transition to the next animation to start
				return false;
			}
			else
			{
				// the entity may fall thru the ground on low frame rates without this
				data.pEntity->SetPos( Vec3(pos.x, pos.y, elevation) );
			}
		}
	}
	return true;
}

void CAGFreeFall::EnteredState( SAnimationStateData& data )
{
	if ( IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(data.pEntity->GetId()) )
	if ( IVehicle* pVehicle = pActor->GetLinkedVehicle() )
	if ( IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(data.pEntity->GetId()) )
	{
		// this will send false exiting-is-done notification
		// which would then break the link between the vehicle and the character
		pSeat->ForceFinishExiting();

		// this is here just in case
		data.pAnimatedCharacter->SetMovementControlMethods( eMCM_Animation, eMCM_Animation );
	}
}
