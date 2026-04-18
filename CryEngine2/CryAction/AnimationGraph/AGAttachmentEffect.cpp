#include "StdAfx.h"
#include "AGAttachmentEffect.h"
#include "AnimationGraphCVars.h"
#include "AnimationGraphState.h"

CAGAttachmentEffect::CAGAttachmentEffect( CAGAttachmentEffectFactory * pFactory ) 
: IAnimationStateNode(eASNF_Update)
, m_pFactory(pFactory)
, m_countInputId(-7)
, m_sizeInputId(-7)
{  
}

void CAGAttachmentEffect::EnteredState( SAnimationStateData& data )
{
	if (m_pFactory->m_sizeInput.length())
		m_sizeInputId  = data.pState->GetInputId(m_pFactory->m_sizeInput.c_str());  
	if (m_pFactory->m_countInput.length())
		m_countInputId = data.pState->GetInputId(m_pFactory->m_countInput.c_str());

	ICharacterInstance *pCharacter = data.pEntity->GetCharacter(0);

	if (!pCharacter || 0 == m_pFactory->m_effect[0] || 0 == m_pFactory->GetAttachmentName()[0])
		return;

	// get attachment, return if it doesn't exist
	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment* pA = pIAttachmentManager->GetInterfaceByName(m_pFactory->GetAttachmentName());  

	bool doLog = CAnimationGraphCVars::Get().m_logEffects > 0;

	if (!pA)
	{
		if (doLog)
			CryLog("Attachment %s not found (%s), returning..", m_pFactory->GetAttachmentName().c_str(), data.pEntity->GetName());
		return;
	}

	CEffectAttachment *pEffectAttachment = new CEffectAttachment(m_pFactory->m_effect, m_pFactory->m_pos, Vec3(0,1,0), 1.f);

	pA->AddBinding(pEffectAttachment);  
	pA->HideAttachment(0);    
	m_pFactory->m_pEA = pEffectAttachment;

	if (doLog)
		CryLog("Attached effect %s to %s", m_pFactory->m_effect.c_str(), m_pFactory->GetAttachmentName().c_str());
}

void CAGAttachmentEffect::EnterState( SAnimationStateData& data, bool dueToRollback )
{
}

bool CAGAttachmentEffect::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAGAttachmentEffect::LeftState( SAnimationStateData& data, bool wasEntered )
{
	if (wasEntered)
	{
		ICharacterInstance *pCharacter = data.pEntity->GetCharacter(0);
		if (pCharacter)
		{
			IAttachment* pA = pCharacter->GetIAttachmentManager()->GetInterfaceByName(m_pFactory->GetAttachmentName());
			if (pA)
				pA->ClearBinding();
		}
		m_pFactory->m_pEA = 0;
	}

	delete this;
}

void CAGAttachmentEffect::LeaveState( SAnimationStateData& data )
{
}

void CAGAttachmentEffect::GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky )
{  
}

void CAGAttachmentEffect::Update( SAnimationStateData& data )
{	  
  if (m_pFactory->m_pEA)
  { 
    m_pFactory->PerformOp(m_sizeInputId, m_countInputId, data);
  }
}



const IAnimationStateNodeFactory::Params * CAGAttachmentEffectFactory::GetParameters()
{
	static const Params params[] = 
	{
		{true,  "string", "attachment", "Attachment Name",  "" },
    {true,  "vec3",   "pos",        "Position Offset",  "0,0,0" },    
		{true,  "string", "effect",     "Particle effect",  "" },
    {true,  "string", "sizeScaleInput",  "SizeScale Input",       "" },    
    {true,  "float",  "sizeScaleOffset", "SizeScale Offset",      "0" },
    {true,  "float",  "sizeScaleMult",   "SizeScale Multiplier",  "1.0" },
    {true,  "string", "countScaleInput", "CountScale Input",      "" },
    {true,  "float",  "countScaleOffset","CountScale Offset",     "0" },
    {true,  "float",  "countScaleMult",  "CountScale Multiplier", "1.0" },
		{0}
	};
	return params;
}

bool CAGAttachmentEffectFactory::Init( const XmlNodeRef& node, IAnimationGraphPtr )
{
	bool ok = true;
	
	m_attachment = node->getAttr("attachment");
  m_effect = node->getAttr("effect");
	ok &= node->getAttr("pos", m_pos);
  
  m_sizeInput = node->getAttr("sizeScaleInput");
  m_countInput = node->getAttr("countScaleInput");

  ok &= node->getAttr("sizeScaleOffset", m_sizeOffset);
  ok &= node->getAttr("sizeScaleMult", m_sizeMult);
  ok &= node->getAttr("countScaleOffset", m_countOffset);
  ok &= node->getAttr("countScaleMult", m_countMult);
  
  m_pEA = 0;

	return ok;
}


void CAGAttachmentEffectFactory::Release()
{
	delete this;
}

IAnimationStateNode * CAGAttachmentEffectFactory::Create()
{
	return new CAGAttachmentEffect(this);
}

const char * CAGAttachmentEffectFactory::GetCategory()
{
	return "AttachmentEffect";
}

const char * CAGAttachmentEffectFactory::GetName()
{
	return "AttachmentEffect";
}

void CAGAttachmentEffectFactory::PerformOp( int sizeInput, int countInput, const SAnimationStateData& data )
 {
  // update emitter  
  SpawnParams sp;

  float sizeScale = (sizeInput >= 0) ? data.pState->GetInputAsFloat(sizeInput) : 1.f;
  sp.fSizeScale = (sizeScale + m_sizeOffset) * m_sizeMult;
  
  float countScale = (countInput >= 0) ? data.pState->GetInputAsFloat(countInput) : 1.f;
  sp.fCountScale = (countScale + m_countOffset) * m_countMult;
  
  m_pEA->SetSpawnParams(sp);   

	bool doLog = CAnimationGraphCVars::Get().m_logEffects > 1;
  if (doLog)
  {
    static IRenderer* pRenderer = gEnv->pRenderer;
    static float drawColor[4] = {1,1,1,1};
    static int dx = 10; 
    static int dy = 15;
    
    int x = dx;
    int y = 100;

    pRenderer->Draw2dLabel(x, y+=dy, 1.4f, drawColor, false, "AGAttachmentEffect:");          
    pRenderer->Draw2dLabel(x, y+=dy, 1.2f, drawColor, false, "effect: %s", m_effect.c_str());                
    pRenderer->Draw2dLabel(x, y+=dy, 1.2f, drawColor, false, "fSizeScale: %f", sp.fSizeScale);      
    pRenderer->Draw2dLabel(x, y+=dy, 1.2f, drawColor, false, "fCountScale: %f", sp.fCountScale);
    pRenderer->Draw2dLabel(x, y+=dy, 1.2f, drawColor, false, "attachment: %s", m_attachment.c_str());
  }
}

void CAGAttachmentEffectFactory::SerializeAsFile(bool reading, AG_FILE *file)
{
	FileSerializationHelper h(reading, file);

	h.StringValue(&m_attachment);
	h.Value(&m_pos);
	h.StringValue(&m_effect);
	h.StringValue(&m_sizeInput);
	h.StringValue(&m_countInput);
	h.Value(&m_sizeOffset);
	h.Value(&m_sizeMult);
	h.Value(&m_countOffset);
	h.Value(&m_countMult);

	if(reading)
		m_pEA = NULL;
	/*CEffectAttachment* m_pEA;  ---*/
}