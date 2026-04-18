#ifndef __CRYDXPSDEPTHSTENCILVIEW__
#define __CRYDXPSDEPTHSTENCILVIEW__

#include "../CCryDXPSResource.hpp"
#include "../../../CCryDXPSMisc.hpp"
#include "../../../Layer0/CCryDXPS.hpp"
#include "../Textures/CCryDXPSTexture2D.hpp"

class CCryDXPSDepthStencilView;
class CCryDXPSDepthStencilView	:	public CCryDXPSResource	,	public CCryRefAndWeak<CCryDXPSDepthStencilView>
{
private:
	APWeakTexture2D						m_pTex;
public:
	CCryDXPSDepthStencilView(CCryDXPSTexture2D* pTex):
		CCryDXPSResource(EDXPS_RT_DEPTHSTENCILVIEW),
		CCryRefAndWeak<CCryDXPSDepthStencilView>(this),	m_pTex(pTex)
	{
	}
	CCryDXPSTexture2D*	Texture(){return m_pTex;}
public:
	inline	void		ReleaseResources()
									{
									}

	void						GetDesc(D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc);
	void						GetResource(ID3D10Resource** pResource);
	unsigned long		AddRef(){return IncRef();}
};

typedef CCryDXPSDepthStencilView ID3D10DepthStencilView;

#endif

