#ifndef _STARS_H_
#define _STARS_H_

#pragma once

class CStars
{
public:
	CStars();
	~CStars();

	void Render();

private:
	bool LoadData();

private:
	uint32 m_numStars;
	//CVertexBuffer* m_pStarVB;
	IRenderMesh* m_pStarMesh;
	CShader* m_pShader;
	
	CCryName m_shaderTech;
	CCryName m_vspnStarSize;
	CCryName m_pspnStarIntensity;
};

#endif  // #ifndef _STARS_H_
