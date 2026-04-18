////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   SoundProxy.h
//  Version:     v1.00
//  Created:     26/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SoundProxy_h__
#define __SoundProxy_h__
#pragma once

#include <ISound.h>

// forward declarations.
struct SEntityEvent;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
struct CSoundProxy : public IEntitySoundProxy, public ISoundEventListener
{
	CSoundProxy( CEntity *pEntity );
	virtual IEntity* GetEntity() const { return (IEntity*) m_pEntity; };

	//////////////////////////////////////////////////////////////////////////
	// IEntityProxy interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetType() { return ENTITY_PROXY_SOUND; }
	virtual void Release();
	virtual void Done() {};
	virtual	void Update( SEntityUpdateContext &ctx ) {}
	virtual	void ProcessEvent( SEntityEvent &event );
	virtual void Init( IEntity *pEntity,SEntitySpawnParams &params ) {};
	virtual void SerializeXML( XmlNodeRef &entityNode,bool bLoading ) {};
	virtual void Serialize( TSerialize ser );
	virtual bool NeedSerialize() { return false; };
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntitySoundProxy interface.
	//////////////////////////////////////////////////////////////////////////
	// Play a sound, return internal slot in entity where this sound is played.
	virtual ISound* GetSound( tSoundID nSoundId );
	virtual ISound* GetSoundByIndex(uint32 nIndex); // will return NULL on invalid index
	virtual bool PlaySound( ISound *pSound, const Vec3 &vPos=Vec3(0,0,0),const Vec3 &vDirection=FORWARD_DIRECTION,float fSoundScale=1.0f, bool bLipSync=true );
	// new calls
	virtual tSoundID PlaySound( const char *sGroupAndSoundName, const Vec3 &vOffset,const Vec3 &vDirection, uint32 nSoundFlags, ESoundSemantic eSemantic);
	virtual tSoundID PlaySound( const char *sGroupAndSoundName, const Vec3 &vOffset,const Vec3 &vDirection, uint32 nSoundFlags, ESoundSemantic eSemantic, EntityId *pSkipEnts=0,int nSkipEnts=0);
	virtual tSoundID PlaySoundEx( const char *sGroupAndSoundName, const Vec3 &vOffset,const Vec3 &vDirection, uint32 nSoundFlags, float fVolume, float fMinRadius,float fMaxRadius, ESoundSemantic eSemantic);
	virtual tSoundID PlaySoundEx( const char *sGroupAndSoundName, const Vec3 &vOffset,const Vec3 &vDirection, uint32 nSoundFlags, float fVolume, float fMinRadius,float fMaxRadius, ESoundSemantic eSemantic, EntityId *pSkipEnts=0,int nSkipEnts=0);
	virtual bool SetStaticSound(tSoundID nSoundId, bool bStatic);
	virtual bool GetStaticSound(const tSoundID nSoundId);
	virtual void PauseSound( tSoundID nSoundId, bool bPause );
	virtual void StopSound( tSoundID nSoundId, ESoundStopMode eStopMode=ESoundStopMode_EventFade );
	virtual void StopAllSounds();
	virtual void SetSoundPos( tSoundID nSoundId, const Vec3 &vPos );
	virtual Vec3 GetSoundPos( tSoundID nSoundId );
	virtual void SetSoundDirection( tSoundID nSoundId, const Vec3 &dir );
	virtual Vec3 GetSoundDirection( tSoundID nSoundId );
	virtual void SetEffectRadius(float fEffectRadius ) { m_fEffectRadius = fEffectRadius; };
	virtual float GetEffectRadius( ) { return m_fEffectRadius; };
	virtual const char* GetTailName() { return m_sTailname.c_str(); }
	virtual void CheckVisibilityForTailName(const float fLength, const float fDistanceToRecalculate);

	virtual void UpdateSounds();
	

	//////////////////////////////////////////////////////////////////////////
	// ISoundEventListener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnSoundEvent( ESoundCallbackEvent event,ISound *pSound );
	//////////////////////////////////////////////////////////////////////////

private:
	//////////////////////////////////////////////////////////////////////////
	struct SAttachedSound
	{
		tSoundID nSoundID;
		_smart_ptr<ISound> pSound;
		Vec3 vOffset;
		Vec3 vDirection;
		bool bOffset;
		bool bStatic;
	};
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Private methods.
	//////////////////////////////////////////////////////////////////////////
	void OnMove();
	void OnEnter(IEntity *pWho, IEntity *pArea);
	void OnLeave(IEntity *pWho, IEntity *pArea);
	void OnLeaveNear(IEntity *pWho, IEntity *pArea);
	void OnMoveNear(IEntity *pWho, IEntity *pArea, float fFade, float fDistanceSq);
	void OnMoveInside(IEntity *pWho, IEntity *pArea);
	void OnExclusiveMoveInside(IEntity *pWho, IEntity *pAreaHigh, IEntity *pAreaLow);

	SAttachedSound* FindAttachedSound( tSoundID nSoundId );
	bool AddAttachedSound(ISound* pSound, const bool bOffset, const Vec3 &vOffset, const Vec3 &vDirection);
	bool RemoveAttachedSound( tSoundID nSoundId );
	void OnHide(bool bHide);
	void RetrieveTailName(IEntity *pArea);
	
	void UpdateHeadTransformation();
	void SetSoundPosition(ISound *pSound, const Vec3 *vOffset, const Vec3 *vDirection);

private:
	//////////////////////////////////////////////////////////////////////////
	// Private member variables.
	//////////////////////////////////////////////////////////////////////////
	// Host entity.
	CEntity *m_pEntity;
	bool		m_bHide;
		
	float m_fEffectRadius;
	string m_sTailname;
	Vec3 m_vLastVisibilityPos;
	tSoundID m_currentLipSyncId;

	Matrix34 m_Headtm;

	// Map of currently playing sounds.
	typedef std::vector<SAttachedSound>			AttachedSoundsVec;
	typedef std::vector<_smart_ptr<ISound> >	SoundSmartVec;
	AttachedSoundsVec m_SoundsAttached;
};

#endif //__SoundProxy_h__
