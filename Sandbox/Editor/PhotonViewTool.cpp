#include "StdAfx.h"
#include "PhotonTool.h"
#include "Viewport.h"
#include "Objects\DisplayContext.h"

#include "PhotonViewTool.h"
#include "IRenderAuxGeom.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CPhotonViewTool,CEditTool)

extern CPhotonMap* g_pGlobalMap;

//////////////////////////////////////////////////////////////////////////
void CPhotonViewTool::Display( DisplayContext& dc )
{

	m_pDC = &dc;
	

  IRenderAuxGeom* pRendAuxGeom = GetIEditor()->GetRenderer()->GetIRenderAuxGeom();

	if (g_pGlobalMap == NULL)
		return;

	pRendAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags);

	for(int32 i =0; i<g_pGlobalMap->m_numPhotons; i++)
	{
    ColorB col;

		Vec3& vCol = g_pGlobalMap->m_pPhotons[i].C;

		f32 maxChannel	=	max(max(vCol.x,vCol.y),vCol.z);

    col.r = 255 * (g_pGlobalMap->m_pPhotons[i].C.x * 1.0f / maxChannel); //fix * number
		col.g = 255 * (g_pGlobalMap->m_pPhotons[i].C.y * 1.0f / maxChannel);
		col.b = 255 * (g_pGlobalMap->m_pPhotons[i].C.z * 1.0f / maxChannel);

		pRendAuxGeom->DrawPoint(g_pGlobalMap->m_pPhotons[i].P, col);
	}

}

void CPhotonViewTool::ShowPhotonMap()
{
	GetIEditor()->SetEditTool(new CPhotonViewTool);
}

void CPhotonViewTool::RegisterTool( CRegistrationContext &rc )
{
	rc.pCommandManager->RegisterCommand("EditTool.PhotonTool.ShowPhotonMap",functor(CPhotonViewTool::ShowPhotonMap));
}
