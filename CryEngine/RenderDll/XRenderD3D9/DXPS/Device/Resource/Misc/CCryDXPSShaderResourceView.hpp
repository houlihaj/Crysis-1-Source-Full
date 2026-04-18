#ifndef __CRYDXPSSHADERRESOURCEVIEW__
#define __CRYDXPSSHADERRESOURCEVIEW__

#include "../CCryDXPSResource.hpp"
#include "../../../CCryDXPSMisc.hpp"
#include "../../../Layer0/CCryDXPS.hpp"
#include "../Textures/CCryDXPSTextureBase.hpp"

class CCryDXPSShaderResourceView;
class CCryDXPSShaderResourceView	:	public CCryDXPSResource	,	public CCryRefAndWeak<CCryDXPSShaderResourceView>
{
private:
	APWeakTexture										m_pTex;
	D3D10_SHADER_RESOURCE_VIEW_DESC	m_ShaderResourceViewDesc;
public:
	CCryDXPSShaderResourceView(CCryDXPSTexture* pTex,const D3D10_SHADER_RESOURCE_VIEW_DESC* pSRV):
		CCryDXPSResource(EDXPS_RT_SHADERRESOURCEVIEW),
		CCryRefAndWeak<CCryDXPSShaderResourceView>(this),	m_pTex(pTex),
		m_ShaderResourceViewDesc(*pSRV)
	{
	}
	CCryDXPSTexture*	TextureRaw(){return m_pTex;}
public:
	inline	void		ReleaseResources()
									{
									}

	void						GetDesc(struct D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc);
	void						GetResource(ID3D10Resource** pResource);
//	unsigned long		AddRef(){return IncRef();}
	APWeakTexture&	Texture(){return m_pTex;}
};

typedef CCryDXPSShaderResourceView ID3D10ShaderResourceView;

#endif

