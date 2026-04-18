/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Simple user interface implementation.
							 Translates input events, and sends them to script.
							 Provides drawing functions.
  
 -------------------------------------------------------------------------
  History:
  - 16:8:2004   17:50 : Created by Márcio Martins

*************************************************************************/
#ifndef __UISYSTEM_H__
#define __UISYSTEM_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IInput.h>
#include "ILevelSystem.h"


typedef struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F TUIVertex;

enum EUIPrimitiveType
{
	eUIPT_None,
	eUIPT_Line,
	eUIPT_Tri,
};

enum EUIAlignType
{
	eUIA_Left,
	eUIA_Right,
	eUIA_Center,
	eUIA_Middle,
	eUIA_Top,
	eUIA_Bottom,
};


struct ISystem;
struct IGameFramework;
class CScriptBind_UI;
struct IFlashPlayer;


class CUISystem :
	public IInputEventListener,
	public ILevelSystemListener
	//public IUISystem,
{
public:
	CUISystem(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CUISystem();

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char *levelName);
	virtual void OnLoadingStart(ILevelInfo *pLevel);
	virtual void OnLoadingComplete(ILevel *pLevel);
	virtual void OnLoadingError(ILevelInfo *pLevel, const char *error);
	virtual void OnLoadingProgress(ILevelInfo *pLevel, int progressAmount);
	//~ILevelSystemListener

	void Release() { delete this; };

	void Reset();
	
	void PauseGame(bool pause, bool force);
	bool IsGamePaused();
	void PreUpdate(float deltaTime, int frameId);
	void PostUpdate(float deltaTime, int frameId);
	
	bool IsEditor() { return m_pSystem->IsEditor(); };

	int GetWidth() const { return m_width; };
	int GetHeight() const { return m_height; };

	int GetScreenWidth() const { return m_pRenderer->GetWidth(); };
	int GetScreenHeight() const { return m_pRenderer->GetHeight(); };

	void EnableInput(bool enable);
	bool OnInputEvent(const SInputEvent &event);

	void Begin2D(int width, int height);
	void Clear(const ColorB &color);
	void SetScissor(int l, int t, int w, int h);
	void SetBlend(int src, int dst);
	void DrawLine(float x, float y, float x2, float y2, const ColorB &color);
	void DrawTri(float x0, float y0, float x1, float y1, float x2, float y2, int texId, const ColorB &color, float *texCoords);
	void DrawQuad(float l, float t, float w, float h, int texId, const ColorB &color, float *texCoords,float angleZ = 0.0f,float rx = 1.0f,float ry = 1.0f);
	void DrawCircle(float x, float y, float radius, int slices, const ColorB &color);
	void Flush2D();
	void End2D();

	void Render3DEntity(EntityId entityId);
	void Render3DEntityEx(EntityId entityId, float fov, float znear, float zfar, float zrangemin, float zrangemax);
	
	int CreateRenderTarget(int width, int height);
	void DestroyRenderTarget(int id);
	void SetRenderTarget(int id);

	void Localize(wstring &localized, const char *key, bool forceEnglish);
	void SetFont(const char *fontName, const char *fontEffect);
	void DrawText(float x, float y, const wchar_t *str, float size, const ColorB &color, int halign, int valign);
	vector2f GetTextSize(const wchar_t *str, float size);

	const wchar_t *ToWideString(const char *src);

private:
	void OnScissorChanged();
	void RegisterCVars();
	void UnregisterCVars();

	IGameFramework					*m_pGameFramework;
	ISystem									*m_pSystem;
	IRenderer								*m_pRenderer;
	IFFont									*m_pFont;
	CScriptBind_UI					*m_pScriptUI;

	EUIPrimitiveType				m_lastPrimitiveType;
	int											m_lastTexId;

	int											m_scissorLeft;
	int											m_scissorTop;
	int											m_scissorWidth;
	int											m_scissorHeight;

	int											m_width;
	int											m_height;

	float										m_dX;
	float										m_dY;
	float										m_dZ;
	int											m_padX;
	int											m_padY;
	bool										m_inputDirty;

	int											m_renderTarget;
	bool										m_loading;

	std::vector<TUIVertex>	m_vertexList;
	std::vector<uint16>			m_indexList;
};


#endif //__UISYSTEM_H__
