#ifndef __ANIMPHYSICSNODES_H__
#define __ANIMPHYSICSNODES_H__

#pragma once

#include "IAnimationStateNode.h"

class CAnimTentacleParams : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAnimTentacleParams() : IAnimationStateNode(eASNF_Update) {}

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback );
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Instant; }
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState( SAnimationStateData& data ) {}
	virtual void LeftState( SAnimationStateData& data, bool ) {}
	virtual void Update( SAnimationStateData& data );
	void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky ) 
	{
		hard = sticky = 0.0f;
	}
	virtual const Params * GetParameters();
	virtual IAnimationStateNodeFactory * GetFactory() { return this; }
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement );
	// ~IAnimationStateNode

	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr );
	virtual void Release();
	virtual IAnimationStateNode * Create();
	virtual const char * GetCategory() {return "TentaclePhysics";}
	virtual const char * GetName() {return "TentacleParams";}
	virtual void SerializeAsFile(bool reading, AG_FILE *file);

	virtual bool IsLessThan( IAnimationStateNodeFactory * pFactory )
	{
		AG_LT_BEGIN_FACTORY(CAnimTentacleParams);
			AG_LT_ELEM(m_bones);
			AG_LT_ELEM(m_animBlend);
			AG_LT_ELEM(m_jointLimit);
			AG_LT_ELEM(m_animBlendID);
			AG_LT_ELEM(m_jointLimitID);
		AG_LT_END();
	}
	// ~IAnimationStateNodeFactory

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->Add(m_bones);
	}

private:
	
	void SetTentacles(ICharacterInstance *pCharacter,pe_params_rope *ppRope,const char *bones = NULL);

private:

	string m_bones;
	float m_animBlend;
	float m_jointLimit;

	uint8 m_animBlendID;
	uint8 m_jointLimitID;
};

class CAnimFallAndPlay : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAnimFallAndPlay() : IAnimationStateNode(0) {}

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback ) {}
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Entered; }
	virtual bool CanLeaveState( SAnimationStateData& data )
	{
		if (data.pEntity)
		{
			// optimization: allow leaving the state if actor is dead
			IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor( data.pEntity->GetId() );
			if ( !pActor || pActor->GetHealth() > 0 )
			{
				if ( pActor->IsFallen() )
					return false;
				if (IPhysicalEntity * pPE = data.pEntity->GetPhysics())
				{
					if (pPE->GetType() != PE_LIVING)
						return false;
					if (ICharacterInstance * pCE = data.pEntity->GetCharacter(0))
					{
						ISkeletonAnim* pISkeletonAnim=pCE->GetISkeletonAnim();
						if (pISkeletonAnim->GetNumAnimsInFIFO(0)>1)
							return false;
						if (pISkeletonAnim->GetNumAnimsInFIFO(0)==1)
						{
							const CAnimation& rAnimation = pISkeletonAnim->GetAnimFromFIFO(0,0);
							if (rAnimation.m_fAnimTime<1.0f)
								return false;
						}
					}
				}
			}

			// start blinking again
			if (ICharacterInstance * pCI = data.pEntity->GetCharacter(0))
			{
				pCI->EnableProceduralFacialAnimation( pActor != NULL && pActor->GetHealth() > 0 );
			}
		}
		return true;
	}
	virtual void LeaveState( SAnimationStateData& data );
	virtual void EnteredState( SAnimationStateData& data ) {}
	virtual void LeftState( SAnimationStateData& data, bool ) {}
	void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky ) 
	{
		hard = sticky = start + 0.3f;
	}
	virtual const Params * GetParameters() { static const Params params[] = {{0}}; return params; }
	virtual IAnimationStateNodeFactory * GetFactory() { return this; }
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement ) {}
	// ~IAnimationStateNode

	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr ) { return true; }
	virtual void Release() { delete this; }
	virtual IAnimationStateNode * Create() { return this; }
	virtual const char * GetCategory() {return "Physics";}
	virtual const char * GetName() {return "FallAndPlay";}	
	virtual void SerializeAsFile(bool reading, AG_FILE *file) { SerializeAsFile_NodeBase(reading, file); }

	virtual bool IsLessThan(IAnimationStateNodeFactory * pFactory ) { return false; }
	// ~IAnimationStateNodeFactory

	virtual void GetStateMemoryStatistics(ICrySizer * s)
	{
	}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

};

class CAGFreeFall : public IAnimationStateNodeFactory, public IAnimationStateNode
{
public:
	CAGFreeFall() : IAnimationStateNode(0) {}

	// IAnimationStateNode
	virtual void EnterState( SAnimationStateData& data, bool dueToRollback ) {}
	virtual EHasEnteredState HasEnteredState( SAnimationStateData& data ) { return eHES_Instant; }
	virtual bool CanLeaveState( SAnimationStateData& data );
	virtual void LeaveState( SAnimationStateData& data ) {}
	virtual void EnteredState( SAnimationStateData& data );
	virtual void LeftState( SAnimationStateData& data, bool ) {}
	void GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky ) 
	{
		hard = sticky = 0.0f;
	}
	virtual const Params * GetParameters() { static const Params params[] = {{0}}; return params; }
	virtual IAnimationStateNodeFactory * GetFactory() { return this; }
	virtual void DebugDraw( SAnimationStateData& data, IRenderer * pRenderer, int x, int& y, int yIncrement ) {}
	// ~IAnimationStateNode

	// IAnimationStateNodeFactory
	virtual bool Init( const XmlNodeRef& node, IAnimationGraphPtr ) { return true; }
	virtual void Release() { delete this; }
	virtual IAnimationStateNode * Create() { return this; }
	virtual const char * GetCategory() { return "FreeFall"; }
	virtual const char * GetName() { return "FreeFall"; }	
	virtual void SerializeAsFile(bool reading, AG_FILE *file) { SerializeAsFile_NodeBase(reading, file); }
	virtual bool IsLessThan(IAnimationStateNodeFactory * pFactory ) { return false; }
	// ~IAnimationStateNodeFactory

	virtual void GetStateMemoryStatistics(ICrySizer * s) {}
	virtual void GetFactoryMemoryStatistics(ICrySizer * s) { s->Add(*this); }
};

#endif
