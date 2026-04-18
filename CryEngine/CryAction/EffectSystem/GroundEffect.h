#ifndef __GROUNDEFFECT_H__
#define __GROUNDEFFECT_H__

#include "../IEffectSystem.h"

struct IParticleEffect;

class CGroundEffect : public IGroundEffect
{
public:
	CGroundEffect(IEntity* pParent);
	virtual ~CGroundEffect();

	// IGroundEffect
	virtual void	SetHeight(float height);
  virtual void  SetHeightScale(float sizeScale, float countScale); 
  virtual void  SetBaseScale(float sizeScale, float countScale, float speedScale=1.f);
  virtual void  SetInterpolation(float speed);
	virtual void	SetFlags(int flags)			{	m_flags = flags; m_isActive = false; }
	virtual int	  GetFlags()							{	return m_flags; }
	virtual bool	SetParticleEffect(const char* name);
  virtual void  SetInteraction(const char* name);  
	virtual void	Update();
	virtual void  Stop(bool stop);
	// ~IGroundEffect

protected:
  void  SetSpawnParams(const SpawnParams& params);
  void  Deactivate();

	IEntity*	m_pEntity;
	int				m_flags;	
	float			m_height;	// the height at which the ground effect starts playing
	bool			m_isActive;
	float			m_ratio;
	bool			m_isStopped;
  int       m_matId;

	int								m_slot;	// slot that the effect lands in
	IParticleEffect*	m_pParticleEffect;  
  string            m_interaction; // for lookup in MaterialEffects system  
  
  float m_sizeScale;
  float m_sizeGoal;
  float m_countScale;
  float m_speedScale;
  float m_speedGoal;
  float m_interpSpeed;
  float m_maxHeightCountScale;
  float m_maxHeightSizeScale;  
  
  };
#endif //__GROUNDEFFECT_H__
