#ifndef __CRYDXPSTEXTUREBASE__
#define __CRYDXPSTEXTUREBASE__

#include "../CCryDXPSResource.hpp"
#include "../../../Layer0/CCryDXPS.hpp"

class CCryDXPSTexture;
class CCryDXPSTexture	:	public CCryDXPSResource	,	public	CCryRefAndWeak<CCryDXPSTexture>
{
protected:
	CellGcmTexture								m_GCMTexture;
	DXGI_FORMAT										m_Format;
	uint16												m_MemItemID;

public:
																CCryDXPSTexture(CCRYDXPSResType T):
																CCryDXPSResource(T),
																CCryRefAndWeak<CCryDXPSTexture>(this)
																{
																}
																inline	~CCryDXPSTexture()
																{
																	ReleaseResources();
																}

	inline	void									ReleaseResources()
																{
																	tdLayer0::Instance().Memory().Free(m_MemItemID);
														#if defined(_DEBUG)
																	m_MemItemID	=	0;
														#endif
																}

	inline const CellGcmTexture*	GcmTexture()const{return &m_GCMTexture;}
	inline unsigned	int						SizeX()const{return m_GCMTexture.width;}
	inline unsigned	int						SizeY()const{return m_GCMTexture.height;}
	inline unsigned	int						SizeZ()const{return m_GCMTexture.depth;}
	inline DXGI_FORMAT						Format()const{return m_Format;}
	inline void*									RawPointer(){return tdLayer0::Instance().Memory().ResolveHandle(m_MemItemID);}
};


typedef CCryAPtrRefCnt<CCryDXPSTexture>		APRefTexture;
typedef CCryAPtrWeakCnt<CCryDXPSTexture>	APWeakTexture;


#endif

