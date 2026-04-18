#include "stdafx.h"
#include "CryModEffMorph.h"

// starts the morphing sequence
void CryModEffMorph::StartMorph (int nMorphTargetId, const CryCharMorphParams& rParams)
{
	m_Params = rParams;
	m_nMorphTargetId = nMorphTargetId;
	m_fTime = rParams.m_fStartTime;
	m_nFlags = rParams.m_nFlags;
}

void CryModEffMorph::stop()
{
	m_nMorphTargetId = -1;
}

// advances the current time of the played animation and returns the blending factor by which this animation affects the bone pose
void CryModEffMorph::Tick (f32 fDeltaTime)
{
	if (m_nMorphTargetId < 0)
		return;

	if (!(m_nFlags & m_Params.FLAGS_FREEZE))
		m_fTime += fDeltaTime * m_Params.m_fSpeed;
	
	if ((m_nFlags & m_Params.FLAGS_INSTANTANEOUS) ||
		!(m_nFlags & m_Params.FLAGS_NO_BLENDOUT) && m_fTime > m_Params.m_fBlendIn + m_Params.m_fBlendOut + m_Params.m_fLength)
		// we're finished
		m_nMorphTargetId = -1;
}



// Smoothes linear blending into cubic (b-spline) with 0-derivatives
// near 0 and 1
inline f32 SmoothBlendValue (f32 fBlend)
{
	if (fBlend <= 0) return 0;
	if (fBlend >= 1) return 1;
	fBlend -= 0.5f;
	return 0.5f - 2.0f * (fBlend*fBlend*fBlend) + 1.5f * fBlend;
}


// returns the blending for the morph target
f32 CryModEffMorph::getBlending()const
{
	f32 fTimeStable = m_fTime - m_Params.m_fBlendIn;
	
	if (fTimeStable < 0) // blending in...
		return SmoothBlendValue(m_fTime/m_Params.m_fBlendIn) * m_Params.m_fAmplitude;

	if (m_nFlags & m_Params.FLAGS_NO_BLENDOUT)
		return m_Params.m_fAmplitude; // never blending out - stable morph

	f32 fTimeBlendOut = fTimeStable - m_Params.m_fLength;
	
	if (fTimeBlendOut < 0)
		return m_Params.m_fAmplitude;

	return SmoothBlendValue(1-fTimeBlendOut/m_Params.m_fBlendOut) * m_Params.m_fAmplitude;
}

// returns false when this morph target is inactive
bool CryModEffMorph::isActive()const
{
	return m_nMorphTargetId >= 0;
}

// returns the morph target, or -1 if none
int CryModEffMorph::getMorphTargetId () const
{
	return m_nMorphTargetId;
}
