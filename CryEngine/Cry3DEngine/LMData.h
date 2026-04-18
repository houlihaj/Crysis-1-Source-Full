#ifndef _3DENGINE_LIGHTMAPDATA_H_
#define _3DENGINE_LIGHTMAPDATA_H_

TYPEDEF_AUTOPTR(RenderLMData);

struct SLMData
{
	SLMData() { m_pLMTCBuffer=0; }
	RenderLMData_AutoPtr m_pLMData;
	struct IRenderMesh * m_pLMTCBuffer;
};

#endif // _3DENGINE_LIGHTMAPDATA_H_
