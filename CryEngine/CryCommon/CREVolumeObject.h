#ifndef _CREVOLUMEOBJECT_
#define _CREVOLUMEOBJECT_

#pragma once

#include "VertexFormats.h"


struct IVolumeObjectRenderNode;


class CREVolumeObject : public CRendElement
{
public:
	struct IVolumeTexture
	{
	public:
		virtual void Release() = 0;
		virtual bool Create(unsigned int width, unsigned int height, unsigned int depth, unsigned char* pData) = 0;
		virtual bool Update(unsigned int width, unsigned int height, unsigned int depth, const unsigned char* pData) = 0;
		virtual int GetTexID() const = 0;

	protected:
		virtual ~IVolumeTexture() {}
	};

public:
	CREVolumeObject();

	virtual ~CREVolumeObject();
	virtual void mfPrepare();
	virtual bool mfDraw(CShader* ef, SShaderPass* sfm);
	virtual float mfDistanceToCameraSquared(Matrix34& matInst);

	virtual IVolumeTexture* CreateVolumeTexture() const;

	Vec3 m_center;
	Matrix34 m_matInv;
	Vec3 m_eyePosInWS;
	Vec3 m_eyePosInOS;
	Plane m_volumeTraceStartPlane;
	AABB m_renderBoundsOS;
	bool m_viewerInsideVolume;
	bool m_nearPlaneIntersectsVolume;
	float m_alpha;
	float m_scale;

	IVolumeTexture* m_pDensVol;
	IVolumeTexture* m_pShadVol;
};


#endif // #ifndef _CREVOLUMEOBJECT_