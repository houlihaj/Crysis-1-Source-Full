#include "StdAfx.h"
#include "PhotonTool.h"
#include "Viewport.h"
#include "Objects\DisplayContext.h"

#include "PhotonTool.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CPhotonTool,CEditTool)

extern CGeomCore* g_pGC;

//////////////////////////////////////////////////////////////////////////
void CPhotonTool::Display( DisplayContext& dc )
{

	m_pDC = &dc;

	if (g_pGC == NULL)
		return;

	CCryOctree* pOctree = g_pGC->GetOctree();
	if (pOctree == NULL)
	{
		return;
	}

	DrawOctree(pOctree->m_root);
    
}

void CPhotonTool::DrawOctree(COctreeCell* pCell)
{

	//fix Octree
	m_pDC->DrawWireBox( Vec3(pCell->m_boundingBox.min[0],pCell->m_boundingBox.min[1],pCell->m_boundingBox.min[2]),
											Vec3(pCell->m_boundingBox.max[0],pCell->m_boundingBox.max[1],pCell->m_boundingBox.max[2]) );

	for (int c = 0; c < 8; c++)
	{
		if (pCell->m_children[c] != NULL)
		{
			DrawOctree(pCell->m_children[c]);
		} 
	}

}

void CPhotonTool::ShowOctree()
{
	GetIEditor()->SetEditTool(new CPhotonTool);
}

void CPhotonTool::RegisterTool( CRegistrationContext &rc )
{
	rc.pCommandManager->RegisterCommand("EditTool.PhotonTool.ShowOctree",functor(CPhotonTool::ShowOctree));
}
