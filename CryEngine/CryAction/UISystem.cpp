/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 16:8:2004   17:50 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include <IEntityRenderState.h>
#include <Cry_Camera.h>
#include "UISystem.h"
#include "IGameFramework.h"
#include "ScriptBind_UI.h"
#include <IFlashPlayer.h>
#include <CryAction.h>
#include <IGameObjectSystem.h>
#include <StringUtils.h>
#include "IActorSystem.h"

//------------------------------------------------------------------------
void LiangBarksyGetMinMax(float t, float p, float &tmin, float &tmax)
{
	if (t < tmin || t > tmax)
	{
		return;
	}

	if (p > 0.0f)
	{
		tmax = t;
	}
	else if (p < 0.0f)
	{
		tmin = t;
	}
}

//------------------------------------------------------------------------
void LiangBarskyClipLine(float &x, float &y, float &x2, float &y2, float xmin, float ymin, float xmax, float ymax)
{
	float dx = x2 - x;
	float dy = y2 - y;
	float rdx = 1.0f / dx;
	float rdy = 1.0f / dy;

	float tl = (xmin - x) * rdx;
	float tr = (xmax - x) * rdx;

	float tt = (ymin - y) * rdy;
	float tb = (ymax - y) * rdy;

	float tmin = 0.0f;
	float tmax = 1.0f;

	LiangBarksyGetMinMax(tl, -dx, tmin, tmax);
	LiangBarksyGetMinMax(tr, dx, tmin, tmax);
	x = x+tmin*dx;
	x2 = x+tmax*dx;

	tmin = 0.0f;
	tmax = 1.0f;
	LiangBarksyGetMinMax(tt, -dy, tmin, tmax);
	LiangBarksyGetMinMax(tb, dy, tmin, tmax);
	y = y+tmin*dy;
	y2 = y+tmax*dy;
}

//------------------------------------------------------------------------
CUISystem::CUISystem(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pGameFramework(pGameFramework),
	m_pSystem(pSystem),
	m_pRenderer(0),
	m_pFont(0),
	m_pScriptUI(0),
	m_lastPrimitiveType(eUIPT_None),
	m_lastTexId(-1),
	m_scissorLeft(0),
	m_scissorTop(0),
	m_scissorWidth(0),
	m_scissorHeight(0),
	m_inputDirty(false),
	m_renderTarget(0),
	m_loading(false)
{
	m_pRenderer = pSystem->GetIRenderer();
	m_pSystem->GetIInput()->AddEventListener(this);

	RegisterCVars();
	
	Reset();
}

//------------------------------------------------------------------------
CUISystem::~CUISystem()
{
	UnregisterCVars();

	m_pSystem->GetIInput()->RemoveEventListener(this);

	if (m_pScriptUI)
	{
		m_pScriptUI->OnRelease();
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnRelease);
		delete m_pScriptUI;
	}
}

//------------------------------------------------------------------------
void CUISystem::Reset()
{
	m_dX = 0;
	m_dY = 0;
	m_dZ = 0;
	m_padX = 0;
	m_padY = 0;
	m_inputDirty = false;

	SetFont("default", "default");

	// re-create script bind
	if (m_pScriptUI)
	{
		m_pScriptUI->OnRelease();
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnRelease);
		delete m_pScriptUI;
	}
	m_pScriptUI = new CScriptBind_UI(m_pSystem, this, m_pGameFramework);

	if (!m_pSystem->GetIScriptSystem()->ExecuteFile("scripts/ui/ui.lua", true, true))
	{
		m_pSystem->GetILog()->Log("$5[$4UI Error$5]$1: failed to load the user interface script!");

		return;
	}
  
	m_pScriptUI->OnInit();
	m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnInit);
}

//------------------------------------------------------------------------
void CUISystem::PauseGame(bool pause, bool force)
{
	m_pGameFramework->PauseGame(pause, force);
}

//------------------------------------------------------------------------
bool CUISystem::IsGamePaused()
{
	return m_pGameFramework->IsGamePaused();
}

//------------------------------------------------------------------------
void CUISystem::PreUpdate(float deltaTime, int frameId)
{
	assert(m_pScriptUI);

	m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnPreUpdate, deltaTime, frameId);

	if (m_renderTarget)
	{
		m_pRenderer->SetRenderTarget(0);
	}
}

//------------------------------------------------------------------------
void CUISystem::PostUpdate(float deltaTime, int frameId)
{
	assert(m_pScriptUI);

	m_pRenderer->EF_StartEf();
	m_pRenderer->SetCamera(GetISystem()->GetViewCamera());
	m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnPostUpdate, deltaTime, frameId);
	m_pRenderer->EF_EndEf3D(0);

	if (m_renderTarget)
	{
		m_pRenderer->SetRenderTarget(0);
	}
}

//------------------------------------------------------------------------
void CUISystem::EnableInput(bool enable)
{
	if (enable)
	{
		m_pSystem->GetIInput()->AddEventListener(this);
	}
	else
	{
		m_pSystem->GetIInput()->RemoveEventListener(this);
	}
}

//------------------------------------------------------------------------
bool CUISystem::OnInputEvent(const SInputEvent &event)
{
	FUNCTION_PROFILER(GetISystem(),PROFILE_GAME);

	switch (event.state)
	{
	case eIS_Changed:
		{
			if (event.keyId == eKI_MouseX)
			{
				m_dX = event.value;
				m_inputDirty = true;
			}
			else if (event.keyId == eKI_MouseY)
			{
				m_dY = event.value;
				m_inputDirty = true;
			}
			else if (event.keyId == eKI_MouseZ)
			{
				m_dZ = event.value;
				m_inputDirty = true;
			}
			else if (event.keyId == eKI_XI_ThumbRX || event.keyId == eKI_PS3_StickRX)
			{
				m_padX = (int)(event.value * 400.0f);
				m_inputDirty = true;
			}
			else if (event.keyId == eKI_XI_ThumbRY || event.keyId == eKI_PS3_StickRY)
			{
				m_padY = (int)(-event.value * 400.0f);
				m_inputDirty = true;
			}
		}
		break;
	case eIS_Pressed:
		{
			if (event.deviceId == eDI_Keyboard)
			{
				const char* keyname = m_pSystem->GetIInput()->GetKeyName(event);
				m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnKeyDown, event.keyId, keyname, 0);
			}
			else if(event.keyId == eKI_Mouse1 || event.keyId == eKI_XI_A || event.keyId == eKI_PS3_Cross)
			{
				m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnMouseDown, 1);
			}
		}
		break;
	case eIS_Released:
		{
			if (event.deviceId == eDI_Keyboard)
			{
				const char* keyname = m_pSystem->GetIInput()->GetKeyName(event);
				m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnKeyUp, event.keyId, keyname, 0);
			}
			else if(event.keyId == eKI_Mouse1 || event.keyId == eKI_XI_A || event.keyId == eKI_PS3_Cross)
			{
				m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnMouseUp, 1);
			}
		}
		break;
	}

	//////////////////////////////////////////////////////////////////////////
	// Temp code to feed flash with user input...
	// to be removed once new UI is ready!
	
	static float s_mouseX( 0.0f );
	static float s_mouseY( 0.0f );

	float width( (float) m_pRenderer->GetWidth() );
	float height( (float) m_pRenderer->GetHeight() );

	switch (event.state)
	{
	case eIS_Changed:
		{
			if (event.keyId == eKI_SYS_Commit && m_inputDirty)
			{
				s_mouseX = clamp_tpl( s_mouseX + m_dX, 0.0f, width );
				s_mouseY = clamp_tpl( s_mouseY + m_dY, 0.0f, height ) ;
				break;
			}
		}
	default:
		{
		}
	}

	if (event.keyId == eKI_XI_Connect || event.keyId == eKI_XI_Disconnect)
	{
		const char* keyname = m_pSystem->GetIInput()->GetKeyName(event);
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnKeyDown, event.keyId, keyname, 0);
	}

	if (event.keyId == eKI_SYS_Commit)
	{
		float change = m_padX*m_padX + m_padY*m_padY;
		if (change>0.0f)
		{
			m_dX = (m_padX * m_pSystem->GetITimer()->GetFrameTime());
			m_dY = (m_padY * m_pSystem->GetITimer()->GetFrameTime());
			m_inputDirty = true;
		}
	}

	if (event.keyId == eKI_SYS_Commit && m_inputDirty)
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnMouseMove, m_dX, m_dY, m_dZ);
		m_inputDirty = false;
	}
	//////////////////////////////////////////////////////////////////////////	

	return false;
}

//------------------------------------------------------------------------
void CUISystem::Begin2D(int width, int height)
{
	m_width = width;
	m_height = height;
}

//------------------------------------------------------------------------
void CUISystem::Clear(const ColorB &color)
{
  float fOneDiv255 = 1.0f / 255.0f;
  ColorF cl = ColorF(color.r*fOneDiv255, color.g*fOneDiv255, color.b*fOneDiv255, color.a*fOneDiv255);
	m_pRenderer->ClearBuffer(FRT_CLEAR, &cl);
}

//------------------------------------------------------------------------
void CUISystem::SetScissor(int l, int t, int w, int h)
{
	m_scissorLeft = l;
	m_scissorTop = t;
	m_scissorWidth = w;
	m_scissorHeight = h;

	OnScissorChanged();
}

//------------------------------------------------------------------------
void CUISystem::SetBlend(int src, int dst)
{
	src |= dst;

	m_pRenderer->SetState(src | GS_NODEPTHTEST);
}

//------------------------------------------------------------------------
void CUISystem::DrawLine(float x, float y, float x2, float y2, const ColorB &color)
{
	if ((m_lastPrimitiveType != eUIPT_Line) || (m_lastTexId != -1))
	{
		Flush2D();
	}

	m_lastPrimitiveType = eUIPT_Line;
	m_lastTexId = -1;

	if (m_scissorWidth >= 0.001f || m_scissorHeight >= 0.001)
	{
		LiangBarskyClipLine(x, y, x2, y2, m_scissorLeft, m_scissorTop, m_scissorLeft+m_scissorWidth, m_scissorTop+m_scissorHeight);
	}

	int		rgb = (m_pRenderer->GetFeatures() & RFT_RGBA);
	uint32	dcolor = rgb ? RGBA8(color.r, color.g, color.b, color.a) : RGBA8(color.b, color.g, color.r, color.a);
	//uint32	dcolor = col;
	int			base = m_vertexList.size();

	static TUIVertex v;

	// v0
	v.color.dcolor = dcolor;
	v.xyz[0] = x;
	v.xyz[1] = y;
	v.xyz[2] = 0.0f;
	v.st[0] = 0.0f;
	v.st[1] = 0.0f;

	m_vertexList.push_back(v);

	// v1
	v.color.dcolor = dcolor;
	v.xyz[0] = x2;
	v.xyz[1] = y2;
	v.xyz[2] = 0.0f;
	v.st[0] = 0.0f;
	v.st[1] = 0.0f;

	m_vertexList.push_back(v);

	m_indexList.push_back(base + 0);
	m_indexList.push_back(base + 1);
}

//------------------------------------------------------------------------
void CUISystem::DrawTri(float x0, float y0, float x1, float y1, float x2, float y2, int texId, const ColorB &color, float *texCoords)
{
	if ((m_lastPrimitiveType != eUIPT_Tri) || (m_lastTexId != texId))
	{
		Flush2D();
	}

	m_lastPrimitiveType = eUIPT_Tri;
	m_lastTexId = texId;

	int		rgb = (m_pRenderer->GetFeatures() & RFT_RGBA);
	uint32	dcolor = rgb ? RGBA8(color.r, color.g, color.b, color.a) : RGBA8(color.b, color.g, color.r, color.a);
	int			base = m_vertexList.size();

	static TUIVertex v;

	// v0
	v.color.dcolor = dcolor;
	v.xyz[0] = x0;
	v.xyz[1] = y0;
	v.xyz[2] = 0.0f;
	v.st[0] = texCoords[0*2+0];
	v.st[1] = texCoords[0*2+1];

	m_vertexList.push_back(v);

	// v1
	v.color.dcolor = dcolor;
	v.xyz[0] = x1;
	v.xyz[1] = y1;
	v.xyz[2] = 0.0f;
	v.st[0] = texCoords[1*2+0];
	v.st[1] = texCoords[1*2+1];

	m_vertexList.push_back(v);

	// v2
	v.color.dcolor = dcolor;
	v.xyz[0] = x2;
	v.xyz[1] = y2;
	v.xyz[2] = 0.0f;
	v.st[0] = texCoords[2*2+0];
	v.st[1] = texCoords[2*2+1];

	m_vertexList.push_back(v);

	m_indexList.push_back(base + 0);
	m_indexList.push_back(base + 1);
	m_indexList.push_back(base + 2);
}

//------------------------------------------------------------------------
void CUISystem::DrawQuad(float l, float t, float w, float h, int texId, const ColorB &color, float *texCoords,float angleZ,float rx,float ry)
{
	if ((m_lastPrimitiveType != eUIPT_Tri) || (m_lastTexId != texId))
	{
		Flush2D();
	}

	m_lastPrimitiveType = eUIPT_Tri;
	m_lastTexId = texId;

	if (!color.a || (w*h <= 0.001f))
	{
		return;
	}

	float x = l;
	float y = t;
	float r = l + w;
	float b = t + h;

	float clippedTexCoords[4] = { 0.0f, 1.0f, 1.0f, 0.0f };

	if (m_scissorWidth > 0.001f && m_scissorHeight > 0.001f)
	{
		// clip the quad to the scissor rect
		x = max((float)m_scissorLeft, x);
		y = max((float)m_scissorTop, y);
		r = min((float)m_scissorLeft + m_scissorWidth, r);
		b = min((float)m_scissorTop + m_scissorHeight, b);

		if ((r - x <= 0.001f) || (b - y <= 0.001f))
		{
			return;
		}

		// clip texture coordinates
		if (texId > -1)
		{
			float texW = 1.0f;
			float texH = 1.0f;
			float rcpWidth = 1.0f / w;
			float rcpHeight = 1.0f / h;

			if (texCoords)
			{
				texW = texCoords[2] - texCoords[0];
				texH = texCoords[3] - texCoords[1];

				// clip horizontal
				clippedTexCoords[0] = texCoords[0] + (texW * ((x - l) * rcpWidth));
				clippedTexCoords[2] = texCoords[2] + (texW * ((r - (l + w)) * rcpWidth));

				// clip vertical
				clippedTexCoords[1] = texCoords[1] + (texH * ((y - t) * rcpHeight));
				clippedTexCoords[3] = texCoords[3] + (texH * ((b - (t + h)) * rcpHeight));
			}
			else
			{
				// clip horizontal
				clippedTexCoords[0] = 0.0f + (texW * ((x - l) * rcpWidth));
				clippedTexCoords[2] = 1.0f + (texW * ((r - (l + w)) * rcpWidth));

				// clip vertical
				clippedTexCoords[1] = 1.0f + (texH * ((y - t) * rcpHeight));
				clippedTexCoords[3] = 0.0f + (texH * ((b - (t + h)) * rcpHeight));
			}
		}
	}
	else if (texCoords)
	{
		clippedTexCoords[0] = texCoords[0];
		clippedTexCoords[1] = texCoords[1];
		clippedTexCoords[2] = texCoords[2];
		clippedTexCoords[3] = texCoords[3];
	}

	int		rgb = (m_pRenderer->GetFeatures() & RFT_RGBA);
	uint32	dcolor = rgb ? RGBA8(color.r, color.g, color.b, color.a) : RGBA8(color.b, color.g, color.r, color.a);
	int			base = m_vertexList.size();

	static TUIVertex v;
	
	Vec3 vertices[4];
	vertices[0].Set(x,y,0);
	vertices[1].Set(r,y,0);
	vertices[2].Set(r,b,0);
	vertices[3].Set(x,b,0);

	//apply rotation
	if (angleZ)
	{
		Vec3 center(Vec3(x+r,y+b,0)*0.5f);
	 	 
		Matrix33 rotMtx;
		rotMtx.SetRotationZ(angleZ);

		for (int i=0;i<4;++i)
		{
			vertices[i] -= center;
			vertices[i].x /= rx;
			vertices[i].y /= ry;

			vertices[i] = rotMtx * vertices[i];

			vertices[i].x *= rx;
			vertices[i].y *= ry;
			vertices[i] += center;
		}
	}

	// v0
	v.color.dcolor = dcolor;
	v.xyz = vertices[0];
	v.st[0] = clippedTexCoords[0];
	v.st[1] = clippedTexCoords[1];

	m_vertexList.push_back(v);

	// v1
	v.color.dcolor = dcolor;
	v.xyz = vertices[1];
	v.st[0] = clippedTexCoords[2];
	v.st[1] = clippedTexCoords[1];

	m_vertexList.push_back(v);

	// v2
	v.color.dcolor = dcolor;
	v.xyz = vertices[2];
	v.st[0] = clippedTexCoords[2];
	v.st[1] = clippedTexCoords[3];

	m_vertexList.push_back(v);

	// v3
	v.color.dcolor = dcolor;
	v.xyz = vertices[3];
	v.st[0] = clippedTexCoords[0];
	v.st[1] = clippedTexCoords[3];

	m_vertexList.push_back(v);

	m_indexList.push_back(base + 0);
	m_indexList.push_back(base + 1);
	m_indexList.push_back(base + 3);
	m_indexList.push_back(base + 1);
	m_indexList.push_back(base + 2);
	m_indexList.push_back(base + 3);
}

//------------------------------------------------------------------------
void CUISystem::DrawCircle(float x, float y, float radius, int slices, const ColorB &color)
{
	if ((m_lastPrimitiveType != eUIPT_Tri) || (m_lastTexId != -1))
	{
		Flush2D();
	}

	m_lastPrimitiveType = eUIPT_Tri;
	m_lastTexId = -1;

	if (radius <= 0.001f)
	{
		return;
	}

	int		rgb = (m_pRenderer->GetFeatures() & RFT_RGBA);
	uint32	dcolor = rgb ? RGBA8(color.r, color.g, color.b, color.a) : RGBA8(color.b, color.g, color.r, color.a);
	int			base = m_vertexList.size();

	static TUIVertex v;

	// center
	v.xyz.x = x;
	v.xyz.y = y;
	v.xyz.z = 0.0f;
	v.st[0] = 0.0f;
	v.st[1] = 0.0f;
	v.color.dcolor = dcolor;
	m_vertexList.push_back(v);

	float slice = DEG2RAD(360.0f/(float)slices);

	for(int i = 0; i <= slices; i++)
	{
		v.xyz.x = x+radius * cosf(i*slice);
		v.xyz.y = y+radius * sinf(i*slice);
		v.xyz.z = 0.0f;
		v.st[0] = 0.0f;
		v.st[1] = 0.0f;
		v.color.dcolor = dcolor;

		m_vertexList.push_back(v);
	}

	for (int f = 0; f < slices; f++)
	{
		m_indexList.push_back(base + 0);
		m_indexList.push_back(base + f + 1);
		m_indexList.push_back(base + f + 2);
	}
}
//------------------------------------------------------------------------
void CUISystem::Flush2D()
{
	if (m_vertexList.empty() || (m_lastPrimitiveType == eUIPT_None))
	{
		return;
	}

	//m_pRenderer->ResetToDefault();

	m_pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);
  
  // Reset texture states and set default color operation for fixed pipeline stuff (else it will use previous states)
  m_pRenderer->SetColorOp(255, 255, eCA_Texture | (eCA_Diffuse<<3), eCA_Texture | (eCA_Diffuse<<3));
  m_pRenderer->SelectTMU(1);
  m_pRenderer->EnableTMU(false); 

	m_pRenderer->SelectTMU(0);
  m_pRenderer->EnableTMU(true);  

 	m_pRenderer->SetCullMode(R_CULL_DISABLE);

	m_pRenderer->Set2DMode(true, m_width, m_height);

	ITexture *pTexture = m_pRenderer->EF_GetTextureByID(m_lastTexId);

	if (!pTexture || (pTexture->GetTextureID() != m_lastTexId))
	{
		m_pRenderer->SetWhiteTexture();
	}
	else
	{
		m_pRenderer->SetTexture(m_lastTexId);
	}

	switch(m_lastPrimitiveType)
	{
	case eUIPT_Line:
		{
			m_pRenderer->DrawDynVB((TUIVertex *)&m_vertexList[0], (uint16 *)&m_indexList[0], m_vertexList.size(), m_indexList.size(), R_PRIMV_LINES);
		}
		break;
	case eUIPT_Tri:
		{
			m_pRenderer->DrawDynVB((TUIVertex *)&m_vertexList[0], (uint16 *)&m_indexList[0], m_vertexList.size(), m_indexList.size(), R_PRIMV_TRIANGLES);
		}
		break;
	}

	m_pRenderer->Set2DMode(false, 0, 0);
	
	m_lastPrimitiveType = eUIPT_None;
	m_lastTexId = -1;

	m_vertexList.resize(0);
	m_indexList.resize(0);
}

//------------------------------------------------------------------------
void CUISystem::End2D()
{
	Flush2D();
}

//------------------------------------------------------------------------
void CUISystem::Render3DEntity(EntityId entityId)
{
	IEntity *pEntity = m_pSystem->GetIEntitySystem()->GetEntity(entityId);
	
	if (!pEntity)
	{
		return;
	}

	IEntityRenderProxy *pProxy = (IEntityRenderProxy *)pEntity->GetProxy(ENTITY_PROXY_RENDER);
	IRenderNode *pRenderNode = pProxy->GetRenderNode();

	SRendParams params;

	params.dwFObjFlags |= FOB_TRANS_MASK;
	params.nDLightMask = 0;
	params.pRenderNode = pRenderNode;
	params.AmbientColor = Vec3(1, 1, 1);

	pRenderNode->Render(params);
}

//------------------------------------------------------------------------
void CUISystem::Render3DEntityEx(EntityId entityId, float fov, float znear, float zfar, float zrangemin, float zrangemax)
{
	IEntity *pEntity = m_pSystem->GetIEntitySystem()->GetEntity(entityId);
	if (!pEntity)
		return;

	IEntityRenderProxy *pProxy = (IEntityRenderProxy *)pEntity->GetProxy(ENTITY_PROXY_RENDER);
	if (!pProxy)
		return;

	// needs to be valid every frame
	static CCamera customCamera;
	customCamera = m_pSystem->GetViewCamera();

	if (fov < 0.0f) fov = customCamera.GetFov();
	if (znear < 0.0f) znear = customCamera.GetNearPlane();
	if (zfar < 0.0f) zfar = customCamera.GetFarPlane();
	if (zrangemin < 0.0f) zrangemin = customCamera.GetZRangeMin();
	if (zrangemax < 0.0f) zrangemax = customCamera.GetZRangeMax();

	customCamera.SetFrustum(customCamera.GetViewSurfaceX(), customCamera.GetViewSurfaceZ(),	fov, znear, zfar);
	customCamera.SetZRange(zrangemin, zrangemax);

	IRenderNode *pRenderNode = pProxy->GetRenderNode();
	IShaderPublicParams *pPublicParams = NULL; //pProxy->GetShaderPublicParams();
	if (!pPublicParams)
		return;

	for (int i = 0; i < pEntity->GetSlotCount(); i++)
	{
		pEntity->SetSlotFlags(i, pEntity->GetSlotFlags(i)|ENTITY_SLOT_RENDER_WITH_CUSTOM_CAMERA);
	}
	
	SShaderParam shaderParam;
	shaderParam.m_Type = eType_CAMERA;
	shaderParam.m_Value.m_pCamera = &customCamera;
	strncpy(shaderParam.m_Name, "custom_camera", 32);

	bool set = false;
	for (int p = 0; p < pPublicParams->GetParamCount(); p++)
	{
		if (!stricmp(pPublicParams->GetParam(p).m_Name, shaderParam.m_Name))
		{
			pPublicParams->SetParam(p, shaderParam);
			set = true;
		}
	}

	if (!set)
	{
		pPublicParams->AddParam(shaderParam);
	}

	SRendParams params;

	params.dwFObjFlags |= FOB_TRANS_MASK;
	params.fAlpha = 1.0f;
	params.pRenderNode = pRenderNode;
	params.AmbientColor = Vec3(1, 1, 1);

	pRenderNode->Render(params);
}

//------------------------------------------------------------------------
int CUISystem::CreateRenderTarget(int width, int height)
{
	return m_pRenderer->CreateRenderTarget(width, height, eTF_A8R8G8B8);
}

//------------------------------------------------------------------------
void CUISystem::DestroyRenderTarget(int id)
{
	m_pRenderer->DestroyRenderTarget(id);
}

//------------------------------------------------------------------------
void CUISystem::SetRenderTarget(int id)
{
	m_pRenderer->SetRenderTarget(id);
	m_renderTarget = id;

	if (id)
	{
		ITexture *pTexture = m_pRenderer->EF_GetTextureByID(id);

		if (pTexture)
		{
			m_pRenderer->SetViewport(0, 0, pTexture->GetSourceWidth(), pTexture->GetSourceHeight());
		}
	}
}

//------------------------------------------------------------------------
void CUISystem::Localize(wstring &localized, const char *key, bool forceEnglish)
{
}

//------------------------------------------------------------------------
void CUISystem::SetFont(const char *fontName, const char *fontEffect)
{
	m_pFont = m_pSystem->GetICryFont()->GetFont(fontName);

	if (!m_pFont)
	{
		m_pFont = m_pSystem->GetICryFont()->GetFont("default");
	}

	if (!m_pFont)
	{
		assert(m_pFont);
		return;
	}

	m_pFont->Reset();
	m_pFont->UseRealPixels(true);
	m_pFont->SetEffect(fontEffect);

	OnScissorChanged();
}

//------------------------------------------------------------------------
void CUISystem::DrawText(float x, float y, const wchar_t *str, float size, const ColorB &color, int halign, int valign)
{
	Flush2D();

	m_pFont->UseRealPixels(true);
	m_pFont->SetSize(vector2f(size, size));
	m_pFont->SetColor(ColorF(color.r*(1.0f/255.0f), color.g*(1.0f/255.0f), color.b*(1.0f/255.0f), color.a*(1.0f/255.0f)));
	m_pRenderer->SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);

	if ((halign != eUIA_Left) || (valign != eUIA_Top))
	{
		vector2f dims(m_pFont->GetTextSizeW(str));

		switch(halign)
		{
		case eUIA_Right:
			x -= dims.x;
			break;
		case eUIA_Center:
			x -= dims.x*0.5;
			break;
		}

		switch(valign)
		{
		case eUIA_Bottom:
			y -= dims.y;
			break;
		case eUIA_Middle:
			y -= dims.y*0.5;
			break;
		}
	}

	m_pFont->DrawStringW(x, y, str);
}

//------------------------------------------------------------------------
vector2f CUISystem::GetTextSize(const wchar_t *str, float size)
{
	m_pFont->UseRealPixels(true);
	m_pFont->SetSize(vector2f(size, size));

	return m_pFont->GetTextSizeW(str, 0);
}

//------------------------------------------------------------------------
void CUISystem::OnScissorChanged()
{
	if (m_pFont)
	{
		if (m_scissorWidth <= 0.001f || m_scissorHeight <= 0.001f)
		{
			m_pFont->EnableClipping(0);
		}
		else
		{
			m_pFont->EnableClipping(1);
			m_pFont->SetClippingRect(m_scissorLeft, m_scissorTop, m_scissorWidth, m_scissorHeight);
		}
	}
}

//------------------------------------------------------------------------
const wchar_t *CUISystem::ToWideString(const char *src)
{
	static wchar_t aux[2048];
	aux[0] = 0;

	int len = strlen(src);
	const char *psrc = src;
	wchar_t *pdst = aux;

	for (int i = 0; i < len; i++)
	{
		mbtowc((wchar_t *)pdst++, psrc++, 1);
	}
	*pdst = 0;

	return aux;
}

//------------------------------------------------------------------------
void CUISystem::OnLevelNotFound(const char *levelName)
{
	m_loading = false;

	if (levelName)
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLevelNotFound, levelName);
	}
	else
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLevelNotFound);
	}
}

//------------------------------------------------------------------------
void CUISystem::OnLoadingStart(ILevelInfo *pLevel)
{
	m_loading = true;

	if (pLevel)
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLoadingStart, pLevel->GetName(), pLevel->GetPath(), pLevel->GetDefaultGameType()->cgfCount);
	}
	else
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLoadingStart);
	}
}

//------------------------------------------------------------------------
void CUISystem::OnLoadingComplete(ILevel *pLevel)
{
	if (pLevel)
	{
		ILevelInfo *pLevelInfo = pLevel->GetLevelInfo();

		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLoadingComplete, pLevelInfo->GetName(), pLevelInfo->GetPath(), pLevelInfo->GetDefaultGameType()->cgfCount);
	}
	else
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLoadingComplete);
	}

	m_loading = false;
}

//------------------------------------------------------------------------
void CUISystem::OnLoadingError(ILevelInfo *pLevel, const char *error)
{
	if (pLevel)
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLoadingError, pLevel->GetName(), pLevel->GetPath());
	}
	else
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLoadingError);
	}

	m_loading = false;
}

//------------------------------------------------------------------------
void CUISystem::OnLoadingProgress(ILevelInfo *pLevel, int progressAmount)
{
	if (!m_loading)
		return;

	if (pLevel)
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLoadingProgress, pLevel->GetName(), pLevel->GetPath(), progressAmount);
	}
	else
	{
		m_pScriptUI->CallScriptEvent(CScriptBind_UI::eUISE_OnLoadingProgress);
	}

	if (m_pRenderer->EF_Query(EFQ_RecurseLevel) <= 0)
	{
		m_pSystem->RenderBegin();

		PreUpdate(0.0f, 0);
		PostUpdate(0.0f, 0);

		m_pSystem->RenderEnd();
	}
}

//------------------------------------------------------------------------
void CUISystem::RegisterCVars()
{
	gEnv->pConsole->AddCommand("ui_reload_scripts", "UI.Reset()", 0, "Reloads UI scripts!");
}

//------------------------------------------------------------------------
void CUISystem::UnregisterCVars()
{
	gEnv->pConsole->RemoveCommand("ui_reload_scripts");
}
