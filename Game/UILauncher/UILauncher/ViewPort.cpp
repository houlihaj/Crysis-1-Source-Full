#include <StdAfx.h>
#include "ViewPort.h"

#include <IActorSystem.h>
#include <IRenderer.h>
#include <IRenderAuxGeom.h>
#include "ITestSystem.h"
#include "Launcher.h"
#include "SceneController.h"


BEGIN_MESSAGE_MAP(CViewPort, CStatic)
	ON_WM_SIZE()
END_MESSAGE_MAP()


CViewPort::CViewPort()
{
	m_initialized = false;
	m_fBarHeight = 0;
}


void CViewPort::Initialize(CSceneStartup* pScene, CSceneController* pController)
{
	m_pScene = pScene;
	m_pController = pController;

	m_pRenderer = gEnv->pRenderer;
	m_pRenderer->CreateContext(m_hWnd);
	SetupRenderContext();

	m_initialized = true;
}


void CViewPort::SetBarHeight(float height)
{
	m_fBarHeight = height;
}


float CViewPort::GetBarHeight()
{
	return m_fBarHeight;
}


float CViewPort::GetMaxBarHeight()
{
	return m_fMaxBarHeight;
}


void CViewPort::Refresh()
{
	if (m_initialized)
	{	
		ISystem* pSystem = gEnv->pSystem;
		CCamera* cam = m_pController->GetCamera();
		IActor * pClActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
		int renderFlags = SHDF_ALLOW_WATER | SHDF_ALLOWPOSTPROCESS | SHDF_ALLOWHDR | SHDF_SORT | SHDF_ZPASS | SHDF_ALLOW_AO;
		
		switch(m_pController->GetViewTarget())
		{
		case VT_CAMERA:
			if (!pClActor->IsThirdPerson())
			{
				pClActor->ToggleThirdPerson();
				m_pScene->Update(true, 0);
			}

			pSystem->Update();
			pSystem->SetViewCamera(*cam);

			pSystem->RenderBegin();
			m_pRenderer->SetViewport(0,0,m_pRenderer->GetWidth(),m_pRenderer->GetHeight());
			gEnv->p3DEngine->RenderWorld(renderFlags, *cam,"CViewPort:Refresh");
			pSystem->RenderEnd();
			break;

		case VT_SPECTATOR:
			if (!pClActor->IsThirdPerson())
				pClActor->ToggleThirdPerson();

			m_pScene->Update(true, 0);
			break;

		case VT_FIRST_PERSON:
			if (pClActor->IsThirdPerson())
				pClActor->ToggleThirdPerson();			

			m_pScene->Update(true, 0);
			break;
		}

		PaintHUD();
	}
	else // clear panel
	{		
		CRect rect;
		GetClientRect(&rect);

		CBrush brush(RGB(0, 0, 0));
		CDC* pDC = GetDC();
		pDC->FillRect(rect, &brush);
	}
}


void CViewPort::DrawScreenBars(float yOffset, int width, int height)
{    
	IRenderAuxGeom* pRenAux = gEnv->pRenderer->GetIRenderAuxGeom();
	ColorB col = RGBA8(0x0,0x00,0x00,0x00);

	Vec3 a(0,yOffset-m_fMaxBarHeight,0);
	Vec3 b(width,yOffset,0);
	Vec3 c(width,yOffset-m_fMaxBarHeight,0);
	Vec3 d(0,yOffset,0);
	pRenAux->DrawTriangle(a,col, b,col, c,col);
	pRenAux->DrawTriangle(a,col, d,col, b,col);

	Vec3 e(0,height-yOffset,0);
	Vec3 f(width,height+m_fMaxBarHeight-yOffset,0);
	Vec3 g(width,height-yOffset,0);
	Vec3 h(0,height+m_fMaxBarHeight-yOffset,0);
	pRenAux->DrawTriangle(e,col, f,col, g,col);
	pRenAux->DrawTriangle(e,col, h,col, f,col);
}


void CViewPort::PaintHUD()
{
	CRect rect;
	GetClientRect(&rect);

	m_pRenderer->Set2DMode(true,rect.right, rect.bottom);

	DrawScreenBars(m_fBarHeight,rect.right, rect.bottom);

	CCamera* cam = m_pController->GetCamera();
	IActor * pClActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
	IEntity* pEntity = pClActor->GetEntity();

	float cWhite[4] = {1,1,1,1};
	char str[256];

	Vec3 camPos = cam->GetPosition();
	sprintf_s(str, 256, "Camera: %d, %d, %d ", (int)camPos.x, (int)camPos.y, (int)camPos.z);
	m_pRenderer->Draw2dLabel( 10, 10, 1.5f, cWhite, false, str);

	Vec3 plrPos = pEntity->GetPos();
	sprintf_s(str, 256, "Spectator: %d, %d, %d ", (int)plrPos.x, (int)plrPos.y, (int)plrPos.z);
	m_pRenderer->Draw2dLabel( 10, 10+15, 1.5f, cWhite, false, str);

	m_pRenderer->Set2DMode(false,rect.right, rect.bottom);
}


bool CViewPort::SetupRenderContext()
{
	CRect r;
	GetClientRect(&r);

	m_pRenderer->SetCurrentContext(m_hWnd);
	m_pRenderer->ChangeViewport(0, 0, r.right, r.bottom);

	// compute height of cut scene blend bars
	float newHeight = (r.right * 9.0) / 16.0;
	m_fMaxBarHeight = (r.bottom - newHeight) / 2;

	return true;
}


void CViewPort::OnSize(UINT nType, int cx, int cy)
{
	if (!m_initialized)
		return;

	SetupRenderContext();
}
