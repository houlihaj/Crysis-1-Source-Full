#ifndef _CREFOGVOLUME_
#define _CREFOGVOLUME_

#pragma once

#include "VertexFormats.h"


struct IFogVolumeRenderNode;


class CREFogVolume : public CRendElement
{
public:
	CREFogVolume();

	virtual ~CREFogVolume();
	virtual void mfPrepare();
	virtual bool mfDraw( CShader* ef, SShaderPass* sfm );
	virtual float mfDistanceToCameraSquared( Matrix34& matInst );

	IFogVolumeRenderNode* m_pFogVolume;

	Vec3 m_center;
	bool m_viewerInsideVolume;
	AABB m_localAABB;
	Matrix34 m_matWSInv;
	float m_globalDensity;
	Vec2 m_softEdgesLerp;
	ColorF m_fogColor;								// color already combined with fHDRDynamic
	Vec3 m_heightFallOffDirScaled;
	Vec3 m_heightFallOffBasePoint;
	Vec3 m_eyePosInWS;
	Vec3 m_eyePosInOS;
};


#endif // #ifndef _CREFOGVOLUME_