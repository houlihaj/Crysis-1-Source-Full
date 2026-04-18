#include "StdAfx.h"
#include "CREVolumeObject.h"
#include "../../../CryCommon/IEntityRenderState.h"

#if defined(DIRECT3D10)
#include "../../XRenderD3D9/DriverD3D.h"
#endif

//////////////////////////////////////////////////////////////////////////
//

class CVolumeTexture : public CREVolumeObject::IVolumeTexture
{
public:
		virtual void Release();
		virtual bool Create(unsigned int width, unsigned int height, unsigned int depth, unsigned char* pData);
		virtual bool Update(unsigned int width, unsigned int height, unsigned int depth, const unsigned char* pData);
		virtual int GetTexID() const;

		CVolumeTexture();
		~CVolumeTexture();

private:
	unsigned int m_width;
	unsigned int m_height;
	unsigned int m_depth;
	CTexture* m_pTex;
};


CVolumeTexture::CVolumeTexture()
: m_width(0)
, m_height(0)
, m_depth(0)
, m_pTex(0)
{
}


CVolumeTexture::~CVolumeTexture()
{
	if (m_pTex)
		gRenDev->RemoveTexture(m_pTex->GetTextureID());
}


void CVolumeTexture::Release()
{
	delete this;
}


bool CVolumeTexture::Create(unsigned int width, unsigned int height, unsigned int depth, unsigned char* pData)
{
	assert(!m_pTex);
	if (!m_pTex)
	{
		char name[128]; name[sizeof(name) - 1] = '\0';
		_snprintf(name, sizeof(name) - 1,"$VolObj_%d", gRenDev->m_TexGenID++);

		int flags(FT_DONT_STREAM | FT_DONT_RESIZE | FT_DONT_ANISO);		
		m_pTex = CTexture::Create3DTexture(name, width, height, depth, 1, flags, pData, eTF_A8, eTF_A8);

		m_width = width;
		m_height = height;
		m_depth = depth;
	}
	return m_pTex != 0;
}


bool CVolumeTexture::Update(unsigned int width, unsigned int height, unsigned int depth, const unsigned char* pData)
{
#if defined(DIRECT3D9) || defined(OPENGL)
	if (!m_pTex || !m_pTex->GetDeviceTexture())
		return false;
#	if !defined(XENON)
	IDirect3DVolumeTexture9* pVolTex = (IDirect3DVolumeTexture9*) m_pTex->GetDeviceTexture();
	D3DLOCKED_BOX box;
	if (SUCCEEDED(pVolTex->LockBox(0, &box, 0, 0)))
	{
		unsigned int cpyWidth = min(width, m_width);
		unsigned int cpyHeight = min(height, m_height);
		unsigned int cpyDepth = min(depth, m_depth);

		for (int d=0; d<cpyDepth; ++d)	
		{
			for (int h=0; h<cpyHeight; ++h)
			{
				unsigned char* pDst = (unsigned char*) ((size_t) box.pBits + d * box.SlicePitch + h * box.RowPitch);
				const unsigned char* pSrc = (const unsigned char*) ((size_t) pData + d * height * width + h * width);
				memcpy(pDst, pSrc, cpyWidth);
			}
		}
		pVolTex->UnlockBox(0);
		return true;
	}
	else
		return false;
#	else
	assert(!"CVolumeTexture::Update() not implemented for XENON yet!");
	return false;
#	endif
#elif defined(DIRECT3D10)
	if (!m_pTex || !m_pTex->GetDeviceTexture())
		return false;

	unsigned int cpyWidth = min(width, m_width);
	unsigned int cpyHeight = min(height, m_height);
	unsigned int cpyDepth = min(depth, m_depth);

	D3D10_BOX box;
	box.left = 0;
	box.right = cpyWidth;
	box.top = 0;
	box.bottom = cpyHeight;
	box.front = 0;
	box.back = cpyDepth;

	ID3D10Texture3D* pVolTex = (ID3D10Texture3D*) m_pTex->GetDeviceTexture();
	gcpRendD3D->m_pd3dDevice->UpdateSubresource(pVolTex, 0, &box, pData, width, height * width);

	return true;
#else
	return true;
#endif
}


int CVolumeTexture::GetTexID() const
{
	return m_pTex ? m_pTex->GetTextureID() : 0;
}


//////////////////////////////////////////////////////////////////////////
//

CREVolumeObject::CREVolumeObject()
: CRendElement()
, m_center(0, 0, 0)
, m_matInv()
, m_eyePosInWS(0, 0, 0)
, m_eyePosInOS(0, 0, 0)
, m_volumeTraceStartPlane(Vec3(0, 0, 1), 0)
, m_renderBoundsOS(Vec3(-1, -1, -1), Vec3(1, 1, 1))
, m_viewerInsideVolume(false)
, m_nearPlaneIntersectsVolume(false)
, m_alpha(1)
, m_scale(1)
, m_pDensVol(0)
, m_pShadVol(0)
{
	mfSetType(eDATA_VolumeObject);
	mfUpdateFlags(FCEF_TRANSFORM);
	m_matInv.SetIdentity();
}


CREVolumeObject::~CREVolumeObject()
{
}


void CREVolumeObject::mfPrepare()
{
	gRenDev->EF_CheckOverflow(0, 0, this);
	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;
}


float CREVolumeObject::mfDistanceToCameraSquared(Matrix34& matInst)
{
	return (gRenDev->GetRCamera().Orig - m_center).GetLengthSquared();
}


CREVolumeObject::IVolumeTexture* CREVolumeObject::CreateVolumeTexture() const
{
	return new CVolumeTexture();
}