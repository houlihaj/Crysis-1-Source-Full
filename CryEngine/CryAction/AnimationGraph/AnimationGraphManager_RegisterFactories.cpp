#include "StdAfx.h"
#include "AnimationGraphManager.h"

#include "AnimEvent.h"
#include "AnimPhysicsNodes.h"

#include "AGAnimation.h"
#include "AGParams.h"
#include "AGTransitionParams.h"
#include "AGAttachmentEffect.h"
#include "AGOutput.h"
#include "AGSetInput.h"
#include "AGSound.h"
#include "AGForceStartImmediately.h"
#include "AGFacial.h"
#include "AGColliderMode.h"
#include "AGMovementControlMethod.h"
#include "AGLookIkOverride.h"
#include "AGWaitForUBAllowed.h"

#include "MGMood.h"
#include "MGTheme.h"
#include "MGDelayEntering.h"
#include "MGDelayLeaving.h"
#include "MGStinger.h"
#include "MGEndMusic.h"

#include "AnimatedCharacter.h"

#include "IGameFramework.h"

void CAnimationGraphManager::RegisterFactories( IGameFramework * pFW )
{
	AddCategory("ParamsLayer1", true);
	AddCategory("ParamsLayer2", true);
	AddCategory("ParamsLayer3", true);
	AddCategory("ParamsLayer4", true);
	AddCategory("ParamsLayer5", true);
	AddCategory("ParamsLayer6", true);
	AddCategory("ParamsLayer7", true);
	AddCategory("ParamsLayer8", true);
	AddCategory("ParamsLayer9", true);

	AddCategory("General", false);
	AddCategory("Physics", false);
	AddCategory("FreeFall", true, false);
	AddCategory("TentaclePhysics", true);
	AddCategory("Output", false, true);
	AddCategory("SetInput", false, true);
	AddCategory("ColliderMode", true, false);
	AddCategory("MovementControlMethod", true, false);
	AddCategory("LookIk", true, false);
	AddCategory("WaitForUBAllowed", true, false);
 
	AddCategory("AttachmentEffect", false, true);
	AddCategory("Event", false, true);
	AddCategory("Sound", false, true); 
	AddCategory("SpecialOverrides", false);
	AddCategory("FacialEffector", false, true);

	AddCategory("MusicTheme", true, false);
	AddCategory("MusicMood", true, false);
	AddCategory("MusicDelayEntering", true, false);
	AddCategory("MusicDelayLeaving", true, false);
	AddCategory("MusicStinger", true, false);
	AddCategory("EndMusic", true, false);

	for (int i=0; i<10; i++)
	{
		string name;
		name.Format("AnimationLayer%d", i);
		AddCategory( name, true );
	}

	//	REGISTER_FACTORY(pFW, "PhysicalDimensions", CAnimPhysicalDimensionsFactory, false);
	REGISTER_FACTORY(pFW, "Event", CAnimEventFactory, false);
	REGISTER_FACTORY(pFW, "TentacleParams", CAnimTentacleParams, false);

	REGISTER_FACTORY(pFW, "ColliderMode", CAGColliderMode, false);
	REGISTER_FACTORY(pFW, "MovementControlMethod", CAGMovementControlMethod, false);
	REGISTER_FACTORY(pFW, "LookIk", CAGLookIkOverride, false);
	REGISTER_FACTORY(pFW, "WaitForUBAllowed", CAGWaitForUBAllowed, false);

	REGISTER_FACTORY(pFW, "ParamsLayer1", CAGParamsLayer<0>, false);
	REGISTER_FACTORY(pFW, "ParamsLayer2", CAGParamsLayer<1>, false);
	REGISTER_FACTORY(pFW, "ParamsLayer3", CAGParamsLayer<2>, false);
	REGISTER_FACTORY(pFW, "ParamsLayer4", CAGParamsLayer<3>, false);
	REGISTER_FACTORY(pFW, "ParamsLayer5", CAGParamsLayer<4>, false);
	REGISTER_FACTORY(pFW, "ParamsLayer6", CAGParamsLayer<5>, false);
	REGISTER_FACTORY(pFW, "ParamsLayer7", CAGParamsLayer<6>, false);
	REGISTER_FACTORY(pFW, "ParamsLayer8", CAGParamsLayer<7>, false);
	REGISTER_FACTORY(pFW, "ParamsLayer9", CAGParamsLayer<8>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer1", CAGTransitionParamsLayer<0>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer2", CAGTransitionParamsLayer<1>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer3", CAGTransitionParamsLayer<2>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer4", CAGTransitionParamsLayer<3>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer5", CAGTransitionParamsLayer<4>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer6", CAGTransitionParamsLayer<5>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer7", CAGTransitionParamsLayer<6>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer8", CAGTransitionParamsLayer<7>, false);
	REGISTER_FACTORY(pFW, "TransitionParamsLayer9", CAGTransitionParamsLayer<8>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer1", CAGAnimationLayer<0>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer2", CAGAnimationLayer<1>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer3", CAGAnimationLayer<2>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer4", CAGAnimationLayer<3>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer5", CAGAnimationLayer<4>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer6", CAGAnimationLayer<5>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer7", CAGAnimationLayer<6>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer8", CAGAnimationLayer<7>, false);
	REGISTER_FACTORY(pFW, "AnimationLayer9", CAGAnimationLayer<8>, false);
	REGISTER_FACTORY(pFW, "AttachmentEffect", CAGAttachmentEffectFactory, false);
	REGISTER_FACTORY(pFW, "Output", CAGOutput, false);
	REGISTER_FACTORY(pFW, "SetInput", CAGSetInputFactory, false);
	REGISTER_FACTORY(pFW, "Sound", CAGSoundFactory, false);
	REGISTER_FACTORY(pFW, "FallAndPlay", CAnimFallAndPlay, false);
	REGISTER_FACTORY(pFW, "FacialEffector", CAGFacial, false);
	REGISTER_FACTORY(pFW, "FreeFall", CAGFreeFall, false);

	REGISTER_FACTORY(pFW, "MusicTheme", CMGTheme, false);
	REGISTER_FACTORY(pFW, "MusicMood", CMGMood, false);
	REGISTER_FACTORY(pFW, "MusicDelayEntering", CMGDelayEnteringFactory, false);
	REGISTER_FACTORY(pFW, "MusicDelayLeaving", CMGDelayLeavingFactory, false);
	REGISTER_FACTORY(pFW, "MusicStinger", CMGStinger, false);
	REGISTER_FACTORY(pFW, "EndMusic", CMGEndMusic, false);

	REGISTER_FACTORY(pFW, "AnimatedCharacter", CAnimatedCharacter, false);

	// these must stay at the end of this function... to be very late acting in states
	REGISTER_FACTORY(pFW, "ForceStartImmediately", CAGForceStartImmediately, false);
}
