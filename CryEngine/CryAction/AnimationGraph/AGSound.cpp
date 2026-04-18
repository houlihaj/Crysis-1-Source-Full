#include "StdAfx.h"
#include "AGSound.h"
#include "AnimationGraphState.h"

#include <ISound.h>

CAGSound::CAGSound( CAGSoundFactory * pFactory ) 
: IAnimationStateNode(eASNF_Update)
, m_pFactory(pFactory)
, m_inputId(-7)
, m_delay(0)
{ 
}

IEntitySoundProxy* GetSoundProxy(IEntity* pEnt)
{
  return (IEntitySoundProxy*)pEnt->GetProxy(ENTITY_PROXY_SOUND);
}

void CAGSound::EnterState( SAnimationStateData& data, bool dueToRollback )
{  
  if (m_pFactory->DebugVal() != 0)
    CryLog("AGSound:EnterState <%s>", data.pEntity->GetName());

  if (!gEnv->pSoundSystem)
    return;

  if (0 == m_pFactory->m_sound.length())
    return;

  if (m_pFactory->m_input.length() > 0)
    m_inputId = data.pState->GetInputId(m_pFactory->m_input.c_str());  

  m_delay = m_pFactory->m_delay;
  
  if (m_delay == 0.f)
  {
    PlaySound(data);
  }  
}

bool CAGSound::CanLeaveState( SAnimationStateData& data )
{
  return (m_delay <= 0.f);
}

void CAGSound::LeaveState( SAnimationStateData& data )
{ 
  /*
  // stop sound
  if (m_pFactory->m_soundId != INVALID_SOUNDID)
  {
    IEntitySoundProxy* pSoundProxy = GetSoundProxy(data.pEntity);
    if (pSoundProxy)
    {
      pSoundProxy->StopSound(m_pFactory->m_soundId);
      m_pFactory->m_soundId = INVALID_SOUNDID;

      if (m_pFactory->DebugVal() != 0)
        CryLog("AGSound:LeaveState <%s>, sound stopped", data.pEntity->GetName());
    }
    else
      CryLog("AGSound:LeaveState <%s>, no sound proxy!", data.pEntity->GetName());
  }  
  */
}

void CAGSound::GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky )
{  
}

void CAGSound::Update( SAnimationStateData& data )
{	
  if (m_delay > 0.f)
  {    
    m_delay -= gEnv->pTimer->GetFrameTime();

    if (m_delay > 0)
    {
      if (m_pFactory->DebugVal() > 0)
      {
        static IRenderer* pRenderer = gEnv->pRenderer;
        static float drawColor[4] = {1,1,1,1};    
        int x = 10;
        int y = 300;
        pRenderer->Draw2dLabel(x, y, 1.2f, drawColor, false, "AGSound: delaying for %f", m_delay);          
      }
    }    
    else 
    {
      PlaySound(data);    
    }
  }
  
  /* this won't work as long as state is immediately left
  if (m_inputId >= 0 && m_pFactory->m_soundId != INVALID_SOUNDID)
  {
    float input = data.pState->GetInputAsFloat(m_inputId);
    
    m_pFactory->PerformOp(input, data);
  }
  */
}


void CAGSound::PlaySound( SAnimationStateData& data )
{
  IEntitySoundProxy* pSoundProxy = GetSoundProxy(data.pEntity);
  if (pSoundProxy)
  {
    static const Vec3 offset(ZERO);
    static const Vec3 dir(FORWARD_DIRECTION);
		int soundFlags = m_pFactory->m_soundType;
		if (m_pFactory->m_bVoiceSound)
			soundFlags |= FLAG_SOUND_VOICE;
    m_pFactory->m_soundId = pSoundProxy->PlaySound(m_pFactory->m_sound.c_str(), offset, dir, soundFlags, eSoundSemantic_Animation);
        
    if (m_pFactory->m_soundId == INVALID_SOUNDID)
      CryLog("[AGSound]: playing of sound %s failed <%s>", m_pFactory->m_sound.c_str(), data.pEntity->GetName());
    else if (m_pFactory->DebugVal()>0)
      CryLog("[AGSound]: playing sound %s in %s", m_pFactory->m_sound.c_str(), data.pEntity->GetName());
  } 
  else
    CryLog("[AGSound]: No sound proxy <%s>!", data.pEntity->GetName());
}


const IAnimationStateNodeFactory::Params * CAGSoundFactory::GetParameters()
{
  static const Params params[] = 
  {     
    {true,   "string", "sound", "Sound Name", "" }, 
    {true,   "float",  "delay", "Delay", "0.0" },
    {false,  "string", "input", "Input", "" },
    {false,  "string", "soundInput", "Sound Input", "" },    
    {false,  "float",  "multiplier", "Multiplier", "1.0" },    
    {false,  "float",  "offset", "Offset", "0.0" },    
		{false,  "int",  "soundType", "Sound Type", "4" },
		{false,  "bool", "voiceSound", "VoiceSound", "0" },
    {0}
  };
  return params;
}

bool CAGSoundFactory::Init( const XmlNodeRef& node, IAnimationGraphPtr )
{
  bool ok = true;

  m_sound = node->getAttr("sound");
  ok &= node->getAttr("delay", m_delay);
  
  m_input = node->getAttr("input");
  m_soundInput = node->getAttr("soundInput");  

  node->getAttr("multiplier", m_mult);

	m_offset = 0.0f;
  node->getAttr("offset", m_offset);

	m_soundType = FLAG_SOUND_3D;
  node->getAttr("soundType", m_soundType);

	m_bVoiceSound = false;
	node->getAttr("voiceSound", m_bVoiceSound);

  m_soundId = INVALID_SOUNDID;
      
  return ok;
}


void CAGSoundFactory::Release()
{
  delete this;
}

IAnimationStateNode * CAGSoundFactory::Create()
{
  return new CAGSound(this);
}

const char * CAGSoundFactory::GetCategory()
{
  return "Sound";
}

const char * CAGSoundFactory::GetName()
{
  return "Sound";
}

void CAGSoundFactory::PerformOp( float input, const SAnimationStateData& data )
{
  // update sound  
  IEntitySoundProxy* pSoundProxy = GetSoundProxy(data.pEntity);
  if (pSoundProxy)
  {
    ISound* pSound = pSoundProxy->GetSound(m_soundId);
    if (pSound)
    {
      float val = m_mult*input + m_offset;
      pSound->SetParam(m_soundInput.c_str(), val);
    }
  }
}

void CAGSoundFactory::SerializeAsFile(bool reading, AG_FILE *file)
{
	FileSerializationHelper h(reading, file);

	h.Value(&m_sound);
	h.Value(&m_delay);
	h.Value(&m_input);
	h.Value(&m_soundInput);
	h.Value(&m_mult);
	h.Value(&m_offset);
	h.Value(&m_soundId);
	h.Value(&m_soundType);
	h.Value(&m_bVoiceSound);
}

