////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   entitynode.cpp
//  Version:     v1.00
//  Created:     23/4/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "EntityNode.h"
#include "EventTrack.h"
#include "CharacterTrack.h"
#include "AnimSplineTrack.h"
#include "ExprTrack.h"
#include "BoolTrack.h"
#include "ISystem.h"
#include "SelectTrack.h"
#include "FaceSeqTrack.h"
#include "LookAtTrack.h"
#include "Movie.h"

#include <ISound.h>
#include <ILipSync.h>
#include <ICryAnimation.h>
#include <CryCharMorphParams.h>
#include <Cry_Camera.h>

// AI
#include <IAgent.h>

#define HEAD_BONE_NAME "Bip01 Head"

#define s_nodeParamsInitialized s_nodeParamsInitializedEnt
#define s_nodeParams s_nodeParamsEnt
#define AddSupportedParam AddSupportedParamEnt

//////////////////////////////////////////////////////////////////////////
namespace
{
	bool s_nodeParamsInitialized = false;
	std::vector<IAnimNode::SParamInfo> s_nodeParams;

	void AddSupportedParam( std::vector<IAnimNode::SParamInfo> &nodeParams,const char *sName,int paramId,EAnimValue valueType )
	{
		IAnimNode::SParamInfo param;
		param.name = sName;
		param.paramId = paramId;
		param.valueType = valueType;
		nodeParams.push_back( param );
	}
};

//////////////////////////////////////////////////////////////////////////
CAnimEntityNode::CAnimEntityNode( IMovieSystem *sys )
: CAnimNode(sys)
{
	m_dwSupportedTracks = PARAM_BIT(APARAM_POS)|PARAM_BIT(APARAM_ROT)|PARAM_BIT(APARAM_SCL)|	
												PARAM_BIT(APARAM_EVENT)|PARAM_BIT(APARAM_VISIBLE)|
												PARAM_BIT(APARAM_SOUND1)|PARAM_BIT(APARAM_SOUND2)|PARAM_BIT(APARAM_SOUND3)|
												PARAM_BIT(APARAM_CHARACTER1)|PARAM_BIT(APARAM_CHARACTER2)|PARAM_BIT(APARAM_CHARACTER3)|
												PARAM_BIT(APARAM_EXPRESSION1)|PARAM_BIT(APARAM_EXPRESSION2)|PARAM_BIT(APARAM_EXPRESSION3);

	m_EntityId = 0;
	m_entityGuid = 0;
	m_target=NULL;
	m_pMovie=sys;
	m_bWasTransRot = false;
	m_nPlayingAnimations = 0;

	m_pos(0,0,0);
	m_scale(1,1,1);
	m_visible = true;

	m_lastEntityKey = -1;
	m_lastCharacterKey[0] = -1;
	m_lastCharacterKey[1] = -1;
	m_lastCharacterKey[2] = -1;
	m_bPlayingAnimationAtLayer[0] = false;
	m_bPlayingAnimationAtLayer[1] = false;
	m_bPlayingAnimationAtLayer[2] = false;

	m_lookAtTarget = "";
	m_lookAtEntityId = 0;
	m_allowAdditionalTransforms = true;
	m_boneSet = eLookAtKeyBoneSet_HeadEyes;

	for (int i=0;i<ENTITY_SOUNDTRACKS;i++)
	{
		m_SoundInfo[i].nLastKey = -1;
		m_SoundInfo[i].pSound = NULL;
		m_SoundInfo[i].sLastFilename = "";
	}

	if (!s_nodeParamsInitialized)
	{
		s_nodeParamsInitialized = true;
		AddSupportedParams( s_nodeParams );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::AddSupportedParams( std::vector<IAnimNode::SParamInfo> &nodeParams )
{
	AddSupportedParam( nodeParams,"Position",APARAM_POS,AVALUE_VECTOR );
	AddSupportedParam( nodeParams,"Rotation",APARAM_ROT,AVALUE_QUAT );
	AddSupportedParam( nodeParams,"Scale",APARAM_SCL,AVALUE_VECTOR );
	AddSupportedParam( nodeParams,"Visibility",APARAM_VISIBLE,AVALUE_BOOL );
	AddSupportedParam( nodeParams,"Event",APARAM_EVENT,AVALUE_EVENT );
	AddSupportedParam( nodeParams,"Sound1",APARAM_SOUND1,AVALUE_SOUND );
	AddSupportedParam( nodeParams,"Sound2",APARAM_SOUND2,AVALUE_SOUND );
	AddSupportedParam( nodeParams,"Sound3",APARAM_SOUND3,AVALUE_SOUND );
	AddSupportedParam( nodeParams,"Animation1",APARAM_CHARACTER1,AVALUE_CHARACTER );
	AddSupportedParam( nodeParams,"Animation2",APARAM_CHARACTER2,AVALUE_CHARACTER );
	AddSupportedParam( nodeParams,"Animation3",APARAM_CHARACTER3,AVALUE_CHARACTER );
	//AddSupportedParam( nodeParams,"Animation4",APARAM_CHARACTER4,AVALUE_CHARACTER );
	//AddSupportedParam( nodeParams,"Animation5",APARAM_CHARACTER5,AVALUE_CHARACTER );
	//AddSupportedParam( nodeParams,"Animation6",APARAM_CHARACTER6,AVALUE_CHARACTER );
	//AddSupportedParam( nodeParams,"Animation7",APARAM_CHARACTER7,AVALUE_CHARACTER );
	//AddSupportedParam( nodeParams,"Animation8",APARAM_CHARACTER8,AVALUE_CHARACTER );
	//AddSupportedParam( nodeParams,"Animation9",APARAM_CHARACTER9,AVALUE_CHARACTER );
	//AddSupportedParam( nodeParams,"Animation10",APARAM_CHARACTER10,AVALUE_CHARACTER );
	AddSupportedParam( nodeParams,"Expression1",APARAM_EXPRESSION1,AVALUE_EXPRESSION );
	AddSupportedParam( nodeParams,"Expression2",APARAM_EXPRESSION2,AVALUE_EXPRESSION );
	AddSupportedParam( nodeParams,"Expression3",APARAM_EXPRESSION3,AVALUE_EXPRESSION );
	//AddSupportedParam( nodeParams,"Expression4",APARAM_EXPRESSION4,AVALUE_EXPRESSION );
	//AddSupportedParam( nodeParams,"Expression5",APARAM_EXPRESSION5,AVALUE_EXPRESSION );
	//AddSupportedParam( nodeParams,"Expression6",APARAM_EXPRESSION6,AVALUE_EXPRESSION );
	//AddSupportedParam( nodeParams,"Expression7",APARAM_EXPRESSION7,AVALUE_EXPRESSION );
	//AddSupportedParam( nodeParams,"Expression8",APARAM_EXPRESSION8,AVALUE_EXPRESSION );
	//AddSupportedParam( nodeParams,"Expression9",APARAM_EXPRESSION9,AVALUE_EXPRESSION );
	//AddSupportedParam( nodeParams,"Expression10",APARAM_EXPRESSION10,AVALUE_EXPRESSION );

	AddSupportedParam( nodeParams,"Facial Sequence",APARAM_FACE_SEQUENCE,AVALUE_FACESEQ );
	AddSupportedParam( nodeParams,"LookAt",APARAM_LOOKAT,AVALUE_LOOKAT );
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::CreateDefaultTracks()
{
	CreateTrack(APARAM_POS);
	CreateTrack(APARAM_ROT);
	CreateTrack(APARAM_EVENT);
};

//////////////////////////////////////////////////////////////////////////
CAnimEntityNode::~CAnimEntityNode()
{
	ReleaseSounds();
}

//////////////////////////////////////////////////////////////////////////
int CAnimEntityNode::GetParamCount() const
{
	return s_nodeParams.size();
}

//////////////////////////////////////////////////////////////////////////
bool CAnimEntityNode::GetParamInfo( int nIndex, SParamInfo &info ) const
{
	if (nIndex >= 0 && nIndex < (int)s_nodeParams.size())
	{
		info = s_nodeParams[nIndex];
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CAnimEntityNode::GetParamInfoFromId( int paramId, SParamInfo &info ) const
{
	for (int i = 0; i < (int)s_nodeParams.size(); i++)
	{
		if (s_nodeParams[i].paramId == paramId)
		{
			info = s_nodeParams[i];
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
IEntity* CAnimEntityNode::GetEntity()
{
	if (!m_EntityId)
	{
		m_EntityId = gEnv->pEntitySystem->FindEntityByGuid(m_entityGuid);
	}
	return gEnv->pEntitySystem->GetEntity(m_EntityId);
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::SetEntityGuid( const EntityGUID &guid )
{
	m_entityGuid = guid;
	m_EntityId = gEnv->pEntitySystem->FindEntityByGuid(m_entityGuid);
};

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::SyncUpdate( ESyncUpdateType type,SAnimContext &ac )
{
	CAnimBlock *anim = (CAnimBlock*)GetAnimBlock();
	if (!anim)
		return;

	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	int paramCount = anim->GetTrackCount();
	for (int paramIndex = 0; paramIndex < paramCount; paramIndex++)
	{
		int trackType;
		IAnimTrack *pTrack;
		if (!anim->GetTrackInfo( paramIndex,trackType,&pTrack ))
			continue;
		switch (trackType)
		{
		case APARAM_LOOKAT:
			{
				CLookAtTrack *pSelTrack = (CLookAtTrack*)pTrack;
				AnimateLookAt( pSelTrack,ac );
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::Animate( SAnimContext &ec )
{
	CAnimBlock *anim = (CAnimBlock*)GetAnimBlock();
	if (!anim)
		return;

	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	Vec3 pos = m_pos;
	Quat rotate = m_rotate;
	Vec3 scale = m_scale;

	bool bPosModified = false;
	bool bAnglesModified = false;
	bool bScaleModified = false;
	
	IAnimTrack *posTrack = NULL;
	IAnimTrack *rotTrack = NULL;
	IAnimTrack *sclTrack = NULL;

	if (!ec.bResetting)
	{
		StopExpressions();
	}

	int paramCount = anim->GetTrackCount();
	for (int paramIndex = 0; paramIndex < paramCount; paramIndex++)
	{
		int trackType;
		IAnimTrack *pTrack;
		if (!anim->GetTrackInfo( paramIndex,trackType,&pTrack ))
			continue;
		switch (trackType)
		{
		case APARAM_POS:
			posTrack = pTrack;
			posTrack->GetValue( ec.time,pos );
			if (posTrack->GetNumKeys() > 0)
				bPosModified = true;
			break;
		case APARAM_ROT:
			rotTrack = pTrack;
			rotTrack->GetValue( ec.time,rotate );
			if (rotTrack->GetNumKeys() > 0)
				bAnglesModified = true;
			break;
		case APARAM_SCL:
			sclTrack = pTrack;
			sclTrack->GetValue( ec.time,scale );
			if (sclTrack->GetNumKeys() > 0)
				bScaleModified = true;
			break;
		case APARAM_EVENT:
			if (!ec.bResetting)
			{
				CEventTrack *entityTrack = (CEventTrack*)pTrack;
				IEventKey key;
				int entityKey = entityTrack->GetActiveKey(ec.time,&key);
				// If key is different or if time is standing exactly on key time.
				//if ((entityKey != m_lastEntityKey || key.time == ec.time) && (!ec.bSingleFrame))
				if (entityKey != m_lastEntityKey || key.time == ec.time)
				{
					m_lastEntityKey = entityKey;
					if (entityKey >= 0)
					{
//						if (!ec.bSingleFrame || key.time == ec.time) // If Single frame update key time must match current time.
							ApplyEventKey( entityTrack,entityKey,key );
					}
				}
			}
			break;
		case APARAM_VISIBLE:
			if (!ec.bResetting)
			{
				IAnimTrack *visTrack = pTrack;
				bool visible = m_visible;
				visTrack->GetValue( ec.time,visible );
				pEntity->Hide(!visible);
				m_visible = visible;
			}
			break;

		//////////////////////////////////////////////////////////////////////////
		case APARAM_SOUND1:
		case APARAM_SOUND2:
		case APARAM_SOUND3:
			if (!ec.bResetting && gEnv->pSoundSystem)
			{
				int nSoundIndex = trackType - APARAM_SOUND1;
				CSoundTrack *pSoundTrack = (CSoundTrack*)pTrack;
				ISoundKey key;
				int nSoundKey = pSoundTrack->GetActiveKey(ec.time, &key);
				
				if (!key.bLoop && key.time+key.fDuration < ec.time)
					nSoundKey = -1;

				if (nSoundKey!=m_SoundInfo[nSoundIndex].nLastKey || key.time==ec.time || nSoundKey==-1 || ec.bSingleFrame)
				{
					{
						ApplySoundKey( pSoundTrack,nSoundKey,nSoundIndex, key, ec);
					}
				}
				else
				{
					// update
					if (m_SoundInfo[nSoundIndex].pSound)
					{
						if (m_SoundInfo[nSoundIndex].pSound->GetFlags() & FLAG_SOUND_3D)
						{
							Vec3 SoundPos = pEntity->GetPos();
							Vec3 SoundDir = pEntity->GetWorldTM().TransformVector(FORWARD_DIRECTION);
							Vec3 vCharacterCenter(0);

							bool bHead = false;
							bool bCharacter = false;

							// 3D sound.

							// testing for head position
							ICharacterInstance * pCharacter = pEntity->GetCharacter(0);

							if (pCharacter)
							{
								bCharacter = true;
								vCharacterCenter = pCharacter->GetAABB().GetCenter();
								SoundPos = pEntity->GetWorldTM()*pCharacter->GetAABB().GetCenter();

								if (m_SoundInfo[nSoundIndex].pSound->GetFlags() & FLAG_SOUND_VOICE)
								{
									bHead = true;
								}
							}

							IEntitySoundProxy *pSoundProxy = (IEntitySoundProxy*)pEntity->CreateProxy(ENTITY_PROXY_SOUND);

							if (pSoundProxy)
							{
								pSoundProxy->UpdateSounds();

 								Vec3 vOffset(0);
 								if (!bHead && bCharacter)
 								{
 									vOffset = vCharacterCenter;
 								}

								pSoundProxy->SetSoundPos(m_SoundInfo[nSoundIndex].pSound->GetId(), vOffset);
							}
							else
							{
								// also set sound position, because entity might not move at all
								m_SoundInfo[nSoundIndex].pSound->SetPosition( SoundPos );
								m_SoundInfo[nSoundIndex].pSound->SetDirection( SoundDir );
							}
						}
					}
				}

				m_SoundInfo[nSoundIndex].nLastKey = nSoundKey;
			}
			break;
		
		//////////////////////////////////////////////////////////////////////////
		case APARAM_CHARACTER1:
		case APARAM_CHARACTER2:
		case APARAM_CHARACTER3:
			if (!ec.bResetting)
			{
				int index = trackType - APARAM_CHARACTER1;
				CCharacterTrack *pCharTrack = (CCharacterTrack*)pTrack;
				AnimateCharacterTrack( pCharTrack,ec,index );
			}
			break;

		case APARAM_CHARACTER4:
		case APARAM_CHARACTER5:
		case APARAM_CHARACTER6:
		case APARAM_CHARACTER7:
		case APARAM_CHARACTER8:
		case APARAM_CHARACTER9:
		case APARAM_CHARACTER10:
			if (!ec.bResetting)
			{
				int index = trackType - APARAM_CHARACTER4+3;
				CCharacterTrack *pCharTrack = (CCharacterTrack*)pTrack;
				AnimateCharacterTrack( pCharTrack,ec,index );
			}
			break;

		//////////////////////////////////////////////////////////////////////////
		case APARAM_EXPRESSION1:
		case APARAM_EXPRESSION2:
		case APARAM_EXPRESSION3:
			if (!ec.bResetting)
			{
				CExprTrack *pExpTrack = (CExprTrack*)pTrack;
				AnimateExpressionTrack( pExpTrack,ec );
			}
			break;

		case APARAM_EXPRESSION4:
		case APARAM_EXPRESSION5:
		case APARAM_EXPRESSION6:
		case APARAM_EXPRESSION7:
		case APARAM_EXPRESSION8:
		case APARAM_EXPRESSION9:
		case APARAM_EXPRESSION10:
			if (!ec.bResetting)
			{
				CExprTrack *pExpTrack = (CExprTrack*)pTrack;
				AnimateExpressionTrack( pExpTrack,ec );
			}
			break;
			//////////////////////////////////////////////////////////////////////////
		case APARAM_FACE_SEQUENCE:
			if (!ec.bResetting)
			{
				CFaceSeqTrack *pSelTrack = (CFaceSeqTrack*)pTrack;
				AnimateFacialSequence( pSelTrack,ec );
			}
			break;
		
		case APARAM_LOOKAT:
			if (!ec.bResetting)
			{
				CLookAtTrack *pSelTrack = (CLookAtTrack*)pTrack;
				AnimateLookAt( pSelTrack,ec );
			}
			break;
		};
	}
	
	if (!ec.bResetting)
	{
		//////////////////////////////////////////////////////////////////////////
		// If not resetting animation sequence.
		//////////////////////////////////////////////////////////////////////////
		if (!ec.bSingleFrame)
		{
			IPhysicalEntity *pEntityPhysics = pEntity->GetPhysics();
			if (pEntityPhysics && !pEntity->IsHidden())
			{
				if (ec.time - m_time < 0.1f)
				{
					float timeStep = ec.time - m_time;

					float rtimeStep = timeStep>1E-5f ? 1.0f/timeStep : 0.0f;
					pe_action_set_velocity asv;
					asv.v = (pos - m_pos)*rtimeStep;
					asv.w = (rotate*!m_rotate).v*(rtimeStep*2);
					pEntityPhysics->Action(&asv);
				}
			}
		}
	} // not single frame

	m_time = ec.time;

	if (bPosModified || bAnglesModified || bScaleModified || (m_target!=NULL))
	{
		m_pos = pos;
		m_scale = scale;
		m_rotate = rotate;

		m_bIgnoreSetParam = true; // Prevents feedback change of track.
		if (m_pOwner)
		{
			m_pOwner->OnNodeAnimated(this);
		}
		else	// no callback specified, so lets move the entity directly
		{
			if (bPosModified)
				pEntity->SetPos(m_pos,ENTITY_XFORM_TRACKVIEW);

			if (bAnglesModified)
				pEntity->SetRotation(m_rotate,ENTITY_XFORM_TRACKVIEW);

			if (bScaleModified)
				pEntity->SetScale(m_scale,ENTITY_XFORM_TRACKVIEW);
		}
		m_bIgnoreSetParam = false; // Prevents feedback change of track.
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::ReleaseSounds()
{
	// stop all sounds
	for (int i=0;i<ENTITY_SOUNDTRACKS;i++)
	{
		if (m_SoundInfo[i].pSound)
			m_SoundInfo[i].pSound->Stop();

		m_SoundInfo[i].pSound = NULL;
		m_SoundInfo[i].nLastKey = -1;
		
		if (!m_SoundInfo[i].sLastFilename.empty())
			m_SoundInfo[i].sLastFilename = "";
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::Reset()
{
	m_EntityId = 0;
	m_lastEntityKey = -1;
	m_lastCharacterKey[0] = -1;
	m_lastCharacterKey[1] = -1;
	m_lastCharacterKey[2] = -1;
	m_lookAtTarget = "";
	m_lookAtEntityId = 0;
	m_allowAdditionalTransforms = true;
	m_boneSet = eLookAtKeyBoneSet_HeadEyes;
	ReleaseSounds();
	ReleaseAllAnims();
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::PrepareAnimations()
{
	// Update durations of all character animations.
	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;
	ICharacterInstance *pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return;

	IAnimationSet* pAnimations = pCharacter->GetIAnimationSet();
	if (!pAnimations)
		return;

	CAnimBlock *anim = (CAnimBlock*)GetAnimBlock();
	if (!anim)
		return;

	int paramCount = anim->GetTrackCount();
	for (int paramIndex = 0; paramIndex < paramCount; paramIndex++)
	{
		int trackType;
		IAnimTrack *pTrack;
		if (!anim->GetTrackInfo( paramIndex,trackType,&pTrack ))
			continue;
		switch (trackType)
		{
		case APARAM_CHARACTER1:
		case APARAM_CHARACTER2:
		case APARAM_CHARACTER3:
			{
				int index = trackType - APARAM_CHARACTER1;
				int numKeys = pTrack->GetNumKeys();
				for (int i = 0; i < numKeys; i++)
				{
					ICharacterKey key;
					pTrack->GetKey( i,&key );
					int animId = pAnimations->GetAnimIDByName( key.animation );
					if (animId >= 0)
					{
						//float duration = pAnimations->GetLength(animId);
						float duration = pAnimations->GetDuration_sec(animId);
						if (duration != key.duration)
						{
							key.duration = duration;
							pTrack->SetKey( i,&key );
						}
					}
				}
			}
			break;
		case APARAM_FACE_SEQUENCE:
			{
				IFacialInstance *pFaceInstance = pCharacter->GetFacialInstance();
				if (pFaceInstance)
				{
					ISelectKey key;
					int numKeys = pTrack->GetNumKeys();
					for (int i = 0; i < numKeys; i++)
					{
						pTrack->GetKey( i,&key );
						_smart_ptr<IFacialAnimSequence> pSequence = LoadFacialSequence(key.szSelection);
						if (pSequence)
						{
							key.fDuration = pSequence->GetTimeRange().Length();
							pTrack->SetKey( i,&key );
						}
					}
				}
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimSequence* CAnimEntityNode::LoadFacialSequence(const char* sequenceName)
{
	FacialSequenceMap::iterator loadedSequencePosition = m_facialSequences.find(CONST_TEMP_STRING(sequenceName));
	if (loadedSequencePosition == m_facialSequences.end())
	{
		IEntity* pEntity = GetEntity();
		ICharacterInstance* pCharacter = (pEntity ? pEntity->GetCharacter(0) : 0);
		IFacialInstance* pFaceInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
		IFacialAnimSequence* pSequence = (pFaceInstance ? pFaceInstance->LoadSequence( sequenceName, false ) : 0);

		// Add the sequence to the cache, even if the pointer is 0 - stop us from continually trying to load a missing sequence.
		loadedSequencePosition = m_facialSequences.insert(std::make_pair(sequenceName, pSequence)).first;
	}

	return (loadedSequencePosition != m_facialSequences.end() ? (*loadedSequencePosition).second : _smart_ptr<IFacialAnimSequence>(0));
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::ReleaseAllFacialSequences()
{
	IEntity* pEntity = GetEntity();
	ICharacterInstance* pCharacter = (pEntity ? pEntity->GetCharacter(0) : 0);
	IFacialInstance* pFaceInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
	if (pFaceInstance)
		pFaceInstance->StopSequence(eFacialSequenceLayer_Trackview);

	// If commented out facial sequence will not be released for this level and will stay cached
	//m_facialSequences.clear();
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::SelectLookIKBlends(ELookAtKeyBoneSet boneSet, float* blends)
{
	switch (boneSet)
	{
		case eLookAtKeyBoneSet_Eyes:
			blends[0] = 0.0f;
			blends[1] = 0.0f;
			blends[2] = 0.0f;
			blends[3] = 0.0f;
			blends[4] = 0.0f;
			break;

		case eLookAtKeyBoneSet_SpineHeadEyes:
			blends[0] = 0.04f;
			blends[1] = 0.06f;
			blends[2] = 0.08f;
			blends[3] = 0.15f;
			blends[4] = 0.60f;
			break;

		case eLookAtKeyBoneSet_HeadEyes:
		default:
			blends[0] = 0.0f;
			blends[1] = 0.0f;
			blends[2] = 0.0f;
			blends[3] = 0.0f;
			blends[4] = 0.60f;
			break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::Activate( bool bActivate )
{
	if (bActivate)
	{
		PrepareAnimations();
	}
	else
	{
		// reset.
		// Reset called explicitly by sequence.
	}
	m_lookAtTarget = "";
	m_lookAtEntityId = 0;
	m_allowAdditionalTransforms = true;
	m_boneSet = eLookAtKeyBoneSet_HeadEyes;
};

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::Pause()
{
	ReleaseSounds();
}

/*
//////////////////////////////////////////////////////////////////////////
Vec3 CAnimEntityNode::GetPos( float time )
{
	Vec3 pos(0,0,0);
	IAnimBlock *anim = GetAnimBlock();
	if (!anim)
		return pos;
	IAnimTrack *posTrack = anim->GetTrack(APARAM_POS);
	if (posTrack)
		posTrack->GetValue( time,pos );
	return pos;
}

//////////////////////////////////////////////////////////////////////////
Quat CAnimEntityNode::GetRotate( float time )
{
	Quat q;
	IAnimBlock *anim = GetAnimBlock();
	if (!anim)
		return q;
	IAnimTrack *rotTrack = anim->GetTrack(APARAM_ROT);
	if (rotTrack)
		rotTrack->GetValue( time,q );
	return q;
}

//////////////////////////////////////////////////////////////////////////
Vec3 CAnimEntityNode::GetScale( float time )
{
	Vec3 scl(1,1,1);
	IAnimBlock *anim = GetAnimBlock();
	if (!anim)
		return scl;
	IAnimTrack *sclTrack = anim->GetTrack(APARAM_SCL);
	if (sclTrack)
		sclTrack->GetValue( time,scl );
	return scl;
}
*/

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::SetPos( float time,const Vec3 &pos )
{
	bool bDefault = !(m_pMovieSystem->IsRecording() && (m_flags&ANODE_FLAG_SELECTED)); // Only selected nodes can be recorded
	IAnimBlock *anim = GetAnimBlock();
	if (anim)
	{
		IAnimTrack *posTrack = anim->GetTrack(APARAM_POS);
		if (posTrack)
		{
			posTrack->SetValue( time,pos,bDefault );
			if (bDefault && posTrack->GetNumKeys() > 0)
			{
				// Track is not empty. offset all keys by move ammount.
				Vec3 offset = pos - m_pos;
				// Iterator over all keys.
				ITcbKey key;
				for (int i = 0; i < posTrack->GetNumKeys(); i++)
				{
					// Offset each key.
					posTrack->GetKey(i,&key);
					key.SetVec3( key.GetVec3()+offset );
					posTrack->SetKey(i,&key);
				}
			}
		}
	}
	m_pos = pos;
}
	
//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::SetRotate( float time,const Quat &quat )
{
	m_rotate = quat;

	bool bDefault = !(m_pMovieSystem->IsRecording() && (m_flags&ANODE_FLAG_SELECTED)); // Only selected nodes can be recorded
	IAnimBlock *anim = GetAnimBlock();
	if (anim)
	{
		IAnimTrack *rotTrack = anim->GetTrack(APARAM_ROT);
		if (rotTrack)
			rotTrack->SetValue( time,m_rotate,bDefault );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::SetScale( float time,const Vec3 &scale )
{
	m_scale = scale;

	bool bDefault = !(m_pMovieSystem->IsRecording() && (m_flags&ANODE_FLAG_SELECTED)); // Only selected nodes can be recorded
	IAnimBlock *anim = GetAnimBlock();
	if (anim)
	{
		IAnimTrack *sclTrack = anim->GetTrack(APARAM_SCL);
		if (sclTrack)
			sclTrack->SetValue( time,scale,bDefault );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::ApplyEventKey( CEventTrack *track,int keyIndex,IEventKey &key )
{
	IEntity* pEntity = GetEntity();
	if (!pEntity)
		return;

	if (*key.animation) // if there is an animation
	{
		// Start playing animation.
		ICharacterInstance *pCharacter = pEntity->GetCharacter(0);
		if (pCharacter)
		{
			CryCharAnimationParams aparams;
			//aparams.fBlendInTime = 0.5f;
			//aparams.fBlendOutTime = 0.5f;
			aparams.m_fTransTime  = 0.15f;

			aparams.m_fTransTime    = 0.0f;
			aparams.m_nFlags = CA_TRACK_VIEW_EXCLUSIVE;

			ISkeletonAnim* pISkeletonAnim = pCharacter->GetISkeletonAnim();
			
			aparams.m_nLayerID = 0;
			pISkeletonAnim->StartAnimation( key.animation,0, 0,0,  aparams );
		//	aparams.m_nLayerID = 1;
		//	pISkeleton->StartAnimation( key.animation,0, 0,0, aparams );
		//	aparams.m_nLayerID = 2;
		//	pISkeleton->StartAnimation( key.animation,0, 0,0, aparams );

			IAnimationSet* pAnimations = pCharacter->GetIAnimationSet();
			assert (pAnimations);
			//float duration = pAnimations->GetLength( key.animation );

			int animId = pAnimations->GetAnimIDByName( key.animation );
			if (animId >= 0)
			{
				float duration = pAnimations->GetDuration_sec(animId);
				if (duration != key.duration)
				{
					key.duration = duration;
					track->SetKey( keyIndex,&key );
				}
			}
		}
		//char str[1024];
		//sprintf( str,"StartAnim: %s",key.animation );
		//GetMovieSystem()->GetSystem()->GetILog()->LogToConsole( str );
	}
	
	if (*key.event) // if there's an event
	{
		// Fire event on Entity.
		IEntityScriptProxy *pScriptProxy = (IEntityScriptProxy*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
		if (pScriptProxy)
			pScriptProxy->CallEvent( key.event );
	}
	//m_entity->Hide( key.hidden );
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::ApplySoundKey( IAnimTrack *pTrack,int nCurrKey,int nLayer, ISoundKey &key, SAnimContext &ec )
{
	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	// there is not audio key, so lets stop any sound
	if (nCurrKey == -1)
	{
		if (m_SoundInfo[nLayer].pSound)
		{
			m_SoundInfo[nLayer].pSound->Stop();
			m_SoundInfo[nLayer].pSound = NULL;
		}
		return;
	}

	if (m_SoundInfo[nLayer].nLastKey == nCurrKey)
		return;

	// stop any sound that is playing on that track
	if (m_SoundInfo[nLayer].pSound)
	{
		m_SoundInfo[nLayer].pSound->Stop();
		m_SoundInfo[nLayer].pSound = NULL;
	}

	int flags = 0;

	bool bNewSound = false;

	if (((strcmp(m_SoundInfo[nLayer].sLastFilename.c_str(), key.pszFilename) != 0) || (!m_SoundInfo[nLayer].pSound)))
	{
	
		//if (key.bStream)
			//flags |= FLAG_SOUND_STREAM;
		//else
			//flags |= FLAG_SOUND_LOAD_SYNCHRONOUSLY; // Always synchronously for now.

		if (key.bLoop)
			flags |= FLAG_SOUND_LOOP;

		if (key.bVoice)
			flags |= FLAG_SOUND_VOICE;

		if (key.b3DSound)
		{
			// 3D sound.
			flags |= FLAG_SOUND_DEFAULT_3D;
		}
		else
		{
			// 2D sound.
			flags |= FLAG_SOUND_2D|FLAG_SOUND_STEREO|FLAG_SOUND_16BITS;
		} 
		// we have a different sound now
		if (m_SoundInfo[nLayer].pSound)
			m_SoundInfo[nLayer].pSound->Stop();
		
		if (gEnv->pSoundSystem)
			m_SoundInfo[nLayer].pSound = gEnv->pSoundSystem->CreateSound(key.pszFilename, flags | FLAG_SOUND_UNSCALABLE | FLAG_SOUND_MOVIE );

		m_SoundInfo[nLayer].sLastFilename = key.pszFilename;
		if (m_SoundInfo[nLayer].pSound)
		{
			if (key.bVoice)
				m_SoundInfo[nLayer].pSound->SetSemantic((ESoundSemantic)(eSoundSemantic_TrackView|eSoundSemantic_Dialog));
			else
				m_SoundInfo[nLayer].pSound->SetSemantic(eSoundSemantic_TrackView);

			m_SoundInfo[nLayer].nLength = m_SoundInfo[nLayer].pSound->GetLengthMs();
			
			if (m_SoundInfo[nLayer].nLength > 0)
				key.fDuration = ((float)m_SoundInfo[nLayer].nLength+10) / 1000.0f;
			
			pTrack->SetKey( nCurrKey,&key ); // Update key duration.
		}
		bNewSound = true;
	}

	if (!m_SoundInfo[nLayer].pSound)
		return;
	
	m_SoundInfo[nLayer].pSound->SetSoundPriority( MOVIE_SOUND_PRIORITY );
	m_SoundInfo[nLayer].pSound->SetVolume(key.fVolume);

	Vec3 SoundPos = pEntity->GetPos();
	Vec3 SoundDir = pEntity->GetWorldTM().TransformVector(FORWARD_DIRECTION);
	Vec3 vCharacterCenter(0);

	bool bHead = false;
	bool bCharacter = false;

	if (m_SoundInfo[nLayer].pSound->GetFlags() & FLAG_SOUND_3D)
	{
		// 3D sound.
		// testing for head position
		ICharacterInstance * pCharacter = pEntity->GetCharacter(0);
		if (pCharacter)
		{
			bCharacter = true;
			vCharacterCenter = pCharacter->GetAABB().GetCenter();
			SoundPos = pEntity->GetWorldTM()*pCharacter->GetAABB().GetCenter();

			if (m_SoundInfo[nLayer].pSound->GetFlags() & FLAG_SOUND_VOICE)
			{
				bHead = true;
			}
		}

		m_SoundInfo[nLayer].pSound->SetPosition( SoundPos );
		m_SoundInfo[nLayer].pSound->SetDirection( SoundDir );

		if (key.inRadius > 0 && key.outRadius > 0)
			m_SoundInfo[nLayer].pSound->SetMinMaxDistance( key.inRadius,key.outRadius );
	}
	else
	{
		// 2D sound.
		m_SoundInfo[nLayer].pSound->SetPan(key.nPan);
	} 

	int nOffset = (int)((ec.time-key.time)*1000.0f);
	if (nOffset > 30 && nOffset < m_SoundInfo[nLayer].nLength) // small 30ms threshold to not skip start
	{
		m_SoundInfo[nLayer].pSound->SetCurrentSamplePos(nOffset, true);
	}

	if (m_SoundInfo[nLayer].nLength != 0 && nOffset > m_SoundInfo[nLayer].nLength && !key.bLoop)
	{
		// If time is outside of sound, do not start it.
		bNewSound = false;
	}

	if (bNewSound)
	{
		((CMovieSystem*)m_pMovie)->OnPlaySound( ec.sequence, m_SoundInfo[nLayer].pSound );
		if (!m_SoundInfo[nLayer].pSound->IsPlaying())
		{
			IEntitySoundProxy *pSoundProxy = (IEntitySoundProxy*)pEntity->CreateProxy(ENTITY_PROXY_SOUND);

			if (pSoundProxy)
			{
				Vec3 vOffset(0);

				if (!bHead && bCharacter)
				{
					vOffset = vCharacterCenter;
				}

				pSoundProxy->PlaySound(m_SoundInfo[nLayer].pSound, vOffset, FORWARD_DIRECTION, 1.0f, key.bLipSync);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::ReleaseAllAnims()
{
	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	ICharacterInstance *pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return;
	IAnimationSet* pAnimations = pCharacter->GetIAnimationSet();
	assert(pAnimations);
	for (TStringSetIt It=m_setAnimationSinks.begin();It!=m_setAnimationSinks.end();++It)
	{
		const char *pszName=(*It).c_str();
		//pCharacter->RemoveAnimationEventSink(pszName, this);
		//pAnimations->UnloadAnimation(pszName);
	}
	m_setAnimationSinks.clear();

	if (m_nPlayingAnimations > 0)
	{
		m_bPlayingAnimationAtLayer[0]=m_bPlayingAnimationAtLayer[1]=m_bPlayingAnimationAtLayer[2] = 0;
		pCharacter->GetISkeletonAnim()->SetTrackViewExclusive(0);

		pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();

		pCharacter->SetAnimationSpeed(1.0000f);
		pCharacter->GetISkeletonAnim()->SetAnimationDrivenMotion(m_bWasTransRot);
		m_nPlayingAnimations = 0;
	}

	ReleaseAllFacialSequences();
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::OnEndAnimation(const char *sAnimation)
{
	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	TStringSetIt It=m_setAnimationSinks.find(sAnimation);
	if (It==m_setAnimationSinks.end())
		return;	// this anim was not started by us...
	m_setAnimationSinks.erase(It);
	ICharacterInstance *pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return;

	IAnimationSet* pAnimations = pCharacter->GetIAnimationSet();
	assert(pAnimations);
	//pCharacter->RemoveAnimationEventSink(sAnimation, this);
	//pAnimations->UnloadAnimation(sAnimation);
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::AnimateCharacterTrack( class CCharacterTrack* track,SAnimContext &ec,int layer )
{
	ISystem* pISystem = GetMovieSystem()->GetSystem();
	IRenderer* pIRenderer			= pISystem->GetIRenderer();
	IRenderAuxGeom* pAuxGeom	= pIRenderer->GetIRenderAuxGeom();



	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	ICharacterInstance *pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return;

	ISkeletonAnim* pISkeletonAnim = pCharacter->GetISkeletonAnim();

	ICharacterKey key;
	int currKey = track->GetActiveKey(ec.time,&key);
	// If key is different or if time is standing exactly on key time.
	if (currKey != m_lastCharacterKey[layer] || key.time == ec.time || ec.time < m_time)
	{
		m_lastCharacterKey[layer] = currKey;
		float t = ec.time - key.time;
		t = key.startTime + t*key.speed;
		if (key.animation[0] && (t < key.duration || key.bLoop))
		{
			// retrieve the animation collection for the model
			IAnimationSet* pAnimations = pCharacter->GetIAnimationSet();
			assert (pAnimations);

			if (key.bUnload)
			{
				m_setAnimationSinks.insert(TStringSetIt::value_type(key.animation));
				//pCharacter->AddAnimationEventSink(key.animation, this);
			}

			if (pISkeletonAnim->GetAnimationDrivenMotion() && m_nPlayingAnimations == 0)
				m_bWasTransRot = true;
			
			if (key.bInPlace)
			{
				pISkeletonAnim->SetAnimationDrivenMotion(1);
			}
			else
			{
				pISkeletonAnim->StopAnimationInLayer(0,0.0f);
				pISkeletonAnim->SetAnimationDrivenMotion(0);
			}

			pISkeletonAnim->SetTrackViewExclusive(1);

			m_bPlayingAnimationAtLayer[layer] = true;
			m_nPlayingAnimations++;

			// Start playing animation.
			CryCharAnimationParams aparams;
			aparams.m_fTransTime = key.blendTime;

		//	aparams.fTransTime    = key.blendTime;
			aparams.m_nFlags = CA_TRACK_VIEW_EXCLUSIVE;
			if (key.bLoop)
				aparams.m_nFlags |= CA_LOOP_ANIMATION;
	
			aparams.m_nLayerID = layer+0;
			pISkeletonAnim->StartAnimation( key.animation,0,  0,0, aparams );

			int animId = pAnimations->GetAnimIDByName( key.animation );
			if (animId >= 0)
			{
				//pCharacter->EnableTimeUpdate(false);
				//pCharacter->SetAnimationSpeed(layer, 0.0000f);
				//float duration = pAnimations->GetLength(animId);
				float duration = pAnimations->GetDuration_sec(animId);// + pAnimations->GetStart(animId);
				if (duration != key.duration)
				{
					key.duration = duration;
					track->SetKey( currKey,&key );
				}
				/*
				if (key.bLoop)
				{
				if (animId >= 0)
				pAnimations->SetLoop( animId,key.bLoop );
				}
				*/
			}
		}
	}

	bool bSomeKeyActive = false;

	// Animate.
	if (currKey >= 0)
	{
		float t = ec.time - key.time;
		t = key.startTime + t*key.speed;
		if (t < key.duration || key.bLoop)
		{
			bSomeKeyActive = true;
			if (key.bLoop)
			{
				t = fmod(t,key.duration);
			}
			pCharacter->SetAnimationSpeed(0.0000f);
			if (ec.dt > 0.0001f && key.duration > 0.0001f) // This should stop this call if the user is moving the time in the editor.
				pISkeletonAnim->ManualSeekAnimationInFIFO(0, 0, t/key.duration, true); // Make sure anim events get triggered.
			pISkeletonAnim->SetLayerTime(layer+0,t);
			pISkeletonAnim->SetLayerTime(layer+1,t);
			pISkeletonAnim->SetLayerTime(layer+2,t);
		}
	}

	// Stop animation.
	if (!bSomeKeyActive && m_bPlayingAnimationAtLayer[layer])
	{
		m_bPlayingAnimationAtLayer[layer] = false;
		m_nPlayingAnimations--;

		if (m_nPlayingAnimations == 0)
		{
			// Not inside animation.
			pCharacter->GetISkeletonAnim()->SetTrackViewExclusive(0);
			
			pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();

			pCharacter->SetAnimationSpeed(1.0000f);
			pCharacter->GetISkeletonAnim()->SetAnimationDrivenMotion(m_bWasTransRot);
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::StopExpressions()
{
	if (m_setExpressions.empty())
		return;

	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	/*
	IEntityCharacter *pEntChar=m_entity->GetCharInterface();
	if (!pEntChar)
		return;
	ILipSync *pLipSync=pEntChar->GetLipSyncInterface();
	if (!pLipSync)
		return;
	for (TStringSetIt It=m_setExpressions.begin();It!=m_setExpressions.end();++It)
	{
		pLipSync->StopExpression((*It).c_str());
	}
	*/
	ICharacterInstance* pInst = pEntity->GetCharacter(0);
	if (pInst)
	{
   pInst->GetIMorphing()->StopAllMorphs();
	}

	m_setExpressions.clear();
}

void CAnimEntityNode::AnimateExpressionTrack(CExprTrack *pTrack, SAnimContext &ec)
{
	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	IExprKey Key;
	int nKeys=pTrack->GetNumKeys();
	// we go through all the keys, since expressions may overlap
	for (int nKey=0;nKey<nKeys;nKey++)
	{
		pTrack->GetKey(nKey, &Key);
		if ((!Key.pszName) || (!Key.pszName[0]))
			return;

		float fKeyLentgh=Key.fBlendIn+Key.fHold+Key.fBlendOut;
		float fOffset=ec.time-Key.time;

		CryCharMorphParams MorphParams;
		MorphParams.m_fAmplitude=Key.fAmp;
		MorphParams.m_fBlendIn=Key.fBlendIn;
		MorphParams.m_fBlendOut=Key.fBlendOut;
		MorphParams.m_fLength=Key.fHold;
		MorphParams.m_fStartTime=fOffset;
		//MorphParams.m_nFlags |= MorphParams.FLAGS_RECURSIVE;
		ICharacterInstance* pInst = pEntity->GetCharacter(0);
		if (pInst)
		{
	//		pInst->StartMorph (Key.pszName, MorphParams);

	//	bool CLipSync::DoExpression(const char* pszMorphTarget, CryCharMorphParams &MorphParams, bool bAnim)
	//	{
	//		if (!m_pCharInst)
	//			return false; // not initialized
	//		ICryCharModel *pModel=m_pCharInst->GetModel();
	//		ASSERT(pModel);
	//		IAnimationSet *pAnimSet=pModel->GetAnimationSet();
	//		ASSERT(pAnimSet);
	//		int nMorphTargetId=pAnimSet->FindMorphTarget(pszMorphTarget);
	//		if (nMorphTargetId==-1)
	//			return false;	// no such morph-target
			// try to set time first in case it is already playing
			//	if (!m_pCharInst->SetMorphTime(nMorphTargetId, MorphParams.fStartTime))

			if ((Key.time>ec.time) || (fOffset>=fKeyLentgh))
			{
				continue;
			}
			
			IMorphing* pIMorphing = pInst->GetIMorphing();
			pIMorphing->StopMorph(Key.pszName);
			pIMorphing->StartMorph(Key.pszName, MorphParams);
			pIMorphing->SetMorphSpeed(Key.pszName, 0.0f);
		}

		/*
		IEntityCharacter *pEntChar=m_entity->GetCharInterface();
		if (!pEntChar)
			return;
		ILipSync *pLipSync=pEntChar->GetLipSyncInterface();
		if (pLipSync)
		{
			float fKeyLentgh=Key.fBlendIn+Key.fHold+Key.fBlendOut;
			float fOffset=ec.time-Key.time;
			if ((Key.time>ec.time) || (fOffset>=fKeyLentgh))
			{
//				pLipSync->StopExpression(Key.pszName);
				continue;
			}
			CryCharMorphParams MorphParams;
			MorphParams.fAmplitude=Key.fAmp;
			MorphParams.fBlendIn=Key.fBlendIn;
			MorphParams.fBlendOut=Key.fBlendOut;
			MorphParams.fLength=Key.fHold;
			MorphParams.fStartTime=fOffset;
			if (pLipSync->DoExpression(Key.pszName, MorphParams, false))
				m_setExpressions.insert(Key.pszName);
		}
		*/
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::AnimateFacialSequence( CFaceSeqTrack *pTrack, SAnimContext &ec )
{
	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	ICharacterInstance *pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return;

	IFacialInstance *pFaceInstance = pCharacter->GetFacialInstance();
	if (!pFaceInstance)
		return;

	IFaceSeqKey key;
	int nkey = pTrack->GetActiveKey(ec.time,&key);
	if (nkey >= 0 && nkey < pTrack->GetNumKeys())
	{
		_smart_ptr<IFacialAnimSequence> pSeq = LoadFacialSequence(key.szSelection);
		if (pSeq)
		{
			key.fDuration = pSeq->GetTimeRange().Length();
			pTrack->SetKey(nkey, &key);

			float t = ec.time - key.time;
			if (t <= key.fDuration)
			{
				if (!pFaceInstance->IsPlaySequence(pSeq, eFacialSequenceLayer_Trackview))
				{
					pFaceInstance->PlaySequence(pSeq, eFacialSequenceLayer_Trackview);
					pFaceInstance->PauseSequence(eFacialSequenceLayer_Trackview, true);
				}
				pFaceInstance->SeekSequence(eFacialSequenceLayer_Trackview,t);
			}
			else
			{
				if (pFaceInstance->IsPlaySequence(pSeq, eFacialSequenceLayer_Trackview))
					pFaceInstance->StopSequence(eFacialSequenceLayer_Trackview);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::AnimateLookAt( CLookAtTrack *pTrack, SAnimContext &ec )
{
	IEntity *pEntity = GetEntity();
	if (!pEntity)
		return;

	ICharacterInstance *pCharacter = 0;

	IPipeUser* pAIPipeUser = NULL;
	IAIObject* pAI = pEntity->GetAI();
	if (pAI)
	{
		pAIPipeUser = pAI->CastToIPipeUser();
	}
	if (!pAIPipeUser)
		pCharacter = pEntity->GetCharacter(0);

	EntityId lookAtEntityId = 0;
	bool allowAdditionalTransforms = 0;
	ELookAtKeyBoneSet boneSet = eLookAtKeyBoneSet_HeadEyes;

	ILookAtKey key;
	int nkey = pTrack->GetActiveKey(ec.time,&key);
	allowAdditionalTransforms = true;
	boneSet = eLookAtKeyBoneSet_HeadEyes;
	if (nkey >= 0)
	{
		allowAdditionalTransforms = key.bAllowAdditionalTransforms;
		boneSet = key.boneSet;
		if ((m_lookAtTarget[0] && !m_lookAtEntityId) || strcmp(key.szSelection,m_lookAtTarget) != 0)
		{
			m_lookAtTarget = key.szSelection;
			IEntity *pTargetEntity = gEnv->pEntitySystem->FindEntityByName(key.szSelection);
			if (pTargetEntity)
			{
				lookAtEntityId = pTargetEntity->GetId();
			}
			else
				lookAtEntityId = 0;
		}
		else
		{
			lookAtEntityId = m_lookAtEntityId;
		}
	}
	else
	{
		lookAtEntityId = 0;
		m_lookAtTarget = "";
		allowAdditionalTransforms = true;
		boneSet = eLookAtKeyBoneSet_HeadEyes;
	}

	if (m_lookAtEntityId != lookAtEntityId || m_allowAdditionalTransforms != allowAdditionalTransforms || m_boneSet != boneSet)
	{
		m_lookAtEntityId = lookAtEntityId;
		m_allowAdditionalTransforms = allowAdditionalTransforms;
		m_boneSet = boneSet;

		// We need to enable smoothing for the facial animations, since look ik can override them and cause snapping.
		IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
		if (pFacialInstance)
			pFacialInstance->TemporarilyEnableBoneRotationSmoothing();
	}

	IEntity *pLookAtEntity = 0;
	if (m_lookAtEntityId)
		pLookAtEntity = gEnv->pEntitySystem->GetEntity(m_lookAtEntityId);

	if (pLookAtEntity)
	{
		Vec3 pos = pLookAtEntity->GetWorldPos();
		ICharacterInstance *pLookAtChar = pLookAtEntity->GetCharacter(0);
		if (pLookAtChar)
		{
			// Try look at head bone.
			int16 nHeadBoneId = pLookAtChar->GetISkeletonPose()->GetJointIDByName( HEAD_BONE_NAME );
			if (nHeadBoneId >= 0)
			{
				pos = pLookAtEntity->GetWorldTM().TransformPoint( pLookAtChar->GetISkeletonPose()->GetAbsJointByID(nHeadBoneId).t );
			//	gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere( pos,0.2f,ColorB(255,0,0,255) );
			}
		}
		float blends[5];
		SelectLookIKBlends(m_boneSet, blends);
		if (pCharacter)
			pCharacter->GetISkeletonPose()->SetLookIK( true,DEG2RAD(120),pos,blends,m_allowAdditionalTransforms );
		if (pAIPipeUser)
			pAIPipeUser->SetLookAtPoint(pos);
	}
	else
	{
		if (pCharacter)
			pCharacter->GetISkeletonPose()->SetLookIK( false,false,Vec3(0,0,0) );
		if (pAIPipeUser)
			pAIPipeUser->ResetLookAt();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::SetAnimBlock( IAnimBlock *block )
{
	CAnimNode::SetAnimBlock(block);
	if (block)
	{
		// Initialize new block with default track values.
		// Used exlusively in Editor.
		IAnimTrack *posTrack = block->GetTrack(APARAM_POS);
		IAnimTrack *rotTrack = block->GetTrack(APARAM_ROT);
		IAnimTrack *sclTrack = block->GetTrack(APARAM_SCL);
		if (posTrack)
			posTrack->SetValue( 0,m_pos,true );
		if (rotTrack)
			rotTrack->SetValue( 0,m_rotate,true );
		if (sclTrack)
			sclTrack->SetValue( 0,m_scale,true );
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimEntityNode::Serialize( XmlNodeRef &xmlNode,bool bLoading )
{
	CAnimNode::Serialize( xmlNode,bLoading );
	if (bLoading)
	{
		xmlNode->getAttr( "Pos",m_pos );
		xmlNode->getAttr( "Rotate",m_rotate );
		xmlNode->getAttr( "Scale",m_scale );
		xmlNode->getAttr( "EntityGUID",m_entityGuid );
	}
	else
	{
		xmlNode->setAttr( "Pos",m_pos );
		xmlNode->setAttr( "Rotate",m_rotate );
		xmlNode->setAttr( "Scale",m_scale );
		xmlNode->setAttr( "EntityGUID",m_entityGuid );
	}
}
#undef s_nodeParamsInitialized
#undef s_nodeParams
#undef AddSupportedParam

