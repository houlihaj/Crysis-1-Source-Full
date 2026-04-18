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
#include "ScriptBind_UI.h"
#include "UISystem.h"
#include "IGameFramework.h"


//------------------------------------------------------------------------
#define TABLE_TO_RGBA(tbl, _r, _g, _b, _a)									\
	{																													\
		tbl->GetAt(1, _r);																			\
		tbl->GetAt(2, _g);																			\
		tbl->GetAt(3, _b);																			\
		tbl->GetAt(4, _a);																			\
	}

#define TABLE_TO_COLOR_B(tbl, color)												\
	{																													\
		int r=0, g=0, b=0, a=0;																	\
		if (tbl->GetAt(1, r))	color.r = r;											\
		if (tbl->GetAt(2, g))	color.g = g;											\
		if (tbl->GetAt(3, b))	color.b = b;											\
		if (tbl->GetAt(4, a))	color.a = a;											\
	}

#define TABLE_TO_COLOR_F(tbl, color)												\
	{																													\
		int r, g, b, a;																					\
		if (tbl->GetAt(1, r))	color.r = r*(1.0f/255.0f);				\
		if (tbl->GetAt(2, g))	color.g = g*(1.0f/255.0f);				\
		if (tbl->GetAt(3, b))	color.b = b*(1.0f/255.0f);				\
		if (tbl->GetAt(4, a))	color.a = a*(1.0f/255.0f);				\
	}

CScriptBind_UI::CScriptBind_UI(ISystem *pSystem, CUISystem *pUISystem, IGameFramework *pGameFramework)
: m_pUISystem(pUISystem),
	m_pGameFramework(pGameFramework)
{
	CScriptableBase::Init(pSystem->GetIScriptSystem(), pSystem);
	SetGlobalName("UI");

	m_pSS->GetGlobalValue("UI", m_scriptTable);

	m_levelInfo.Create(pSystem->GetIScriptSystem());

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_UI::~CScriptBind_UI()
{
}

//------------------------------------------------------------------------
void CScriptBind_UI::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_UI::

	SCRIPT_REG_TEMPLFUNC(Reset, "");
	SCRIPT_REG_TEMPLFUNC(EnableInput, "enable");
	SCRIPT_REG_TEMPLFUNC(Begin2D, "width, height");
	SCRIPT_REG_TEMPLFUNC(Clear, "color");
	SCRIPT_REG_FUNC(SetScissor);
	SCRIPT_REG_FUNC(SetBlend);
	SCRIPT_REG_FUNC(DrawLine);
	SCRIPT_REG_FUNC(DrawTri);
	SCRIPT_REG_FUNC(DrawQuad);
	SCRIPT_REG_FUNC(DrawCircle);
	SCRIPT_REG_TEMPLFUNC(Flush2D, "");
	SCRIPT_REG_TEMPLFUNC(End2D, "");
	SCRIPT_REG_TEMPLFUNC(Render3DEntity, "entityId");
	SCRIPT_REG_TEMPLFUNC(GetWidth, "");
	SCRIPT_REG_TEMPLFUNC(GetHeight, "");
	SCRIPT_REG_TEMPLFUNC(GetScreenWidth, "");
	SCRIPT_REG_TEMPLFUNC(GetScreenHeight, "");
	SCRIPT_REG_TEMPLFUNC(PauseGame, "pause");
	SCRIPT_REG_TEMPLFUNC(IsGamePaused, "");
	SCRIPT_REG_FUNC(Localize);
	SCRIPT_REG_FUNC(SetFont);
	SCRIPT_REG_FUNC(DrawText);
	//SCRIPT_REG_FUNC(DrawStatusText);
	SCRIPT_REG_FUNC(GetTextSize);
	SCRIPT_REG_FUNC(LoadImage);
	SCRIPT_REG_FUNC(FreeImage);
	SCRIPT_REG_FUNC(GetImageSize);
	SCRIPT_REG_FUNC(CreateRenderTarget);
	SCRIPT_REG_FUNC(SetRenderTarget);
	SCRIPT_REG_FUNC(DestroyRenderTarget);
	SCRIPT_REG_FUNC(GetCurrentLevelInfo);
}

//------------------------------------------------------------------------
void CScriptBind_UI::RegisterGlobals()
{
	SCRIPT_REG_GLOBAL(eUIA_Left);
	SCRIPT_REG_GLOBAL(eUIA_Right);
	SCRIPT_REG_GLOBAL(eUIA_Center);
	SCRIPT_REG_GLOBAL(eUIA_Top);
	SCRIPT_REG_GLOBAL(eUIA_Bottom);
	SCRIPT_REG_GLOBAL(eUIA_Middle);
}

//------------------------------------------------------------------------
void CScriptBind_UI::OnInit()
{
	static const char *eventName[eUISE_Last];
	
	eventName[eUISE_OnInit] = "OnInit";
	eventName[eUISE_OnRelease] = "OnRelease";
	eventName[eUISE_OnPreUpdate] = "OnPreUpdate";
	eventName[eUISE_OnPostUpdate] = "OnPostUpdate";
	eventName[eUISE_OnIdle] = "OnIdle";
	eventName[eUISE_OnKeyDown] = "OnKeyDown";
	eventName[eUISE_OnKeyUp] = "OnKeyUp";
	eventName[eUISE_OnMouseDown] = "OnMouseDown";
	eventName[eUISE_OnMouseUp] = "OnMouseUp";
	eventName[eUISE_OnMouseMove] = "OnMouseMove";
	eventName[eUISE_OnLevelNotFound] = "OnLevelNotFound";
	eventName[eUISE_OnLoadingStart] = "OnLoadingStart";
	eventName[eUISE_OnLoadingComplete] = "OnLoadingComplete";
	eventName[eUISE_OnLoadingError] = "OnLoadingError";
	eventName[eUISE_OnLoadingProgress] = "OnLoadingProgress";

	for (int i = 0; i < eUISE_Last; i++)
	{
		m_onEventPtr[i] = m_pSS->GetFunctionPtr("UI", eventName[i]);
	}
}

//------------------------------------------------------------------------
void CScriptBind_UI::OnRelease()
{
	for (int i = 0; i < eUISE_Last; i++)
	{
		m_pSS->ReleaseFunc(m_onEventPtr[i]);
		m_onEventPtr[i] = 0;
	}
}

//------------------------------------------------------------------------
int CScriptBind_UI::Reset(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	
	m_pUISystem->Reset();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::EnableInput(IFunctionHandler *pH, bool enable)
{
	m_pUISystem->EnableInput(enable);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::Begin2D(IFunctionHandler *pH, int width, int height)
{
	m_pUISystem->Begin2D(width, height);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::Clear(IFunctionHandler *pH, SmartScriptTable color)
{
	ColorB clr;

	TABLE_TO_COLOR_B(color, clr);

	m_pUISystem->Clear(clr);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::SetScissor(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS2(0, 4);

	int l=0,
			t=0,
			w=0,
			h=0;
	
	if (pH->GetParams(l, t, w, h))
	{
		m_pUISystem->SetScissor(l, t, w, h);
	}
	else
	{
		m_pUISystem->SetScissor(0, 0, 0, 0);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::SetBlend(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	int src, dst;

	if (pH->GetParams(src, dst))
	{
		m_pUISystem->SetBlend(src, dst);
	}
	else
	{
		m_pUISystem->SetBlend(GS_BLSRC_SRCALPHA, GS_BLDST_ONEMINUSSRCALPHA);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::DrawLine(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(5);
	
	float x0=0.0f,
				y0=0.0f,
				x1=0.0f,
				y1=0.0f;
	SmartScriptTable colorTbl;

	if (pH->GetParams(x0, y0, x1, y1, colorTbl))
	{
		ColorB color;

		TABLE_TO_COLOR_B(colorTbl, color);

		m_pUISystem->DrawLine(x0, y0, x1, y1, color);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::DrawTri(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS3(7, 8, 9);

	float x0=0.0f,
				y0=0.0f,
				x1=0.0f,
				y1=0.0f,
				x2=0.0f,
				y2=0.0f;

	if (pH->GetParams(x0, y0, x1, y1, x2, y2))
	{
		SmartScriptTable	colorTbl;
		SmartScriptTable	texCoordTbl;
		ScriptHandle			texture(-1);
		ColorB						color(255, 255, 255, 255);
		float							texCoord[6] = {0.0f};

		pH->GetParam(7, texture);

		if (pH->GetParam(8, colorTbl))
		{
			TABLE_TO_COLOR_B(colorTbl, color);
		}

		if (pH->GetParam(9, texCoordTbl))
		{
			for (int i = 0; i < 6; i++)
			{
				texCoordTbl->GetAt(i+1, texCoord[i]);
			}
		}

		m_pUISystem->DrawTri(x0, y0, x1, y1, x2, y2, texture.n, color, texCoord);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::DrawQuad(IFunctionHandler *pH)
{
	//SCRIPT_CHECK_PARAMETERS3(5, 6, 7);

	float l=0.0f,
				t=0.0f,
				w=0.0f,
				h=0.0f;

	if (pH->GetParams(l, t, w, h))
	{
		SmartScriptTable	colorTbl;
		SmartScriptTable	texCoordTbl;
		ScriptHandle			texture(-1);
		ColorB						color(255, 255, 255, 255);
		float							texCoord[4] = { 0.0f,
																			0.0f,
																			1.0f,
																			1.0f };
		if (pH->GetParamType(5) == svtPointer)
		{
			pH->GetParam(5, texture);
		}

		if ((pH->GetParamType(6) == svtObject) && pH->GetParam(6, colorTbl))
		{
			TABLE_TO_COLOR_B(colorTbl, color);
		}

		if ((pH->GetParamType(7) == svtObject) && pH->GetParam(7, texCoordTbl))
		{
			for (int i = 0; i < 4; i++)
			{
				texCoordTbl->GetAt(i+1, texCoord[i]);
			}
		}

		float angleZ(0.0f);
		float rx(1.0f);
		float ry(1.0f);

		if (pH->GetParamType(8) == svtNumber)
			pH->GetParam(8, angleZ);
	
		if (pH->GetParamType(9) == svtNumber)
			pH->GetParam(9, rx);

		if (pH->GetParamType(10) == svtNumber)
			pH->GetParam(10, ry);
		
		m_pUISystem->DrawQuad(l, t, w, h, texture.n, color, texCoord, angleZ, rx, ry);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::DrawCircle(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS3(3, 4, 5);

	float x, y, radius;

	if (pH->GetParams(x, y, radius))
	{
		SmartScriptTable	colorTbl;
		ColorB						color(255, 255, 255, 255);
		int								slices=(int)(max(16.0f, radius)*3);

		if (pH->GetParamType(4) == svtNumber)
		{
			pH->GetParam(4, slices);
		}

		if ((pH->GetParamType(5) == svtObject) && pH->GetParam(5, colorTbl))
		{
			TABLE_TO_COLOR_B(colorTbl, color);
		}

		m_pUISystem->DrawCircle(x, y, radius, slices, color);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::Flush2D(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	m_pUISystem->Flush2D();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::End2D(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(0);

	m_pUISystem->End2D();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::Render3DEntity(IFunctionHandler *pH, ScriptHandle entityId)
{
	if (pH->GetParamCount() == 1)
	{
		m_pUISystem->Render3DEntity(entityId.n);
	}
	else
	{
		float fov = -1.0f;
		float znear = -1.0f;
		float zfar = -1.0f;
		float zrangemin = -1.0f;
		float zrangemax = -1.0f;

		if (pH->GetParamCount() >= 2) pH->GetParam(2, fov);
		if (pH->GetParamCount() >= 3) pH->GetParam(3, znear);
		if (pH->GetParamCount() >= 4) pH->GetParam(4, zfar);
		if (pH->GetParamCount() >= 5) pH->GetParam(5, zrangemin);
		if (pH->GetParamCount() >= 6) pH->GetParam(6, zrangemax);

		m_pUISystem->Render3DEntityEx(entityId.n, fov, znear, zfar, zrangemin, zrangemax);
	}
	
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::GetScreenWidth(IFunctionHandler *pH)
{
	return pH->EndFunction(m_pUISystem->GetScreenWidth());
}

//------------------------------------------------------------------------
int CScriptBind_UI::GetScreenHeight(IFunctionHandler *pH)
{
	return pH->EndFunction(m_pUISystem->GetScreenHeight());
}

//------------------------------------------------------------------------
int CScriptBind_UI::GetWidth(IFunctionHandler *pH)
{
	return pH->EndFunction(m_pUISystem->GetWidth());
}

//------------------------------------------------------------------------
int CScriptBind_UI::GetHeight(IFunctionHandler *pH)
{
	return pH->EndFunction(m_pUISystem->GetHeight());
}

//------------------------------------------------------------------------
int CScriptBind_UI::PauseGame(IFunctionHandler *pH, bool pause)
{
	bool forced = false;
	
	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, forced);
	}

	m_pUISystem->PauseGame(pause, forced);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::IsGamePaused(IFunctionHandler *pH)
{
	return pH->EndFunction(m_pUISystem->IsGamePaused());
}

//------------------------------------------------------------------------
int CScriptBind_UI::Localize(IFunctionHandler *pH)
{
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::SetFont(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	const char *fontName = 0;
	const char *fontEffect = 0;

	if (pH->GetParams(fontName, fontEffect))
	{
		m_pUISystem->SetFont(fontName, fontEffect);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::DrawText(IFunctionHandler *pH)
{
	float x=0.0f,
				y=0.0f;
	float size=16.0f;
	const char *text = 0;
	int halign = eUIA_Left;
	int valign = eUIA_Top;
	SmartScriptTable colorTbl;
	
	if (pH->GetParams(x, y, text, size, colorTbl))
	{
		ColorB color(255, 255, 255, 255);

		TABLE_TO_COLOR_B(colorTbl, color);

		if (pH->GetParamCount() > 5)
		{
			pH->GetParam(6, halign);

			if (pH->GetParamCount() > 6)
			{
				pH->GetParam(7, valign);
			}
		}

		m_pUISystem->DrawText(x, y, m_pUISystem->ToWideString(text), size, color, halign, valign);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::GetTextSize(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	float size = 16.0f;
	const char *text = 0;

	if (pH->GetParams(text, size))
	{
		vector2f v = m_pUISystem->GetTextSize((wchar_t *)text, size);

		return pH->EndFunction(v.x, v.y);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::LoadImage(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS2(1, 2);

	const char *imageName = 0;
	bool reload = false;

	if (pH->GetParamType(1) == svtString)
	{
		pH->GetParam(1, imageName);
	}

	if (pH->GetParamType(2) == svtBool)
	{
		pH->GetParam(2, reload);
	}

	if (imageName)
	{
		IRenderer *pRenderer = gEnv->pRenderer;
		ITexture *pTex = pRenderer->EF_LoadTexture(imageName, FT_NOMIPS | FT_DONT_RESIZE, eTT_2D);

		if (pTex)
		{
			return pH->EndFunction(ScriptHandle(pTex->GetTextureID()));
		}
	}
	
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::FreeImage(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	ScriptHandle tex;

	if (pH->GetParamType(1) == svtPointer)
	{
		pH->GetParam(1, tex);

		IRenderer *pRenderer = gEnv->pRenderer;

		pRenderer->RemoveTexture(tex.n);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::GetImageSize(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	ScriptHandle tex;

	if (pH->GetParamType(1) == svtPointer)
	{
		pH->GetParam(1, tex);

		IRenderer *pRenderer = gEnv->pRenderer;
		ITexture *pTexture = pRenderer->EF_GetTextureByID(tex.n);

		if (pTexture)
		{
			return pH->EndFunction(pTexture->GetSourceWidth(), pTexture->GetSourceHeight());
		}
	}

	return pH->EndFunction(0, 0);
}

//------------------------------------------------------------------------
int CScriptBind_UI::CreateRenderTarget(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(2);

	int width = 256, height = 256;

	if (pH->GetParams(width, height))
	{
		return pH->EndFunction(ScriptHandle(m_pUISystem->CreateRenderTarget(width, height)));
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::DestroyRenderTarget(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	ScriptHandle id(0);

	if (pH->GetParamType(1) == svtPointer)
	{
		pH->GetParam(1, id);

		m_pUISystem->DestroyRenderTarget(id.n);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int	CScriptBind_UI::SetRenderTarget(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	ScriptHandle id(0);

	if (pH->GetParamType(1) == svtPointer)
	{
		pH->GetParam(1, id);

		m_pUISystem->SetRenderTarget(id.n);
	}
	else
	{
		m_pUISystem->SetRenderTarget(0);
	}

  return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_UI::GetCurrentLevelInfo(IFunctionHandler *pH)
{
	if (m_pUISystem->IsEditor())
	{
		char *levelName = 0;
		char *levelFolder = 0;

		m_pGameFramework->GetEditorLevel(&levelName, &levelFolder);

		if (levelName && levelName[0])
		{
			string name(levelName);
			string path(levelName);
			
			path.replace("\\", "/");

			m_levelInfo->SetValue("name", name.c_str());
			m_levelInfo->SetValue("path", path.c_str());
			m_levelInfo->SetValue("folderName", path.c_str());
			m_levelInfo->SetValue("terrainSize", gEnv->p3DEngine->GetTerrainSize());
			m_levelInfo->SetValue("heightmapSize", gEnv->p3DEngine->GetTerrainSize()/gEnv->p3DEngine->GetHeightMapUnitSize());

			return pH->EndFunction(m_levelInfo);
		}
	}
	else
	{
		ILevel *pLevel = m_pGameFramework->GetILevelSystem()->GetCurrentLevel();

		if (pLevel)
		{
			ILevelInfo *pLevelInfo = pLevel->GetLevelInfo();

			if (pLevelInfo)
			{
				m_levelInfo->SetValue("name", pLevelInfo->GetName());
				m_levelInfo->SetValue("path", pLevelInfo->GetPath());
				m_levelInfo->SetValue("folderName", pLevelInfo->GetPath());
				m_levelInfo->SetValue("terrainSize", gEnv->p3DEngine->GetTerrainSize());
				m_levelInfo->SetValue("heightmapSize", gEnv->p3DEngine->GetTerrainSize()/gEnv->p3DEngine->GetHeightMapUnitSize());

				return pH->EndFunction(m_levelInfo);
			}
		}
	}

	return pH->EndFunction();
}
