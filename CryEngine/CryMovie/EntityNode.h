////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   entitynode.h
//  Version:     v1.00
//  Created:     23/4/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __entitynode_h__
#define __entitynode_h__

#if _MSC_VER > 1000
//#pragma once
#endif

#include <set>
#include "AnimNode.h"
#include "SoundTrack.h"
#include "StlUtils.h"

#include <IFacialAnimation.h>

#define ENTITY_SOUNDTRACKS	3
#define ENTITY_EXPRTRACKS		3

typedef std::set<string>	TStringSet;
typedef TStringSet::iterator	TStringSetIt;

class CAnimEntityNode : public CAnimNode
{
public:
	CAnimEntityNode( IMovieSystem *sys );
	~CAnimEntityNode();

	virtual EAnimNodeType GetType() const { return ANODE_ENTITY; }

	virtual void SetEntityGuid( const EntityGUID &guid );
	virtual EntityGUID* GetEntityGuid() { return &m_entityGuid; };
	virtual IEntity* GetEntity();

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CAnimNode
	//////////////////////////////////////////////////////////////////////////
	virtual void SyncUpdate( ESyncUpdateType type,SAnimContext &ac );
	virtual void Animate( SAnimContext &ec );
	virtual void CreateDefaultTracks();

	void SetPos( float time,const Vec3 &pos );
	void SetRotate( float time,const Quat &quat );
	void SetScale( float time,const Vec3 &scale );

	float GetTime() { return m_time; };
	Vec3 GetPos() { return m_pos; };
	Quat GetRotate() { return m_rotate; };
	Vec3 GetScale() { return m_scale; };

	// ovverided from IAnimNode
//	IAnimBlock* CreateAnimBlock();

	// Ovveride SetAnimBlock
	void SetAnimBlock( IAnimBlock *block );
	virtual void Activate( bool bActivate );

	//////////////////////////////////////////////////////////////////////////
	void Serialize( XmlNodeRef &xmlNode,bool bLoading );
	void Reset();
	void Pause();

	//////////////////////////////////////////////////////////////////////////
	virtual int GetParamCount() const;
	virtual bool GetParamInfo( int nIndex, SParamInfo &info ) const;
	virtual bool GetParamInfoFromId( int paramId, SParamInfo &info ) const;

	static void AddSupportedParams( std::vector<IAnimNode::SParamInfo> &nodeParams );

protected:
	void ReleaseSounds();
	void ApplyEventKey( class CEventTrack *track,int keyIndex,IEventKey &key );
	void ApplySoundKey( IAnimTrack *pTrack,int nCurrKey,int nLayer, ISoundKey &key, SAnimContext &ec);
	void AnimateCharacterTrack( class CCharacterTrack* track,SAnimContext &ec,int layer );
	void StopExpressions();
	void AnimateExpressionTrack(class CExprTrack *pTrack, SAnimContext &ec);
	void AnimateFacialSequence(class CFaceSeqTrack *pTrack, SAnimContext &ec);
	void AnimateLookAt(class CLookAtTrack *pTrack, SAnimContext &ec);

	void ReleaseAllAnims();
	virtual void OnStartAnimation(const char *sAnimation) {}
	virtual void OnEndAnimation(const char *sAnimation);

	void PrepareAnimations();

	IFacialAnimSequence* LoadFacialSequence(const char* sequenceName);
	void ReleaseAllFacialSequences();

	void SelectLookIKBlends(ELookAtKeyBoneSet boneSet, float* blends);

protected:
	//! Reference to game entity.
	EntityGUID m_entityGuid;
	EntityId m_EntityId;

	TStringSet m_setAnimationSinks;

	IMovieSystem *m_pMovie;

	//! Pointer to target animation node.
	_smart_ptr<IAnimNode> m_target;

	// Cached parameters of node at given time.
	float m_time;
	Vec3 m_pos;
	Quat m_rotate;
	Vec3 m_scale;
	bool m_bWasTransRot;
	int m_nPlayingAnimations;

	bool m_visible;

	//! Last animated key in Entity track.
	int m_lastEntityKey;
	int m_lastCharacterKey[3];
	bool m_bPlayingAnimationAtLayer[3];
	//int m_nLastFacialSequenceKey;
	SSoundInfo m_SoundInfo[ENTITY_SOUNDTRACKS];
	TStringSet m_setExpressions;
	
	string m_lookAtTarget;
	EntityId m_lookAtEntityId;
	bool m_allowAdditionalTransforms;
	ELookAtKeyBoneSet m_boneSet;

	typedef std::map<string,_smart_ptr<IFacialAnimSequence>,stl::less_stricmp<string> > FacialSequenceMap;
	FacialSequenceMap m_facialSequences;
};

#endif // __entitynode_h__
